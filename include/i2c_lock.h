#ifndef I2C_LOCK_H
#define I2C_LOCK_H

#include <Arduino.h>

// El I2C (Wire) lo comparten el touch, el AXP2101, el RTC, el códec y el QMI8658, y se
// usa desde dos tareas: el loop y la tarea de pasos. Una lectura de registro son dos
// operaciones (escribir la dirección y después leer); si otra tarea se mete en el medio,
// la lectura devuelve basura. Por ejemplo, el AXP2101 "veía" toques del botón PWR que
// nadie hacía y la pantalla se apagaba y prendía sin parar.
//
// Uso: { I2CLock lock; ...operaciones con Wire... }   (se libera al salir del bloque)

SemaphoreHandle_t i2cMutex();

class I2CLock {
public:
    I2CLock()  { xSemaphoreTakeRecursive(i2cMutex(), portMAX_DELAY); }
    ~I2CLock() { xSemaphoreGiveRecursive(i2cMutex()); }
    I2CLock(const I2CLock&) = delete;
    I2CLock& operator=(const I2CLock&) = delete;
};

#endif
