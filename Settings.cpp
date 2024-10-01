#include "settings.h"

bool Settings::ENABLE_WIFI = true;
bool Settings::ENABLE_SD = true;

void Settings::applySettings(uint8_t settingName, uint8_t settingValue) {
    switch (settingName) {
        case 0b00000010:  // Set DISPLAY_TIME value in ms (2 bytes)
            DISPLAY_TIME = settingValue;
            break;

        case 0b00000011:  // Set SCROLL_DELAY value (1 byte)
            SCROLL_DELAY = settingValue;
            break;

        case 0b00000100:  // Set ALWAYS_SCROLL (bool)
            ALWAYS_SCROLL = settingValue == 0b11111111;
            break;

        case 0x00000001:  // Set RESTART
            RESTART = settingValue == 0b11111111;
            break;

        default:
            // Unknown setting name, discard the frame
            break;
    }
}
