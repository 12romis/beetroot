#pragma once

#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_err.h"

typedef struct
{
	pcnt_unit_handle_t enc_handle; // Дескриптор апаратного модуля PCNT
	int32_t low_limit;
	int32_t high_limit;
	uint16_t max_glitch_ns;
	pcnt_channel_handle_t chan_a_handle; // Дескриптор каналу А PCNT
	pcnt_channel_handle_t chan_b_handle; // Дескриптор каналу B PCNT
	gpio_num_t gpio_num_a;
	gpio_num_t gpio_num_b;
	bool initialized;

} enc_context;

// API ENCODER
esp_err_t enc_init(enc_context *ctx);
esp_err_t enc_get_count(const enc_context *ctx, int *current_val);
esp_err_t enc_deinit(enc_context *ctx);
