#include "esp_log.h"
#include "i2cdev.h"
#include "i2c_bus.h"

static const char *TAG = "I2C";

// Iніціалізацію бібліотеки i2cdev
void i2c_init(void)
{
	esp_err_t err = i2cdev_init();
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "i2c_init: %s", esp_err_to_name(err));
	}
}
