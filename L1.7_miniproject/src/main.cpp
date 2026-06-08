#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define LED_PIN1 15
#define LED_PIN2 16
#define TDR_PIN 18
#define RELE_OUTPUT_PIN 11

#define TDR_THRESHOLD 2000
#define TIME_INTERVAL 200

unsigned long now = 0;
unsigned long lastUpdate = 0;
bool isLed1On = false;

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(TDR_PIN, INPUT);
    pinMode(LED_PIN1, OUTPUT);
    pinMode(LED_PIN2, OUTPUT);
    pinMode(RELE_OUTPUT_PIN, OUTPUT);

    digitalWrite(RELE_OUTPUT_PIN, HIGH);
}

void loop() {
    int tdrValue = analogRead(TDR_PIN);
    now = millis();

    if (tdrValue > TDR_THRESHOLD) { // якщо TDR значення перевищує поріг, вимкнути реле та увімкнути світлодіоди по черзі
        digitalWrite(RELE_OUTPUT_PIN, LOW);  // вимкнути реле 
        
        if (now - lastUpdate > TIME_INTERVAL) { // перевіряємо, чи минуло достатньо часу для оновлення стану світлодіодів
            if (!isLed1On) {
                digitalWrite(LED_PIN1, HIGH);
                digitalWrite(LED_PIN2, LOW);
                isLed1On = true;
            } else {
                digitalWrite(LED_PIN1, LOW);
                digitalWrite(LED_PIN2, HIGH);
                isLed1On = false;
            }

            lastUpdate = now;
        }
    } else { // якщо TDR значення не перевищує поріг, увімкнути реле та обидва світлодіоди
        digitalWrite(RELE_OUTPUT_PIN, HIGH);
        digitalWrite(LED_PIN1, LOW);
        digitalWrite(LED_PIN2, LOW);
    }

}
