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

typedef enum
{
	MAIN_SCREEN,
	CHANGE_TIME_SCREEN,
	CHANGE_DATE_SCREEN,
	ENV_HISTORY_SCREEN,
} screen_mode_t;	

typedef enum { 
	FIELD_NORMAL, 
	FIELD_SELECTED, 
	FIELD_EDITING 
} field_state_t;

typedef struct
{
	bme280_data_t bme280_data;
	struct tm time;
	encoder_data_t encoder_data;
	screen_mode_t screen_mode;
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
// Обробка кліку кнопки енкодера (навігація по меню/екранах)
void handle_click_event(app_data_t *app_data);
