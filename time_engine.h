#ifndef TIME_ENGINE_H
#define TIME_ENGINE_H

#include <stdint.h>

struct TimeContext {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

// Initializes the time engine using __TIME__ string (e.g. "17:46:32")
void initTimeEngine(const char* compileTimeStr);

// Calculates and returns the current time based on elapsed millis()
TimeContext getCurrentTime();

// For test mode: allows injecting a specific time directly
TimeContext createTimeContext(uint8_t h, uint8_t m, uint8_t s);

#endif
