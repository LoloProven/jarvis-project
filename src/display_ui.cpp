#include "display_ui.h"
#include "power_manager.h"
#include "touch_driver.h"
#include "alarm_manager.h"
#include "audio_manager.h"
#include "wifi_portal.h"
#include "palette.h"
#include "aa_text.h"
#include "time_sync.h"
#include "net_status.h"
#include "step_counter.h"
#include "media_remote.h"
#include "settings.h"
#include "battery_log.h"
#include "weather.h"
#include "jarvis.h"
#include <WiFi.h>

// Estilo "B · Deportivo" (ver palette.h): fondo negro, texto blanco, etiquetas grises en
// mayúsculas, un solo acento lima y letra Barlow Condensed (aa_text.h).
using AAText::CENTER;
using AAText::RIGHT;

// --- CORREGIDO: inicialización validada contra el ejemplo oficial de Waveshare ---
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
WidgetTab DisplayUI::currentTab = TAB_HOME;
int DisplayUI::activeProfile = -1;
int DisplayUI::tempHour = 18;
int DisplayUI::tempMinute = 0;
unsigned long DisplayUI::lastClockUpdate = 0;

// --- MARCO COMÚN ---
// Contenido entre x 40 y 370. Arriba (y 26) el nombre y la batería, lejos de las esquinas
// redondeadas; abajo los 4 botones con íconos de las secciones.
static const int CX = LCD_WIDTH / 2;
static const int CL = 40, CR = 370, CW = CR - CL;
static const int HDR_Y = 26;                       // renglón de FONT_S del encabezado
static const int HDR_TIME_Y = HDR_Y + 17 - 29;     // la hora (FONT_L) en la misma línea de base
static const int HDR_H = 60;                       // tocar acá arriba: volver a INICIO
static const int NAV_Y = 420, NAV_H = 48, NAV_W = 72, NAV_GAP = 8;
static const int NAV_X0 = (LCD_WIDTH - (4 * NAV_W + 3 * NAV_GAP)) / 2;
static const int CONTENT_Y = 66;                   // primer renglón debajo del encabezado

static Profile profiles[4] = {
    {"Lolo",  PAL_ACENTO,  58,  62, 140, 140},
    {"Delfi", PAL_ACENTO, 212,  62, 140, 140},
    {"Fede",  PAL_ACENTO,  58, 216, 140, 140},
    {"Lucia", PAL_ACENTO, 212, 216, 140, 140}
};
static const int PROF_SET_X = 80, PROF_SET_Y = 380, PROF_SET_W = 250, PROF_SET_H = 56;

static const char* DAYS[7]    = { "DOM", "LUN", "MAR", "MIÉ", "JUE", "VIE", "SÁB" };
static const char* MONTHS[12] = { "ENE", "FEB", "MAR", "ABR", "MAY", "JUN",
                                  "JUL", "AGO", "SEP", "OCT", "NOV", "DIC" };

// --- AYUDAS DE DIBUJO ---

// Borde de 2 px (como en la maqueta) sobre fondo negro.
static void outlineBox(Arduino_GFX* g, int x, int y, int w, int h, int r, uint16_t color) {
    g->fillRoundRect(x, y, w, h, r, PAL_FONDO);
    g->drawRoundRect(x, y, w, h, r, color);
    g->drawRoundRect(x + 1, y + 1, w - 2, h - 2, r - 1, color);
}

// Borde de arriba del renglón para que las mayúsculas queden centradas en una caja de alto h.
static int textTopFor(const AAFont& f, int y, int h) {
    return y + h / 2 - f.ascent * 13 / 20;
}

// Botón: relleno en acento con texto negro, o con borde gris y texto blanco.
static void button(Arduino_GFX* g, int x, int y, int w, int h, const char* label, const AAFont& f, bool filled,
                   uint16_t fill = PAL_ACENTO, uint16_t text = PAL_FONDO) {
    uint16_t bg = filled ? fill : PAL_FONDO;
    if (filled) g->fillRoundRect(x, y, w, h, 14, fill);
    else outlineBox(g, x, y, w, h, 14, PAL_BORDE);
    // El renglón se pinta con fondo: que no tape el borde de 2 px.
    int ty = textTopFor(f, y, h);
    if (ty + f.height > y + h - 3) ty = y + h - 3 - f.height;
    if (ty < y + 3) ty = y + 3;
    AAText::drawCentered(g, f, x + w / 2, ty, label, filled ? text : PAL_BLANCO, bg);
}

// Línea gruesa (para los íconos y la cruz de borrar).
static void thickLine(Arduino_GFX* g, int x0, int y0, int x1, int y1, uint16_t c) {
    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++) g->drawLine(x0 + dx, y0 + dy, x1 + dx, y1 + dy, c);
}

static void ring(Arduino_GFX* g, int cx, int cy, int r, int t, uint16_t c, uint16_t bg) {
    g->fillCircle(cx, cy, r, c);
    g->fillCircle(cx, cy, r - t, bg);
}

// "8432" -> "8.432"
static void thousands(char* buf, size_t len, unsigned long n) {
    if (n >= 1000000) snprintf(buf, len, "%lu.%03lu.%03lu", n / 1000000, (n / 1000) % 1000, n % 1000);
    else if (n >= 1000) snprintf(buf, len, "%lu.%03lu", n / 1000, n % 1000);
    else snprintf(buf, len, "%lu", n);
}

// "6,10 km" (coma decimal; con 10 km o más, un solo decimal).
static void kmText(char* buf, size_t len, float km) {
    snprintf(buf, len, km >= 10 ? "%.1f km" : "%.2f km", km);
    for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
}

// --- ÍCONOS DE LOS BOTONES DE ABAJO (unos 26 px, centrados en cx, cy) ---
static void drawNavIcon(Arduino_GFX* g, int i, int cx, int cy, uint16_t c, uint16_t bg) {
    switch (i) {
        case 0:   // ALARMAS: reloj
            ring(g, cx, cy + 1, 12, 3, c, bg);
            g->fillRect(cx - 1, cy - 6, 3, 8, c);
            thickLine(g, cx, cy + 1, cx + 4, cy + 5, c);
            thickLine(g, cx - 13, cy - 8, cx - 9, cy - 12, c);
            thickLine(g, cx + 13, cy - 8, cx + 9, cy - 12, c);
            break;
        case 1:   // CRONO: cronómetro
            ring(g, cx, cy + 2, 11, 3, c, bg);
            g->fillRect(cx - 5, cy - 14, 11, 3, c);
            g->fillRect(cx - 1, cy - 12, 3, 4, c);
            g->fillRect(cx - 1, cy - 4, 3, 7, c);
            break;
        case 2:   // MÚSICA: dos notas
            g->fillCircle(cx - 7, cy + 8, 5, c);
            g->fillCircle(cx + 8, cy + 6, 5, c);
            g->fillRect(cx - 4, cy - 10, 3, 18, c);
            g->fillRect(cx + 11, cy - 12, 3, 18, c);
            for (int k = 0; k < 4; k++) g->drawLine(cx - 4, cy - 10 + k, cx + 13, cy - 13 + k, c);
            break;
        case 3:   // JARVIS: micrófono
            ring(g, cx, cy - 1, 10, 3, c, bg);
            g->fillRect(cx - 11, cy - 13, 23, 12, bg);       // solo la mitad de abajo del arco
            g->fillRoundRect(cx - 5, cy - 14, 11, 18, 5, c);
            g->fillRect(cx - 1, cy + 9, 3, 5, c);
            g->fillRect(cx - 6, cy + 13, 13, 3, c);
            break;
    }
}

// Qué botón de abajo corresponde a la pestaña (-1 = INICIO, ninguno resaltado).
static int navIndex(WidgetTab t) {
    switch (t) {
        case TAB_AGENDA: case TAB_ADD_ALARM: return 0;
        case TAB_CHRONO: return 1;
        case TAB_MUSIC:  return 2;
        case TAB_JARVIS: return 3;
        default:         return -1;
    }
}

