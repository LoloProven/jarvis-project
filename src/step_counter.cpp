#include "step_counter.h"
#include "config.h"
#include "i2c_lock.h"
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

// El sensor guarda las mediciones en su memoria FIFO (hasta 128) a 62,5 por segundo y se
// leen de a tandas: cada 200 ms con el reloj despierto y cada 1 s en modo dormir. Así se
// cuentan pasos también durmiendo (antes, en modo dormir no se contaba nada).
static const uint32_t      READ_MS     = 200;    // cada cuánto se vacía la FIFO estando despierto
static const unsigned long SAMPLE_PERIOD_MS = 16; // 62,5 Hz
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
static volatile float distanceM = 0;        // metros del día (cada paso suma su largo)
static int dayKey = -1;                     // año * 400 + día del año
static bool dirty = false;

// Largo del paso según el ritmo: caminando lento el paso es corto y trotando es largo.
// stride (cm) = STRIDE_CM_PER_SPM * pasos por minuto, entre STRIDE_MIN_CM y STRIDE_MAX_CM.
// Calibrado con Strava: 8056 pasos en 63 min (128 pasos/min) = 7,67 km, o sea ~95 cm por
// paso -> 95 / 128 = 0,74 cm por cada paso/min. Antes era fijo en 70 cm y daba la mitad.
static const float STRIDE_CM_PER_SPM = 0.74f;
static const float STRIDE_MIN_CM = 55, STRIDE_MAX_CM = 140;

static float strideMeters(float avgGapMs) {
    if (avgGapMs <= 0) return STEP_LENGTH_CM / 100.0f;
    float spm = 60000.0f / avgGapMs;
    float cm = constrain(STRIDE_CM_PER_SPM * spm, STRIDE_MIN_CM, STRIDE_MAX_CM);
    return cm / 100.0f;
}

static float gravity = 1.0f;                // promedio lento de |a|
static bool armed = true;                   // listo para detectar el próximo pico
static unsigned long lastStepMs = 0;
static float avgGap = 0;                    // ritmo promedio de la racha (ms entre pasos)
static int streak = 0;                      // pasos seguidos con ritmo de caminata

// Momentos de los últimos pasos contados, para calcular la velocidad.
static const int RECENT = 40;
static volatile unsigned long recentSteps[RECENT];
static volatile int recentHead = 0;

// Momentos y largos de los últimos pasos, para la velocidad.
static volatile float recentStride[RECENT];

static int todayKey() {
    struct tm t;
    if (!getLocalTime(&t, 0)) return -1;
    return (t.tm_year + 1900) * 400 + t.tm_yday;
}

// Sesión del cronómetro (ver más abajo). Se guarda junto con los pasos del día, así sobrevive
// a un apagado (PWR 4 s) o un reinicio: lo acumulado, si corre y desde dónde cuenta.
static uint32_t sessAccSteps = 0, sessStartSteps = 0;
static float sessAccMeters = 0, sessStartMeters = 0;
static bool sessRunning = false;
static volatile bool saveSoon = false;     // la sesión cambió: guardar en la próxima vuelta

static void save() {
    stepPrefs.begin("steps", false);
    stepPrefs.putInt("day", dayKey);
    stepPrefs.putUInt("base", stepCount);
    stepPrefs.putFloat("metros", distanceM);
    stepPrefs.putBool("s_on", sessRunning);
    stepPrefs.putUInt("s_acc", sessAccSteps);
    stepPrefs.putFloat("s_accm", sessAccMeters);
    stepPrefs.putUInt("s_ini", sessStartSteps);
    stepPrefs.putFloat("s_inim", sessStartMeters);
    stepPrefs.end();
    dirty = false;
    saveSoon = false;
}

