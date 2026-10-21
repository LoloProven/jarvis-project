#include "i2c_lock.h"

// Recursivo: una función que ya tiene el turno puede llamar a otra que también lo pide.
SemaphoreHandle_t i2cMutex() {
    static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
    return m;
}
