#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define RELAY_OUTPUT_PIN 8
#define RELAY_INPUT_PIN 11

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    pinMode(RELAY_OUTPUT_PIN, OUTPUT);
    pinMode(RELAY_INPUT_PIN, INPUT_PULLUP);
}

void loop()
{
    
    unsigned long send_command_time = 0;
    unsigned long finished_command_time = 0;
    unsigned long process_times[10];
    unsigned long sum_process_time = 0;

    for (int i = 0; i < 10; i++) {
        send_command_time = millis();
        digitalWrite(RELAY_OUTPUT_PIN, HIGH);
        while (digitalRead(RELAY_INPUT_PIN)) {
            // wait for relay to close
        }
        finished_command_time = millis();
        process_times[i] = finished_command_time - send_command_time;
        sum_process_time += process_times[i];
        Serial.print("Relay switched on in ");
        Serial.print(process_times[i]);
        Serial.println(" ms");

        delay(1000);
        digitalWrite(RELAY_OUTPUT_PIN, LOW);
        delay(2000);
    }

    Serial.print("Average: ");
    Serial.print(sum_process_time / 10);
    Serial.println(" ms");
    
}
