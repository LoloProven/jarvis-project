#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include <Arduino.h>

class TouchDriver {
public:
    static bool getTouch(int16_t &x, int16_t &y);
    static void waitForRelease();
};

#endif