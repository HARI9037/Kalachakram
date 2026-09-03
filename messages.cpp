#include "messages.h"
#include <Arduino.h>
#include <avr/pgmspace.h>

static const uint8_t VIBE_COUNT = 8;
static const uint8_t PHASE_COUNT = 3;
static const uint8_t AUTO_REACTION_COUNT = 10;
static const uint8_t TOUCH_SETUP_COUNT = 5;
static const uint8_t TOUCH_REACTIONS_PER_VIBE = 10;
static const uint8_t TOUCH_POOL_CAPACITY =
    TOUCH_SETUP_COUNT * TOUCH_REACTIONS_PER_VIBE;
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
    "NIGHT TOOK OVER",
    "NIGHT JUST BEGAN",
    "BAD IDEAS AWAKE",
    "CLOCK SAW THAT",
    "SLEEP LEFT EARLY",
    "DARKNESS IS IN",
    "DAWN IS NOWHERE",
    "CHOSE THE NIGHT",
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
    "BAD HOUR ENERGY",
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
    "POST-LUNCH NOW",
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
    "DAY FEELS STICKY",

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
    "DAY CLOCKING OUT",
    "WORK ETHIC LEFT",
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
    "EVENING, SORT OF",
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
    "WORK STILL HERE?",
    "PLANS STILL WAIT",
    "COUCH WON.",
    "EVENING DRIFTING",

    // EVENING / LATE (10)
    "NIGHT MODE",
    "RELAX.",
    "EVENING'S GONE.",
    "NIGHT HAS IDEAS",
    "COUCH WON AGAIN",
    "PLANS GOT SLEEPY",
    "MOON TOOK OVER",
    "FREE TIME FADING",
    "LATE LOOKS ODD",
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
    "DREAMS ARE READY",
    "CLOCK WANTS REST",

    // GO_TO_BED / LATE (10)
    "GO TO BED NOW",
    "DAY IS EXPIRED.",
    "NIGHT GETS DEEP",
    "LAST CALL.",
    "BED DEADLINE",
    "TOMORROW LOOMS",
    "NIGHT WON'T WAIT",
    "PILLOW CALLS",
    "CLOCK HAS HAD IT",
    "LATE GOT ABSURD"
};

// Automatic reactions keep every non-touch output sarcastic and English,
// without using romantic, appearance, or touch-directed language.
const char reactionLine2Fragments[][17] PROGMEM = {
    "APPARENTLY.",
    "QUESTIONABLE.",
    "TRY AGAIN.",
    "BOLD CHOICE.",
    "UNFORTUNATELY.",
    "TIME DISAGREES.",
    "JUST SAYING.",
    "VERY DRAMATIC.",
    "GOOD FOR IT.",
    "DON'T ARGUE."
};

