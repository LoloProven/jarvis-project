#ifndef STEP_COUNTER_H
#define STEP_COUNTER_H

#include <stdint.h>

// Pasos del día con el podómetro interno del QMI8658 (cuenta solo, sin gastar CPU).
// El total se guarda en la memoria permanente cada 5 minutos y vuelve a 0 a la medianoche.
class StepCounter {
public:
    static void init();             // después de Wire.begin()
    static void loop();             // lee el sensor cada 2 s
    static uint32_t steps();        // pasos de hoy
    static float distanceKm();      // pasos * STEP_LENGTH_CM
    static bool available();        // false si no se encontró el sensor
};

#endif
