#include "alarm_manager.h"
#include "display_ui.h"
#include "time.h"

AlarmItem AlarmManager::alarmList[MAX_ALARMS];
int AlarmManager::alarmCount = 0;
bool AlarmManager::chronoRunning = false;
unsigned long AlarmManager::chronoStartTime = 0;
unsigned long AlarmManager::chronoElapsedTime = 0;

void AlarmManager::init() {
    alarmCount = 0;
}

void AlarmManager::addAlarm(int h, int m) {
    if (alarmCount < MAX_ALARMS) {
        alarmList[alarmCount].hour = h;
        alarmList[alarmCount].minute = m;
        alarmList[alarmCount].active = true;
        alarmList[alarmCount].lastFiredKey = -1;
        alarmCount++;
    }
}

void AlarmManager::deleteAlarm(int index) {
    if (index < 0 || index >= alarmCount) return;
    for (int i = index; i < alarmCount - 1; i++) {
        alarmList[i] = alarmList[i + 1];
    }
    alarmCount--;
}

// Por ahora las alarmas son de una sola vez: al sonar quedan inactivas
// (triggerAlarmRinging pone active = false) y no se repiten al día siguiente.
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