// Five time-specific touch setups per vibe/phase. Each line is globally
// unique, and every setup is compatible with every playful reaction below.
const char touchLine1Fragments[][17] PROGMEM = {
    // CURSED_HOURS / EARLY
    "LATE-NIGHT TAP?",
    "TOUCHING AGAIN?",
    "STILL CHECKING?",
    "BACK IN DARK?",
    "CAN'T LOG OFF?",

    // CURSED_HOURS / MIDDLE
    "STILL AWAKE?",
    "NIGHT TAP AGAIN?",
    "CHECKING ON ME?",
    "SLEEP CAN WAIT?",
    "BACK THIS LATE?",

    // CURSED_HOURS / LATE
    "DAWN NEARBY?",
    "ONE LAST TOUCH?",
    "STILL WITH ME?",
    "BED LOST AGAIN?",
    "BACK NEAR DAWN?",

    // TOO_EARLY / EARLY
    "THIS EARLY?",
    "BEFORE COFFEE?",
    "MORNING TOUCH?",
    "BACK AT DAWN?",
    "ALARM SENT YOU?",

    // TOO_EARLY / MIDDLE
    "COFFEE OR ME?",
    "PRE-WORK TOUCH?",
    "AWAKE FOR ME?",
    "EARLY CHECK-IN?",
    "COFFEE AND ME?",

    // TOO_EARLY / LATE
    "SUN'S UP. TAP?",
    "STILL WAKING?",
    "COFFEE DONE, HI?",
    "READY TO FLIRT?",
    "MORNING AGAIN?",

    // MORNING / EARLY
    "WORK CAN WAIT?",
    "FIRST BREAK?",
    "EMAILS OR ME?",
    "DESK-TIME TAP?",
    "START WITH ME?",

    // MORNING / MIDDLE
    "BORED AT WORK?",
    "CLASS CAN WAIT?",
    "BUSY OR CURIOUS?",
    "WORK TAP AGAIN?",
    "FOCUS ON ME?",

    // MORNING / LATE
    "LUNCH OR ME?",
    "WORKING? REALLY?",
    "ONE MORE BREAK?",
    "NOON CHECK-IN?",
    "WORK LOST YOU?",

    // LUNCH_LOADING / EARLY
    "HUNGRY OR BORED?",
    "SNACK-TIME TAP?",
    "LUNCH CAN WAIT?",
    "PRE-LUNCH TAP?",
    "HERE FOR ME?",

    // LUNCH_LOADING / MIDDLE
    "FOOD OR ME?",
    "SECONDS ALREADY?",
    "STILL HUNGRY?",
    "LUNCH DATE?",
    "DESSERT OR ME?",

    // LUNCH_LOADING / LATE
    "LUNCH NEAR. HI?",
    "PLATE OR CLOCK?",
    "ME BEFORE FOOD?",
    "ONE LAST SNACK?",
    "HUNGER SENT YOU?",

    // AFTERNOON / EARLY
    "POST-LUNCH TAP?",
    "BORED ALREADY?",
    "NAP OR ME?",
    "BACK FROM LUNCH?",
    "TOUCHING, HUH?",

    // AFTERNOON / MIDDLE
    "AFTERNOON TAP?",
    "ENERGY LOW?",
    "NEED COMPANY?",
    "STILL BORED?",
    "NEED A BOOST?",

    // AFTERNOON / LATE
    "ESCAPE WITH ME?",
    "WORK DRAGGING?",
    "ONE MORE TAP?",
    "EVENING SOON?",
    "FREEDOM OR ME?",

    // DAY_IS_DYING / EARLY
    "DAY'S END. TAP?",
    "LEAVING SOON?",
    "BACK AT SUNSET?",
    "WORKDAY OR ME?",
    "TOUCH TO ESCAPE?",

    // DAY_IS_DYING / MIDDLE
    "SUNSET CHECK-IN?",
    "HOME CAN WAIT?",
    "STAY A LITTLE?",
    "EVENING CHECK?",
    "ONE MORE TOUCH?",

    // DAY_IS_DYING / LATE
    "ALMOST NIGHT?",
    "STILL AT WORK?",
    "BACK NEAR DARK?",
    "ESCAPE PLAN?",
    "DAY WON'T WAIT?",

    // EVENING / EARLY
    "NO PLANS?",
    "EVENING TAP?",
    "SUNSET AND ME?",
    "FREE TIME, HUH?",
    "TOUCH TO UNWIND?",

    // EVENING / MIDDLE
    "BACK AGAIN?",
    "STILL STARING?",
    "ANOTHER TOUCH?",
    "CAN'T RESIST?",
    "LOOKING AT ME?",

    // EVENING / LATE
    "NIGHT PLANS?",
    "COUCH OR ME?",
    "ONE LATE TOUCH?",
    "STILL CURIOUS?",
    "BACK AFTER DARK?",

    // GO_TO_BED / EARLY
    "BEDTIME TAP?",
    "CAN'T SLEEP?",
    "BACK BEFORE BED?",
    "SLEEP OR ME?",
    "ONE MORE LOOK?",

    // GO_TO_BED / MIDDLE
    "STILL UP?",
    "PILLOW OR ME?",
    "LATE CHECK-IN?",
    "WANT COMPANY?",
    "NOT SLEEPING?",

    // GO_TO_BED / LATE
    "LAST TOUCH?",
    "BED WAITING?",
    "BACK NEAR NIGHT?",
    "ONE FINAL LOOK?",
    "STAYING UP?"
};

