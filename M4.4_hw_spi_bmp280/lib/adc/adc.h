#pragma once

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

// #include "esp_err.h"

typedef struct
{
	adc_unit_t unit_id;
	adc_bitwidth_t bitwidth;
	adc_channel_t *channels;
	adc_oneshot_unit_handle_t adc_handle; // Дескриптор апаратного модуля ADC 1
	uint8_t channel_count;
	adc_cali_handle_t *cali_handles;
	bool initialized;
} adc_context;

esp_err_t adc_init(adc_context *ctx);
esp_err_t adc_read_all_voltage_mv(const adc_context *ctx, int *raw, int *voltage_mv);
esp_err_t adc_deinit(adc_context *ctx);
