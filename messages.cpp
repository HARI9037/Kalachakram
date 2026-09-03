#include "messages.h"
#include <Arduino.h>
#include <avr/pgmspace.h>

static const uint8_t VIBE_COUNT = 8;
static const uint8_t PHASE_COUNT = 3;
static const uint8_t REACTION_COUNT = 10;
static const uint8_t MAX_POOL_BYTES = 15; // 120 combinations / 8

struct ContextPoolConfig {
    uint16_t line1Offset;
    uint8_t line1Count;
    uint8_t durationMinutes;
};

// Every setup line is globally unique. Sharing the compatible reaction bank
// therefore cannot create duplicate complete messages across contexts.
const char contextLine1Fragments[][17] PROGMEM = {
    // CURSED_HOURS / EARLY (12)
    "THIS HOUR",
    "NOTHING GOOD NOW",
    "MIDNIGHT PASSED.",
    "NIGHT JUST BEGAN",
    "BAD IDEAS AWAKE",
    "CLOCK SAW THAT",
    "SLEEP LEFT EARLY",
    "DARKNESS IS IN",
    "DAWN IS NOWHERE",
    "CHOSE MIDNIGHT",
    "NIGHT HAS NOTES",
    "TIME GOT CURSED",

    // CURSED_HOURS / MIDDLE (12)
    "WHY STILL AWAKE?",
    "SLEEP IS A THING",
    "STILL NIGHT.",
    "CLOCK IS AWAKE",
    "NIGHT WON'T END",
    "DREAMS FILED OFF",
    "PILLOW WAITING",
    "2AM QUESTIONS",
    "REST MODE FAILED",
    "MOON IS JUDGING",
    "LATE GOT LATER",
    "MIND IS ONLINE",

    // CURSED_HOURS / LATE (12)
    "GO TO BED.",
    "LOG OFF.",
    "DAWN IS CLOSE.",
    "MORNING SOON.",
    "NIGHT SHIFT LOST",
    "SLEEP DEADLINE",
    "SUN WARMING UP",
    "REGRET INBOUND",
    "LAST DARK HOUR",
    "BED STILL EXISTS",
    "DAWN FOUND YOU",
    "NIGHT IS EXPIRED",

    // TOO_EARLY / EARLY (10)
    "TOO EARLY.",
    "EARLY MISTAKES",
    "DAWN ARRIVED.",
    "RUDE DAWN ENTRY",
    "COFFEE NOT FOUND",
    "SUN HIT START",
    "ALARM WON AGAIN",
    "MORNING ATTACKS",
    "EYES HALF CLOSED",
    "WAKE MODE WAITS",

    // TOO_EARLY / MIDDLE (10)
    "MORNING?",
    "COFFEE FIRST.",
    "MORNING EXISTS.",
    "COFFEE WINDOW.",
    "BRAIN BOOTING",
    "SUN IS TOO LOUD",
    "WAKEUP BUFFERING",
    "DAY NEEDS COFFEE",
    "EYELIDS BARGAIN",
    "ALARM AFTERMATH",

    // TOO_EARLY / LATE (10)
    "SUN IS UP.",
    "BARELY AWAKE.",
    "ALMOST AWAKE.",
    "EARLY IS ENDING.",
    "COFFEE WORKED?",
    "BRAIN HAS SIGNAL",
    "MORNING CALMED",
    "EYES ONLINE NOW",
    "DAWN NEARLY DONE",
    "DAY MODE LOADING",

    // MORNING / EARLY (10)
    "DAY STARTED.",
    "MORNING-ISH.",
    "MORNING IS HERE.",
    "EMAILS AWAKEN.",
    "WORKDAY BOOTING",
    "INBOX HAS RISEN",
    "TASKS SAY HELLO",
    "FOCUS REQUESTED",
    "DESK MODE ACTIVE",
    "TODAY STARTED",

    // MORNING / MIDDLE (10)
    "PRODUCTIVITY",
    "DOING THINGS?",
    "DAY IN PROGRESS.",
    "TASKS EXIST.",
    "WORK LOOKS REAL",
    "INBOX WANTS MORE",
    "FOCUS OPTIONAL",
    "MEETING DETECTED",
    "BUSY LOOK READY",
    "DEADLINES HOVER",

    // MORNING / LATE (10)
    "PRETEND TO WORK",
    "STILL MORNING.",
    "NOON IS NEARBY.",
    "MORNING ENDING.",
    "WORK FACE ON",
    "LUNCH THOUGHTS",
    "FOCUS SIGNED OUT",
    "INBOX IS HUNGRY",
    "NOON EYES YOU",
    "TASKS, THEN FOOD",

    // LUNCH_LOADING / EARLY (8)
    "LUNCH IS COMING",
    "FOOD SOON.",
    "HUNGER ONLINE.",
    "LUNCH DETECTED.",
    "SNACK RADAR ON",
    "STOMACH ONLINE",
    "FOCUS GETS SHAKY",
    "NOON FEELS CLOSE",

    // LUNCH_LOADING / MIDDLE (8)
    "WORK CAN WAIT.",
    "FOCUS FADING.",
    "FOOD THOUGHTS.",
    "CLOCK SAYS SNACK",
    "HUNGER HAS PLANS",
    "LUNCH TAB OPEN",
    "BRAIN WANTS FOOD",
    "NOON IS TEASING",

    // LUNCH_LOADING / LATE (8)
    "PRE-LUNCH STAGE.",
    "ESSENTIALLY NOON",
    "ALMOST LUNCH.",
    "WORK FOCUS LEFT",
    "FORKS ON STANDBY",
    "SNACKS FEEL NEAR",
    "WORK TASTES ODD",
    "HUNGER WON THIS",

    // AFTERNOON / EARLY (10)
    "DEFINITELY PM",
    "LUNCH IS GONE.",
    "LUNCH AFTERMATH.",
    "DAY CONTINUES.",
    "POST-LUNCH MODE",
    "ENERGY ATE LUNCH",
    "DESK FEELS HEAVY",
    "NOON LEFT CRUMBS",
    "WORK RESUMED?",
    "NAP IDEA FOUND",

    // AFTERNOON / MIDDLE (10)
    "WORK ENERGY LOW",
    "LONG SLUMP BEGUN",
    "AFTERNOON HUM.",
    "BATTERY IS SAD",
    "ENERGY LEFT?",
    "CLOCK MOVES SLOW",
    "DESK GRAVITY",
    "NAP TAB OPEN",
    "FOCUS TOOK LEAVE",
    "PM FEELS STICKY",

    // AFTERNOON / LATE (10)
    "STILL HERE?",
    "AFTERNOON GLITCH",
    "EVENING LOADING.",
    "WORKDAY FADING.",
    "CLOCK DRAGS FEET",
    "ENERGY SAID BYE",
    "SUN LOOKS TIRED",
    "TASKS LOSE GRIP",
    "ESCAPE IS CLOSE",
    "DESK TIME WILTS",

    // DAY_IS_DYING / EARLY (8)
    "DAY RUNNING OUT",
    "WORK DRIVE LEFT",
    "DAY LOSING GRIP",
    "AFTERNOON EXIT.",
    "SUN CHECKED OUT",
    "WORKDAY CRACKING",
    "CLOCK LEANS HOME",
    "DAYLIGHT RESIGNS",

    // DAY_IS_DYING / MIDDLE (8)
    "ALMOST EVENING.",
    "FINISH UP.",
    "EVENING NEARBY.",
    "SUN DONE WORKING",
    "DEADLINES BLINK",
    "WORK MASK SLIPS",
    "HOME MODE CALLS",
    "DAY PACKS A BAG",

    // DAY_IS_DYING / LATE (8)
    "BASICALLY SIX.",
    "SUNSET INCOMING.",
    "CLOCK SAYS WRAP",
    "DAY ALMOST OVER.",
    "WORKDAY EXHALES",
    "SUN CLOCKS OUT",
    "EVENING AT DOOR",
    "TASKS CAN WAIT",

    // EVENING / EARLY (10)
    "EVENING-ISH.",
    "DAY IS OVER.",
    "EVENING STARTED.",
    "DAY IS ENDING.",
    "WORK MODE FADING",
    "FREE TIME BOOTS",
    "SUNSET ARRIVED",
    "PLANS LOOK REAL",
    "CLOCK GOT SOFT",
    "NIGHT KNOCKS",

    // EVENING / MIDDLE (10)
    "WORK?",
    "FREE TIME.",
    "DINNER MAYBE.",
    "FREE TIME FOUND.",
    "PLANS PENDING",
    "COUCH MODE ON",
    "MISS ME ALREADY?",
    "STILL STARING?",
    "CLOCK DATE?",
    "EVENING FLIRTS",

    // EVENING / LATE (10)
    "NIGHT MODE",
    "RELAX.",
    "EVENING'S GONE.",
    "NIGHT HAS IDEAS",
    "COUCH WON AGAIN",
    "PLANS GOT SLEEPY",
    "MOON TOOK OVER",
    "FREE TIME FADING",
    "LATE LOOKS GOOD",
    "BEDROOM CALLING",

    // GO_TO_BED / EARLY (10)
    "TOMORROW IS NEAR",
    "LATE ENOUGH.",
    "BEDTIME NEARBY.",
    "NIGHT STARTED.",
    "BEDTIME WINDOW",
    "PILLOW SENT PING",
    "YAWN MODE ACTIVE",
    "TOMORROW KNOCKS",
    "NIGHT LIGHTS DIM",
    "REST IS TRENDING",

    // GO_TO_BED / MIDDLE (10)
    "WHY ARE YOU HERE",
    "SLEEP EXISTS.",
    "SLEEP WAITS",
    "TOMORROW WAITS.",
    "BED ASKS WHY",
    "EYELIDS VOTING",
    "NIGHT GETS LATE",
    "PILLOW IS ONLINE",
    "DREAMS WANT YOU",
    "CLOCK WANTS REST",

    // GO_TO_BED / LATE (10)
    "GO TO BED NOW",
    "DAY IS EXPIRED.",
    "MIDNIGHT NEARS.",
    "LAST CALL.",
    "BED DEADLINE",
    "TOMORROW LOOMS",
    "NIGHT WON'T WAIT",
    "PILLOW CALLS",
    "CLOCK HAS HAD IT",
    "LATE GOT ABSURD"
};

