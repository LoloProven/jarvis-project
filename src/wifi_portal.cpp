#include "wifi_portal.h"
#include "display_ui.h"
#include "config.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>

static WebServer server(80);
static DNSServer dnsServer;
static Preferences prefs;

void WiFiPortal::initSaved() {
    prefs.begin("wifi_config", true);
    String savedSSID = prefs.getString("ssid", MY_WIFI_SSID);
    String savedPASS = prefs.getString("pass", MY_WIFI_PASSWORD);
    prefs.end();

    if (savedSSID.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID.c_str(), savedPASS.c_str());
        
        // Espera no bloqueante corta (máximo 6 segundos)
        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 6000) {
            delay(100);
        }
    }
}

void WiFiPortal::startPortal() {
    DisplayUI::setScreenState(SCREEN_WIFI_CONFIG);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("JARVIS-Clock-Config");
    dnsServer.start(53, "*", WiFi.softAPIP());

    DisplayUI::drawWifiPortalScreen();

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

        server.send(200, "text/html", "<html><body><h2>Guardado! Conectando...</h2></body></html>");
        delay(1000);
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(reqSSID.c_str(), reqPASS.c_str());
        DisplayUI::setScreenState(SCREEN_PROFILES);
        DisplayUI::drawProfileMenu();
    });

    server.begin();
}

void WiFiPortal::handlePortal() {
    dnsServer.processNextRequest();
    server.handleClient();
}