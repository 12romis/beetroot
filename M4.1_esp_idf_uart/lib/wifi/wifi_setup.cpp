#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include <string.h>

#include "WifiConfig.h"
#include "wifi_setup.h"

static constexpr int ESP_MAXIMUM_RETRY = 5;

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static constexpr EventBits_t WIFI_CONNECTED_BIT = BIT0;
static constexpr EventBits_t WIFI_FAIL_BIT = BIT1;

static constexpr wifi_auth_mode_t ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD = WIFI_AUTH_WPA2_PSK;

static const char TAG[] = "wifi";
static bool isConnected = false;

// Обробник подій Wi-Fi.
// Викликається автоматично системою ESP-IDF при виникненні подій,
// таких як запуск Wi-Fi, підключення до точки доступу,
// втрата з'єднання або отримання IP-адреси.
//
// event_base  — джерело події (наприклад WIFI_EVENT або IP_EVENT)
// event_id    — конкретна подія (наприклад STA_START, GOT_IP)
// event_data  — додаткові дані, що передаються разом з подією
static void event_handler(void *arg, esp_event_base_t event_base,
						  int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		if (s_retry_num < ESP_MAXIMUM_RETRY)
		{
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		}
		else
		{
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG, "connect to the AP fail");
		isConnected = false;
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		isConnected = true;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

// Функція ініціалізації Wi-Fi у режимі Station (STA).
// Виконує налаштування Wi-Fi модуля ESP32-S3,
// створює обробники подій Wi-Fi та IP,
// задає параметри підключення (SSID, пароль),
// переводить ESP32 у режим клієнта та запускає підключення до точки доступу.
//
// Після успішного підключення подія передається у event_handler().
void wifi_init_sta(void)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&event_handler,
														NULL,
														&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&event_handler,
														NULL,
														&instance_got_ip));

	// wifi_config_t wifi_config = {
	// 	.sta = {
	// 		.ssid = ESP_WIFI_SSID,
	// 		.password = ESP_WIFI_PASS,
	// 		/* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
	// 		 * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
	// 		 * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
	// 		 * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
	// 		 */
	// 		.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
	// 		// .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
	// 		// .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
	// 	},
	// };

	wifi_config_t wifi_config = {};
	strcpy((char *)wifi_config.sta.ssid, config::WIFI_SSID);
	strcpy((char *)wifi_config.sta.password, config::WIFI_PASS);
	wifi_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
										   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
										   pdFALSE,
										   pdFALSE,
										   portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT)
	{
		ESP_LOGI(TAG, "connected to app SSID:%s", config::WIFI_SSID);
	}
	else if (bits & WIFI_FAIL_BIT)
	{
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
				 config::WIFI_SSID, config::WIFI_PASS);
	}
	else
	{
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
}

bool isWifiConnected()
{
	return isConnected;
}
