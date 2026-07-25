#include <stdio.h>
#include <time.h>
#include "driver/gpio.h"
#include "driver/uart.h"
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
static bool stm32_led_state = false;
constexpr gpio_num_t LED_GPIO = GPIO_NUM_4;

// uart management variables
constexpr gpio_num_t UART1_TX_GPIO = GPIO_NUM_17;
constexpr gpio_num_t UART1_RX_GPIO = GPIO_NUM_18;
constexpr uart_port_t UART1_PORT = UART_NUM_1;
constexpr int UART1_BAUD_RATE = 115200;
constexpr int UART1_RX_BUF_SIZE = 256;
constexpr int UART1_TX_BUF_SIZE = 256;

// uart command codes (protocol shared with STM32)
constexpr uint8_t CMD_LED_OFF = 0xA0;
constexpr uint8_t CMD_LED_ON = 0xA1;

bool getled_state()
{
	return stm32_led_state;
}

static void uart1_init(void)
{
	uart_config_t uart_config = {};
	uart_config.baud_rate = UART1_BAUD_RATE;
	uart_config.data_bits = UART_DATA_8_BITS;
	uart_config.parity = UART_PARITY_DISABLE;
	uart_config.stop_bits = UART_STOP_BITS_1;
	uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	uart_config.source_clk = UART_SCLK_DEFAULT;

	ESP_ERROR_CHECK(uart_driver_install(UART1_PORT, UART1_RX_BUF_SIZE, UART1_TX_BUF_SIZE, 0, NULL, 0));
	ESP_ERROR_CHECK(uart_param_config(UART1_PORT, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(UART1_PORT, UART1_TX_GPIO, UART1_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

static void buttons_process(deb *btns, uint8_t btns_count)
{
	for (uint8_t i = 0; i < btns_count; i++)
	{
		ButtonEvent event = deb_get_btn_event(&btns[i]);
		// ButtonState status = deb_get_btn_status(&btns[i]);

		if (event == LONG_PRESS_EVENT || event == CLICK_EVENT)
		{
			stm32_led_state = !stm32_led_state;
			uint8_t cmd = stm32_led_state ? CMD_LED_ON : CMD_LED_OFF;
			uart_write_bytes(UART1_PORT, (const char *)&cmd, sizeof(cmd));
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

	// UART1 initialization
	uart1_init();

	while (1)
	{
		// Зчитуємо стан кнопки та передаємо по UART
		ESP_ERROR_CHECK(deb_btns_update(btns, BUTTONS_COUNT));
		buttons_process(btns, BUTTONS_COUNT);

		// Зчитуємо дані з UART1 та керуємо LED
		uint8_t cmd;
		int len = uart_read_bytes(UART1_PORT, &cmd, sizeof(cmd), 0);

		if (len > 0)
		{
			if (cmd == CMD_LED_ON)
			{
				gpio_set_level(LED_GPIO, true);
			}
			else if (cmd == CMD_LED_OFF)
			{
				gpio_set_level(LED_GPIO, false);
			}
		}
		

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
