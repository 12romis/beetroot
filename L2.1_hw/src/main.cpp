#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
constexpr int BUTTON_PIN = 8;

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

enum class LedState {
    OFF,
    ON
};

class LedConfig {
    public:
        static constexpr int LED_PIN = 15;
        static constexpr int LED2_PIN = 9;
        static constexpr int BLINK_INTERVAL = 500;
        static constexpr int SHORT_PRESS_BLINKS = 3; // todo
        static constexpr int LONG_PRESS_BLINKS = 5; // todo
};

class Led {
    private:
        uint8_t pin;
        LedState state;
        unsigned long lastToggleTime = 0;

    public:
        Led(uint8_t pin) : pin(pin), state(LedState::OFF) {}

        void init() {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
        }

        void setState(LedState state) {
            this->state = state;
            digitalWrite(pin, state == LedState::ON ? HIGH : LOW);
        }

        void toggle() {
            setState(state == LedState::ON ? LedState::OFF : LedState::ON);
        }
        
        LedState getState() const {
            return state;
        }

        unsigned long getLastToggleTime() const {
            return lastToggleTime;
        }

        void updateLastToggleTime(unsigned long time) {
            lastToggleTime = time;
        }
};   
Led led(LedConfig::LED_PIN);


enum class Led2Mode {
    OFF,
    ON,
    BLINK,
    MODES_COUNT = 3,
};

class Led2{
    private:
        uint8_t pin;
        Led2Mode mode;

    public:
        Led2(uint8_t pin) : pin(pin), mode(Led2Mode::OFF) {}

        void init() {
            pinMode(pin, OUTPUT);
            update();
        }

        void nextMode() {
            int next = (static_cast<int>(mode) + 1) % static_cast<int>(Led2Mode::MODES_COUNT);
            setMode(static_cast<Led2Mode>(next));
        }

        void setMode(Led2Mode mode) {
            this->mode = mode;
            update();
        }
            
        Led2Mode getMode() const {
            return mode;
        }

        void update() {
            switch (mode) {
                case Led2Mode::OFF:
                    digitalWrite(pin, LOW);
                    break;
                case Led2Mode::ON:
                    digitalWrite(pin, HIGH);
                    break;
                case Led2Mode::BLINK:
                    digitalWrite(pin, millis() % LedConfig::BLINK_INTERVAL < LedConfig::BLINK_INTERVAL / 2 ? HIGH : LOW);
                    break;
            }
        }
};
Led2 led2(LedConfig::LED2_PIN);

class Measurement {
    private:
        unsigned long num_measurements;
        unsigned long processing_time;
        static constexpr int PRINT_EVERY_MEASUREMENTS = 1000000;

    public:
        Measurement(unsigned long num_measurements, unsigned long processing_time) : 
        num_measurements(0), processing_time(0) {}

        unsigned long getNumMeasurements() const {
            return num_measurements;
        }

        unsigned long getProcessingTime() const {
            return processing_time;
        }

        void addMeasurement(unsigned long time) {
            num_measurements++;
            processing_time += time;

            if (num_measurements % PRINT_EVERY_MEASUREMENTS == 0) {
                Serial.print("Processed ");
                Serial.print(num_measurements);
                Serial.print(" measurements in ");
                Serial.print(processing_time / 1000.0, 2);
                Serial.println(" s");
            }
        }
};
Measurement measurement(0,0);

class Button {
    private:
        uint8_t pin;
        volatile bool is_pressed;
        static Button* instance; // static instance pointer
        static constexpr int BOUNCE = 500; // debounce time in milliseconds
        unsigned long lastBounce;
        static portMUX_TYPE mux;

    public:
        Button(uint8_t pin) : pin(pin), is_pressed(false), lastBounce(0) {}

        void init() {
            instance = this; // set the static instance pointer to this object
            pinMode(pin, INPUT);  // button has pull-down resistor
            attachInterrupt(digitalPinToInterrupt(pin), onInterrupt, RISING);
        }

        static void onInterrupt() {
            portENTER_CRITICAL_ISR(&mux);  // enter critical section to safely access shared data
            instance->is_pressed = true;
            portEXIT_CRITICAL_ISR(&mux);
        }

        void handlePress() {
            portENTER_CRITICAL(&mux);
            bool pressed = is_pressed;
            if (pressed) is_pressed = false;
            portEXIT_CRITICAL(&mux);

            if (pressed && (millis() - lastBounce) > BOUNCE) {
                lastBounce = millis();
                led2.nextMode();
                Serial.println("Button pressed, Led2 mode changed");
            }
        }
};
Button* Button::instance = nullptr;
Button btn(BUTTON_PIN);
portMUX_TYPE Button::mux = portMUX_INITIALIZER_UNLOCKED;


void setup()
{
    Serial.begin(115200);
    rgb.begin();
    rgb.setPixelColor(0, 0, 0, 0);
    rgb.show();

    led.init();
    led2.init();
    btn.init();
}

void loop()
{
    unsigned long loop_start = millis();

    if (loop_start - led.getLastToggleTime() >= LedConfig::BLINK_INTERVAL) {
        led.toggle();
        Serial.print("LED state is ");
        Serial.println(led.getState() == LedState::ON ? "ON" : "OFF");
        led.updateLastToggleTime(loop_start);
    }

    btn.handlePress();
    led2.update();
    measurement.addMeasurement(millis() - loop_start);
}
