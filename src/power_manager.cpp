#include "power_manager.h"
#include "config.h"
#include "display_ui.h"
#include "i2c_lock.h"

XPowersPMU PowerManager::PMU;
bool PowerManager::screenOff = false;
unsigned long PowerManager::lastActivity = 0;

void PowerManager::init() {
    Wire.begin(PMU_SDA, PMU_SCL);
    if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
        // Las tensiones de las salidas (DC1, ALDO1-4, BLDO1-2) se dejan como vienen de
        // fábrica, igual que los ejemplos de Waveshare, que no las tocan. Antes se forzaban
        // todas a 3,3 V, pero no todas son de 3,3 V (ALDO4, por ejemplo, viene en 1,8 V).
        PMU.enableBattVoltageMeasure();
        PMU.enableBattDetection();          // necesario para el porcentaje (como el ejemplo 05)

        // Mantener apretado PWR 4 s = apagado total por hardware (no pasa por el software).
        PMU.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
        PMU.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
        PMU.clearIrqStatus();
    }
    lastActivity = millis();
}

// Solo se apaga el panel. No se tocan las salidas del AXP2101: DC1 alimenta al
// sistema, y cortarlo apagaba o reiniciaba el reloj entero.
void PowerManager::turnScreenOff() {
    if (screenOff) return;
    screenOff = true;
    DisplayUI::getGFX()->displayOff();
}

void PowerManager::turnScreenOn(bool redraw) {
    lastActivity = millis();
    if (!screenOff) return;
    screenOff = false;
    DisplayUI::getGFX()->displayOn();
    if (redraw) DisplayUI::refreshCurrentScreen();   // con la hora y el cronómetro al día
}

void PowerManager::checkButton() {
    bool shortPress;
    {
        I2CLock lock;                        // el I2C se comparte con la tarea de pasos
        // getIrqStatus() actualiza los registros internos de la librería
        PMU.getIrqStatus();
        shortPress = PMU.isPekeyShortPressIrq();
        PMU.clearIrqStatus();
    }
    // Protección extra: dos toques de PWR a menos de 400 ms no son de una persona; así
    // una lectura mala nunca puede dejar la pantalla prendiendo y apagando sin parar.
    static unsigned long lastPress = 0;
    if (shortPress && millis() - lastPress > 400) {
        lastPress = millis();
        if (screenOff) turnScreenOn();
        else turnScreenOff();
    }
}

void PowerManager::checkInactivity() {
    if (!screenOff && millis() - lastActivity > SCREEN_TIMEOUT_MS) {
        turnScreenOff();
    }
}

static const unsigned long BATTERY_READ_MS = 30000;
static int battPercent = -1;
static bool battCharging = false;
static unsigned long lastBatteryRead = 0;

void PowerManager::updateBattery() {
    if (lastBatteryRead != 0 && millis() - lastBatteryRead < BATTERY_READ_MS) return;
    lastBatteryRead = millis();
    I2CLock lock;                            // el I2C se comparte con la tarea de pasos
    battPercent = PMU.isBatteryConnect() ? PMU.getBatteryPercent() : -1;
    if (battPercent > 100) battPercent = 100;
    battCharging = PMU.isCharging();
}

int PowerManager::batteryPercent() { return battPercent; }
bool PowerManager::isCharging() { return battCharging; }

void PowerManager::notifyActivity() { lastActivity = millis(); }
bool PowerManager::isScreenOff() { return screenOff; }
XPowersPMU& PowerManager::getPMU() { return PMU; }
