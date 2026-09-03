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
const unsigned long MESSAGE_INTERVAL = 180000UL; // 3 minutes
VibeCategory currentVibe = (VibeCategory)-1;
ContextPhase currentPhase = (ContextPhase)-1;

void runTests();
void printTimeContext(const TimeContext& ctx);

void setup() {
    Serial.begin(9600);
    while (!Serial) { ; } // wait for serial port to connect
    
    Serial.println(F("================================"));
    Serial.println(F("KALACHAKRAM"));
    Serial.println(F("V0.4 - CONTEXT DEPTH"));
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
    ContextPhase phase = classifyContextPhase(current, vibe);
    
    bool vibeChanged = (vibe != currentVibe);
    bool phaseChanged = (phase != currentPhase);
    bool timeExpired = (currentMillis - lastMessageTime >= MESSAGE_INTERVAL);
    
    // First time init
    if (currentVibe == (VibeCategory)-1) {
        vibeChanged = true;
    }
    
    if (vibeChanged || phaseChanged || timeExpired) {
        currentVibe = vibe;
        currentPhase = phase;
        lastMessageTime = currentMillis;
        
        Message msg;
        selectMessage(vibe, phase, &msg);
        
        Serial.println(F("================================"));
        Serial.print(F("TIME: "));
        printTimeContext(current);
        Serial.print(F("\nVIBE: "));
        Serial.println(getVibeName(vibe));
        Serial.print(F("PHASE: "));
        Serial.println(getContextPhaseName(phase));
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
        Serial.print(getVibeName(vibe));
        Serial.print(F(" | PHASE: "));
        Serial.println(getContextPhaseName(phase));
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

struct PhaseTestCase {
    TimeContext time;
    VibeCategory vibe;
    ContextPhase expected;
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

void runSinglePhaseTest(
    const PhaseTestCase& test,
    int& passCount,
    int& failCount
) {
    Serial.print(F("[PHASE TEST] "));
    printTimeContext(test.time);

    ContextPhase actual = classifyContextPhase(test.time, test.vibe);

    if (actual == test.expected) {
        Serial.print(F(" -> PASS"));
        passCount++;
    } else {
        Serial.print(F(" -> FAIL!"));
        failCount++;
    }

    Serial.print(F(" (Expected: "));
    Serial.print(getContextPhaseName(test.expected));
    Serial.print(F(", Actual: "));
    Serial.print(getContextPhaseName(actual));
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

    Serial.println(F("\n=== CONTEXT PHASE BOUNDARY TESTS ==="));
    PhaseTestCase phaseTests[] = {
        { createTimeContext(0, 0, 0), CURSED_HOURS, PHASE_EARLY },
        { createTimeContext(1, 39, 59), CURSED_HOURS, PHASE_EARLY },
        { createTimeContext(1, 40, 0), CURSED_HOURS, PHASE_MIDDLE },
        { createTimeContext(3, 19, 59), CURSED_HOURS, PHASE_MIDDLE },
        { createTimeContext(3, 20, 0), CURSED_HOURS, PHASE_LATE },
        { createTimeContext(4, 59, 59), CURSED_HOURS, PHASE_LATE },

        { createTimeContext(5, 0, 0), TOO_EARLY, PHASE_EARLY },
        { createTimeContext(5, 59, 59), TOO_EARLY, PHASE_EARLY },
        { createTimeContext(6, 0, 0), TOO_EARLY, PHASE_MIDDLE },
        { createTimeContext(6, 59, 59), TOO_EARLY, PHASE_MIDDLE },
        { createTimeContext(7, 0, 0), TOO_EARLY, PHASE_LATE },
        { createTimeContext(7, 59, 59), TOO_EARLY, PHASE_LATE },

        { createTimeContext(8, 0, 0), MORNING, PHASE_EARLY },
        { createTimeContext(8, 59, 59), MORNING, PHASE_EARLY },
        { createTimeContext(9, 0, 0), MORNING, PHASE_MIDDLE },
        { createTimeContext(9, 59, 59), MORNING, PHASE_MIDDLE },
        { createTimeContext(10, 0, 0), MORNING, PHASE_LATE },
        { createTimeContext(10, 59, 59), MORNING, PHASE_LATE },

        { createTimeContext(11, 0, 0), LUNCH_LOADING, PHASE_EARLY },
        { createTimeContext(11, 39, 59), LUNCH_LOADING, PHASE_EARLY },
        { createTimeContext(11, 40, 0), LUNCH_LOADING, PHASE_MIDDLE },
        { createTimeContext(12, 19, 59), LUNCH_LOADING, PHASE_MIDDLE },
        { createTimeContext(12, 20, 0), LUNCH_LOADING, PHASE_LATE },
        { createTimeContext(12, 59, 59), LUNCH_LOADING, PHASE_LATE },

        { createTimeContext(13, 0, 0), AFTERNOON, PHASE_EARLY },
        { createTimeContext(13, 59, 59), AFTERNOON, PHASE_EARLY },
        { createTimeContext(14, 0, 0), AFTERNOON, PHASE_MIDDLE },
        { createTimeContext(14, 59, 59), AFTERNOON, PHASE_MIDDLE },
        { createTimeContext(15, 0, 0), AFTERNOON, PHASE_LATE },
        { createTimeContext(15, 59, 59), AFTERNOON, PHASE_LATE },

        { createTimeContext(16, 0, 0), DAY_IS_DYING, PHASE_EARLY },
        { createTimeContext(16, 39, 59), DAY_IS_DYING, PHASE_EARLY },
        { createTimeContext(16, 40, 0), DAY_IS_DYING, PHASE_MIDDLE },
        { createTimeContext(17, 19, 59), DAY_IS_DYING, PHASE_MIDDLE },
        { createTimeContext(17, 20, 0), DAY_IS_DYING, PHASE_LATE },
        { createTimeContext(17, 59, 59), DAY_IS_DYING, PHASE_LATE },

        { createTimeContext(18, 0, 0), EVENING, PHASE_EARLY },
        { createTimeContext(18, 59, 59), EVENING, PHASE_EARLY },
        { createTimeContext(19, 0, 0), EVENING, PHASE_MIDDLE },
        { createTimeContext(19, 59, 59), EVENING, PHASE_MIDDLE },
        { createTimeContext(20, 0, 0), EVENING, PHASE_LATE },
        { createTimeContext(20, 59, 59), EVENING, PHASE_LATE },

        { createTimeContext(21, 0, 0), GO_TO_BED, PHASE_EARLY },
        { createTimeContext(21, 59, 59), GO_TO_BED, PHASE_EARLY },
        { createTimeContext(22, 0, 0), GO_TO_BED, PHASE_MIDDLE },
        { createTimeContext(22, 59, 59), GO_TO_BED, PHASE_MIDDLE },
        { createTimeContext(23, 0, 0), GO_TO_BED, PHASE_LATE },
        { createTimeContext(23, 59, 59), GO_TO_BED, PHASE_LATE }
    };

    for (int i = 0; i < 48; i++) {
        runSinglePhaseTest(phaseTests[i], passCount, failCount);
    }
    
    Serial.println(F("\n=== CLASSIFICATION RESULT ==="));
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
    Serial.println(F(" / 96 messages valid"));
    if (validCount != 96) msgFail++;
    else msgPass++;
    
    int totalCount = countMessages();
    Serial.print(F("Total messages: "));
    Serial.println(totalCount);
    if (totalCount != 96) msgFail++;
    else msgPass++;
    
    Serial.println(F("\n=== REPEAT PREVENTION TESTS ==="));
    int repeats = testRepetition();
    Serial.print(F("Immediate repeats: "));
    Serial.println(repeats);
    if (repeats > 0) msgFail++;
    else msgPass++;

    Serial.println(F("\n=== MESSAGE CYCLE TESTS ==="));
    int cycleFailures = testMessageCycle();
    Serial.print(F("Cycle failures: "));
    Serial.println(cycleFailures);
    if (cycleFailures > 0) msgFail++;
    else msgPass++;
    
    Serial.println(F("\n=== V0.4 RESULT ==="));
    Serial.print(msgPass);
    Serial.println(F(" PASSED"));
    Serial.print(msgFail);
    Serial.println(F(" FAILED"));
    Serial.println();
}
#endif
