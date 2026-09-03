#include <Arduino.h>
#include "time_engine.h"
#include "vibe_engine.h"
#include "messages.h"
#include "display_controller.h"
#include "touch_controller.h"

// Define to 1 to enable test mode, 0 for normal mode.
// The guard also allows the build command to override this value.
#ifndef KALACHAKRAM_TEST_MODE
#define KALACHAKRAM_TEST_MODE 0
#endif

unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 1000; // 1 second

unsigned long lastMessageTime = 0;
const unsigned long MESSAGE_INTERVAL = 60000UL; // 1 minute
const unsigned long TOUCH_FLIRT_DISPLAY_MS = 10000UL; // 10 seconds
bool hasNormalMessage = false;
bool flirtOverlayActive = false;
unsigned long flirtOverlayStartTime = 0;
VibeCategory currentVibe = CURSED_HOURS;
ContextPhase currentPhase = PHASE_EARLY;
Message cachedNormalMessage;

void runTests();
void printTimeContext(const TimeContext& ctx);

enum RuntimeAction {
    ACTION_NONE,
    ACTION_SELECT_NORMAL,
    ACTION_SELECT_TOUCH,
    ACTION_RESTORE_NORMAL
};

static RuntimeAction getRuntimeAction(
    unsigned long currentMillis,
    unsigned long previousSelectionMillis,
    unsigned long overlayStartMillis,
    bool hasCachedNormal,
    bool overlayActive,
    bool contextChanged,
    bool touchRequested
) {
    if (!hasCachedNormal || contextChanged) return ACTION_SELECT_NORMAL;
    if (touchRequested) return ACTION_SELECT_TOUCH;
    if (overlayActive) {
        return currentMillis - overlayStartMillis >= TOUCH_FLIRT_DISPLAY_MS
            ? ACTION_RESTORE_NORMAL
            : ACTION_NONE;
    }
    return currentMillis - previousSelectionMillis >= MESSAGE_INTERVAL
        ? ACTION_SELECT_NORMAL
        : ACTION_NONE;
}

void setup() {
    Serial.begin(9600);
    while (!Serial) { ; } // wait for serial port to connect
    
    Serial.println(F("================================"));
    Serial.println(F("KALACHAKRAM"));
    Serial.println(F("V0.6.2 - ENGLISH PERSONALITY"));
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
    initTouchSensor();
#endif
}

