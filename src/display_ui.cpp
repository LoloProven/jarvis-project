#include "display_ui.h"
#include "power_manager.h"
#include "touch_driver.h"
#include "alarm_manager.h"
#include "audio_manager.h"
#include "wifi_portal.h"
#include "palette.h"
#include <WiFi.h>

// --- CORREGIDO: inicialización validada contra el ejemplo oficial de Waveshare ---
// OJO: estos nombres de pines (LCD_CS, LCD_SCLK, LCD_SDIO0-3, LCD_RESET) tienen
// que venir de pin_config.h (el que copiaste a include/). Si tu config.h ya
// definía LCD_RST / LCD_D0-D3 con otro nombre, puede haber conflicto de doble
// definición al compilar -- en ese caso, borrá esas definiciones viejas de
// config.h y dejá que pin_config.h sea la única fuente de verdad para pines.
Arduino_DataBus *DisplayUI::bus = new Arduino_ESP32QSPI(
    LCD_CS    /* CS */,
    LCD_SCLK  /* SCK */,
    LCD_SDIO0 /* SDIO0 */,
    LCD_SDIO1 /* SDIO1 */,
    LCD_SDIO2 /* SDIO2 */,
    LCD_SDIO3 /* SDIO3 */
);

Arduino_GFX *DisplayUI::gfx = new Arduino_CO5300(
    bus,
    LCD_RESET /* RST */,
    0         /* rotation */,
    LCD_WIDTH, LCD_HEIGHT,
    22 /* col_offset1 */,
    0  /* row_offset1 */,
    0  /* col_offset2 */,
    0  /* row_offset2 */
);

ScreenState DisplayUI::currentScreen = SCREEN_PROFILES;
WidgetTab DisplayUI::currentTab = TAB_AGENDA;
int DisplayUI::activeProfile = -1;
int DisplayUI::tempHour = 18;
int DisplayUI::tempMinute = 0;
unsigned long DisplayUI::lastClockUpdate = 0;

// --- ENCABEZADO DEL DASHBOARD (franja de 50 px, todo centrado en y=32) ---
static const int HEADER_H     = 50;
static const int CLOCK_W      = 5 * 18;                                  // "HH:MM" con setTextSize(3)
static const int CLOCK_X      = LCD_WIDTH - SAFE_MARGIN_X - CLOCK_W;     // 265
static const int CLOCK_Y      = SAFE_MARGIN_TOP;                         // 20..44
static const int NAME_X       = SAFE_MARGIN_X;
static const int NAME_Y       = SAFE_MARGIN_TOP + 4;                     // 24..40
static const int WIFI_BOX_X   = 124;                                     // después de un nombre de 5 letras
static const int WIFI_BOX_W   = CLOCK_X - 10 - WIFI_BOX_X;               // termina antes del fondo de la hora
static const int WIFI_BOX_Y   = SAFE_MARGIN_TOP - 1;
static const int WIFI_BOX_H   = 26;

static Profile profiles[4] = {
    {"Lolo",   PAL_CELESTE,  35,  30, 150, 150},
    {"Delfi",  PAL_ROSA,    225,  30, 150, 150},
    {"Fede",   PAL_MENTA,    35, 210, 150, 150},
    {"Lucia",  PAL_LILA,    225, 210, 150, 150}
};

// Pestaña del dashboard: activa en acento con texto oscuro, inactiva en tarjeta con texto claro.
static void drawTab(Arduino_GFX* g, int x, int w, int textX, const char* label, bool active) {
    g->fillRect(x, 65, w, 38, active ? PAL_ACENTO : PAL_TARJETA);
    g->setTextColor(active ? PAL_TEXTO : PAL_TEXTO_CLARO);
    g->setTextSize(2);
    g->setCursor(textX, 76);
    g->print(label);
}

void DisplayUI::init() {
    gfx->begin();
}

Arduino_GFX* DisplayUI::getGFX() { return gfx; }
ScreenState DisplayUI::getCurrentScreen() { return currentScreen; }
void DisplayUI::setScreenState(ScreenState state) { currentScreen = state; }

