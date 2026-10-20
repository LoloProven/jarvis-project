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
#define RGB565_BLACK      0x0000
#define RGB565_WHITE      0xFFFF
#define RGB565_RED        0xF800
#define RGB565_GREEN      0x07E0
#define RGB565_BLUE       0x001F
#define RGB565_CYAN       0x07FF
#define RGB565_MAGENTA    0xF81F
#define RGB565_YELLOW     0xFFE0
#define RGB565_ORANGE     0xFD20
#define RGB565_NAVY       0x000F
#define RGB565_PURPLE     0x780F
#define RGB565_OLIVE      0x7BE0
#define RGB565_LIGHTGREY  0xC618
#define RGB565_DARKGREY   0x7BEF
#define DARK_GREEN        0x03E0

#endif