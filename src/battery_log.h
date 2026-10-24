#ifndef BATTERY_LOG_H
#define BATTERY_LOG_H

#include <stdint.h>
#include <time.h>

// Registro de la batería para saber cuánto gasta el reloj en cada estado.
// Cada INTERVAL_MIN minutos guarda: hora, porcentaje, si estuvo cargando y cuántos minutos
// pasó con la pantalla prendida, apagada, en modo dormir y con el WiFi prendido.
// Se guarda en la memoria permanente, así sobrevive a un reinicio o a quedarse sin batería.
class BatteryLog {
public:
    static const int INTERVAL_MIN = 15;
    static const int MAX_ENTRIES = 48;     // 12 horas

    struct Entry {
        uint32_t when;                     // hora (time_t) al cerrar el tramo
        uint8_t pct;                       // porcentaje al cerrar el tramo
        uint8_t charging;                  // 1 si se cargó en algún momento del tramo
        uint8_t onMin, offMin, sleepMin, wifiMin;   // minutos en cada estado
        uint16_t mv;                       // voltaje de la batería al cerrar el tramo
        uint8_t asleepPct;                 // del tiempo en modo dormir, % que el chip durmió de verdad
    };

    static void init();                    // después de PowerManager::init()
    static void tick();                    // en cada vuelta del loop, también en modo dormir
    static int count();
    static const Entry& get(int i);        // 0 = el más viejo
    // Gasto promedio (% por hora) en tramos donde casi todo el tiempo fue ese estado
    // (0 = prendida, 1 = apagada, 2 = dormido). Devuelve -1 si todavía no hay datos.
    static float ratePerHour(int state);
    static void clear();
};

#endif
