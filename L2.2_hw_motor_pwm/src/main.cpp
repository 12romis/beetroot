#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN    48
#define MOTOR_PIN   8
#define POT_PIN    10

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    ledcAttach(MOTOR_PIN, 5000, 8);

    pinMode(POT_PIN, INPUT);
    analogReadResolution(10); // 10-bit: 0–1023
}

void loop()
{
    int pot_value = analogRead(POT_PIN);
    int pwm_value = map(pot_value, 0, 1023, 0, 255);

    ledcWrite(MOTOR_PIN, pwm_value);

    Serial.print("POT: ");
    Serial.print(pot_value);
    Serial.print("  PWM: ");
    Serial.println(pwm_value);

    delay(50); // ~20 оновлень/с
}
