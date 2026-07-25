#pragma once

// Алгоритм антидребезгу зсувного регістру (Shift-Register Debounce)
// Порт логіки з M4.1_esp_idf_uart/lib/debounce під STM32 HAL

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
	BUTTON_ERROR = -1,
	RELEASED, // кнопка відпущена
	PRESSED	  // кнопка натиснута
} ButtonState;

typedef enum
{
	NONE_EVENT,				// подія відсутня
	PRESSED_EVENT,			// момент натискання
	RELEASED_EVENT,			// момент відпускання
	CLICK_EVENT,			// коротке натискання
	LONG_PRESS_EVENT,		// довге утримання
	LONG_PRESS_REPEAT_EVENT // повтор довгого утримання
} ButtonEvent;

typedef struct
{
	// 1. Конфігураційні параметри
	GPIO_TypeDef *port;	 // Порт GPIO кнопки
	uint16_t pin;		 // Пін GPIO кнопки (GPIO_PIN_x)
	uint16_t longPressTime;	 // Час довгого натискання (мс)
	uint16_t repeatInterval; // Інтервал повторення події (мс)
	uint8_t activeLevel;	 // Натиснута - 0; натиснута - 1
	uint8_t shiftBits;		 // кількість вибірок: 4 або 8
	uint8_t shiftRegister;	 // Історія останніх опитувань GPIO

	// 2. Внутрішній стан
	uint32_t pressStartTime; // Час початку натискання (мс, HAL_GetTick)
	uint32_t lastRepeatTime; // Час останнього повтору
	ButtonState pinState;	 // Поточний стан кнопки
	ButtonEvent event;		 // Подія кнопки
	bool longPressActive;	 // Флаг довгого натискання
	bool initialized;		 // Ініціалізація
} deb;

// Debounce API
void deb_init(deb *btn);
HAL_StatusTypeDef deb_btns_update(deb *btns, uint8_t btns_count);
ButtonEvent deb_get_btn_event(deb *btn);
ButtonState deb_get_btn_status(const deb *btn);
