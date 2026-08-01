#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

enum ButtonState
{
	BUTTON_ERROR = -1,
	RELEASED, // кнопка відпущена
	PRESSED	  // кнопка натиснута
};

enum ButtonEvent
{
	NONE_EVENT,				// подія відсутня
	PRESSED_EVENT,			// момент натискання
	RELEASED_EVENT,			// момент відпускання
	CLICK_EVENT,			// коротке натискання
	LONG_PRESS_EVENT,		// довге утримання
	LONG_PRESS_REPEAT_EVENT // повтор довгого утримання
};

typedef struct
{
	// 1. Конфігураційні параметри
	gpio_num_t pin;			 // Номер GPIO піна кнопки
	uint16_t longPressTime;	 // Час довгого натискання (мс)
	uint16_t repeatInterval; // Інтервал повторення події (мс)
	uint8_t activeLevel;	 // Натиснута - 0; натиснута - 1
	uint8_t shiftBits;		 // кількість вибірок: 4 або 8
	uint8_t shiftRegister;	 // Історія останніх 8 опитувань GPIO

	// 2. Внутрішній стан
	uint64_t pressStartTime; // Час початку натискання
	uint64_t lastRepeatTime; // Час останнього повтору
	ButtonState pinState;	 // Поточний стан кнопки
	ButtonEvent event;		 // Подія кнопки
	bool longPressActive;	 // Флаг довгого натискання
	bool initialized;		 // Ініціалізація
} deb;

// Debounce API
void deb_init(deb *btn);
esp_err_t deb_btns_update(deb *btns, uint8_t btns_count);
ButtonEvent deb_get_btn_event(deb *btn);
ButtonState deb_get_btn_status(const deb *btn);
