#ifndef JARVIS_H
#define JARVIS_H

#include <stdint.h>

// Asistente JARVIS: graba la pregunta, la manda a n8n (JARVIS_URL en config.h) y recibe
// la respuesta: el texto en los encabezados X-Pregunta / X-Respuesta y la voz en el cuerpo
// (mono, 16 bits, 24 kHz). La IA, la voz y Google Calendar van en n8n
// (flujo en n8n-flujos/jarvis-reloj.json).
class Jarvis {
public:
    enum Result { OK, NO_WIFI, TOO_SHORT, SERVER_ERROR };

    // Bloquea hasta terminar de hablar la respuesta. onStatus avisa cada paso
    // ("Escuchando...", "Pensando..."); onAnswer, cuando ya están question() y answer().
    static Result ask(const char* profile, bool (*keepRecording)(),
                      void (*onStatus)(const char*), void (*onAnswer)());
    static const char* question();
    static const char* answer();
    static int lastHttpCode();          // para mostrar el error si falla

    // Historial de las últimas charlas (para la página de ajustes), guardado en la memoria
    // permanente: sobrevive a un reinicio. 0 = la más nueva.
    struct Talk {
        uint32_t when;                  // hora (time_t)
        char question[160];
        char answer[400];
        char actions[80];               // lo que hizo en el reloj ("ALARMA 07:00;TIMER 600")
    };
    static int historyCount();
    static const Talk& history(int i);
};

#endif