static void drawNav(Arduino_GFX* g, WidgetTab tab) {
    int active = navIndex(tab);
    for (int i = 0; i < 4; i++) {
        int x = NAV_X0 + i * (NAV_W + NAV_GAP);
        bool on = (i == active);
        if (on) g->fillRoundRect(x, NAV_Y, NAV_W, NAV_H, 14, PAL_ACENTO);
        else outlineBox(g, x, NAV_Y, NAV_W, NAV_H, 14, PAL_BORDE);
        uint16_t bg = on ? PAL_ACENTO : PAL_FONDO;
        drawNavIcon(g, i, x + NAV_W / 2, NAV_Y + NAV_H / 2, on ? PAL_FONDO : PAL_BLANCO, bg);
    }
}

// --- BATERÍA Y CONEXIÓN (encabezado) ---

// Gris normal, lima cargando, rojo con menos de 20 %.
static uint16_t batteryColor() {
    int pct = PowerManager::batteryPercent();
    if (PowerManager::isCharging()) return PAL_ACENTO;
    if (pct >= 0 && pct < 20) return PAL_ALERTA;
    return PAL_GRIS;
}

// "85%", "85%+" cargando, "--" sin batería.
static void batteryText(char* buf, size_t len) {
    int pct = PowerManager::batteryPercent();
    if (pct < 0) snprintf(buf, len, "--");
    else snprintf(buf, len, "%d%%%s", pct, PowerManager::isCharging() ? "+" : "");
}

// Lo que ya está dibujado, para redibujar solo lo que cambia (si no, parpadea).
static char shownBatt[8];
static uint16_t shownBattColor;
static int shownNet = -1;
static char shownTime[12];
static char shownDate[20];
static char shownSteps[16];
static char shownKm[16];
static char shownAlarm[16];
static int shownBar = -1;
static uint32_t shownWeather = 0xFFFFFFFF;

static void forgetShown() {
    shownBatt[0] = shownTime[0] = shownDate[0] = shownSteps[0] = shownKm[0] = shownAlarm[0] = 0;
    shownBattColor = 0;
    shownNet = -1;
    shownBar = -1;
    shownWeather = 0xFFFFFFFF;
}

static const int BATT_RIGHT = 350, BATT_BOX_W = 56;

static void drawHeaderBattery(Arduino_GFX* g, bool force) {
    char buf[8];
    batteryText(buf, sizeof(buf));
    uint16_t color = batteryColor();
    if (!force && color == shownBattColor && strcmp(buf, shownBatt) == 0) return;
    strlcpy(shownBatt, buf, sizeof(shownBatt));
    shownBattColor = color;
    AAText::drawBox(g, FONT_S, BATT_RIGHT - BATT_BOX_W, HDR_Y, BATT_BOX_W, buf, color, PAL_FONDO, RIGHT, 1);
}

// Puntito a la izquierda de la batería: lima con internet, rojo sin conexión.
static void drawNetDot(Arduino_GFX* g, bool force) {
    int net = NetStatus::get() == NET_ONLINE ? 1 : 0;
    if (!force && net == shownNet) return;
    shownNet = net;
    g->fillCircle(BATT_RIGHT - BATT_BOX_W - 8, HDR_Y + 10, 4, net ? PAL_ACENTO : PAL_ALERTA);
}

// Hora "HH:MM" y si es confiable (si nunca llegó por NTP, se muestra en rojo).
static bool clockText(char* buf, size_t len) {
    struct tm t;
    if (!getLocalTime(&t, 0)) { snprintf(buf, len, "--:--"); return false; }
    snprintf(buf, len, "%02d:%02d", t.tm_hour, t.tm_min);
    return true;
}

static void dateText(char* buf, size_t len) {
    struct tm t;
    if (!getLocalTime(&t, 0)) { buf[0] = 0; return; }
    snprintf(buf, len, "%s %d %s", DAYS[t.tm_wday], t.tm_mday, MONTHS[t.tm_mon]);
}

void DisplayUI::init() {
    gfx->begin();
    Settings::applyBrightness();   // brillo guardado (Settings::load() ya se llamó)
}

Arduino_GFX* DisplayUI::getGFX() { return gfx; }
ScreenState DisplayUI::getCurrentScreen() { return currentScreen; }
void DisplayUI::setScreenState(ScreenState state) { currentScreen = state; }

String DisplayUI::getFormattedTime() {
    char buf[12];
    clockText(buf, sizeof(buf));
    return String(buf);
}

void DisplayUI::refreshCurrentScreen() {
    if (currentScreen == SCREEN_PROFILES) drawProfileMenu();
    else if (currentScreen == SCREEN_DASHBOARD) drawDashboard();
    else if (currentScreen == SCREEN_WIFI_CONFIG) drawWifiPortalScreen();
    else if (currentScreen == SCREEN_SETTINGS) drawSettingsScreen();
    else if (currentScreen == SCREEN_BATTERY) drawBatteryScreen();
}

// --- MENÚ DE PERFILES ---
void DisplayUI::drawAvatar(int x, int y, uint16_t color, const char* name) {
    outlineBox(gfx, x, y, 140, 140, 18, PAL_BORDE);
    char initial[2] = { name[0], 0 };
    AAText::drawCentered(gfx, FONT_XXL, x + 70, y + 14, initial, color, PAL_FONDO);
    char upper[12];
    int i = 0;
    for (; name[i] && i < 11; i++) upper[i] = toupper(name[i]);
    upper[i] = 0;
    AAText::drawCentered(gfx, FONT_M, x + 70, y + 100, upper, PAL_GRIS, PAL_FONDO, 2);
}

void DisplayUI::drawProfileMenu() {
    if (PowerManager::isScreenOff()) return;   // se dibuja al encender
    gfx->fillScreen(PAL_FONDO);
    char buf[8];
    batteryText(buf, sizeof(buf));
    AAText::drawCentered(gfx, FONT_S, CX, HDR_Y, buf, batteryColor(), PAL_FONDO, 1);
    for (int i = 0; i < 4; i++) {
        drawAvatar(profiles[i].x, profiles[i].y, profiles[i].color, profiles[i].name);
    }
    button(gfx, PROF_SET_X, PROF_SET_Y, PROF_SET_W, PROF_SET_H, "AJUSTES", FONT_M, false);
}

// --- PANTALLA AJUSTES ---
// Cada ajuste: etiqueta y una fila [ - ]  valor  [ + ] con botones grandes.
static const int SET_MINUS_X = 50, SET_PLUS_X = 270, SET_BTN_W = 90, SET_BTN_H = 56;
static const int SET_BRIGHT_Y = 96, SET_VOLUME_Y = 198;       // fila de botones de cada ajuste
static const int SET_WIFI_X = 50, SET_BATT_X = 210, SET_ROW2_W = 150, SET_WIFI_Y = 282;
static const int SET_BACK_X = 100, SET_BACK_W = 210, SET_BACK_Y = 362, SET_ROW_H = 56;

static void drawSettingRow(Arduino_GFX* g, const char* title, int y, const char* value) {
    AAText::drawCentered(g, FONT_S, CX, y - 28, title, PAL_GRIS, PAL_FONDO, 2);
    button(g, SET_MINUS_X, y, SET_BTN_W, SET_BTN_H, "-", FONT_XL, false);
    button(g, SET_PLUS_X, y, SET_BTN_W, SET_BTN_H, "+", FONT_XL, false);
    // Valor en el medio, en una caja de ancho fijo (cambia de largo sin dejar restos).
    int bx = SET_MINUS_X + SET_BTN_W + 4, bw = SET_PLUS_X - bx - 4;
    AAText::drawBox(g, FONT_L, bx, textTopFor(FONT_L, y, SET_BTN_H), bw, value, PAL_BLANCO, PAL_FONDO, CENTER);
}

static void drawSettingValues(Arduino_GFX* g) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", Settings::brightness() * 100 / Settings::BRIGHTNESS_LEVELS);
    drawSettingRow(g, "BRILLO", SET_BRIGHT_Y, buf);
    snprintf(buf, sizeof(buf), "%d/10", Settings::alarmVolume());
    drawSettingRow(g, "VOLUMEN DE LA ALARMA", SET_VOLUME_Y, buf);
}

