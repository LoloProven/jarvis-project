#include "power_manager.h"
#include "config.h"
#include "display_ui.h"

XPowersPMU PowerManager::PMU;
bool PowerManager::screenOff = false;

void PowerManager::init() {
    Wire.begin(PMU_SDA, PMU_SCL);
    if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
        PMU.setDC1Voltage(3300); PMU.enableDC1();
        PMU.setALDO1Voltage(3300); PMU.enableALDO1();
        PMU.setALDO2Voltage(3300); PMU.enableALDO2();
        PMU.setALDO3Voltage(3300); PMU.enableALDO3();
        PMU.setBLDO1Voltage(3300); PMU.enableBLDO1();
        PMU.setBLDO2Voltage(3300); PMU.enableBLDO2();
        PMU.enableBattVoltageMeasure();

        PMU.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
        PMU.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
        PMU.clearIrqStatus();
    }
}

void PowerManager::toggleScreenState() {
    screenOff = !screenOff;
    if (screenOff) {
        PMU.disableALDO1();
        PMU.disableDC1();
        DisplayUI::getGFX()->displayOff();
    } else {
        PMU.enableALDO1();
        PMU.enableDC1();
        DisplayUI::getGFX()->displayOn();
        DisplayUI::refreshCurrentScreen();
    }
}

void PowerManager::checkButton() {
    // getIrqStatus() actualiza los registros internos de la librería
    PMU.getIrqStatus();
    if (PMU.isPekeyShortPressIrq()) {
        toggleScreenState();
        PMU.clearIrqStatus();
    }
}

bool PowerManager::isScreenOff() { return screenOff; }
XPowersPMU& PowerManager::getPMU() { return PMU; }