String DisplayUI::getFormattedTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "--:--";
    char timeBuff[10];
    strftime(timeBuff, sizeof(timeBuff), "%H:%M", &timeinfo);
    return String(timeBuff);
}

void DisplayUI::refreshCurrentScreen() {
    if (currentScreen == SCREEN_PROFILES) drawProfileMenu();
    else if (currentScreen == SCREEN_DASHBOARD) drawDashboard();
}

void DisplayUI::drawAvatar(int x, int y, uint16_t color, const char* name) {
    gfx->fillRoundRect(x, y, 150, 150, 15, color);
    gfx->drawRoundRect(x, y, 150, 150, 15, PAL_TEXTO_CLARO);
    gfx->fillCircle(x + 75, y + 50, 25, PAL_TEXTO);
    gfx->fillRoundRect(x + 40, y + 80, 70, 40, 10, PAL_TEXTO);
    gfx->setTextColor(PAL_TEXTO_CLARO);  // el nombre va debajo del cuadrado, sobre negro
    gfx->setTextSize(2);
    gfx->setCursor(x + 20, y + 160);
    gfx->print(name);
}

void DisplayUI::drawProfileMenu() {
    gfx->fillScreen(PAL_FONDO);
    for (int i = 0; i < 4; i++) {
        drawAvatar(profiles[i].x, profiles[i].y, profiles[i].color, profiles[i].name);
    }
    gfx->fillRoundRect(50, 400, 310, 60, 12, PAL_ACENTO);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(2);
    gfx->setCursor(95, 420);
    gfx->print("CONFIGURAR WIFI");
}

void DisplayUI::drawHeaderClockOnly() {
    if (activeProfile < 0 || PowerManager::isScreenOff()) return;
    gfx->fillRect(CLOCK_X - 5, 0, LCD_WIDTH - (CLOCK_X - 5), HEADER_H, profiles[activeProfile].color);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(3);
    gfx->setCursor(CLOCK_X, CLOCK_Y);
    gfx->print(getFormattedTime());
}

