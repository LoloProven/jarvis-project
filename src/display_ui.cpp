#include "display_ui.h"
#include "power_manager.h"
#include "touch_driver.h"
#include "alarm_manager.h"
#include "audio_manager.h"
#include "wifi_portal.h"
#include "palette.h"
#include "time_sync.h"
#include "net_status.h"
#include "step_counter.h"
#include "media_remote.h"
#include "settings.h"

static const int TAB_W = 88;   // 4 pestañas de 88 px
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
static const int WIFI_BOX_X   = 119;                                     // después de un nombre de 5 letras (termina en 115)
static const int WIFI_BOX_W   = CLOCK_X - 4 - WIFI_BOX_X;                // 142 px, termina antes del fondo de la hora
static const int WIFI_BOX_Y   = SAFE_MARGIN_TOP - 1;
static const int WIFI_BOX_H   = 26;

static Profile profiles[4] = {
    {"Lolo",   PAL_CELESTE,  35,  30, 150, 150},
    {"Delfi",  PAL_ROSA,    225,  30, 150, 150},
    {"Fede",   PAL_MENTA,    35, 210, 150, 150},
    {"Lucia",  PAL_LILA,    225, 210, 150, 150}
};

static void drawBattery(Arduino_GFX* g, int x, int y);   // definida junto a la barra inferior

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
    Settings::applyBrightness();   // brillo guardado (Settings::load() ya se llamó)
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
    else if (currentScreen == SCREEN_WIFI_CONFIG) drawWifiPortalScreen();
    else if (currentScreen == SCREEN_SETTINGS) drawSettingsScreen();
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
    if (PowerManager::isScreenOff()) return;   // se dibuja al encender
    gfx->fillScreen(PAL_FONDO);
    for (int i = 0; i < 4; i++) {
        drawAvatar(profiles[i].x, profiles[i].y, profiles[i].color, profiles[i].name);
    }
    gfx->fillRoundRect(50, 400, 310, 60, 12, PAL_ACENTO);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(3);
    gfx->setCursor(50 + (310 - 7 * 18) / 2, 418);
    gfx->print("AJUSTES");

    drawBattery(gfx, 182, 6);   // arriba al centro, lejos de las esquinas redondeadas
}

// --- PANTALLA AJUSTES ---
// Cada ajuste: título y una fila [ - ]  valor  [ + ] con botones grandes.
static const int SET_MINUS_X = 50, SET_PLUS_X = 270, SET_BTN_W = 90, SET_BTN_H = 60;
static const int SET_BRIGHT_Y = 95, SET_VOLUME_Y = 215;      // fila de botones de cada ajuste
static const int SET_WIFI_Y = 320, SET_BACK_Y = 395;          // botones de abajo (alto 55)

static void drawSettingRow(Arduino_GFX* g, const char* title, int y, const char* value) {
    g->setTextColor(PAL_LILA);
    g->setTextSize(2);
    g->setCursor((LCD_WIDTH - strlen(title) * 12) / 2, y - 30);
    g->print(title);
    g->fillRoundRect(SET_MINUS_X, y, SET_BTN_W, SET_BTN_H, 12, PAL_ACENTO);
    g->fillRoundRect(SET_PLUS_X, y, SET_BTN_W, SET_BTN_H, 12, PAL_ACENTO);
    g->setTextColor(PAL_TEXTO);
    g->setTextSize(4);
    g->setCursor(SET_MINUS_X + (SET_BTN_W - 24) / 2, y + 15); g->print("-");
    g->setCursor(SET_PLUS_X + (SET_BTN_W - 24) / 2, y + 15); g->print("+");
    // Valor en el medio (se borra antes porque cambia de largo).
    g->fillRect(SET_MINUS_X + SET_BTN_W + 5, y, SET_PLUS_X - SET_MINUS_X - SET_BTN_W - 10, SET_BTN_H, PAL_FONDO);
    g->setTextColor(PAL_TEXTO_CLARO);
    g->setTextSize(3);
    g->setCursor((LCD_WIDTH - strlen(value) * 18) / 2, y + 18);
    g->print(value);
}

static void drawSettingValues(Arduino_GFX* g) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", Settings::brightness() * 100 / Settings::BRIGHTNESS_LEVELS);
    drawSettingRow(g, "Brillo", SET_BRIGHT_Y, buf);
    snprintf(buf, sizeof(buf), "%d/10", Settings::alarmVolume());
    drawSettingRow(g, "Volumen de la alarma", SET_VOLUME_Y, buf);
}

