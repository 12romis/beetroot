#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <i2cdev.h>

#define SSD1306_I2C_ADDR 0x3C

// Простая структура дескриптора дисплея, обернутая вокруг i2cdev
typedef struct
{
    i2c_dev_t i2c_dev;
    uint8_t width;
    uint8_t height;
} ssd1306_t;

// Прототипы базовых функций для текста и цифр
esp_err_t ssd1306_init_desc(ssd1306_t *dev, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio);
esp_err_t ssd1306_init_display(ssd1306_t *dev);
esp_err_t ssd1306_clear(ssd1306_t *dev);
esp_err_t ssd1306_draw_string(ssd1306_t *dev, uint8_t x, uint8_t page, const char *str);