// Standalone reactions are intentionally compatible with every setup above.
const char reactionLine2Fragments[][17] PROGMEM = {
    "APPARENTLY.",
    "QUESTIONABLE.",
    "I NOTICED.",
    "BOLD CHOICE.",
    "HOW ADORABLE.",
    "TIME DISAGREES.",
    "REMEMBER?",
    "VERY DRAMATIC.",
    "THAT TRACKS.",
    "DON'T ARGUE."
};

// {line1 offset, line1 count, phase duration in minutes}
const ContextPoolConfig contextPools[8][3] PROGMEM = {
    { // CURSED_HOURS
        {0,   12, 100}, // EARLY
        {12,  12, 100}, // MIDDLE
        {24,  12, 100}  // LATE
    },
    { // TOO_EARLY
        {36,  10, 60}, // EARLY
        {46,  10, 60}, // MIDDLE
        {56,  10, 60}  // LATE
    },
    { // MORNING
        {66,  10, 60}, // EARLY
        {76,  10, 60}, // MIDDLE
        {86,  10, 60}  // LATE
    },
    { // LUNCH_LOADING
        {96,   8, 40}, // EARLY
        {104,  8, 40}, // MIDDLE
        {112,  8, 40}  // LATE
    },
    { // AFTERNOON
        {120, 10, 60}, // EARLY
        {130, 10, 60}, // MIDDLE
        {140, 10, 60}  // LATE
    },
    { // DAY_IS_DYING
        {150,  8, 40}, // EARLY
        {158,  8, 40}, // MIDDLE
        {166,  8, 40}  // LATE
    },
    { // EVENING
        {174, 10, 60}, // EARLY
        {184, 10, 60}, // MIDDLE
        {194, 10, 60}  // LATE
    },
    { // GO_TO_BED
        {204, 10, 60}, // EARLY
        {214, 10, 60}, // MIDDLE
        {224, 10, 60}  // LATE
    }
};