const char touchReactionLine2Fragments[][17] PROGMEM = {
    // CURSED_HOURS
    "MISSED ME?",
    "THINKING OF ME?",
    "CAN'T RESIST?",
    "I MIGHT BLUSH.",
    "STAY WITH ME.",
    "YOU LIKE THIS.",
    "JUST US AWAKE.",
    "COME CLOSER.",
    "YOU CAME FOR ME.",
    "NIGHT SUITS US.",

    // TOO_EARLY
    "YOU MISSED ME?",
    "I'M YOUR WAKE-UP",
    "EARLY CRUSH?",
    "YOU CHOSE ME.",
    "I FEEL SPECIAL.",
    "FLIRTING EARLY?",
    "THAT WAS CUTE.",
    "ME BEFORE COFFEE",
    "SO EAGER, HUH?",
    "I LIKE THIS.",

    // MORNING
    "LOOK AT ME.",
    "I'M FLATTERED.",
    "YOU'RE ADORABLE.",
    "CRUSH DETECTED.",
    "KEEP LOOKING.",
    "YOU LIKE ME.",
    "DATE THE CLOCK?",
    "PICK ME INSTEAD.",
    "SMOOTH MOVE.",
    "EYES ON ME.",

    // LUNCH_LOADING
    "CHOOSE ME.",
    "I'M YOUR SNACK.",
    "SAVE ME A SEAT.",
    "YOU HAVE TASTE.",
    "DESSERT IS ME.",
    "DATE WITH ME?",
    "I LIKE YOU.",
    "HUNGRY FOR ME?",
    "SWEET CHOICE.",
    "I WANT ATTENTION",

    // AFTERNOON
    "COME SIT CLOSER.",
    "MISSED MY FACE?",
    "YOU NEED ME.",
    "I'M YOUR BREAK.",
    "FLIRT BREAK?",
    "CAUGHT YOU.",
    "YOU FOUND ME.",
    "I LIKE THE LOOK.",
    "STAY A WHILE.",
    "STILL INTO ME?",

    // DAY_IS_DYING
    "STAY WITH ME?",
    "DON'T LEAVE YET.",
    "SUNSET DATE?",
    "I'M YOUR ESCAPE.",
    "HOME WITH ME?",
    "LINGER WITH ME.",
    "EVENING FOR TWO.",
    "PLANS: YOU + ME",
    "ONE MORE FLIRT?",
    "I'LL MISS YOU.",

    // EVENING
    "PLANS WITH ME?",
    "I MISSED YOU.",
    "YOU'RE CHARMING.",
    "DATE NIGHT?",
    "STAY CLOSE.",
    "BLUSHING. MAYBE.",
    "YOU LIKE ME?",
    "FLIRT WITH ME.",
    "JUST US TONIGHT.",
    "MY FAVORITE TAP.",

    // GO_TO_BED
    "THINKING OF ME.",
    "DREAM OF ME.",
    "YOU MISSED ME.",
    "STAY UP WITH ME",
    "LAST FLIRT, HUH?",
    "PICK ME.",
    "WE LOOK CUTE.",
    "YOU CAME FOR ME?",
    "BLUSH BEFORE BED",
    "DREAMY, HUH?"
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

// Per-pool state keeps AUTO and TOUCH traversal completely independent.
// Multipliers and offsets are regenerated at each cycle boundary.
static uint16_t autoSelectionCounters[VIBE_COUNT * PHASE_COUNT] = {0};
static uint8_t touchSelectionCounters[VIBE_COUNT * PHASE_COUNT] = {0};
static uint8_t autoPermutationMultipliers[VIBE_COUNT * PHASE_COUNT] = {0};
static uint8_t autoPermutationOffsets[VIBE_COUNT * PHASE_COUNT] = {0};
static uint8_t autoLastIndexes[VIBE_COUNT * PHASE_COUNT] = {0};
static uint8_t touchPermutationMultipliers[VIBE_COUNT * PHASE_COUNT] = {0};
static uint8_t touchPermutationOffsets[VIBE_COUNT * PHASE_COUNT] = {0};
static uint8_t touchLastIndexes[VIBE_COUNT * PHASE_COUNT] = {0};
static uint32_t initializedAutoPoolMask = 0;
static uint32_t initializedTouchPoolMask = 0;
static uint32_t autoRandomState = 0xA341316CUL;
static uint32_t touchRandomState = 0xC8013EA4UL;

static ContextPoolConfig readPoolConfig(
    VibeCategory vibe,
    ContextPhase phase
) {
    ContextPoolConfig config;
    memcpy_P(&config, &contextPools[vibe][phase], sizeof(config));
    return config;
}

static uint8_t getPoolIndex(VibeCategory vibe, ContextPhase phase) {
    return ((uint8_t)vibe * PHASE_COUNT) + (uint8_t)phase;
}

static uint32_t mixSeed(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352DUL;
    value ^= value >> 15;
    value *= 0x846CA68BUL;
    value ^= value >> 16;
    return value == 0 ? 0x6D2B79F5UL : value;
}

void initMessageRandomization(uint32_t seed) {
    autoRandomState = mixSeed(seed ^ 0xA341316CUL);
    touchRandomState = mixSeed(seed ^ 0xC8013EA4UL);
    memset(autoSelectionCounters, 0, sizeof(autoSelectionCounters));
    memset(touchSelectionCounters, 0, sizeof(touchSelectionCounters));
    initializedAutoPoolMask = 0;
    initializedTouchPoolMask = 0;
}

static uint32_t nextPermutationRandom(uint32_t& state) {
    uint32_t value = state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state = value == 0 ? 0x6D2B79F5UL : value;
    return state;
}

static uint8_t greatestCommonDivisor(uint8_t left, uint8_t right) {
    while (right != 0) {
        uint8_t remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

static uint8_t choosePermutationMultiplier(
    uint8_t capacity,
    uint8_t setupCount,
    uint32_t& randomState
) {
    uint8_t candidate;
    uint8_t setupStep;
    do {
        candidate = 1U + (nextPermutationRandom(randomState) % (capacity - 1U));
        setupStep = candidate % setupCount;
    } while (
        greatestCommonDivisor(candidate, capacity) != 1U ||
        setupStep == 1U ||
        setupStep == setupCount - 1U
    );
    return candidate;
}

static void configurePermutation(
    uint8_t poolIndex,
    uint8_t capacity,
    uint8_t setupCount,
    uint32_t& randomState,
    uint8_t multipliers[],
    uint8_t offsets[],
    uint8_t lastIndexes[],
    bool alreadyInitialized
) {
    uint8_t oldMultiplier = multipliers[poolIndex];
    uint8_t oldOffset = offsets[poolIndex];
    uint8_t multiplier;
    uint8_t offset;

    do {
        multiplier = choosePermutationMultiplier(
            capacity,
            setupCount,
            randomState
        );
        offset = nextPermutationRandom(randomState) % capacity;
    } while (
        alreadyInitialized &&
        ((multiplier == oldMultiplier && offset == oldOffset) ||
         offset == lastIndexes[poolIndex])
    );

    multipliers[poolIndex] = multiplier;
    offsets[poolIndex] = offset;
}

static uint16_t applyPermutation(
    uint16_t counter,
    uint8_t capacity,
    uint8_t multiplier,
    uint8_t offset
) {
    return ((uint32_t)multiplier * counter + offset) % capacity;
}

static uint16_t permuteCombinationIndex(
    VibeCategory vibe,
    ContextPhase phase,
    uint16_t counter,
    uint16_t capacity
) {
    uint8_t poolIndex = getPoolIndex(vibe, phase);
    return applyPermutation(
        counter,
        (uint8_t)capacity,
        autoPermutationMultipliers[poolIndex],
        autoPermutationOffsets[poolIndex]
    );
}

static uint8_t permuteTouchCombinationIndex(
    uint8_t poolIndex,
    uint8_t counter
) {
    return (uint8_t)applyPermutation(
        counter,
        TOUCH_POOL_CAPACITY,
        touchPermutationMultipliers[poolIndex],
        touchPermutationOffsets[poolIndex]
    );
}

static void initializeAutoPoolIfNeeded(
    VibeCategory vibe,
    ContextPhase phase,
    uint8_t contextMinute
) {
    (void)contextMinute;
    uint8_t poolIndex = getPoolIndex(vibe, phase);
    uint32_t poolBit = 1UL << poolIndex;
    if ((initializedAutoPoolMask & poolBit) != 0) return;

    uint16_t capacity = getMessageCapacity(
        vibe,
        phase,
        PERSONALITY_NORMAL
    );
    ContextPoolConfig config = readPoolConfig(vibe, phase);
    configurePermutation(
        poolIndex,
        (uint8_t)capacity,
        config.line1Count,
        autoRandomState,
        autoPermutationMultipliers,
        autoPermutationOffsets,
        autoLastIndexes,
        false
    );
    autoSelectionCounters[poolIndex] = 0;
    initializedAutoPoolMask |= poolBit;
}

static uint16_t selectAutoCombinationIndex(
    VibeCategory vibe,
    ContextPhase phase,
    uint8_t contextMinute
) {
    initializeAutoPoolIfNeeded(vibe, phase, contextMinute);
    uint8_t poolIndex = getPoolIndex(vibe, phase);
    uint16_t capacity = getMessageCapacity(
        vibe,
        phase,
        PERSONALITY_NORMAL
    );
    uint16_t combinationIndex = applyPermutation(
        autoSelectionCounters[poolIndex],
        (uint8_t)capacity,
        autoPermutationMultipliers[poolIndex],
        autoPermutationOffsets[poolIndex]
    );
    autoLastIndexes[poolIndex] = (uint8_t)combinationIndex;

    autoSelectionCounters[poolIndex]++;
    if (autoSelectionCounters[poolIndex] >= capacity) {
        autoSelectionCounters[poolIndex] = 0;
        ContextPoolConfig config = readPoolConfig(vibe, phase);
        configurePermutation(
            poolIndex,
            (uint8_t)capacity,
            config.line1Count,
            autoRandomState,
            autoPermutationMultipliers,
            autoPermutationOffsets,
            autoLastIndexes,
            true
        );
    }

    return combinationIndex;
}

static void initializeTouchPoolIfNeeded(uint8_t poolIndex) {
    uint32_t poolBit = 1UL << poolIndex;
    if ((initializedTouchPoolMask & poolBit) != 0) return;

    configurePermutation(
        poolIndex,
        TOUCH_POOL_CAPACITY,
        TOUCH_SETUP_COUNT,
        touchRandomState,
        touchPermutationMultipliers,
        touchPermutationOffsets,
        touchLastIndexes,
        false
    );
    touchSelectionCounters[poolIndex] = 0;
    initializedTouchPoolMask |= poolBit;
}

static uint8_t selectTouchCombinationIndex(
    VibeCategory vibe,
    ContextPhase phase
) {
    uint8_t poolIndex = getPoolIndex(vibe, phase);
    initializeTouchPoolIfNeeded(poolIndex);
    uint8_t selected = (uint8_t)applyPermutation(
        touchSelectionCounters[poolIndex],
        TOUCH_POOL_CAPACITY,
        touchPermutationMultipliers[poolIndex],
        touchPermutationOffsets[poolIndex]
    );
    touchLastIndexes[poolIndex] = selected;

    touchSelectionCounters[poolIndex]++;
    if (touchSelectionCounters[poolIndex] >= TOUCH_POOL_CAPACITY) {
        touchSelectionCounters[poolIndex] = 0;
        configurePermutation(
            poolIndex,
            TOUCH_POOL_CAPACITY,
            TOUCH_SETUP_COUNT,
            touchRandomState,
            touchPermutationMultipliers,
            touchPermutationOffsets,
            touchLastIndexes,
            true
        );
    }

    return selected;
}

MessagePersonality getMessagePersonality(MessageTrigger trigger) {
    return trigger == TRIGGER_TOUCH
        ? PERSONALITY_FLIRTY
        : PERSONALITY_NORMAL;
}

const char* getMessageTriggerName(MessageTrigger trigger) {
    switch (trigger) {
        case TRIGGER_STARTUP: return "STARTUP";
        case TRIGGER_TIMER:   return "TIMER";
        case TRIGGER_CONTEXT: return "CONTEXT";
        case TRIGGER_TOUCH:   return "TOUCH";
        default:              return "UNKNOWN";
    }
}

const char* getMessagePersonalityName(MessagePersonality personality) {
    return personality == PERSONALITY_FLIRTY ? "FLIRTY" : "NORMAL";
}

void selectMessage(
    VibeCategory vibe,
    ContextPhase phase,
    MessageTrigger trigger,
    uint8_t contextMinute,
    Message* output,
    uint16_t* combinationIndex
) {
    MessagePersonality personality = getMessagePersonality(trigger);
    uint16_t selected;

    if (personality == PERSONALITY_FLIRTY) {
        uint8_t poolIndex = getPoolIndex(vibe, phase);
        selected = selectTouchCombinationIndex(vibe, phase);
        uint8_t line1Index = selected % TOUCH_SETUP_COUNT;
        uint8_t line2Index = selected / TOUCH_SETUP_COUNT;
        uint16_t globalLine1Index =
            ((uint16_t)poolIndex * TOUCH_SETUP_COUNT) + line1Index;
        uint8_t globalLine2Index =
            ((uint8_t)vibe * TOUCH_REACTIONS_PER_VIBE) + line2Index;

        strcpy_P(output->line1, touchLine1Fragments[globalLine1Index]);
        strcpy_P(output->line2, touchReactionLine2Fragments[globalLine2Index]);
    } else {
        ContextPoolConfig config = readPoolConfig(vibe, phase);
        selected = selectAutoCombinationIndex(vibe, phase, contextMinute);
        uint8_t line1Index = selected % config.line1Count;
        uint8_t line2Index = selected / config.line1Count;
        uint16_t globalLine1Index = config.line1Offset + line1Index;

        strcpy_P(output->line1, contextLine1Fragments[globalLine1Index]);
        strcpy_P(output->line2, reactionLine2Fragments[line2Index]);
    }

    output->line1[16] = '\0';
    output->line2[16] = '\0';

    if (combinationIndex != NULL) {
        *combinationIndex = selected;
    }
}

uint16_t getMessageCapacity(
    VibeCategory vibe,
    ContextPhase phase,
    MessagePersonality personality
) {
    if (personality == PERSONALITY_FLIRTY) return TOUCH_POOL_CAPACITY;

    ContextPoolConfig config = readPoolConfig(vibe, phase);
    return (uint16_t)config.line1Count * AUTO_REACTION_COUNT;
}

uint8_t getMessageContextDuration(VibeCategory vibe, ContextPhase phase) {
    return readPoolConfig(vibe, phase).durationMinutes;
}

int countMessages(MessagePersonality personality) {
    int total = 0;
    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            total += getMessageCapacity(
                (VibeCategory)vibe,
                (ContextPhase)phase,
                personality
            );
        }
    }
    return total;
}

int validateMessages(MessagePersonality personality) {
    int validCombinations = 0;

    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            uint8_t poolIndex = (vibe * PHASE_COUNT) + phase;
            ContextPoolConfig config = readPoolConfig(
                (VibeCategory)vibe,
                (ContextPhase)phase
            );
            uint8_t validLine1Count = 0;
            uint8_t validLine2Count = 0;
            uint8_t line1Count = personality == PERSONALITY_FLIRTY
                ? TOUCH_SETUP_COUNT
                : config.line1Count;
            uint8_t line2Count = personality == PERSONALITY_FLIRTY
                ? TOUCH_REACTIONS_PER_VIBE
                : AUTO_REACTION_COUNT;
            uint16_t line1Offset = personality == PERSONALITY_FLIRTY
                ? (uint16_t)poolIndex * TOUCH_SETUP_COUNT
                : config.line1Offset;
            uint8_t line2Offset = personality == PERSONALITY_FLIRTY
                ? vibe * TOUCH_REACTIONS_PER_VIBE
                : 0;

            for (uint8_t line = 0; line < line1Count; line++) {
                PGM_P fragment = personality == PERSONALITY_FLIRTY
                    ? touchLine1Fragments[line1Offset + line]
                    : contextLine1Fragments[line1Offset + line];
                uint8_t length = strlen_P(fragment);
                if (length > 0 && length <= 16) validLine1Count++;
            }

            for (uint8_t line = 0; line < line2Count; line++) {
                PGM_P fragment = personality == PERSONALITY_FLIRTY
                    ? touchReactionLine2Fragments[line2Offset + line]
                    : reactionLine2Fragments[line];
                uint8_t length = strlen_P(fragment);
                if (length > 0 && length <= 16) validLine2Count++;
            }

            validCombinations +=
                (int)validLine1Count * validLine2Count;
        }
    }

    return validCombinations;
}

