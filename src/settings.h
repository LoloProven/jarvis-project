#ifndef SETTINGS_H
#define SETTINGS_H

// Ajustes del usuario guardados en la memoria permanente (NVS): brillo de la pantalla y
// volumen de la alarma. Se cambian desde la pantalla AJUSTES.
class Settings {
public:
    static const int BRIGHTNESS_LEVELS = 5;   // 20 %, 40 %, 60 %, 80 %, 100 %

    static void load();                        // al arrancar (antes de usar la pantalla)
    static int brightness();                   // 1..BRIGHTNESS_LEVELS
    static void setBrightness(int level);      // guarda y aplica al panel
    static void applyBrightness();             // vuelve a aplicarlo (p. ej. al prender la pantalla)
    static int alarmVolume();                  // 0..10
    static void setAlarmVolume(int vol);       // guarda
};

#endif
