#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "pin_config.h"   // pines de pantalla/touch/SD (LCD_*, IIC_*, TP_*, SDMMC_*)
#include "secrets.h"      // credenciales wifi y webhook (no se sube a git)

// --- CREDENCIALES WIFI ---
static const char* const WIFI_SSID = SECRET_WIFI_SSID;
static const char* const WIFI_PASS = SECRET_WIFI_PASS;
#define MY_WIFI_SSID     WIFI_SSID
#define MY_WIFI_PASSWORD WIFI_PASS

// --- ALIAS de nombres de pines viejos -> nuevos (pin_config.h) ---
#define LCD_D0  LCD_SDIO0
#define LCD_D1  LCD_SDIO1
#define LCD_D2  LCD_SDIO2
#define LCD_D3  LCD_SDIO3
#define LCD_RST LCD_RESET

// --- PINES que NO estaban en pin_config.h ---
#define PMU_SDA 15
#define PMU_SCL 14
#define TOUCH_I2C_ADDR 0x38

// PINES AUDIO (parlante y micrófono I2S)
#define I2S_MIC_CLK   47
#define I2S_MIC_DATA  48
#define I2S_SPK_BCLK  42
#define I2S_SPK_LRCK  41
#define I2S_SPK_DOUT  46

// --- usado por AlarmManager ---
struct AlarmItem {
    int hour;
    int minute;
    bool active;
};

// --- PERFILES CON WEBHOOK (backend n8n) ---
struct UserProfile {
    const char* id;
    const char* name;
    const char* webhook_url;
};

static const UserProfile PROFILES[] = {
    {"user_main",  "Lolo",  SECRET_WEBHOOK_MAIN},
    {"user_santi", "Santi", ""},
    {"user_delfi", "Delfi", ""},
    {"user_fede",  "Fede",  ""},
    {"user_lucia", "Lucia", ""}
};

static const int TOTAL_PROFILES = sizeof(PROFILES) / sizeof(PROFILES[0]);

// --- ESTADOS DE PANTALLA ---
enum ScreenState {
    SCREEN_PROFILES,
    SCREEN_DASHBOARD,
    SCREEN_WIFI_CONFIG,
    SCREEN_ALARM_RINGING
};

// --- TABS DEL DASHBOARD ---
enum WidgetTab {
    TAB_AGENDA,
    TAB_CHRONO,
    TAB_ADD_ALARM,
    TAB_AUDIO
};

// --- PERFIL VISUAL (avatar del menú) ---
struct Profile {
    const char* name;
    uint16_t color;
    int x, y, w, h;
};

// --- COLORES RGB565 ---
// Los RGB565_* vienen de Arduino_GFX; acá solo los que la librería no trae.
#define DARK_GREEN        0x03E0

// --- MARGEN SEGURO ---
// Las esquinas redondeadas del AMOLED tapan ~50 px a cada lado en la parte
// de arriba (y abajo). Lo que se dibuje cerca de los bordes tiene que quedar
// dentro de estos márgenes.
#define SAFE_MARGIN_X    55
#define SAFE_MARGIN_TOP  20

#endif