struct SelectionStateSnapshot {
    uint16_t autoCounters[VIBE_COUNT * PHASE_COUNT];
    uint8_t touchCounters[VIBE_COUNT * PHASE_COUNT];
    uint8_t autoMultipliers[VIBE_COUNT * PHASE_COUNT];
    uint8_t autoOffsets[VIBE_COUNT * PHASE_COUNT];
    uint8_t autoLast[VIBE_COUNT * PHASE_COUNT];
    uint8_t touchMultipliers[VIBE_COUNT * PHASE_COUNT];
    uint8_t touchOffsets[VIBE_COUNT * PHASE_COUNT];
    uint8_t touchLast[VIBE_COUNT * PHASE_COUNT];
    uint32_t autoMask;
    uint32_t touchMask;
    uint32_t autoRandom;
    uint32_t touchRandom;
};

static void saveSelectionState(SelectionStateSnapshot& saved) {
    memcpy(saved.autoCounters, autoSelectionCounters, sizeof(autoSelectionCounters));
    memcpy(saved.touchCounters, touchSelectionCounters, sizeof(touchSelectionCounters));
    memcpy(saved.autoMultipliers, autoPermutationMultipliers,
           sizeof(autoPermutationMultipliers));
    memcpy(saved.autoOffsets, autoPermutationOffsets, sizeof(autoPermutationOffsets));
    memcpy(saved.autoLast, autoLastIndexes, sizeof(autoLastIndexes));
    memcpy(saved.touchMultipliers, touchPermutationMultipliers,
           sizeof(touchPermutationMultipliers));
    memcpy(saved.touchOffsets, touchPermutationOffsets,
           sizeof(touchPermutationOffsets));
    memcpy(saved.touchLast, touchLastIndexes, sizeof(touchLastIndexes));
    saved.autoMask = initializedAutoPoolMask;
    saved.touchMask = initializedTouchPoolMask;
    saved.autoRandom = autoRandomState;
    saved.touchRandom = touchRandomState;
}