void loop() {
#if !KALACHAKRAM_TEST_MODE
    unsigned long currentMillis = millis();
    
    TimeContext current = getCurrentTime();
    VibeCategory vibe = classifyVibe(current);
    ContextPhase phase = classifyContextPhase(current, vibe);
    uint8_t contextMinute = getMinuteInContextPhase(current, vibe, phase);

    bool contextChanged = hasNormalMessage &&
        (vibe != currentVibe || phase != currentPhase);
    bool touchRequested = wasTouchPressed(currentMillis);

    RuntimeAction action = getRuntimeAction(
        currentMillis,
        lastMessageTime,
        flirtOverlayStartTime,
        hasNormalMessage,
        flirtOverlayActive,
        contextChanged,
        touchRequested
    );

    if (action == ACTION_SELECT_NORMAL || action == ACTION_SELECT_TOUCH) {
        MessageTrigger trigger = TRIGGER_TOUCH;
        if (action == ACTION_SELECT_NORMAL) {
            if (!hasNormalMessage) trigger = TRIGGER_STARTUP;
            else if (contextChanged) trigger = TRIGGER_CONTEXT;
            else trigger = TRIGGER_TIMER;
        }

        Message msg;
        uint16_t combinationIndex;
        selectMessage(
            vibe,
            phase,
            trigger,
            contextMinute,
            &msg,
            &combinationIndex
        );

        if (action == ACTION_SELECT_NORMAL) {
            cachedNormalMessage = msg;
            currentVibe = vibe;
            currentPhase = phase;
            hasNormalMessage = true;
            flirtOverlayActive = false;
        } else {
            flirtOverlayActive = true;
            flirtOverlayStartTime = currentMillis;
        }

        // A normal selection or accepted touch starts a fresh 60-second window.
        // Restoring the cached message does not touch this timestamp.
        lastMessageTime = currentMillis;
        
        Serial.println(F("================================"));
        Serial.print(F("TRIGGER: "));
        Serial.println(getMessageTriggerName(trigger));
        Serial.print(F("PERSONALITY: "));
        Serial.println(getMessagePersonalityName(getMessagePersonality(trigger)));
        Serial.print(F("TIME: "));
        printTimeContext(current);
        Serial.print(F("\nVIBE: "));
        Serial.println(getVibeName(vibe));
        Serial.print(F("PHASE: "));
        Serial.println(getContextPhaseName(phase));
        Serial.print(F("COMBINATION: "));
        Serial.println(combinationIndex);
        Serial.println(F("\nMESSAGE:"));
        Serial.println(msg.line1);
        Serial.println(msg.line2);
        Serial.println(F("================================\n"));

        displayMessage(msg);
    } else if (action == ACTION_RESTORE_NORMAL) {
        flirtOverlayActive = false;
        Serial.println(F("OVERLAY: RESTORE NORMAL"));
        Serial.println(cachedNormalMessage.line1);
        Serial.println(cachedNormalMessage.line2);
        displayMessage(cachedNormalMessage);
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

void runSingleTest(const TestCase& t, int& passCount, int& failCount);
void runSinglePhaseTest(
    const PhaseTestCase& test,
    int& passCount,
    int& failCount
);

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
    
    Serial.println(F("=== COMPOSITION VALIDATION TESTS ==="));
    int validAutoCount = validateMessages(PERSONALITY_NORMAL);
    Serial.print(F("AUTO: "));
    Serial.print(validAutoCount);
    Serial.println(F(" / 2340 combinations valid"));
    if (validAutoCount != 2340) msgFail++;
    else msgPass++;

    int validTouchCount = validateMessages(PERSONALITY_FLIRTY);
    Serial.print(F("TOUCH: "));
    Serial.print(validTouchCount);
    Serial.println(F(" / 1200 combinations valid"));
    if (validTouchCount != 1200) msgFail++;
    else msgPass++;
    
    int totalAutoCount = countMessages(PERSONALITY_NORMAL);
    int totalTouchCount = countMessages(PERSONALITY_FLIRTY);
    Serial.print(F("AUTO combinations: "));
    Serial.println(totalAutoCount);
    Serial.print(F("TOUCH combinations: "));
    Serial.println(totalTouchCount);
    if (totalAutoCount != 2340 || totalTouchCount != 1200) msgFail++;
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

    Serial.println(F("\n=== REFRESH SCHEDULER TESTS ==="));
    int schedulerFailures = 0;
    unsigned long selectionTime = 1000UL;

    if (getRuntimeAction(60999UL, selectionTime, 0UL,
                         true, false, false, false) != ACTION_NONE) {
        schedulerFailures++;
    }
    if (getRuntimeAction(61000UL, selectionTime, 0UL,
                         true, false, false, false) != ACTION_SELECT_NORMAL) {
        schedulerFailures++;
    }
    if (getRuntimeAction(1001UL, selectionTime, 0UL,
                         true, false, true, false) != ACTION_SELECT_NORMAL) {
        schedulerFailures++;
    }
    if (getRuntimeAction(0UL, 0UL, 0UL,
                         false, false, false, false) != ACTION_SELECT_NORMAL) {
        schedulerFailures++;
    }
    if (getRuntimeAction(1001UL, selectionTime, 0UL,
                         true, false, false, true) != ACTION_SELECT_TOUCH) {
        schedulerFailures++;
    }

    // A1 at t=0, T1 at t=25s, restore A1 at t=35s, then A2 at t=85s.
    if (getRuntimeAction(34999UL, 25000UL, 25000UL,
                         true, true, false, false) != ACTION_NONE) {
        schedulerFailures++;
    }
    if (getRuntimeAction(35000UL, 25000UL, 25000UL,
                         true, true, false, false) != ACTION_RESTORE_NORMAL) {
        schedulerFailures++;
    }
    if (getRuntimeAction(84999UL, 25000UL, 25000UL,
                         true, false, false, false) != ACTION_NONE) {
        schedulerFailures++;
    }
    if (getRuntimeAction(85000UL, 25000UL, 25000UL,
                         true, false, false, false) != ACTION_SELECT_NORMAL) {
        schedulerFailures++;
    }

    // Re-touch takes priority over expiry and restarts both timers.
    if (getRuntimeAction(30000UL, 25000UL, 25000UL,
                         true, true, false, true) != ACTION_SELECT_TOUCH) {
        schedulerFailures++;
    }
    if (getRuntimeAction(40000UL, 30000UL, 30000UL,
                         true, true, false, false) != ACTION_RESTORE_NORMAL) {
        schedulerFailures++;
    }
    if (getRuntimeAction(90000UL, 30000UL, 30000UL,
                         true, false, false, false) != ACTION_SELECT_NORMAL) {
        schedulerFailures++;
    }

    // A context transition always cancels the old-context flirt overlay.
    if (getRuntimeAction(30000UL, 25000UL, 25000UL,
                         true, true, true, true) != ACTION_SELECT_NORMAL) {
        schedulerFailures++;
    }

    if (getMessagePersonality(TRIGGER_STARTUP) != PERSONALITY_NORMAL ||
        getMessagePersonality(TRIGGER_TIMER) != PERSONALITY_NORMAL ||
        getMessagePersonality(TRIGGER_CONTEXT) != PERSONALITY_NORMAL ||
        getMessagePersonality(TRIGGER_TOUCH) != PERSONALITY_FLIRTY) {
        schedulerFailures++;
    }

    unsigned long rolloverSelectionTime = 0xFFFFFF00UL;
    unsigned long rolloverRefreshTime = rolloverSelectionTime + MESSAGE_INTERVAL;
    if (getRuntimeAction(rolloverRefreshTime, rolloverSelectionTime, 0UL,
                         true, false, false, false) != ACTION_SELECT_NORMAL) {
        schedulerFailures++;
    }
    unsigned long rolloverFlirtTime = 0xFFFFF000UL;
    unsigned long rolloverRestoreTime =
        rolloverFlirtTime + TOUCH_FLIRT_DISPLAY_MS;
    if (getRuntimeAction(rolloverRestoreTime, rolloverFlirtTime,
                         rolloverFlirtTime, true, true, false, false) !=
        ACTION_RESTORE_NORMAL) {
        schedulerFailures++;
    }

    Serial.print(F("Scheduler failures: "));
    Serial.println(schedulerFailures);
    if (schedulerFailures > 0) msgFail++;
    else msgPass++;
    
    Serial.println(F("\n=== V0.6.2 RESULT ==="));
    Serial.print(msgPass);
    Serial.println(F(" PASSED"));
    Serial.print(msgFail);
    Serial.println(F(" FAILED"));
    Serial.println();
}
#endif
