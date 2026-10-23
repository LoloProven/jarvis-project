#include "wifi_portal.h"
#include "display_ui.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>

static WebServer server(80);
static DNSServer dnsServer;
static Preferences prefs;

static WiFiMulti wifiMulti;
static SemaphoreHandle_t wifiLock = NULL;      // wifiMulti se usa desde el loop y desde la tarea
static volatile bool portalActive = false;     // con el portal abierto no se buscan redes
static volatile bool suspended = false;        // WiFi apagado para ahorrar batería
static volatile bool kick = false;             // pide reconectar ya, sin esperar
static bool handlersRegistered = false;

static const uint32_t CONNECT_TIMEOUT_MS  = 8000;    // por intento (incluye buscar redes)
// Si no encuentra ninguna red conocida (p. ej. en la calle), cada intento fallido duplica la
// espera hasta el próximo: 15 s, 30 s, 1 min... hasta 5 min. Antes probaba cada 15 s sin
// parar, y buscar redes es de lo que más batería gasta. Al conectarse vuelve a 15 s.
static const uint32_t RECONNECT_MIN_MS    = 15000;
static const uint32_t RECONNECT_MAX_MS    = 300000;

// Suma una red a la lista si tiene nombre y no estaba ya.
static void addNetwork(const String& ssid, const String& pass) {
    static String known[4];
    static int knownCount = 0;
    if (ssid.length() == 0) return;
    for (int i = 0; i < knownCount; i++) if (known[i] == ssid) return;
    if (knownCount < 4) known[knownCount++] = ssid;

    xSemaphoreTake(wifiLock, portMAX_DELAY);
    wifiMulti.addAP(ssid.c_str(), pass.c_str());
    xSemaphoreGive(wifiLock);
}

