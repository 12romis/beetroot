#pragma once

#include <esp_err.h>
#include <string.h>
#include <stdbool.h>

typedef struct
{
	float temperature;
	float humidity;
	float pressure;
} bme280_data_t;

typedef struct
{
	int position;
	bool pressed;
} encoder_data_t;

typedef struct
{
	bme280_data_t bme280_data;
	struct tm time;
	encoder_data_t encoder_data;
} app_data_t;

app_data_t *get_app_data(void);

void i2c_init();
// Ініціалізація OLED-дисплея
void oled_dev_init();
// Ініціалізація RTC
void rtc_dev_init(app_data_t *app_data);
// Ініціалізація BMP280
void bmp280_dev_init();
// Ініціалізація енкодера
void encoder_dev_init();

// Читання часу з RTC
void rtc_read(app_data_t *app_data);
// Читання даних з BME280
void bme280_read(app_data_t *app_data);
// Оновлення OLED-дисплея
void oled_update(app_data_t *app_data);
