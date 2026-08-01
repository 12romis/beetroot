#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h" // новий I2C-драйвер (той самий, що використовує nopnop)

#include "ssd1306.h" // nopnop2002 — драйвер OLED SSD1306 (новий драйвер)

static const char *TAG = "main";

// --- Спільна I2C-шина: OLED (0x3C) + DS1307 (0x68) на одних пінах ---
// nopnop створює шину (i2c_new_master_bus) на I2C_PORT_0; DS1307 підключаємо
// до тієї ж шини окремим пристроєм зі СВОЄЮ частотою (новий драйвер це вміє).
#define I2C_SDA_GPIO GPIO_NUM_8
#define I2C_SCL_GPIO GPIO_NUM_9
#define DS1307_ADDR 0x68
#define DS1307_FREQ_HZ 100000 // 100 кГц — максимум для DS1307

// Назви днів тижня. Індекс 0 = неділя (відповідає tm_wday)
static const char *daysOfTheWeek[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"};

// --- DS1307: дані в регістрах у форматі BCD ---
static inline uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0f); }
static inline uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

// Читає час з DS1307 (7 регістрів, починаючи з 0x00)
static esp_err_t ds1307_read(i2c_master_dev_handle_t rtc, struct tm *t)
{
    uint8_t reg = 0x00;
    uint8_t d[7];
    // записати адресу регістра + одразу прочитати 7 байтів
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

// Виводить день тижня, дату і час на OLED-дисплей.
// Екран 128x64 = 8 сторінок по 8px; символ 8px завширшки -> 16 символів у рядку.
static void updateDisplay(SSD1306_t *oled, const struct tm *t)
{
    char line[40];
    const char *dayName = (t->tm_wday >= 0 && t->tm_wday < 7)
                              ? daysOfTheWeek[t->tm_wday]
                              : "?";

    // Сторінка 0 — день тижня. Доповнюємо пробілами до 16 символів (%-16s),
    // щоб новий текст сам затирав старий без окремого clear_line — без мерехтіння.
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
}

extern "C" void app_main(void)
{
    // --- OLED: nopnop створює нову I2C-шину і додає дисплей як пристрій ---
    SSD1306_t oled;
    i2c_master_init(&oled, I2C_SDA_GPIO, I2C_SCL_GPIO, -1); // reset = -1 (немає піна)
    ssd1306_init(&oled, 128, 64);
    ssd1306_clear_screen(&oled, false);
    ssd1306_contrast(&oled, 0xff);

    // --- DS1307 на ТУ САМУ шину, окремим пристроєм зі своєю частотою 100 кГц ---
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

        ESP_LOGI(TAG, "%s %02d.%02d.%04d %02d:%02d:%02d",
                 daysOfTheWeek[now.tm_wday], now.tm_mday, now.tm_mon + 1,
                 now.tm_year + 1900, now.tm_hour, now.tm_min, now.tm_sec);

        updateDisplay(&oled, &now);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
