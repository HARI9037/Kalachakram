#ifndef VIBE_ENGINE_H
#define VIBE_ENGINE_H

#include "time_engine.h"

enum VibeCategory {
    CURSED_HOURS,
    TOO_EARLY,
    MORNING,
    LUNCH_LOADING,
    AFTERNOON,
    DAY_IS_DYING,
    EVENING,
    GO_TO_BED
};

VibeCategory classifyVibe(const TimeContext& time);

const char* getVibeName(VibeCategory vibe);

#endif
