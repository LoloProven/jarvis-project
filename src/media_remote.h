#ifndef MEDIA_REMOTE_H
#define MEDIA_REMOTE_H

// Control remoto de música por Bluetooth LE. El reloj se presenta ante el celular como
// un teclado multimedia (HID) con 5 teclas; Spotify (o cualquier app) responde igual que
// a los botones de los auriculares. El audio sigue yendo del celular a los auriculares:
// el reloj solo manda las órdenes.
//
// La primera vez se vincula desde Ajustes > Bluetooth del celular ("JARVIS Reloj");
// después se reconecta solo.
class MediaRemote {
public:
    enum Key { PREV, PLAY_PAUSE, NEXT, VOL_UP, VOL_DOWN };

    static void init();
    static bool isConnected();
    static void press(Key key);
};

#endif