void DisplayUI::drawSettingsScreen() {
    if (PowerManager::isScreenOff()) return;
    gfx->fillScreen(PAL_FONDO);
    AAText::drawCentered(gfx, FONT_L, CX, 20, "AJUSTES", PAL_BLANCO, PAL_FONDO, 2);
    drawSettingValues(gfx);
    button(gfx, SET_WIFI_X, SET_WIFI_Y, SET_ROW2_W, SET_ROW_H, "WIFI", FONT_M, false);
    button(gfx, SET_BATT_X, SET_WIFI_Y, SET_ROW2_W, SET_ROW_H, "BATERÍA", FONT_M, false);
    button(gfx, SET_BACK_X, SET_BACK_Y, SET_BACK_W, SET_ROW_H, "VOLVER", FONT_M, true);
}

// --- PANTALLA BATERIA ---
// Gasto por hora en cada estado y los últimos tramos de 15 min del registro, el más
// nuevo arriba. mV = voltaje; P/A/D = minutos con la pantalla prendida, apagada y en modo
// dormir; S% = del tiempo en modo dormir, cuánto durmió de verdad el chip; C = enchufado o
// cargando (no cuenta para el gasto). La tabla usa la letra de ancho fijo de la pantalla
// (tamaño 2) para que las columnas queden alineadas.
static const int BAT_BTN_Y = 400, BAT_BTN_H = 50;
static const int BAT_ROWS = 8;

void DisplayUI::drawBatteryScreen() {
    if (PowerManager::isScreenOff()) return;
    gfx->fillScreen(PAL_FONDO);
    AAText::drawCentered(gfx, FONT_L, CX, 20, "BATERÍA", PAL_BLANCO, PAL_FONDO, 2);

    gfx->setTextSize(2);
    gfx->setTextColor(PAL_GRIS);
    gfx->setCursor(50, 62);
    gfx->print("Gasto por hora:");
    static const char* names[] = {"Prendida", "Apagada ", "Dormido "};
    char buf[32];
    for (int s = 0; s < 3; s++) {
        float r = BatteryLog::ratePerHour(s);
        if (r < 0) snprintf(buf, sizeof(buf), "%s  sin datos", names[s]);
        else       snprintf(buf, sizeof(buf), "%s  %.1f %%/h", names[s], r);
        gfx->setTextColor(PAL_BLANCO);
        gfx->setCursor(50, 85 + s * 22);
        gfx->print(buf);
    }

    gfx->setTextColor(PAL_ACENTO);
    gfx->setCursor(30, 160);
    gfx->print("Hora     %   mV  P  A  D  S%");
    gfx->setTextColor(PAL_BLANCO);
    int n = BatteryLog::count();
    if (n == 0) {
        gfx->setCursor(50, 185);
        gfx->print("Cada 15 min se anota");
        gfx->setCursor(50, 205);
        gfx->print("una linea.");
    }
    for (int row = 0; row < BAT_ROWS && row < n; row++) {
        const BatteryLog::Entry& e = BatteryLog::get(n - 1 - row);
        time_t t = e.when;
        struct tm tmv;
        localtime_r(&t, &tmv);
        snprintf(buf, sizeof(buf), "%02d:%02d %3d%% %4d %2d %2d %2d %3d%s", tmv.tm_hour, tmv.tm_min, e.pct,
                 e.mv, e.onMin, e.offMin, e.sleepMin, e.asleepPct, e.charging ? " C" : "");
        gfx->setCursor(30, 185 + row * 20);
        gfx->print(buf);
    }
    gfx->setTextColor(PAL_GRIS);
    gfx->setCursor(30, 350);
    gfx->print("P prend. A apag. D dormido");
    gfx->setCursor(30, 372);
    gfx->print("S% = durmio de verdad  C=USB");

    button(gfx, 60, BAT_BTN_Y, 140, BAT_BTN_H, "BORRAR", FONT_M, false);
    button(gfx, 210, BAT_BTN_Y, 140, BAT_BTN_H, "VOLVER", FONT_M, true);
}

// --- ENCABEZADO DEL DASHBOARD ---
// Nombre a la izquierda, batería y conexión a la derecha y, fuera de INICIO, la hora al medio.
static void drawHeader(Arduino_GFX* g, const char* name) {
    char upper[12];
    int i = 0;
    for (; name[i] && i < 11; i++) upper[i] = toupper(name[i]);
    upper[i] = 0;
    AAText::draw(g, FONT_S, 62, HDR_Y, upper, PAL_GRIS, PAL_FONDO, 2);
    drawHeaderBattery(g, true);
    drawNetDot(g, true);
}

// --- PANTALLA DE INICIO ---
// Hora gigante, fecha, pasos del día con barra hacia la meta, distancia, clima y la
// próxima alarma.
static const unsigned long STEP_GOAL = 10000;
static const int HOME_CLOCK_Y = 58;
static const int HOME_DATE_Y = 150;
static const int HOME_STEPS_Y = 198;                 // renglón FONT_S de "PASOS"
static const int HOME_BAR_Y = 228, HOME_BAR_H = 10;
static const int CARD_Y = 252, CARD_H = 72, CARD_W = 160, CARD2_X = CR - CARD_W;
static const int ALARM_CARD_Y = 336, ALARM_CARD_H = 56;

// Próxima alarma activa: "07:30" o "" si no hay.
static void nextAlarmText(char* buf, size_t len) {
    buf[0] = 0;
    struct tm t;
    if (!getLocalTime(&t, 0)) return;
    int now = t.tm_hour * 60 + t.tm_min, best = 100000;
    for (int i = 0; i < AlarmManager::alarmCount; i++) {
        const AlarmItem& a = AlarmManager::alarmList[i];
        if (!a.active) continue;
        int in = (a.hour * 60 + a.minute - now + 1440) % 1440;
        if (in == 0) in = 1440;
        if (in < best) { best = in; snprintf(buf, len, "%02d:%02d", a.hour, a.minute); }
    }
}

static void drawWeatherCard(Arduino_GFX* g) {
    shownWeather = Weather::version();
    g->fillRect(CARD2_X + 4, CARD_Y + 26, CARD_W - 8, CARD_H - 30, PAL_FONDO);
    int x = CARD2_X + 14, y = CARD_Y + 30;
    if (!Weather::available()) {
        AAText::draw(g, FONT_L, x, y, "--°", PAL_GRIS, PAL_FONDO);
        return;
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "%d°", Weather::temperature());
    int w = AAText::draw(g, FONT_L, x, y, buf, PAL_BLANCO, PAL_FONDO);
    int dx = x + w + 6;
    AAText::drawBox(g, FONT_S, dx, y + FONT_L.ascent - FONT_S.ascent, CARD2_X + CARD_W - 8 - dx,
                    Weather::description(), PAL_GRIS, PAL_FONDO);
}

// Datos que cambian (cada segundo se llama; dibuja solo lo distinto).
static void refreshHome(Arduino_GFX* g, bool force) {
    char buf[20];
    if (StepCounter::available()) {
        unsigned long steps = StepCounter::steps();
        thousands(buf, sizeof(buf), steps);
        if (force || strcmp(buf, shownSteps) != 0) {
            strlcpy(shownSteps, buf, sizeof(shownSteps));
            int goalW = AAText::width(FONT_S, " / 10.000");
            AAText::drawBox(g, FONT_L, CR - goalW - 140, HOME_STEPS_Y + FONT_S.ascent - FONT_L.ascent, 140,
                            buf, PAL_BLANCO, PAL_FONDO, RIGHT);
        }
        int fill = min(steps, STEP_GOAL) * CW / STEP_GOAL;
        if (force || fill != shownBar) {
            shownBar = fill;
            g->fillRoundRect(CL, HOME_BAR_Y, CW, HOME_BAR_H, HOME_BAR_H / 2, PAL_BORDE);
            if (fill >= HOME_BAR_H) g->fillRoundRect(CL, HOME_BAR_Y, fill, HOME_BAR_H, HOME_BAR_H / 2, PAL_ACENTO);
        }
        kmText(buf, sizeof(buf), StepCounter::distanceKm());
    } else {
        strlcpy(buf, "sin sensor", sizeof(buf));
    }
    if (force || strcmp(buf, shownKm) != 0) {
        strlcpy(shownKm, buf, sizeof(shownKm));
        AAText::drawBox(g, FONT_L, CL + 14, CARD_Y + 30, CARD_W - 22, buf, PAL_BLANCO, PAL_FONDO);
    }
    if (force || Weather::version() != shownWeather) drawWeatherCard(g);

    nextAlarmText(buf, sizeof(buf));
    if (force || strcmp(buf, shownAlarm) != 0) {
        strlcpy(shownAlarm, buf, sizeof(shownAlarm));
        int y = textTopFor(FONT_L, ALARM_CARD_Y, ALARM_CARD_H);
        if (buf[0]) AAText::drawBox(g, FONT_L, CR - 134, y, 120, buf, PAL_BLANCO, PAL_FONDO, RIGHT);
        else AAText::drawBox(g, FONT_L, CR - 134, y, 120, "ninguna", PAL_GRIS, PAL_FONDO, RIGHT);
    }
}

