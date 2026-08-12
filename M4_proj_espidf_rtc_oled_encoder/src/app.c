#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2cdev.h"
#include "ds1307.h"
#include "bmp280.h"
#include "iot_knob.h"
#include "iot_button.h"
#include "u8g2.h"
#include "app.h"

static u8g2_t u8g2;
static i2c_dev_t oled_dev;
static i2c_dev_t rtc_dev;
static bmp280_t bmp_dev;
static knob_handle_t s_knob = NULL;
static button_handle_t s_btn = NULL;

// Буфер одного I2C-транзиту для OLED: 1 байт контролю + повний кадр 128x64/8.
// Весь кадр збирається тут між START_TRANSFER і END_TRANSFER, а тоді йде
// ОДНИМ i2c_dev_write() — щоб не розбивати SSD1306-протокол на частини.
#define OLED_I2C_TX_BUF_SIZE (1 + 128 * 64 / 8)
static uint8_t oled_tx_buf[OLED_I2C_TX_BUF_SIZE];
static size_t oled_tx_len;

// Конфігурація I2C ESP32-S3
static const gpio_num_t I2C_PORT = I2C_NUM_0;
static const gpio_num_t I2C_SDA_GPIO = GPIO_NUM_8;
static const gpio_num_t I2C_SCL_GPIO = GPIO_NUM_9;

//------- Енкодер ------------
// Фактична розпіновка: CLK->GPIO12, DT->GPIO11, SW->GPIO10
#define ENC_A GPIO_NUM_12
#define ENC_B GPIO_NUM_11
#define ENC_BTN GPIO_NUM_10

static const char *TAG = "APP";

static app_data_t app_data;

// функція для отримання вказівника на структуру app_data_t
app_data_t *get_app_data(void)
{
	return &app_data;
}

// Ініціалізація поточного часу з компіляції програми
static void rtc_set_time(struct tm *time)
{
	static const char *months[] =
		{
			"Jan", "Feb", "Mar", "Apr",
			"May", "Jun", "Jul", "Aug",
			"Sep", "Oct", "Nov", "Dec"};

	char month[4];
	int year;
	memset(time, 0, sizeof(struct tm));

	sscanf(__DATE__, "%3s %d %d",
		   month,
		   &time->tm_mday,
		   &year);

	time->tm_year = year - 1900;

	for (int i = 0; i < 12; i++)
	{
		if (strcmp(month, months[i]) == 0)
		{
			time->tm_mon = i;
			break;
		}
	}

	sscanf(__TIME__, "%d:%d:%d",
		   &time->tm_hour,
		   &time->tm_min,
		   &time->tm_sec);
}

// Iніціалізацію бібліотеки i2cdev
void i2c_init(void)
{
	// ----- Init i2cdev -----
	esp_err_t err = i2cdev_init();
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "i2c_init: %s", esp_err_to_name(err));
	}
}

// u8x8 HAL: транспортний рівень для u8g2, побудований на вже наявному i2cdev
// (той самий i2c_dev_t/i2c_dev_write, що й RTC/BMP280) — OLED ділить ОДНУ
// I2C-шину з рештою пристроїв замість створення власної окремої шини.
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
void oled_dev_init()
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

// Ініціалізація RTC
void rtc_dev_init(app_data_t *app_data)
{
	memset(&rtc_dev, 0, sizeof(i2c_dev_t));

	esp_err_t err = ds1307_init_desc(&rtc_dev,
									 I2C_PORT,
									 I2C_SDA_GPIO,
									 I2C_SCL_GPIO);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "rtc_dev_init: %s", esp_err_to_name(err));
	}

	rtc_set_time(&app_data->time);

	err = ds1307_set_time(&rtc_dev, &app_data->time);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "rtc_dev_init: %s", esp_err_to_name(err));
	}
}

// Ініціалізація BMP280
void bmp280_dev_init()
{
	memset(&bmp_dev, 0, sizeof(bmp280_t));

	esp_err_t err = bmp280_init_desc(&bmp_dev,
									 BMP280_I2C_ADDRESS_0,
									 I2C_PORT,
									 I2C_SDA_GPIO,
									 I2C_SCL_GPIO);

	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "bmp280_dev_init: %s", esp_err_to_name(err));
	}

	bmp280_params_t bmp_params;
	bmp280_init_default_params(&bmp_params);

	err = bmp280_init(&bmp_dev, &bmp_params);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "bmp280_dev_init: %s", esp_err_to_name(err));
	}
}

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

// Колбек подвійного кліку кнопки енкодера — навігація "назад"
static void encoder_btn_double_cb(void *arg, void *usr_data)
{
	app_data_t *data = (app_data_t *)usr_data;
	data->screen_mode = MAIN_SCREEN;
}


// Ініціалізація енкодера
void encoder_dev_init()
{
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

	iot_knob_register_cb(s_knob, KNOB_LEFT, encoder_left_cb, &app_data);
	iot_knob_register_cb(s_knob, KNOB_RIGHT, encoder_right_cb, &app_data);

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

	iot_button_register_cb(s_btn, BUTTON_SINGLE_CLICK, encoder_btn_cb, &app_data);
	iot_button_register_cb(s_btn, BUTTON_DOUBLE_CLICK, encoder_btn_double_cb, &app_data);
}

