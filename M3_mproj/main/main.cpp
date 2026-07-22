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

// encoder and button GPIOs
const gpio_num_t ENC_A_GPIO = GPIO_NUM_17;
const gpio_num_t ENC_B_GPIO = GPIO_NUM_16;
const gpio_num_t ENC_BUTTON_GPIO = GPIO_NUM_15;
constexpr uint8_t BUTTONS_COUNT = 1;

constexpr uint16_t ENCODER_DEBOUNCE_NS = 1000;
constexpr int32_t ENCODER_MAX_COUNT = INT16_MAX; // 32767
constexpr int32_t ENCODER_MIN_COUNT = INT16_MIN; // -32768

constexpr uint16_t LONG_PRESS_TIME = 1000; // час довгого натискання (мс)
constexpr uint16_t REPEAT_INTERVAL = 500;  // інтервал повторення події (мс)

// Reporting interval for encoder position
constexpr int64_t REPORT_INTERVAL_US = 2'000'000; // 2 сек

// Servo control variables
constexpr gpio_num_t SERVO1_GPIO = GPIO_NUM_40;
constexpr gpio_num_t SERVO2_GPIO = GPIO_NUM_41;
static constexpr uint8_t CHANNEL_COUNT = 2;
static constexpr uint16_t FREQ_HZ = 50;
static constexpr uint16_t PERIOD_MS = 1000 / FREQ_HZ;
static constexpr uint16_t MAX_ANGLE = 90;
static constexpr adc_bitwidth_t ADC_BIT = ADC_BITWIDTH_12;
static constexpr ledc_timer_bit_t LEDC_TIMER_BIT = LEDC_TIMER_12_BIT;
constexpr int TICKS_FOR_FULL_SERVO_SWEEP = 60; // максимальна кількість імпульсів енкодера для повного обертання серво
constexpr int SERVO_MIN_PULSE_US = 1000;
constexpr int SERVO_MAX_PULSE_US = 2000;

// led management variables
static bool led_state = false;
constexpr gpio_num_t LED_GPIO = GPIO_NUM_4;

static pwm_context servo2pwm_ctx = {};


int angleToDuty(int angle)
{
    constexpr int PERIOD_US = PERIOD_MS * 1000; // 1'000'000 / FREQ_HZ = 20 мс при 50Гц

    int max_duty = (1 << LEDC_TIMER_BIT) - 1;
    int pulse_us = SERVO_MIN_PULSE_US + (angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) / MAX_ANGLE;
    return (pulse_us * max_duty) / PERIOD_US;
}

void handle_mqtt_message(const char *topic, const char *data, esp_mqtt_client_handle_t client)
{
	if (topic == NULL || data == NULL || client == NULL)
	{
		return;
	}

	if (strcmp(topic, MQTT_LED_COMMANDS) == 0)
	{
		// Command: ON/OFF
		if (strcmp(data, "ON") == 0)
		{
			ESP_LOGI(TAG, "Command: LED ON");

			if (gpio_set_level(LED_GPIO, 1) == ESP_OK)
			{
				led_state = true;
				esp_mqtt_client_publish(client, MQTT_STATUS, led_state ? "ON" : "OFF", 0, 0, 0);
			}

			else
			{
				ESP_LOGE(TAG, "Failed to set LED ON");
			}
		}

		else if (strcmp(data, "OFF") == 0)
		{
			ESP_LOGI(TAG, "Command: LED OFF");

			if (gpio_set_level(LED_GPIO, 0) == ESP_OK)
			{
				led_state = false;
				esp_mqtt_client_publish(client, MQTT_STATUS, led_state ? "ON" : "OFF", 0, 0, 0);
			}
			else
			{
				ESP_LOGE(TAG, "Failed to set LED OFF");
			}
		}

		// Status command: returns the current state of the LED
		else if (strcmp(data, "STATUS") == 0)
		{
			if (esp_mqtt_client_publish(client, MQTT_STATUS, led_state ? "ON" : "OFF", 0, 0, 0) < 0)
			{
				ESP_LOGE(TAG, "Failed to publish status");
			}
			else
			{
				ESP_LOGI(TAG, "Status sent");
			}
		}
		else
		{
			ESP_LOGW(TAG, "Unknown command: %s", data);
		}
	}

	else if(strcmp(topic, MQTT_SERVO_ANGLE) == 0)
	{
		int angle = atoi(data);
		if (angle < 0) angle = 0;
		if (angle > MAX_ANGLE) angle = MAX_ANGLE;
		int duty = angleToDuty(angle);
		esp_err_t err = pwm_set_duty(&servo2pwm_ctx, duty); // mqtt серво2
		if (err != ESP_OK)
		{
			ESP_LOGE(TAG, "Failed to set PWM duty for Servo2");
		}
	}

	
}

bool getled_state()
{
	return led_state;
}

static void buttons_process(deb *btns, uint8_t btns_count, enc_context &enc_ctx)
{
	for (uint8_t i = 0; i < btns_count; i++)
	{
		ButtonEvent event = deb_get_btn_event(&btns[i]);
		// ButtonState status = deb_get_btn_status(&btns[i]);

		if (event == LONG_PRESS_EVENT || event == CLICK_EVENT)
		{
			led_state = !led_state;
			enc_clear_count(&enc_ctx); // Reset encoder count on button press
		}
	}
}