void DisplayUI::drawSettingsScreen() {
    if (PowerManager::isScreenOff()) return;
    gfx->fillScreen(PAL_FONDO);
    gfx->setTextColor(PAL_CELESTE);
    gfx->setTextSize(3);
    gfx->setCursor((LCD_WIDTH - 7 * 18) / 2, 22);
    gfx->print("AJUSTES");
    drawSettingValues(gfx);

    gfx->fillRoundRect(60, SET_WIFI_Y, 290, 55, 12, PAL_LILA);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(2);
    gfx->setCursor((LCD_WIDTH - 15 * 12) / 2, SET_WIFI_Y + 20);
    gfx->print("CONFIGURAR WIFI");

    gfx->fillRoundRect(100, SET_BACK_Y, 210, 55, 12, PAL_ALERTA);
    gfx->setCursor((LCD_WIDTH - 6 * 12) / 2, SET_BACK_Y + 20);
    gfx->print("VOLVER");
}

// La hora se llama cada segundo pero solo cambia una vez por minuto: se redibuja
// solo si cambió el texto o si pasó a estar sincronizada (si no, parpadea).
static String lastClockText;
static int lastClockSynced = -1;           // -1 = forzar el próximo dibujo

void DisplayUI::drawHeaderClockOnly() {
    if (activeProfile < 0 || PowerManager::isScreenOff()) return;
    String text = getFormattedTime();
    int syncedNow = TimeSync::isSynced() ? 1 : 0;
    if (text == lastClockText && syncedNow == lastClockSynced) return;
    lastClockText = text;
    lastClockSynced = syncedNow;

    gfx->fillRect(CLOCK_X - 3, 0, LCD_WIDTH - (CLOCK_X - 3), HEADER_H, profiles[activeProfile].color);
    if (TimeSync::isSynced()) {
        gfx->setTextColor(PAL_TEXTO);
    } else {
        // Hora de respaldo (nunca llegó la de NTP): dígitos en coral sobre una
        // pastilla oscura, para que se note que no es confiable.
        gfx->fillRoundRect(CLOCK_X - 3, CLOCK_Y - 3, CLOCK_W + 5, 30, 6, PAL_TEXTO);
        gfx->setTextColor(PAL_ALERTA);
    }
    gfx->setTextSize(3);
    gfx->setCursor(CLOCK_X, CLOCK_Y);
    gfx->print(text);
}

// --- CRONÓMETRO: zona de los dígitos dentro de la caja del dashboard ---
static const int CHRONO_Y      = 170;
static const int CHRONO_H      = 40;                  // setTextSize(5): 8 px * 5
static const int CHRONO_CHAR_W = 30;                  // 6 px * 5

// La pestaña CRONO tiene dos modos: cronómetro y temporizador (selector arriba).
static bool chronoShowTimer = false;

void DisplayUI::drawChronoDigits() {
    // Temporizador: se redondea hacia arriba (con 0,4 s restantes todavía muestra 00:01).
    unsigned long ms = chronoShowTimer ? AlarmManager::getTimerRemaining() + 999
                                       : AlarmManager::getChronoTime();
    unsigned long totalSec = ms / 1000;
    int seconds = totalSec % 60;
    int minutes = (totalSec / 60) % 60;
    unsigned long hours = totalSec / 3600;

    // MM:SS durante la primera hora; después H:MM:SS en vez de volver a 00:00.
    char buf[16];
    if (hours > 0) snprintf(buf, sizeof(buf), "%lu:%02d:%02d", hours, minutes, seconds);
    else           snprintf(buf, sizeof(buf), "%02d:%02d", minutes, seconds);

    // Los dígitos se escriben con fondo (sin borrar antes) para que no parpadee; solo se
    // borra la zona cuando cambia el largo (al pasar de MM:SS a H:MM:SS o al resetear).
    static size_t lastLen = 0;
    size_t len = strlen(buf);
    if (len != lastLen) {
        gfx->fillRect(20, CHRONO_Y, LCD_WIDTH - 40, CHRONO_H, PAL_FONDO);   // sin tocar el borde de la caja
        lastLen = len;
    }
    int textW = len * CHRONO_CHAR_W;
    gfx->setTextSize(5);
    gfx->setTextColor(PAL_MENTA, PAL_FONDO);
    gfx->setCursor((LCD_WIDTH - textW) / 2, CHRONO_Y);
    gfx->print(buf);
}

void DisplayUI::refreshChronoDigits() {
    if (PowerManager::isScreenOff()) return;
    if (currentScreen != SCREEN_DASHBOARD || currentTab != TAB_CHRONO) return;
    drawChronoDigits();
}

// Texto tamaño 2 con letras 1 px más juntas de lo normal (11 px en vez de 12), así
// "Sin conexion" (12 letras) entra en la cajita entre el nombre y la hora.
static const int NET_CHAR_ADV = 11;

