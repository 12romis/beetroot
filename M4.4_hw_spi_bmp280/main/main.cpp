#include <stdio.h>
#include <time.h>
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"
#include "ds1307.h"  // Підключення бібліотеки для роботи з RTC DS1307
#include "ssd1306.h" // Підключення бібліотеки для роботи з OLED-дисплеєм SSD1306

// Піни шини I2C
#define SDA_GRPIO GPIO_NUM_8
#define SCL_GPIO GPIO_NUM_9
#define RESET_GPIO -1              // немає піна RESET на 4-піновому модулі

static const char TAG[] = "main";

// Назви днів тижня. Індекс 0 = неділя
const char daysOfTheWeek[7][13] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"};


// Показання датчика BMP280 за один вимір
struct SensorData
{
  float temperature; // Температура, °C
  float pressure;    // Тиск, гПа
};

// Доповнює число нулем зліва, якщо воно однозначне (напр. 5 -> "05")
String twoDigits(int value)
{
  return (value < 10 ? "0" : "") + String(value);
}

// Зчитує поточні показання температури та тиску з BMP280
// SensorData readSensorData()
// {
//   SensorData data;
//   data.temperature = bmp.readTemperature();
//   data.pressure = bmp.readPressure() / 100.0F; // переведення з Па у гПа
//   return data;
// }

// Формує рядок дати у форматі ДД.ММ.РРРР
String formatDate(const DateTime &dt_now)
{
  return twoDigits(dt_now.day()) + "." + twoDigits(dt_now.month()) + "." + String(dt_now.year());
}

// Формує рядок часу у форматі ГГ:ХХ:СС
String formatTime(const DateTime &dt_now)
{
  return twoDigits(dt_now.hour()) + ":" + twoDigits(dt_now.minute()) + ":" + twoDigits(dt_now.second());
}

// Повертає назву дня тижня для заданого моменту часу
String formatDayOfWeek(const DateTime &dt_now)
{
  return daysOfTheWeek[dt_now.dayOfTheWeek()];
}

// Виводить день тижня, дату, час і показання датчика на OLED-дисплей
void updateDisplay(const SSD1306_t *oled_dev, 
	const DateTime &dt_now, 
	// const SensorData &sensorData
)
{
	ssd1306_clear_screen(&oled_dev, false);  // Очищення екрану
	ssd1306_contrast(&oled_dev, 0xff);  // Максимальна контрастність

	//   u8g2.setFont(u8g2_font_6x13B_t_cyrillic); 
	// int timeWidth = u8g2.getUTF8Width(formatTime(dt_now).c_str());
	// int screenWidth = u8g2.getDisplayWidth();
	// int dateX = (screenWidth - timeWidth) / 2; // Центрування часу по горизонталі
	// u8g2.drawUTF8(dateX, 14, formatTime(dt_now).c_str());
	char timeStr[9] = formatTime(dt_now).c_str();
	ssd1306_display_text(&oled_dev, 1, timeStr, strlen(timeStr), false);

	char dayStr[20] = formatDayOfWeek(dt_now).c_str();
	ssd1306_display_text(&oled_dev, 5, 30, dayStr, strlen(dayStr), false);

	char dateStr[20] = formatDate(dt_now).c_str();
	ssd1306_display_text(&oled_dev, 62, 30, dateStr, strlen(dateStr), false);

//   u8g2.drawUTF8(5, 45, ("Temp: " + String(sensorData.temperature, 1) + " C").c_str());
//   u8g2.drawUTF8(5, 60, ("Press: " + String(sensorData.pressure, 1) + " hPa").c_str());

}



extern "C" void app_main(void)
{
	// Initialize RTC DS1307
	i2c_dev_t dev = {0};
	struct tm dt_now;
	ds1307_init_desc(&dev, I2C_NUM_0, SDA_GRPIO, SCL_GPIO);

	SSD1306_t oled_dev;
    // Ініціалізація I2C-шини та панелі
    i2c_master_init(&oled_dev, SDA_GRPIO, SCL_GPIO, RESET_GPIO);
    ssd1306_init(&oled_dev, 128, 64);

    ssd1306_clear_screen(&oled_dev, false);  // Очищення екрану
    ssd1306_contrast(&oled_dev, 0xff);  // Максимальна контрастність


	while (1)
	{
		ds1307_get_time(&dev, &dt_now);

		// DateTime now = readCurrentTime();
		// SensorData sensorData = readSensorData();

		ESP_LOGI(TAG, "Current time: %s, %s %s", formatDayOfWeek(dt_now), formatDate(dt_now), formatTime(dt_now));

		updateDisplay(&oled_dev, dt_now);

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