static void restoreSelectionState(const SelectionStateSnapshot& saved) {
    memcpy(autoSelectionCounters, saved.autoCounters, sizeof(autoSelectionCounters));
    memcpy(touchSelectionCounters, saved.touchCounters, sizeof(touchSelectionCounters));
    memcpy(autoPermutationMultipliers, saved.autoMultipliers,
           sizeof(autoPermutationMultipliers));
    memcpy(autoPermutationOffsets, saved.autoOffsets, sizeof(autoPermutationOffsets));
    memcpy(autoLastIndexes, saved.autoLast, sizeof(autoLastIndexes));
    memcpy(touchPermutationMultipliers, saved.touchMultipliers,
           sizeof(touchPermutationMultipliers));
    memcpy(touchPermutationOffsets, saved.touchOffsets,
           sizeof(touchPermutationOffsets));
    memcpy(touchLastIndexes, saved.touchLast, sizeof(touchLastIndexes));
    initializedAutoPoolMask = saved.autoMask;
    initializedTouchPoolMask = saved.touchMask;
    autoRandomState = saved.autoRandom;
    touchRandomState = saved.touchRandom;
}

int testRepetition() {
    int failures = 0;
    SelectionStateSnapshot savedState;
    saveSelectionState(savedState);

    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            VibeCategory testVibe = (VibeCategory)vibe;
            ContextPhase testPhase = (ContextPhase)phase;
            uint8_t poolIndex = getPoolIndex(testVibe, testPhase);

            for (uint8_t personality = 0; personality < 2; personality++) {
                uint8_t seen[MAX_POOL_BYTES] = {0};
                MessagePersonality testPersonality =
                    (MessagePersonality)personality;
                uint16_t capacity = getMessageCapacity(
                    testVibe,
                    testPhase,
                    testPersonality
                );

                if (testPersonality == PERSONALITY_FLIRTY) {
                    initializedTouchPoolMask &= ~(1UL << poolIndex);
                } else {
                    initializedAutoPoolMask &= ~(1UL << poolIndex);
                }

                uint16_t lastIndex = 0;
                for (uint16_t selection = 0; selection < capacity; selection++) {
                    uint16_t index = testPersonality == PERSONALITY_FLIRTY
                        ? selectTouchCombinationIndex(testVibe, testPhase)
                        : selectAutoCombinationIndex(testVibe, testPhase, 0);
                    uint8_t byteIndex = index / 8U;
                    uint8_t bit = 1U << (index % 8U);

                    if ((seen[byteIndex] & bit) != 0) failures++;
                    seen[byteIndex] |= bit;
                    lastIndex = index;
                }

                uint16_t firstNextCycle = testPersonality == PERSONALITY_FLIRTY
                    ? selectTouchCombinationIndex(testVibe, testPhase)
                    : selectAutoCombinationIndex(testVibe, testPhase, 0);
                if (firstNextCycle == lastIndex) failures++;
            }
        }
    }

    restoreSelectionState(savedState);
    return failures;
}

