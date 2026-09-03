#include "display_controller.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static const uint8_t LCD_COLUMNS = 16;
static const uint8_t LCD_ROWS = 2;

static LiquidCrystal_I2C lcd(KALACHAKRAM_LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

static void padLine(const char* source, char paddedLine[17]) {
    uint8_t index = 0;

    while (index < LCD_COLUMNS && source[index] != '\0') {
        paddedLine[index] = source[index];
        index++;
    }

    while (index < LCD_COLUMNS) {
        paddedLine[index] = ' ';
        index++;
    }

    paddedLine[LCD_COLUMNS] = '\0';
}

void initDisplay() {
    Wire.begin();
    lcd.init();
    lcd.backlight();
}

void displayMessage(const Message& message) {
    char paddedLine[17];

    padLine(message.line1, paddedLine);
    lcd.setCursor(0, 0);
    lcd.print(paddedLine);

    padLine(message.line2, paddedLine);
    lcd.setCursor(0, 1);
    lcd.print(paddedLine);
}
