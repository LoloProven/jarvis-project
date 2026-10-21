#include "power_manager.h"
#include "config.h"
#include "display_ui.h"
#include "i2c_lock.h"
#include "net_status.h"
#include "wifi_portal.h"
#include "touch_driver.h"
#include "media_remote.h"
#include "settings.h"
#include <esp_sleep.h>
#include <driver/gpio.h>

XPowersPMU PowerManager::PMU;
bool PowerManager::screenOff = false;
bool PowerManager::sleeping = false;
unsigned long PowerManager::lastActivity = 0;

void PowerManager::init() {
    Wire.begin(PMU_SDA, PMU_SCL);
    if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
        // Las tensiones de las salidas (DC1, ALDO1-4, BLDO1-2) se dejan como vienen de
        // fábrica, igual que los ejemplos de Waveshare, que no las tocan. Antes se forzaban
        // todas a 3,3 V, pero no todas son de 3,3 V (ALDO4, por ejemplo, viene en 1,8 V).
        PMU.enableBattVoltageMeasure();
        PMU.enableBattDetection();          // necesario para el porcentaje (como el ejemplo 05)

        // Botón PWR: toque corto = pantalla; 1,5 s = modo dormir; 4 s = apagado total
        // por hardware (ese no pasa por el software).
        PMU.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
        PMU.setIrqLevelTime(XPOWERS_AXP2101_IRQ_TIME_1S5);
        PMU.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ);
        PMU.clearIrqStatus();
    }
    lastActivity = millis();
}

// Con la pantalla apagada el reloj sigue andando (hora, pasos, alarmas), pero gastando
// menos: el procesador baja a 80 MHz, no se revisa internet y, pasados WIFI_OFF_AFTER_MS,
// se apaga el WiFi. Antes seguía todo a pleno y la batería no llegaba a la mañana.
static unsigned long screenOffSince = 0;

// Solo se apaga el panel. No se tocan las salidas del AXP2101: DC1 alimenta al
// sistema, y cortarlo apagaba o reiniciaba el reloj entero.
// "Vistazo": al levantar la muñeca se prende solo el panel para mostrar la hora grande,
// sin acelerar el procesador ni reconectar el WiFi (gasta poco). Si después se toca la
// pantalla, turnScreenOn() completa el encendido.
static bool glance = false;

void PowerManager::turnScreenOff() {
    if (screenOff) return;
    screenOff = true;
    if (!glance) screenOffSince = millis();  // un vistazo no reinicia la cuenta para dormir
    glance = false;
    DisplayUI::getGFX()->displayOff();
    NetStatus::setPaused(true);
    setCpuFrequencyMhz(80);
}

void PowerManager::glanceOn() {
    if (!screenOff || sleeping) return;
    lastActivity = millis();
    screenOff = false;
    glance = true;
    DisplayUI::getGFX()->displayOn();
    Settings::applyBrightness();
}

bool PowerManager::isGlance() { return glance; }

void PowerManager::turnScreenOn(bool redraw) {
    lastActivity = millis();
    if (sleeping) {                          // prender la pantalla siempre sale del modo dormir
        sleeping = false;
        MediaRemote::init();                 // el Bluetooth se apagó al dormir
    }
    if (glance) {                            // venía de un vistazo: el panel ya está prendido
        glance = false;
        setCpuFrequencyMhz(240);
        NetStatus::setPaused(false);
        WiFiPortal::resume();
        if (redraw) DisplayUI::refreshCurrentScreen();
        return;
    }
    if (!screenOff) return;
    screenOff = false;
    setCpuFrequencyMhz(240);
    NetStatus::setPaused(false);
    WiFiPortal::resume();
    DisplayUI::getGFX()->displayOn();
    Settings::applyBrightness();             // el panel puede volver con el brillo de fábrica
    if (redraw) DisplayUI::refreshCurrentScreen();   // con la hora y el cronómetro al día
}

void PowerManager::lowPowerTick() {
    if (!screenOff || sleeping) return;
    if (DisplayUI::getCurrentScreen() == SCREEN_WIFI_CONFIG) return;   // el portal necesita WiFi
    unsigned long off = millis() - screenOffSince;
    if (off > SLEEP_AFTER_MS) enterSleep();
    else if (off > WIFI_OFF_AFTER_MS && !WiFiPortal::isSuspended()) WiFiPortal::suspend();
}

// --- MODO DORMIR ---
// Sueño liviano del ESP32: gasta muy poco, pero no se reinicia (hora, cronómetro y
// alarmas siguen). Se despierta solo cada segundo para mirar el botón PWR y las
// alarmas, y al instante si se toca la pantalla o se aprieta BOOT.
// (Con el apagado total de 4 s no hay corriente y ninguna alarma puede sonar.)
static const gpio_num_t BOOT_BUTTON = GPIO_NUM_0;

