#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h" // новий I2C-драйвер (той самий, що використовує nopnop)
#include "driver/spi_master.h" // SPI для BMP280

#include "ssd1306.h" // nopnop2002 — драйвер OLED SSD1306 (новий драйвер)

static const char *TAG = "main";

// --- Спільна I2C-шина: OLED (0x3C) + DS1307 (0x68) на одних пінах ---
#define I2C_SDA_GPIO GPIO_NUM_8
#define I2C_SCL_GPIO GPIO_NUM_9
#define DS1307_ADDR 0x68
#define DS1307_FREQ_HZ 100000 // 100 кГц — максимум для DS1307

// --- BMP280 по SPI (4-wire) ---
#define BMP_SCLK_GPIO GPIO_NUM_12 // SCL на модулі
#define BMP_MOSI_GPIO GPIO_NUM_11 // SDA/SDI на модулі
#define BMP_MISO_GPIO GPIO_NUM_13 // SDO на модулі
#define BMP_CS_GPIO GPIO_NUM_10    // CSB на модулі
#define BMP_HOST SPI2_HOST

// Назви днів тижня. Індекс 0 = неділя (відповідає tm_wday)
static const char *daysOfTheWeek[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"};

// =================== DS1307 (I2C) ===================
// Дані в регістрах у форматі BCD
static inline uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0f); }
static inline uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

// Читає час з DS1307 (7 регістрів, починаючи з 0x00)
static esp_err_t ds1307_read(i2c_master_dev_handle_t rtc, struct tm *t)
{
    uint8_t reg = 0x00;
    uint8_t d[7];
    esp_err_t err = i2c_master_transmit_receive(rtc, &reg, 1, d, sizeof(d), 100);
    if (err != ESP_OK)
        return err;

    t->tm_sec = bcd2dec(d[0] & 0x7f); // біт7 = CH (clock halt)
    t->tm_min = bcd2dec(d[1] & 0x7f);
    t->tm_hour = bcd2dec(d[2] & 0x3f); // 24-годинний режим
    t->tm_wday = (d[3] & 0x07) - 1;    // DS1307: 1-7  ->  tm_wday 0-6
    t->tm_mday = bcd2dec(d[4] & 0x3f);
    t->tm_mon = bcd2dec(d[5] & 0x1f) - 1; // 1-12  ->  0-11
    t->tm_year = bcd2dec(d[6]) + 100;     // 20xx  ->  роки від 1900
    return ESP_OK;
}

// Записує час у DS1307 і запускає годинник (біт CH = 0)
static esp_err_t ds1307_write(i2c_master_dev_handle_t rtc, const struct tm *t)
{
    uint8_t d[8];
    d[0] = 0x00;                       // стартовий регістр
    d[1] = dec2bcd(t->tm_sec) & 0x7f;  // CH=0 -> годинник іде
    d[2] = dec2bcd(t->tm_min);
    d[3] = dec2bcd(t->tm_hour) & 0x3f; // 24h
    d[4] = dec2bcd(t->tm_wday + 1);
    d[5] = dec2bcd(t->tm_mday);
    d[6] = dec2bcd(t->tm_mon + 1);
    d[7] = dec2bcd(t->tm_year - 100);
    return i2c_master_transmit(rtc, d, sizeof(d), 100);
}

// =================== BMP280 (SPI) ===================
// Калібрувальні коефіцієнти (читаються з чипа при старті)
static struct
{
    uint16_t T1;
    int16_t T2, T3;
    uint16_t P1;
    int16_t P2, P3, P4, P5, P6, P7, P8, P9;
} cal;
static int32_t t_fine; // проміжна температура для компенсації тиску

// Читання N регістрів BMP280 по SPI: старший біт адреси = 1 (read)
static esp_err_t bmp_read(spi_device_handle_t dev, uint8_t reg, uint8_t *out, size_t len)
{
    uint8_t tx[32] = {0};
    uint8_t rx[32] = {0};
    tx[0] = reg | 0x80; // MSB=1 -> читання
    spi_transaction_t t = {};
    t.length = 8 * (len + 1); // 1 байт адреси + len байтів даних
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    esp_err_t err = spi_device_transmit(dev, &t);
    if (err == ESP_OK)
        memcpy(out, &rx[1], len); // перший прийнятий байт (під час адреси) відкидаємо
    return err;
}

