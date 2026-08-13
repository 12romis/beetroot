#pragma once

#include "app.h"

// Ініціалізація BMP280
void bmp280_dev_init(void);
// Читання даних з BME280/BMP280 (температура, вологість, тиск у гПа)
void bme280_read(app_data_t *app_data);
