#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define LED_PIN1 15
#define LED_PIN2 17
#define BOOT_BTN_PIN 0
#define BUTTON_PIN 7

unsigned long lastBootBtnDebounce = 0;
unsigned long lastExtBtnDebounce = 0;
const int debounceDelay = 50;
bool bootBtnPressInitialized = false;
bool extBtnPressInitialized = false;
const int longPressDuration = 1000; 

// void (*blinkMode)() = nullptr; 
int blinkMode = 1;
unsigned long lastBlinkTime = 0;
const unsigned long blinkInterval = 250; 

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(LED_PIN1, OUTPUT);
    pinMode(LED_PIN2, OUTPUT);
    pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN, INPUT);

    bootBtnPressInitialized = false;
    extBtnPressInitialized = false;
}

void loop() {
    int curr_bootBtnState = digitalRead(BOOT_BTN_PIN);
    if (!curr_bootBtnState) {
        if (!bootBtnPressInitialized) {
            lastBootBtnDebounce = millis();
            bootBtnPressInitialized = true;
        } 
    } else {
        if (bootBtnPressInitialized && (millis() - lastBootBtnDebounce) > debounceDelay) {
            blinkMode = 1;
            Serial.println("blinkMode1 activated");
        }
        bootBtnPressInitialized = false; // кнопку відпустили — скидаємо
    }
    

    int curr_extBtnState = digitalRead(BUTTON_PIN);
    if (curr_extBtnState) {
        if (!extBtnPressInitialized) {
            lastExtBtnDebounce = millis();
            extBtnPressInitialized = true;
        }
    }
    if (!curr_extBtnState) {
        if (extBtnPressInitialized && (millis() - lastExtBtnDebounce) > longPressDuration) {
            blinkMode = 3;
            Serial.println("blinkMode3 activated");
        } else if (extBtnPressInitialized && (millis() - lastExtBtnDebounce) > debounceDelay) {
            blinkMode = 2;
            Serial.println("blinkMode2 activated");
        } 
        extBtnPressInitialized = false; // кнопку відпустили — скидаємо
    }

    unsigned long now_ms = millis();

    if (blinkMode == 1) {
        if (now_ms - lastBlinkTime >= blinkInterval) {
            lastBlinkTime = now_ms;
            int led1State = digitalRead(LED_PIN1);
            digitalWrite(LED_PIN1, !led1State);
            digitalWrite(LED_PIN2, !led1State);
        }
    } else if (blinkMode == 2) {
        if (now_ms - lastBlinkTime >= blinkInterval) {
            lastBlinkTime = now_ms;
            int led1State = digitalRead(LED_PIN1);
            digitalWrite(LED_PIN1, !led1State);
            digitalWrite(LED_PIN2, led1State);
        }
    } else if (blinkMode == 3) {
        if (now_ms - lastBlinkTime >= blinkInterval/2) {
            lastBlinkTime = now_ms;
            int led1State = digitalRead(LED_PIN1);
            digitalWrite(LED_PIN1, !led1State);
            digitalWrite(LED_PIN2, led1State);
        }
    }

}
