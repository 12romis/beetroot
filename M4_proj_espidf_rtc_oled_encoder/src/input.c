#include "app_rtc.h"
#include "input.h"

// Обгортання значення в діапазон [min, max] (включно), коректно і для від'ємних зсувів
static int wrap_int(int value, int min, int max)
{
	int range = max - min + 1;
	return ((value - min) % range + range) % range + min;
}

// Скільки полів навігації (включно з "Back", де воно є) на кожному екрані
static int screen_field_count(screen_mode_t screen)
{
	switch (screen)
	{
	case MAIN_SCREEN:
		return 3; // change time / change date / env history
	case CHANGE_TIME_SCREEN:
		return 3; // hour / minute / Back
	case CHANGE_DATE_SCREEN:
		return 4; // day / month / year / Back
	case ENV_HISTORY_SCREEN:
		return 1;
	}
	return 1;
}

// Змінити значення поля, що зараз редагується (CHANGE_TIME/CHANGE_DATE), на delta
static void apply_field_delta(app_data_t *app_data, int delta)
{
	if (app_data->screen_mode == CHANGE_TIME_SCREEN)
	{
		if (app_data->selected_field == 0) // hour
		{
			app_data->time.tm_hour = wrap_int(app_data->time.tm_hour + delta, 0, 23);
		}
		else if (app_data->selected_field == 1) // minute
		{
			app_data->time.tm_min = wrap_int(app_data->time.tm_min + delta, 0, 59);
		}
	}
	else if (app_data->screen_mode == CHANGE_DATE_SCREEN)
	{
		if (app_data->selected_field == 0) // day
		{
			app_data->time.tm_mday = wrap_int(app_data->time.tm_mday + delta, 1, 31);
		}
		else if (app_data->selected_field == 1) // month
		{
			app_data->time.tm_mon = wrap_int(app_data->time.tm_mon + delta, 0, 11);
		}
		else if (app_data->selected_field == 2) // year
		{
			app_data->time.tm_year += delta; // без обгортання — роки просто ростуть/спадають
		}
	}
}

// Застосувати накопичену з минулого разу дельту обертання енкодера і скинути
// лічильник — position завжди означає лише "зміну з минулого разу", а не
// абсолютне значення. Без редагування — рухає фокус (selected_field),
// під час редагування — міняє значення вибраного поля.
static void consume_encoder_delta(app_data_t *app_data)
{
	int delta = app_data->encoder_data.position;
	if (delta == 0)
	{
		return;
	}
	app_data->encoder_data.position = 0;

	if (app_data->field_editing)
	{
		apply_field_delta(app_data, delta);
	}
	else
	{
		int count = screen_field_count(app_data->screen_mode);
		app_data->selected_field = wrap_int(app_data->selected_field + delta, 0, count - 1);
	}
}

// Підтвердити поле, що редагувалось, записавши час у RTC
static void confirm_field_edit(app_data_t *app_data)
{
	app_data->field_editing = false;
	rtc_write_time(app_data);
}

void handle_encoder_input(app_data_t *app_data)
{
	// Обертання енкодера обробляємо щотіку, незалежно від кліку
	consume_encoder_delta(app_data);

	if (!app_data->encoder_data.pressed)
	{
		return;
	}
	app_data->encoder_data.pressed = false;

	switch (app_data->screen_mode)
	{
	case MAIN_SCREEN:
		switch (app_data->selected_field)
		{
		case 0:
			app_data->screen_mode = CHANGE_TIME_SCREEN;
			break;
		case 1:
			app_data->screen_mode = CHANGE_DATE_SCREEN;
			break;
		case 2:
			app_data->screen_mode = ENV_HISTORY_SCREEN;
			break;
		}
		app_data->selected_field = 0;
		break;

	case CHANGE_TIME_SCREEN:
		if (app_data->selected_field == 2) // Back
		{
			app_data->screen_mode = MAIN_SCREEN;
			app_data->selected_field = 0;
			app_data->field_editing = false;
		}
		else if (app_data->field_editing)
		{
			confirm_field_edit(app_data);
		}
		else
		{
			app_data->field_editing = true;
		}
		break;

	case CHANGE_DATE_SCREEN:
		if (app_data->selected_field == 3) // Back
		{
			app_data->screen_mode = MAIN_SCREEN;
			app_data->selected_field = 0;
			app_data->field_editing = false;
		}
		else if (app_data->field_editing)
		{
			confirm_field_edit(app_data);
		}
		else
		{
			app_data->field_editing = true;
		}
		break;

	case ENV_HISTORY_SCREEN:
		break;
	}
}
