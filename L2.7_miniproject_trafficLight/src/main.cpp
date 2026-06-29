#include <Arduino.h>

constexpr uint8_t PIN_RED    = 2;
constexpr uint8_t PIN_YELLOW = 3;
constexpr uint8_t PIN_GREEN  = 4;

void setLights(bool red, bool yellow, bool green) {
    digitalWrite(PIN_RED,    red    ? HIGH : LOW);
    digitalWrite(PIN_YELLOW, yellow ? HIGH : LOW);
    digitalWrite(PIN_GREEN,  green  ? HIGH : LOW);
    // Serial.printf("[%6lu s] RED:%s  YELLOW:%s  GREEN:%s\n",
    //     millis() / 1000,
    //     red    ? "ON " : "off",
    //     yellow ? "ON " : "off",
    //     green  ? "ON " : "off");
}

void setup() {
    pinMode(PIN_RED,    OUTPUT);
    pinMode(PIN_YELLOW, OUTPUT);
    pinMode(PIN_GREEN,  OUTPUT);
    // Serial.begin(115200);
    setLights(false, false, false);
}

void loop() {
    // Phase 1: Red — 10s
    setLights(true, false, false);
    delay(10000);

    // Phase 2: Red + Yellow — 2s
    setLights(true, true, false);
    delay(2000);

    // Phase 3: Green — 10s
    setLights(false, false, true);
    delay(10000);

    for (int i = 0; i < 10; i++)
    {
        setLights(false, false, false);
        delay(500);
        setLights(false, false, true);
        delay(500);
    }

    // Phase 4: Yellow — 2s
    setLights(false, true, false);
    delay(2000);
}