// Retaining one counter per pool prevents an arbitrary mid-phase boot from
// replaying that starting pool when it is revisited just before 24 hours.
static uint16_t poolSelectionCounters[VIBE_COUNT * PHASE_COUNT] = {0};
static uint32_t initializedPoolMask = 0;

static ContextPoolConfig readPoolConfig(
    VibeCategory vibe,
    ContextPhase phase
) {
    ContextPoolConfig config;
    memcpy_P(&config, &contextPools[vibe][phase], sizeof(config));
    return config;
}

static uint8_t getPermutationMultiplier(uint16_t capacity) {
    if (capacity == 120U) return 43U;
    if (capacity == 100U) return 37U;
    return 27U; // capacity 80
}

static uint16_t getPermutationOffset(
    VibeCategory vibe,
    ContextPhase phase,
    uint16_t capacity
) {
    uint8_t poolIndex = ((uint8_t)vibe * PHASE_COUNT) + (uint8_t)phase;
    return ((uint16_t)poolIndex * 17U + 11U) % capacity;
}

static uint8_t getPoolIndex(VibeCategory vibe, ContextPhase phase) {
    return ((uint8_t)vibe * PHASE_COUNT) + (uint8_t)phase;
}

static uint16_t permuteCombinationIndex(
    VibeCategory vibe,
    ContextPhase phase,
    uint16_t counter,
    uint16_t capacity
) {
    uint16_t multiplier = getPermutationMultiplier(capacity);
    uint16_t offset = getPermutationOffset(vibe, phase, capacity);
    return ((uint32_t)multiplier * counter + offset) % capacity;
}

