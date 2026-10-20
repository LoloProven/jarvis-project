#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

class PowerManager {
public:
    static void init();
    static void checkButton();
    static void toggleScreenState();
    static bool isScreenOff();
    static XPowersPMU& getPMU();

private:
    static XPowersPMU PMU;
    static bool screenOff;
};

#endif