#ifndef STEP_COUNTER_H
#define STEP_COUNTER_H

#include <stdint.h>

// Pasos del día contados por software con el acelerómetro del QMI8658.
// El total se guarda en la memoria permanente cada 5 minutos y vuelve a 0 a la medianoche.
class StepCounter {
public:
    static void init();             // después de Wire.begin()
    static void loop();             // lee el sensor cada 2 s
    static uint32_t steps();        // pasos de hoy
    static float distanceKm();      // suma del largo de cada paso (según el ritmo)
    static float speedKmh();        // estimada con el ritmo de los últimos 10 s (0 si estás quieto)
    static bool available();        // false si no se encontró el sensor
    static bool takeWristRaise();   // true una vez por cada vez que se levanta la muñeca
    static void pollWhileSleeping();  // en modo dormir: procesar lo que juntó el sensor
    static bool walkedWithin(unsigned long ms);   // hubo pasos en los últimos ms
};

#endif
