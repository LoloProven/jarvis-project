#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

// Pantalla apagada "como un celular": se apaga solo el panel (DISPOFF + modo reposo),
// la CPU y el WiFi siguen andando, así que la hora, el cronómetro y las alarmas continúan.
// Mantener apretado PWR 4 s es otra cosa: el AXP2101 corta toda la energía por hardware.
class PowerManager {
public:
    static void init();
    static void checkButton();             // toque corto de PWR: apaga o enciende la pantalla
    static void checkInactivity();         // apaga la pantalla tras SCREEN_TIMEOUT_MS sin toques
    static void notifyActivity();          // reinicia el contador de inactividad
    static void lowPowerTick();            // con la pantalla apagada un rato, apaga el WiFi
    static void turnScreenOff();
    static void turnScreenOn(bool redraw = true);
    static bool isScreenOff();
    static XPowersPMU& getPMU();

    // Batería (se lee del AXP2101 cada BATTERY_READ_MS desde updateBattery()).
    static void updateBattery();
    static int batteryPercent();        // 0-100, o -1 si no hay batería conectada
    static bool isCharging();

private:
    static XPowersPMU PMU;
    static bool screenOff;
    static unsigned long lastActivity;
};

#endif