static void printCondensed(Arduino_GFX* g, int x, int y, const char* text) {
    for (int i = 0; text[i]; i++) {
        g->setCursor(x + i * NET_CHAR_ADV, y);
        g->print(text[i]);
    }
}

// Cajita de conexión del encabezado: "Conectado" en menta si hay internet,
// "Sin conexion" en coral en cualquier otro caso (sin WiFi o WiFi sin internet).
void DisplayUI::drawNetBox() {
    bool connected = (NetStatus::get() == NET_ONLINE);
    const char* label = connected ? "Conectado" : "Sin conexion";

    // Se borra con el color del perfil para no dejar restos del estado anterior.
    gfx->fillRect(WIFI_BOX_X, WIFI_BOX_Y, WIFI_BOX_W, WIFI_BOX_H, profiles[activeProfile].color);
    gfx->fillRoundRect(WIFI_BOX_X, WIFI_BOX_Y, WIFI_BOX_W, WIFI_BOX_H, 6, connected ? PAL_OK : PAL_ALERTA);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(2);
    int textW = strlen(label) * NET_CHAR_ADV - 1;
    printCondensed(gfx, WIFI_BOX_X + (WIFI_BOX_W - textW) / 2, WIFI_BOX_Y + (WIFI_BOX_H - 16) / 2, label);
}

void DisplayUI::refreshNetStatus(bool wifiChanged) {
    if (PowerManager::isScreenOff() || currentScreen != SCREEN_DASHBOARD) return;
    // Las pestañas ya no dependen del WiFi (MÚSICA va por Bluetooth): alcanza con la cajita.
    drawNetBox();
}

// --- PESTAÑA MÚSICA (dentro de la caja 15..395 x 110..410) ---
// Fila de arriba: anterior, play/pausa, siguiente. Fila de abajo: volumen - y +.
struct MusicButton { int x, y, w, h; const char* label; MediaRemote::Key key; };
static const MusicButton MUSIC_BUTTONS[] = {
    {  40, 215, 100, 80, "|<<", MediaRemote::PREV },
    { 155, 215, 100, 80, ">||", MediaRemote::PLAY_PAUSE },
    { 270, 215, 100, 80, ">>|", MediaRemote::NEXT },
    {  40, 315, 155, 70, "VOL-", MediaRemote::VOL_DOWN },
    { 215, 315, 155, 70, "VOL+", MediaRemote::VOL_UP },
};
static const int MUSIC_BUTTON_COUNT = sizeof(MUSIC_BUTTONS) / sizeof(MUSIC_BUTTONS[0]);
static bool lastMusicConnected = false;

static void drawMusicButton(Arduino_GFX* g, const MusicButton& b, uint16_t fill, uint16_t text) {
    g->fillRoundRect(b.x, b.y, b.w, b.h, 12, fill);
    g->setTextColor(text);
    g->setTextSize(3);
    int tw = strlen(b.label) * 18;
    g->setCursor(b.x + (b.w - tw) / 2, b.y + (b.h - 24) / 2);
    g->print(b.label);
}

// Imprime hasta maxChars caracteres; si el texto es más largo, lo corta con "..".
static void printFit(Arduino_GFX* g, const char* s, int maxChars) {
    int n = strlen(s);
    if (n <= maxChars) { g->print(s); return; }
    for (int i = 0; i < maxChars - 2; i++) g->print(s[i]);
    g->print("..");
}

static uint32_t lastTrackVersion = 0;

// Zona de arriba de la pestaña: la canción que suena (o cómo vincular el reloj).
static void drawMusicStatus(Arduino_GFX* g) {
    bool conn = MediaRemote::isConnected();
    lastMusicConnected = conn;
    lastTrackVersion = MediaRemote::trackInfoVersion();
    g->fillRect(20, 118, LCD_WIDTH - 40, 90, PAL_FONDO);   // zona del estado
    g->setTextSize(2);
    if (conn && MediaRemote::title()[0]) {
        // Entran 29 letras de 12 px entre x=30 y x=380.
        g->setTextColor(PAL_CELESTE);
        g->setCursor(30, 128); printFit(g, MediaRemote::title(), 29);
        g->setTextColor(PAL_LILA);
        g->setCursor(30, 154); printFit(g, MediaRemote::artist(), 29);
        g->setTextColor(MediaRemote::isPlaying() ? PAL_MENTA : PAL_AMARILLO);
        g->setCursor(30, 182); g->print(MediaRemote::isPlaying() ? "Sonando" : "En pausa");
    } else if (conn) {
        g->setTextColor(PAL_MENTA);
        g->setCursor(30, 130); g->print("Conectado al celular");
        g->setTextColor(PAL_LILA);
        g->setCursor(30, 160); g->print("Pone algo en Spotify u");
        g->setCursor(30, 182); g->print("otra app de musica");
    } else {
        g->setTextColor(PAL_AMARILLO);
        g->setCursor(30, 130); g->print("Sin conectar");
        g->setTextColor(PAL_TEXTO_CLARO);
        g->setCursor(30, 160); g->print("En el celular: Ajustes >");
        g->setCursor(30, 182); g->print("Bluetooth > JARVIS Reloj");
    }
}

