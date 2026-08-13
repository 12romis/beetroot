#pragma once

#include "app.h"

// Ініціалізація RTC (DS1307): підключення дескриптора + виставлення часу компіляції
void rtc_dev_init(app_data_t *app_data);
// Читання поточного часу з RTC у app_data->time
void rtc_read(app_data_t *app_data);
// Запис app_data->time у RTC (використовується при підтвердженні редагування поля)
esp_err_t rtc_write_time(app_data_t *app_data);