// Запис одного регістра BMP280: старший біт адреси = 0 (write)
static esp_err_t bmp_write(spi_device_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = {(uint8_t)(reg & 0x7f), val};
    spi_transaction_t t = {};
    t.length = 16;
    t.tx_buffer = tx;
    return spi_device_transmit(dev, &t);
}

// Перевіряє chip id, читає калібрування, налаштовує режим вимірювання
static esp_err_t bmp280_init(spi_device_handle_t dev)
{
    uint8_t id = 0;
    esp_err_t err = bmp_read(dev, 0xD0, &id, 1);
    if (err != ESP_OK)
        return err;
    ESP_LOGI(TAG, "BMP280 chip id: 0x%02X (очікуємо 0x58)", id);
    if (id != 0x58)
        return ESP_ERR_NOT_FOUND;

    uint8_t b[24];
    err = bmp_read(dev, 0x88, b, sizeof(b)); // калібрування 0x88..0x9F
    if (err != ESP_OK)
        return err;
    cal.T1 = b[0] | (b[1] << 8);
    cal.T2 = (int16_t)(b[2] | (b[3] << 8));
    cal.T3 = (int16_t)(b[4] | (b[5] << 8));
    cal.P1 = b[6] | (b[7] << 8);
    cal.P2 = (int16_t)(b[8] | (b[9] << 8));
    cal.P3 = (int16_t)(b[10] | (b[11] << 8));
    cal.P4 = (int16_t)(b[12] | (b[13] << 8));
    cal.P5 = (int16_t)(b[14] | (b[15] << 8));
    cal.P6 = (int16_t)(b[16] | (b[17] << 8));
    cal.P7 = (int16_t)(b[18] | (b[19] << 8));
    cal.P8 = (int16_t)(b[20] | (b[21] << 8));
    cal.P9 = (int16_t)(b[22] | (b[23] << 8));

    // config (0xF5): t_sb=0.5ms, filter x4, spi3w_en=0 (4-wire SPI)
    err = bmp_write(dev, 0xF5, (0x00 << 5) | (0x02 << 2) | 0x00);
    if (err != ESP_OK)
        return err;
    // ctrl_meas (0xF4): osrs_t x2, osrs_p x16, mode=normal
    return bmp_write(dev, 0xF4, (0x02 << 5) | (0x05 << 2) | 0x03);
}

// Формули компенсації з даташита Bosch (32/64-бітні цілочислові)
static int32_t bmp280_comp_T(int32_t adc_T)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)cal.T1 << 1))) * ((int32_t)cal.T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)cal.T1)) * (((adc_T >> 4) - ((int32_t)cal.T1)))) >> 12) * ((int32_t)cal.T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T; // температура в 0.01 °C
}

static uint32_t bmp280_comp_P(int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)cal.P6;
    var2 = var2 + ((var1 * (int64_t)cal.P5) << 17);
    var2 = var2 + (((int64_t)cal.P4) << 35);
    var1 = ((var1 * var1 * (int64_t)cal.P3) >> 8) + ((var1 * (int64_t)cal.P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)cal.P1) >> 33;
    if (var1 == 0)
        return 0; // ділення на нуль
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)cal.P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)cal.P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)cal.P7) << 4);
    return (uint32_t)p; // тиск у форматі Q24.8: p/256 = Па
}

// Читає сирі дані й повертає температуру (°C) і тиск (гПа)
static esp_err_t bmp280_read_data(spi_device_handle_t dev, float *temp_c, float *press_hpa)
{
    uint8_t d[6];
    esp_err_t err = bmp_read(dev, 0xF7, d, sizeof(d)); // press(3) + temp(3)
    if (err != ESP_OK)
        return err;
    int32_t adc_P = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
    int32_t adc_T = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);

    *temp_c = bmp280_comp_T(adc_T) / 100.0f; // спершу T (рахує t_fine)
    *press_hpa = bmp280_comp_P(adc_P) / 25600.0f; // /256 = Па, /100 = гПа
    return ESP_OK;
}