static void drawHome(Arduino_GFX* g) {
    AAText::draw(g, FONT_S, CL, HOME_STEPS_Y, "PASOS", PAL_GRIS, PAL_FONDO, 1);
    AAText::drawRight(g, FONT_S, CR, HOME_STEPS_Y, " / 10.000", PAL_GRIS, PAL_FONDO);

    outlineBox(g, CL, CARD_Y, CARD_W, CARD_H, 16, PAL_BORDE);
    outlineBox(g, CARD2_X, CARD_Y, CARD_W, CARD_H, 16, PAL_BORDE);
    AAText::draw(g, FONT_S, CL + 14, CARD_Y + 8, "DISTANCIA", PAL_GRIS, PAL_FONDO, 1);
    AAText::draw(g, FONT_S, CARD2_X + 14, CARD_Y + 8, "CLIMA", PAL_GRIS, PAL_FONDO, 1);

    outlineBox(g, CL, ALARM_CARD_Y, CW, ALARM_CARD_H, 16, PAL_BORDE);
    AAText::draw(g, FONT_S, CL + 14, textTopFor(FONT_S, ALARM_CARD_Y, ALARM_CARD_H), "PRÓXIMA ALARMA",
                 PAL_GRIS, PAL_FONDO, 1);
    refreshHome(g, true);
}

// La hora se llama cada segundo pero solo cambia una vez por minuto: se redibuja solo si
// cambió el texto o si pasó a estar sincronizada.
void DisplayUI::drawHeaderClockOnly() {
    if (activeProfile < 0 || PowerManager::isScreenOff() || currentScreen != SCREEN_DASHBOARD) return;
    char buf[12];
    clockText(buf, sizeof(buf));
    bool synced = TimeSync::isSynced();
    char key[16];
    snprintf(key, sizeof(key), "%s%d", buf, synced);
    if (strcmp(key, shownTime) == 0) return;
    strlcpy(shownTime, key, sizeof(shownTime));
    // Hora de respaldo (nunca llegó la de NTP): en rojo, para que se note que no es confiable.
    uint16_t color = synced ? PAL_BLANCO : PAL_ALERTA;
    if (currentTab == TAB_HOME) {
        AAText::drawBox(gfx, FONT_HUGE, CL, HOME_CLOCK_Y, CW, buf, color, PAL_FONDO, CENTER);
        char date[20];
        dateText(date, sizeof(date));
        if (strcmp(date, shownDate) != 0) {
            strlcpy(shownDate, date, sizeof(shownDate));
            AAText::drawBox(gfx, FONT_M, CL, HOME_DATE_Y, CW, date, PAL_ACENTO, PAL_FONDO, CENTER, 2);
        }
    } else {
        AAText::drawBox(gfx, FONT_L, CX - 50, HDR_TIME_Y, 100, buf, color, PAL_FONDO, CENTER);
    }
}

// --- CRONO ---
// Selector [CRONÓMETRO | TEMPORIZADOR], los dígitos y, abajo, INICIAR/PAUSA y RESET.
static const int SEG_Y = CONTENT_Y, SEG_H = 44, SEG_W = 163, SEG2_X = CR - SEG_W;
static const int CHRONO_Y = 122;                     // FONT_XXL: hasta 201
static const int SESSION_Y = 212;
static const int TIMER_BTN_Y = 214, TIMER_BTN_H = 50, TIMER_BTN_W = 104, TIMER_BTN_STEP = 113;
static const int CHRONO_BTN_Y = 300, CHRONO_BTN_H = 56, CHRONO_BTN_W = 160, CHRONO_BTN2_X = CR - CHRONO_BTN_W;

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
    bool running = chronoShowTimer ? AlarmManager::isTimerRunning() : AlarmManager::isChronoRunning();
    // Caja de ancho fijo: los números tienen todos el mismo ancho y no parpadea.
    AAText::drawBox(gfx, FONT_XXL, CL, CHRONO_Y, CW, buf, running ? PAL_ACENTO : PAL_BLANCO, PAL_FONDO, CENTER);
}

// Sesión del cronómetro (solo en modo CRONOMETRO), debajo de los dígitos: pasos y km
// desde INICIAR, sin contar las pausas, y la velocidad del momento.
static void drawSessionLine(Arduino_GFX* g) {
    if (chronoShowTimer) return;
    char steps[16], km[16], buf[48];
    thousands(steps, sizeof(steps), StepCounter::sessionSteps());
    kmText(km, sizeof(km), StepCounter::sessionKm());
    snprintf(buf, sizeof(buf), "%s pasos  ·  %s", steps, km);
    AAText::drawBox(g, FONT_M, CL, SESSION_Y, CW, buf, PAL_BLANCO, PAL_FONDO, CENTER);
    float v = StepCounter::speedKmh();
    snprintf(buf, sizeof(buf), v >= 10 ? "%.0f KM/H" : "%.1f KM/H", v);
    for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
    AAText::drawBox(g, FONT_S, CL, SESSION_Y + 30, CW, buf, v > 0 ? PAL_ACENTO : PAL_GRIS, PAL_FONDO, CENTER, 1);
}

void DisplayUI::refreshChronoDigits() {
    if (PowerManager::isScreenOff()) return;
    if (currentScreen != SCREEN_DASHBOARD || currentTab != TAB_CHRONO) return;
    drawChronoDigits();
    drawSessionLine(gfx);
}

static void drawChronoTab(Arduino_GFX* g) {
    if (chronoShowTimer) {
        button(g, CL, SEG_Y, SEG_W, SEG_H, "CRONÓMETRO", FONT_M, false);
        button(g, SEG2_X, SEG_Y, SEG_W, SEG_H, "TEMPORIZADOR", FONT_M, true);
    } else {
        button(g, CL, SEG_Y, SEG_W, SEG_H, "CRONÓMETRO", FONT_M, true);
        button(g, SEG2_X, SEG_Y, SEG_W, SEG_H, "TEMPORIZADOR", FONT_M, false);
    }
    if (chronoShowTimer) {
        static const char* labels[3] = { "+1 MIN", "+5 MIN", "+10 MIN" };
        for (int i = 0; i < 3; i++)
            button(g, CL + i * TIMER_BTN_STEP, TIMER_BTN_Y, TIMER_BTN_W, TIMER_BTN_H, labels[i], FONT_M, false);
    }
    bool running = chronoShowTimer ? AlarmManager::isTimerRunning() : AlarmManager::isChronoRunning();
    if (running) button(g, CL, CHRONO_BTN_Y, CHRONO_BTN_W, CHRONO_BTN_H, "PAUSA", FONT_L, true, PAL_BLANCO);
    else button(g, CL, CHRONO_BTN_Y, CHRONO_BTN_W, CHRONO_BTN_H, "INICIAR", FONT_L, true);
    button(g, CHRONO_BTN2_X, CHRONO_BTN_Y, CHRONO_BTN_W, CHRONO_BTN_H, "RESET", FONT_L, false);
}

// --- ALARMAS ---
// Lista: cada alarma en una tarjeta (hora + interruptor) con el botón de borrar al lado.
static const int ALARM_ROW_Y = 94, ALARM_ROW_STEP = 58, ALARM_ROW_H = 50;
static const int ALARM_CARD_W = 270, ALARM_DEL_X = CL + ALARM_CARD_W + 8, ALARM_DEL_W = CR - ALARM_DEL_X;
static const int NEW_ALARM_H = 50;

