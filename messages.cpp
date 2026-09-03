#include "messages.h"
#include <Arduino.h>
#include <avr/pgmspace.h>

const char vibe_messages[8][6][2][17] PROGMEM = {
    { // CURSED_HOURS (0)
        {"WHY ARE YOU", "STILL AWAKE?"},
        {"THIS HOUR", "SHOULD NOT EXIST"},
        {"GO TO BED.", "SERIOUSLY."},
        {"SLEEP IS A", "GOOD THING."},
        {"NOTHING GOOD", "HAPPENS NOW."},
        {"LOG OFF.", "RIGHT NOW."}
    },
    { // TOO_EARLY (1)
        {"TOO EARLY.", "TRY AGAIN."},
        {"MORNING?", "UNFORTUNATELY."},
        {"COFFEE FIRST.", "TIME LATER."},
        {"SUN IS UP.", "I AM NOT."},
        {"MISTAKES WERE", "MADE TODAY."},
        {"BARELY AWAKE.", "DO NOT PROVOKE."}
    },
    { // MORNING (2)
        {"DAY STARTED.", "APPARENTLY."},
        {"MORNING-ISH.", "GOOD ENOUGH."},
        {"PRODUCTIVITY", "EXPECTED."},
        {"DOING THINGS?", "SAD."},
        {"TIME TO PRETEND", "TO WORK."},
        {"STILL MORNING.", "BARELY."}
    },
    { // LUNCH_LOADING (3)
        {"LUNCH IS", "APPROACHING."},
        {"FOOD SOON.", "PROBABLY."},
        {"WORK CAN WAIT.", "ALMOST."},
        {"FOCUS FADING.", "HUNGER RISING."},
        {"PRE-LUNCH STAGE.", "SURVIVE."},
        {"ESSENTIALLY NOON", "STOP TYPING."}
    },
    { // AFTERNOON (4)
        {"DEFINITELY", "AFTERNOON."},
        {"WORK ENERGY", "DECLINING."},
        {"LUNCH IS GONE.", "MOVE ON."},
        {"THE LONG SLUMP", "HAS BEGUN."},
        {"STILL HERE?", "WHY?"},
        {"AFTERNOON GLITCH", "IN PROGRESS."}
    },
    { // DAY_IS_DYING (5)
        {"ALMOST EVENING.", "PROBABLY."},
        {"DAY IS", "RUNNING OUT."},
        {"PRODUCTIVITY", "HAS LEFT."},
        {"BASICALLY SIX.", "DON'T ARGUE."},
        {"FINISH UP.", "OR DON'T."},
        {"SUNSET INCOMING.", "GIVE UP."}
    },
    { // EVENING (6)
        {"EVENING-ISH.", "GOOD ENOUGH."},
        {"DAY IS OVER.", "MOSTLY."},
        {"WORK?", "QUESTIONABLE."},
        {"FREE TIME.", "WASTE IT."},
        {"NIGHT MODE", "ACTIVATED."},
        {"RELAX.", "OR WHATEVER."}
    },
    { // GO_TO_BED (7)
        {"GO TO BED.", "SERIOUSLY."},
        {"SLEEP EXISTS.", "REMEMBER?"},
        {"TOMORROW IS", "GETTING CLOSE."},
        {"LATE ENOUGH.", "LOGGING OFF."},
        {"WHY ARE YOU", "STILL LOOKING?"},
        {"DAY IS EXPIRED.", "GO AWAY."}
    }
};

static int lastSelectedIndex = -1;
static VibeCategory lastVibe = (VibeCategory)-1;

void selectMessage(VibeCategory vibe, Message* output) {
    if (vibe != lastVibe) {
        lastSelectedIndex = -1;
        lastVibe = vibe;
    }
    
    int nextIndex;
    
    if (lastSelectedIndex == -1) {
        nextIndex = random(6);
    } else {
        // Pick from the remaining 5 to strictly prevent immediate repeat
        nextIndex = random(5);
        if (nextIndex >= lastSelectedIndex) {
            nextIndex++;
        }
    }
    
    lastSelectedIndex = nextIndex;
    
    strcpy_P(output->line1, vibe_messages[vibe][nextIndex][0]);
    strcpy_P(output->line2, vibe_messages[vibe][nextIndex][1]);
}

int validateMessages() {
    int validCount = 0;
    for (int v = 0; v < 8; v++) {
        for (int m = 0; m < 6; m++) {
            int len1 = strlen_P(vibe_messages[v][m][0]);
            int len2 = strlen_P(vibe_messages[v][m][1]);
            
            if (len1 <= 16 && len2 <= 16) {
                validCount++;
            } else {
                Serial.print(F("INVALID LENGTH at Vibe "));
                Serial.print(v);
                Serial.print(F(" Msg "));
                Serial.println(m);
            }
        }
    }
    return validCount;
}

int countMessages() {
    return 8 * 6; // Fixed 48 in this architecture
}

int testRepetition() {
    int totalRepeats = 0;
    
    // Save state so we don't break main logic state
    int savedIndex = lastSelectedIndex;
    VibeCategory savedVibe = lastVibe;
    
    for (int v = 0; v < 8; v++) {
        VibeCategory vibe = (VibeCategory)v;
        Message msg;
        
        char lastLine1[17] = "";
        
        // Force reset per vibe test
        lastVibe = (VibeCategory)-1;
        
        for (int i = 0; i < 20; i++) {
            selectMessage(vibe, &msg);
            if (strcmp(msg.line1, lastLine1) == 0 && i > 0) {
                totalRepeats++;
            }
            strcpy(lastLine1, msg.line1);
        }
    }
    
    // Restore state
    lastSelectedIndex = savedIndex;
    lastVibe = savedVibe;
    
    return totalRepeats;
}