void DisplayUI::drawDashboard() {
    if (PowerManager::isScreenOff()) return;
    gfx->fillScreen(PAL_FONDO);
    bool online = (WiFi.status() == WL_CONNECTED);

    gfx->fillRect(0, 0, LCD_WIDTH, HEADER_H, profiles[activeProfile].color);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(2);
    gfx->setCursor(NAME_X, NAME_Y);
    gfx->printf("%s", profiles[activeProfile].name);

    gfx->fillRoundRect(WIFI_BOX_X, WIFI_BOX_Y, WIFI_BOX_W, WIFI_BOX_H, 6, online ? PAL_OK : PAL_ALERTA);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setCursor(WIFI_BOX_X + 6, WIFI_BOX_Y + 9);
    gfx->setTextSize(1);
    if (online) {
        String ssid = WiFi.SSID();
        if (ssid.length() > 10) ssid = ssid.substring(0, 8) + "..";
        gfx->printf("WiFi: %s", ssid.c_str());
    } else {
        gfx->print("WiFi: Desconectado");
    }

    drawHeaderClockOnly();

    int tabW = online ? 88 : 115;
    drawTab(gfx, 15,                tabW, 25,                "AGENDA",  currentTab == TAB_AGENDA);
    drawTab(gfx, 20 + tabW,         tabW, 30 + tabW,         "CRONO",   currentTab == TAB_CHRONO);
    drawTab(gfx, 25 + (tabW * 2),   tabW, 30 + (tabW * 2),   "+ALARMA", currentTab == TAB_ADD_ALARM);
    if (online) {
        drawTab(gfx, 30 + (tabW * 3), tabW, 40 + (tabW * 3), "VOZ",     currentTab == TAB_AUDIO);
    }

    gfx->drawRoundRect(15, 110, 380, 300, 12, PAL_LILA);

    if (currentTab == TAB_AGENDA) {
        gfx->setTextColor(PAL_AMARILLO);
        gfx->setTextSize(2);
        gfx->setCursor(30, 125);
        gfx->printf("Alarmas Programadas (%d/%d)", AlarmManager::alarmCount, MAX_ALARMS);

        if (AlarmManager::alarmCount == 0) {
            gfx->setTextColor(PAL_LILA);
            gfx->setCursor(40, 220);
            gfx->print("Sin alarmas. Ve a +ALARMA");
        } else {
            for (int i = 0; i < AlarmManager::alarmCount; i++) {
                int yPos = 160 + (i * 50);
                bool active = AlarmManager::alarmList[i].active;
                gfx->fillRoundRect(30, yPos, 110, 40, 8, active ? PAL_ACENTO : PAL_TARJETA);
                gfx->setTextColor(active ? PAL_TEXTO : PAL_TEXTO_CLARO);
                gfx->setTextSize(3);
                gfx->setCursor(40, yPos + 8);
                gfx->printf("%02d:%02d", AlarmManager::alarmList[i].hour, AlarmManager::alarmList[i].minute);

                gfx->setTextColor(active ? PAL_OK : PAL_ALERTA);
                gfx->setTextSize(2);
                gfx->setCursor(155, yPos + 12);
                gfx->print(active ? "ACTIVA" : "INACTIVA");

                gfx->fillRoundRect(310, yPos, 45, 40, 8, PAL_ALERTA);
                gfx->setTextColor(PAL_TEXTO);
                gfx->setTextSize(3);
                gfx->setCursor(325, yPos + 8);
                gfx->print("X");
            }
        }
    } else if (currentTab == TAB_CHRONO) {
        unsigned long currentMillis = AlarmManager::getChronoTime();
        int seconds = (currentMillis / 1000) % 60;
        int minutes = (currentMillis / (1000 * 60)) % 60;

        gfx->setTextSize(5);
        gfx->setTextColor(PAL_MENTA);
        gfx->setCursor(130, 170);
        gfx->printf("%02d:%02d", minutes, seconds);

        gfx->fillRoundRect(45, 290, 130, 50, 10, AlarmManager::isChronoRunning() ? PAL_ALERTA : PAL_OK);
        gfx->setTextColor(PAL_TEXTO);
        gfx->setTextSize(2);
        gfx->setCursor(60, 305);
        gfx->print(AlarmManager::isChronoRunning() ? "PAUSA" : "INICIAR");

        gfx->fillRoundRect(235, 290, 130, 50, 10, PAL_LILA);
        gfx->setCursor(265, 305);
        gfx->print("RESET");
    } else if (currentTab == TAB_ADD_ALARM) {
        gfx->setTextColor(PAL_TEXTO_CLARO);
        gfx->setTextSize(2);
        gfx->setCursor(30, 125); gfx->print("Nueva Alarma:");

        gfx->setTextSize(5);
        gfx->setTextColor(PAL_CELESTE);
        gfx->setCursor(40, 155);
        gfx->printf("%02d:%02d", tempHour, tempMinute);

        gfx->setTextSize(2);
        gfx->setTextColor(PAL_AMARILLO);
        gfx->setCursor(230, 130);
        gfx->printf("Vol: %d/10", AudioManager::masterVolume);

        gfx->fillRoundRect(230, 155, 60, 40, 6, PAL_LILA);
        gfx->fillRoundRect(300, 155, 60, 40, 6, PAL_LILA);
        gfx->setTextColor(PAL_TEXTO);
        gfx->setTextSize(3);
        gfx->setCursor(252, 162); gfx->print("-");
        gfx->setCursor(322, 162); gfx->print("+");

        gfx->fillRoundRect(40, 225, 85, 45, 8, PAL_ACENTO);
        gfx->fillRoundRect(140, 225, 85, 45, 8, PAL_ACENTO);
        gfx->setTextSize(3);
        gfx->setCursor(75, 235); gfx->print("+");
        gfx->setCursor(175, 235); gfx->print("+");

        gfx->fillRoundRect(40, 280, 85, 45, 8, PAL_ACENTO);
        gfx->fillRoundRect(140, 280, 85, 45, 8, PAL_ACENTO);
        gfx->setCursor(75, 290); gfx->print("-");
        gfx->setCursor(175, 290); gfx->print("-");

        gfx->fillRoundRect(240, 225, 140, 100, 12, PAL_OK);
        gfx->setTextSize(2);
        gfx->setCursor(255, 265);
        gfx->print("GUARDAR");
    } else if (currentTab == TAB_AUDIO && online) {
        gfx->fillCircle(205, 230, 65, PAL_ACENTO);
        gfx->setTextColor(PAL_TEXTO);
        gfx->setTextSize(2);
        gfx->setCursor(160, 222);
        gfx->print("MANTENER");
    }

    gfx->fillRoundRect(100, 430, 210, 45, 12, PAL_ALERTA);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(2);
    gfx->setCursor(120, 442);
    gfx->print("Cerrar Sesion");
}

