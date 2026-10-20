#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>

class AudioManager {
public:
    static void init();
    static void playTone(int volume);
    static void stopTone();
    static void recordAndSend();
    static int masterVolume;

private:
    static bool isRecording;
};

#endif