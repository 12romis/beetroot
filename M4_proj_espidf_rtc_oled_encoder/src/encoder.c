#include <stdbool.h>
#include "esp_log.h"
#include "iot_knob.h"
#include "iot_button.h"
#include "board_config.h"
#include "app.h"
#include "encoder.h"

static const char *TAG = "ENCODER";
static knob_handle_t s_knob = NULL;
static button_handle_t s_btn = NULL;

// Колбек повороту енкодера вліво
static void encoder_left_cb(void *arg, void *usr_data)
{
	app_data_t *data = (app_data_t *)usr_data;
	data->encoder_data.position--;
}

// Колбек повороту енкодера вправо
static void encoder_right_cb(void *arg, void *usr_data)
{
	app_data_t *data = (app_data_t *)usr_data;
	data->encoder_data.position++;
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

	// ----- Обертання (knob) -----
	knob_config_t knob_cfg = {
		.default_direction = 0,
		.gpio_encoder_a = ENC_A,
		.gpio_encoder_b = ENC_B,
	};

	s_knob = iot_knob_create(&knob_cfg);
	if (s_knob == NULL)
	{
		ESP_LOGE(TAG, "encoder_dev_init: knob create failed");
		return;
	}

	iot_knob_register_cb(s_knob, KNOB_LEFT, encoder_left_cb, app_data);
	iot_knob_register_cb(s_knob, KNOB_RIGHT, encoder_right_cb, app_data);

	// ----- Кнопка енкодера -----
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
