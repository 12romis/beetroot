#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "led_strip.h"

#define RGB_PIN     GPIO_NUM_48
#define LED_PIN     GPIO_NUM_4
#define BTN_PIN     GPIO_NUM_15

#define POLL_MS         10
#define DEBOUNCE_TICKS  (20   / POLL_MS)
#define PRINT_TICKS     (3000 / POLL_MS)

static const char *TAG = "main";

typedef enum {
    BTN_IDLE,
    BTN_DEBOUNCE_PRESS,
    BTN_PRESSED,
    BTN_DEBOUNCE_RELEASE,
} btn_state_t;

// Таблиця кольорів для RGB: {R, G, B}
static const uint8_t colors[][3] = {
    {255,   0,   0},  // червоний
    {  0, 255,   0},  // зелений
    {  0,   0, 255},  // синій
    {255, 255,   0},  // жовтий
    {  0, 255, 255},  // блакитний
    {255,   0, 255},  // пурпурний
};
#define NUM_COLORS (sizeof(colors) / sizeof(colors[0]))

// ISR-callback таймера — викликається кожні 500 мс апаратним таймером.
// IRAM_ATTR: код розміщується в IRAM (швидка пам'ять),
// бо під час переривання кеш flash може бути недоступний.
static void IRAM_ATTR led_blink_cb(void *arg)
{
    static bool led_state = false;
    led_state = !led_state;
    gpio_set_level(LED_PIN, led_state);
}

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

    io_conf.pin_bit_mask = (1ULL << BTN_PIN);
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
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

    // Створення і старт апаратного таймера переривання
    esp_timer_handle_t blink_timer;
    const esp_timer_create_args_t blink_args = {
        .callback        = led_blink_cb,
        .dispatch_method = ESP_TIMER_ISR,   // викликати callback прямо з ISR
        .name            = "led_blink",
    };
    ESP_ERROR_CHECK(esp_timer_create(&blink_args, &blink_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(blink_timer, 500 * 1000)); // 500 000 мкс = 500 мс

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
                    // Циклічна зміна кольору RGB при кожному кліку
                    uint8_t ci = (count - 1) % NUM_COLORS;
                    led_strip_set_pixel(led_strip, 0, colors[ci][0], colors[ci][1], colors[ci][2]);
                    led_strip_refresh(led_strip);
                }
                break;

            case BTN_PRESSED:
                if (!high) {
                    state = BTN_DEBOUNCE_RELEASE;
                    hold_ticks = 0;
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