void DisplayUI::drawWifiPortalScreen() {
    gfx->fillScreen(PAL_FONDO);
    gfx->setTextColor(PAL_CELESTE);
    gfx->setTextSize(2);
    gfx->setCursor(35, 40);
    gfx->print("CONFIGURAR WIFI");

    gfx->setTextColor(PAL_TEXTO_CLARO);
    gfx->setCursor(25, 110); gfx->print("1. Conectate a la red:");
    gfx->setTextColor(PAL_AMARILLO);
    gfx->setCursor(25, 140); gfx->print("   JARVIS-Clock-Config");

    gfx->setTextColor(PAL_TEXTO_CLARO);
    gfx->setCursor(25, 200); gfx->print("2. Abre en tu navegador:");
    gfx->setTextColor(PAL_AMARILLO);
    gfx->setCursor(25, 230); gfx->print("   http://192.168.4.1");

    gfx->fillRoundRect(50, 340, 300, 70, 15, PAL_ALERTA);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(3);
    gfx->setCursor(120, 362); gfx->print("VOLVER");
}

void DisplayUI::triggerAlarmRinging(int alarmIdx) {
    if (PowerManager::isScreenOff()) PowerManager::toggleScreenState();

    currentScreen = SCREEN_ALARM_RINGING;
    // Pantalla completa en coral: es lo más llamativo de la paleta y contrasta
    // con el resto de la interfaz, que es casi toda negra.
    gfx->fillScreen(PAL_ALERTA);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(4);
    gfx->setCursor(80, 50); gfx->print("ALARMA!");

    gfx->setTextSize(6);
    gfx->setCursor(110, 120);
    gfx->printf("%02d:%02d", AlarmManager::alarmList[alarmIdx].hour, AlarmManager::alarmList[alarmIdx].minute);

    gfx->fillRoundRect(20, 220, 370, 240, 20, PAL_TEXTO);
    gfx->setTextColor(PAL_ALERTA);
    gfx->setTextSize(4);
    gfx->setCursor(65, 310); gfx->print("DESACTIVAR");

    while (true) {
        AudioManager::playTone(AudioManager::masterVolume);
        int16_t tx, ty;
        if (TouchDriver::getTouch(tx, ty) && ty >= 200) {
            TouchDriver::waitForRelease();
            break;
        }
    }

    AudioManager::stopTone();
    AlarmManager::alarmList[alarmIdx].active = false;
    currentScreen = SCREEN_DASHBOARD;
    drawDashboard();
}

