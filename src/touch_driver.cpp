#include "touch_driver.h"
#include "config.h"
#include "i2c_lock.h"
#include <Wire.h>

// Igual que el ejemplo de Waveshare (Arduino_FT3x68 con TP_INT): el FT3168 baja
// TP_INT cuando hay un toque, y solo entonces se lee por I2C. Antes se le preguntaba
// todo el tiempo, y cuando el chip entraba en reposo no respondía y llenaba el log
// de errores de I2C en cada vuelta del loop.
//
// Mientras el dedo sigue apoyado, el chip repite el pulso; se considera que hay
// toque si hubo un pulso en los últimos TOUCH_ACTIVE_MS.
static const uint32_t TOUCH_ACTIVE_MS = 60;
static volatile uint32_t lastIrqMs = 0;
static volatile bool irqSeen = false;

static void IRAM_ATTR onTouchIrq() {
    lastIrqMs = millis();
    irqSeen = true;
}

void TouchDriver::init() {
    pinMode(TP_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TP_INT), onTouchIrq, FALLING);
}

// Se lee también con la pantalla apagada: un toque la vuelve a encender.
bool TouchDriver::getTouch(int16_t &tx, int16_t &ty) {
    if (!irqSeen || millis() - lastIrqMs > TOUCH_ACTIVE_MS) return false;

    I2CLock lock;                                // el I2C se comparte con la tarea de pasos
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(0x02);
    if (Wire.endTransmission() != 0) return false;

    if (Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)5) == 5) {
        uint8_t points = Wire.read() & 0x0F;
        if (points > 0) {
            uint8_t xH = Wire.read(), xL = Wire.read();
            uint8_t yH = Wire.read(), yL = Wire.read();
            tx = ((xH & 0x0F) << 8) | xL;
            ty = ((yH & 0x0F) << 8) | yL;
            return true;
        }
    }
    return false;
}

void TouchDriver::waitForRelease() {
    int16_t tx, ty;
    unsigned long timeout = millis();
    while (getTouch(tx, ty) && (millis() - timeout < 800)) {
        delay(15);
    }
}