// =================== Дисплей ===================
// Екран 128x64 = 8 сторінок по 8px; символ 8px завширшки -> 16 символів у рядку.
static void updateDisplay(SSD1306_t *oled, const struct tm *t, float temp_c, float press_hpa)
{
    char line[40];
    char val[24];

    const char *dayName = (t->tm_wday >= 0 && t->tm_wday < 7)
                              ? daysOfTheWeek[t->tm_wday]
                              : "?";

    // Сторінка 0 — день тижня. %-16s доповнює пробілами до 16 символів,
    // щоб новий текст затирав старий без окремого clear_line (без мерехтіння).
    snprintf(line, sizeof(line), "%-16s", dayName);
    ssd1306_display_text(oled, 0, line, strlen(line), false);

    // Сторінка 2 — дата
    snprintf(line, sizeof(line), "%02d.%02d.%04d",
             t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
    ssd1306_display_text(oled, 2, line, strlen(line), false);

    // Сторінка 4 — час
    snprintf(line, sizeof(line), "%02d:%02d:%02d",
             t->tm_hour, t->tm_min, t->tm_sec);
    ssd1306_display_text(oled, 4, line, strlen(line), false);

    // Сторінка 6 — температура і тиск (теж добиваємо до 16 символів)
    snprintf(val, sizeof(val), "%.1fC | %.0fhPa", temp_c, press_hpa);
    snprintf(line, sizeof(line), "%-18s", val);
    ssd1306_display_text(oled, 6, line, strlen(line), false);
}

extern "C" void app_main(void)
{
    // --- OLED: nopnop створює нову I2C-шину і додає дисплей як пристрій ---
    SSD1306_t oled;
    i2c_master_init(&oled, I2C_SDA_GPIO, I2C_SCL_GPIO, -1); // reset = -1 (немає піна)
    ssd1306_init(&oled, 128, 64);
    ssd1306_clear_screen(&oled, false);
    ssd1306_contrast(&oled, 0xff);

    // --- DS1307 на ТУ САМУ I2C-шину, окремим пристроєм зі своєю частотою 100 кГц ---
    i2c_master_dev_handle_t rtc;
    i2c_device_config_t rtc_cfg = {};
    rtc_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    rtc_cfg.device_address = DS1307_ADDR;
    rtc_cfg.scl_speed_hz = DS1307_FREQ_HZ;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(oled._i2c_bus_handle, &rtc_cfg, &rtc));

    // --- Один раз виставити час DS1307 ---
    // struct tm set = {};
    // set.tm_year = 2026 - 1900; set.tm_mon = 8 - 1; set.tm_mday = 1;
    // set.tm_wday = 6; set.tm_hour = 15; set.tm_min = 10; set.tm_sec = 15;
    // ESP_ERROR_CHECK(ds1307_write(rtc, &set));

    // --- BMP280 по SPI ---
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = BMP_MOSI_GPIO;
    buscfg.miso_io_num = BMP_MISO_GPIO;
    buscfg.sclk_io_num = BMP_SCLK_GPIO;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 32;
    ESP_ERROR_CHECK(spi_bus_initialize(BMP_HOST, &buscfg, SPI_DMA_DISABLED));

    spi_device_interface_config_t devcfg = {};
    devcfg.mode = 0;                     // BMP280 підтримує SPI mode 0 і 3
    devcfg.clock_speed_hz = 1000000;     // 1 МГц (BMP280 тримає до 10 МГц)
    devcfg.spics_io_num = BMP_CS_GPIO;
    devcfg.queue_size = 1;
    spi_device_handle_t bmp;
    ESP_ERROR_CHECK(spi_bus_add_device(BMP_HOST, &devcfg, &bmp));

    if (bmp280_init(bmp) != ESP_OK)
        ESP_LOGE(TAG, "BMP280 не знайдено (перевір SPI-розводку)");

    struct tm now;
    while (1)
    {
        esp_err_t err = ds1307_read(rtc, &now);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "DS1307 read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        float temp_c = 0, press_hpa = 0;
        esp_err_t berr = bmp280_read_data(bmp, &temp_c, &press_hpa);
        if (berr != ESP_OK)
            ESP_LOGW(TAG, "BMP280 read failed: %s", esp_err_to_name(berr));

        ESP_LOGI(TAG, "%s %02d.%02d.%04d %02d:%02d:%02d | %.1f C  %.1f hPa",
                 daysOfTheWeek[now.tm_wday], now.tm_mday, now.tm_mon + 1,
                 now.tm_year + 1900, now.tm_hour, now.tm_min, now.tm_sec,
                 temp_c, press_hpa);

        updateDisplay(&oled, &now, temp_c, press_hpa);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
