#ifndef TOUCH_CONTROLLER_H
#define TOUCH_CONTROLLER_H

#include <stdint.h>

// Defaults for a digital-output, active-HIGH touch sensor module.
// Change these values if the physical module uses another pin or polarity.
#define KALACHAKRAM_TOUCH_PIN 2
#define KALACHAKRAM_TOUCH_ACTIVE_HIGH 1

void initTouchSensor();

// Returns true once for each debounced touch press.
bool wasTouchPressed(uint32_t currentMillis);

#endif
