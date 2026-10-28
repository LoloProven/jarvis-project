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
#include "battery_log.h"
#include "notifications.h"
#include "weather.h"
#include <WiFi.h>

static unsigned long lastClockUpdate = 0;
static NetState lastNetState = NET_NO_WIFI;
// Después de despertar con un toque, se ignora la pantalla hasta que se suelte el dedo,
// así el toque que despierta no activa el botón que quedó debajo.
static bool ignoreUntilRelease = false;
// Hora grande al levantar la muñeca: cuánto se muestra antes de apagarse sola.
static const unsigned long GLANCE_MS = 5000;
static unsigned long glanceUntil = 0;
// Notificación en pantalla.
static const unsigned long NOTICE_MS = 8000;
static const unsigned long CALL_MS = 30000;
static bool noticeShown = false;
static bool noticeCall = false;
static unsigned long noticeUntil = 0;

void setup() {
    Serial.begin(115200);

    PowerManager::init();          // inicia Wire (I2C compartido por PMU, touch y audio)
    TouchDriver::init();
    Settings::load();              // brillo y volumen guardados (antes de la pantalla)
    BatteryLog::init();
    pinMode(JARVIS_BUTTON, INPUT_PULLUP);
    DisplayUI::init();
    AudioManager::init();

    // Zona horaria y hora de respaldo; tiene que ir antes de conectar el WiFi
    // para no perderse el evento de conexión.
    TimeSync::init();
    AlarmManager::init();          // después de TimeSync: el crono recuperado necesita la hora real
    StepCounter::init();           // después de TimeSync: necesita la fecha para saber qué día es
    NetStatus::init();
    MediaRemote::init();           // Bluetooth: control de la música del celular

    WiFiPortal::initSaved();
    DisplayUI::drawProfileMenu();
}

