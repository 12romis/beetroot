/*

1. MQTT Bhai — мобільний MQTT-клієнт.

2. MQTT Explorer — клієнт MQTT для ПК.
   https://mqtt-explorer.com/

3. HiveMQ WebSocket Client — вебклієнт MQTT.
   https://www.hivemq.com/demos/websocket-client/

*/

#include <stdio.h>
#include <time.h>
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "mqtt.h"
#include "wifi_setup.h"

static const char TAG[] = "main";
static bool led_state = false;
constexpr gpio_num_t LED_GPIO = GPIO_NUM_16;

void handle_mqtt_message(const char *topic, const char *data)
{
	if (topic == NULL || data == NULL)
	{
		return;
	}

	if (strcmp(topic, MQTT_COMMANDS) != 0)
	{
		return;
	}

	// Get the MQTT client handle
	esp_mqtt_client_handle_t client = get_mqtt_client();

	if (client == NULL)
	{
		ESP_LOGE(TAG, "MQTT client is NULL");
		return;
	}

	// Command: ON/OFF
	if (strcmp(data, "ON") == 0)
	{
		ESP_LOGI(TAG, "Command: LED ON");

		if (gpio_set_level(LED_GPIO, 1) == ESP_OK)
		{
			led_state = true;
			esp_mqtt_client_publish(client, MQTT_STATUS, led_state ? "ON" : "OFF", 0, 0, 0);
		}

		else
		{
			ESP_LOGE(TAG, "Failed to set LED ON");
		}
	}

	else if (strcmp(data, "OFF") == 0)
	{
		ESP_LOGI(TAG, "Command: LED OFF");

		if (gpio_set_level(LED_GPIO, 0) == ESP_OK)
		{
			led_state = false;
			esp_mqtt_client_publish(client, MQTT_STATUS, led_state ? "ON" : "OFF", 0, 0, 0);
		}
		else
		{
			ESP_LOGE(TAG, "Failed to set LED OFF");
		}
	}

	// Status command: returns the current state of the LED
	else if (strcmp(data, "STATUS") == 0)
	{
		if (esp_mqtt_client_publish(client, MQTT_STATUS, led_state ? "ON" : "OFF", 0, 0, 0) < 0)
		{
			ESP_LOGE(TAG, "Failed to publish status");
		}
		else
		{
			ESP_LOGI(TAG, "Status sent");
		}
	}
	else
	{
		ESP_LOGW(TAG, "Unknown command: %s", data);
	}
}

bool getled_state()
{
	return led_state;
}

extern "C" void app_main(void)
{
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

	// Сконфігурувати LED GPIO
	gpio_config_t io_conf = {};
	io_conf.pin_bit_mask = (1ULL << LED_GPIO);
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK(gpio_config(&io_conf));
	ESP_ERROR_CHECK(gpio_set_level(LED_GPIO, 1));

	wifi_init_sta();

	if (isWifiConnected())
	{
		mqtt_set_message_handler(handle_mqtt_message);
		mqtt_start();
	}

	while (1)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