static void initializePoolIfNeeded(
    VibeCategory vibe,
    ContextPhase phase,
    uint8_t contextMinute
) {
    uint8_t poolIndex = getPoolIndex(vibe, phase);
    uint32_t poolBit = 1UL << poolIndex;
    if ((initializedPoolMask & poolBit) != 0) return;

    uint16_t capacity = getMessageCapacity(vibe, phase);
    poolSelectionCounters[poolIndex] = contextMinute % capacity;
    initializedPoolMask |= poolBit;
}

static uint16_t selectCombinationIndex(
    VibeCategory vibe,
    ContextPhase phase,
    uint8_t contextMinute
) {
    initializePoolIfNeeded(vibe, phase, contextMinute);
    uint8_t poolIndex = getPoolIndex(vibe, phase);
    uint16_t capacity = getMessageCapacity(vibe, phase);
    uint16_t combinationIndex = permuteCombinationIndex(
        vibe,
        phase,
        poolSelectionCounters[poolIndex],
        capacity
    );

    poolSelectionCounters[poolIndex]++;
    if (poolSelectionCounters[poolIndex] >= capacity) {
        poolSelectionCounters[poolIndex] = 0;
    }

    return combinationIndex;
}

void selectMessage(
    VibeCategory vibe,
    ContextPhase phase,
    uint8_t contextMinute,
    Message* output,
    uint16_t* combinationIndex
) {
    ContextPoolConfig config = readPoolConfig(vibe, phase);
    uint16_t selected = selectCombinationIndex(vibe, phase, contextMinute);
    uint8_t line1Index = selected % config.line1Count;
    uint8_t line2Index = selected / config.line1Count;
    uint16_t globalLine1Index = config.line1Offset + line1Index;

    strcpy_P(output->line1, contextLine1Fragments[globalLine1Index]);
    strcpy_P(output->line2, reactionLine2Fragments[line2Index]);
    output->line1[16] = '\0';
    output->line2[16] = '\0';

    if (combinationIndex != NULL) {
        *combinationIndex = selected;
    }
}

uint16_t getMessageCapacity(VibeCategory vibe, ContextPhase phase) {
    ContextPoolConfig config = readPoolConfig(vibe, phase);
    return (uint16_t)config.line1Count * REACTION_COUNT;
}

uint8_t getMessageContextDuration(VibeCategory vibe, ContextPhase phase) {
    return readPoolConfig(vibe, phase).durationMinutes;
}

int countMessages() {
    int total = 0;
    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            total += getMessageCapacity(
                (VibeCategory)vibe,
                (ContextPhase)phase
            );
        }
    }
    return total;
}

int validateMessages() {
    int validCombinations = 0;

    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            ContextPoolConfig config = readPoolConfig(
                (VibeCategory)vibe,
                (ContextPhase)phase
            );
            uint8_t validLine1Count = 0;
            uint8_t validLine2Count = 0;

            for (uint8_t line = 0; line < config.line1Count; line++) {
                uint8_t length = strlen_P(
                    contextLine1Fragments[config.line1Offset + line]
                );
                if (length > 0 && length <= 16) validLine1Count++;
            }

            for (uint8_t line = 0; line < REACTION_COUNT; line++) {
                uint8_t length = strlen_P(reactionLine2Fragments[line]);
                if (length > 0 && length <= 16) validLine2Count++;
            }

            validCombinations +=
                (int)validLine1Count * validLine2Count;
        }
    }

    return validCombinations;
}

