#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define INTERVAL 500
#define LED_COUNT 3
const int LED_PINS[] = {15, 9, 10};

#define BUTTON_PIN 8
#define BOUNCE 200

bool ascSortOrder = true;
unsigned long lastLedToggleTime = 0;
int lastChangedLedIndex = 0;

bool lastButtonState = LOW;
bool stableButtonState = LOW;
unsigned long lastBounce = 0;

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void offAllLeds()
{
    for (int i = 0; i < 3; i++)
    {
        digitalWrite(LED_PINS[i], LOW);
    }
}

void setup()
{
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    for (int i = 0; i < LED_COUNT; i++)
    {
        pinMode(LED_PINS[i], OUTPUT);
    }
    pinMode(BUTTON_PIN, INPUT);  // button has pull-down resistor, so no need to set it up here
}

void loop()
{
    unsigned long now = millis();

    if (now - lastLedToggleTime >= INTERVAL){
        
        offAllLeds();

        if (ascSortOrder) {
            if (lastChangedLedIndex == LED_COUNT - 1) {
                lastChangedLedIndex = 0;
            } else {
                lastChangedLedIndex++;
            }
        } else {
            if (lastChangedLedIndex == 0) {
                lastChangedLedIndex = LED_COUNT - 1;
            } else {
                lastChangedLedIndex--;
            }
        }
        digitalWrite(LED_PINS[lastChangedLedIndex], HIGH);
        Serial.printf("LED %d is ON\n", lastChangedLedIndex);
        lastLedToggleTime = now;
    }


    bool current = digitalRead(BUTTON_PIN);

    if (current != lastButtonState) {
        lastBounce = now;
    }
    lastButtonState = current;

    if (now - lastBounce >= BOUNCE && current != stableButtonState) {
        stableButtonState = current;
        if (stableButtonState == LOW) {  // button released
            ascSortOrder = !ascSortOrder;
            Serial.println("Button released, order changed");
        }
    }
}