void PowerManager::enterSleep() {
    if (sleeping) return;
    if (DisplayUI::getCurrentScreen() == SCREEN_WIFI_CONFIG) return;   // el portal necesita WiFi
    if (!screenOff) {
        DisplayUI::showSleepMessage();           // aviso breve antes de apagar
        delay(1200);
    }
    turnScreenOff();
    WiFiPortal::suspend();
    MediaRemote::stop();                         // con Bluetooth el chip no puede dormir
    pinMode(BOOT_BUTTON, INPUT_PULLUP);
    sleeping = true;
}

PowerManager::WakeReason PowerManager::sleepTick() {
    // Con el Bluetooth activo el sueño liviano del chip se cuelga (el controlador BLE
    // precompilado no soporta dormir) y el reloj no se podía despertar. En ese caso se
    // usa un "sueño suave": se espera 1 s mirando el touch y BOOT, sin dormir el chip.
    // Gasta más, pero se despierta siempre.
    if (MediaRemote::isEnabled()) {
        int16_t tx, ty;
        for (int i = 0; i < 20; i++) {
            if (TouchDriver::getTouch(tx, ty)) { turnScreenOn(); return WAKE_TOUCH; }
            if (digitalRead(BOOT_BUTTON) == LOW) { turnScreenOn(); return WAKE_BUTTON; }
            delay(50);
        }
        checkButton();
        return sleeping ? WAKE_NONE : WAKE_BUTTON;
    }
    {
        I2CLock lock;                            // que no quede ninguna lectura I2C a medias
        detachInterrupt(digitalPinToInterrupt(TP_INT));
        gpio_wakeup_enable((gpio_num_t)TP_INT, GPIO_INTR_LOW_LEVEL);   // toque en la pantalla
        gpio_wakeup_enable(BOOT_BUTTON, GPIO_INTR_LOW_LEVEL);             // botón BOOT
        esp_sleep_enable_gpio_wakeup();
        esp_sleep_enable_timer_wakeup(1000000);                        // 1 s
        esp_light_sleep_start();
        gpio_wakeup_disable((gpio_num_t)TP_INT);
        gpio_wakeup_disable(BOOT_BUTTON);
    }
    TouchDriver::init();                         // vuelve a enganchar la interrupción del touch

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
        bool boot = digitalRead(BOOT_BUTTON) == LOW;
        turnScreenOn();
        return boot ? WAKE_BUTTON : WAKE_TOUCH;
    }
    checkButton();                               // PWR (corto o largo) también despierta
    return sleeping ? WAKE_NONE : WAKE_BUTTON;
}

void PowerManager::checkButton() {
    bool shortPress, longPress;
    {
        I2CLock lock;                        // el I2C se comparte con la tarea de pasos
        // getIrqStatus() actualiza los registros internos de la librería
        PMU.getIrqStatus();
        shortPress = PMU.isPekeyShortPressIrq();
        longPress = PMU.isPekeyLongPressIrq();
        PMU.clearIrqStatus();
    }
    // Protección extra: dos toques de PWR a menos de 400 ms no son de una persona; así
    // una lectura mala nunca puede dejar la pantalla prendiendo y apagando sin parar.
    static unsigned long lastPress = 0;
    if (!(shortPress || longPress) || millis() - lastPress <= 400) return;
    lastPress = millis();

    if (sleeping) turnScreenOn();            // cualquier toque de PWR despierta
    else if (longPress) enterSleep();        // 1,5 s: modo dormir
    else if (screenOff) turnScreenOn();
    else turnScreenOff();
}

void PowerManager::checkInactivity() {
    if (!screenOff && millis() - lastActivity > SCREEN_TIMEOUT_MS) {
        turnScreenOff();
    }
}

static const unsigned long BATTERY_READ_MS = 30000;
static int battPercent = -1;
static bool battCharging = false;
static unsigned long lastBatteryRead = 0;

void PowerManager::updateBattery() {
    if (lastBatteryRead != 0 && millis() - lastBatteryRead < BATTERY_READ_MS) return;
    lastBatteryRead = millis();
    I2CLock lock;                            // el I2C se comparte con la tarea de pasos
    battPercent = PMU.isBatteryConnect() ? PMU.getBatteryPercent() : -1;
    if (battPercent > 100) battPercent = 100;
    battCharging = PMU.isCharging();
}

int PowerManager::batteryPercent() { return battPercent; }
bool PowerManager::isCharging() { return battCharging; }

void PowerManager::notifyActivity() { lastActivity = millis(); }
bool PowerManager::isScreenOff() { return screenOff; }
bool PowerManager::isSleeping() { return sleeping; }
XPowersPMU& PowerManager::getPMU() { return PMU; }