static void drawMusicTab(Arduino_GFX* g) {
    bool conn = MediaRemote::isConnected();
    drawMusicStatus(g);
    for (int i = 0; i < MUSIC_BUTTON_COUNT; i++) {
        if (conn) drawMusicButton(g, MUSIC_BUTTONS[i], PAL_ACENTO, PAL_TEXTO);
        else      drawMusicButton(g, MUSIC_BUTTONS[i], PAL_TARJETA, PAL_LILA);   // deshabilitados
    }
}

// --- Toques en los botones de música (sin bloquear) ---
// handleTouch se llama en cada vuelta del loop mientras el dedo está apoyado. Si el
// último toque fue hace menos de MUSIC_HOLD_GAP_MS es el mismo dedo todavía apoyado.
static const unsigned long MUSIC_HOLD_GAP_MS  = 90;
static const unsigned long MUSIC_REPEAT_AFTER = 450;   // mantener volumen: empieza a repetir...
static const unsigned long MUSIC_REPEAT_EVERY = 150;   // ...y repite cada 150 ms
static const unsigned long MUSIC_FLASH_MS     = 200;   // blanco al apretar: en menta casi no se distinguía del celeste
static int musicHeld = -1;                 // botón que se está apretando
static unsigned long musicLastTouch = 0, musicHeldSince = 0, musicLastSend = 0;
static int musicFlash = -1;                // botón pintado de blanco
static unsigned long musicFlashUntil = 0;

static void musicSend(Arduino_GFX* g, int i) {
    const MusicButton& b = MUSIC_BUTTONS[i];
    MediaRemote::press(b.key);
    musicLastSend = millis();
    drawMusicButton(g, b, PAL_TEXTO_CLARO, PAL_TEXTO);
    musicFlash = i;
    musicFlashUntil = millis() + MUSIC_FLASH_MS;
}

void DisplayUI::handleMusicButton(int i) {
    unsigned long now = millis();
    bool sameFinger = (musicHeld == i) && (now - musicLastTouch < MUSIC_HOLD_GAP_MS);
    musicLastTouch = now;
    if (!sameFinger) {                       // toque nuevo: se manda ya
        musicHeld = i;
        musicHeldSince = now;
        musicSend(gfx, i);
        return;
    }
    MediaRemote::Key k = MUSIC_BUTTONS[i].key;
    bool isVolume = (k == MediaRemote::VOL_UP || k == MediaRemote::VOL_DOWN);
    if (isVolume && now - musicHeldSince > MUSIC_REPEAT_AFTER && now - musicLastSend > MUSIC_REPEAT_EVERY) {
        musicSend(gfx, i);                   // volumen mantenido: repite
    }
}

// Vuelve a pintar de celeste el botón que titiló (se llama en cada vuelta del loop).
void DisplayUI::musicTick() {
    if (musicFlash < 0 || millis() < musicFlashUntil) return;
    int i = musicFlash;
    musicFlash = -1;
    if (PowerManager::isScreenOff() || currentScreen != SCREEN_DASHBOARD || currentTab != TAB_MUSIC) return;
    drawMusicButton(gfx, MUSIC_BUTTONS[i], MediaRemote::isConnected() ? PAL_ACENTO : PAL_TARJETA,
                    MediaRemote::isConnected() ? PAL_TEXTO : PAL_LILA);
}

// Si cambia el estado de la conexión Bluetooth con la pestaña a la vista, se redibuja.
void DisplayUI::refreshMusicTab() {
    if (PowerManager::isScreenOff() || currentScreen != SCREEN_DASHBOARD || currentTab != TAB_MUSIC) return;
    if (MediaRemote::isConnected() != lastMusicConnected) drawMusicTab(gfx);   // botones incluidos
    else if (MediaRemote::trackInfoVersion() != lastTrackVersion) drawMusicStatus(gfx);   // solo la canción
}

