#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>

class AudioManager {
public:
    static void init();
    static void playTone(int volume);
    static void stopTone();
    // Graba del micrófono (mono, 16 bits, 16 kHz) mientras keepGoing() devuelva true o
    // hasta llenar maxSamples. Devuelve cuántas muestras grabó. Si se pasa "progress", ahí va
    // dejando cuántas lleva (JARVIS las sube mientras se habla).
    static size_t record(int16_t* out, size_t maxSamples, bool (*keepGoing)(),
                         volatile size_t* progress = nullptr);
    // Reproduce voz mono de 16 bits a 24 kHz (la que manda n8n), de a pedazos.
    static void beginPlayback();
    static void playPcm24k(const int16_t* samples, size_t n);
    static void endPlayback();
    static int masterVolume;

private:
    static bool isRecording;
};

#endif