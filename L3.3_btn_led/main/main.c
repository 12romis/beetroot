#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "driver/ledc.h"

#define LED_PIN       GPIO_NUM_4
#define LED_PIN2      GPIO_NUM_5
#define DELAY_MS     10

#define FREQ 25000
#define DUTY_MAX 1023

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_CHANNEL2 LEDC_CHANNEL_1
#define LEDC_RESOLUTION LEDC_TIMER_10_BIT


static const char *TAG = "main";

void app_main(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz = FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = LED_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0, //(1 << LEDC_RESOLUTION) * DUTY / 100,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel_config_t ledc_channel2 = {
        .gpio_num = LED_PIN2,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL2,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0, //(1 << LEDC_RESOLUTION) * DUTY / 100,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel2));

    ESP_LOGI(TAG, "Starting...");

    while (1) {
        for(int d = 0; d <= DUTY_MAX; d += 10) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, d);
            // ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, (1 << LEDC_RESOLUTION) * d / 100);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
            
            // ESP_LOGI(TAG, "Duty: %d%%", d);
            vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
        }
        vTaskDelay(pdMS_TO_TICKS(DELAY_MS*4));

        for(int d = DUTY_MAX; d >= 0; d -= 10) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, d);
            // ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, (1 << LEDC_RESOLUTION) * d / 100);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
            
            // ESP_LOGI(TAG, "Duty: %d%%", d);
            vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
        }



        for(int d = 0; d <= DUTY_MAX; d += 20) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL2, d);
            // ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, (1 << LEDC_RESOLUTION) * d / 100);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL2);
            
            // ESP_LOGI(TAG, "Duty: %d%%", d);
            vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
        }
        vTaskDelay(pdMS_TO_TICKS(DELAY_MS*4));

        for(int d = DUTY_MAX; d >= 0; d -= 20) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL2, d);
            // ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, (1 << LEDC_RESOLUTION) * d / 100);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL2);
            
            // ESP_LOGI(TAG, "Duty: %d%%", d);
            vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
        }
    }
}