static void countStep(unsigned long now) {
    float stride = strideMeters(avgGap);
    stepCount = stepCount + 1;
    distanceM = distanceM + stride;
    recentSteps[recentHead] = now;
    recentStride[recentHead] = stride;
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

// --- LEVANTAR LA MUÑECA ---
// Pantalla hacia arriba = z cerca de -1 g (así mide quieto sobre la mesa, boca arriba).
// Se dispara cuando pasa de otra posición (brazo colgando o caminando) a pantalla hacia
// arriba y se queda así WRIST_HOLD_MS. Quieto sobre la mesa no se dispara: no hay cambio.
static const unsigned long WRIST_HOLD_MS   = 250;
static const unsigned long WRIST_WINDOW_MS = 1500;  // el cambio tiene que ser reciente
static unsigned long lastNotFaceUp = 0, faceUpSince = 0;
static bool wristFired = false;                     // ya avisó en esta "pantalla hacia arriba"
static volatile bool wristRaised = false;

static void wristCheck(float ax, float ay, float az, unsigned long now) {
    bool faceUp = az < -0.75f && fabsf(ax) < 0.5f && fabsf(ay) < 0.5f;
    if (!faceUp) {
        lastNotFaceUp = now;
        faceUpSince = 0;
        wristFired = false;
        return;
    }
    if (faceUpSince == 0) faceUpSince = now;
    if (!wristFired && now - faceUpSince >= WRIST_HOLD_MS &&
        faceUpSince - lastNotFaceUp < WRIST_WINDOW_MS) {
        wristFired = true;
        wristRaised = true;
    }
}

bool StepCounter::takeWristRaise() {
    if (!wristRaised) return false;
    wristRaised = false;
    return true;
}

// Vacía la FIFO del sensor y procesa todas las mediciones. Se llama desde la tarea (reloj
// despierto) y desde el loop en modo dormir; el I2CLock (recursivo) evita que se pisen.
// La medición i de n se tomó hace (n-1-i) períodos.
static __IMUdata fifoBuf[128];

static void drainFifo() {
    I2CLock lock;
    uint16_t n = qmi.readFromFifo(fifoBuf, 128, nullptr, 0);
    unsigned long now = millis();
    for (uint16_t i = 0; i < n; i++) {
        unsigned long t = now - (unsigned long)(n - 1 - i) * SAMPLE_PERIOD_MS;
        float ax = fifoBuf[i].x, ay = fifoBuf[i].y, az = fifoBuf[i].z;
        onSample(sqrtf(ax * ax + ay * ay + az * az), t);
        wristCheck(ax, ay, az, t);
    }
}

static void dayAndSave() {
    static unsigned long lastDayCheck = 0, lastSave = 0;
    unsigned long now = millis();
    // Medianoche (o la hora pasó a ser la real por NTP/RTC y cambió el día): de nuevo a 0.
    if (now - lastDayCheck >= 2000) {
        lastDayCheck = now;
        int today = todayKey();
        if (today >= 0 && today != dayKey) {
            // Sesión corriendo a la medianoche: lo caminado hasta ahora pasa a lo acumulado y
            // sigue contando desde 0 con el día nuevo.
            if (sessRunning) {
                if (stepCount >= sessStartSteps) sessAccSteps += stepCount - sessStartSteps;
                if (distanceM >= sessStartMeters) sessAccMeters += distanceM - sessStartMeters;
                sessStartSteps = 0;
                sessStartMeters = 0;
            }
            dayKey = today;
            stepCount = 0;
            distanceM = 0;
            streak = 0;
            save();
        }
    }
    // Con la sesión corriendo se guarda cada minuto (un apagado pierde a lo sumo eso).
    unsigned long every = sessRunning ? 60000UL : SAVE_MS;
    if (saveSoon || (dirty && now - lastSave >= every)) {
        lastSave = now;
        save();
    }
}

static void stepTask(void*) {
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        drainFifo();
        dayAndSave();
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(READ_MS));
    }
}

