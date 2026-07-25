#pragma once

#include "driver/ledc.h"
#include "esp_err.h"
#include "driver/gpio.h"

typedef struct
{
	gpio_num_t gpio_num;
	ledc_timer_t timer_num;
	ledc_channel_t channel;
	uint32_t freq_hz;
	ledc_timer_bit_t duty_resolution;
	ledc_mode_t speed_mode;
	bool initialized;
} pwm_context;

esp_err_t pwm_init(pwm_context *ctx);
esp_err_t pwm_set_duty(const pwm_context *ctx, int duty);
esp_err_t pwm_deinit(pwm_context *ctx);