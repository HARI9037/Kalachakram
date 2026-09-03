#include "messages.h"
#include <Arduino.h>
#include <avr/pgmspace.h>

static const uint8_t VIBE_COUNT = 8;
static const uint8_t PHASE_COUNT = 3;
static const uint8_t MESSAGES_PER_PHASE = 4;
static const uint8_t ALL_MESSAGES_USED = 0x0F;

const char vibe_messages[8][3][4][2][17] PROGMEM = {
    { // CURSED_HOURS
        { // EARLY
            {"THIS HOUR", "SHOULD NOT EXIST"},
            {"NOTHING GOOD", "HAPPENS NOW."},
            {"MIDNIGHT PASSED.", "BAD DECISION."},
            {"NIGHT JUST", "GETTING WORSE."}
        },
        { // MIDDLE
            {"WHY ARE YOU", "STILL AWAKE?"},
            {"SLEEP IS A", "GOOD THING."},
            {"STILL NIGHT.", "STILL AWAKE."},
            {"THE CLOCK IS", "JUDGING YOU."}
        },
        { // LATE
            {"GO TO BED.", "SERIOUSLY."},
            {"LOG OFF.", "RIGHT NOW."},
            {"DAWN IS CLOSE.", "YOU LOST."},
            {"MORNING SOON.", "REGRETTABLE."}
        }
    },
    { // TOO_EARLY
        { // EARLY
            {"TOO EARLY.", "TRY AGAIN."},
            {"MISTAKES WERE", "MADE TODAY."},
            {"DAWN ARRIVED.", "RUDELY."},
            {"THE DAY STARTED", "WITHOUT CONSENT."}
        },
        { // MIDDLE
            {"MORNING?", "UNFORTUNATELY."},
            {"COFFEE FIRST.", "TIME LATER."},
            {"MORNING EXISTS.", "UNFORTUNATE."},
            {"COFFEE WINDOW.", "ENTER NOW."}
        },
        { // LATE
            {"SUN IS UP.", "I AM NOT."},
            {"BARELY AWAKE.", "DO NOT PROVOKE."},
            {"ALMOST AWAKE.", "NOT QUITE."},
            {"EARLY IS ENDING.", "SLOWLY."}
        }
    },
    { // MORNING
        { // EARLY
            {"DAY STARTED.", "APPARENTLY."},
            {"MORNING-ISH.", "GOOD ENOUGH."},
            {"MORNING IS HERE.", "ACT NATURAL."},
            {"EMAILS AWAKEN.", "HIDE."}
        },
        { // MIDDLE
            {"PRODUCTIVITY", "EXPECTED."},
            {"DOING THINGS?", "SAD."},
            {"DAY IN PROGRESS.", "ALLEGEDLY."},
            {"TASKS EXIST.", "UNFORTUNATELY."}
        },
        { // LATE
            {"TIME TO PRETEND", "TO WORK."},
            {"STILL MORNING.", "BARELY."},
            {"NOON IS NEARBY.", "STAY ALERT."},
            {"MORNING ENDING.", "PRODUCTIVITY?"}
        }
    },
    { // LUNCH_LOADING
        { // EARLY
            {"LUNCH IS", "APPROACHING."},
            {"FOOD SOON.", "PROBABLY."},
            {"HUNGER ONLINE.", "WORK OFFLINE."},
            {"LUNCH DETECTED.", "NOT YET."}
        },
        { // MIDDLE
            {"WORK CAN WAIT.", "ALMOST."},
            {"FOCUS FADING.", "HUNGER RISING."},
            {"FOOD THOUGHTS.", "TAKING OVER."},
            {"CLOCK SAYS", "SEEK SNACKS."}
        },
        { // LATE
            {"PRE-LUNCH STAGE.", "SURVIVE."},
            {"ESSENTIALLY NOON", "STOP TYPING."},
            {"ALMOST LUNCH.", "STAY STRONG."},
            {"PRODUCTIVITY", "ENDED EARLY."}
        }
    },
    { // AFTERNOON
        { // EARLY
            {"DEFINITELY", "AFTERNOON."},
            {"LUNCH IS GONE.", "MOVE ON."},
            {"LUNCH AFTERMATH.", "NOW WHAT?"},
            {"DAY CONTINUES.", "FOR SOME REASON."}
        },
        { // MIDDLE
            {"WORK ENERGY", "DECLINING."},
            {"THE LONG SLUMP", "HAS BEGUN."},
            {"AFTERNOON HUM.", "LOW BATTERY."},
            {"ENERGY LEFT.", "NO COMMENT."}
        },
        { // LATE
            {"STILL HERE?", "WHY?"},
            {"AFTERNOON GLITCH", "IN PROGRESS."},
            {"EVENING LOADING.", "VERY SLOWLY."},
            {"WORKDAY FADING.", "LET IT."}
        }
    },
    { // DAY_IS_DYING
        { // EARLY
            {"DAY IS", "RUNNING OUT."},
            {"PRODUCTIVITY", "HAS LEFT."},
            {"DAY LOSING", "ITS GRIP."},
            {"AFTERNOON EXIT.", "PLEASE WAIT."}
        },
        { // MIDDLE
            {"ALMOST EVENING.", "PROBABLY."},
            {"FINISH UP.", "OR DON'T."},
            {"EVENING NEARBY.", "ACT BUSY."},
            {"THE SUN IS", "DONE WORKING."}
        },
        { // LATE
            {"BASICALLY SIX.", "DON'T ARGUE."},
            {"SUNSET INCOMING.", "GIVE UP."},
            {"CLOCK SAYS", "WRAP IT UP."},
            {"DAY ALMOST OVER.", "GOOD ENOUGH."}
        }
    },
    { // EVENING
        { // EARLY
            {"EVENING-ISH.", "GOOD ENOUGH."},
            {"DAY IS OVER.", "MOSTLY."},
            {"EVENING STARTED.", "APPARENTLY."},
            {"DAY IS ENDING.", "DON'T PANIC."}
        },
        { // MIDDLE
            {"WORK?", "QUESTIONABLE."},
            {"FREE TIME.", "WASTE IT."},
            {"DINNER MAYBE.", "DECIDE LATER."},
            {"FREE TIME FOUND.", "MISUSE IT."}
        },
        { // LATE
            {"NIGHT MODE", "ACTIVATED."},
            {"RELAX.", "OR WHATEVER."},
            {"EVENING'S GONE.", "MOSTLY."},
            {"NIGHT IS", "GETTING IDEAS."}
        }
    },
    { // GO_TO_BED
        { // EARLY
            {"TOMORROW IS", "GETTING CLOSE."},
            {"LATE ENOUGH.", "LOGGING OFF."},
            {"BEDTIME NEARBY.", "IGNORE WISELY."},
            {"NIGHT STARTED.", "LOWER AMBITION."}
        },
        { // MIDDLE
            {"WHY ARE YOU", "STILL LOOKING?"},
            {"SLEEP EXISTS.", "REMEMBER?"},
            {"SLEEP WINDOW.", "STILL OPEN."},
            {"TOMORROW WAITS.", "MENACINGLY."}
        },
        { // LATE
            {"GO TO BED.", "SERIOUSLY."},
            {"DAY IS EXPIRED.", "GO AWAY."},
            {"MIDNIGHT NEARS.", "BAD SIGN."},
            {"LAST CALL.", "FOR BEING AWAKE."}
        }
    }
};