void StepCounter::init() {
    sensorOk = qmi.init(Wire, IIC_SDA, IIC_SCL, QMI8658_L_SLAVE_ADDRESS);
    if (sensorOk) {
        qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_62_5Hz);
        sensorOk = qmi.enableAccelerometer();
        // Modo "stream": guarda las últimas 128 mediciones (2 s a 62,5 Hz).
        if (sensorOk) qmi.configFIFO(SensorQMI8658::FIFO_MODE_STREAM, SensorQMI8658::FIFO_SAMPLES_128);
    }

    // Recupera el total de hoy si el reloj se reinició durante el día.
    stepPrefs.begin("steps", true);
    int savedDay = stepPrefs.getInt("day", -1);
    uint32_t savedSteps = stepPrefs.getUInt("base", 0);
    float savedMeters = stepPrefs.getFloat("metros", 0);
    bool savedOn = stepPrefs.getBool("s_on", false);
    sessAccSteps = stepPrefs.getUInt("s_acc", 0);
    sessAccMeters = stepPrefs.getFloat("s_accm", 0);
    sessStartSteps = stepPrefs.getUInt("s_ini", 0);
    sessStartMeters = stepPrefs.getFloat("s_inim", 0);
    stepPrefs.end();
    dayKey = todayKey();
    bool sameDay = (savedDay == dayKey && dayKey >= 0);
    stepCount = sameDay ? savedSteps : 0;
    distanceM = sameDay ? savedMeters : 0;
    // Sesión que corría cuando se apagó: sigue. Si cambió el día, lo del día guardado pasa a
    // lo acumulado y cuenta desde 0.
    sessRunning = savedOn;
    if (sessRunning && !sameDay) {
        if (savedSteps >= sessStartSteps) sessAccSteps += savedSteps - sessStartSteps;
        if (savedMeters >= sessStartMeters) sessAccMeters += savedMeters - sessStartMeters;
        sessStartSteps = 0;
        sessStartMeters = 0;
    }

    // Núcleo 0, como las tareas de red.
    if (sensorOk) xTaskCreatePinnedToCore(stepTask, "steps", 4096, NULL, 2, NULL, 0);
}

void StepCounter::loop() {
    // Todo el trabajo lo hace stepTask; se deja por si hace falta algo desde el loop.
}

// En modo dormir la tarea casi no corre: el loop vacía la FIFO en cada despertar (1 s).
void StepCounter::pollWhileSleeping() {
    if (!sensorOk) return;
    drainFifo();
    dayAndSave();
}

// --- SESIÓN (atada al cronómetro) ---
// Cuenta solo mientras el cronómetro corre: lo acumulado en tramos ya terminados más lo
// del tramo actual (desde el último INICIAR). Los datos del día no se tocan. Cada cambio
// se guarda enseguida (saveSoon).

void StepCounter::sessionStart() {
    if (sessRunning) return;
    sessStartSteps = stepCount;
    sessStartMeters = distanceM;
    sessRunning = true;
    saveSoon = true;
}

void StepCounter::sessionPause() {
    if (!sessRunning) return;
    sessAccSteps += stepCount - sessStartSteps;
    sessAccMeters += distanceM - sessStartMeters;
    sessRunning = false;
    saveSoon = true;
}

void StepCounter::sessionReset() {
    sessRunning = false;
    sessAccSteps = 0;
    sessAccMeters = 0;
    saveSoon = true;
}

uint32_t StepCounter::sessionSteps() {
    // (si pasó la medianoche durante la sesión, stepCount volvió a 0: se ignora ese tramo)
    uint32_t cur = (sessRunning && stepCount >= sessStartSteps) ? stepCount - sessStartSteps : 0;
    return sessAccSteps + cur;
}

float StepCounter::sessionKm() {
    float cur = (sessRunning && distanceM >= sessStartMeters) ? distanceM - sessStartMeters : 0;
    return (sessAccMeters + cur) / 1000.0f;
}

uint32_t StepCounter::steps() { return stepCount; }
float StepCounter::distanceKm() { return distanceM / 1000.0f; }
bool StepCounter::available() { return sensorOk; }

// true si hubo pasos en los últimos "ms" (para no dormirse mientras caminás).
bool StepCounter::walkedWithin(unsigned long ms) {
    unsigned long now = millis();
    for (int i = 0; i < RECENT; i++) {
        unsigned long t = recentSteps[i];
        if (t != 0 && now - t <= ms) return true;
    }
    return false;
}

// Velocidad estimada: largo de los pasos de los últimos 10 s. 0 si estás quieto.
float StepCounter::speedKmh() {
    unsigned long now = millis();
    int n = 0;
    float meters = 0;
    for (int i = 0; i < RECENT; i++) {
        unsigned long t = recentSteps[i];
        if (t != 0 && now - t <= SPEED_WINDOW_MS) { n++; meters += recentStride[i]; }
    }
    if (n < 3) return 0;
    return meters / (SPEED_WINDOW_MS / 1000.0f) * 3.6f;
}
