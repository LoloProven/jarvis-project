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

// Hotspot del celular (opcional). Para activarlo, agregar en secrets.h:
//   #define SECRET_HOTSPOT_SSID "nombre del hotspot"
//   #define SECRET_HOTSPOT_PASS "clave del hotspot"
// El hotspot tiene que estar en 2,4 GHz (el ESP32-S3 no ve redes de 5 GHz).
#if defined(SECRET_HOTSPOT_SSID) && defined(SECRET_HOTSPOT_PASS)
#define HAS_HOTSPOT 1
#define HOTSPOT_SSID SECRET_HOTSPOT_SSID
#define HOTSPOT_PASS SECRET_HOTSPOT_PASS
#endif

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

// Tiempo sin tocar la pantalla hasta que se apaga sola.
#define SCREEN_TIMEOUT_MS  15000UL

// Con la pantalla apagada: al minuto se apaga el WiFi (se reconecta al prenderla).
#define WIFI_OFF_AFTER_MS  60000UL
// ...y a los 10 minutos entra en modo dormir (gasta muy poco; las alarmas suenan igual y los
// pasos se siguen contando). No entra si estás caminando o corre el cronómetro/temporizador.
#define SLEEP_AFTER_MS     600000UL

// Largo de paso de respaldo (cm), solo para el primer paso de una racha. La distancia usa
// el largo según el ritmo (step_counter.cpp, STRIDE_CM_PER_SPM).
// Antes: largo de un paso para calcular la distancia (cm). Ajustalo a tu paso:
// caminá 10 pasos, medí la distancia y dividila por 10.
#define STEP_LENGTH_CM  70

// PINES AUDIO: códec ES8311 (parlante y micrófono por el mismo I2S), tomados
// del ejemplo 08_ES8311 de Waveshare. El ES8311 se configura por I2C (Wire).
#define I2S_MCLK      16
#define I2S_BCLK      41
#define I2S_WS        45
#define I2S_DOUT      40    // ESP32 -> ES8311 (parlante)
#define I2S_DIN       42    // ES8311 -> ESP32 (micrófono)
#define AUDIO_PA_EN   46    // habilita el amplificador del parlante (HIGH = encendido)
#define ES8311_ADDR   0x18

// --- usado por AlarmManager ---
struct AlarmItem {
    int hour;
    int minute;
    bool active;
    long lastFiredKey;   // minuto (día del año + hora + minuto) en que sonó por última vez; -1 = nunca
};

// --- PERFILES CON WEBHOOK (backend n8n) ---
struct UserProfile {
    const char* id;
    const char* name;
    const char* webhook_url;
};

static const UserProfile PROFILES[] = {
    {"user_main",  "Lolo",  SECRET_WEBHOOK_MAIN},
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
    SCREEN_ALARM_RINGING,
    SCREEN_SETTINGS,       // AJUSTES: brillo, volumen de la alarma y WiFi
    SCREEN_BATTERY         // registro de la batería (se entra desde AJUSTES)
};

// --- TABS DEL DASHBOARD ---
enum WidgetTab {
    TAB_AGENDA,
    TAB_CHRONO,
    TAB_ADD_ALARM,
    TAB_MUSIC      // control de la música del celular por Bluetooth (reemplazó a VOZ)
};

// --- PERFIL VISUAL (avatar del menú) ---
struct Profile {
    const char* name;
    uint16_t color;
    int x, y, w, h;
};

// Los colores de la interfaz están en palette.h.

// --- MARGEN SEGURO ---
// Las esquinas redondeadas del AMOLED tapan ~50 px a cada lado en la parte
// de arriba (y abajo). Lo que se dibuje cerca de los bordes tiene que quedar
// dentro de estos márgenes.
#define SAFE_MARGIN_X    55
#define SAFE_MARGIN_TOP  20

#endif