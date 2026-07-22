#pragma once

#include "mqtt_client.h"

inline constexpr char MQTT_COMMANDS[] = "esp32test/commands";
inline constexpr char MQTT_STATUS[] = "esp32test/status";
inline constexpr char MQTT_ENC_POS[] = "esp32test/encoder/position";

bool getled_state();

void mqtt_start();

typedef void (*mqtt_message_handler_t)(const char *topic, const char *data, esp_mqtt_client_handle_t client);

void mqtt_set_message_handler(mqtt_message_handler_t handler);

esp_mqtt_client_handle_t get_mqtt_client();
