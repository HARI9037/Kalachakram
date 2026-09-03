#ifndef MESSAGES_H
#define MESSAGES_H

#include "vibe_engine.h"

struct Message {
    char line1[17];
    char line2[17];
};

enum MessageTrigger {
    TRIGGER_STARTUP,
    TRIGGER_TIMER,
    TRIGGER_CONTEXT,
    TRIGGER_TOUCH
};

enum MessagePersonality {
    PERSONALITY_NORMAL,
    PERSONALITY_FLIRTY
};

// Selects the next unique combination from the active contextual pool.
// contextMinute anchors a mid-phase startup to its wall-clock minute slot.
void selectMessage(
    VibeCategory vibe,
    ContextPhase phase,
    MessageTrigger trigger,
    uint8_t contextMinute,
    Message* output,
    uint16_t* combinationIndex
);

MessagePersonality getMessagePersonality(MessageTrigger trigger);
const char* getMessageTriggerName(MessageTrigger trigger);
const char* getMessagePersonalityName(MessagePersonality personality);

uint16_t getMessageCapacity(
    VibeCategory vibe,
    ContextPhase phase,
    MessagePersonality personality
);
uint8_t getMessageContextDuration(VibeCategory vibe, ContextPhase phase);

// Returns the number of initialized messages whose two lines fit the LCD
int validateMessages(MessagePersonality personality);

// Counts initialized two-line messages in the database
int countMessages(MessagePersonality personality);

// Tests selection uniqueness across every complete contextual cycle.
int testRepetition();

// Tests capacity, generated buffers, and context-switch behavior.
int testMessageCycle();

#endif
