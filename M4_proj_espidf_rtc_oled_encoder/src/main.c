// https://github.com/UncleRus/esp-idf-lib?utm_source=chatgpt.com

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>
#include <esp_log.h>

#include "app.h"

static const char *TAG = "MAIN";

void app_main(void)
{

	// Вказівник на структуру app_data_t
	app_data_t *data = get_app_data();

	i2c_init(); // Iніціалізацію бібліотеки i2cdev
	oled_dev_init(); // Ініціалізація OLED-дисплея
	rtc_dev_init(data); // Ініціалізація RTC
	bmp280_dev_init(); // Ініціалізація BMP280
	encoder_dev_init(); // Ініціалізація енкодера



	while (1)
	{
		
		rtc_read(data); // Читання часу з RTC
		bme280_read(data); // Читання даних з BME280
		oled_update(data); // Оновлення OLED-дисплея

		ESP_LOGI(TAG, "Time: %04d-%02d-%02d %02d:%02d:%02d",
				 data->time.tm_year + 1900,
				 data->time.tm_mon + 1,
				 data->time.tm_mday,
				 data->time.tm_hour,
				 data->time.tm_min,
				 data->time.tm_sec);

		ESP_LOGI(TAG, "T: %.2f C H: %.2f %% P: %.2f hPa",
				 data->bme280_data.temperature,
				 data->bme280_data.humidity,
				 data->bme280_data.pressure);

		ESP_LOGI(TAG, "Encoder position: %d, pressed: %s",
				 data->encoder_data.position,
				 data->encoder_data.pressed ? "true" : "false");

		vTaskDelay(pdMS_TO_TICKS(2000)); // Затримка 2 секунди
	}
}
