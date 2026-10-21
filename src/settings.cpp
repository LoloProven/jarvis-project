#include "settings.h"
#include "display_ui.h"
#include "audio_manager.h"
#include <Preferences.h>

static Preferences prefs;
static int brightnessLevel = 4;            // 80 % por defecto: casi igual y gasta menos que el 100 %

void Settings::load() {
    prefs.begin("ajustes", true);
    brightnessLevel = constrain(prefs.getInt("brillo", 4), 1, BRIGHTNESS_LEVELS);
    AudioManager::masterVolume = constrain(prefs.getInt("volumen", 10), 0, 10);
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
    applyBrightness();
    prefs.begin("ajustes", false);
    prefs.putInt("brillo", brightnessLevel);
    prefs.end();
}

int Settings::alarmVolume() { return AudioManager::masterVolume; }

void Settings::setAlarmVolume(int vol) {
    AudioManager::masterVolume = constrain(vol, 0, 10);
    prefs.begin("ajustes", false);
    prefs.putInt("volumen", AudioManager::masterVolume);
    prefs.end();
}
