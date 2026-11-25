#include "web_settings.h"
#include "web_page.h"
#include "web_icon.h"
#include "settings.h"
#include "alarm_manager.h"
#include "battery_log.h"
#include "power_manager.h"
#include "step_counter.h"
#include "media_remote.h"
#include "weather.h"
#include "display_ui.h"
#include "jarvis.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

// Puerto 80, igual que el portal de WiFi (wifi_portal.cpp): con el portal abierto este
// servidor se apaga (el reloj deja de estar conectado a una red y el loop no lo atiende).
static WebServer web(80);
static bool started = false;
static bool routesSet = false;
static unsigned long lastUse = 0;
static const unsigned long IN_USE_MS = 5UL * 60 * 1000;

static void markUse() { lastUse = millis(); if (lastUse == 0) lastUse = 1; }

// Después de cambiar algo desde la página, la pantalla del reloj se pone al día.
static void refreshWatch() {
    if (PowerManager::isScreenOff()) return;
    ScreenState s = DisplayUI::getCurrentScreen();
    if (s == SCREEN_DASHBOARD || s == SCREEN_SETTINGS) DisplayUI::refreshCurrentScreen();
}

static void sendState() {
    markUse();
    DynamicJsonDocument doc(12288);
    char buf[16];
    struct tm t;
    if (getLocalTime(&t, 0)) snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    else strcpy(buf, "--:--");
    doc["hora"] = buf;
    doc["bateria"] = PowerManager::batteryPercent();
    doc["cargando"] = PowerManager::isCharging();
    doc["wifi"]["red"] = WiFi.SSID();
    doc["wifi"]["senal"] = WiFi.RSSI();
    doc["wifi"]["ip"] = WiFi.localIP().toString();
    doc["bt"]["on"] = MediaRemote::isEnabled();
    doc["bt"]["conectado"] = MediaRemote::isConnected();
    doc["pasos"] = StepCounter::steps();
    doc["km"] = StepCounter::distanceKm();
    doc["clima"]["ok"] = Weather::available();
    doc["clima"]["temp"] = Weather::temperature();
    doc["clima"]["desc"] = Weather::description();

    JsonObject a = doc.createNestedObject("ajustes");
    a["brillo"] = Settings::brightness();
    a["volumen"] = Settings::alarmVolume();
    a["bt"] = Settings::bluetooth();
    a["avisos"] = Settings::notifications();
    a["muneca"] = Settings::wristRaise();
    a["clima"] = Settings::weather();
    a["pantalla"] = Settings::screenTimeoutMs() / 1000;
    a["dormir"] = Settings::sleepAfterMs() / 60000;
    a["meta"] = Settings::stepGoal();

    doc["maxAlarmas"] = MAX_ALARMS;
    JsonArray al = doc.createNestedArray("alarmas");
    for (int i = 0; i < AlarmManager::alarmCount; i++) {
        JsonObject o = al.createNestedObject();
        o["h"] = AlarmManager::alarmList[i].hour;
        o["m"] = AlarmManager::alarmList[i].minute;
        o["on"] = AlarmManager::alarmList[i].active;
    }

    JsonArray g = doc.createNestedArray("gasto");
    for (int s = 0; s < 3; s++) {
        float r = BatteryLog::ratePerHour(s);
        g.add(r < 0 ? -1.0f : roundf(r * 10) / 10);
    }
    // Registro de la batería, el más nuevo primero.
    JsonArray reg = doc.createNestedArray("registro");
    int n = BatteryLog::count();
    for (int i = n - 1; i >= 0; i--) {
        const BatteryLog::Entry& e = BatteryLog::get(i);
        time_t when = e.when;
        struct tm tm;
        localtime_r(&when, &tm);
        snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
        JsonObject o = reg.createNestedObject();
        o["t"] = buf;
        o["pct"] = e.pct;
        o["mv"] = e.mv;
        o["p"] = e.onMin;
        o["a"] = e.offMin;
        o["d"] = e.sleepMin;
        o["s"] = e.asleepPct;
        o["c"] = e.charging;
    }
    String out;
    serializeJson(doc, out);
    web.sendHeader("Cache-Control", "no-store");
    web.send(200, "application/json", out);
}

static void handleSetting() {
    markUse();
    String k = web.arg("k");
    long v = web.arg("v").toInt();
    if (k == "brillo") Settings::setBrightness(v);
    else if (k == "volumen") Settings::setAlarmVolume(v);
    else if (k == "bt") Settings::setBluetooth(v != 0);
    else if (k == "avisos") Settings::setNotifications(v != 0);
    else if (k == "muneca") Settings::setWristRaise(v != 0);
    else if (k == "clima") Settings::setWeather(v != 0);
    else if (k == "pantalla") Settings::setScreenTimeoutSec(v);
    else if (k == "dormir") Settings::setSleepAfterMin(v);
    else if (k == "meta") Settings::setStepGoal(v);
    else { web.send(400, "text/plain", "ajuste desconocido"); return; }
    Serial.printf("[web] ajuste %s = %ld\n", k.c_str(), v);
    refreshWatch();
    web.send(200, "text/plain", "ok");
}

