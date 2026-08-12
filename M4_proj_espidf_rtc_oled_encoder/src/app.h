#pragma once

#include <esp_err.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

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

typedef enum
{
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
	int selected_field;   // індекс поля в фокусі на поточному екрані (включно з "Back")
	bool field_editing;   // чи зараз редагується значення selected_field
} app_data_t;

// Вказівник на спільний стан застосунку (визначений в app.c)
app_data_t *get_app_data(void);