void DisplayUI::drawDashboard() {
    if (PowerManager::isScreenOff()) return;
    gfx->fillScreen(PAL_FONDO);

    gfx->fillRect(0, 0, LCD_WIDTH, HEADER_H, profiles[activeProfile].color);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(2);
    gfx->setCursor(NAME_X, NAME_Y);
    gfx->printf("%s", profiles[activeProfile].name);

    drawNetBox();
    lastClockSynced = -1;          // el encabezado se acaba de pintar: forzar la hora
    drawHeaderClockOnly();

    // Siempre 4 pestañas (antes VOZ aparecía solo con WiFi).
    drawTab(gfx, 15,                TAB_W, 25,                "AGENDA",  currentTab == TAB_AGENDA);
    drawTab(gfx, 20 + TAB_W,        TAB_W, 30 + TAB_W,        "CRONO",   currentTab == TAB_CHRONO);
    drawTab(gfx, 25 + (TAB_W * 2),  TAB_W, 30 + (TAB_W * 2),  "+ALARMA", currentTab == TAB_ADD_ALARM);
    drawTab(gfx, 30 + (TAB_W * 3),  TAB_W, 38 + (TAB_W * 3),  "MUSICA",  currentTab == TAB_MUSIC);

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
        // Selector de modo: [CRONOMETRO | TEMPORIZADOR]
        gfx->setTextSize(2);
        gfx->fillRoundRect(25, 118, 178, 36, 8, chronoShowTimer ? PAL_TARJETA : PAL_ACENTO);
        gfx->fillRoundRect(207, 118, 178, 36, 8, chronoShowTimer ? PAL_ACENTO : PAL_TARJETA);
        gfx->setTextColor(chronoShowTimer ? PAL_TEXTO_CLARO : PAL_TEXTO);
        gfx->setCursor(25 + (178 - 10 * 12) / 2, 128); gfx->print("CRONOMETRO");
        gfx->setTextColor(chronoShowTimer ? PAL_TEXTO : PAL_TEXTO_CLARO);
        gfx->setCursor(207 + (178 - 12 * 12) / 2, 128); gfx->print("TEMPORIZADOR");

        drawChronoDigits();

        if (chronoShowTimer) {
            // Botones para cargar el tiempo.
            const char* labels[3] = { "+1 min", "+5 min", "+10 min" };
            for (int i = 0; i < 3; i++) {
                int x = 30 + i * 120;
                gfx->fillRoundRect(x, 225, 110, 45, 10, PAL_LILA);
                gfx->setTextColor(PAL_TEXTO);
                gfx->setCursor(x + (110 - strlen(labels[i]) * 12) / 2, 240);
                gfx->print(labels[i]);
            }
        }

        bool running = chronoShowTimer ? AlarmManager::isTimerRunning() : AlarmManager::isChronoRunning();
        gfx->fillRoundRect(45, 290, 130, 50, 10, running ? PAL_ALERTA : PAL_OK);
        gfx->setTextColor(PAL_TEXTO);
        gfx->setTextSize(2);
        const char* action = running ? "PAUSA" : "INICIAR";
        gfx->setCursor(45 + (130 - strlen(action) * 12) / 2, 305);
        gfx->print(action);

        gfx->fillRoundRect(235, 290, 130, 50, 10, PAL_LILA);
        gfx->setCursor(235 + (130 - 5 * 12) / 2, 305);
        gfx->print("RESET");
    } else if (currentTab == TAB_ADD_ALARM) {
        gfx->setTextColor(PAL_TEXTO_CLARO);
        gfx->setTextSize(2);
        gfx->setCursor(30, 125); gfx->print("Nueva Alarma:");

        gfx->setTextSize(5);
        gfx->setTextColor(PAL_CELESTE);
        gfx->setCursor(40, 155);
        gfx->printf("%02d:%02d", tempHour, tempMinute);

        // (El volumen de la alarma pasó a la pantalla AJUSTES.)
        gfx->setTextColor(PAL_TEXTO);
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
    } else if (currentTab == TAB_MUSIC) {
        drawMusicTab(gfx);
    }

    drawBottomBar();
}

// --- BARRA INFERIOR DEL DASHBOARD (debajo de la caja, que termina en y=410) ---
// Izquierda: pasos y distancia. Centro: batería y velocidad. Derecha: botón Salir.
// Abajo las esquinas redondeadas tapan ~35-45 px, por eso nada pasa de x 55..350.
static const int BAR_Y        = 428;
static const int BAR_H        = 48;
static const int STEPS_X      = 55;
static const int BATT_X       = 192;      // también la velocidad, en el renglón de abajo
static const int SALIR_X      = 282;
static const int SALIR_W      = 68;

// Color de la batería: menta >50 %, amarillo 20-50 %, coral <20 %, celeste cargando.
static uint16_t batteryColor() {
    int pct = PowerManager::batteryPercent();
    return PowerManager::isCharging() ? PAL_CELESTE
         : pct < 0   ? PAL_LILA
         : pct > 50  ? PAL_OK
         : pct >= 20 ? PAL_AMARILLO
         : PAL_ALERTA;
}

