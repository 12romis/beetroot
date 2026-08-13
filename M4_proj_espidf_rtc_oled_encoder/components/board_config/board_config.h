#pragma once

#include <driver/gpio.h>
#include <driver/i2c.h>

// Конфігурація I2C ESP32-S3 (спільна шина для OLED/RTC/BMP280)
static const gpio_num_t I2C_PORT = I2C_NUM_0;
static const gpio_num_t I2C_SDA_GPIO = GPIO_NUM_8;
static const gpio_num_t I2C_SCL_GPIO = GPIO_NUM_9;

//------- Енкодер ------------
// Фактична розпіновка: CLK->GPIO12, DT->GPIO11, SW->GPIO10
#define ENC_A GPIO_NUM_12
#define ENC_B GPIO_NUM_11
#define ENC_BTN GPIO_NUM_10
