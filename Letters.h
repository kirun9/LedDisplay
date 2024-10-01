#ifndef _Letters_h
#define _Letters_h

#include "arduino.h"

class Letters {
public:
    static const uint16_t DHCP[];
    static const uint16_t _0[];
    static const uint16_t _1[];
    static const uint16_t _2[];
    static const uint16_t _3[];
    static const uint16_t _4[];
    static const uint16_t _5[];
    static const uint16_t _6[];
    static const uint16_t _7[];
    static const uint16_t _8[];
    static const uint16_t _9[];
    static const uint16_t _DOT[];
    static const uint16_t NODHCP[];

    static const uint16_t* getNumber(int number) {
        switch (number) {
            case 0: return _0;
            case 1: return _1;
            case 2: return _2;
            case 3: return _3;
            case 4: return _4;
            case 5: return _5;
            case 6: return _6;
            case 7: return _7;
            case 8: return _8;
            case 9: return _9;
        }
        return { 0x0000 };
    }
};

const uint16_t Letters::DHCP[] = { 0x001D, 0x0000, 0x001C, 0x0004, 0x0018, 0x0000, 0x001D, 0x0000, 0x0014, 0x0000, 0x0000 };
const uint16_t Letters::_0[] = { 0x000E, 0x0011, 0x000E, 0x0000 };
const uint16_t Letters::_1[] = { 0x0012, 0x001F, 0x0010, 0x0000 };
const uint16_t Letters::_2[] = { 0x0019, 0x0015, 0x0012, 0x0000 };
const uint16_t Letters::_3[] = { 0x0011, 0x0015, 0x000E, 0x0000 };
const uint16_t Letters::_4[] = { 0x0007, 0x0004, 0x001F, 0x0000 };
const uint16_t Letters::_5[] = { 0x0017, 0x0015, 0x000D, 0x0000 };
const uint16_t Letters::_6[] = { 0x001E, 0x0015, 0x001D, 0x0000 };
const uint16_t Letters::_7[] = { 0x0019, 0x0005, 0x0003, 0x0000 };
const uint16_t Letters::_8[] = { 0x001F, 0x0015, 0x001F, 0x0000 };
const uint16_t Letters::_9[] = { 0x0017, 0x0015, 0x000F, 0x0000 };
const uint16_t Letters::_DOT[] = { 0x0010, 0x0000 };
const uint16_t Letters::NODHCP[] = { 0x001F, 0x0008, 0x001F, 0x0000, 0x001D, 0x0000, 0x001F, 0x0005, 0x0005, 0x0000, 0x001D, 0x0000, 0x0000, 0x0000, 0x001F, 0x0015, 0x0015, 0x0000, 0x001F, 0x0005, 0x001A, 0x0000, 0x001F, 0x0005, 0x001A, 0x0000, 0x001F, 0x0011, 0x001F, 0x0000, 0x001F, 0x0005, 0x001A };
#endif