// Читання часу з RTC
void rtc_read(app_data_t *app_data)
{
	esp_err_t err = ds1307_get_time(&rtc_dev, &app_data->time);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "rtc_read_error: %s", esp_err_to_name(err));
	}
}

// Читання даних з BME280
void bme280_read(app_data_t *app_data)
{
	esp_err_t err = bmp280_read_float(&bmp_dev, &app_data->bme280_data.temperature, &app_data->bme280_data.pressure, &app_data->bme280_data.humidity);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "bme280_read_error: %s", esp_err_to_name(err));
	}

	// bmp280_read_float повертає тиск у Паскалях — переводимо в гПа (те, що
	// далі й підписано як "hPa" в логах/на екрані)
	app_data->bme280_data.pressure /= 100.0f;
}



// малює одне поле (наприклад "14") на позиції x,y і повертає x для наступного поля
static u8g2_uint_t draw_time_field(u8g2_uint_t x, u8g2_uint_t y, const char *text, field_state_t state)
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


static void update_main_screen(app_data_t *app_data)
{
	int selected_menu_item = ((app_data->encoder_data.position % 3) + 3) % 3;
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
	snprintf(text_buf, sizeof(text_buf), "%schange time", selected_menu_item == 0 ? ">" : " ");
	u8g2_DrawStr(&u8g2, 2, 40, text_buf);
	snprintf(text_buf, sizeof(text_buf), "%schange date", selected_menu_item == 1 ? ">" : " ");
	u8g2_DrawStr(&u8g2, 2, 50, text_buf);
	snprintf(text_buf, sizeof(text_buf), "%senv history", selected_menu_item == 2 ? ">" : " ");
	u8g2_DrawStr(&u8g2, 2, 60, text_buf);

	u8g2_SendBuffer(&u8g2);
}

static void update_change_time_screen(app_data_t *app_data)
{
	// u8g2_uint_t x = 10, y = 30;
	// char buf[4];

	// snprintf(buf, sizeof(buf), "%02d", app_data->time.tm_hour);
	// x = draw_time_field(x, y, buf, hour_state);   // hour_state: NORMAL/SELECTED/EDITING

	// x = draw_time_field(x, y, ":", FIELD_NORMAL); // роздільник — завжди звичайний

	// snprintf(buf, sizeof(buf), "%02d", app_data->time.tm_min);
	// x = draw_time_field(x, y, buf, min_state);



	int selected_item = ((app_data->encoder_data.position % 2) + 2) % 2;
	char text_buf[32];

	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_logisoso16_tf);

	// Час
	snprintf(text_buf, sizeof(text_buf),
			 "%02d:%02d",
			 app_data->time.tm_hour,
			 app_data->time.tm_min);
	u8g2_uint_t w = u8g2_GetStrWidth(&u8g2, text_buf);
	u8g2_uint_t x = (u8g2_GetDisplayWidth(&u8g2) - w) / 2;
	u8g2_DrawStr(&u8g2, x, 30, text_buf);

	// Back to menu
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
	snprintf(text_buf, sizeof(text_buf), "Back");
	u8g2_DrawStr(&u8g2, 2, 60, text_buf);

	u8g2_SendBuffer(&u8g2);
}

static void update_change_date_screen(app_data_t *app_data)
{
	int selected_item = ((app_data->encoder_data.position % 3) + 3) % 3;
	char text_buf[32];

	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_logisoso16_tf);

	// Дата
	snprintf(text_buf, sizeof(text_buf),
			 "%02d.%02d.%04d",
			 app_data->time.tm_mday,
			 app_data->time.tm_mon + 1,
			 app_data->time.tm_year + 1900);
	u8g2_uint_t w = u8g2_GetStrWidth(&u8g2, text_buf);
	u8g2_uint_t x = (u8g2_GetDisplayWidth(&u8g2) - w) / 2;
	u8g2_DrawStr(&u8g2, x, 30, text_buf);

	// Back to menu
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
	snprintf(text_buf, sizeof(text_buf), "Back");
	u8g2_DrawStr(&u8g2, 2, 60, text_buf);

	u8g2_SendBuffer(&u8g2);
}

// Оновлення OLED-дисплея
void oled_update(app_data_t *app_data)
{
	switch (app_data->screen_mode)
	{
	case MAIN_SCREEN:
	{
		update_main_screen(app_data);
		break;
	}
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

void handle_click_event(app_data_t *app_data)
{
	if (!app_data->encoder_data.pressed)
	{
		return;
	}
	

	switch (app_data->screen_mode)
	{
	case MAIN_SCREEN:
	{
		int menu_position = ((app_data->encoder_data.position % 3) + 3) % 3;
		switch (menu_position)
		{
		case 0:
			app_data->screen_mode = CHANGE_TIME_SCREEN;
			break;
		case 1:
			app_data->screen_mode = CHANGE_DATE_SCREEN;
			break;
		case 2:
			app_data->screen_mode = ENV_HISTORY_SCREEN;
			break;
		default:
			break;
		}
		app_data->encoder_data.position = 0;
		ESP_LOGI(TAG, "Menu item selected: %d, screen: %d", menu_position, app_data->screen_mode);
		break;
	}
	case CHANGE_TIME_SCREEN:
		break;
	case CHANGE_DATE_SCREEN:
		break;
	case ENV_HISTORY_SCREEN:
		break;
	default:
		break;
	}
	app_data->encoder_data.pressed = false;
}