static uint8_t usedMessageMask = 0;
static int8_t lastSelectedIndex = -1;
static VibeCategory activeVibe = (VibeCategory)-1;
static ContextPhase activePhase = (ContextPhase)-1;

static void startContextCycle(VibeCategory vibe, ContextPhase phase) {
    activeVibe = vibe;
    activePhase = phase;
    usedMessageMask = 0;
    lastSelectedIndex = -1;
}

static uint8_t selectMessageIndex(VibeCategory vibe, ContextPhase phase) {
    if (vibe != activeVibe || phase != activePhase) {
        startContextCycle(vibe, phase);
    }

    bool startingNewCycle = (usedMessageMask == ALL_MESSAGES_USED);
    if (startingNewCycle) {
        usedMessageMask = 0;
    }

    uint8_t availableIndexes[MESSAGES_PER_PHASE];
    uint8_t availableCount = 0;

    for (uint8_t index = 0; index < MESSAGES_PER_PHASE; index++) {
        bool alreadyUsed = (usedMessageMask & (1U << index)) != 0;
        bool immediateRepeat =
            startingNewCycle && index == (uint8_t)lastSelectedIndex;

        if (!alreadyUsed && !immediateRepeat) {
            availableIndexes[availableCount] = index;
            availableCount++;
        }
    }

    uint8_t selectedIndex = availableIndexes[random(availableCount)];
    usedMessageMask |= (1U << selectedIndex);
    lastSelectedIndex = selectedIndex;
    return selectedIndex;
}

