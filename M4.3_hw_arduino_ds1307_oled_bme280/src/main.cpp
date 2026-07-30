#include <RTClib.h>          // Бібліотека для роботи з модулями RTC (DS3231 / DS1307)
#include <U8g2lib.h>         // Бібліотека для керування OLED-дисплеєм
#include <Adafruit_Sensor.h> // Базовий інтерфейс сенсорів Adafruit (потрібен для BMP280)
#include <Adafruit_BMP280.h> // Бібліотека для датчика температури/тиску BMP280
#include <Wire.h>            // Бібліотека для роботи з шиною I2C

// Піни шини I2C
#define I2C_SDA 8
#define I2C_SCL 9

// I2C-адреса датчика BMP280 (0x76 або 0x77 залежно від модуля)
#define BMP280_I2C_ADDRESS 0x76

#define SEALEVELPRESSURE_HPA (1013.25)

// Об'єкт RTC (змінити на RTC_DS3231 для модуля DS3231)
RTC_DS1307 rtc;

// Об'єкт OLED-дисплея (128x64, апаратний I2C)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Об'єкт датчика BMP280 (підключення по I2C)
Adafruit_BMP280 bmp;

// Назви днів тижня. Індекс 0 = неділя (саме так їх нумерує RTClib у dayOfTheWeek())
const char daysOfTheWeek[7][20] = {
    "Неділя", "Понеділок", "Вівторок", "Середа",
    "Четвер", "П'ятниця", "Субота"};

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

// Ініціалізація серійного порту (для виводу повідомлень у монітор)
void initSerial()
{
  Serial.begin(115200);
}

// Ініціалізація шини I2C на заданих пінах SDA/SCL
void initI2C()
{
  Wire.begin(I2C_SDA, I2C_SCL);
}

// Сканує шину I2C (адреси 1-126) і виводить у Serial адреси знайдених пристроїв
void scanI2CBus()
{
  Serial.println("Сканування шини I2C...");

  int devicesFound = 0;
  for (uint8_t address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("Знайдено пристрій за адресою 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
      devicesFound++;
    }
  }

  if (devicesFound == 0)
    Serial.println("Жодного пристрою на шині I2C не знайдено");
  else
  {
    Serial.print(devicesFound);
    Serial.println(" пристрій(-ів) знайдено");
  }
}

// Реєстр ID чипа у BME280/BMP280 (адреса регістра однакова для обох)
#define BOSCH_CHIP_ID_REGISTER 0xD0

// Читає ID чипа напряму через Wire (в обхід Adafruit_BME280), щоб визначити,
// який чип реально розпаяно на платі за заданою адресою
void readChipID(uint8_t address)
{
  Wire.beginTransmission(address);
  Wire.write(BOSCH_CHIP_ID_REGISTER);
  uint8_t error = Wire.endTransmission(false); // restart, шину не відпускаємо

  if (error != 0)
  {
    Serial.print("Не вдалося прочитати ID чипа за адресою 0x");
    Serial.println(address, HEX);
    return;
  }

  Wire.requestFrom(address, (uint8_t)1);
  if (!Wire.available())
  {
    Serial.println("Чип не відповів на запит ID");
    return;
  }

  uint8_t chipId = Wire.read();
  Serial.print("ID чипа за адресою 0x");
  Serial.print(address, HEX);
  Serial.print(": 0x");
  Serial.print(chipId, HEX);
  Serial.print(" -> ");

  switch (chipId)
  {
  case 0x60:
    Serial.println("BME280 (температура + тиск + вологість)");
    break;
  case 0x58:
    Serial.println("BMP280 (лише температура + тиск, без вологості)");
    break;
  case 0x56:
  case 0x57:
    Serial.println("BMP280 (інженерний зразок)");
    break;
  case 0x61:
    Serial.println("BME680 (температура + тиск + вологість + гази)");
    break;
  default:
    Serial.println("невідомий чип");
    break;
  }
}

