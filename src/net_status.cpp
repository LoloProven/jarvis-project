#include "net_status.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Página que responde "204 sin contenido" cuando hay internet de verdad (la misma
// que usa Android). Si un portal cautivo o un router sin salida responde otra cosa,
// cuenta como "sin internet".
static const char* CHECK_URL = "http://connectivitycheck.gstatic.com/generate_204";
static const uint32_t CHECK_EVERY_ONLINE_MS  = 30000;   // con internet: revisar cada 30 s
static const uint32_t CHECK_EVERY_OFFLINE_MS = 10000;   // sin internet: reintentar cada 10 s
static const uint16_t HTTP_TIMEOUT_MS        = 4000;

static volatile NetState state = NET_NO_WIFI;
static volatile bool paused = false;       // con la pantalla apagada no se revisa (ahorra batería)

static bool hasInternet() {
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(CHECK_URL)) return false;
    int code = http.GET();
    http.end();
    return code == 204;
}

static void netTask(void*) {
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            state = NET_NO_WIFI;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (paused) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (state == NET_NO_WIFI) state = NET_CHECKING;   // recién conectado

        bool ok = hasInternet();
        if (WiFi.status() != WL_CONNECTED) continue;      // se cayó mientras probaba
        state = ok ? NET_ONLINE : NET_NO_INTERNET;

        // Espera en tramos cortos para notar enseguida si se corta el WiFi.
        uint32_t wait = ok ? CHECK_EVERY_ONLINE_MS : CHECK_EVERY_OFFLINE_MS;
        for (uint32_t t = 0; t < wait && WiFi.status() == WL_CONNECTED && !paused; t += 500) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

void NetStatus::init() {
    // Núcleo 0 (el del WiFi); el loop de Arduino y la pantalla corren en el 1.
    xTaskCreatePinnedToCore(netTask, "netStatus", 6144, NULL, 1, NULL, 0);
}

NetState NetStatus::get() { return state; }
void NetStatus::setPaused(bool p) { paused = p; }
