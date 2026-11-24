#include "settings.h"
#include "display_ui.h"
#include "audio_manager.h"
#include "media_remote.h"
#include "power_manager.h"
#include <Preferences.h>

static Preferences prefs;
static int brightnessLevel = 4;            // 80 % por defecto: casi igual y gasta menos que el 100 %
static bool btOn = true, noticesOn = true, wristOn = true, weatherOn = true;
static int screenSec = SCREEN_TIMEOUT_MS / 1000;
static int sleepMin = SLEEP_AFTER_MS / 60000;
static long goal = 10000;

void Settings::load() {
    prefs.begin("ajustes", true);
    brightnessLevel = constrain(prefs.getInt("brillo", 4), 1, BRIGHTNESS_LEVELS);
    AudioManager::masterVolume = constrain(prefs.getInt("volumen", 10), 0, 10);
    btOn = prefs.getBool("bt", true);
    noticesOn = prefs.getBool("avisos", true);
    wristOn = prefs.getBool("muneca", true);
    weatherOn = prefs.getBool("clima", true);
    screenSec = constrain(prefs.getInt("pantalla", screenSec), 10, 120);
    sleepMin = constrain(prefs.getInt("dormir", sleepMin), 0, 60);
    goal = constrain(prefs.getLong("meta", goal), 1000L, 50000L);
    prefs.end();
}

static void saveInt(const char* key, int v) {
    prefs.begin("ajustes", false);
    prefs.putInt(key, v);
    prefs.end();
}

static void saveBool(const char* key, bool v) {
    prefs.begin("ajustes", false);
    prefs.putBool(key, v);
    prefs.end();
}

int Settings::brightness() { return brightnessLevel; }

void Settings::applyBrightness() {
    // El CO5300 va de 0 a 255; nivel 1 = 20 % ... nivel 5 = 100 %.
    // setBrightness es del CO5300, no de Arduino_GFX: la pantalla se creó como Arduino_CO5300.
    static_cast<Arduino_CO5300*>(DisplayUI::getGFX())->setBrightness(brightnessLevel * 255 / BRIGHTNESS_LEVELS);
}

void Settings::setBrightness(int level) {
    brightnessLevel = constrain(level, 1, BRIGHTNESS_LEVELS);
    if (!PowerManager::isScreenOff()) applyBrightness();
    saveInt("brillo", brightnessLevel);
}

int Settings::alarmVolume() { return AudioManager::masterVolume; }

void Settings::setAlarmVolume(int vol) {
    AudioManager::masterVolume = constrain(vol, 0, 10);
    saveInt("volumen", AudioManager::masterVolume);
}

bool Settings::bluetooth() { return btOn; }

// Apagado no se ven los avisos ni anda MÚSICA, pero el chip puede dormir de verdad en el
// modo dormir y gasta menos con la pantalla apagada.
void Settings::setBluetooth(bool on) {
    btOn = on;
    saveBool("bt", on);
    if (on && !PowerManager::isSleeping()) MediaRemote::init();
    else if (!on) MediaRemote::stop();
}

bool Settings::notifications() { return noticesOn; }
void Settings::setNotifications(bool on) { noticesOn = on; saveBool("avisos", on); }
bool Settings::wristRaise() { return wristOn; }
void Settings::setWristRaise(bool on) { wristOn = on; saveBool("muneca", on); }
bool Settings::weather() { return weatherOn; }
void Settings::setWeather(bool on) { weatherOn = on; saveBool("clima", on); }

unsigned long Settings::screenTimeoutMs() { return screenSec * 1000UL; }
void Settings::setScreenTimeoutSec(int sec) { screenSec = constrain(sec, 10, 120); saveInt("pantalla", screenSec); }
unsigned long Settings::sleepAfterMs() { return sleepMin * 60000UL; }
void Settings::setSleepAfterMin(int min) { sleepMin = constrain(min, 0, 60); saveInt("dormir", sleepMin); }
unsigned long Settings::stepGoal() { return goal; }

void Settings::setStepGoal(long steps) {
    goal = constrain(steps, 1000L, 50000L);
    prefs.begin("ajustes", false);
    prefs.putLong("meta", goal);
    prefs.end();
}
