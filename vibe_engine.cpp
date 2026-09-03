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
