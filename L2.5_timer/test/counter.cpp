#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define LED_PIN 4
constexpr int BUTTON_PIN = 8;

bool toggleFlag = false;
constexpr int DEBOUNCE_DELAY_MS = 100; 
unsigned long lastInterruptTime = 0;
volatile int pulseCount = 0;
unsigned long lastReportTime = 0;

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void IRAM_ATTR handleButtonInterrupt() {
    unsigned long currentInterruptTime = millis();
    if (currentInterruptTime - lastInterruptTime > DEBOUNCE_DELAY_MS) {
        pulseCount++;
        lastInterruptTime = currentInterruptTime;
    }
}


void setup()
{
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, RISING);
    Serial.println("Starting...");
}

void loop()
{
    if (millis() - lastReportTime >= 1000) {
        int current = pulseCount;
        pulseCount = 0;

        if (current > 0) {
            Serial.printf("Pulses: %d \n", current);
        }

        lastReportTime = millis();
    }
    

}