// Ініціалізація модуля годинника реального часу (RTC).
// Якщо RTC не знайдено — виконання зупиняється (щоб не працювати з невалідним часом)
void initRTC()
{
  if (!rtc.begin())
  {
    Serial.println("RTC not found");
    while (1)
      delay(10);
  }

  if (!rtc.isrunning())
  {
    Serial.println("RTC is NOT running, let's set the time!");
    // Якщо модуль щойно підключили або в ньому сіла батарейка резервного живлення,
    // виставляємо час компіляції цього скетчу
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // Час можна також задати вручну, наприклад:
    // rtc.adjust(DateTime(2026, 7, 29, 18, 0, 0));
  }
}

// Ініціалізація OLED-дисплея
void initDisplay()
{
  u8g2.begin();
}

// Ініціалізація датчика BMP280.
// Якщо датчик не знайдено на шині — виконання зупиняється
void initBMP280()
{
  if (!bmp.begin(BMP280_I2C_ADDRESS))
  {
    Serial.println("BMP280 not found");
    while (1)
      delay(10);
  }
}

// Зчитує поточний час з RTC
DateTime readCurrentTime()
{
  return rtc.now();
}

// Зчитує поточні показання температури та тиску з BMP280
SensorData readSensorData()
{
  SensorData data;
  data.temperature = bmp.readTemperature();
  data.pressure = bmp.readPressure() / 100.0F; // переведення з Па у гПа
  return data;
}

// Формує рядок дати у форматі ДД.ММ.РРРР
String formatDate(const DateTime &now)
{
  return twoDigits(now.day()) + "." + twoDigits(now.month()) + "." + String(now.year());
}

// Формує рядок часу у форматі ГГ:ХХ:СС
String formatTime(const DateTime &now)
{
  return twoDigits(now.hour()) + ":" + twoDigits(now.minute()) + ":" + twoDigits(now.second());
}

// Повертає назву дня тижня українською для заданого моменту часу
String formatDayOfWeek(const DateTime &now)
{
  return daysOfTheWeek[now.dayOfTheWeek()];
}

// Виводить день тижня, дату, час і показання датчика на OLED-дисплей
void updateDisplay(const DateTime &now, const SensorData &sensorData)
{
  u8g2.clearBuffer();                      // Очищення буфера дисплея перед малюванням нового кадру

  u8g2.setFont(u8g2_font_6x13B_t_cyrillic); 
  int timeWidth = u8g2.getUTF8Width(formatTime(now).c_str());
  int screenWidth = u8g2.getDisplayWidth();
  int dateX = (screenWidth - timeWidth) / 2; // Центрування часу по горизонталі
  u8g2.drawUTF8(dateX, 14, formatTime(now).c_str());

  u8g2.setFont(u8g2_font_6x12_t_cyrillic);
  u8g2.drawUTF8(5, 30, formatDayOfWeek(now).c_str()); // drawUTF8 щоб кириличні символи виводились коректно
  u8g2.drawUTF8(62, 30, formatDate(now).c_str());
  u8g2.drawUTF8(5, 45, ("Temp: " + String(sensorData.temperature, 1) + " C").c_str());
  u8g2.drawUTF8(5, 60, ("Press: " + String(sensorData.pressure, 1) + " hPa").c_str());

  u8g2.sendBuffer(); // Передача заповненого буфера на дисплей
}

void setup()
{
  initSerial();
  initI2C();
  scanI2CBus();
  readChipID(BMP280_I2C_ADDRESS);
  initRTC();
  initDisplay();
  initBMP280();
  

  Serial.println();
}

void loop()
{
  DateTime now = readCurrentTime();
  SensorData sensorData = readSensorData();

  Serial.print("Current time: ");
  Serial.print(formatDayOfWeek(now));
  Serial.print(", ");
  Serial.print(formatDate(now));
  Serial.print(" ");
  Serial.println(formatTime(now));

  updateDisplay(now, sensorData);

  delay(1000);
}
