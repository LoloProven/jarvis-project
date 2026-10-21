#ifndef TIME_SYNC_H
#define TIME_SYNC_H

class TimeSync {
public:
    static void init();        // zona horaria + hora de respaldo + aviso de WiFi conectado
    static void loop();        // pide la hora por NTP cuando el WiFi se conecta
    static bool isSynced();    // true si alguna vez llegó la hora por NTP
};

#endif