// "85%", "85%+" cargando, "--" sin batería.
static void batteryText(char* buf, size_t len) {
    int pct = PowerManager::batteryPercent();
    if (pct < 0) snprintf(buf, len, "--");
    else snprintf(buf, len, "%d%%%s", pct, PowerManager::isCharging() ? "+" : "");
}

static void drawBattery(Arduino_GFX* g, int x, int y) {
    char buf[8];
    batteryText(buf, sizeof(buf));
    g->setTextColor(batteryColor());
    g->setTextSize(2);
    g->setCursor(x, y);
    g->print(buf);
}

// Para que no parpadee, cada dato de la barra se escribe con su propio fondo
// (sin borrar antes) y rellenado con espacios hasta un ancho fijo, y solo si cambió.
enum BarField { BF_STEPS, BF_KM, BF_BATT, BF_SPEED, BF_COUNT };
static char barText[BF_COUNT][14];
static uint16_t barColor[BF_COUNT];

static void drawBarField(Arduino_GFX* g, BarField f, int x, int y, int widthChars,
                         const char* text, uint16_t color, bool force) {
    if (!force && barColor[f] == color && strcmp(barText[f], text) == 0) return;
    strncpy(barText[f], text, sizeof(barText[f]) - 1);
    barText[f][sizeof(barText[f]) - 1] = 0;
    barColor[f] = color;
    g->setTextColor(color, PAL_FONDO);
    g->setTextSize(2);
    g->setCursor(x, y);
    g->printf("%-*s", widthChars, text);
}

// Pasos, km y velocidad solo se muestran en la pestaña CRONO; en las demás la barra
// tiene solo la batería y el botón Salir.
static void updateBarFields(Arduino_GFX* g, bool force, bool showSteps) {
    char buf[16];
    if (!showSteps) {
        // (el dashboard se acaba de pintar de negro, no hay nada que borrar)
    } else if (StepCounter::available()) {
        snprintf(buf, sizeof(buf), "%lu pasos", (unsigned long)StepCounter::steps());
        drawBarField(g, BF_STEPS, STEPS_X, BAR_Y + 2, 11, buf, PAL_MENTA, force);
        snprintf(buf, sizeof(buf), "%.2f km", StepCounter::distanceKm());
        drawBarField(g, BF_KM, STEPS_X, BAR_Y + 26, 11, buf, PAL_TEXTO_CLARO, force);

        // Velocidad estimada con el ritmo de pasos (sin GPS). Hasta 7 caracteres.
        float v = StepCounter::speedKmh();
        if (v >= 10) snprintf(buf, sizeof(buf), "%.0fkm/h", v);
        else snprintf(buf, sizeof(buf), "%.1fkm/h", v);
        drawBarField(g, BF_SPEED, BATT_X, BAR_Y + 26, 7, buf, v > 0 ? PAL_CELESTE : PAL_LILA, force);
    } else {
        drawBarField(g, BF_STEPS, STEPS_X, BAR_Y + 16, 11, "sin sensor", PAL_LILA, force);
    }
    batteryText(buf, sizeof(buf));
    drawBarField(g, BF_BATT, BATT_X, BAR_Y + 2, 7, buf, batteryColor(), force);
}

// Dibujo completo (al armar el dashboard, que ya pintó el fondo de negro).
void DisplayUI::drawBottomBar() {
    updateBarFields(gfx, true, currentTab == TAB_CHRONO);

    gfx->fillRoundRect(SALIR_X, BAR_Y, SALIR_W, 45, 12, PAL_ALERTA);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(2);
    gfx->setCursor(SALIR_X + (SALIR_W - 5 * 12) / 2, BAR_Y + 15);
    gfx->print("Salir");
}

// Actualización periódica: solo los datos que cambiaron, sin tocar el botón.
void DisplayUI::refreshBottomBar() {
    if (PowerManager::isScreenOff() || currentScreen != SCREEN_DASHBOARD) return;
    updateBarFields(gfx, false, currentTab == TAB_CHRONO);
}

void DisplayUI::showSleepMessage() {
    if (PowerManager::isScreenOff()) return;
    gfx->fillScreen(PAL_FONDO);
    gfx->setTextColor(PAL_LILA);
    gfx->setTextSize(3);
    gfx->setCursor((LCD_WIDTH - 11 * 18) / 2, 200);
    gfx->print("Modo dormir");
    gfx->setTextColor(PAL_TEXTO_CLARO);
    gfx->setTextSize(2);
    gfx->setCursor((LCD_WIDTH - 19 * 12) / 2, 250);
    gfx->print("Toca para despertar");
}

