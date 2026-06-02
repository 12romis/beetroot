#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define LED_PIN1 15
#define LED_PIN2 17
#define BOOT_BTN_PIN 0
#define BUTTON_PIN 7

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

volatile int16_t counter = 0;
volatile bool btnPressed = false;

void IRAM_ATTR reaction() {
    counter++;
    btnPressed = true;
}

void setup() {
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(BUTTON_PIN, INPUT);

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), reaction, RISING);
}

void loop() {
    if (btnPressed) {
        btnPressed = false;
        Serial.println("Count: " + String(counter));
    }
    // Serial.print("HELLO");
    delay(250);
}
