#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "adc.h"

// Звільнення дескрипторів калібрування ADC та очищення ресурсів калібрування
static esp_err_t adc_release_del_cali(adc_context *ctx)
{
	// Перевірка контексту та масиву калібрування
	if (ctx == NULL || ctx->cali_handles == NULL)
	{
		return ESP_ERR_NOT_FOUND;
	}

	esp_err_t first_err = ESP_OK;

	// Перебір усіх ADC каналів
	for (uint8_t i = 0; i < ctx->channel_count; i++)
	{
		if (ctx->cali_handles[i] == NULL)
		{
			continue;
		}

		// Видалення об'єкта калібрування ADC
		esp_err_t err = adc_cali_delete_scheme_curve_fitting(ctx->cali_handles[i]);

		// Скидає дескриптор калібрування
		ctx->cali_handles[i] = NULL;

		if (first_err == ESP_OK && err != ESP_OK)
		{
			first_err = err;
		}
	}

	return first_err;
}

//-------------ADC1 Config + Calibration---------------
esp_err_t adc_init(adc_context *ctx)
{
	if (ctx == NULL || ctx->channels == NULL || ctx->cali_handles == NULL || ctx->channel_count == 0)
	{
		return ESP_ERR_INVALID_ARG;
	}

	if (ctx->initialized)
	{
		return ESP_ERR_INVALID_STATE;
	}

	for (uint8_t i = 0; i < ctx->channel_count; i++)
	{
		ctx->cali_handles[i] = NULL;
	}

	//-------------ADC1 Config ---------------
	// Створення ADC unit + налаштування каналів + створення калібрування
	adc_oneshot_unit_init_cfg_t init_config = {
		.unit_id = ctx->unit_id,			// Використання ADC 1 ADC_UNIT_1
		.clk_src = ADC_RTC_CLK_SRC_DEFAULT, // стандартне джерело CLK
		.ulp_mode = ADC_ULP_MODE_DISABLE,	// вимкнення режиму ULP
	};

	esp_err_t err = adc_oneshot_new_unit(&init_config, &ctx->adc_handle); // Створення дескриптора ADC 1
	if (err != ESP_OK)
	{
		return err;
	}

	adc_oneshot_chan_cfg_t config = {
		.atten = ADC_ATTEN_DB_12,  // Діапазон до ~3.1В
		.bitwidth = ctx->bitwidth, // Зазвичай 12 біт ADC_BITWIDTH_DEFAULT
	};

	for (uint8_t i = 0; i < ctx->channel_count; i++)
	{
		// ESP_ERROR_CHECK(
		// 	adc_oneshot_config_channel(ctx->adc_handle, ctx->channels[i], &config));

		err = adc_oneshot_config_channel(ctx->adc_handle, ctx->channels[i], &config);
		if (err != ESP_OK)
		{
			adc_oneshot_del_unit(ctx->adc_handle);
			ctx->adc_handle = NULL;
			return err;
		}
	}

	//-------------ADC1 Calibration ---------------
	uint8_t created_cali_count = 0;
	for (uint8_t i = 0; i < ctx->channel_count; i++)
	{
		adc_cali_curve_fitting_config_t cali_config = {
			.unit_id = ctx->unit_id,
			.chan = ctx->channels[i],
			.atten = ADC_ATTEN_DB_12,
			.bitwidth = ctx->bitwidth,
		};

		err = adc_cali_create_scheme_curve_fitting(&cali_config, &ctx->cali_handles[i]);
		if (err != ESP_OK)
		{
			adc_release_del_cali(ctx);
			adc_oneshot_del_unit(ctx->adc_handle);
			ctx->adc_handle = NULL;
			return err;
		}
		created_cali_count++;
	}
	ctx->initialized = true;

	return ESP_OK;
}

esp_err_t adc_read_all_voltage_mv(const adc_context *ctx, int *raw, int *voltage_mv)
{
	if (ctx == NULL || voltage_mv == NULL || raw == NULL || !ctx->initialized || ctx->channels == NULL || ctx->cali_handles == NULL || ctx->channel_count == 0)
	{
		return ESP_ERR_INVALID_ARG;
	}

	for (uint8_t i = 0; i < ctx->channel_count; i++)
	{

		esp_err_t err = adc_oneshot_read(ctx->adc_handle, ctx->channels[i], &raw[i]);
		if (err != ESP_OK)
		{
			return err;
		}

		err = adc_cali_raw_to_voltage(ctx->cali_handles[i], raw[i], &voltage_mv[i]);
		if (err != ESP_OK)
		{
			return err;
		}
	}

	return ESP_OK;
}

//------------- Деініціалізації ADC ---------------
// Видалення калібрування + видалення ADC unit + очищення handle
esp_err_t adc_deinit(adc_context *ctx)
{
	// Перевірка вказівника на структуру контексту ADC
	if (ctx == NULL)
	{
		// Якщо вказівник NULL — помилка неправильний аргумент
		return ESP_ERR_INVALID_ARG;
	}

	// Звільнення ресурсів калібрування ADC для всіх каналів
	esp_err_t first_err = adc_release_del_cali(ctx);

	// Перевірка дескриптору (handle) ADC-блоку
	if (ctx->adc_handle == NULL)
	{
		// ADC вже був видалений
		ctx->initialized = false;
		return first_err;
	}

	// Видалення ADC unit
	esp_err_t err = adc_oneshot_del_unit(ctx->adc_handle);
	if (first_err == ESP_OK && err != ESP_OK)
	{
		first_err = err;
	}

	// Очищення дескриптора ADC
	ctx->adc_handle = NULL;
	// Зміна стану ADC:
	ctx->initialized = false;

	return first_err;
}
