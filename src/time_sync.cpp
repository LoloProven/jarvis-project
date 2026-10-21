#include "time_sync.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include "time.h"

// Argentina: UTC-3 todo el año, sin horario de verano (formato POSIX: el signo va invertido).
static const char* TZ_ARGENTINA = "<-03>3";

static volatile bool synced = false;
static volatile bool pendingNtp = false;

static void onTimeSynced(struct timeval* tv) {
    synced = true;
}

void TimeSync::init() {
    setenv("TZ", TZ_ARGENTINA, 1);
    tzset();

    // Hora de respaldo hasta que llegue la de NTP (se muestra marcada como no confiable).
    struct tm tm = {};
    tm.tm_year = 2026 - 1900; tm.tm_mon = 8; tm.tm_mday = 17;
    tm.tm_hour = 0; tm.tm_min = 30; tm.tm_sec = 0;
    tm.tm_isdst = 0;
    timeval tv = { mktime(&tm), 0 };
    settimeofday(&tv, NULL);

    sntp_set_time_sync_notification_cb(onTimeSynced);

    // El WiFi puede conectarse al arrancar o más tarde desde el portal: cada vez que
    // obtiene IP se pide la hora. El evento corre en otra tarea, así que solo marca
    // el pedido y configTzTime se llama desde loop().
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        pendingNtp = true;
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
}

void TimeSync::loop() {
    if (!pendingNtp) return;
    pendingNtp = false;
    configTzTime(TZ_ARGENTINA, "pool.ntp.org", "time.google.com", "ar.pool.ntp.org");
}

bool TimeSync::isSynced() { return synced; }
