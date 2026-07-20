#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#define POT_1       ADC_CHANNEL_3
#define POLL_MS     500


static const char *TAG = "main";

void app_main(void)
{
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, POT_1, &chan_cfg));


    ESP_LOGI(TAG, "Starting...");

    while (1) {
        int raw = 0;
        adc_oneshot_read(adc1_handle, POT_1, &raw);
        ESP_LOGI(TAG, "Raw value: %d", raw);

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}
