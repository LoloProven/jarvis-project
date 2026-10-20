#include <Arduino.h>
#include "power_manager.h"
#include "display_ui.h"
#include "touch_driver.h"
#include "alarm_manager.h"
#include "audio_manager.h"
#include "wifi_portal.h"
#include "time.h"
#include <sys/time.h>

static unsigned long lastClockUpdate = 0;

void setup() {
    Serial.begin(115200);

    PowerManager::init();
    DisplayUI::init();
    AlarmManager::init();
    AudioManager::init();

    // Fecha inicial por defecto
    struct tm tm;
    tm.tm_year = 2026 - 1900; tm.tm_mon = 8; tm.tm_mday = 17;
    tm.tm_hour = 0; tm.tm_min = 30; tm.tm_sec = 0;
    timeval tv = { mktime(&tm), 0 };
    settimeofday(&tv, NULL);

    WiFiPortal::initSaved();
    DisplayUI::drawProfileMenu();
}

void loop() {
    PowerManager::checkButton();
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