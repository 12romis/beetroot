#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define LED_PIN1 15

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    // pinMode(LED_PIN1, OUTPUT);

    ledcAttach(LED_PIN1, 5000, 8);
}

void loop()
{
    for (int duty = 0; duty <= 255; duty++)
    {
        ledcWrite(LED_PIN1, duty);
        delay(5);
    }
    for (int duty = 255; duty >= 0; duty--)
    {
        ledcWrite(LED_PIN1, duty);
        delay(5);
    }
}
