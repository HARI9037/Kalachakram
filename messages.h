#ifndef MESSAGES_H
#define MESSAGES_H

#include "vibe_engine.h"

struct Message {
    char line1[17];
    char line2[17];
};

// Selects a random message for the given vibe, avoiding immediate repetition
void selectMessage(VibeCategory vibe, Message* output);

// Returns the number of valid messages found (max 48) where lengths <= 16
int validateMessages();

// Returns the total number of messages in the database
int countMessages();

// Tests selection algorithm for immediate repeats across all categories
int testRepetition();

#endif
