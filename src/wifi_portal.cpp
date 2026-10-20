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
static bool handlersRegistered = false;

static const uint32_t CONNECT_TIMEOUT_MS  = 8000;    // por intento (incluye buscar redes)
static const uint32_t RECONNECT_EVERY_MS  = 15000;   // cada cuánto reintenta si no hay WiFi

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

// Si no hay WiFi, busca entre las redes conocidas la que esté disponible con mejor
// señal y se conecta. Así, al salir de casa se pasa sola al hotspot del celular.
// Corre en el núcleo 0 para no trabar la pantalla mientras busca.
static void wifiKeeperTask(void*) {
    for (;;) {
        if (!portalActive && WiFi.status() != WL_CONNECTED) {
            xSemaphoreTake(wifiLock, portMAX_DELAY);
            wifiMulti.run(CONNECT_TIMEOUT_MS);
            xSemaphoreGive(wifiLock);
        }
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_EVERY_MS));
    }
}

void WiFiPortal::initSaved() {
    wifiLock = xSemaphoreCreateMutex();
    WiFi.mode(WIFI_STA);

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

    // Primer intento al arrancar (bloquea como máximo CONNECT_TIMEOUT_MS).
    xSemaphoreTake(wifiLock, portMAX_DELAY);
    wifiMulti.run(CONNECT_TIMEOUT_MS);
    xSemaphoreGive(wifiLock);

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
