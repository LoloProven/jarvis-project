#include "step_counter.h"
#include "config.h"
#include <Wire.h>
#include <Preferences.h>
#include "time.h"
#include "SensorQMI8658.hpp"

// Pasos contados por software a partir del acelerómetro del QMI8658.
// (El podómetro interno del chip quedaba en 0 aunque se caminara.)
//
// Una tarea propia lee la aceleración ~50 veces por segundo, así no se pierden
// lecturas mientras el loop dibuja o atiende el touch. Se calcula el módulo |a|
// (en g; quieto = 1), se le resta un promedio lento (la gravedad) y se busca el pico
// de cada balanceo: un paso es un pico que supera PEAK_G, después de haber bajado de
// VALLEY_G, con un ritmo parecido al de los pasos anteriores. Para no contar
// movimientos sueltos, recién se empieza a contar con START_STEPS pasos seguidos.
//
// Ajustado probando en el reloj: con 0,12 g y 4 pasos de entrada contaba moviendo un
// poco el brazo; con 0,25 g, ritmo 0,6-1,6x y 8 pasos de entrada no contaba caminando.

static SensorQMI8658 qmi;
static Preferences stepPrefs;
static bool sensorOk = false;

static const uint32_t      SAMPLE_MS   = 20;     // ~50 lecturas por segundo
static const float         PEAK_G      = 0.18f;  // cuánto tiene que subir |a| sobre el promedio
static const float         VALLEY_G    = -0.08f; // y cuánto tiene que bajar antes del próximo paso
static const unsigned long MIN_STEP_MS = 280;    // más rápido que esto no es un paso
static const unsigned long MAX_STEP_MS = 2000;   // más lento que esto corta la racha
static const float         RHYTHM_MIN  = 0.5f;   // cada paso tarda entre la mitad y el doble
static const float         RHYTHM_MAX  = 2.0f;   // del ritmo promedio de la racha
static const int           START_STEPS = 6;      // pasos seguidos antes de empezar a contar
static const unsigned long SAVE_MS     = 300000;
static const unsigned long SPEED_WINDOW_MS = 10000;  // velocidad con los pasos de los últimos 10 s

static volatile uint32_t stepCount = 0;
static int dayKey = -1;                     // año * 400 + día del año
static bool dirty = false;

static float gravity = 1.0f;                // promedio lento de |a|
static bool armed = true;                   // listo para detectar el próximo pico
static unsigned long lastStepMs = 0;
static float avgGap = 0;                    // ritmo promedio de la racha (ms entre pasos)
static int streak = 0;                      // pasos seguidos con ritmo de caminata

// Momentos de los últimos pasos contados, para calcular la velocidad.
static const int RECENT = 40;
static volatile unsigned long recentSteps[RECENT];
static volatile int recentHead = 0;

static int todayKey() {
    struct tm t;
    if (!getLocalTime(&t, 0)) return -1;
    return (t.tm_year + 1900) * 400 + t.tm_yday;
}

static void save() {
    stepPrefs.begin("steps", false);
    stepPrefs.putInt("day", dayKey);
    stepPrefs.putUInt("base", stepCount);
    stepPrefs.end();
    dirty = false;
}

static void countStep(unsigned long now) {
    stepCount = stepCount + 1;     // (solo esta tarea escribe; el loop solo lee)
    recentSteps[recentHead] = now;
    recentHead = (recentHead + 1) % RECENT;
    dirty = true;
}

static void onSample(float mag, unsigned long now) {
    gravity += (mag - gravity) * 0.02f;          // se adapta en ~1 s
    float d = mag - gravity;

    if (armed && d > PEAK_G) {
        armed = false;                           // hasta que baje por debajo de VALLEY_G
        unsigned long gap = now - lastStepMs;
        if (gap < MIN_STEP_MS) return;           // rebote del mismo paso
        lastStepMs = now;
        bool steady = avgGap == 0 || (gap >= avgGap * RHYTHM_MIN && gap <= avgGap * RHYTHM_MAX);
        if (gap > MAX_STEP_MS || !steady) {      // arranca una racha nueva
            streak = 1;
            avgGap = 0;
            return;
        }
        avgGap = (avgGap == 0) ? gap : avgGap * 0.7f + gap * 0.3f;
        streak++;
        if (streak == START_STEPS) {             // se suman también los de la entrada
            for (int i = 0; i < START_STEPS; i++) countStep(now);
        } else if (streak > START_STEPS) {
            countStep(now);
        }
    } else if (!armed && d < VALLEY_G) {
        armed = true;                            // balanceo completo: listo para el próximo paso
    }
}

static void stepTask(void*) {
    TickType_t wake = xTaskGetTickCount();
    unsigned long lastDayCheck = 0, lastSave = 0;
    for (;;) {
        unsigned long now = millis();
        float ax, ay, az;
        if (qmi.getAccelerometer(ax, ay, az)) {
            onSample(sqrtf(ax * ax + ay * ay + az * az), now);
        }

        // Medianoche (o la hora pasó a ser la real por NTP/RTC y cambió el día): de nuevo a 0.
        if (now - lastDayCheck >= 2000) {
            lastDayCheck = now;
            int today = todayKey();
            if (today >= 0 && today != dayKey) {
                dayKey = today;
                stepCount = 0;
                streak = 0;
                save();
            }
        }
        if (dirty && now - lastSave >= SAVE_MS) {
            lastSave = now;
            save();
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(SAMPLE_MS));
    }
}

void StepCounter::init() {
    sensorOk = qmi.init(Wire, IIC_SDA, IIC_SCL, QMI8658_L_SLAVE_ADDRESS);
    if (sensorOk) {
        qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_125Hz);
        sensorOk = qmi.enableAccelerometer();
    }

    // Recupera el total de hoy si el reloj se reinició durante el día.
    stepPrefs.begin("steps", true);
    int savedDay = stepPrefs.getInt("day", -1);
    uint32_t savedSteps = stepPrefs.getUInt("base", 0);
    stepPrefs.end();
    dayKey = todayKey();
    stepCount = (savedDay == dayKey && dayKey >= 0) ? savedSteps : 0;

    // Núcleo 0, como las tareas de red; el Wire de Arduino serializa el acceso al I2C
    // entre tareas, así que puede convivir con el touch y el PMU del loop.
    if (sensorOk) xTaskCreatePinnedToCore(stepTask, "steps", 4096, NULL, 2, NULL, 0);
}

void StepCounter::loop() {
    // Todo el trabajo lo hace stepTask; se deja por si hace falta algo desde el loop.
}

uint32_t StepCounter::steps() { return stepCount; }
float StepCounter::distanceKm() { return stepCount * (STEP_LENGTH_CM / 100000.0f); }
bool StepCounter::available() { return sensorOk; }

// Velocidad estimada: pasos de los últimos 10 s * largo del paso. 0 si estás quieto.
float StepCounter::speedKmh() {
    unsigned long now = millis();
    int n = 0;
    for (int i = 0; i < RECENT; i++) {
        unsigned long t = recentSteps[i];
        if (t != 0 && now - t <= SPEED_WINDOW_MS) n++;
    }
    if (n < 3) return 0;
    float stepsPerSec = n / (SPEED_WINDOW_MS / 1000.0f);
    return stepsPerSec * (STEP_LENGTH_CM / 100.0f) * 3.6f;
}
