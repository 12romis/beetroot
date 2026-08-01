#include "bmp280.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "bmp280";

// Регістри BMP280
#define BMP280_REG_ID 0xD0
#define BMP280_REG_CALIB 0x88 // 0x88..0x9F (24 байти)
#define BMP280_REG_CONFIG 0xF5
#define BMP280_REG_CTRL 0xF4
#define BMP280_REG_DATA 0xF7 // press(3) + temp(3)
#define BMP280_CHIP_ID 0x58  // BME280 = 0x60

// Читання N регістрів: старший біт адреси = 1 (read)
static esp_err_t reg_read(bmp280_t *dev, uint8_t reg, uint8_t *out, size_t len)
{
    uint8_t tx[32] = {0};
    uint8_t rx[32] = {0};
    tx[0] = reg | 0x80;
    spi_transaction_t t = {};
    t.length = 8 * (len + 1); // 1 байт адреси + len байтів даних
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    esp_err_t err = spi_device_transmit(dev->_dev, &t);
    if (err == ESP_OK)
        memcpy(out, &rx[1], len); // перший прийнятий байт (під час адреси) відкидаємо
    return err;
}

// Запис одного регістра: старший біт адреси = 0 (write)
static esp_err_t reg_write(bmp280_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = {(uint8_t)(reg & 0x7f), val};
    spi_transaction_t t = {};
    t.length = 16;
    t.tx_buffer = tx;
    return spi_device_transmit(dev->_dev, &t);
}

// Формули компенсації з даташита Bosch (цілочислові). comp_T рахує _t_fine,
// тож його треба викликати перед comp_P.
static int32_t comp_T(bmp280_t *dev, int32_t adc_T)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)dev->_T1 << 1))) * ((int32_t)dev->_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dev->_T1)) * (((adc_T >> 4) - ((int32_t)dev->_T1)))) >> 12) * ((int32_t)dev->_T3)) >> 14;
    dev->_t_fine = var1 + var2;
    T = (dev->_t_fine * 5 + 128) >> 8;
    return T; // температура в 0.01 °C
}

static uint32_t comp_P(bmp280_t *dev, int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)dev->_t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dev->_P6;
    var2 = var2 + ((var1 * (int64_t)dev->_P5) << 17);
    var2 = var2 + (((int64_t)dev->_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dev->_P3) >> 8) + ((var1 * (int64_t)dev->_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dev->_P1) >> 33;
    if (var1 == 0)
        return 0; // ділення на нуль
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dev->_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dev->_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dev->_P7) << 4);
    return (uint32_t)p; // тиск у форматі Q24.8: p/256 = Па
}

esp_err_t bmp280_init(bmp280_t *dev)
{
    if (dev->clock_hz <= 0)
        dev->clock_hz = 1000000;

    // 1. Ініціалізація SPI-шини
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = dev->mosi_gpio;
    buscfg.miso_io_num = dev->miso_gpio;
    buscfg.sclk_io_num = dev->sclk_gpio;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 32;
    esp_err_t err = spi_bus_initialize(dev->host, &buscfg, SPI_DMA_DISABLED);
    if (err != ESP_OK)
        return err;

    // 2. Додати пристрій BMP280 на шину
    spi_device_interface_config_t devcfg = {};
    devcfg.mode = 0; // BMP280 підтримує SPI mode 0 і 3
    devcfg.clock_speed_hz = dev->clock_hz;
    devcfg.spics_io_num = dev->cs_gpio;
    devcfg.queue_size = 1;
    err = spi_bus_add_device(dev->host, &devcfg, &dev->_dev);
    if (err != ESP_OK)
        return err;

    // 3. Перевірити chip id
    uint8_t id = 0;
    err = reg_read(dev, BMP280_REG_ID, &id, 1);
    if (err != ESP_OK)
        return err;
    ESP_LOGI(TAG, "chip id: 0x%02X (BMP280=0x58, BME280=0x60)", id);
    if (id != BMP280_CHIP_ID && id != 0x60)
        return ESP_ERR_NOT_FOUND;

    // 4. Прочитати калібрувальні коефіцієнти (little-endian)
    uint8_t b[24];
    err = reg_read(dev, BMP280_REG_CALIB, b, sizeof(b));
    if (err != ESP_OK)
        return err;
    dev->_T1 = b[0] | (b[1] << 8);
    dev->_T2 = (int16_t)(b[2] | (b[3] << 8));
    dev->_T3 = (int16_t)(b[4] | (b[5] << 8));
    dev->_P1 = b[6] | (b[7] << 8);
    dev->_P2 = (int16_t)(b[8] | (b[9] << 8));
    dev->_P3 = (int16_t)(b[10] | (b[11] << 8));
    dev->_P4 = (int16_t)(b[12] | (b[13] << 8));
    dev->_P5 = (int16_t)(b[14] | (b[15] << 8));
    dev->_P6 = (int16_t)(b[16] | (b[17] << 8));
    dev->_P7 = (int16_t)(b[18] | (b[19] << 8));
    dev->_P8 = (int16_t)(b[20] | (b[21] << 8));
    dev->_P9 = (int16_t)(b[22] | (b[23] << 8));

    // 5. Налаштувати режим:
    //    config (0xF5): t_sb=0.5ms, filter x4, spi3w_en=0 (4-wire SPI)
    err = reg_write(dev, BMP280_REG_CONFIG, (0x00 << 5) | (0x02 << 2) | 0x00);
    if (err != ESP_OK)
        return err;
    //    ctrl_meas (0xF4): osrs_t x2, osrs_p x16, mode=normal
    err = reg_write(dev, BMP280_REG_CTRL, (0x02 << 5) | (0x05 << 2) | 0x03);
    if (err != ESP_OK)
        return err;

    dev->_initialized = true;
    return ESP_OK;
}

esp_err_t bmp280_read(bmp280_t *dev, float *temp_c, float *press_hpa)
{
    uint8_t d[6];
    esp_err_t err = reg_read(dev, BMP280_REG_DATA, d, sizeof(d));
    if (err != ESP_OK)
        return err;

    int32_t adc_P = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
    int32_t adc_T = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);

    int32_t T = comp_T(dev, adc_T);  // спершу T (рахує _t_fine)
    uint32_t P = comp_P(dev, adc_P);

    if (temp_c)
        *temp_c = T / 100.0f;
    if (press_hpa)
        *press_hpa = P / 25600.0f; // /256 = Па, /100 = гПа
    return ESP_OK;
}
