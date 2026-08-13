#include <stdbool.h>
#include "esp_log.h"
#include "encoder.h" // esp-idf-lib/encoder — обертання (HW timer-based декодер квадратури)
#include "iot_button.h" // espressif/button — клік/подвійний клік кнопки енкодера (без змін)
#include "board_config.h"
#include "app.h"
#include "rotary_encoder.h"

static const char *TAG = "ENCODER";
static rotary_encoder_handle_t s_rotary = NULL;
static button_handle_t s_btn = NULL;

// Колбек подій esp-idf-lib/encoder — нас цікавить лише RE_ET_CHANGED (обертання);
// кнопку на pin_btn не задаємо, її й надалі обробляє окремо espressif/button нижче.
static void rotary_encoder_cb(const rotary_encoder_event_t *event, void *ctx)
{
	if (event->type != RE_ET_CHANGED)
	{
		return;
	}

	app_data_t *data = (app_data_t *)ctx;
	data->encoder_data.position += event->diff;
}

// Колбек натискання кнопки енкодера
static void encoder_btn_cb(void *arg, void *usr_data)
{
	app_data_t *data = (app_data_t *)usr_data;
	data->encoder_data.pressed = true;
}

// Колбек подвійного кліку кнопки енкодера — аварійний вихід у головне меню
static void encoder_btn_double_cb(void *arg, void *usr_data)
{
	app_data_t *data = (app_data_t *)usr_data;
	data->screen_mode = MAIN_SCREEN;
	data->selected_field = 0;
	data->field_editing = false;
}

// Ініціалізація енкодера
void encoder_dev_init(void)
{
	app_data_t *app_data = get_app_data();

	// ----- Обертання (esp-idf-lib/encoder) -----
	rotary_encoder_config_t enc_cfg = ROTARY_ENCODER_DEFAULT_CONFIG();
	enc_cfg.pin_a = ENC_A;
	enc_cfg.pin_b = ENC_B;
	enc_cfg.pin_btn = GPIO_NUM_NC; // кнопка лишається на espressif/button, не тут
	enc_cfg.callback = rotary_encoder_cb;
	enc_cfg.callback_ctx = app_data;

	esp_err_t err = rotary_encoder_create(&enc_cfg, &s_rotary);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "encoder_dev_init: rotary_encoder_create failed: %s", esp_err_to_name(err));
	}

	// ----- Кнопка енкодера (без змін) -----
	button_config_t btn_cfg = {
		.type = BUTTON_TYPE_GPIO,
		.long_press_time = 1000,
		.short_press_time = 50,
		.gpio_button_config = {
			.gpio_num = ENC_BTN,
			.active_level = 0, // енкодер замикає кнопку на GND
		},
	};

	s_btn = iot_button_create(&btn_cfg);
	if (s_btn == NULL)
	{
		ESP_LOGE(TAG, "encoder_dev_init: button create failed");
		return;
	}

	iot_button_register_cb(s_btn, BUTTON_SINGLE_CLICK, encoder_btn_cb, app_data);
	iot_button_register_cb(s_btn, BUTTON_DOUBLE_CLICK, encoder_btn_double_cb, app_data);
}
