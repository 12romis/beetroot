#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/uhci.h"
#include "esp_timer.h"
#include "esp_adc/adc_continuous.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "led.h"

#define LED_OUT GPIO_NUM_18

// Потенціометр підключений повзунком до GPIO4 = ADC1_CHANNEL_3 на ESP32-S3
#define ADC_UNIT ADC_UNIT_1
#define ADC_CHANNEL ADC_CHANNEL_3
#define ADC_ATTEN ADC_ATTEN_DB_12	 // приблизно 0 - 3.3V на вході
#define ADC_BIT_WIDTH ADC_BITWIDTH_12 // 12-бітний результат: 0 - 4095

#define ADC_SAMPLE_FREQ_HZ 20000	// частота вибірки АЦП, Гц
#define ADC_FRAME_SAMPLES 64		// скільки вибірок вміщує один DMA-фрейм
#define ADC_FRAME_SIZE (ADC_FRAME_SAMPLES * SOC_ADC_DIGI_RESULT_BYTES)

// Окремий UART-порт для DMA-передачі (UART0 залишається під стандартну
// консоль логів ESP_LOGI, щоб не конфліктувати з нею)
#define UART_DMA_PORT UART_NUM_1
#define UART_DMA_TX_GPIO GPIO_NUM_17
#define UART_DMA_BAUD 115200
#define UART_DMA_MSG_SIZE 64 // фіксований розмір "рядка", вирівняний під DMA

static const char *TAG = "ADC_DMA";

