#include <string.h>
#include "esp_log.h"
#include "i2cdev.h"
#include "bmp280.h"
#include "board_config.h"
#include "env_sensor.h"

static const char *TAG = "ENV";
static bmp280_t bmp_dev;

// Ініціалізація BMP280
void bmp280_dev_init(void)
{
	memset(&bmp_dev, 0, sizeof(bmp280_t));

	esp_err_t err = bmp280_init_desc(&bmp_dev,
									 BMP280_I2C_ADDRESS_0,
									 I2C_PORT,
									 I2C_SDA_GPIO,
									 I2C_SCL_GPIO);

	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "bmp280_dev_init: %s", esp_err_to_name(err));
	}

	bmp280_params_t bmp_params;
	bmp280_init_default_params(&bmp_params);

	err = bmp280_init(&bmp_dev, &bmp_params);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "bmp280_dev_init: %s", esp_err_to_name(err));
	}
}

// Читання даних з BME280
void bme280_read(app_data_t *app_data)
{
	esp_err_t err = bmp280_read_float(&bmp_dev, &app_data->bme280_data.temperature, &app_data->bme280_data.pressure, &app_data->bme280_data.humidity);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "bme280_read_error: %s", esp_err_to_name(err));
	}

	// bmp280_read_float повертає тиск у Паскалях — переводимо в гПа (те, що
	// далі й підписано як "hPa" в логах/на екрані)
	app_data->bme280_data.pressure /= 100.0f;
}
