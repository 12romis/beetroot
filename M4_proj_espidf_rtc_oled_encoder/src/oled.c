#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2cdev.h"
#include "u8g2.h"
#include "board_config.h"
#include "oled.h"

static const char *TAG = "OLED";

static u8g2_t u8g2;
static i2c_dev_t oled_dev;

// Буфер одного I2C-транзиту для OLED: 1 байт контролю + повний кадр 128x64/8.
#define OLED_I2C_TX_BUF_SIZE (1 + 128 * 64 / 8)
static uint8_t oled_tx_buf[OLED_I2C_TX_BUF_SIZE];
static size_t oled_tx_len;

// u8x8 HAL: транспортний рівень для u8g2, побудований на вже наявному i2cdev
static uint8_t u8x8_byte_i2cdev_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
	switch (msg)
	{
	case U8X8_MSG_BYTE_SET_DC:
		return 1;

	case U8X8_MSG_BYTE_INIT:
		return 1;

	case U8X8_MSG_BYTE_START_TRANSFER:
		oled_tx_len = 0;
		return 1;

	case U8X8_MSG_BYTE_SEND:
	{
		size_t n = (size_t)arg_int;
		if (oled_tx_len + n > sizeof(oled_tx_buf))
		{
			ESP_LOGE(TAG, "oled_dev: i2c tx buffer overflow (%u + %u > %u)",
					 (unsigned)oled_tx_len, (unsigned)n, (unsigned)sizeof(oled_tx_buf));
			return 0;
		}
		memcpy(&oled_tx_buf[oled_tx_len], arg_ptr, n);
		oled_tx_len += n;
		return 1;
	}

	case U8X8_MSG_BYTE_END_TRANSFER:
	{
		esp_err_t err = i2c_dev_write(&oled_dev, NULL, 0, oled_tx_buf, oled_tx_len);
		if (err != ESP_OK)
		{
			ESP_LOGE(TAG, "oled_dev: i2c write failed: %s", esp_err_to_name(err));
			return 0;
		}
		return 1;
	}

	default:
		return 1;
	}
}

// u8x8 HAL: затримки (OLED-модуль без апаратного reset/CS — GPIO-повідомлення ігноруємо)
static uint8_t u8x8_gpio_and_delay_i2cdev_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
	switch (msg)
	{
	case U8X8_MSG_DELAY_MILLI:
		vTaskDelay(pdMS_TO_TICKS(arg_int));
		return 1;

	case U8X8_MSG_DELAY_10MICRO:
		esp_rom_delay_us(10 * arg_int);
		return 1;

	default:
		return 1;
	}
}

// Ініціалізація OLED-дисплея
void oled_dev_init(void)
{
	memset(&oled_dev, 0, sizeof(i2c_dev_t));
	oled_dev.port = I2C_PORT;
	oled_dev.addr = 0x3C;
	oled_dev.cfg.sda_io_num = I2C_SDA_GPIO;
	oled_dev.cfg.scl_io_num = I2C_SCL_GPIO;
	oled_dev.cfg.master.clk_speed = 400000;

	esp_err_t err = i2c_dev_create_mutex(&oled_dev);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "oled_dev_init: %s", esp_err_to_name(err));
		return;
	}

	u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0,
											u8x8_byte_i2cdev_cb,
											u8x8_gpio_and_delay_i2cdev_cb);
	u8x8_SetI2CAddress(&u8g2.u8x8, 0x3C << 1);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);
	u8g2_ClearBuffer(&u8g2);
}

// малює одне поле (наприклад "14") на позиції x,y і повертає x для наступного поля
static u8g2_uint_t draw_field(u8g2_uint_t x, u8g2_uint_t y, const char *text, field_state_t state)
{
	u8g2_uint_t w = u8g2_GetStrWidth(&u8g2, text);
	u8g2_uint_t asc = u8g2_GetAscent(&u8g2);
	u8g2_uint_t desc = -u8g2_GetDescent(&u8g2); // у u8g2 descent зазвичай від'ємний

	if (state == FIELD_SELECTED)
	{
		u8g2_DrawFrame(&u8g2, x - 1, y - asc - 1, w + 2, asc + desc + 2);
	}
	else if (state == FIELD_EDITING)
	{
		u8g2_SetDrawColor(&u8g2, 1);
		u8g2_DrawBox(&u8g2, x - 1, y - asc - 1, w + 2, asc + desc + 2);
		u8g2_SetDrawColor(&u8g2, 0); // текст поверх залитого боксу = "інверсія"
	}

	u8g2_DrawStr(&u8g2, x, y, text);
	u8g2_SetDrawColor(&u8g2, 1); // обов'язково повернути назад для наступних елементів

	return x + w;
}

// Стан підсвітки поля field_index для малювання (рамка/інверсія/нічого)
static field_state_t field_state_for(app_data_t *app_data, int field_index)
{
	if (app_data->selected_field != field_index)
	{
		return FIELD_NORMAL;
	}
	return app_data->field_editing ? FIELD_EDITING : FIELD_SELECTED;
}

