#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

#define LED_PIN 4
#define RELAY_PIN 8

hw_timer_t * timer = NULL;

constexpr int TICK_PERIOD = 4; // minutes
constexpr int ON_TICK = 1; // munutes

volatile int tick = -1;
volatile bool motorState = false;


void IRAM_ATTR onTimer() {
    tick = (tick + 1) % TICK_PERIOD;
    motorState = tick < ON_TICK;
}

void setup()
{
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(LED_PIN, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);

    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000*1000*60, true);  // run every 1 minute
    timerAlarmEnable(timer);

    Serial.println("Starting...");
}

void loop()
{
    digitalWrite(LED_PIN, motorState);
    digitalWrite(RELAY_PIN, motorState);
}
