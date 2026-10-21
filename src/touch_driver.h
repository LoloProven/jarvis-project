#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include <Arduino.h>

class TouchDriver {
public:
    static void init();      // después de Wire.begin(): engancha la interrupción de TP_INT
    static bool getTouch(int16_t &x, int16_t &y);
    static void waitForRelease();
};

#endif