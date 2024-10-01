#ifndef _SETTINGS_h
#define _SETTINGS_h

#include "arduino.h"

class Settings {

public:

    Settings() {}

    static const int MAX_VALUES = 1000;
    static const int MAX_SEND_SIZE = 112;
    int SCROLL_DELAY = 4;
    int DISPLAY_TIME = 2;
    bool ALWAYS_SCROLL = false;
    bool RESTART = false;
    static bool ENABLE_WIFI;
    static bool ENABLE_SD;
    void applySettings(uint8_t settingName, uint8_t settingValue);
};

extern Settings settings;

#endif