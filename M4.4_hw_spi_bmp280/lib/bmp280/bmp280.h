#pragma once

// Драйвер BMP280 / BME280 по SPI (4-wire) для ESP-IDF (новий spi_master).
// Використання:
//   bmp280_t bmp = {};
//   bmp.host = SPI2_HOST;
//   bmp.sclk_gpio = GPIO_NUM_12;
//   bmp.mosi_gpio = GPIO_NUM_11;   // SDA/SDI на модулі
//   bmp.miso_gpio = GPIO_NUM_13;   // SDO на модулі
//   bmp.cs_gpio   = GPIO_NUM_10;   // CSB на модулі
//   bmp.clock_hz  = 1000000;       // 0 -> дефолт 1 МГц
//   ESP_ERROR_CHECK(bmp280_init(&bmp));
//   float t, p; bmp280_read(&bmp, &t, &p);

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

typedef struct
{
    // --- Заповнює користувач перед bmp280_init() ---
    spi_host_device_t host; // напр. SPI2_HOST
    gpio_num_t sclk_gpio;   // SCL/SCK
    gpio_num_t mosi_gpio;   // SDA/SDI
    gpio_num_t miso_gpio;   // SDO
    gpio_num_t cs_gpio;     // CSB
    int clock_hz;           // частота SPI; 0 -> 1 МГц (BMP280 тримає до 10 МГц)

    // --- Внутрішнє (заповнює бібліотека) ---
    spi_device_handle_t _dev;
    uint16_t _T1;
    int16_t _T2, _T3;
    uint16_t _P1;
    int16_t _P2, _P3, _P4, _P5, _P6, _P7, _P8, _P9;
    int32_t _t_fine;
    bool _initialized;
} bmp280_t;

#ifdef __cplusplus
extern "C"
{
#endif

    // Ініціалізує SPI-шину й пристрій, перевіряє chip id, читає калібрування
    // і вмикає нормальний режим вимірювання.
    // Повертає ESP_ERR_NOT_FOUND, якщо chip id не збігається (нема зв'язку).
    esp_err_t bmp280_init(bmp280_t *dev);

    // Читає поточні температуру (°C) і тиск (гПа). Будь-який з покажчиків може бути NULL.
    esp_err_t bmp280_read(bmp280_t *dev, float *temp_c, float *press_hpa);

#ifdef __cplusplus
}
#endif