#if defined(TEST_ONLY_HOTSPOT) && defined(HAS_HOTSPOT)
// Diagnóstico por USB (solo en la versión de prueba): dice si el hotspot se ve y por
// qué falla la conexión, sin imprimir nombres de redes ni claves.
static void diagHotspot() {
    String want = HOTSPOT_SSID;
    String wantLower = want; wantLower.toLowerCase();
    int n = WiFi.scanNetworks();
    bool found = false;
    Serial.printf("[diag] redes visibles (2,4 GHz): %d\n", n);
    for (int i = 0; i < n; i++) {
        String s = WiFi.SSID(i);
        String sl = s; sl.toLowerCase();
        if (s == want) {
            found = true;
            Serial.printf("[diag] HOTSPOT VISTO: canal %d, senal %d dBm, seguridad %d\n",
                          WiFi.channel(i), WiFi.RSSI(i), (int)WiFi.encryptionType(i));
        } else if (sl == wantLower) {
            Serial.println("[diag] red IGUAL salvo MAYUSCULAS/minusculas");
            for (unsigned j = 0; j < s.length(); j++) {
                if (s[j] != want[j]) {
                    Serial.printf("[diag]   posicion %u: el celular transmite '%c', secrets.h tiene '%c'\n",
                                  j + 1, s[j], want[j]);
                }
            }
            // Prueba de confirmación: conectarse con el nombre tal como lo transmite el celular.
            String realSsid = s;
            WiFi.scanDelete();
            Serial.println("[diag] probando conectar con el nombre tal como lo transmite el celular...");
            WiFi.begin(realSsid.c_str(), HOTSPOT_PASS);
            unsigned long t0 = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) vTaskDelay(pdMS_TO_TICKS(200));
            Serial.printf("[diag] con el nombre real: %s (estado %d)\n",
                          WiFi.status() == WL_CONNECTED ? "CONECTADO" : "fallo", (int)WiFi.status());
            return;
        } else if (s.length() > 0 && (sl.indexOf(wantLower) >= 0 || wantLower.indexOf(sl) >= 0)) {
            Serial.printf("[diag] red que CONTIENE o esta contenida en el nombre (largo %d vs %d)\n",
                          s.length(), want.length());
        } else if (s.length() == want.length()) {
            int diff = 0, firstPos = -1;
            for (unsigned j = 0; j < s.length(); j++) {
                if (sl[j] != wantLower[j]) { diff++; if (firstPos < 0) firstPos = j; }
            }
            if (diff <= 3) {
                Serial.printf("[diag] red con el mismo largo y %d letra(s) distinta(s), la primera en la posicion %d "
                              "(codigo visto %d, esperado %d)\n", diff, firstPos + 1,
                              (int)(uint8_t)s[firstPos], (int)(uint8_t)want[firstPos]);
            }
        }
    }
    // Cuántas redes ve en el canal 6 (donde la PC ve el hotspot) y la señal más fuerte.
    int ch6 = 0, best = -127;
    for (int i = 0; i < n; i++) {
        if (WiFi.channel(i) == 6) ch6++;
        if (WiFi.RSSI(i) > best) best = WiFi.RSSI(i);
    }
    Serial.printf("[diag] redes en canal 6: %d, senal mas fuerte vista: %d dBm\n", ch6, best);
    WiFi.scanDelete();

    if (!found) {
        Serial.printf("[diag] hotspot NO visible en la busqueda general (largo esperado %d)\n", want.length());

        // Búsqueda dirigida: pregunta por la red por su nombre, en el canal 6 y en todos,
        // con más tiempo por canal. Algunos celulares solo responden así.
        int m = WiFi.scanNetworks(false, true, false, 600, 6, HOTSPOT_SSID);
        Serial.printf("[diag] busqueda por nombre en canal 6: %d resultado(s)\n", m < 0 ? 0 : m);
        for (int i = 0; i < m; i++) {
            Serial.printf("[diag]   -> canal %d, senal %d dBm, seguridad %d\n",
                          WiFi.channel(i), WiFi.RSSI(i), (int)WiFi.encryptionType(i));
        }
        WiFi.scanDelete();
        m = WiFi.scanNetworks(false, true, false, 600, 0, HOTSPOT_SSID);
        Serial.printf("[diag] busqueda por nombre en todos los canales: %d resultado(s)\n", m < 0 ? 0 : m);
        WiFi.scanDelete();

        // Intento directo, sin WiFiMulti: el motivo de la desconexión sale por el evento.
        Serial.println("[diag] intento directo con WiFi.begin...");
        WiFi.begin(HOTSPOT_SSID, HOTSPOT_PASS);
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) vTaskDelay(pdMS_TO_TICKS(200));
        Serial.printf("[diag] intento directo: %s (estado %d)\n",
                      WiFi.status() == WL_CONNECTED ? "CONECTADO" : "fallo", (int)WiFi.status());
    }
}
#endif

// Si no hay WiFi, busca entre las redes conocidas la que esté disponible con mejor
// señal y se conecta. Así, al salir de casa se pasa sola al hotspot del celular.
// Corre en el núcleo 0 para no trabar la pantalla mientras busca.
static void wifiKeeperTask(void*) {
    uint32_t wait = RECONNECT_MIN_MS;
    for (;;) {
        if (!portalActive && !suspended && WiFi.status() != WL_CONNECTED) {
#if defined(TEST_ONLY_HOTSPOT) && defined(HAS_HOTSPOT)
            diagHotspot();
#endif
            xSemaphoreTake(wifiLock, portMAX_DELAY);
            uint8_t res = wifiMulti.run(CONNECT_TIMEOUT_MS);
            xSemaphoreGive(wifiLock);
            if (res != WL_CONNECTED) wait = min(wait * 2, RECONNECT_MAX_MS);
#ifdef TEST_ONLY_HOTSPOT
            Serial.printf("[diag] resultado de la conexion: %d (3=conectado, 1=no se ve la red, 4=fallo, 6=desconectado)\n", res);
#endif
        }
        // Espera en tramos cortos para reconectar enseguida si resume() lo pide.
        if (WiFi.status() == WL_CONNECTED) wait = RECONNECT_MIN_MS;
        for (uint32_t t = 0; t < wait && !kick; t += 500) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        kick = false;
    }
}

