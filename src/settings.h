#ifndef SETTINGS_H
#define SETTINGS_H

// Ajustes del usuario guardados en la memoria permanente (NVS). Se cambian desde la pantalla
// AJUSTES del reloj (brillo y volumen) o desde la página de ajustes (web_settings.cpp).
class Settings {
public:
    static const int BRIGHTNESS_LEVELS = 5;   // 20 %, 40 %, 60 %, 80 %, 100 %

    static void load();                        // al arrancar (antes de usar la pantalla)
    static int brightness();                   // 1..BRIGHTNESS_LEVELS
    static void setBrightness(int level);      // guarda y aplica al panel
    static void applyBrightness();             // vuelve a aplicarlo (p. ej. al prender la pantalla)
    static int alarmVolume();                  // 0..10
    static void setAlarmVolume(int vol);       // guarda

    // Funciones que se pueden apagar para ahorrar batería.
    static bool bluetooth();                   // música y avisos del iPhone
    static void setBluetooth(bool on);         // lo prende o apaga en el momento
    static bool notifications();               // mostrar los avisos del iPhone
    static void setNotifications(bool on);
    static bool wristRaise();                  // hora grande al levantar la muñeca
    static void setWristRaise(bool on);
    static bool weather();                     // pedir el clima cada 30 min
    static void setWeather(bool on);

    static unsigned long screenTimeoutMs();    // pantalla sin tocar hasta que se apaga
    static void setScreenTimeoutSec(int sec);  // 10..120
    static unsigned long sleepAfterMs();       // pantalla apagada hasta el modo dormir (0 = nunca)
    static void setSleepAfterMin(int min);     // 0..60
    static unsigned long stepGoal();           // meta de pasos del día
    static void setStepGoal(long steps);       // 1000..50000
};

#endif
