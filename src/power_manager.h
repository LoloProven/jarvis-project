#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

// Botón PWR:
//  - toque corto: apaga/prende la pantalla. El reloj sigue andando (hora, pasos, alarmas)
//    pero gastando menos: 80 MHz, sin revisar internet y sin WiFi al minuto.
//  - 1,5 s: modo dormir (sueño liviano). Gasta muy poco; la hora, los pasos y las alarmas
//    siguen. Se despierta con PWR, BOOT, tocando la pantalla o con una alarma.
//    También entra solo tras SLEEP_AFTER_MS con la pantalla apagada.
//  - 4 s: apagado total por hardware (el AXP2101 corta la energía; no suena nada).
class PowerManager {
public:
    enum WakeReason { WAKE_NONE, WAKE_TOUCH, WAKE_BUTTON };

    static void init();
    static void checkButton();             // PWR: corto = pantalla, largo = modo dormir
    static void checkInactivity();         // apaga la pantalla tras SCREEN_TIMEOUT_MS sin toques
    static void notifyActivity();          // reinicia el contador de inactividad
    static void lowPowerTick();            // pantalla apagada un rato: apaga el WiFi y después duerme
    static void turnScreenOff();
    static void turnScreenOn(bool redraw = true);   // también sale del modo dormir
    static void glanceOn();                // solo el panel, para la hora grande (levantar la muñeca)
    static bool isGlance();
    static bool isScreenOff();
    static XPowersPMU& getPMU();

    static void enterSleep();
    static bool isSleeping();
    static WakeReason sleepTick();         // duerme hasta 1 s; devuelve por qué se despertó

    // Batería (se lee del AXP2101 cada BATTERY_READ_MS desde updateBattery()).
    static void updateBattery();
    static int batteryPercent();        // 0-100, o -1 si no hay batería conectada
    static bool isCharging();

private:
    static XPowersPMU PMU;
    static bool screenOff;
    static bool sleeping;
    static unsigned long lastActivity;
};

#endif