int encoderToServoAngle(int raw_ticks)
{
    int angle = (raw_ticks * MAX_ANGLE) / TICKS_FOR_FULL_SERVO_SWEEP;
    if (angle < 0)   angle = 0;
    if (angle > MAX_ANGLE) angle = MAX_ANGLE;
    return angle;
}


extern "C" void app_main(void)
{
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

	// Сконфігурувати LED GPIO
	gpio_config_t io_conf = {};
	io_conf.pin_bit_mask = (1ULL << LED_GPIO);
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK(gpio_config(&io_conf));

	// ENC CONTEXT Init
	enc_context enc_ctx = {};
	enc_ctx.low_limit = ENCODER_MIN_COUNT;
	enc_ctx.high_limit = ENCODER_MAX_COUNT;
	enc_ctx.max_glitch_ns = ENCODER_DEBOUNCE_NS;
	enc_ctx.gpio_num_a = ENC_A_GPIO;
	enc_ctx.gpio_num_b = ENC_B_GPIO;
	enc_ctx.initialized = false;
	ESP_ERROR_CHECK(enc_init(&enc_ctx));

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
	// 1. Конфігураційні параметри (Заповнює користувач)
	btns[0].pin = ENC_BUTTON_GPIO;
	btns[0].longPressTime = LONG_PRESS_TIME;
	btns[0].repeatInterval = REPEAT_INTERVAL;
	btns[0].activeLevel = 0;	  // Натиснута - 0; натиснута - 1
	btns[0].shiftBits = 4;		  // 4 - 0x0F ; 8 - 0xFF
	btns[0].shiftRegister = 0x0F; // 4 - 0x0F ; 8 - 0xFF
	deb_init(&btns[0]);

    // PWM Initialization for Servo1 Control
    pwm_context servo1pwm_ctx = {};
    servo1pwm_ctx.gpio_num = SERVO1_GPIO;
    servo1pwm_ctx.timer_num = LEDC_TIMER_0;
    servo1pwm_ctx.channel = LEDC_CHANNEL_0;
    servo1pwm_ctx.freq_hz = FREQ_HZ;
    servo1pwm_ctx.duty_resolution = LEDC_TIMER_BIT;
    servo1pwm_ctx.speed_mode = LEDC_LOW_SPEED_MODE;
    servo1pwm_ctx.initialized = false;
    pwm_init(&servo1pwm_ctx);

	// PWM Initialization for Servo2 Control
    servo2pwm_ctx.gpio_num = SERVO2_GPIO;
    servo2pwm_ctx.timer_num = LEDC_TIMER_0;
    servo2pwm_ctx.channel = LEDC_CHANNEL_1;
    servo2pwm_ctx.freq_hz = FREQ_HZ;
    servo2pwm_ctx.duty_resolution = LEDC_TIMER_BIT;
    servo2pwm_ctx.speed_mode = LEDC_LOW_SPEED_MODE;
    servo2pwm_ctx.initialized = false;
    pwm_init(&servo2pwm_ctx);


	wifi_init_sta();

	if (isWifiConnected())
	{
		mqtt_set_message_handler(handle_mqtt_message);
		mqtt_start();
	}

	int current_enc_val = 0;
	int64_t last_report_us = 0;
	int last_published_angle = -1; // -1, щоб перша реальна публікація точно відбулась

	// Get the MQTT client handle
	esp_mqtt_client_handle_t mqtt_client = get_mqtt_client();

	while (1)
	{
		// Зчитуємо поточне значення PCNT
		ESP_ERROR_CHECK(enc_get_count(&enc_ctx, &current_enc_val));

		// Зчитуємо стани входів A, B PCNT
		int a_state = gpio_get_level(ENC_A_GPIO);
		int b_state = gpio_get_level(ENC_B_GPIO);

		// Calculate the servo angle based on encoder value and set PWM duty cycle
		int angle = encoderToServoAngle(current_enc_val);

		if (angle != last_published_angle)
		{
			char angle_str[8];
			snprintf(angle_str, sizeof(angle_str), "%d", angle);
			esp_mqtt_client_publish(mqtt_client, MQTT_SERVO_ANGLE, angle_str, 0, 0, 0); // серво2 — через MQTT
			last_published_angle = angle;
		}

		int duty = angleToDuty(angle);
		esp_err_t err = pwm_set_duty(&servo1pwm_ctx, duty); // локальне серво1
		if (err != ESP_OK)
		{
			ESP_LOGE(TAG, "Failed to set PWM duty for Servo1");
		}

		// Зчитуємо стан кнопки та керуємо LED
		ESP_ERROR_CHECK(deb_btns_update(btns, BUTTONS_COUNT));
		buttons_process(btns, BUTTONS_COUNT, enc_ctx);

		gpio_set_level(LED_GPIO, led_state);

		// Report encoder position every REPORT_INTERVAL_US microseconds
		int64_t now = esp_timer_get_time();
		if (now - last_report_us >= REPORT_INTERVAL_US)
		{
			ESP_LOGI(TAG, "Encoder position: %d | Servo Angle: %d | Duty: %d | A: %d | B: %d", current_enc_val, angle, duty, a_state, b_state);
			
			// Publish the encoder position to MQTT
			char pos_str[12];
			snprintf(pos_str, sizeof(pos_str), "%d", current_enc_val);
			esp_mqtt_client_publish(mqtt_client, MQTT_ENC_POS, pos_str, 0, 0, 0);
			
			last_report_us = now;
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
