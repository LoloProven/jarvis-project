#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include "config.h"

#define MAX_ALARMS 5

class AlarmManager {
public:
    static void init();
    static void checkAlarms();
    static void addAlarm(int h, int m);
    static void deleteAlarm(int index);
    static void toggleAlarm(int index);   // activa/desactiva sin borrarla

    // Cronómetro
    static void toggleChrono();
    static void resetChrono();
    static bool isChronoRunning();
    static unsigned long getChronoTime();

    static AlarmItem alarmList[MAX_ALARMS];
    static int alarmCount;

private:
    static void save();
    static bool chronoRunning;
    static unsigned long chronoStartTime;
    static unsigned long chronoElapsedTime;
};

#endif