void DisplayUI::drawWifiPortalScreen() {
    if (PowerManager::isScreenOff()) return;   // se dibuja al encender
    gfx->fillScreen(PAL_FONDO);
    gfx->setTextColor(PAL_CELESTE);
    gfx->setTextSize(2);
    gfx->setCursor((LCD_WIDTH - 15 * 12) / 2, 40);   // centrado: en x=35 lo tapaba la esquina
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

// Pantalla que suena (alarma o temporizador). Bloquea hasta que se toca un botón y
// devuelve true si se eligió POSPONER. Al terminar vuelve a la pantalla que había.
bool DisplayUI::ringScreen(const char* title, const char* timeText, bool allowSnooze) {
    // Si la pantalla estaba apagada, se enciende (sin redibujar: esta pantalla dibuja la suya).
    PowerManager::turnScreenOn(false);

    ScreenState previousScreen = currentScreen;
    currentScreen = SCREEN_ALARM_RINGING;
    // Pantalla completa en coral: es lo más llamativo de la paleta y contrasta
    // con el resto de la interfaz, que es casi toda negra.
    gfx->fillScreen(PAL_ALERTA);
    gfx->setTextColor(PAL_TEXTO);
    gfx->setTextSize(4);
    gfx->setCursor((LCD_WIDTH - strlen(title) * 24) / 2, 50); gfx->print(title);
    gfx->setTextSize(6);
    gfx->setCursor((LCD_WIDTH - strlen(timeText) * 36) / 2, 120); gfx->print(timeText);

    // Cajas entre x 40..370 y hasta y=445, lejos de las esquinas redondeadas de abajo.
    const int SNOOZE_Y = 220, SNOOZE_H = 95;
    const int OFF_Y = allowSnooze ? 330 : 220;
    const int OFF_H = 445 - OFF_Y;
    if (allowSnooze) {
        gfx->fillRoundRect(40, SNOOZE_Y, LCD_WIDTH - 80, SNOOZE_H, 20, PAL_TEXTO_CLARO);
        gfx->setTextColor(PAL_TEXTO);
        gfx->setTextSize(3);
        gfx->setCursor((LCD_WIDTH - 8 * 18) / 2, SNOOZE_Y + 22); gfx->print("POSPONER");
        gfx->setTextSize(2);
        gfx->setCursor((LCD_WIDTH - 5 * 12) / 2, SNOOZE_Y + 60); gfx->print("5 min");
    }
    gfx->fillRoundRect(40, OFF_Y, LCD_WIDTH - 80, OFF_H, 20, PAL_TEXTO);
    gfx->setTextColor(PAL_ALERTA);
    gfx->setTextSize(4);
    gfx->setCursor((LCD_WIDTH - 10 * 24) / 2, OFF_Y + (OFF_H - 32) / 2); gfx->print("DESACTIVAR");

    bool snoozed = false;
    while (true) {
        AudioManager::playTone(AudioManager::masterVolume);
        int16_t tx, ty;
        if (TouchDriver::getTouch(tx, ty)) {
            if (allowSnooze && ty >= SNOOZE_Y && ty <= SNOOZE_Y + SNOOZE_H) { snoozed = true; break; }
            if (ty >= OFF_Y) break;
        }
    }
    TouchDriver::waitForRelease();
    AudioManager::stopTone();

    currentScreen = previousScreen;
    PowerManager::notifyActivity();   // esto bloquea el loop: reiniciar la inactividad
    refreshCurrentScreen();
    return snoozed;
}

void DisplayUI::triggerAlarmRinging(int alarmIdx) {
    char t[8];
    snprintf(t, sizeof(t), "%02d:%02d", AlarmManager::alarmList[alarmIdx].hour, AlarmManager::alarmList[alarmIdx].minute);
    if (ringScreen("ALARMA!", t, true)) AlarmManager::snooze(alarmIdx, 5);
    // Si se desactiva, la alarma sigue activa: vuelve a sonar mañana a la misma hora.
}

void DisplayUI::handleTouch(int16_t tx, int16_t ty) {
    if (currentScreen == SCREEN_SETTINGS) {
        auto inBtn = [&](int x, int y, int w, int h) { return tx >= x && tx <= x + w && ty >= y && ty <= y + h; };
        if (inBtn(SET_MINUS_X, SET_BRIGHT_Y, SET_BTN_W, SET_BTN_H)) Settings::setBrightness(Settings::brightness() - 1);
        else if (inBtn(SET_PLUS_X, SET_BRIGHT_Y, SET_BTN_W, SET_BTN_H)) Settings::setBrightness(Settings::brightness() + 1);
        else if (inBtn(SET_MINUS_X, SET_VOLUME_Y, SET_BTN_W, SET_BTN_H)) Settings::setAlarmVolume(Settings::alarmVolume() - 1);
        else if (inBtn(SET_PLUS_X, SET_VOLUME_Y, SET_BTN_W, SET_BTN_H)) Settings::setAlarmVolume(Settings::alarmVolume() + 1);
        else if (inBtn(60, SET_WIFI_Y, 290, 55)) { TouchDriver::waitForRelease(); WiFiPortal::startPortal(); return; }
        else if (inBtn(100, SET_BACK_Y, 210, 55)) {
            TouchDriver::waitForRelease();
            currentScreen = SCREEN_PROFILES;
            drawProfileMenu();
            return;
        } else return;
        drawSettingValues(gfx);                  // solo cambian los números
        TouchDriver::waitForRelease();
        return;
    }

    if (currentScreen == SCREEN_WIFI_CONFIG) {
        if (tx >= 50 && tx <= 350 && ty >= 340 && ty <= 410) {
            TouchDriver::waitForRelease();
            WiFiPortal::stopPortal();      // vuelve al menú y a buscar las redes conocidas
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
            currentScreen = SCREEN_SETTINGS;
            drawSettingsScreen();
            return;
        }
    } else if (currentScreen == SCREEN_DASHBOARD) {
        if (ty >= 65 && ty <= 103) {
            if (tx >= 15 && tx <= 15 + TAB_W) currentTab = TAB_AGENDA;
            else if (tx >= 20 + TAB_W && tx <= 20 + (TAB_W * 2)) currentTab = TAB_CHRONO;
            else if (tx >= 25 + (TAB_W * 2) && tx <= 25 + (TAB_W * 3)) currentTab = TAB_ADD_ALARM;
            else if (tx >= 30 + (TAB_W * 3) && tx <= 390) currentTab = TAB_MUSIC;

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
                // Tocar la hora (o el texto ACTIVA/INACTIVA) la activa o desactiva.
                if (tx >= 30 && tx <= 300 && ty >= yPos && ty <= yPos + 40) {
                    TouchDriver::waitForRelease();
                    AlarmManager::toggleAlarm(i);
                    drawDashboard();
                    return;
                }
            }
        }

        if (currentTab == TAB_CHRONO) {
            bool handled = false;
            if (ty >= 118 && ty <= 154) {                         // selector de modo
                chronoShowTimer = (tx >= 205);
                handled = true;
            } else if (chronoShowTimer && ty >= 225 && ty <= 270) {   // +1 / +5 / +10 min
                static const int MINUTES[3] = { 1, 5, 10 };
                for (int i = 0; i < 3; i++) {
                    int x = 30 + i * 120;
                    if (tx >= x && tx <= x + 110) { AlarmManager::timerAdd(MINUTES[i]); handled = true; }
                }
            } else if (ty >= 290 && ty <= 340) {                  // INICIAR/PAUSA y RESET
                if (tx >= 45 && tx <= 175) {
                    if (chronoShowTimer) AlarmManager::toggleTimer(); else AlarmManager::toggleChrono();
                    handled = true;
                } else if (tx >= 235 && tx <= 365) {
                    if (chronoShowTimer) AlarmManager::resetTimer(); else AlarmManager::resetChrono();
                    handled = true;
                }
            }
            if (handled) {
                TouchDriver::waitForRelease();
                drawDashboard();
                return;
            }
        }

        if (currentTab == TAB_ADD_ALARM) {
            // GUARDAR va primero: ocupa la misma franja vertical que las filas de +/−,
            // y si esas se revisaban antes se quedaban con el toque.
            if (tx >= 235 && tx <= 385 && ty >= 220 && ty <= 330) {
                AlarmManager::addAlarm(tempHour, tempMinute);
                currentTab = TAB_AGENDA;
                TouchDriver::waitForRelease();
                drawDashboard();
                return;
            } else if (ty >= 225 && ty <= 270) {
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
            }
        }

        // MÚSICA: la tecla se manda apenas se toca, sin esperar a que se suelte el dedo
        // (antes waitForRelease frenaba hasta 0,8 s y no se podía apretar rápido).
        // Mantener VOL+/VOL- repite la tecla. El botón titila en blanco (ver musicTick).
        if (currentTab == TAB_MUSIC && MediaRemote::isConnected()) {
            for (int i = 0; i < MUSIC_BUTTON_COUNT; i++) {
                const MusicButton& b = MUSIC_BUTTONS[i];
                if (tx >= b.x && tx <= b.x + b.w && ty >= b.y && ty <= b.y + b.h) {
                    handleMusicButton(i);
                    return;
                }
            }
            musicHeld = -1;
        }

        if (tx >= SALIR_X && tx <= SALIR_X + SALIR_W && ty >= BAR_Y && ty <= BAR_Y + 45) {
            TouchDriver::waitForRelease();
            activeProfile = -1;
            currentScreen = SCREEN_PROFILES;
            drawProfileMenu();
            return;
        }
    }
}