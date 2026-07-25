#include "stm32f4xx_hal.h"
#include "debounce.h"
#include <stdbool.h>

// button
#define BUTTON_PORT GPIOA
#define BUTTON_PIN GPIO_PIN_0
#define BUTTONS_COUNT 1
#define LONG_PRESS_TIME 1000U // час довгого натискання (мс)
#define REPEAT_INTERVAL 500U  // інтервал повторення події (мс)

// led management (вбудований LED BlackPill, активний рівень LOW)
#define LED_PORT GPIOC
#define LED_PIN GPIO_PIN_13
static bool esp32_led_state = false;

// uart management (USART2: PA2 = TX, PA3 = RX)
#define UART_BAUD_RATE 115200
static UART_HandleTypeDef huart2;

// uart command codes (protocol shared with ESP32)
#define CMD_LED_OFF 0xA0
#define CMD_LED_ON 0xA1

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void USART2_Init(void);
static void Error_Handler(void);

static void led_set(bool on)
{
	// LED активний по низькому рівню
	HAL_GPIO_WritePin(LED_PORT, LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void buttons_process(deb *btns, uint8_t btns_count)
{
	for (uint8_t i = 0; i < btns_count; i++)
	{
		ButtonEvent event = deb_get_btn_event(&btns[i]);

		if (event == LONG_PRESS_EVENT || event == CLICK_EVENT)
		{
			esp32_led_state = !esp32_led_state;
			uint8_t cmd = esp32_led_state ? CMD_LED_ON : CMD_LED_OFF;
			HAL_UART_Transmit(&huart2, &cmd, sizeof(cmd), 100);
		}
	}
}

int main(void)
{
	HAL_Init();
	SystemClock_Config();
	GPIO_Init();
	USART2_Init();

	// DEBOUNCE initialization
	deb btns[BUTTONS_COUNT] = {0};
	btns[0].port = BUTTON_PORT;
	btns[0].pin = BUTTON_PIN;
	btns[0].longPressTime = LONG_PRESS_TIME;
	btns[0].repeatInterval = REPEAT_INTERVAL;
	btns[0].activeLevel = 0;	  // Натиснута - 0; натиснута - 1
	btns[0].shiftBits = 4;		  // 4 - 0x0F ; 8 - 0xFF
	btns[0].shiftRegister = 0x0F; // 4 - 0x0F ; 8 - 0xFF
	deb_init(&btns[0]);

	while (1)
	{
		// Зчитуємо стан кнопки та передаємо по UART
		deb_btns_update(btns, BUTTONS_COUNT);
		buttons_process(btns, BUTTONS_COUNT);

		// Зчитуємо дані з UART та керуємо LED
		uint8_t cmd;
		if (HAL_UART_Receive(&huart2, &cmd, sizeof(cmd), 0) == HAL_OK)
		{
			if (cmd == CMD_LED_ON)
			{
				led_set(true);
			}
			else if (cmd == CMD_LED_OFF)
			{
				led_set(false);
			}
		}

		HAL_Delay(10);
	}
}

// HSE 25MHz -> SYSCLK 84MHz (PLLM=25, PLLN=336, PLLP=4, PLLQ=7)
static void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 25;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
								   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
		Error_Handler();
	}
}

static void GPIO_Init(void)
{
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();

	GPIO_InitTypeDef gpio = {0};

	// LED PC13, активний по низькому рівню -> за замовчуванням вимкнений (HIGH)
	HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
	gpio.Pin = LED_PIN;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LED_PORT, &gpio);

	// Button PA0, зовнішня кнопка на GND, підтяжка вгору, натиснута - 0
	gpio.Pin = BUTTON_PIN;
	gpio.Mode = GPIO_MODE_INPUT;
	gpio.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(BUTTON_PORT, &gpio);
}

static void USART2_Init(void)
{
	__HAL_RCC_USART2_CLK_ENABLE();

	// PA2 = TX, PA3 = RX (AF7)
	GPIO_InitTypeDef gpio = {0};
	gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
	gpio.Mode = GPIO_MODE_AF_PP;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	gpio.Alternate = GPIO_AF7_USART2;
	HAL_GPIO_Init(GPIOA, &gpio);

	huart2.Instance = USART2;
	huart2.Init.BaudRate = UART_BAUD_RATE;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart2) != HAL_OK)
	{
		Error_Handler();
	}
}

static void Error_Handler(void)
{
	__disable_irq();
	while (1)
	{
	}
}
