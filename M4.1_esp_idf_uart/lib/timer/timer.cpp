#include "timer.h"

//------------- Налаштування таймера + каналу -------
esp_err_t pwm_init(pwm_context *ctx)
{
	// Перевірка коректності аргументів
	if (ctx == NULL)
	{
		return ESP_ERR_INVALID_ARG;
	}

	if (ctx->initialized)
	{
		return ESP_ERR_INVALID_STATE;
	}

	//----Налаштування таймера ---------------
	ledc_timer_config_t timer_config = {};
	timer_config.speed_mode = ctx->speed_mode;			 // Режим швидкостий/повільний таймер
	timer_config.duty_resolution = ctx->duty_resolution; // Роздільна здатність ШІМ
	timer_config.timer_num = ctx->timer_num;			 // Номер таймера LEDC
	timer_config.freq_hz = ctx->freq_hz;				 // Частота сигналу ШІМ (Гц)
	timer_config.clk_cfg = LEDC_AUTO_CLK;				 // Автоматичний вибір джерела CLK

	esp_err_t err = ledc_timer_config(&timer_config);
	if (err != ESP_OK)
	{
		return err;
	}

	//----Налаштування каналу ---------------
	ledc_channel_config_t channel_config = {};
	channel_config.gpio_num = ctx->gpio_num;					// Номер GPIO для виходу ШІМ
	channel_config.speed_mode = ctx->speed_mode;				// Режим швидкості LEDC
	channel_config.channel = ctx->channel;						// Номер каналу ШІМ
	channel_config.timer_sel = ctx->timer_num;					// Таймер, прив'язаний до каналу
	channel_config.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD; // Режим роботи у сні

	err = ledc_channel_config(&channel_config);
	if (err != ESP_OK)
	{
		return err;
	}

	ctx->initialized = true;
	return ESP_OK;
}

//----Налаштування + встановлення  PWM (duty) ------------------
esp_err_t pwm_set_duty(const pwm_context *ctx, int duty)
{
	// Перевірка коректності аргументів
	if (ctx == NULL || !ctx->initialized || duty < 0)
	{
		return ESP_ERR_INVALID_ARG;
	}

	// Налаштування duty для PWM каналу
	esp_err_t err = ledc_set_duty(ctx->speed_mode, ctx->channel, duty);
	if (err != ESP_OK)
	{
		return err;
	}
	// Встановлення нового значення duty
	return ledc_update_duty(ctx->speed_mode, ctx->channel);
}

//------------- Деініціалізації таймера ---------------
esp_err_t pwm_deinit(pwm_context *ctx)
{
	// Перевірка коректності аргументів
	if (ctx == NULL)
	{
		return ESP_ERR_INVALID_ARG;
	}

	// Перевірка деініціалізації таймера
	if (!ctx->initialized)
	{
		return ESP_OK;
	}

	// Зупинка PWM сигналу на заданому каналі
	esp_err_t first_err = ledc_stop(ctx->speed_mode, ctx->channel, 0);

	ledc_timer_config_t timer_config = {};
	timer_config.speed_mode = ctx->speed_mode;
	timer_config.timer_num = ctx->timer_num;
	timer_config.deconfigure = true;

	esp_err_t err = ledc_timer_config(&timer_config);
	if (first_err == ESP_OK && err != ESP_OK)
	{
		first_err = err;
	}
	// PWM - неініціалізований
	ctx->initialized = false;
	return first_err;
}