static int newAlarmY() {
    return ALARM_ROW_Y + AlarmManager::alarmCount * ALARM_ROW_STEP;
}

static void drawSwitch(Arduino_GFX* g, int x, int cy, bool on) {
    g->fillRoundRect(x, cy - 14, 52, 28, 14, on ? PAL_ACENTO : PAL_BORDE);
    g->fillCircle(on ? x + 38 : x + 14, cy, 10, on ? PAL_FONDO : PAL_GRIS);
}

static void drawAlarmsTab(Arduino_GFX* g) {
    AAText::draw(g, FONT_S, CL, CONTENT_Y, "ALARMAS", PAL_GRIS, PAL_FONDO, 2);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d/%d", AlarmManager::alarmCount, MAX_ALARMS);
    AAText::drawRight(g, FONT_S, CR, CONTENT_Y, buf, PAL_GRIS, PAL_FONDO, 1);

    for (int i = 0; i < AlarmManager::alarmCount; i++) {
        int y = ALARM_ROW_Y + i * ALARM_ROW_STEP;
        bool active = AlarmManager::alarmList[i].active;
        outlineBox(g, CL, y, ALARM_CARD_W, ALARM_ROW_H, 14, PAL_BORDE);
        snprintf(buf, sizeof(buf), "%02d:%02d", AlarmManager::alarmList[i].hour, AlarmManager::alarmList[i].minute);
        AAText::draw(g, FONT_L, CL + 16, textTopFor(FONT_L, y, ALARM_ROW_H), buf, active ? PAL_BLANCO : PAL_GRIS, PAL_FONDO);
        drawSwitch(g, CL + ALARM_CARD_W - 66, y + ALARM_ROW_H / 2, active);

        outlineBox(g, ALARM_DEL_X, y, ALARM_DEL_W, ALARM_ROW_H, 14, PAL_BORDE);
        int cx = ALARM_DEL_X + ALARM_DEL_W / 2, cy = y + ALARM_ROW_H / 2;
        thickLine(g, cx - 8, cy - 8, cx + 8, cy + 8, PAL_ALERTA);
        thickLine(g, cx - 8, cy + 8, cx + 8, cy - 8, PAL_ALERTA);
    }
    if (AlarmManager::alarmCount < MAX_ALARMS) {
        button(g, CL, newAlarmY(), CW, NEW_ALARM_H, "+ NUEVA ALARMA", FONT_M, true);
    }
    if (AlarmManager::alarmCount == 0) {
        AAText::drawWrapped(g, FONT_M, CL, newAlarmY() + NEW_ALARM_H + 24, CW, 3,
                            "También se las podés pedir a JARVIS: \"poneme una alarma a las 7\".",
                            PAL_GRIS, PAL_FONDO);
    }
}

// Nueva alarma: hora grande con [+] y [-] debajo de las horas y de los minutos.
static const int ADD_TIME_Y = 94;
static const int ADD_HOUR_X = 80, ADD_MIN_X = 220, ADD_COL_W = 110;
static const int ADD_PLUS_Y = 190, ADD_MINUS_Y = 250, ADD_BTN_H = 50;
static const int ADD_SAVE_Y = 322, ADD_SAVE_H = 52;

static void drawAddTime(Arduino_GFX* g, int hour, int minute) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", hour);
    AAText::drawBox(g, FONT_XXL, ADD_HOUR_X, ADD_TIME_Y, ADD_COL_W, buf, PAL_BLANCO, PAL_FONDO, CENTER);
    snprintf(buf, sizeof(buf), "%02d", minute);
    AAText::drawBox(g, FONT_XXL, ADD_MIN_X, ADD_TIME_Y, ADD_COL_W, buf, PAL_BLANCO, PAL_FONDO, CENTER);
}

static void drawAddAlarmTab(Arduino_GFX* g, int hour, int minute) {
    AAText::draw(g, FONT_S, CL, CONTENT_Y, "NUEVA ALARMA", PAL_GRIS, PAL_FONDO, 2);
    drawAddTime(g, hour, minute);
    AAText::drawCentered(g, FONT_XXL, CX, ADD_TIME_Y - 4, ":", PAL_GRIS, PAL_FONDO);
    button(g, ADD_HOUR_X, ADD_PLUS_Y, ADD_COL_W, ADD_BTN_H, "+", FONT_L, false);
    button(g, ADD_MIN_X, ADD_PLUS_Y, ADD_COL_W, ADD_BTN_H, "+", FONT_L, false);
    button(g, ADD_HOUR_X, ADD_MINUS_Y, ADD_COL_W, ADD_BTN_H, "-", FONT_L, false);
    button(g, ADD_MIN_X, ADD_MINUS_Y, ADD_COL_W, ADD_BTN_H, "-", FONT_L, false);
    button(g, CL, ADD_SAVE_Y, CW, ADD_SAVE_H, "GUARDAR", FONT_L, true);
}

// --- MÚSICA ---
// Arriba la canción; fila de botones anterior, play/pausa, siguiente y, abajo, el volumen.
struct MusicButton { int x, y, w, h; const char* label; MediaRemote::Key key; };
static const MusicButton MUSIC_BUTTONS[] = {
    { CL,  214,  96, 80, nullptr, MediaRemote::PREV },
    { 157, 214,  96, 80, nullptr, MediaRemote::PLAY_PAUSE },
    { 274, 214,  96, 80, nullptr, MediaRemote::NEXT },
    { CL,  310, 160, 64, "VOL -", MediaRemote::VOL_DOWN },
    { 210, 310, 160, 64, "VOL +", MediaRemote::VOL_UP },
};
static const int MUSIC_BUTTON_COUNT = sizeof(MUSIC_BUTTONS) / sizeof(MUSIC_BUTTONS[0]);
static const int MUSIC_STATUS_Y = CONTENT_Y, MUSIC_STATUS_H = 140;
static bool lastMusicConnected = false;

// flash = recién apretado (blanco por un momento).
static void drawMusicButton(Arduino_GFX* g, int i, bool flash) {
    const MusicButton& b = MUSIC_BUTTONS[i];
    bool conn = MediaRemote::isConnected();
    bool filled = flash || (conn && b.key == MediaRemote::PLAY_PAUSE);
    uint16_t fill = flash ? PAL_BLANCO : PAL_ACENTO;
    uint16_t fg = filled ? PAL_FONDO : (conn ? PAL_BLANCO : PAL_GRIS);
    uint16_t bg = filled ? fill : PAL_FONDO;
    if (filled) g->fillRoundRect(b.x, b.y, b.w, b.h, 14, fill);
    else outlineBox(g, b.x, b.y, b.w, b.h, 14, PAL_BORDE);
    int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    switch (b.key) {
        case MediaRemote::PREV:
            g->fillRect(cx - 13, cy - 11, 4, 22, fg);
            g->fillTriangle(cx + 11, cy - 11, cx + 11, cy + 11, cx - 8, cy, fg);
            break;
        case MediaRemote::NEXT:
            g->fillRect(cx + 10, cy - 11, 4, 22, fg);
            g->fillTriangle(cx - 11, cy - 11, cx - 11, cy + 11, cx + 8, cy, fg);
            break;
        case MediaRemote::PLAY_PAUSE:
            g->fillTriangle(cx - 16, cy - 12, cx - 16, cy + 12, cx + 2, cy, fg);
            g->fillRect(cx + 6, cy - 11, 4, 22, fg);
            g->fillRect(cx + 13, cy - 11, 4, 22, fg);
            break;
        default:
            AAText::drawCentered(g, FONT_L, cx, textTopFor(FONT_L, b.y, b.h), b.label, fg, bg, 1);
            break;
    }
}

static uint32_t lastTrackVersion = 0;

