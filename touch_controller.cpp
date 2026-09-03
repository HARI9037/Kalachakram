#include "touch_controller.h"
#include <Arduino.h>

static const uint32_t TOUCH_DEBOUNCE_INTERVAL = 50UL;

static bool rawTouched = false;
static bool stableTouched = false;
static uint32_t lastRawChangeTime = 0;

static bool readTouchState() {
#if KALACHAKRAM_TOUCH_ACTIVE_HIGH
    return digitalRead(KALACHAKRAM_TOUCH_PIN) == HIGH;
#else
    return digitalRead(KALACHAKRAM_TOUCH_PIN) == LOW;
#endif
}

void initTouchSensor() {
    pinMode(KALACHAKRAM_TOUCH_PIN, INPUT);

    bool initialState = readTouchState();
    rawTouched = initialState;
    stableTouched = initialState;
    lastRawChangeTime = millis();
}

bool wasTouchPressed(uint32_t currentMillis) {
    bool currentRawState = readTouchState();

    if (currentRawState != rawTouched) {
        rawTouched = currentRawState;
        lastRawChangeTime = currentMillis;
    }

    if (
        rawTouched != stableTouched &&
        currentMillis - lastRawChangeTime >= TOUCH_DEBOUNCE_INTERVAL
    ) {
        stableTouched = rawTouched;
        return stableTouched;
    }

    return false;
}
