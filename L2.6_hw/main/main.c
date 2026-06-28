#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"

#define RGB_PIN     GPIO_NUM_48
#define LED_PIN     GPIO_NUM_4
#define BTN_PIN     GPIO_NUM_15

#define POLL_MS         10
#define DEBOUNCE_TICKS  (20  / POLL_MS)   // 20ms → 2 тіки
#define PRINT_TICKS     (3000 / POLL_MS)  // 3000ms → 300 тіків

static const char *TAG = "main";

typedef enum {
    BTN_IDLE,
    BTN_DEBOUNCE_PRESS,
    BTN_PRESSED,
    BTN_DEBOUNCE_RELEASE,
} btn_state_t;

void app_main(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask  = (1ULL << BTN_PIN);
    io_conf.mode          = GPIO_MODE_INPUT;
    io_conf.pull_down_en  = GPIO_PULLDOWN_ENABLE;
    gpio_config(&io_conf);

    led_strip_config_t strip_config = {
        .strip_gpio_num   = RGB_PIN,
        .max_leds         = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model        = LED_MODEL_WS2812,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_ERROR_CHECK(led_strip_clear(led_strip));

    ESP_LOGI(TAG, "Starting...");

    unsigned int count       = 0;
    btn_state_t  state       = BTN_IDLE;
    unsigned int hold_ticks  = 0;
    unsigned int print_ticks = 0;

    while (1) {
        bool high = gpio_get_level(BTN_PIN) == 1;
        
        switch (state) {
            case BTN_IDLE:
                if (high) {
                    state = BTN_DEBOUNCE_PRESS;
                    hold_ticks = 0;
                }
                break;

            case BTN_DEBOUNCE_PRESS:
                if (!high) {
                    state = BTN_IDLE;
                } else if (++hold_ticks >= DEBOUNCE_TICKS) {
                    state = BTN_PRESSED;
                    count++;
                    gpio_set_level(LED_PIN, 1);
                }
                break;

            case BTN_PRESSED:
                if (!high) {
                    state = BTN_DEBOUNCE_RELEASE;
                    hold_ticks = 0;
                    gpio_set_level(LED_PIN, 0);
                }
                break;

            case BTN_DEBOUNCE_RELEASE:
                if (high) {
                    state = BTN_PRESSED;
                } else if (++hold_ticks >= DEBOUNCE_TICKS) {
                    state = BTN_IDLE;
                }
                break;
        }

        if (++print_ticks >= PRINT_TICKS) {
            ESP_LOGI(TAG, "Count: %u", count);
            print_ticks = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}
