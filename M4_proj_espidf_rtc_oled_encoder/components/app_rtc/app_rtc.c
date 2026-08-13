#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "i2cdev.h"
#include "ds1307.h"
#include "board_config.h"
#include "app_rtc.h"

static const char *TAG = "RTC";
static i2c_dev_t rtc_dev;

// Ініціалізація поточного часу з компіляції програми
static void rtc_set_time(struct tm *time)
{
	static const char *months[] =
		{
			"Jan", "Feb", "Mar", "Apr",
			"May", "Jun", "Jul", "Aug",
			"Sep", "Oct", "Nov", "Dec"};

	char month[4];
	int year;
	memset(time, 0, sizeof(struct tm));

	sscanf(__DATE__, "%3s %d %d",
		   month,
		   &time->tm_mday,
		   &year);

	time->tm_year = year - 1900;

	for (int i = 0; i < 12; i++)
	{
		if (strcmp(month, months[i]) == 0)
		{
			time->tm_mon = i;
			break;
		}
	}

	sscanf(__TIME__, "%d:%d:%d",
		   &time->tm_hour,
		   &time->tm_min,
		   &time->tm_sec);
}

// Ініціалізація RTC
void rtc_dev_init(app_data_t *app_data)
{
	memset(&rtc_dev, 0, sizeof(i2c_dev_t));

	esp_err_t err = ds1307_init_desc(&rtc_dev,
									 I2C_PORT,
									 I2C_SDA_GPIO,
									 I2C_SCL_GPIO);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "rtc_dev_init: %s", esp_err_to_name(err));
	}

	rtc_set_time(&app_data->time);

	err = ds1307_set_time(&rtc_dev, &app_data->time);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "rtc_dev_init: %s", esp_err_to_name(err));
	}
}

// Читання часу з RTC
void rtc_read(app_data_t *app_data)
{
	esp_err_t err = ds1307_get_time(&rtc_dev, &app_data->time);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "rtc_read_error: %s", esp_err_to_name(err));
	}
}

// Запис app_data->time у RTC
esp_err_t rtc_write_time(app_data_t *app_data)
{
	esp_err_t err = ds1307_set_time(&rtc_dev, &app_data->time);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "rtc_write_time: %s", esp_err_to_name(err));
	}
	return err;
}
