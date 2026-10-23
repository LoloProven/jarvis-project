#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <stdint.h>

class NimBLEClient;

// Notificaciones del iPhone (WhatsApp, mensajes, llamadas...) por ANCS, el servicio de
// Apple para relojes. Usa la misma conexión Bluetooth que el control de la música.
// La primera vez el iPhone pide permiso; si no, en Ajustes > Bluetooth > JARVIS Reloj (i)
// activar "Compartir notificaciones del sistema". Solo iPhone (Android no tiene ANCS).
// En modo dormir el Bluetooth está apagado: no llegan (las anteriores no se muestran).
class Notifications {
public:
    struct Notice {
        char app[20];          // "WhatsApp", "Mensajes", "Telefono"...
        char title[48];        // quién manda (o el grupo)
        char message[128];     // el texto, sin tildes y cortado
        bool call;             // llamada entrante
    };

    static void attach(NimBLEClient* client);   // tras la vinculación cifrada (media_remote)
    static void detach();                       // se cortó o se apaga el Bluetooth
    static bool take(Notice& out);              // true si llegó una nueva (se saca de la cola)
    static bool callActive();                   // la llamada sigue sonando en el iPhone
    static void rejectCall();                   // cortar la llamada entrante
};

#endif
