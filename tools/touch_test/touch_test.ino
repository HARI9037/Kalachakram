#include <Arduino.h>

// Match these to the current Kalachakram touch configuration.
const uint8_t TOUCH_TEST_PIN = 2;
const bool TOUCH_TEST_ACTIVE_HIGH = true;

const unsigned long RAW_PRINT_INTERVAL = 100UL;

int previousRawState = LOW;
unsigned long lastRawPrintTime = 0;

void setup() {
    Serial.begin(9600);
    pinMode(TOUCH_TEST_PIN, INPUT);

    previousRawState = digitalRead(TOUCH_TEST_PIN);

    Serial.println(F("KALACHAKRAM TOUCH SENSOR TEST"));
    Serial.print(F("PIN: D"));
    Serial.println(TOUCH_TEST_PIN);
    Serial.print(F("EXPECTED ACTIVE LEVEL: "));
    Serial.println(TOUCH_TEST_ACTIVE_HIGH ? F("HIGH") : F("LOW"));
    Serial.println(F("Observe untouched, touched, and released values."));
    Serial.println();
}

void loop() {
    unsigned long currentMillis = millis();
    int rawState = digitalRead(TOUCH_TEST_PIN);

    if (rawState != previousRawState) {
        Serial.print(F("EDGE: "));
        Serial.print(previousRawState);
        Serial.print(F(" -> "));
        Serial.println(rawState);

        bool active = TOUCH_TEST_ACTIVE_HIGH
            ? rawState == HIGH
            : rawState == LOW;
        if (active) {
            Serial.println(F("TOUCH EVENT"));
        }

        previousRawState = rawState;
    }

    if (currentMillis - lastRawPrintTime >= RAW_PRINT_INTERVAL) {
        lastRawPrintTime = currentMillis;
        Serial.print(F("RAW TOUCH: "));
        Serial.println(rawState);
    }
}