// Zona de arriba de la pestaña: la canción que suena (o cómo vincular el reloj).
static void drawMusicStatus(Arduino_GFX* g) {
    bool conn = MediaRemote::isConnected();
    lastMusicConnected = conn;
    lastTrackVersion = MediaRemote::trackInfoVersion();
    g->fillRect(CL, MUSIC_STATUS_Y, CW, MUSIC_STATUS_H, PAL_FONDO);
    int y = MUSIC_STATUS_Y;
    if (conn && MediaRemote::title()[0]) {
        const char* state = MediaRemote::isPlaying() ? "SONANDO" : "EN PAUSA";
        AAText::draw(g, FONT_S, CL, y, state, MediaRemote::isPlaying() ? PAL_ACENTO : PAL_GRIS, PAL_FONDO, 2);
        y = AAText::drawWrapped(g, FONT_L, CL, y + 28, CW, 2, MediaRemote::title(), PAL_BLANCO, PAL_FONDO, 0);
        AAText::drawWrapped(g, FONT_M, CL, y + 2, CW, 1, MediaRemote::artist(), PAL_GRIS, PAL_FONDO);
    } else if (conn) {
        AAText::draw(g, FONT_S, CL, y, "CONECTADO AL CELULAR", PAL_ACENTO, PAL_FONDO, 2);
        AAText::drawWrapped(g, FONT_M, CL, y + 30, CW, 3, "Poné algo en Spotify u otra app de música.",
                            PAL_GRIS, PAL_FONDO);
    } else {
        AAText::draw(g, FONT_S, CL, y, "SIN CONECTAR", PAL_ALERTA, PAL_FONDO, 2);
        AAText::drawWrapped(g, FONT_M, CL, y + 30, CW, 3, "En el celular: Ajustes > Bluetooth > JARVIS Reloj",
                            PAL_BLANCO, PAL_FONDO);
    }
}

static void drawMusicTab(Arduino_GFX* g) {
    drawMusicStatus(g);
    for (int i = 0; i < MUSIC_BUTTON_COUNT; i++) drawMusicButton(g, i, false);
}

// --- Toques en los botones de música (sin bloquear) ---
// handleTouch se llama en cada vuelta del loop mientras el dedo está apoyado. Si el
// último toque fue hace menos de MUSIC_HOLD_GAP_MS es el mismo dedo todavía apoyado.
static const unsigned long MUSIC_HOLD_GAP_MS  = 90;
static const unsigned long MUSIC_REPEAT_AFTER = 450;   // mantener volumen: empieza a repetir...
static const unsigned long MUSIC_REPEAT_EVERY = 150;   // ...y repite cada 150 ms
static const unsigned long MUSIC_FLASH_MS     = 200;   // blanco al apretar
static int musicHeld = -1;                 // botón que se está apretando
static unsigned long musicLastTouch = 0, musicHeldSince = 0, musicLastSend = 0;
static int musicFlash = -1;                // botón pintado de blanco
static unsigned long musicFlashUntil = 0;

