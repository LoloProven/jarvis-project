#ifndef WEATHER_H
#define WEATHER_H

#include <stdint.h>

// Clima de Córdoba para la pantalla de inicio: lo pide a Open-Meteo (sin clave, por HTTP)
// cada 30 min mientras haya WiFi, en una tarea aparte para no frenar el loop.
class Weather {
public:
    static void loop();                 // se fija si toca pedirlo
    static bool available();            // ya llegó al menos una vez
    static int temperature();           // °C redondeados
    static const char* description();   // "despejado", "nublado", "lluvia"...
    static uint32_t version();          // cambia cada vez que llega un dato nuevo
};

#endif
