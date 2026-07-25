#include "debounce.h"

static int deb_read_pin(const deb *btn)
{
	return (HAL_GPIO_ReadPin(btn->port, btn->pin) == GPIO_PIN_SET) ? 1 : 0;
}

void deb_init(deb *btn)
{
	if (btn == NULL)
	{
		return;
	}

	// 2. Внутрішній стан
	btn->pressStartTime = 0;
	btn->lastRepeatTime = 0;
	btn->pinState =
		(deb_read_pin(btn) == btn->activeLevel) ? PRESSED : RELEASED;
	btn->event = NONE_EVENT;
	btn->longPressActive = true;
	btn->initialized = true;
}

HAL_StatusTypeDef deb_btns_update(deb *btns, uint8_t btns_count)
{
	if (btns == NULL || btns_count == 0)
	{
		return HAL_ERROR;
	}

	// Отримуємо мілісекунди один раз для всього масиву кнопок
	uint32_t now = HAL_GetTick();

	HAL_StatusTypeDef err = HAL_OK;

	for (uint8_t i = 0; i < btns_count; i++)
	{
		if (!btns[i].initialized)
		{
			err = HAL_ERROR;
			continue;
		}

		// За замовчуванням кожну ітерацію подія відсутня
		btns[i].event = NONE_EVENT;

		// 1. Читання GPIO
		int currentPinState = deb_read_pin(&btns[i]);

		// 2. Shift Register Debounce
		uint8_t mask = (1 << btns[i].shiftBits) - 1;

		btns[i].shiftRegister <<= 1;
		btns[i].shiftRegister |= (currentPinState & 0x01);
		btns[i].shiftRegister &= mask;

		// 3. Визначення стабільного стану
		ButtonState newPinState = btns[i].pinState;

		if (btns[i].shiftRegister == 0)
		{
			newPinState = (btns[i].activeLevel == 0) ? PRESSED : RELEASED;
		}
		else if (btns[i].shiftRegister == mask)
		{
			newPinState = (btns[i].activeLevel == 1) ? PRESSED : RELEASED;
		}

		// 4. Зміна стану кнопки
		if (newPinState != btns[i].pinState)
		{
			btns[i].pinState = newPinState;

			if (newPinState == PRESSED)
			{
				btns[i].pressStartTime = now;
				btns[i].longPressActive = false;
				btns[i].lastRepeatTime = 0;
				btns[i].event = PRESSED_EVENT;
			}
			else
			{
				btns[i].event = (!btns[i].longPressActive) ? CLICK_EVENT : RELEASED_EVENT;
			}
			continue;
		}

		// 5. LONG PRESS
		if (btns[i].pinState == PRESSED)
		{
			// Перевірка 1: Перше тривале затискання кнопки
			if (!btns[i].longPressActive)
			{
				uint32_t heldTime = now - btns[i].pressStartTime;

				if (heldTime >= btns[i].longPressTime)
				{
					btns[i].longPressActive = true;
					btns[i].lastRepeatTime = now;
					btns[i].event = LONG_PRESS_EVENT; // Генеруємо подію утримання
				}
			}
			// Перевірка 2: Повторення події (Auto-repeat)
			else
			{
				if (now - btns[i].lastRepeatTime >= btns[i].repeatInterval)
				{
					btns[i].lastRepeatTime = now;
					btns[i].event = LONG_PRESS_REPEAT_EVENT;
				}
			}
		}
	}
	return err;
}

ButtonEvent deb_get_btn_event(deb *btn)
{
	if (btn == NULL || !btn->initialized)
	{
		return NONE_EVENT;
	}

	ButtonEvent current_event = btn->event;
	btn->event = NONE_EVENT;

	return current_event;
}

ButtonState deb_get_btn_status(const deb *btn)
{
	if (btn == NULL || !btn->initialized)
	{
		return BUTTON_ERROR;
	}

	return btn->pinState;
}