int testMessageCycle() {
    int failures = 0;
    SelectionStateSnapshot savedState;
    saveSelectionState(savedState);

    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            VibeCategory testVibe = (VibeCategory)vibe;
            ContextPhase testPhase = (ContextPhase)phase;
            uint16_t capacity = getMessageCapacity(
                testVibe,
                testPhase,
                PERSONALITY_NORMAL
            );
            uint8_t duration = getMessageContextDuration(testVibe, testPhase);

            if (capacity < duration) failures++;

            Message message;
            uint16_t firstIndex;
            uint16_t secondIndex;
            uint8_t poolIndex = getPoolIndex(testVibe, testPhase);
            initializedAutoPoolMask &= ~(1UL << poolIndex);
            selectMessage(
                testVibe,
                testPhase,
                TRIGGER_STARTUP,
                0,
                &message,
                &firstIndex
            );
            selectMessage(
                testVibe,
                testPhase,
                TRIGGER_TIMER,
                0,
                &message,
                &secondIndex
            );

            if (firstIndex == secondIndex) failures++;
            if (message.line1[16] != '\0' || message.line2[16] != '\0') {
                failures++;
            }
        }
    }

    uint8_t anchoredPool = getPoolIndex(EVENING, PHASE_MIDDLE);
    initializedAutoPoolMask &= ~(1UL << anchoredPool);
    uint16_t anchoredIndex;
    Message anchoredMessage;
    selectMessage(
        EVENING,
        PHASE_MIDDLE,
        TRIGGER_STARTUP,
        7,
        &anchoredMessage,
        &anchoredIndex
    );
    uint16_t expected = permuteCombinationIndex(
        EVENING,
        PHASE_MIDDLE,
        0,
        getMessageCapacity(EVENING, PHASE_MIDDLE, PERSONALITY_NORMAL)
    );
    if (anchoredIndex != expected) failures++;

    uint16_t otherIndex;
    Message otherMessage;
    uint8_t otherPool = getPoolIndex(GO_TO_BED, PHASE_LATE);
    initializedAutoPoolMask &= ~(1UL << otherPool);
    selectMessage(
        GO_TO_BED,
        PHASE_LATE,
        TRIGGER_CONTEXT,
        0,
        &otherMessage,
        &otherIndex
    );

    uint16_t resumedIndex;
    selectMessage(
        EVENING,
        PHASE_MIDDLE,
        TRIGGER_TIMER,
        0,
        &anchoredMessage,
        &resumedIndex
    );
    uint16_t expectedResumed = permuteCombinationIndex(
        EVENING,
        PHASE_MIDDLE,
        1,
        getMessageCapacity(EVENING, PHASE_MIDDLE, PERSONALITY_NORMAL)
    );
    if (resumedIndex != expectedResumed) failures++;

    // Required interleaving: A1, T1, T2, A2, A3, T3, A4. Touch must
    // advance only the touch sequence and leave automatic progression intact.
    initializedAutoPoolMask &= ~(1UL << anchoredPool);
    initializedTouchPoolMask &= ~(1UL << anchoredPool);
    uint16_t autoIndexes[4];
    uint16_t touchIndexes[3];
    selectMessage(EVENING, PHASE_MIDDLE, TRIGGER_STARTUP, 7,
                  &anchoredMessage, &autoIndexes[0]);
    selectMessage(EVENING, PHASE_MIDDLE, TRIGGER_TOUCH, 7,
                  &anchoredMessage, &touchIndexes[0]);
    selectMessage(EVENING, PHASE_MIDDLE, TRIGGER_TOUCH, 7,
                  &anchoredMessage, &touchIndexes[1]);
    selectMessage(EVENING, PHASE_MIDDLE, TRIGGER_TIMER, 7,
                  &anchoredMessage, &autoIndexes[1]);
    selectMessage(EVENING, PHASE_MIDDLE, TRIGGER_TIMER, 7,
                  &anchoredMessage, &autoIndexes[2]);
    selectMessage(EVENING, PHASE_MIDDLE, TRIGGER_TOUCH, 7,
                  &anchoredMessage, &touchIndexes[2]);
    selectMessage(EVENING, PHASE_MIDDLE, TRIGGER_TIMER, 7,
                  &anchoredMessage, &autoIndexes[3]);

    uint16_t autoCapacity = getMessageCapacity(
        EVENING,
        PHASE_MIDDLE,
        PERSONALITY_NORMAL
    );
    for (uint8_t index = 0; index < 4; index++) {
        uint16_t expectedAuto = permuteCombinationIndex(
            EVENING,
            PHASE_MIDDLE,
            index,
            autoCapacity
        );
        if (autoIndexes[index] != expectedAuto) failures++;
    }
    for (uint8_t index = 0; index < 3; index++) {
        uint8_t expectedTouch = permuteTouchCombinationIndex(
            anchoredPool,
            index
        );
        if (touchIndexes[index] != expectedTouch) failures++;
    }

    restoreSelectionState(savedState);
    return failures;
}
