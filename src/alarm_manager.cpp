#include "alarm_manager.h"
#include "display_ui.h"
#include "time.h"
#include <Preferences.h>
#include <sys/time.h>
#include "time_sync.h"

AlarmItem AlarmManager::alarmList[MAX_ALARMS];
int AlarmManager::alarmCount = 0;
bool AlarmManager::chronoRunning = false;
unsigned long AlarmManager::chronoStartTime = 0;
unsigned long AlarmManager::chronoElapsedTime = 0;

// Las alarmas se guardan en la memoria permanente (NVS) para que no se pierdan al
// reiniciar o apagar el reloj: 3 bytes por alarma (hora, minuto, activa).
static Preferences alarmPrefs;
static const char* ALARM_NS = "alarms";

// --- CRONÓMETRO Y TEMPORIZADOR QUE SOBREVIVEN A UN APAGADO ---
// Mientras el reloj anda se usa millis(); en la memoria (NVS) se guarda la hora real (del
// RTC, que sigue contando apagado) en que arrancó el cronómetro o termina el temporizador.
// Al prender se recalcula con la hora actual: el tiempo apagado también cuenta.
static bool timerRunning = false;
static unsigned long timerEndMs = 0;
static unsigned long timerLeftMs = 0;

static int64_t nowEpochMs() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void saveChrono() {
    alarmPrefs.begin("crono", false);
    alarmPrefs.putBool("c_on", AlarmManager::chronoRunning);
    alarmPrefs.putULong("c_acc", AlarmManager::chronoElapsedTime);
    // Hora real en que arrancó el tramo actual.
    alarmPrefs.putLong64("c_ini", nowEpochMs() - (int64_t)(millis() - AlarmManager::chronoStartTime));
    alarmPrefs.putBool("t_on", timerRunning);
    alarmPrefs.putULong("t_left", timerLeftMs);
    alarmPrefs.putLong64("t_fin", nowEpochMs() + (int64_t)(long)(timerEndMs - millis()));
    alarmPrefs.end();
}

static void loadChrono() {
    alarmPrefs.begin("crono", true);
    bool cOn = alarmPrefs.getBool("c_on", false);
    AlarmManager::chronoElapsedTime = alarmPrefs.getULong("c_acc", 0);
    int64_t cIni = alarmPrefs.getLong64("c_ini", 0);
    bool tOn = alarmPrefs.getBool("t_on", false);
    timerLeftMs = alarmPrefs.getULong("t_left", 0);
    int64_t tFin = alarmPrefs.getLong64("t_fin", 0);
    alarmPrefs.end();

    // Sin hora confiable (RTC sin pila y sin internet todavía) no se puede saber cuánto pasó:
    // queda en pausa con lo que tenía.
    bool timeOk = TimeSync::isSynced();
    int64_t now = nowEpochMs();
    if (cOn) {
        int64_t ran = now - cIni;
        if (timeOk && ran >= 0 && ran < 7LL * 24 * 3600 * 1000) {
            AlarmManager::chronoRunning = true;
            AlarmManager::chronoStartTime = millis() - (unsigned long)ran;
        }
    }
    if (tOn) {
        int64_t left = tFin - now;
        if (timeOk) {
            timerRunning = true;               // si ya pasó, suena enseguida
            timerEndMs = millis() + (unsigned long)(left > 0 ? left : 0);
        }
    }
    Serial.printf("[crono] recuperado: crono %s %lu ms, temporizador %s\n", AlarmManager::chronoRunning ? "corriendo" : "quieto",
                  AlarmManager::getChronoTime(), timerRunning ? "corriendo" : "quieto");
}

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
    loadChrono();          // después de TimeSync::init (main.cpp): necesita la hora real
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

// Posponer: la alarma vuelve a sonar en snoozeAt (hora del sistema). No se guarda en la
// memoria: si el reloj se reinicia, se pierde el "posponer" (la alarma diaria no).
static time_t snoozeAt = 0;
static int snoozeIdx = -1;

void AlarmManager::snooze(int index, int minutes) {
    snoozeIdx = index;
    snoozeAt = time(nullptr) + minutes * 60;
}

// --- TEMPORIZADOR ---
// Corriendo: termina en timerEndMs (millis). En pausa: quedan timerLeftMs.

void AlarmManager::timerAdd(int minutes) {
    if (timerRunning) timerEndMs += minutes * 60000UL;
    else timerLeftMs += minutes * 60000UL;
    saveChrono();
}

void AlarmManager::startTimer(unsigned long seconds) {
    timerRunning = true;
    timerLeftMs = 0;
    timerEndMs = millis() + seconds * 1000UL;
    saveChrono();
}

int AlarmManager::findAlarm(int h, int m) {
    for (int i = 0; i < alarmCount; i++) {
        if (alarmList[i].hour == h && alarmList[i].minute == m) return i;
    }
    return -1;
}

void AlarmManager::toggleTimer() {
    if (timerRunning) {
        timerLeftMs = getTimerRemaining();
        timerRunning = false;
    } else if (timerLeftMs > 0) {
        timerEndMs = millis() + timerLeftMs;
        timerRunning = true;
    }
    saveChrono();
}

void AlarmManager::resetTimer() {
    timerRunning = false;
    timerLeftMs = 0;
    saveChrono();
}

bool AlarmManager::isTimerRunning() { return timerRunning; }

unsigned long AlarmManager::getTimerRemaining() {
    if (!timerRunning) return timerLeftMs;
    long left = (long)(timerEndMs - millis());
    return left > 0 ? left : 0;
}

// Las alarmas se repiten todos los días a la misma hora mientras estén activas.
void AlarmManager::checkAlarms() {
    if (timerRunning && getTimerRemaining() == 0) {
        timerRunning = false;
        timerLeftMs = 0;
        saveChrono();
        DisplayUI::ringScreen("TEMPORIZADOR", "00:00", false);
    }

    if (snoozeAt != 0 && time(nullptr) >= snoozeAt) {
        snoozeAt = 0;
        if (snoozeIdx >= 0 && snoozeIdx < alarmCount) DisplayUI::triggerAlarmRinging(snoozeIdx);
    }

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
    saveChrono();
}

void AlarmManager::resetChrono() {
    chronoRunning = false;
    chronoElapsedTime = 0;
    saveChrono();
}

bool AlarmManager::isChronoRunning() { return chronoRunning; }

unsigned long AlarmManager::getChronoTime() {
    return chronoRunning ? (millis() - chronoStartTime + chronoElapsedTime) : chronoElapsedTime;
}