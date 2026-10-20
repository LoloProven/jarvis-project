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

void AlarmManager::checkAlarms() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    for (int i = 0; i < alarmCount; i++) {
        if (alarmList[i].active) {
            if (timeinfo.tm_hour == alarmList[i].hour && 
                timeinfo.tm_min  == alarmList[i].minute && 
                timeinfo.tm_sec  == 0) {
                DisplayUI::triggerAlarmRinging(i);
                break;
            }
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