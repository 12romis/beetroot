#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define LED_PIN1 15
#define POT_PIN 10
#define SENSOR_PIN 11
#define BOOT_BTN_PIN 0
#define BUTTON_PIN 7
    
Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void ledOn(int ledPin) {
    digitalWrite(ledPin, HIGH);
}

void ledOff(int ledPin) {
    digitalWrite(ledPin, LOW);
}

void setup() {
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(LED_PIN1, OUTPUT);
    pinMode(POT_PIN, INPUT);
    pinMode(SENSOR_PIN, INPUT_PULLDOWN);
    pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
    int bootBtnState = digitalRead(BOOT_BTN_PIN);
    Serial.print("Boot button state: ");
    Serial.print(bootBtnState);

    if (bootBtnState == HIGH)
    {
        rgb.setPixelColor(0, 255, 0, 0); // Red color
    } else {
        rgb.setPixelColor(0, 0, 255, 0); // Green color
    }
    
    int buttonState = digitalRead(BUTTON_PIN);
    Serial.print(" | Button state: ");
    Serial.println(buttonState);

    if (buttonState == HIGH)
    {
        rgb.setPixelColor(0, 0, 0, 255); // Blue color
    }
    rgb.show();
    
    int state = digitalRead(SENSOR_PIN);
    Serial.print("Sensor state: ");
    Serial.println(state);

    int potValue = analogRead(POT_PIN);
    Serial.print("Potentiometer value: ");
    Serial.println(potValue);
    int delayTime = map(potValue, 0, 4095, 100, 2000); // Map potentiometer value to delay time (100ms to 2000ms)
    Serial.print("Delay time: ");
    Serial.print(delayTime);
    Serial.println(" ms");

    ledOn(LED_PIN1);
    delay(delayTime);

    ledOff(LED_PIN1);
    delay(delayTime/2); // Shorter delay for off state

}
