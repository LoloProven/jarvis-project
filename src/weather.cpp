#include "weather.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Córdoba (las mismas coordenadas que usa la herramienta Clima de JARVIS en n8n).
static const char* URL = "http://api.open-meteo.com/v1/forecast?latitude=-31.42&longitude=-64.18"
                         "&current=temperature_2m,weather_code&timezone=auto";
static const unsigned long EVERY_MS = 30UL * 60 * 1000;
static const unsigned long RETRY_MS = 2UL * 60 * 1000;    // si falló, reintenta a los 2 min

static volatile bool busy = false;
static bool hasData = false;
static int temp = 0;
static int code = -1;
static volatile uint32_t ver = 0;
static unsigned long lastTry = 0;
static bool lastOk = false;

// Códigos WMO de Open-Meteo, en pocas palabras (entran en la tarjeta CLIMA).
static const char* describe(int c) {
    if (c == 0) return "despejado";
    if (c <= 2) return "algo nublado";
    if (c == 3) return "nublado";
    if (c == 45 || c == 48) return "niebla";
    if (c >= 51 && c <= 57) return "llovizna";
    if (c >= 61 && c <= 67) return "lluvia";
    if (c >= 71 && c <= 77) return "nieve";
    if (c >= 80 && c <= 82) return "chaparrones";
    if (c >= 85 && c <= 86) return "nieve";
    if (c >= 95) return "tormenta";
    return "";
}

static void fetchTask(void*) {
    HTTPClient http;
    http.setTimeout(8000);
    bool ok = false;
    if (http.begin(URL)) {
        int status = http.GET();
        if (status == 200) {
            StaticJsonDocument<128> filter;
            filter["current"]["temperature_2m"] = true;
            filter["current"]["weather_code"] = true;
            DynamicJsonDocument doc(512);
            // getString() junta el cuerpo aunque venga "chunked" (getStream() no lo arma).
            String body = http.getString();
            if (!deserializeJson(doc, body, DeserializationOption::Filter(filter)) &&
                doc["current"]["temperature_2m"].is<float>()) {
                temp = (int)lroundf(doc["current"]["temperature_2m"].as<float>());
                code = doc["current"]["weather_code"] | -1;
                hasData = true;
                ver = ver + 1;
                ok = true;
            }
        }
        Serial.printf("[clima] HTTP %d -> %s %d C (codigo %d)\n", status, ok ? "ok" : "fallo", temp, code);
        http.end();
    }
    lastOk = ok;
    busy = false;
    vTaskDelete(nullptr);
}

void Weather::loop() {
    if (busy || WiFi.status() != WL_CONNECTED) return;
    unsigned long wait = lastOk ? EVERY_MS : RETRY_MS;
    if (lastTry != 0 && millis() - lastTry < wait) return;
    lastTry = millis();
    busy = true;
    if (xTaskCreate(fetchTask, "clima", 6144, nullptr, 1, nullptr) != pdPASS) busy = false;
}

bool Weather::available() { return hasData; }
int Weather::temperature() { return temp; }
const char* Weather::description() { return describe(code); }
uint32_t Weather::version() { return ver; }
