#include <Arduino.h>
#include "power_manager.h"
#include "display_ui.h"
#include "touch_driver.h"
#include "alarm_manager.h"
#include "audio_manager.h"
#include "wifi_portal.h"
#include "time_sync.h"

static unsigned long lastClockUpdate = 0;

void setup() {
    Serial.begin(115200);

    PowerManager::init();
    DisplayUI::init();
    AlarmManager::init();
    AudioManager::init();

    // Zona horaria y hora de respaldo; tiene que ir antes de conectar el WiFi
    // para no perderse el evento de conexión.
    TimeSync::init();

    WiFiPortal::initSaved();
    DisplayUI::drawProfileMenu();
}

void loop() {
    PowerManager::checkButton();
    TimeSync::loop();
    AlarmManager::checkAlarms();

    if (PowerManager::isScreenOff()) {
        delay(50);
        return;
    }

    if (DisplayUI::getCurrentScreen() == SCREEN_WIFI_CONFIG) {
        WiFiPortal::handlePortal();
    }

    if (DisplayUI::getCurrentScreen() == SCREEN_DASHBOARD && millis() - lastClockUpdate > 1000) {
        lastClockUpdate = millis();
        DisplayUI::drawHeaderClockOnly();
        if (AlarmManager::isChronoRunning()) {
            DisplayUI::drawDashboard();
        }
    }

    int16_t tx, ty;
    if (TouchDriver::getTouch(tx, ty)) {
        DisplayUI::handleTouch(tx, ty);
    }
}