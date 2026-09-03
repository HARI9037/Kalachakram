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

enum ContextPhase {
    PHASE_EARLY,
    PHASE_MIDDLE,
    PHASE_LATE
};

VibeCategory classifyVibe(const TimeContext& time);
ContextPhase classifyContextPhase(const TimeContext& time, VibeCategory vibe);

const char* getVibeName(VibeCategory vibe);
const char* getContextPhaseName(ContextPhase phase);

#endif
