#pragma once

#include "app.h"

// Ініціалізація OLED-дисплея
void oled_dev_init(void);
// Оновлення OLED-дисплея (малює екран відповідно до app_data->screen_mode)
void oled_update(app_data_t *app_data);
