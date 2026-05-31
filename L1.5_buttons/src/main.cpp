#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define LED_PIN1 15
// #define POT_PIN 10
// #define SENSOR_PIN 11
// #define BOOT_BTN_PIN 0
#define BUTTON_PIN 7

int buttonState = 0;
int ledState = 0;

int lastDebounceTime = 0;
int debounceDelay = 50;
    
Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(LED_PIN1, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);
}

void loop() {

    int reading = digitalRead(BUTTON_PIN);
    Serial.print("Raw button reading: ");
    Serial.println(reading);

    if (reading != buttonState) {
        lastDebounceTime = millis();
        buttonState = reading;
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        ledState = !ledState;
    }

    Serial.print("Button state: ");
    Serial.println(buttonState);

    Serial.print("LED state: ");
    Serial.println(ledState);

    digitalWrite(LED_PIN1, ledState);

    // delay(10);

    Serial.println("----");

}