static void handleAlarm() {
    markUse();
    String action = web.arg("accion");
    int i = web.arg("i").toInt();
    if (action == "nueva") {
        int h = web.arg("h").toInt(), m = web.arg("m").toInt();
        if (h < 0 || h > 23 || m < 0 || m > 59 || AlarmManager::alarmCount >= MAX_ALARMS) {
            web.send(400, "text/plain", "no se puede");
            return;
        }
        if (AlarmManager::findAlarm(h, m) < 0) AlarmManager::addAlarm(h, m);
    } else if (action == "borrar" && i >= 0 && i < AlarmManager::alarmCount) {
        AlarmManager::deleteAlarm(i);
    } else if (action == "cambiar" && i >= 0 && i < AlarmManager::alarmCount) {
        AlarmManager::toggleAlarm(i);
    } else {
        web.send(400, "text/plain", "pedido raro");
        return;
    }
    Serial.printf("[web] alarma: %s\n", action.c_str());
    refreshWatch();
    web.send(200, "text/plain", "ok");
}

// Últimas charlas con JARVIS (las guarda jarvis.cpp).
static void sendHistory() {
    markUse();
    DynamicJsonDocument doc(16384);
    JsonArray list = doc.createNestedArray("charlas");
    char buf[24];
    for (int i = 0; i < Jarvis::historyCount(); i++) {
        const Jarvis::Talk& t = Jarvis::history(i);
        time_t when = t.when;
        struct tm tm;
        localtime_r(&when, &tm);
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M", &tm);
        JsonObject o = list.createNestedObject();
        o["cuando"] = buf;
        o["pregunta"] = t.question;
        o["respuesta"] = t.answer;
        o["acciones"] = t.actions;
    }
    String out;
    serializeJson(doc, out);
    web.sendHeader("Cache-Control", "no-store");
    web.send(200, "application/json", out);
}

// Agenda y planilla: el reloj se las pide a los flujos "JARVIS agenda" y "JARVIS planilla" de
// n8n (por el túnel, con la clave del reloj; así la clave nunca llega al celu). Se guardan
// 2 min para no pedirlas a cada rato.
static const unsigned long PROXY_CACHE_MS = 2UL * 60 * 1000;
struct N8nProxy {
    const char* suffix;                     // se suma a JARVIS_URL: "-agenda" o "-planilla"
    String cache;
    unsigned long at;
};
static N8nProxy agenda = {"-agenda", "", 0};
static N8nProxy planilla = {"-planilla", "", 0};

static void sendFromN8n(N8nProxy& p) {
    markUse();
    bool fresh = web.hasArg("nueva");
    if (!fresh && p.cache.length() && millis() - p.at < PROXY_CACHE_MS) {
        web.send(200, "application/json", p.cache);
        return;
    }
    NetworkClientSecure client;
    client.setInsecure();                   // como JARVIS: lo protege la clave
    HTTPClient http;
    http.setTimeout(10000);
    http.setConnectTimeout(8000);
    int code = -1;
    String body;
    if (http.begin(client, String(JARVIS_URL) + p.suffix)) {
        http.addHeader("X-Jarvis-Clave", JARVIS_KEY);
        http.addHeader("ngrok-skip-browser-warning", "1");
        code = http.GET();
        if (code == 200) body = http.getString();
        http.end();
    }
    Serial.printf("[web] %s: HTTP %d (%u bytes)\n", p.suffix + 1, code, (unsigned)body.length());
    if (code == 200 && body.startsWith("{")) {
        p.cache = body;
        p.at = millis();
        web.send(200, "application/json", body);
    } else {
        // n8n apagado, sin túnel o sin internet: la página lo explica.
        web.send(200, "application/json", String("{\"error\":") + code + "}");
    }
}

static void setRoutes() {
    web.on("/", HTTP_GET, []() {
        markUse();
        web.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
    });
    web.on("/api/estado", HTTP_GET, sendState);
    web.on("/api/ajuste", HTTP_POST, handleSetting);
    web.on("/api/alarma", HTTP_POST, handleAlarm);
    web.on("/api/historial", HTTP_GET, sendHistory);
    web.on("/api/agenda", HTTP_GET, []() { sendFromN8n(agenda); });
    web.on("/api/planilla", HTTP_GET, []() { sendFromN8n(planilla); });
    web.on("/icono.png", HTTP_GET, []() {
        web.sendHeader("Cache-Control", "max-age=604800");
        web.send_P(200, "image/png", (const char*)WEB_ICON_PNG, sizeof(WEB_ICON_PNG));
    });
    web.on("/manifest.json", HTTP_GET, []() {
        web.send_P(200, "application/manifest+json", WEB_MANIFEST);
    });
    web.onNotFound([]() {               // cualquier otra dirección: a la página
        web.sendHeader("Location", "/");
        web.send(302, "text/plain", "");
    });
}

void WebSettings::loop() {
    bool canRun = WiFi.status() == WL_CONNECTED && DisplayUI::getCurrentScreen() != SCREEN_WIFI_CONFIG;
    if (canRun && !started) {
        if (!routesSet) { setRoutes(); routesSet = true; }
        web.begin();
        // jarvis.local: así la dirección es siempre la misma aunque cambie la IP.
        if (MDNS.begin("jarvis")) MDNS.addService("http", "tcp", 80);
        started = true;
        Serial.printf("[web] pagina de ajustes en http://jarvis.local/ y %s\n", ipUrl().c_str());
    } else if (!canRun && started) {
        web.stop();
        MDNS.end();
        started = false;
    }
    if (started) web.handleClient();
}

bool WebSettings::running() { return started; }
bool WebSettings::inUse() { return lastUse != 0 && millis() - lastUse < IN_USE_MS; }
void WebSettings::keepAlive() { markUse(); }
String WebSettings::ipUrl() { return "http://" + WiFi.localIP().toString() + "/"; }
const char* WebSettings::nameUrl() { return "http://jarvis.local/"; }
