#include <stdio.h>
#include <time.h>
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "encoder.h"
#include "debounce.h"
#include "adc.h"
#include "timer.h"

#include "mqtt.h"
#include "wifi_setup.h"

static const char TAG[] = "main";

// button
constexpr gpio_num_t ENC_BUTTON_GPIO = GPIO_NUM_15;
constexpr uint8_t BUTTONS_COUNT = 1;
constexpr uint16_t LONG_PRESS_TIME = 1000; // час довгого натискання (мс)
constexpr uint16_t REPEAT_INTERVAL = 500;  // інтервал повторення події (мс)

// led management variables
static bool led_state = false;
constexpr gpio_num_t LED_GPIO = GPIO_NUM_4;

// uart management variables
constexpr gpio_num_t UART1_TX_GPIO = GPIO_NUM_17;
constexpr gpio_num_t UART1_RX_GPIO = GPIO_NUM_18;



bool getled_state()
{
	return led_state;
}

static void buttons_process(deb *btns, uint8_t btns_count)
{
	for (uint8_t i = 0; i < btns_count; i++)
	{
		ButtonEvent event = deb_get_btn_event(&btns[i]);
		// ButtonState status = deb_get_btn_status(&btns[i]);

		if (event == LONG_PRESS_EVENT || event == CLICK_EVENT)
		{
			led_state = !led_state;
		}
	}
}


extern "C" void app_main(void)
{

	// Сконфігурувати LED GPIO
	gpio_config_t io_conf = {};
	io_conf.pin_bit_mask = (1ULL << LED_GPIO);
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK(gpio_config(&io_conf));

	// GPIO BUTTON initialization
	gpio_config_t btn_conf = {};
	btn_conf.pin_bit_mask = (1ULL << ENC_BUTTON_GPIO);
	btn_conf.mode = GPIO_MODE_INPUT;
	btn_conf.pull_up_en = GPIO_PULLUP_ENABLE;
	btn_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	btn_conf.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK(gpio_config(&btn_conf));

	// DEBOUNCE initialization
	deb btns[BUTTONS_COUNT] = {};
	btns[0].pin = ENC_BUTTON_GPIO;
	btns[0].longPressTime = LONG_PRESS_TIME;
	btns[0].repeatInterval = REPEAT_INTERVAL;
	btns[0].activeLevel = 0;	  // Натиснута - 0; натиснута - 1
	btns[0].shiftBits = 4;		  // 4 - 0x0F ; 8 - 0xFF
	btns[0].shiftRegister = 0x0F; // 4 - 0x0F ; 8 - 0xFF
	deb_init(&btns[0]);

	while (1)
	{
		// Зчитуємо стан кнопки та передаємо по UART
		ESP_ERROR_CHECK(deb_btns_update(btns, BUTTONS_COUNT));
		buttons_process(btns, BUTTONS_COUNT);

		gpio_set_level(LED_GPIO, led_state);

		

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
