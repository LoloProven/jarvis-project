#ifndef WEB_SETTINGS_H
#define WEB_SETTINGS_H

#include <Arduino.h>

// Página de ajustes del reloj para abrir desde el celu o la compu: http://jarvis.local/
// (o la IP del reloj). Anda mientras el reloj está conectado a un WiFi (el de casa o el
// hotspot del iPhone) y el celu en la misma red. Muestra qué funciones están prendidas con
// consejos de batería, los ajustes, las alarmas y el registro de la batería.
class WebSettings {
public:
    static void loop();            // en el loop: prende/apaga el servidor según el WiFi y atiende
    static bool running();         // el servidor está escuchando (hay WiFi)
    static bool inUse();           // la página se usó hace poco: no apagar el WiFi ni dormir
    static void keepAlive();       // como si se hubiera usado (pantalla del QR)
    static String ipUrl();         // "http://192.168.0.23/"
    static const char* nameUrl();  // "http://jarvis.local/"
};

#endif
