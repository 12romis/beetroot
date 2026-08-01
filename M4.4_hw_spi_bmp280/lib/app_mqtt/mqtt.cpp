#include "esp_log.h"
#include "esp_crt_bundle.h"

#include "mqtt.h"
#include "MqttConfig.h"

esp_mqtt_client_handle_t mqttClient = NULL;
static mqtt_message_handler_t s_handler = nullptr;

static const char TAG[] = "mqtt";


void mqtt_set_message_handler(mqtt_message_handler_t handler) { s_handler = handler; }

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
	// esp_mqtt_event_handle_t event = event_data;
	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

	esp_mqtt_client_handle_t client = event->client;
	// int msg_id;
	switch ((esp_mqtt_event_id_t)event_id)
	{
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

		// Підписник: приймає команди ON/OFF
		esp_mqtt_client_subscribe(client, MQTT_LED_COMMANDS, 0);
		// Підписник: приймає кут сервопривода
		esp_mqtt_client_subscribe(client, MQTT_SERVO_ANGLE, 0);

		// Издатель: отправляет текущее состояние LED
		esp_mqtt_client_publish(client, MQTT_STATUS, getled_state() ? "ON" : "OFF", 0, 0, 0);
		break;

	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ", event->msg_id, (uint8_t)*event->data);
		break;

	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;

	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;

	case MQTT_EVENT_DATA:
	{
		// A message arrived on a topic we're subscribed to.
		// ESP_LOGI(TAG, "MQTT_EVENT_DATA");
		// printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
		// printf("DATA=%.*s\r\n", event->data_len, event->data);

		// event->topic/event->data are NOT null-terminated and may be
		// fragments of a larger message, so copy them into local buffers
		// and add a '\0' before treating them as C strings.
		char topic[event->topic_len + 1];
		memcpy(topic, event->topic, event->topic_len);
		topic[event->topic_len] = '\0';

		char data[event->data_len + 1];
		memcpy(data, event->data, event->data_len);
		data[event->data_len] = '\0';

		if (s_handler) s_handler(topic, data, mqttClient);

		break;
	}
	case MQTT_EVENT_ERROR:
		ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
		if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
		{
			ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
			ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
			ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
					 strerror(event->error_handle->esp_transport_sock_errno));
		}
		else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
		{
			ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
		}
		else
		{
			ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
		}
		break;

	default:
		ESP_LOGI(TAG, "Other event id:%d", event->event_id);
		break;
	}
}

void mqtt_start(void)
{
	esp_mqtt_client_config_t mqtt_cfg = {};

	mqtt_cfg.broker.address.uri = config::MQTT_BROKER_URI;
	mqtt_cfg.broker.address.port = config::MQTT_BROKER_PORT;
	mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
	mqtt_cfg.credentials.username = config::MQTT_USERNAME;
	mqtt_cfg.credentials.authentication.password = config::MQTT_PASSWORD;

	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
	mqttClient = client;
	esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(client);
}

esp_mqtt_client_handle_t get_mqtt_client()
{
	return mqttClient;
}