#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define LED_PIN1 2
#define LED_PIN2 21
#define LED_PIN3 45
#define DELAY_TIME 200

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void ledOn(int ledPin) {
    digitalWrite(ledPin, HIGH);
}

void switchLEDs() {
    digitalWrite(LED_PIN1, !digitalRead(LED_PIN1));
    digitalWrite(LED_PIN2, !digitalRead(LED_PIN2));
    digitalWrite(LED_PIN3, !digitalRead(LED_PIN3));
}

void blinkTimes(int times) {
    for (int i = 0; i < times; i++) {
        ledOn(LED_PIN1);
        ledOn(LED_PIN2);
        ledOn(LED_PIN3);
        delay(DELAY_TIME);

        switchLEDs();
        delay(DELAY_TIME);
    }
}

void setup() {
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(LED_PIN1, OUTPUT);
    pinMode(LED_PIN2, OUTPUT);
    pinMode(LED_PIN3, OUTPUT);
}

void loop() {
    ledOn(LED_PIN1);
    delay(DELAY_TIME);

    ledOn(LED_PIN2);
    delay(DELAY_TIME);

    ledOn(LED_PIN3);
    delay(DELAY_TIME);

    switchLEDs();
    delay(DELAY_TIME);

    blinkTimes(3);
    Serial.println("Loop done...");

}
