#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include "messages.h"

// Placeholder/default until the physical I2C scanner result is known.
// Change this single value to the address reported by the scanner.
#define KALACHAKRAM_LCD_ADDRESS 0x27

void initDisplay();
void displayMessage(const Message& message);

#endif
