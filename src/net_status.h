#ifndef NET_STATUS_H
#define NET_STATUS_H

enum NetState {
    NET_NO_WIFI,       // no está conectado a ninguna red
    NET_CHECKING,      // conectado al WiFi, todavía no se sabe si hay internet
    NET_NO_INTERNET,   // conectado al WiFi, pero la prueba de internet falló
    NET_ONLINE         // conectado y con internet
};

// Comprueba en segundo plano (otra tarea de FreeRTOS) si hay salida a internet,
// para no trabar la pantalla mientras espera la respuesta.
class NetStatus {
public:
    static void init();
    static NetState get();
};

#endif
