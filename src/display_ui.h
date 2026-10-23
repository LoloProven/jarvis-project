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
    static void refreshNetStatus(bool wifiChanged);   // solo si se ve el dashboard
    static void refreshBottomBar();                   // pasos y batería; solo si se ve el dashboard
    static void refreshMusicTab();                    // si cambió la conexión Bluetooth
    static void musicTick();                          // vuelve el color del botón que titiló
    static void drawWifiPortalScreen();
    static void showSleepMessage();      // aviso antes de entrar en modo dormir
    static void drawSettingsScreen();    // AJUSTES: brillo, volumen y WiFi
    static void drawBatteryScreen();     // registro de la batería (desde AJUSTES)
    static void drawBigClock();          // hora grande en blanco sobre negro (levantar la muñeca)
    // Notificación del iPhone a pantalla completa; con llamada, botón RECHAZAR abajo.
    static void drawNotification(const char* app, const char* title, const char* message, bool call);
    static bool hitRejectButton(int16_t x, int16_t y);
    // JARVIS: va a su pestaña, graba mientras keepRecording() (BOOT apretado), pregunta a
    // n8n y muestra/dice la respuesta. Bloquea hasta terminar.
    static void talkToJarvis(bool (*keepRecording)());
    static void triggerAlarmRinging(int alarmIdx);
    // Pantalla que suena (alarma/temporizador); devuelve true si se eligió POSPONER.
    static bool ringScreen(const char* title, const char* timeText, bool allowSnooze);
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
    static void drawJarvisTab();
    static void drawNetBox();
    static void drawBottomBar();
    static void handleMusicButton(int index);
};

#endif