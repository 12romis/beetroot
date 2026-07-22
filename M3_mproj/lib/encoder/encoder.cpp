#include "encoder.h"

//-------------ENCODER INIT ---------------
esp_err_t enc_init(enc_context *ctx)
{
	if (ctx == NULL)
	{
		return ESP_ERR_INVALID_ARG;
	}

	if (ctx->initialized)
	{
		return ESP_ERR_INVALID_STATE;
	}

	// Створення unit PCNT
	pcnt_unit_config_t unit_config = {};
	unit_config.low_limit = ctx->low_limit;	  // Нижня межа лічильника енкодера
	unit_config.high_limit = ctx->high_limit; // Верхня межа лічильника енкодера
	unit_config.intr_priority = 0;			  // Пріоритет переривання за замов.
	unit_config.flags.accum_count = 0;		  // Вимкнення режиму накопичення рахунку

	// Ініціалізація нового блоку PCNT
	esp_err_t err = pcnt_new_unit(&unit_config, &ctx->enc_handle);

	if (err != ESP_OK)
	{
		return err;
	}

	// Налаштування фільтра брязкоту контактів для сигналу енкодера
	pcnt_glitch_filter_config_t filter_config = {.max_glitch_ns = ctx->max_glitch_ns};
	// Увімкнення фільтрації коротких імпульсних перешкод у модулі PCNT
	err = pcnt_unit_set_glitch_filter(ctx->enc_handle, &filter_config);
	if (err != ESP_OK)
	{
		return err;
	}

	// Налаштування каналу енкодера A
	// Створення та ініціалізація структури конфігурації каналу PCNT
	pcnt_chan_config_t chan_a_config = {};
	chan_a_config.edge_gpio_num = ctx->gpio_num_a;	// Вибір GPIO для (канал A)
	chan_a_config.level_gpio_num = ctx->gpio_num_b; // Вибір GPIO для (канал В)
	chan_a_config.flags.invert_edge_input = 0;		// Без інверсії сигналу A
	chan_a_config.flags.invert_level_input = 0;		// Без інверсії сигналу В
	chan_a_config.flags.virt_edge_io_level = 0;
	chan_a_config.flags.virt_level_io_level = 0;

	// Створення каналу A PCNT
	err = pcnt_new_channel(ctx->enc_handle, &chan_a_config, &ctx->chan_a_handle);
	if (err != ESP_OK)
	{
		return err;
	}

	pcnt_chan_config_t chan_b_config = {};
	chan_b_config.edge_gpio_num = ctx->gpio_num_b;	// Вибір GPIO для (канал В)
	chan_b_config.level_gpio_num = ctx->gpio_num_a; // Вибір GPIO для (канал A)
	chan_b_config.flags.invert_edge_input = 0;		// Без інверсії сигналу A
	chan_b_config.flags.invert_level_input = 0;		// Без інверсії сигналу В
	chan_b_config.flags.virt_edge_io_level = 0;
	chan_b_config.flags.virt_level_io_level = 0;

	// Створення каналу B PCNT
	err = pcnt_new_channel(ctx->enc_handle, &chan_b_config, &ctx->chan_b_handle);
	if (err != ESP_OK)
	{
		return err;
	}

	// Квадратурна логіка
	// Канал A: при B=1 наростаючий фронт (0->1) робить -1, а спадаючий (1->0) робить +1
	err = pcnt_channel_set_edge_action(ctx->chan_a_handle,
									   PCNT_CHANNEL_EDGE_ACTION_DECREASE,
									   PCNT_CHANNEL_EDGE_ACTION_INCREASE);
	if (err != ESP_OK)
	{
		return err;
	}
	// Канал A: при B=1 зберігає базову логіку фронтів, а при B=0 — інвертує
	err = pcnt_channel_set_level_action(ctx->chan_a_handle,
										PCNT_CHANNEL_LEVEL_ACTION_KEEP,
										PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
	if (err != ESP_OK)
	{
		return err;
	}
	// Канал B: при A=1 наростаючий фронт (0->1) робить +1, а спадаючий (1->0) робить -1
	err = pcnt_channel_set_edge_action(ctx->chan_b_handle,
									   PCNT_CHANNEL_EDGE_ACTION_INCREASE,
									   PCNT_CHANNEL_EDGE_ACTION_DECREASE);
	if (err != ESP_OK)
	{
		return err;
	}
	// Канал B: при A=1 зберігає базову логіку фронтів, а при A=0 — інвертує
	err = pcnt_channel_set_level_action(ctx->chan_b_handle,
										PCNT_CHANNEL_LEVEL_ACTION_KEEP,
										PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
	if (err != ESP_OK)
	{
		return err;
	}

	err = pcnt_unit_enable(ctx->enc_handle); // Активує) апаратний блок PCNT
	if (err != ESP_OK)
	{
		return err;
	}

	err = pcnt_unit_clear_count(ctx->enc_handle); // Обнуляє значення PCNT
	if (err != ESP_OK)
	{
		return err;
	}

	err = pcnt_unit_start(ctx->enc_handle); // Запускає PCNT
	if (err != ESP_OK)
	{
		return err;
	}

	ctx->initialized = true;
	return ESP_OK;
}

// Считування поточного значення PCNT
esp_err_t enc_get_count(const enc_context *ctx, int *current_val)
{
	if (ctx == NULL || !ctx->initialized || current_val == NULL)
	{
		return ESP_ERR_INVALID_ARG;
	}

	return pcnt_unit_get_count(ctx->enc_handle, current_val);
}

//------------- Деініціалізації ENC ---------------
esp_err_t enc_deinit(enc_context *ctx)
{
	if (ctx == NULL)
	{
		return ESP_ERR_INVALID_ARG;
	}

	esp_err_t first_err = ESP_OK;
	esp_err_t err;

	// Зупинка та вимкнення PCNT
	if (ctx->enc_handle != NULL)
	{
		// Зупинка апаратного блоку PCNT
		err = pcnt_unit_stop(ctx->enc_handle);
		if (err != ESP_OK && first_err == ESP_OK)
		{
			first_err = err;
		}

		// Вимкнення апаратного блоку PCNT
		err = pcnt_unit_disable(ctx->enc_handle);
		if (err != ESP_OK && first_err == ESP_OK)
		{
			first_err = err;
		}
	}

	// Видалення каналу А PCNT
	if (ctx->chan_a_handle != NULL)
	{
		err = pcnt_del_channel(ctx->chan_a_handle);
		if (err != ESP_OK && first_err == ESP_OK)
		{
			first_err = err;
		}
	}

	// Видалення каналу В PCNT
	if (ctx->chan_b_handle != NULL)
	{
		err = pcnt_del_channel(ctx->chan_b_handle);
		if (err != ESP_OK && first_err == ESP_OK)
		{
			first_err = err;
		}
	}

	// Видалення блоку PCNT
	if (ctx->enc_handle != NULL)
	{
		err = pcnt_del_unit(ctx->enc_handle);
		if (err != ESP_OK && first_err == ESP_OK)
		{
			first_err = err;
		}
	}

	// Очищення дескрипторів
	ctx->chan_a_handle = NULL;
	ctx->chan_b_handle = NULL;
	ctx->enc_handle = NULL;

	ctx->initialized = false;

	return first_err;
}
