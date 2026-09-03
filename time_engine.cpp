#include "time_engine.h"
#include <Arduino.h>

static uint32_t startSecondsSinceMidnight = 0;
static uint32_t startTimeMillis = 0;

void initTimeEngine(const char* compileTimeStr) {
    // compileTimeStr format is "HH:MM:SS"
    if (compileTimeStr && compileTimeStr[2] == ':' && compileTimeStr[5] == ':') {
        uint8_t h = (compileTimeStr[0] - '0') * 10 + (compileTimeStr[1] - '0');
        uint8_t m = (compileTimeStr[3] - '0') * 10 + (compileTimeStr[4] - '0');
        uint8_t s = (compileTimeStr[6] - '0') * 10 + (compileTimeStr[7] - '0');
        
        startSecondsSinceMidnight = (h * 3600UL) + (m * 60UL) + s;
    } else {
        startSecondsSinceMidnight = 0;
    }
    
    startTimeMillis = millis();
}

TimeContext getCurrentTime() {
    uint32_t currentMillis = millis();
    uint32_t elapsedMillis = currentMillis - startTimeMillis; // safe unsigned subtraction
    uint32_t elapsedSeconds = elapsedMillis / 1000;
    
    uint32_t currentSecondsSinceMidnight = startSecondsSinceMidnight + elapsedSeconds;
    
    // Wrap around 24 hours (86400 seconds)
    currentSecondsSinceMidnight = currentSecondsSinceMidnight % 86400UL;
    
    TimeContext ctx;
    ctx.hour = (currentSecondsSinceMidnight / 3600) % 24;
    ctx.minute = (currentSecondsSinceMidnight % 3600) / 60;
    ctx.second = currentSecondsSinceMidnight % 60;
    
    return ctx;
}

TimeContext createTimeContext(uint8_t h, uint8_t m, uint8_t s) {
    TimeContext ctx;
    ctx.hour = h;
    ctx.minute = m;
    ctx.second = s;
    return ctx;
}
