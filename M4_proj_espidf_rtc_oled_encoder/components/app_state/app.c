#include "app.h"

// Єдиний екземпляр стану застосунку — увесь інший код отримує доступ до
// нього лише через get_app_data(), щоб не плодити глобальні змінні по файлах
static app_data_t app_data;

// функція для отримання вказівника на структуру app_data_t
app_data_t *get_app_data(void)
{
	return &app_data;
}