void WiFiPortal::initSaved() {
    wifiLock = xSemaphoreCreateMutex();
    WiFi.mode(WIFI_STA);

#ifdef TEST_ONLY_HOTSPOT
#ifndef HAS_HOTSPOT
    Serial.println("[diag] ATENCION: no se encontro SECRET_HOTSPOT_SSID/PASS en secrets.h");
#endif
    // Motivo de cada desconexión (p. ej. 15 = clave incorrecta, 201 = no encuentra la red,
    // 202 = falla de autenticación, 210 = seguridad incompatible, como solo WPA3).
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        Serial.printf("[diag] desconectado, motivo: %d\n", info.wifi_sta_disconnected.reason);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
#endif

    prefs.begin("wifi_config", true);
    String savedSSID = prefs.getString("ssid", "");
    String savedPASS = prefs.getString("pass", "");
    prefs.end();

    // Para probar el hotspot sin tocar secrets.h: compilar con -DTEST_ONLY_HOTSPOT
    // y el reloj ignora la red de casa y la del portal.
#ifndef TEST_ONLY_HOTSPOT
    addNetwork(savedSSID, savedPASS);                  // la última configurada desde el portal
    addNetwork(MY_WIFI_SSID, MY_WIFI_PASSWORD);        // la de casa
#endif
#ifdef HAS_HOTSPOT
    addNetwork(HOTSPOT_SSID, HOTSPOT_PASS);            // el hotspot del celular
#endif

    // La primera conexión la hace la tarea apenas arranca, en segundo plano. Antes se
    // hacía acá y bloqueaba el arranque ~10 s con la pantalla en negro.
    xTaskCreatePinnedToCore(wifiKeeperTask, "wifiKeeper", 4096, NULL, 1, NULL, 0);
}

void WiFiPortal::startPortal() {
    portalActive = true;
    DisplayUI::setScreenState(SCREEN_WIFI_CONFIG);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("JARVIS-Clock-Config");
    dnsServer.start(53, "*", WiFi.softAPIP());

    DisplayUI::drawWifiPortalScreen();

    if (!handlersRegistered) {
        handlersRegistered = true;
        server.on("/", []() {
            String html = "<html><body><h2>Configurar Wi-Fi JARVIS</h2>"
                          "<form action='/save' method='POST'>"
                          "Red SSID:<br><input type='text' name='s'><br><br>"
                          "Clave:<br><input type='password' name='p'><br><br>"
                          "<input type='submit' value='Guardar'></form></body></html>";
            server.send(200, "text/html", html);
        });

        server.on("/save", []() {
            String reqSSID = server.arg("s");
            String reqPASS = server.arg("p");

            prefs.begin("wifi_config", false);
            prefs.putString("ssid", reqSSID);
            prefs.putString("pass", reqPASS);
            prefs.end();
            addNetwork(reqSSID, reqPASS);

            server.send(200, "text/html", "<html><body><h2>Guardado! Conectando...</h2></body></html>");
            delay(1000);
            WiFiPortal::stopPortal();
            WiFi.begin(reqSSID.c_str(), reqPASS.c_str());
        });
    }

    server.begin();
}

void WiFiPortal::stopPortal() {
    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    portalActive = false;          // la tarea vuelve a buscar redes conocidas
    DisplayUI::setScreenState(SCREEN_PROFILES);
    DisplayUI::drawProfileMenu();
}

void WiFiPortal::handlePortal() {
    dnsServer.processNextRequest();
    server.handleClient();
}

// Apaga la radio WiFi (la pantalla lleva un rato apagada). No toca el portal.
void WiFiPortal::suspend() {
    if (suspended || portalActive) return;
    xSemaphoreTake(wifiLock, portMAX_DELAY);     // espera si la tarea está conectando
    suspended = true;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    xSemaphoreGive(wifiLock);
}

// Vuelve a prender el WiFi y reconecta enseguida a la red conocida disponible.
void WiFiPortal::resume() {
    if (!suspended) return;
    WiFi.mode(WIFI_STA);
    suspended = false;
    kick = true;
}

bool WiFiPortal::isSuspended() { return suspended; }
