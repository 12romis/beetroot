#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

#define LED_PIN   4
#define BTN_PIN   8

#define POLL_MS     10
#define DEBOUNCE_MS 20

unsigned int count = 0;

enum BtnState { IDLE, DEBOUNCE_PRESS, PRESSED, DEBOUNCE_RELEASE };

void setup()
{
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_PIN, INPUT); // pulldown

    Serial.println("Starting...");
}

void loop()
{
    unsigned long now = millis();

    static unsigned long lastPollTime = 0;
    if (now - lastPollTime >= POLL_MS) {
        lastPollTime = now;

        static BtnState state = IDLE;
        static unsigned long stateEnterTime = 0;
        bool high = digitalRead(BTN_PIN) == HIGH;

        switch (state) {
            case IDLE:
                if (high) {
                    state = DEBOUNCE_PRESS;
                    stateEnterTime = now;
                }
                break;

            case DEBOUNCE_PRESS:
                if (!high) {
                    state = IDLE;                           // шум — повертаємось
                } else if (now - stateEnterTime >= DEBOUNCE_MS) {
                    state = PRESSED;
                    count++;                               // підтверджене натискання
                }
                break;

            case PRESSED:
                if (!high) {
                    state = DEBOUNCE_RELEASE;
                    stateEnterTime = now;
                }
                break;

            case DEBOUNCE_RELEASE:
                if (high) {
                    state = PRESSED;                       // шум — кнопка ще тримається
                } else if (now - stateEnterTime >= DEBOUNCE_MS) {
                    state = IDLE;
                }
                break;
        }
    }

    static unsigned long lastPrintTime = 0;
    if (now - lastPrintTime > 3000) {
        Serial.printf("Count: %d \n", count);
        lastPrintTime = now;
    }
}
