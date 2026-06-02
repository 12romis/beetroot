#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define LED_PIN1 15
#define LDR_PIN 8

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(LDR_PIN, INPUT);
    pinMode(LED_PIN1, OUTPUT);
    analogReadResolution(12);
}

void loop() {
    int ldrValue = analogRead(LDR_PIN);
    float voltage = ldrValue * (3.3 / 4095.0);
    // int brightness = map(ldrValue, 0, 4095, 255, 0);
    int millivolts = analogReadMilliVolts(LDR_PIN);
    Serial.print("LDR Value: ");
    Serial.print(ldrValue);
    Serial.print(" Voltage: ");
    Serial.print(voltage);
    Serial.print(" Analog Volts: ");
    Serial.print(millivolts/1000.0);
    Serial.print(" +/-: ");
    Serial.println(voltage-millivolts/1000.0);
    // Serial.print(" Brightness: ");
    // Serial.println(brightness);



    // analogWrite(LED_PIN1, brightness);

    if (ldrValue < 1000) {
        rgb.setPixelColor(0, 255, 0, 0); // Red
    } else {
        rgb.setPixelColor(0, 0, 0, 0);
    }
    rgb.show();

    delay(100);
}
