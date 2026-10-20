#include "touch_driver.h"
#include "config.h"
#include "power_manager.h"
#include <Wire.h>

bool TouchDriver::getTouch(int16_t &tx, int16_t &ty) {
    if (PowerManager::isScreenOff()) return false;

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