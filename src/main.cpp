#include <Arduino.h>
#include "power_manager.h"
#include "display_ui.h"
#include "touch_driver.h"
#include "alarm_manager.h"
#include "audio_manager.h"
#include "wifi_portal.h"
#include "time_sync.h"
#include "net_status.h"
#include "step_counter.h"

static unsigned long lastClockUpdate = 0;
static NetState lastNetState = NET_NO_WIFI;
static int lastBattery = -2;
static bool lastCharging = false;
static uint32_t lastSteps = 0;
static int lastSpeedTenths = 0;
// Después de despertar con un toque, se ignora la pantalla hasta que se suelte el dedo,
// así el toque que despierta no activa el botón que quedó debajo.
static bool ignoreUntilRelease = false;

void setup() {
    Serial.begin(115200);

    PowerManager::init();          // inicia Wire (I2C compartido por PMU, touch y audio)
    TouchDriver::init();
    DisplayUI::init();
    AlarmManager::init();
    AudioManager::init();

    // Zona horaria y hora de respaldo; tiene que ir antes de conectar el WiFi
    // para no perderse el evento de conexión.
    TimeSync::init();
    StepCounter::init();           // después de TimeSync: necesita la fecha para saber qué día es
    NetStatus::init();

    WiFiPortal::initSaved();
    DisplayUI::drawProfileMenu();
}

void loop() {
    // Esto corre siempre, con la pantalla encendida o apagada.
    PowerManager::checkButton();
    PowerManager::updateBattery();        // lee el AXP2101 cada 30 s
    StepCounter::loop();                  // los pasos se cuentan en su propia tarea
    TimeSync::loop();
    AlarmManager::checkAlarms();          // si suena con la pantalla apagada, la enciende

    if (DisplayUI::getCurrentScreen() == SCREEN_WIFI_CONFIG) {
        WiFiPortal::handlePortal();
    }

    int16_t tx, ty;
    bool touched = TouchDriver::getTouch(tx, ty);

    if (PowerManager::isScreenOff()) {
        if (touched) {
            PowerManager::turnScreenOn();  // el primer toque solo despierta
            ignoreUntilRelease = true;
        } else {
            delay(20);
        }
        return;                            // con la pantalla apagada no se dibuja nada
    }

    if (ignoreUntilRelease) {
        if (!touched) ignoreUntilRelease = false;
        touched = false;
    }

    if (touched) PowerManager::notifyActivity();
    PowerManager::checkInactivity();
    if (PowerManager::isScreenOff()) return;

    // El dashboard solo cambia entre "Conectado" y "Sin conexion", pero se sigue el
    // estado completo porque las pestañas dependen de si hay WiFi (VOZ).
    NetState net = NetStatus::get();
    if (net != lastNetState) {
        bool wifiChanged = (net == NET_NO_WIFI) != (lastNetState == NET_NO_WIFI);
        lastNetState = net;
        DisplayUI::refreshNetStatus(wifiChanged);
    }

    int batt = PowerManager::batteryPercent();
    bool charging = PowerManager::isCharging();
    uint32_t steps = StepCounter::steps();
    int speedTenths = (int)(StepCounter::speedKmh() * 10);   // cambia aunque no haya pasos nuevos (al frenar)
    if (batt != lastBattery || charging != lastCharging || steps != lastSteps || speedTenths != lastSpeedTenths) {
        lastBattery = batt;
        lastCharging = charging;
        lastSteps = steps;
        lastSpeedTenths = speedTenths;
        DisplayUI::refreshBottomBar();
    }

    if (DisplayUI::getCurrentScreen() == SCREEN_DASHBOARD && millis() - lastClockUpdate > 1000) {
        lastClockUpdate = millis();
        DisplayUI::drawHeaderClockOnly();
        if (AlarmManager::isChronoRunning()) {
            DisplayUI::refreshChronoDigits();   // solo los dígitos, y solo en la pestaña CRONO
        }
    }

    if (touched) {
        DisplayUI::handleTouch(tx, ty);
    }
}
