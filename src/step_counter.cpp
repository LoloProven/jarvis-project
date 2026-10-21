#include "step_counter.h"
#include "config.h"
#include <Wire.h>
#include <Preferences.h>
#include "time.h"
#include "SensorQMI8658.hpp"

static SensorQMI8658 qmi;
static Preferences stepPrefs;
static bool sensorOk = false;

// steps = base + lo que cuenta el chip desde el último "vaciado". Cada minuto el
// contador del chip se suma a base y se pone en 0; base se guarda en la memoria.
static uint32_t base = 0;
static uint32_t chipCount = 0;
static int dayKey = -1;                     // año * 400 + día del año
static unsigned long lastRead = 0;
static unsigned long lastFold = 0;
static unsigned long lastSave = 0;
static bool dirty = false;

static const unsigned long READ_MS = 2000;
static const unsigned long FOLD_MS = 60000;
static const unsigned long SAVE_MS = 300000;

static int todayKey() {
    struct tm t;
    if (!getLocalTime(&t, 0)) return -1;
    return (t.tm_year + 1900) * 400 + t.tm_yday;
}

static void save() {
    stepPrefs.begin("steps", false);
    stepPrefs.putInt("day", dayKey);
    stepPrefs.putUInt("base", base);
    stepPrefs.end();
    dirty = false;
}

void StepCounter::init() {
    sensorOk = qmi.init(Wire, IIC_SDA, IIC_SCL, QMI8658_L_SLAVE_ADDRESS);
    if (sensorOk) {
        // Parámetros del ejemplo QMI8658_PedometerExample de SensorLib.
        qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_2G, SensorQMI8658::ACC_ODR_62_5Hz);
        qmi.enableAccelerometer();
        qmi.configPedometer(50,     // muestras para detectar un paso
                            200,    // pico a pico mínimo (mg)
                            100,    // pico mínimo (mg)
                            200,    // tiempo máximo entre pasos (muestras, ~4 s)
                            20,     // tiempo mínimo entre pasos (muestras)
                            10,     // pasos seguidos para empezar a contar
                            0,
                            4);     // actualiza el registro cada 4 pasos
        sensorOk = qmi.enablePedometer();
        qmi.clearPedometerCounter();
    }

    // Recupera el total de hoy si el reloj se reinició durante el día.
    stepPrefs.begin("steps", true);
    int savedDay = stepPrefs.getInt("day", -1);
    uint32_t savedBase = stepPrefs.getUInt("base", 0);
    stepPrefs.end();
    dayKey = todayKey();
    base = (savedDay == dayKey && dayKey >= 0) ? savedBase : 0;
}

void StepCounter::loop() {
    if (!sensorOk) return;
    unsigned long now = millis();
    if (now - lastRead < READ_MS) return;
    lastRead = now;

    // Medianoche (o la hora pasó a ser la real por NTP/RTC y cambió el día): de nuevo a 0.
    int today = todayKey();
    if (today >= 0 && today != dayKey) {
        dayKey = today;
        base = 0;
        qmi.clearPedometerCounter();
        chipCount = 0;
        save();
        return;
    }

    chipCount = qmi.getPedometerCounter();

    if (now - lastFold >= FOLD_MS && chipCount > 0) {
        lastFold = now;
        base += chipCount;
        chipCount = 0;
        qmi.clearPedometerCounter();
        dirty = true;
    }
    if (dirty && now - lastSave >= SAVE_MS) {
        lastSave = now;
        save();
    }
}

uint32_t StepCounter::steps() { return base + chipCount; }
float StepCounter::distanceKm() { return steps() * (STEP_LENGTH_CM / 100000.0f); }
bool StepCounter::available() { return sensorOk; }
