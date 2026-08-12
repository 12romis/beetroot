#include "esp_log.h"
#include "i2cdev.h"
#include "ssd1306.h"
#include "ds1307.h"
#include "bmp280.h"
#include "iot_knob.h"
#include "iot_button.h"
#include "app.h"

static ssd1306_t oled_dev;
static i2c_dev_t rtc_dev;
static bmp280_t bmp_dev;
static knob_handle_t s_knob = NULL;
static button_handle_t s_btn = NULL;

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

// Ініціалізація OLED-дисплея
void oled_dev_init()
{
	memset(&oled_dev, 0, sizeof(ssd1306_t));

	esp_err_t err = ssd1306_init_desc(&oled_dev,
									  I2C_PORT,
									  I2C_SDA_GPIO,
									  I2C_SCL_GPIO);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "oled_init: %s", esp_err_to_name(err));
	}

	err = ssd1306_init_display(&oled_dev);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "oled_init: %s", esp_err_to_name(err));
	}

	ssd1306_clear(&oled_dev);
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
}

// Оновлення OLED-дисплея
void oled_update(app_data_t *app_data)
{
	char text_buf[32] = {0};

	ssd1306_draw_string(&oled_dev, 5, 1, text_buf);

	// Дата
	snprintf(text_buf, sizeof(text_buf),
			 "%02d.%02d.%04d",
			 app_data->time.tm_mday,
			 app_data->time.tm_mon + 1,
			 app_data->time.tm_year + 1900);

	ssd1306_draw_string(&oled_dev, 5, 1, text_buf);

	// Час
	snprintf(text_buf, sizeof(text_buf),
			 "%02d:%02d:%02d",
			 app_data->time.tm_hour,
			 app_data->time.tm_min,
			 app_data->time.tm_sec);

	ssd1306_draw_string(&oled_dev, 5, 2, text_buf);

	// Температура
	snprintf(text_buf, sizeof(text_buf),
			 "T: %.1f C",
			 app_data->bme280_data.temperature);

	ssd1306_draw_string(&oled_dev, 5, 4, text_buf);

	// Вологість
	snprintf(text_buf, sizeof(text_buf),
			 "H: %.1f %%",
			 app_data->bme280_data.humidity);

	ssd1306_draw_string(&oled_dev, 5, 5, text_buf);

	// Тиск
	snprintf(text_buf, sizeof(text_buf),
			 "P: %.1f hPa",
			 app_data->bme280_data.pressure);

	ssd1306_draw_string(&oled_dev, 5, 6, text_buf);
}