void DisplayUI::handleTouch(int16_t tx, int16_t ty) {
    if (currentScreen == SCREEN_WIFI_CONFIG) {
        if (tx >= 50 && tx <= 350 && ty >= 340 && ty <= 410) {
            TouchDriver::waitForRelease();
            WiFi.softAPdisconnect(true);
            currentScreen = SCREEN_PROFILES;
            drawProfileMenu();
        }
        return;
    }

    if (currentScreen == SCREEN_PROFILES) {
        for (int i = 0; i < 4; i++) {
            if (tx >= profiles[i].x && tx <= (profiles[i].x + profiles[i].w) &&
                ty >= profiles[i].y && ty <= (profiles[i].y + profiles[i].h)) {
                TouchDriver::waitForRelease();
                activeProfile = i;
                currentScreen = SCREEN_DASHBOARD;
                drawDashboard();
                return;
            }
        }
        if (tx >= 50 && tx <= 360 && ty >= 400 && ty <= 460) {
            TouchDriver::waitForRelease();
            WiFiPortal::startPortal();
            return;
        }
    } else if (currentScreen == SCREEN_DASHBOARD) {
        bool online = (WiFi.status() == WL_CONNECTED);

        if (ty >= 65 && ty <= 103) {
            int tabW = online ? 88 : 115;
            if (tx >= 15 && tx <= 15 + tabW) currentTab = TAB_AGENDA;
            else if (tx >= 20 + tabW && tx <= 20 + (tabW * 2)) currentTab = TAB_CHRONO;
            else if (tx >= 25 + (tabW * 2) && tx <= 25 + (tabW * 3)) currentTab = TAB_ADD_ALARM;
            else if (online && tx >= 30 + (tabW * 3) && tx <= 390) currentTab = TAB_AUDIO;

            TouchDriver::waitForRelease();
            drawDashboard();
            return;
        }

        if (currentTab == TAB_AGENDA) {
            for (int i = 0; i < AlarmManager::alarmCount; i++) {
                int yPos = 160 + (i * 50);
                if (tx >= 310 && tx <= 355 && ty >= yPos && ty <= yPos + 40) {
                    TouchDriver::waitForRelease();
                    AlarmManager::deleteAlarm(i);
                    drawDashboard();
                    return;
                }
            }
        }

        if (currentTab == TAB_CHRONO && ty >= 290 && ty <= 340) {
            if (tx >= 45 && tx <= 175) AlarmManager::toggleChrono();
            else if (tx >= 235 && tx <= 365) AlarmManager::resetChrono();
            TouchDriver::waitForRelease();
            drawDashboard();
            return;
        }

        if (currentTab == TAB_ADD_ALARM) {
            if (ty >= 155 && ty <= 195) {
                if (tx >= 230 && tx <= 290 && AudioManager::masterVolume > 0) AudioManager::masterVolume--;
                if (tx >= 300 && tx <= 360 && AudioManager::masterVolume < 10) AudioManager::masterVolume++;
                drawDashboard();
                TouchDriver::waitForRelease();
                return;
            }
            if (ty >= 225 && ty <= 270) {
                if (tx >= 40 && tx <= 125) tempHour = (tempHour + 1) % 24;
                if (tx >= 140 && tx <= 225) tempMinute = (tempMinute + 1) % 60;
                TouchDriver::waitForRelease();
                drawDashboard();
                return;
            } else if (ty >= 280 && ty <= 325) {
                if (tx >= 40 && tx <= 125) tempHour = (tempHour + 23) % 24;
                if (tx >= 140 && tx <= 225) tempMinute = (tempMinute + 59) % 60;
                TouchDriver::waitForRelease();
                drawDashboard();
                return;
            } else if (tx >= 235 && tx <= 385 && ty >= 220 && ty <= 330) {
                AlarmManager::addAlarm(tempHour, tempMinute);
                currentTab = TAB_AGENDA;
                TouchDriver::waitForRelease();
                drawDashboard();
                return;
            }
        }

        // --- BOTÓN DE VOZ CON FEEDBACK Y DETECCIÓN MEJORADA ---
        if (currentTab == TAB_AUDIO && online && tx >= 130 && tx <= 280 && ty >= 150 && ty <= 300) {
            gfx->fillCircle(205, 230, 65, PAL_ALERTA);
            gfx->setTextColor(PAL_TEXTO);
            gfx->setTextSize(2);
            gfx->setCursor(150, 222);
            gfx->print("GRABANDO");

            AudioManager::recordAndSend();

            TouchDriver::waitForRelease();
            drawDashboard();
            return;
        }

        if (tx >= 100 && tx <= 310 && ty >= 430 && ty <= 475) {
            TouchDriver::waitForRelease();
            activeProfile = -1;
            currentScreen = SCREEN_PROFILES;
            drawProfileMenu();
            return;
        }
    }
}