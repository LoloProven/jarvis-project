#include "time_sync.h"
#include "config.h"
#include "i2c_lock.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include "time.h"
#include "SensorPCF85063.hpp"

// Argentina: UTC-3 todo el año, sin horario de verano (formato POSIX: el signo va invertido).
static const char* TZ_ARGENTINA = "<-03>3";

// Reloj de tiempo real de la placa (PCF85063, como el ejemplo 03 de Waveshare). Sigue
// contando con el reloj apagado, así que al arrancar sin internet la hora es correcta.
// Guarda la hora local de Argentina (sin horario de verano, el desfase es fijo).
static SensorPCF85063 rtc;
static bool rtcOk = false;
static bool rtcValid = false;              // el RTC tenía una hora confiable al arrancar

static volatile bool synced = false;       // llegó la hora por NTP
static volatile bool pendingNtp = false;
static volatile bool pendingRtcWrite = false;

static void onTimeSynced(struct timeval* tv) {
    synced = true;
    pendingRtcWrite = true;                // se escribe desde loop(): el I2C no se toca desde otra tarea
}

// Bit OS (oscilador detenido) del registro de segundos: si está en 1, el RTC se quedó
// sin energía en algún momento y la hora que tiene no sirve.
static bool rtcOscillatorStopped() {
    Wire.beginTransmission(PCF85063_SLAVE_ADDRESS);
    Wire.write(0x04);
    if (Wire.endTransmission(false) != 0) return true;
    if (Wire.requestFrom((uint8_t)PCF85063_SLAVE_ADDRESS, (uint8_t)1) != 1) return true;
    return (Wire.read() & 0x80) != 0;
}

static void setFallbackTime() {
    struct tm tm = {};
    tm.tm_year = 2026 - 1900; tm.tm_mon = 8; tm.tm_mday = 17;
    tm.tm_hour = 0; tm.tm_min = 30; tm.tm_sec = 0;
    tm.tm_isdst = 0;
    timeval tv = { mktime(&tm), 0 };
    settimeofday(&tv, NULL);
}

void TimeSync::init() {
    setenv("TZ", TZ_ARGENTINA, 1);
    tzset();

    rtcOk = rtc.init(Wire, IIC_SDA, IIC_SCL);   // en SensorLib 0.2.x se llama init (el ejemplo usa begin)
    if (rtcOk && !rtcOscillatorStopped()) {
        RTC_DateTime dt = rtc.getDateTime();
        if (dt.year >= 2025 && dt.year < 2100) {
            struct tm tm = {};
            tm.tm_year = dt.year - 1900; tm.tm_mon = dt.month - 1; tm.tm_mday = dt.day;
            tm.tm_hour = dt.hour; tm.tm_min = dt.minute; tm.tm_sec = dt.second;
            tm.tm_isdst = 0;
            timeval tv = { mktime(&tm), 0 };
            settimeofday(&tv, NULL);
            rtcValid = true;
        }
    }
    // Sin RTC confiable: hora de respaldo hasta que llegue la de NTP (se muestra en coral).
    if (!rtcValid) setFallbackTime();

    sntp_set_time_sync_notification_cb(onTimeSynced);

    // El WiFi puede conectarse al arrancar o más tarde desde el portal: cada vez que
    // obtiene IP se pide la hora. El evento corre en otra tarea, así que solo marca
    // el pedido y configTzTime se llama desde loop().
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        pendingNtp = true;
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
}

void TimeSync::loop() {
    if (pendingNtp) {
        pendingNtp = false;
        configTzTime(TZ_ARGENTINA, "pool.ntp.org", "time.google.com", "ar.pool.ntp.org");
    }
    if (pendingRtcWrite && rtcOk) {
        pendingRtcWrite = false;
        struct tm now;
        if (getLocalTime(&now, 0)) {
            // setDateTime también borra el bit OS, así la próxima vez se confía en el RTC.
            I2CLock lock;                    // el I2C se comparte con la tarea de pasos
            rtc.setDateTime(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
                            now.tm_hour, now.tm_min, now.tm_sec);
            rtcValid = true;
        }
    }
}

// Confiable si llegó por NTP o si el RTC tenía una hora válida al arrancar.
bool TimeSync::isSynced() { return synced || rtcValid; }
