#include "vibe_engine.h"

VibeCategory classifyVibe(const TimeContext& time) {
    uint8_t h = time.hour;
    
    if (h >= 0 && h < 5) {
        return CURSED_HOURS;
    } else if (h >= 5 && h < 8) {
        return TOO_EARLY;
    } else if (h >= 8 && h < 11) {
        return MORNING;
    } else if (h >= 11 && h < 13) {
        return LUNCH_LOADING;
    } else if (h >= 13 && h < 16) {
        return AFTERNOON;
    } else if (h >= 16 && h < 18) {
        return DAY_IS_DYING;
    } else if (h >= 18 && h < 21) {
        return EVENING;
    } else { // 21 to 23
        return GO_TO_BED;
    }
}

static void getVibeWindowMinutes(
    VibeCategory vibe,
    uint16_t& startMinutes,
    uint16_t& durationMinutes
) {
    switch (vibe) {
        case CURSED_HOURS:
            startMinutes = 0;
            durationMinutes = 300;
            break;
        case TOO_EARLY:
            startMinutes = 300;
            durationMinutes = 180;
            break;
        case MORNING:
            startMinutes = 480;
            durationMinutes = 180;
            break;
        case LUNCH_LOADING:
            startMinutes = 660;
            durationMinutes = 120;
            break;
        case AFTERNOON:
            startMinutes = 780;
            durationMinutes = 180;
            break;
        case DAY_IS_DYING:
            startMinutes = 960;
            durationMinutes = 120;
            break;
        case EVENING:
            startMinutes = 1080;
            durationMinutes = 180;
            break;
        case GO_TO_BED:
        default:
            startMinutes = 1260;
            durationMinutes = 180;
            break;
    }
}

ContextPhase classifyContextPhase(const TimeContext& time, VibeCategory vibe) {
    uint16_t startMinutes;
    uint16_t durationMinutes;
    getVibeWindowMinutes(vibe, startMinutes, durationMinutes);

    uint32_t currentSeconds =
        (time.hour * 3600UL) + (time.minute * 60UL) + time.second;
    uint32_t startSeconds = startMinutes * 60UL;
    uint32_t durationSeconds = durationMinutes * 60UL;
    uint32_t elapsedSeconds = currentSeconds - startSeconds;
    uint32_t scaledProgress = elapsedSeconds * 3UL;

    if (scaledProgress < durationSeconds) {
        return PHASE_EARLY;
    } else if (scaledProgress < durationSeconds * 2UL) {
        return PHASE_MIDDLE;
    } else {
        return PHASE_LATE;
    }
}

const char* getVibeName(VibeCategory vibe) {
    switch(vibe) {
        case CURSED_HOURS:   return "CURSED_HOURS";
        case TOO_EARLY:      return "TOO_EARLY";
        case MORNING:        return "MORNING";
        case LUNCH_LOADING:  return "LUNCH_LOADING";
        case AFTERNOON:      return "AFTERNOON";
        case DAY_IS_DYING:   return "DAY_IS_DYING";
        case EVENING:        return "EVENING";
        case GO_TO_BED:      return "GO_TO_BED";
        default:             return "UNKNOWN";
    }
}

const char* getContextPhaseName(ContextPhase phase) {
    switch (phase) {
        case PHASE_EARLY:  return "EARLY";
        case PHASE_MIDDLE: return "MIDDLE";
        case PHASE_LATE:   return "LATE";
        default:           return "UNKNOWN";
    }
}
