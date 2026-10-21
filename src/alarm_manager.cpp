#include "alarm_manager.h"
#include "display_ui.h"
#include "time.h"
#include <Preferences.h>

AlarmItem AlarmManager::alarmList[MAX_ALARMS];
int AlarmManager::alarmCount = 0;
bool AlarmManager::chronoRunning = false;
unsigned long AlarmManager::chronoStartTime = 0;
unsigned long AlarmManager::chronoElapsedTime = 0;

// Las alarmas se guardan en la memoria permanente (NVS) para que no se pierdan al
// reiniciar o apagar el reloj: 3 bytes por alarma (hora, minuto, activa).
static Preferences alarmPrefs;
static const char* ALARM_NS = "alarms";

void AlarmManager::init() {
    alarmCount = 0;
    alarmPrefs.begin(ALARM_NS, true);
    uint8_t buf[MAX_ALARMS * 3];
    size_t len = alarmPrefs.getBytesLength("list");
    if (len > 0 && len <= sizeof(buf) && len % 3 == 0) {
        alarmPrefs.getBytes("list", buf, len);
        for (size_t i = 0; i < len / 3; i++) {
            if (buf[i * 3] > 23 || buf[i * 3 + 1] > 59) continue;   // dato corrupto
            alarmList[alarmCount].hour = buf[i * 3];
            alarmList[alarmCount].minute = buf[i * 3 + 1];
            alarmList[alarmCount].active = buf[i * 3 + 2] != 0;
            alarmList[alarmCount].lastFiredKey = -1;
            alarmCount++;
        }
    }
    alarmPrefs.end();
}

void AlarmManager::save() {
    uint8_t buf[MAX_ALARMS * 3];
    for (int i = 0; i < alarmCount; i++) {
        buf[i * 3]     = alarmList[i].hour;
        buf[i * 3 + 1] = alarmList[i].minute;
        buf[i * 3 + 2] = alarmList[i].active ? 1 : 0;
    }
    alarmPrefs.begin(ALARM_NS, false);
    if (alarmCount > 0) alarmPrefs.putBytes("list", buf, alarmCount * 3);
    else alarmPrefs.remove("list");
    alarmPrefs.end();
}

void AlarmManager::addAlarm(int h, int m) {
    if (alarmCount < MAX_ALARMS) {
        alarmList[alarmCount].hour = h;
        alarmList[alarmCount].minute = m;
        alarmList[alarmCount].active = true;
        alarmList[alarmCount].lastFiredKey = -1;
        alarmCount++;
        save();
    }
}

void AlarmManager::deleteAlarm(int index) {
    if (index < 0 || index >= alarmCount) return;
    for (int i = index; i < alarmCount - 1; i++) {
        alarmList[i] = alarmList[i + 1];
    }
    alarmCount--;
    save();
}

void AlarmManager::toggleAlarm(int index) {
    if (index < 0 || index >= alarmCount) return;
    alarmList[index].active = !alarmList[index].active;
    save();
}

// Las alarmas se repiten todos los días a la misma hora mientras estén activas.
void AlarmManager::checkAlarms() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0)) return;

    // Se dispara en cualquier segundo del minuto que coincide, así no se pierde si
    // el loop estuvo ocupado justo en el segundo :00. lastFiredKey evita que vuelva
    // a sonar dentro del mismo minuto.
    long nowKey = (long)timeinfo.tm_yday * 1440 + timeinfo.tm_hour * 60 + timeinfo.tm_min;

    for (int i = 0; i < alarmCount; i++) {
        if (alarmList[i].active &&
            alarmList[i].lastFiredKey != nowKey &&
            timeinfo.tm_hour == alarmList[i].hour &&
            timeinfo.tm_min  == alarmList[i].minute) {
            alarmList[i].lastFiredKey = nowKey;
            DisplayUI::triggerAlarmRinging(i);   // bloquea hasta que se desactiva
        }
    }
}

void AlarmManager::toggleChrono() {
    if (!chronoRunning) {
        chronoStartTime = millis();
        chronoRunning = true;
    } else {
        chronoElapsedTime += millis() - chronoStartTime;
        chronoRunning = false;
    }
}

void AlarmManager::resetChrono() {
    chronoRunning = false;
    chronoElapsedTime = 0;
}

bool AlarmManager::isChronoRunning() { return chronoRunning; }

unsigned long AlarmManager::getChronoTime() {
    return chronoRunning ? (millis() - chronoStartTime + chronoElapsedTime) : chronoElapsedTime;
}