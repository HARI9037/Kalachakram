#include <Arduino.h>
#include "time_engine.h"
#include "vibe_engine.h"
#include "messages.h"
#include "display_controller.h"

// Define to 1 to enable test mode, 0 for normal mode
#define KALACHAKRAM_TEST_MODE 0

unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 1000; // 1 second

unsigned long lastMessageTime = 0;
const unsigned long MESSAGE_INTERVAL = 60000UL; // 60 seconds
VibeCategory currentVibe = (VibeCategory)-1;

void runTests();
void printTimeContext(const TimeContext& ctx);

void setup() {
    Serial.begin(9600);
    while (!Serial) { ; } // wait for serial port to connect
    
    Serial.println(F("================================"));
    Serial.println(F("KALACHAKRAM"));
    Serial.println(F("V0.3 - EYES"));
    Serial.println(F("================================"));

#if KALACHAKRAM_TEST_MODE
    Serial.println(F("MODE: TEST"));
    Serial.println(F("RUNNING TESTS..."));
    Serial.println();
    

    runTests();
#else
    Serial.println(F("MODE: NORMAL"));
    Serial.println(F("TIME SOURCE: COMPILE TIME + MILLIS"));
    Serial.println();
    initTimeEngine(__TIME__);
    initDisplay();
#endif
}

void loop() {
#if !KALACHAKRAM_TEST_MODE
    unsigned long currentMillis = millis();
    
    TimeContext current = getCurrentTime();
    VibeCategory vibe = classifyVibe(current);
    
    bool vibeChanged = (vibe != currentVibe);
    bool timeExpired = (currentMillis - lastMessageTime >= MESSAGE_INTERVAL);
    
    // First time init
    if (currentVibe == (VibeCategory)-1) {
        vibeChanged = true;
    }
    
    if (vibeChanged || timeExpired) {
        currentVibe = vibe;
        lastMessageTime = currentMillis;
        
        Message msg;
        selectMessage(vibe, &msg);
        
        Serial.println(F("================================"));
        Serial.print(F("TIME: "));
        printTimeContext(current);
        Serial.print(F("\nVIBE: "));
        Serial.println(getVibeName(vibe));
        Serial.println(F("\nMESSAGE:"));
        Serial.println(msg.line1);
        Serial.println(msg.line2);
        Serial.println(F("================================\n"));

        displayMessage(msg);
    }
    
    // 1-second debug tick
    if (currentMillis - lastPrintTime >= PRINT_INTERVAL) {
        lastPrintTime = currentMillis;
        Serial.print(F("[DEBUG] TIME: "));
        printTimeContext(current);
        Serial.print(F(" | VIBE: "));
        Serial.println(getVibeName(vibe));
    }
#endif
}

void printTimeContext(const TimeContext& ctx) {
    if (ctx.hour < 10) Serial.print('0');
    Serial.print(ctx.hour);
    Serial.print(':');
    if (ctx.minute < 10) Serial.print('0');
    Serial.print(ctx.minute);
    Serial.print(':');
    if (ctx.second < 10) Serial.print('0');
    Serial.print(ctx.second);
}

#if KALACHAKRAM_TEST_MODE
struct TestCase {
    TimeContext time;
    VibeCategory expected;
};

void runSingleTest(const TestCase& t, int& passCount, int& failCount) {
    Serial.print(F("[TEST] "));
    printTimeContext(t.time);
    
    VibeCategory actual = classifyVibe(t.time);
    
    if (actual == t.expected) {
        Serial.print(F(" -> PASS"));
        passCount++;
    } else {
        Serial.print(F(" -> FAIL!"));
        failCount++;
    }
    
    Serial.print(F(" (Expected: "));
    Serial.print(getVibeName(t.expected));
    Serial.print(F(", Actual: "));
    Serial.print(getVibeName(actual));
    Serial.println(F(")"));
}

void runTests() {
    int passCount = 0;
    int failCount = 0;
    
    Serial.println(F("=== STANDARD CATEGORY TESTS ==="));
    TestCase stdTests[] = {
        { createTimeContext(2, 30, 0), CURSED_HOURS },
        { createTimeContext(6, 15, 0), TOO_EARLY },
        { createTimeContext(9, 0, 0), MORNING },
        { createTimeContext(11, 45, 0), LUNCH_LOADING },
        { createTimeContext(14, 30, 0), AFTERNOON },
        { createTimeContext(16, 45, 0), DAY_IS_DYING },
        { createTimeContext(19, 0, 0), EVENING },
        { createTimeContext(22, 30, 0), GO_TO_BED }
    };
    
    for (int i = 0; i < 8; i++) {
        runSingleTest(stdTests[i], passCount, failCount);
    }
    
    Serial.println(F("\n=== BOUNDARY TESTS ==="));
    TestCase boundaryTests[] = {
        { createTimeContext(4, 59, 59), CURSED_HOURS },
        { createTimeContext(5, 0, 0), TOO_EARLY },
        { createTimeContext(7, 59, 59), TOO_EARLY },
        { createTimeContext(8, 0, 0), MORNING },
        { createTimeContext(10, 59, 59), MORNING },
        { createTimeContext(11, 0, 0), LUNCH_LOADING },
        { createTimeContext(12, 59, 59), LUNCH_LOADING },
        { createTimeContext(13, 0, 0), AFTERNOON },
        { createTimeContext(15, 59, 59), AFTERNOON },
        { createTimeContext(16, 0, 0), DAY_IS_DYING },
        { createTimeContext(17, 59, 59), DAY_IS_DYING },
        { createTimeContext(18, 0, 0), EVENING },
        { createTimeContext(20, 59, 59), EVENING },
        { createTimeContext(21, 0, 0), GO_TO_BED },
        { createTimeContext(23, 59, 59), GO_TO_BED },
        { createTimeContext(0, 0, 0), CURSED_HOURS }
    };
    
    for (int i = 0; i < 16; i++) {
        runSingleTest(boundaryTests[i], passCount, failCount);
    }
    
    Serial.println(F("\n=== V0.1 RESULT ==="));
    Serial.print(passCount);
    Serial.println(F(" PASSED"));
    Serial.print(failCount);
    Serial.println(F(" FAILED"));
    Serial.println();
    
    int msgPass = 0;
    int msgFail = 0;
    
    Serial.println(F("=== MESSAGE VALIDATION TESTS ==="));
    int validCount = validateMessages();
    Serial.print(validCount);
    Serial.println(F(" / 48 message lengths valid"));
    if (validCount != 48) msgFail++;
    else msgPass++;
    
    int totalCount = countMessages();
    Serial.print(F("Total messages: "));
    Serial.println(totalCount);
    if (totalCount != 48) msgFail++;
    else msgPass++;
    
    Serial.println(F("\n=== REPEAT PREVENTION TESTS ==="));
    int repeats = testRepetition();
    Serial.print(F("Immediate repeats: "));
    Serial.println(repeats);
    if (repeats > 0) msgFail++;
    else msgPass++;
    
    Serial.println(F("\n=== V0.2 RESULT ==="));
    Serial.print(msgPass);
    Serial.println(F(" PASSED"));
    Serial.print(msgFail);
    Serial.println(F(" FAILED"));
    Serial.println();
}
#endif