static void update_main_screen(app_data_t *app_data)
{
	char text_buf[64];

	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_luRS08_te);

	// Час і Дата — верхній рядок
	snprintf(text_buf, sizeof(text_buf),
			 "%02d:%02d:%02d %02d.%02d.%04d",
			 app_data->time.tm_hour,
			 app_data->time.tm_min,
			 app_data->time.tm_sec,
			 app_data->time.tm_mday,
			 app_data->time.tm_mon + 1,
			 app_data->time.tm_year + 1900);
	u8g2_uint_t w = u8g2_GetStrWidth(&u8g2, text_buf);
	u8g2_uint_t x = (u8g2_GetDisplayWidth(&u8g2) - w) / 2;
	u8g2_DrawStr(&u8g2, x, 10, text_buf);

	// Температура і тиск
	snprintf(text_buf, sizeof(text_buf),
			 "%.2f C | %.0f hPa",
			 app_data->bme280_data.temperature,
			 app_data->bme280_data.pressure);
	w = u8g2_GetStrWidth(&u8g2, text_buf);
	x = (u8g2_GetDisplayWidth(&u8g2) - w) / 2;
	u8g2_DrawStr(&u8g2, x, 25, text_buf);

	u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
	// Меню
	snprintf(text_buf, sizeof(text_buf), "%schange time", app_data->selected_field == 0 ? "> " : "  ");
	u8g2_DrawStr(&u8g2, 2, 40, text_buf);
	snprintf(text_buf, sizeof(text_buf), "%schange date", app_data->selected_field == 1 ? "> " : "  ");
	u8g2_DrawStr(&u8g2, 2, 50, text_buf);
	snprintf(text_buf, sizeof(text_buf), "%senv history", app_data->selected_field == 2 ? "> " : "  ");
	u8g2_DrawStr(&u8g2, 2, 60, text_buf);

	u8g2_SendBuffer(&u8g2);
}

static void update_change_time_screen(app_data_t *app_data)
{
	char full[24];
	char buf[16];

	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_logisoso16_tf);

	// Ширину рахуємо по повному "HH:MM", щоб рядок лишався по центру,
	// а самі частини малюємо окремо — так кожну можна підсвітити окремо
	snprintf(full, sizeof(full), "%02d:%02d", app_data->time.tm_hour, app_data->time.tm_min);
	u8g2_uint_t w = u8g2_GetStrWidth(&u8g2, full);
	u8g2_uint_t x = (u8g2_GetDisplayWidth(&u8g2) - w) / 2;
	u8g2_uint_t y = 30;

	snprintf(buf, sizeof(buf), "%02d", app_data->time.tm_hour);
	x = draw_field(x, y, buf, field_state_for(app_data, 0));

	x = draw_field(x, y, ":", FIELD_NORMAL);

	snprintf(buf, sizeof(buf), "%02d", app_data->time.tm_min);
	draw_field(x, y, buf, field_state_for(app_data, 1));

	// Back
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
	draw_field(2, 60, "Back", field_state_for(app_data, 2));

	u8g2_SendBuffer(&u8g2);
}

static void update_change_date_screen(app_data_t *app_data)
{
	char full[40];
	char buf[16];

	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_logisoso16_tf);

	snprintf(full, sizeof(full), "%02d.%02d.%04d",
			 app_data->time.tm_mday, app_data->time.tm_mon + 1, app_data->time.tm_year + 1900);
	u8g2_uint_t w = u8g2_GetStrWidth(&u8g2, full);
	u8g2_uint_t x = (u8g2_GetDisplayWidth(&u8g2) - w) / 2;
	u8g2_uint_t y = 30;

	snprintf(buf, sizeof(buf), "%02d", app_data->time.tm_mday);
	x = draw_field(x, y, buf, field_state_for(app_data, 0));
	x = draw_field(x, y, ".", FIELD_NORMAL);

	snprintf(buf, sizeof(buf), "%02d", app_data->time.tm_mon + 1);
	x = draw_field(x, y, buf, field_state_for(app_data, 1));
	x = draw_field(x, y, ".", FIELD_NORMAL);

	snprintf(buf, sizeof(buf), "%04d", app_data->time.tm_year + 1900);
	draw_field(x, y, buf, field_state_for(app_data, 2));

	// Back
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
	draw_field(2, 60, "Back", field_state_for(app_data, 3));

	u8g2_SendBuffer(&u8g2);
}

// Оновлення OLED-дисплея
void oled_update(app_data_t *app_data)
{
	switch (app_data->screen_mode)
	{
	case MAIN_SCREEN:
		update_main_screen(app_data);
		break;
	case CHANGE_TIME_SCREEN:
		update_change_time_screen(app_data);
		break;
	case CHANGE_DATE_SCREEN:
		update_change_date_screen(app_data);
		break;
	case ENV_HISTORY_SCREEN:
		printf("ENV_HISTORY_SCREEN\n");
		break;
	}
}
