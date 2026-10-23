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
    static void snooze(int index, int minutes);   // vuelve a sonar dentro de "minutes"
    static int findAlarm(int h, int m);   // índice de la alarma a esa hora, o -1

    // Cronómetro
    static void toggleChrono();
    static void resetChrono();
    static bool isChronoRunning();
    static unsigned long getChronoTime();

    // Temporizador (cuenta regresiva). Al llegar a 0 suena como una alarma.
    static void timerAdd(int minutes);        // suma minutos (también mientras corre)
    static void startTimer(unsigned long seconds);   // lo pone en ese tiempo y arranca (JARVIS)
    static void toggleTimer();                // iniciar / pausa
    static void resetTimer();
    static bool isTimerRunning();
    static unsigned long getTimerRemaining(); // ms que faltan

    static AlarmItem alarmList[MAX_ALARMS];
    static int alarmCount;

private:
    static void save();
    static bool chronoRunning;
    static unsigned long chronoStartTime;
    static unsigned long chronoElapsedTime;
};

#endif