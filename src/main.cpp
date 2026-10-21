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
#include "media_remote.h"
#include "settings.h"

static unsigned long lastClockUpdate = 0;
static NetState lastNetState = NET_NO_WIFI;
// Después de despertar con un toque, se ignora la pantalla hasta que se suelte el dedo,
// así el toque que despierta no activa el botón que quedó debajo.
static bool ignoreUntilRelease = false;

void setup() {
    Serial.begin(115200);

    PowerManager::init();          // inicia Wire (I2C compartido por PMU, touch y audio)
    TouchDriver::init();
    Settings::load();              // brillo y volumen guardados (antes de la pantalla)
    DisplayUI::init();
    AlarmManager::init();
    AudioManager::init();

    // Zona horaria y hora de respaldo; tiene que ir antes de conectar el WiFi
    // para no perderse el evento de conexión.
    TimeSync::init();
    StepCounter::init();           // después de TimeSync: necesita la fecha para saber qué día es
    NetStatus::init();
    MediaRemote::init();           // Bluetooth: control de la música del celular

    WiFiPortal::initSaved();
    DisplayUI::drawProfileMenu();
}

void loop() {
    // Modo dormir: duerme de a 1 s y entre medio solo mira PWR y las alarmas.
    if (PowerManager::isSleeping()) {
        PowerManager::WakeReason why = PowerManager::sleepTick();
        AlarmManager::checkAlarms();          // si suena, prende la pantalla y sale del modo dormir
        if (why == PowerManager::WAKE_TOUCH) ignoreUntilRelease = true;   // ese toque solo despierta
        return;
    }

    // Esto corre siempre, con la pantalla encendida o apagada.
    PowerManager::checkButton();
    PowerManager::updateBattery();        // lee el AXP2101 cada 30 s
    PowerManager::lowPowerTick();         // pantalla apagada hace rato: apaga el WiFi
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

    // Cajita "Conectado" / "Sin conexion" del encabezado.
    NetState net = NetStatus::get();
    if (net != lastNetState) {
        bool wifiChanged = (net == NET_NO_WIFI) != (lastNetState == NET_NO_WIFI);
        lastNetState = net;
        DisplayUI::refreshNetStatus(wifiChanged);
    }

    // Una vez por segundo: hora, barra inferior (pasos, km, velocidad, batería) y
    // cronómetro. Cada uno redibuja solo lo que cambió, así la pantalla no parpadea.
    if (DisplayUI::getCurrentScreen() == SCREEN_DASHBOARD && millis() - lastClockUpdate > 1000) {
        lastClockUpdate = millis();
        DisplayUI::drawHeaderClockOnly();
        DisplayUI::refreshBottomBar();
        DisplayUI::refreshMusicTab();
        if (AlarmManager::isChronoRunning()) {
            DisplayUI::refreshChronoDigits();   // solo los dígitos, y solo en la pestaña CRONO
        }
    }

    DisplayUI::musicTick();                    // botón de música que titiló: vuelve a celeste

    if (touched) {
        DisplayUI::handleTouch(tx, ty);
    }
}