static void saveSelectionState(
    uint16_t savedCounters[VIBE_COUNT * PHASE_COUNT],
    uint32_t& savedMask
) {
    memcpy(savedCounters, poolSelectionCounters, sizeof(poolSelectionCounters));
    savedMask = initializedPoolMask;
}

static void restoreSelectionState(
    const uint16_t savedCounters[VIBE_COUNT * PHASE_COUNT],
    uint32_t savedMask
) {
    memcpy(poolSelectionCounters, savedCounters, sizeof(poolSelectionCounters));
    initializedPoolMask = savedMask;
}

int testRepetition() {
    int failures = 0;
    uint16_t savedCounters[VIBE_COUNT * PHASE_COUNT];
    uint32_t savedMask;
    saveSelectionState(savedCounters, savedMask);

    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            uint8_t seen[MAX_POOL_BYTES] = {0};
            VibeCategory testVibe = (VibeCategory)vibe;
            ContextPhase testPhase = (ContextPhase)phase;
            uint16_t capacity = getMessageCapacity(testVibe, testPhase);
            uint8_t poolIndex = getPoolIndex(testVibe, testPhase);
            poolSelectionCounters[poolIndex] = 0;
            initializedPoolMask |= 1UL << poolIndex;

            for (uint16_t selection = 0; selection < capacity; selection++) {
                uint16_t index = selectCombinationIndex(
                    testVibe,
                    testPhase,
                    0
                );
                uint8_t byteIndex = index / 8U;
                uint8_t bit = 1U << (index % 8U);

                if ((seen[byteIndex] & bit) != 0) failures++;
                seen[byteIndex] |= bit;
            }
        }
    }

    restoreSelectionState(savedCounters, savedMask);
    return failures;
}

int testMessageCycle() {
    int failures = 0;
    uint16_t savedCounters[VIBE_COUNT * PHASE_COUNT];
    uint32_t savedMask;
    saveSelectionState(savedCounters, savedMask);

    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            VibeCategory testVibe = (VibeCategory)vibe;
            ContextPhase testPhase = (ContextPhase)phase;
            uint16_t capacity = getMessageCapacity(testVibe, testPhase);
            uint8_t duration = getMessageContextDuration(testVibe, testPhase);

            if (capacity < duration) failures++;

            Message message;
            uint16_t firstIndex;
            uint16_t secondIndex;
            uint8_t poolIndex = getPoolIndex(testVibe, testPhase);
            initializedPoolMask &= ~(1UL << poolIndex);
            selectMessage(testVibe, testPhase, 0, &message, &firstIndex);
            selectMessage(testVibe, testPhase, 0, &message, &secondIndex);

            if (firstIndex == secondIndex) failures++;
            if (message.line1[16] != '\0' || message.line2[16] != '\0') {
                failures++;
            }
        }
    }

    uint8_t anchoredPool = getPoolIndex(EVENING, PHASE_MIDDLE);
    initializedPoolMask &= ~(1UL << anchoredPool);
    uint16_t anchoredIndex;
    Message anchoredMessage;
    selectMessage(
        EVENING,
        PHASE_MIDDLE,
        7,
        &anchoredMessage,
        &anchoredIndex
    );
    uint16_t expected = permuteCombinationIndex(
        EVENING,
        PHASE_MIDDLE,
        7,
        getMessageCapacity(EVENING, PHASE_MIDDLE)
    );
    if (anchoredIndex != expected) failures++;

    uint16_t otherIndex;
    Message otherMessage;
    uint8_t otherPool = getPoolIndex(GO_TO_BED, PHASE_LATE);
    initializedPoolMask &= ~(1UL << otherPool);
    selectMessage(
        GO_TO_BED,
        PHASE_LATE,
        0,
        &otherMessage,
        &otherIndex
    );

    uint16_t resumedIndex;
    selectMessage(
        EVENING,
        PHASE_MIDDLE,
        0,
        &anchoredMessage,
        &resumedIndex
    );
    uint16_t expectedResumed = permuteCombinationIndex(
        EVENING,
        PHASE_MIDDLE,
        8,
        getMessageCapacity(EVENING, PHASE_MIDDLE)
    );
    if (resumedIndex != expectedResumed) failures++;

    restoreSelectionState(savedCounters, savedMask);
    return failures;
}
