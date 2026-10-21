#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

// Redes conocidas: la guardada desde el portal, la de casa (secrets.h) y, si está
// definida en secrets.h, el hotspot del celular. Si se pierde la conexión, una tarea
// en segundo plano busca cuál de ellas hay cerca y se conecta sola.
class WiFiPortal {
public:
    static void initSaved();
    static void startPortal();
    static void stopPortal();      // botón VOLVER: cierra el portal y vuelve a buscar redes
    static void handlePortal();
    static void suspend();         // apaga el WiFi para ahorrar batería
    static void resume();          // lo vuelve a prender y reconecta enseguida
    static bool isSuspended();
};

#endif
