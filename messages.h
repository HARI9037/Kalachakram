#ifndef MESSAGES_H
#define MESSAGES_H

#include "vibe_engine.h"

struct Message {
    char line1[17];
    char line2[17];
};

// Selects from the active vibe/phase pool without repeating within a cycle
void selectMessage(VibeCategory vibe, ContextPhase phase, Message* output);

// Returns the number of initialized messages whose two lines fit the LCD
int validateMessages();

// Counts initialized two-line messages in the database
int countMessages();

// Tests selection algorithm for immediate repeats across all contextual pools
int testRepetition();

// Tests full four-message cycle coverage and context-reset behavior
int testMessageCycle();

#endif
