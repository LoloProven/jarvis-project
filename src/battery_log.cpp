#include "battery_log.h"
#include "power_manager.h"
#include "wifi_portal.h"
#include "i2c_lock.h"
#include <Arduino.h>
#include <Preferences.h>

static Preferences prefs;
static BatteryLog::Entry entries[BatteryLog::MAX_ENTRIES];
static int entryCount = 0;
static uint8_t startPct[BatteryLog::MAX_ENTRIES];   // porcentaje al empezar cada tramo

// Tramo en curso: milisegundos en cada estado.
static uint32_t msOn = 0, msOff = 0, msSleep = 0, msWifi = 0;
static unsigned long tramoStart = 0, lastTick = 0;
static int tramoStartPct = -1;
static bool tramoCharging = false;

static int readPercent(bool& charging) {
    I2CLock lock;                          // el I2C se comparte con la tarea de pasos
    XPowersPMU& pmu = PowerManager::getPMU();
    charging = pmu.isCharging();
    if (!pmu.isBatteryConnect()) return -1;
    int p = pmu.getBatteryPercent();
    return p > 100 ? 100 : p;
}

static void save() {
    prefs.begin("batlog", false);
    prefs.putBytes("e", entries, entryCount * sizeof(BatteryLog::Entry));
    prefs.putBytes("s", startPct, entryCount);
    prefs.end();
}

void BatteryLog::init() {
    prefs.begin("batlog", true);
    size_t n = prefs.getBytesLength("e") / sizeof(Entry);
    if (n > MAX_ENTRIES) n = MAX_ENTRIES;
    if (n > 0 && prefs.getBytesLength("s") == n) {
        prefs.getBytes("e", entries, n * sizeof(Entry));
        prefs.getBytes("s", startPct, n);
        entryCount = n;
    }
    prefs.end();
    tramoStart = lastTick = millis();
    tramoStartPct = readPercent(tramoCharging);
}

void BatteryLog::tick() {
    unsigned long now = millis();
    uint32_t dt = now - lastTick;
    lastTick = now;
    if (PowerManager::isSleeping()) msSleep += dt;
    else if (PowerManager::isScreenOff()) msOff += dt;
    else msOn += dt;                       // también el vistazo de la muñeca (panel prendido)
    if (!WiFiPortal::isSuspended()) msWifi += dt;
    if (!PowerManager::isSleeping() && PowerManager::isCharging()) tramoCharging = true;

    if (now - tramoStart < (unsigned long)INTERVAL_MIN * 60000UL) return;

    bool chargingNow;
    int pct = readPercent(chargingNow);
    if (pct >= 0 && tramoStartPct >= 0) {
        if (entryCount == MAX_ENTRIES) {   // se descarta el más viejo
            memmove(entries, entries + 1, (MAX_ENTRIES - 1) * sizeof(Entry));
            memmove(startPct, startPct + 1, MAX_ENTRIES - 1);
            entryCount--;
        }
        Entry& e = entries[entryCount];
        e.when = (uint32_t)time(nullptr);
        e.pct = pct;
        e.charging = (tramoCharging || chargingNow || pct > tramoStartPct) ? 1 : 0;
        e.onMin = (msOn + 30000) / 60000;
        e.offMin = (msOff + 30000) / 60000;
        e.sleepMin = (msSleep + 30000) / 60000;
        e.wifiMin = (msWifi + 30000) / 60000;
        startPct[entryCount] = tramoStartPct;
        entryCount++;
        save();
    }
    tramoStart = now;
    tramoStartPct = pct;
    tramoCharging = chargingNow;
    msOn = msOff = msSleep = msWifi = 0;
}

int BatteryLog::count() { return entryCount; }
const BatteryLog::Entry& BatteryLog::get(int i) { return entries[i]; }

float BatteryLog::ratePerHour(int state) {
    int dropped = 0, minutes = 0;
    for (int i = 0; i < entryCount; i++) {
        const Entry& e = entries[i];
        if (e.charging) continue;
        int total = e.onMin + e.offMin + e.sleepMin;
        int inState = state == 0 ? e.onMin : state == 1 ? e.offMin : e.sleepMin;
        if (total == 0 || inState * 10 < total * 8) continue;   // menos del 80 % en ese estado
        dropped += (int)startPct[i] - (int)e.pct;
        minutes += total;
    }
    if (minutes < 30) return -1;           // muy poco tiempo para decir algo
    return dropped * 60.0f / minutes;
}

void BatteryLog::clear() {
    entryCount = 0;
    prefs.begin("batlog", false);
    prefs.clear();
    prefs.end();
}