void selectMessage(VibeCategory vibe, ContextPhase phase, Message* output) {
    uint8_t selectedIndex = selectMessageIndex(vibe, phase);

    strcpy_P(output->line1, vibe_messages[vibe][phase][selectedIndex][0]);
    strcpy_P(output->line2, vibe_messages[vibe][phase][selectedIndex][1]);
}

int validateMessages() {
    int validCount = 0;

    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            for (uint8_t message = 0; message < MESSAGES_PER_PHASE; message++) {
                const char* line1 = vibe_messages[vibe][phase][message][0];
                const char* line2 = vibe_messages[vibe][phase][message][1];
                int length1 = strlen_P(line1);
                int length2 = strlen_P(line2);

                if (
                    length1 > 0 && length1 <= 16 &&
                    length2 > 0 && length2 <= 16
                ) {
                    validCount++;
                } else {
                    Serial.print(F("INVALID MESSAGE: vibe "));
                    Serial.print(vibe);
                    Serial.print(F(", phase "));
                    Serial.print(phase);
                    Serial.print(F(", message "));
                    Serial.println(message);
                }
            }
        }
    }

    return validCount;
}

int countMessages() {
    int initializedCount = 0;

    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            for (uint8_t message = 0; message < MESSAGES_PER_PHASE; message++) {
                const char* line1 = vibe_messages[vibe][phase][message][0];
                const char* line2 = vibe_messages[vibe][phase][message][1];

                if (pgm_read_byte(line1) != '\0' && pgm_read_byte(line2) != '\0') {
                    initializedCount++;
                }
            }
        }
    }

    return initializedCount;
}

int testRepetition() {
    int immediateRepeats = 0;

    uint8_t savedMask = usedMessageMask;
    int8_t savedIndex = lastSelectedIndex;
    VibeCategory savedVibe = activeVibe;
    ContextPhase savedPhase = activePhase;

    for (uint8_t vibe = 0; vibe < VIBE_COUNT; vibe++) {
        for (uint8_t phase = 0; phase < PHASE_COUNT; phase++) {
            startContextCycle((VibeCategory)vibe, (ContextPhase)phase);
            int8_t previousIndex = -1;

            for (uint8_t selection = 0; selection < 20; selection++) {
                uint8_t selectedIndex = selectMessageIndex(
                    (VibeCategory)vibe,
                    (ContextPhase)phase
                );

                if (selectedIndex == (uint8_t)previousIndex) {
                    immediateRepeats++;
                }

                previousIndex = selectedIndex;
            }
        }
    }

    usedMessageMask = savedMask;
    lastSelectedIndex = savedIndex;
    activeVibe = savedVibe;
    activePhase = savedPhase;

    return immediateRepeats;
}

int testMessageCycle() {
    int failures = 0;

    uint8_t savedMask = usedMessageMask;
    int8_t savedIndex = lastSelectedIndex;
    VibeCategory savedVibe = activeVibe;
    ContextPhase savedPhase = activePhase;

    startContextCycle(EVENING, PHASE_MIDDLE);
    uint8_t seenMask = 0;

    for (uint8_t selection = 0; selection < MESSAGES_PER_PHASE; selection++) {
        uint8_t selectedIndex =
            selectMessageIndex(EVENING, PHASE_MIDDLE);
        uint8_t selectedBit = 1U << selectedIndex;

        if ((seenMask & selectedBit) != 0) {
            failures++;
        }

        seenMask |= selectedBit;
    }

    if (seenMask != ALL_MESSAGES_USED) {
        failures++;
    }

    uint8_t firstNextCycle = selectMessageIndex(EVENING, PHASE_MIDDLE);
    if (usedMessageMask != (uint8_t)(1U << firstNextCycle)) {
        failures++;
    }

    uint8_t firstNewContext = selectMessageIndex(EVENING, PHASE_LATE);
    if (
        activeVibe != EVENING ||
        activePhase != PHASE_LATE ||
        usedMessageMask != (uint8_t)(1U << firstNewContext)
    ) {
        failures++;
    }

    uint8_t firstNewVibe = selectMessageIndex(GO_TO_BED, PHASE_LATE);
    if (
        activeVibe != GO_TO_BED ||
        activePhase != PHASE_LATE ||
        usedMessageMask != (uint8_t)(1U << firstNewVibe)
    ) {
        failures++;
    }

    usedMessageMask = savedMask;
    lastSelectedIndex = savedIndex;
    activeVibe = savedVibe;
    activePhase = savedPhase;

    return failures;
}
