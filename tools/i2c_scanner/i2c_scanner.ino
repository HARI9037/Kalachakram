#include <Wire.h>

const unsigned long SCAN_INTERVAL = 5000UL;
unsigned long lastScanTime = 0;
bool firstScan = true;

void scanI2CBus() {
    uint8_t deviceCount = 0;

    Serial.println(F("Scanning..."));
    Serial.println();

    // Scan the normal usable 7-bit I2C address range: 0x01 through 0x7E.
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            Serial.print(F("Found device at 0x"));
            if (address < 16) {
                Serial.print('0');
            }
            Serial.println(address, HEX);
            deviceCount++;
        } else if (error == 4) {
            Serial.print(F("Unknown error at 0x"));
            if (address < 16) {
                Serial.print('0');
            }
            Serial.println(address, HEX);
        }
    }

    Serial.println();
    Serial.print(F("Devices found: "));
    Serial.println(deviceCount);
    Serial.println();
}

void setup() {
    Serial.begin(9600);
    while (!Serial) { ; }

    Wire.begin();

    Serial.println(F("KALACHAKRAM I2C SCANNER"));
    Serial.println();
}

void loop() {
    unsigned long currentMillis = millis();

    if (firstScan || currentMillis - lastScanTime >= SCAN_INTERVAL) {
        firstScan = false;
        lastScanTime = currentMillis;
        scanI2CBus();
    }
}