// Ініціалізація UART + UHCI (UART Host Controller Interface).
// UHCI - це апаратний "міст" між UART і GDMA: замість того, щоб CPU
// по одному байту штовхав дані у FIFO через переривання, UHCI сам,
// через DMA, вичитує буфер з пам'яті і подає його у UART-передавач.
static uhci_controller_handle_t uart_dma_init(void)
{
	uart_config_t uart_config = {
		.baud_rate = UART_DMA_BAUD,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	ESP_ERROR_CHECK(uart_param_config(UART_DMA_PORT, &uart_config));
	// RX нам не потрібен (лише передача), тому передаємо -1
	ESP_ERROR_CHECK(uart_set_pin(UART_DMA_PORT, UART_DMA_TX_GPIO, -1, -1, -1));

	uhci_controller_config_t uhci_cfg = {
		.uart_port = UART_DMA_PORT,
		.tx_trans_queue_depth = 4,
		.max_transmit_size = UART_DMA_MSG_SIZE,
		.max_receive_internal_mem = 64, // RX не використовується, лишаємо мінімум
		.dma_burst_size = 32,
	};

	uhci_controller_handle_t uhci_handle = NULL;
	ESP_ERROR_CHECK(uhci_new_controller(&uhci_cfg, &uhci_handle));

	return uhci_handle;
}

// Ініціалізація ADC у режимі continuous (читання через DMA)
static adc_continuous_handle_t adc_continuous_init(void)
{
	adc_continuous_handle_t handle = NULL;

	// Конфігурація внутрішнього пулу драйвера: скільки пам'яті
	// може займати черга готових DMA-фреймів і який розмір одного фрейму
	adc_continuous_handle_cfg_t adc_config = {
		.max_store_buf_size = ADC_FRAME_SIZE * 4,
		.conv_frame_size = ADC_FRAME_SIZE,
	};
	ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

	// Опис одного (єдиного) каналу, який буде оцифровувати АЦП
	adc_digi_pattern_config_t adc_pattern = {
		.atten = ADC_ATTEN,
		.channel = ADC_CHANNEL,
		.unit = ADC_UNIT,
		.bit_width = ADC_BIT_WIDTH,
	};

	adc_continuous_config_t dig_cfg = {
		.pattern_num = 1,
		.adc_pattern = &adc_pattern,
		.sample_freq_hz = ADC_SAMPLE_FREQ_HZ,
		.conv_mode = ADC_CONV_SINGLE_UNIT_1,
		.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
	};
	ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

	return handle;
}

extern "C" void app_main()
{
	Led led(LED_OUT);

	adc_continuous_handle_t adc_handle = adc_continuous_init();
	uhci_controller_handle_t uart_dma_handle = uart_dma_init();

	// Апаратний АЦП починає безперервно оцифровувати сигнал
	// і сам, без участі CPU, переносити результати в пам'ять через DMA
	ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

	uint8_t frame_buf[ADC_FRAME_SIZE];

	// Буфер для UART-DMA повинен лежати у DMA-сумісній пам'яті й бути
	// вирівняним - виділяємо його один раз і надалі перевикористовуємо
	uint8_t *uart_dma_buf = (uint8_t *)heap_caps_aligned_alloc(
		64, UART_DMA_MSG_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
	if (!uart_dma_buf)
	{
		ESP_LOGE(TAG, "Failed to allocate UART DMA buffer");
		return;
	}

	while (1)
	{
		uint32_t out_len = 0;

		// Чекаємо, поки DMA назбирає черговий фрейм вибірок, і забираємо його.
		// adc_continuous_read блокує задачу (без активного очікування CPU),
		// доки не набереться ADC_FRAME_SIZE байт або не спливе таймаут.
		esp_err_t ret = adc_continuous_read(adc_handle, frame_buf, ADC_FRAME_SIZE,
											 &out_len, 1000);

		if (ret == ESP_OK && out_len > 0)
		{
			// Один "сирий" фрейм містить кілька вибірок формату adc_digi_output_data_t.
			// Беремо останню вибірку з фрейму як поточне значення потенціометра.
			adc_digi_output_data_t *last =
				(adc_digi_output_data_t *)&frame_buf[out_len - SOC_ADC_DIGI_RESULT_BYTES];

			if (last->type2.channel == ADC_CHANNEL)
			{
				// Орієнтовний переклад "сирого" 12-бітного значення у вольти
				// (без калібрування esp_adc_cal, тому точність приблизна)
				float voltage = (last->type2.data / 4095.0f) * 3.3f;
				ESP_LOGI(TAG, "raw=%u  voltage=%.2f V  (samples in frame: %lu)",
						 (unsigned)last->type2.data, voltage,
						 (unsigned long)(out_len / SOC_ADC_DIGI_RESULT_BYTES));

				// Формуємо фіксований 64-байтний "рядок" і відправляємо його
				// в UART_DMA_PORT через UHCI. memset заповнює хвіст пробілами,
				// а останні 2 байти завжди \r\n, щоб рядок коректно
				// переносився в терміналі незалежно від довжини тексту.
				memset(uart_dma_buf, ' ', UART_DMA_MSG_SIZE);
				int n = snprintf((char *)uart_dma_buf, UART_DMA_MSG_SIZE - 2,
								  "raw=%u voltage=%.2fV", (unsigned)last->type2.data, voltage);
				if (n >= 0 && n < UART_DMA_MSG_SIZE - 2)
				{
					uart_dma_buf[n] = ' '; // прибираємо '\0', який залишив snprintf
				}
				uart_dma_buf[UART_DMA_MSG_SIZE - 2] = '\r';
				uart_dma_buf[UART_DMA_MSG_SIZE - 1] = '\n';

				// Неблокуючий запуск DMA-передачі: CPU лише "запускає" відправку
				ESP_ERROR_CHECK(uhci_transmit(uart_dma_handle, uart_dma_buf, UART_DMA_MSG_SIZE));
				// Чекаємо завершення, щоб безпечно перевикористати той самий буфер
				// на наступній ітерації (буфер не можна чіпати, доки DMA ним володіє)
				uhci_wait_all_tx_transaction_done(uart_dma_handle, 1000);
			}
		}
		else
		{
			ESP_LOGW(TAG, "ADC read timeout/error: %s", esp_err_to_name(ret));
		}

		// Блимання світлодіодом для візуалізації циклу
		led.on();
		vTaskDelay(50 / portTICK_PERIOD_MS);
		led.off();
		vTaskDelay(450 / portTICK_PERIOD_MS);
	}

	adc_continuous_stop(adc_handle);
	adc_continuous_deinit(adc_handle);
	uhci_del_controller(uart_dma_handle);
	free(uart_dma_buf);
}