void loop() {
    DisplayUI::flush();                       // lo dibujado en la vuelta anterior, a la pantalla
    BatteryLog::tick();                       // cuánto tiempo en cada estado (y cada 15 min, el %)

    // Modo dormir: duerme de a 1 s y entre medio solo mira PWR y las alarmas.
    if (PowerManager::isSleeping()) {
        PowerManager::WakeReason why = PowerManager::sleepTick();
        StepCounter::pollWhileSleeping();     // los pasos se siguen contando durmiendo
        StepCounter::takeWristRaise();        // durmiendo no hay vistazo de la muñeca
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
    Weather::loop();                      // clima para INICIO (cada 30 min, con WiFi)
    AlarmManager::checkAlarms();          // si suena con la pantalla apagada, la enciende

    if (DisplayUI::getCurrentScreen() == SCREEN_WIFI_CONFIG) {
        WiFiPortal::handlePortal();
    }

    int16_t tx, ty;
    bool touched = TouchDriver::getTouch(tx, ty);

    // Notificación del iPhone: prende la pantalla y la muestra, sin sonido (así lo prefiere
    // el usuario). Se cierra tocando, a los NOTICE_MS o, si es una llamada, cuando deja de
    // sonar en el iPhone.
    Notifications::Notice notice;
    if (Notifications::take(notice) && DisplayUI::getCurrentScreen() != SCREEN_WIFI_CONFIG) {
        PowerManager::turnScreenOn(false);
        DisplayUI::drawNotification(notice.app, notice.title, notice.message, notice.call);
        noticeShown = true;
        noticeCall = notice.call;
        noticeUntil = millis() + (notice.call ? CALL_MS : NOTICE_MS);
        ignoreUntilRelease = touched;
    }
    if (noticeShown) {
        if (ignoreUntilRelease) {
            if (!touched) ignoreUntilRelease = false;
            touched = false;
        }
        bool close = (long)(millis() - noticeUntil) > 0 || (noticeCall && !Notifications::callActive());
        if (touched) {
            if (noticeCall && DisplayUI::hitRejectButton(tx, ty)) Notifications::rejectCall();
            TouchDriver::waitForRelease();
            close = true;
        }
        if (close) {
            noticeShown = false;
            PowerManager::notifyActivity();    // queda prendida el tiempo normal y se apaga sola
            DisplayUI::refreshCurrentScreen();
            lastNetState = NetStatus::get();
        }
        return;
    }

    // Diagnóstico por USB: cada cambio de BOOT y un "sigo vivo" cada 30 s (BOOT dejó de
    // responder dos veces en la calle, sin dejar rastro).
    static int lastBoot = -1;
    static unsigned long lastAlive = 0;
    int bootNow = digitalRead(JARVIS_BUTTON);
    if (bootNow != lastBoot) {
        lastBoot = bootNow;
        Serial.printf("[boot] %s (pantalla %d, apagada %d, wifi %d)\n", bootNow == LOW ? "apretado" : "suelto",
                      (int)DisplayUI::getCurrentScreen(), PowerManager::isScreenOff(), WiFi.status() == WL_CONNECTED);
    }
    if (millis() - lastAlive > 30000) {
        lastAlive = millis();
        Serial.printf("[vivo] boot=%d pantalla=%d apagada=%d wifi=%d memoria=%u\n", bootNow,
                      (int)DisplayUI::getCurrentScreen(), PowerManager::isScreenOff(), WiFi.status() == WL_CONNECTED,
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }

    // BOOT apretado = hablarle a JARVIS (desde cualquier pestaña, también con la pantalla
    // apagada). Graba mientras se mantiene apretado.
    if (digitalRead(JARVIS_BUTTON) == LOW && DisplayUI::getCurrentScreen() == SCREEN_DASHBOARD) {
        PowerManager::turnScreenOn();
        DisplayUI::talkToJarvis([]() { return digitalRead(JARVIS_BUTTON) == LOW; });
        PowerManager::notifyActivity();        // la respuesta queda en pantalla el tiempo normal
        ignoreUntilRelease = true;
        return;
    }

    if (PowerManager::isScreenOff()) {
        if (touched) {
            PowerManager::turnScreenOn();  // el primer toque solo despierta
            ignoreUntilRelease = true;
        } else if (StepCounter::takeWristRaise()) {
            PowerManager::glanceOn();      // levantó la muñeca: hora grande
            DisplayUI::drawBigClock();
            glanceUntil = millis() + GLANCE_MS;
        } else {
            delay(20);
        }
        return;                            // con la pantalla apagada no se dibuja nada
    }

    // Vistazo (hora grande): un toque entra al reloj normal; si no, se apaga solo.
    if (PowerManager::isGlance()) {
        if (touched) {
            PowerManager::turnScreenOn();  // completa el encendido y dibuja la pantalla actual
            ignoreUntilRelease = true;
        } else if ((long)(millis() - glanceUntil) > 0) {
            PowerManager::turnScreenOff();
        }
        return;
    }
    StepCounter::takeWristRaise();         // con la pantalla prendida no hace falta

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

    // Una vez por segundo: hora, batería, datos de INICIO (pasos, km, clima, alarma) y
    // cronómetro. Cada uno redibuja solo lo que cambió, así la pantalla no parpadea.
    if (DisplayUI::getCurrentScreen() == SCREEN_DASHBOARD && millis() - lastClockUpdate > 1000) {
        lastClockUpdate = millis();
        DisplayUI::drawHeaderClockOnly();
        DisplayUI::refreshLiveData();         // batería y datos de INICIO
        DisplayUI::refreshMusicTab();
        if (AlarmManager::isChronoRunning() || AlarmManager::isTimerRunning()) {
            DisplayUI::refreshChronoDigits();   // solo los dígitos, y solo en la pestaña CRONO
        }
    }

    DisplayUI::musicTick();                    // botón de música que titiló: vuelve a celeste

    if (touched) {
        DisplayUI::handleTouch(tx, ty);
    }
}