static void musicSend(Arduino_GFX* g, int i) {
    MediaRemote::press(MUSIC_BUTTONS[i].key);
    musicLastSend = millis();
    drawMusicButton(g, i, true);
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

// Vuelve a su color el botón que titiló (se llama en cada vuelta del loop).
void DisplayUI::musicTick() {
    if (musicFlash < 0 || millis() < musicFlashUntil) return;
    int i = musicFlash;
    musicFlash = -1;
    if (PowerManager::isScreenOff() || currentScreen != SCREEN_DASHBOARD || currentTab != TAB_MUSIC) return;
    drawMusicButton(gfx, i, false);
}

// Si cambia el estado de la conexión Bluetooth con la pestaña a la vista, se redibuja.
void DisplayUI::refreshMusicTab() {
    if (PowerManager::isScreenOff() || currentScreen != SCREEN_DASHBOARD || currentTab != TAB_MUSIC) return;
    if (MediaRemote::isConnected() != lastMusicConnected) drawMusicTab(gfx);   // botones incluidos
    else if (MediaRemote::trackInfoVersion() != lastTrackVersion) drawMusicStatus(gfx);   // solo la canción
}

// --- DASHBOARD ---
void DisplayUI::drawDashboard() {
    if (PowerManager::isScreenOff()) return;
    gfx->fillScreen(PAL_FONDO);
    forgetShown();
    drawHeader(gfx, profiles[activeProfile].name);
    drawHeaderClockOnly();

    switch (currentTab) {
        case TAB_HOME:      drawHome(gfx); break;
        case TAB_AGENDA:    drawAlarmsTab(gfx); break;
        case TAB_ADD_ALARM: drawAddAlarmTab(gfx, tempHour, tempMinute); break;
        case TAB_CHRONO:    drawChronoTab(gfx); drawChronoDigits(); drawSessionLine(gfx); break;
        case TAB_MUSIC:     drawMusicTab(gfx); break;
        case TAB_JARVIS:    drawJarvisTab(); break;
    }
    drawNav(gfx, currentTab);
}

void DisplayUI::refreshNetStatus(bool wifiChanged) {
    (void)wifiChanged;
    if (PowerManager::isScreenOff() || currentScreen != SCREEN_DASHBOARD) return;
    drawNetDot(gfx, false);
}

// Una vez por segundo: batería del encabezado y, en INICIO, pasos, km, clima y alarma.
void DisplayUI::refreshLiveData() {
    if (PowerManager::isScreenOff() || currentScreen != SCREEN_DASHBOARD) return;
    drawHeaderBattery(gfx, false);
    if (currentTab == TAB_HOME) refreshHome(gfx, false);
}

// Hora grande al levantar la muñeca, con la fecha abajo.
void DisplayUI::drawBigClock() {
    gfx->fillScreen(PAL_FONDO);
    char buf[20];
    if (!clockText(buf, sizeof(buf))) return;
    AAText::drawCentered(gfx, FONT_HUGE, CX, 170, buf, PAL_BLANCO, PAL_FONDO);
    dateText(buf, sizeof(buf));
    AAText::drawCentered(gfx, FONT_M, CX, 268, buf, PAL_ACENTO, PAL_FONDO, 2);
}

// --- NOTIFICACIONES ---
static const int NOTIF_BTN_Y = 375, NOTIF_BTN_H = 70;

void DisplayUI::drawNotification(const char* app, const char* title, const char* message, bool call) {
    gfx->fillScreen(PAL_FONDO);
    // Etiqueta con la app (o "LLAMADA"): lima, o roja si es una llamada.
    char label[24];
    int i = 0;
    const char* src = call ? "LLAMADA" : app;
    for (; src[i] && i < 23; i++) label[i] = toupper((unsigned char)src[i]);
    label[i] = 0;
    uint16_t pill = call ? PAL_ALERTA : PAL_ACENTO;
    uint16_t pillText = call ? PAL_BLANCO : PAL_FONDO;
    int labelW = AAText::width(FONT_M, label, 2) + 36;
    gfx->fillRoundRect(CX - labelW / 2, 28, labelW, 38, 19, pill);
    AAText::drawCentered(gfx, FONT_M, CX, textTopFor(FONT_M, 28, 38), label, pillText, pill, 2);

    // Quién (2 renglones) y el mensaje.
    int y = AAText::drawWrapped(gfx, FONT_L, 50, 84, 310, 2, title, PAL_BLANCO, PAL_FONDO, 0);
    if (call) {
        button(gfx, 60, NOTIF_BTN_Y, LCD_WIDTH - 120, NOTIF_BTN_H, "RECHAZAR", FONT_XL, true, PAL_ALERTA, PAL_BLANCO);
        AAText::drawCentered(gfx, FONT_S, CX, NOTIF_BTN_Y - 36, "Tocá arriba para cerrar", PAL_GRIS, PAL_FONDO);
        return;
    }
    int lineH = FONT_M.height + 2;
    AAText::drawWrapped(gfx, FONT_M, 50, y + 10, 310, (405 - (y + 10)) / lineH, message, PAL_GRIS, PAL_FONDO);
    AAText::drawCentered(gfx, FONT_S, CX, 425, "Tocá para cerrar", PAL_GRIS, PAL_FONDO);
}

bool DisplayUI::hitRejectButton(int16_t x, int16_t y) {
    return x >= 60 && x <= LCD_WIDTH - 60 && y >= NOTIF_BTN_Y && y <= NOTIF_BTN_Y + NOTIF_BTN_H;
}

// --- PESTAÑA JARVIS ---
// Arriba el estado (qué hacer o en qué paso va), después lo que dijiste y la respuesta.
static const char* JARVIS_IDLE = "Mantené BOOT y hablá";
static char jarvisStatus[40] = "";

static void drawJarvisStatus(Arduino_GFX* g) {
    AAText::drawBox(g, FONT_M, CL, CONTENT_Y, CW, jarvisStatus[0] ? jarvisStatus : JARVIS_IDLE,
                    PAL_ACENTO, PAL_FONDO);
}

void DisplayUI::drawJarvisTab() {
    gfx->fillRect(CL, CONTENT_Y, CW, NAV_Y - 8 - CONTENT_Y, PAL_FONDO);
    drawJarvisStatus(gfx);
    int y = CONTENT_Y + 38;
    int lineH = FONT_M.height + 2;
    if (Jarvis::question()[0]) {
        y = AAText::drawWrapped(gfx, FONT_M, CL, y, CW, 2, Jarvis::question(), PAL_GRIS, PAL_FONDO) + 8;
    }
    if (Jarvis::answer()[0]) {
        AAText::drawWrapped(gfx, FONT_M, CL, y, CW, (NAV_Y - 10 - y) / lineH, Jarvis::answer(), PAL_BLANCO, PAL_FONDO);
    } else if (!Jarvis::question()[0]) {
        AAText::drawWrapped(gfx, FONT_M, CL, y + 8, CW, 8,
                            "Preguntale por tu agenda, que te ponga una alarma, el clima o lo que quieras. "
                            "Necesita internet (WiFi o el hotspot del celu) y la compu con n8n prendido.",
                            PAL_GRIS, PAL_FONDO);
    }
}

// Callbacks de Jarvis::ask mientras se habla (el loop está frenado: se dibuja desde acá).
static void jarvisOnStatus(const char* s) {
    strlcpy(jarvisStatus, s, sizeof(jarvisStatus));
    drawJarvisStatus(DisplayUI::getGFX());
}
static void jarvisOnAnswer() {
    jarvisStatus[0] = 0;
    DisplayUI::refreshCurrentScreen();     // muestra pregunta y respuesta mientras habla
}

void DisplayUI::talkToJarvis(bool (*keepRecording)()) {
    if (currentScreen != SCREEN_DASHBOARD || activeProfile < 0) return;
    if (currentTab != TAB_JARVIS) {
        currentTab = TAB_JARVIS;
        drawDashboard();
    }
    Jarvis::Result r = Jarvis::ask(profiles[activeProfile].name, keepRecording, jarvisOnStatus, jarvisOnAnswer);
    switch (r) {
        case Jarvis::OK:        jarvisStatus[0] = 0; break;
        case Jarvis::NO_WIFI:   strlcpy(jarvisStatus, "Sin internet: probá de nuevo", sizeof(jarvisStatus)); break;
        case Jarvis::TOO_SHORT: strlcpy(jarvisStatus, "Muy corto: mantené BOOT", sizeof(jarvisStatus)); break;
        case Jarvis::SERVER_ERROR:
            if (Jarvis::lastHttpCode() < 0) strlcpy(jarvisStatus, "n8n no responde", sizeof(jarvisStatus));
            else snprintf(jarvisStatus, sizeof(jarvisStatus), "Error de n8n (%d)", Jarvis::lastHttpCode());
            break;
    }
    drawJarvisStatus(gfx);
}

void DisplayUI::showSleepMessage() {
    if (PowerManager::isScreenOff()) return;
    gfx->fillScreen(PAL_FONDO);
    AAText::drawCentered(gfx, FONT_XL, CX, 190, "MODO DORMIR", PAL_BLANCO, PAL_FONDO, 2);
    AAText::drawCentered(gfx, FONT_M, CX, 250, "Tocá para despertar", PAL_GRIS, PAL_FONDO);
}

// --- CONFIGURAR WIFI ---
static const int PORTAL_BTN_X = 50, PORTAL_BTN_Y = 340, PORTAL_BTN_W = 310, PORTAL_BTN_H = 70;

void DisplayUI::drawWifiPortalScreen() {
    if (PowerManager::isScreenOff()) return;   // se dibuja al encender
    gfx->fillScreen(PAL_FONDO);
    AAText::drawCentered(gfx, FONT_L, CX, 28, "CONFIGURAR WIFI", PAL_BLANCO, PAL_FONDO, 2);
    AAText::draw(gfx, FONT_M, 50, 100, "1. Conectate a la red:", PAL_GRIS, PAL_FONDO);
    AAText::draw(gfx, FONT_L, 50, 128, "JARVIS-Clock-Config", PAL_ACENTO, PAL_FONDO);
    AAText::draw(gfx, FONT_M, 50, 190, "2. Abrí en el navegador:", PAL_GRIS, PAL_FONDO);
    AAText::draw(gfx, FONT_L, 50, 218, "http://192.168.4.1", PAL_ACENTO, PAL_FONDO);
    button(gfx, PORTAL_BTN_X, PORTAL_BTN_Y, PORTAL_BTN_W, PORTAL_BTN_H, "VOLVER", FONT_L, true);
}

// Pantalla que suena (alarma o temporizador): toda en acento para que se note. Bloquea
// hasta que se toca un botón y devuelve true si se eligió POSPONER. Al terminar vuelve a
// la pantalla que había.
bool DisplayUI::ringScreen(const char* title, const char* timeText, bool allowSnooze) {
    // Si la pantalla estaba apagada, se enciende (sin redibujar: esta pantalla dibuja la suya).
    PowerManager::turnScreenOn(false);

    ScreenState previousScreen = currentScreen;
    currentScreen = SCREEN_ALARM_RINGING;
    gfx->fillScreen(PAL_ACENTO);
    AAText::drawCentered(gfx, FONT_L, CX, 44, title, PAL_FONDO, PAL_ACENTO, 3);
    AAText::drawCentered(gfx, FONT_HUGE, CX, 96, timeText, PAL_FONDO, PAL_ACENTO);

    // Cajas entre x 40..370 y hasta y=445, lejos de las esquinas redondeadas de abajo.
    const int SNOOZE_Y = 220, SNOOZE_H = 95;
    const int OFF_Y = allowSnooze ? 330 : 220;
    const int OFF_H = 445 - OFF_Y;
    if (allowSnooze) {
        gfx->fillRoundRect(40, SNOOZE_Y, LCD_WIDTH - 80, SNOOZE_H, 20, PAL_FONDO);
        AAText::drawCentered(gfx, FONT_XL, CX, SNOOZE_Y + 14, "POSPONER", PAL_BLANCO, PAL_FONDO, 2);
        AAText::drawCentered(gfx, FONT_S, CX, SNOOZE_Y + 64, "5 MIN", PAL_GRIS, PAL_FONDO, 2);
    }
    gfx->fillRoundRect(40, OFF_Y, LCD_WIDTH - 80, OFF_H, 20, PAL_BLANCO);
    AAText::drawCentered(gfx, FONT_XL, CX, textTopFor(FONT_XL, OFF_Y, OFF_H), "DESACTIVAR", PAL_FONDO, PAL_BLANCO, 2);

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
    if (ringScreen("ALARMA", t, true)) AlarmManager::snooze(alarmIdx, 5);
    // Si se desactiva, la alarma sigue activa: vuelve a sonar mañana a la misma hora.
}

// --- TOQUES ---
static bool inBox(int16_t tx, int16_t ty, int x, int y, int w, int h) {
    return tx >= x && tx <= x + w && ty >= y && ty <= y + h;
}

void DisplayUI::handleTouch(int16_t tx, int16_t ty) {
    if (currentScreen == SCREEN_SETTINGS) {
        if (inBox(tx, ty, SET_MINUS_X, SET_BRIGHT_Y, SET_BTN_W, SET_BTN_H)) Settings::setBrightness(Settings::brightness() - 1);
        else if (inBox(tx, ty, SET_PLUS_X, SET_BRIGHT_Y, SET_BTN_W, SET_BTN_H)) Settings::setBrightness(Settings::brightness() + 1);
        else if (inBox(tx, ty, SET_MINUS_X, SET_VOLUME_Y, SET_BTN_W, SET_BTN_H)) Settings::setAlarmVolume(Settings::alarmVolume() - 1);
        else if (inBox(tx, ty, SET_PLUS_X, SET_VOLUME_Y, SET_BTN_W, SET_BTN_H)) Settings::setAlarmVolume(Settings::alarmVolume() + 1);
        else if (inBox(tx, ty, SET_WIFI_X, SET_WIFI_Y, SET_ROW2_W, SET_ROW_H)) {
            TouchDriver::waitForRelease();
            WiFiPortal::startPortal();
            return;
        } else if (inBox(tx, ty, SET_BATT_X, SET_WIFI_Y, SET_ROW2_W, SET_ROW_H)) {
            TouchDriver::waitForRelease();
            currentScreen = SCREEN_BATTERY;
            drawBatteryScreen();
            return;
        } else if (inBox(tx, ty, SET_BACK_X, SET_BACK_Y, SET_BACK_W, SET_ROW_H)) {
            TouchDriver::waitForRelease();
            currentScreen = SCREEN_PROFILES;
            drawProfileMenu();
            return;
        } else return;
        drawSettingValues(gfx);                  // solo cambian los números
        TouchDriver::waitForRelease();
        return;
    }

    if (currentScreen == SCREEN_BATTERY) {
        if (ty < BAT_BTN_Y || ty > BAT_BTN_Y + BAT_BTN_H) return;
        if (tx >= 60 && tx <= 200) {
            TouchDriver::waitForRelease();
            BatteryLog::clear();
            drawBatteryScreen();
        } else if (tx >= 210 && tx <= 350) {
            TouchDriver::waitForRelease();
            currentScreen = SCREEN_SETTINGS;
            drawSettingsScreen();
        }
        return;
    }

    if (currentScreen == SCREEN_WIFI_CONFIG) {
        if (inBox(tx, ty, PORTAL_BTN_X, PORTAL_BTN_Y, PORTAL_BTN_W, PORTAL_BTN_H)) {
            TouchDriver::waitForRelease();
            WiFiPortal::stopPortal();      // vuelve al menú y a buscar las redes conocidas
        }
        return;
    }

    if (currentScreen == SCREEN_PROFILES) {
        for (int i = 0; i < 4; i++) {
            if (inBox(tx, ty, profiles[i].x, profiles[i].y, profiles[i].w, profiles[i].h)) {
                TouchDriver::waitForRelease();
                activeProfile = i;
                currentScreen = SCREEN_DASHBOARD;
                currentTab = TAB_HOME;
                drawDashboard();
                return;
            }
        }
        if (inBox(tx, ty, PROF_SET_X, PROF_SET_Y, PROF_SET_W, PROF_SET_H)) {
            TouchDriver::waitForRelease();
            currentScreen = SCREEN_SETTINGS;
            drawSettingsScreen();
            return;
        }
        return;
    }

    if (currentScreen != SCREEN_DASHBOARD) return;

    // Arriba: en INICIO, tocar el nombre vuelve a los perfiles; en las demás, a INICIO.
    if (ty < HDR_H) {
        if (currentTab == TAB_HOME) {
            if (tx > 170) return;
            TouchDriver::waitForRelease();
            activeProfile = -1;
            currentScreen = SCREEN_PROFILES;
            drawProfileMenu();
            return;
        }
        TouchDriver::waitForRelease();
        currentTab = TAB_HOME;
        drawDashboard();
        return;
    }

    // Botones de abajo: la sección; el de la sección que ya está abierta vuelve a INICIO.
    if (ty >= NAV_Y - 6 && tx >= NAV_X0 && tx < NAV_X0 + 4 * (NAV_W + NAV_GAP)) {
        int i = (tx - NAV_X0) / (NAV_W + NAV_GAP);
        static const WidgetTab TABS[4] = { TAB_AGENDA, TAB_CHRONO, TAB_MUSIC, TAB_JARVIS };
        currentTab = (i == navIndex(currentTab)) ? TAB_HOME : TABS[i];
        TouchDriver::waitForRelease();
        drawDashboard();
        return;
    }

    if (currentTab == TAB_AGENDA) {
        if (AlarmManager::alarmCount < MAX_ALARMS && inBox(tx, ty, CL, newAlarmY(), CW, NEW_ALARM_H)) {
            TouchDriver::waitForRelease();
            currentTab = TAB_ADD_ALARM;
            drawDashboard();
            return;
        }
        for (int i = 0; i < AlarmManager::alarmCount; i++) {
            int y = ALARM_ROW_Y + i * ALARM_ROW_STEP;
            if (inBox(tx, ty, ALARM_DEL_X, y, ALARM_DEL_W, ALARM_ROW_H)) {
                TouchDriver::waitForRelease();
                AlarmManager::deleteAlarm(i);
                drawDashboard();
                return;
            }
            // Tocar la tarjeta (hora o interruptor) la activa o desactiva.
            if (inBox(tx, ty, CL, y, ALARM_CARD_W, ALARM_ROW_H)) {
                TouchDriver::waitForRelease();
                AlarmManager::toggleAlarm(i);
                drawDashboard();
                return;
            }
        }
    }

    if (currentTab == TAB_CHRONO) {
        bool handled = false;
        if (inBox(tx, ty, CL, SEG_Y, CW, SEG_H)) {                       // selector de modo
            chronoShowTimer = (tx >= CX);
            handled = true;
        } else if (chronoShowTimer && ty >= TIMER_BTN_Y && ty <= TIMER_BTN_Y + TIMER_BTN_H) {   // +1 / +5 / +10 min
            static const int MINUTES[3] = { 1, 5, 10 };
            for (int i = 0; i < 3; i++) {
                int x = CL + i * TIMER_BTN_STEP;
                if (tx >= x && tx <= x + TIMER_BTN_W) { AlarmManager::timerAdd(MINUTES[i]); handled = true; }
            }
        } else if (ty >= CHRONO_BTN_Y && ty <= CHRONO_BTN_Y + CHRONO_BTN_H) {   // INICIAR/PAUSA y RESET
            if (tx >= CL && tx <= CL + CHRONO_BTN_W) {
                if (chronoShowTimer) AlarmManager::toggleTimer();
                else {
                    AlarmManager::toggleChrono();
                    if (AlarmManager::isChronoRunning()) StepCounter::sessionStart();
                    else StepCounter::sessionPause();
                }
                handled = true;
            } else if (tx >= CHRONO_BTN2_X && tx <= CR) {
                if (chronoShowTimer) AlarmManager::resetTimer();
                else { AlarmManager::resetChrono(); StepCounter::sessionReset(); }
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
        if (inBox(tx, ty, CL, ADD_SAVE_Y, CW, ADD_SAVE_H)) {
            AlarmManager::addAlarm(tempHour, tempMinute);
            currentTab = TAB_AGENDA;
            TouchDriver::waitForRelease();
            drawDashboard();
            return;
        }
        bool changed = true;
        if (inBox(tx, ty, ADD_HOUR_X, ADD_PLUS_Y, ADD_COL_W, ADD_BTN_H)) tempHour = (tempHour + 1) % 24;
        else if (inBox(tx, ty, ADD_MIN_X, ADD_PLUS_Y, ADD_COL_W, ADD_BTN_H)) tempMinute = (tempMinute + 1) % 60;
        else if (inBox(tx, ty, ADD_HOUR_X, ADD_MINUS_Y, ADD_COL_W, ADD_BTN_H)) tempHour = (tempHour + 23) % 24;
        else if (inBox(tx, ty, ADD_MIN_X, ADD_MINUS_Y, ADD_COL_W, ADD_BTN_H)) tempMinute = (tempMinute + 59) % 60;
        else changed = false;
        if (changed) {
            drawAddTime(gfx, tempHour, tempMinute);      // solo los números: no parpadea
            TouchDriver::waitForRelease();
            return;
        }
    }

    // MÚSICA: la tecla se manda apenas se toca, sin esperar a que se suelte el dedo.
    // Mantener VOL+/VOL- repite la tecla. El botón titila en blanco (ver musicTick).
    if (currentTab == TAB_MUSIC && MediaRemote::isConnected()) {
        for (int i = 0; i < MUSIC_BUTTON_COUNT; i++) {
            const MusicButton& b = MUSIC_BUTTONS[i];
            if (inBox(tx, ty, b.x, b.y, b.w, b.h)) {
                handleMusicButton(i);
                return;
            }
        }
        musicHeld = -1;
    }
}
