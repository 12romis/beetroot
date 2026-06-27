#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

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
    // digitalWrite(LED_PIN, ledState);
}

void IRAM_ATTR counterISR(){
    if (digitalRead(BTN_PIN) == HIGH){
        count++;
    }
}

void setup()
{
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_PIN, INPUT); // button with pulldown

    timerLED = timerBegin(0, 80, true);
    timerAttachInterrupt(timerLED, &ledOnISR, true);
    timerAlarmWrite(timerLED, 500000, true);
    timerAlarmEnable(timerLED);

    timerCounter = timerBegin(1, 80, true);
    timerAttachInterrupt(timerCounter, &counterISR, true);
    timerAlarmWrite(timerCounter, 1000, true);
    timerAlarmEnable(timerCounter);

    Serial.println("Starting...");
}

void loop()
{
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

}
