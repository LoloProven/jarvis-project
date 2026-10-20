#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include "config.h"
#include <Arduino_GFX_Library.h>

class DisplayUI {
public:
    static void init();
    static void refreshCurrentScreen();
    static void drawProfileMenu();
    static void drawDashboard();
    static void drawHeaderClockOnly();
    static void refreshChronoDigits();   // solo si se ve la pestaña CRONO
    static void drawWifiPortalScreen();
    static void triggerAlarmRinging(int alarmIdx);
    static void handleTouch(int16_t x, int16_t y);

    static Arduino_GFX* getGFX();
    static ScreenState getCurrentScreen();
    static void setScreenState(ScreenState state);
    static String getFormattedTime();

private:
    static Arduino_DataBus *bus;
    static Arduino_GFX *gfx;
    static ScreenState currentScreen;
    static WidgetTab currentTab;
    static int activeProfile;
    static int tempHour;
    static int tempMinute;
    static unsigned long lastClockUpdate;

    static void drawAvatar(int x, int y, uint16_t color, const char* name);
    static void drawChronoDigits();
};

#endif