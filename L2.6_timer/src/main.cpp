#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "esp_task_wdt.h"

#define RGB_PIN 48
#define LED_PIN 4
#define BTN_PIN 8

hw_timer_t * timerCounter = NULL;
hw_timer_t * timerLED = NULL;
volatile bool ledState = false;
volatile unsigned long count = 0;

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void IRAM_ATTR ledOnISR() {
    ledState = !ledState;
}

void IRAM_ATTR counterISR(){
    static bool lastState = LOW;
    bool currentState = digitalRead(BTN_PIN);
    if (currentState == HIGH && lastState == LOW) {  // тільки перехід LOW→HIGH
        count++;
    }
    lastState = currentState;
}


void setup()
{
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_PIN, INPUT_PULLDOWN); // button with pulldown

    timerLED = timerBegin(0, 80, true);
    timerAttachInterrupt(timerLED, &ledOnISR, true);
    timerAlarmWrite(timerLED, 500000, true);
    timerAlarmEnable(timerLED);

    timerCounter = timerBegin(1, 80, true);
    timerAttachInterrupt(timerCounter, &counterISR, true);
    timerAlarmWrite(timerCounter, 1000, true);
    timerAlarmEnable(timerCounter);

    esp_task_wdt_init(5, true);
    esp_task_wdt_add(NULL);

    Serial.println("Starting...");
}

void loop()
{
    esp_task_wdt_reset();
    if (ledState){
        ledState = false;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 2000){
        uint32_t snapshot = 0;
        noInterrupts();
        snapshot = count;
        interrupts();
        Serial.printf("Count: %d \n", snapshot);
        lastPrint = millis();
    }

    if (count > 10){while(true){}}

}
