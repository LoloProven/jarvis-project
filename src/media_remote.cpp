#include "media_remote.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

// Hecho con NimBLE-Arduino (h2zero), como la versión NimBLE de ESP32-BLE-Keyboard, que
// funciona con iPhone: teclado (reporte 1, no se usa pero iOS lo espera) + teclas
// multimedia (reporte 2, 16 bits).
//
// Con la librería BLE del core no funcionaba: con NimBLE descarta la segunda
// característica con el mismo UUID (en HID todos los reportes son 0x2A4D), así que no
// se podía tener teclado + multimedia, y con un solo reporte el iPhone ignoraba las teclas.
static const char* DEVICE_NAME = "JARVIS Reloj";

static const uint8_t REPORT_MAP[] = {
    // --- Teclado (reporte 1) ---
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,       // Generic Desktop / Keyboard / Collection
    0x85, 0x01,                               //   Report ID (1)
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,       //   modificadores (Ctrl, Shift, ...)
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,       //   byte reservado
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,   // LEDs (salida)
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,   // 6 teclas
    0xC0,
    // --- Teclas multimedia (reporte 2, 16 bits), tal cual ESP32-BLE-Keyboard ---
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,       // Consumer / Consumer Control / Collection
    0x85, 0x02,                               //   Report ID (2)
    0x05, 0x0C,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10,   // 16 bits
    0x09, 0xB5,                               //   bit 0: siguiente
    0x09, 0xB6,                               //   bit 1: anterior
    0x09, 0xB7,                               //   bit 2: stop
    0x09, 0xCD,                               //   bit 3: play/pausa
    0x09, 0xE2,                               //   bit 4: mute
    0x09, 0xE9,                               //   bit 5: volumen +
    0x09, 0xEA,                               //   bit 6: volumen -
    0x0A, 0x23, 0x02, 0x0A, 0x94, 0x01, 0x0A, 0x92, 0x01, 0x0A, 0x2A, 0x02,   // bits 7-10
    0x0A, 0x21, 0x02, 0x0A, 0x26, 0x02, 0x0A, 0x24, 0x02, 0x0A, 0x83, 0x01,   // bits 11-14
    0x0A, 0x8A, 0x01,                                                         // bit 15
    0x81, 0x02,
    0xC0
};

static uint16_t keyBit(MediaRemote::Key k) {
    switch (k) {
        case MediaRemote::NEXT:       return 1 << 0;
        case MediaRemote::PREV:       return 1 << 1;
        case MediaRemote::PLAY_PAUSE: return 1 << 3;
        case MediaRemote::VOL_UP:     return 1 << 5;
        case MediaRemote::VOL_DOWN:   return 1 << 6;
    }
    return 0;
}

static NimBLEHIDDevice* hid = nullptr;
static NimBLECharacteristic* inputMedia = nullptr;
static volatile bool connected = false;
static volatile bool mediaSubscribed = false;

// --- AMS (Apple Media Service): qué canción suena en el iPhone ---
// Por la misma conexión, el reloj actúa también como cliente del iPhone: se suscribe a
// "Entity Update" y pide título y artista de la canción y el estado del reproductor.
// El iPhone avisa solo cada vez que cambian. (Solo iPhone: Android no tiene AMS.)
static const NimBLEUUID AMS_SERVICE("89D3502B-0F36-433A-8EF4-C502AD55F8DC");
static const NimBLEUUID AMS_ENTITY_UPDATE("2F7CABCE-808D-411F-9A0C-BB92BA96C102");
static const uint8_t AMS_ENTITY_PLAYER = 0, AMS_ENTITY_TRACK = 2;
static const uint8_t AMS_PLAYER_PLAYBACK_INFO = 1;
static const uint8_t AMS_TRACK_ARTIST = 0, AMS_TRACK_TITLE = 2;

static NimBLEClient* amsClient = nullptr;
static char trackTitle[48] = "";
static char trackArtist[40] = "";
static volatile bool trackPlaying = false;
static volatile uint32_t trackVersion = 0;     // sube cada vez que cambia algo (para redibujar)

// La letra del reloj no tiene tildes ni ñ: se pasan a la letra sin acento (UTF-8 -> ASCII).
static void toAscii(const uint8_t* in, size_t len, char* out, size_t outLen) {
    size_t o = 0;
    for (size_t i = 0; i < len && o + 1 < outLen; i++) {
        uint8_t c = in[i];
        if (c < 0x80) { out[o++] = (c >= 0x20) ? c : ' '; continue; }
        if (c == 0xC3 && i + 1 < len) {            // Latin-1: á é í ó ú ñ ü ç ...
            uint8_t d = in[++i];
            const char* map = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
            out[o++] = (d >= 0x80 && d <= 0xBF) ? map[d - 0x80] : '?';
            continue;
        }
        // Otros caracteres (emojis, etc.): se saltean los bytes de continuación.
        out[o++] = '?';
        while (i + 1 < len && (in[i + 1] & 0xC0) == 0x80) i++;
    }
    out[o] = 0;
}

static void onAmsUpdate(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    if (len < 3) return;
    uint8_t entity = data[0], attr = data[1];        // data[2] = flags; el valor sigue
    const uint8_t* val = data + 3;
    size_t vlen = len - 3;
    if (entity == AMS_ENTITY_TRACK && attr == AMS_TRACK_TITLE) {
        toAscii(val, vlen, trackTitle, sizeof(trackTitle));
    } else if (entity == AMS_ENTITY_TRACK && attr == AMS_TRACK_ARTIST) {
        toAscii(val, vlen, trackArtist, sizeof(trackArtist));
    } else if (entity == AMS_ENTITY_PLAYER && attr == AMS_PLAYER_PLAYBACK_INFO) {
        trackPlaying = (vlen > 0 && val[0] == '1');  // "estado,velocidad,tiempo": 1 = sonando
    } else {
        return;
    }
    trackVersion = trackVersion + 1;
    Serial.printf("[ams] %s - %s (%s)\n", trackTitle, trackArtist, trackPlaying ? "sonando" : "en pausa");
}

// Busca el servicio AMS en el iPhone y se suscribe. Tarda un poco (descubre los servicios),
// así que corre en una tarea aparte para no frenar la pantalla; termina sola.
static void amsSetupTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(500));
    NimBLEClient* c = amsClient;
    NimBLERemoteService* svc = (c && c->isConnected()) ? c->getService(AMS_SERVICE) : nullptr;
    NimBLERemoteCharacteristic* upd = svc ? svc->getCharacteristic(AMS_ENTITY_UPDATE) : nullptr;
    if (upd && upd->subscribe(true, onAmsUpdate)) {
        const uint8_t track[]  = { AMS_ENTITY_TRACK, AMS_TRACK_ARTIST, AMS_TRACK_TITLE };
        const uint8_t player[] = { AMS_ENTITY_PLAYER, AMS_PLAYER_PLAYBACK_INFO };
        upd->writeValue(track, sizeof(track), true);
        upd->writeValue(player, sizeof(player), true);
        Serial.println("[ams] suscripto a la cancion que suena");
    } else {
        Serial.println("[ams] no se encontro el servicio de musica del iPhone");
    }
    vTaskDelete(NULL);
}

class RemoteServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
        connected = true;
        amsClient = server->getClient(info);  // para pedirle datos al iPhone por esta conexión
        Serial.println("[bt] celular conectado");
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
        connected = false;
        mediaSubscribed = false;
        amsClient = nullptr;
        trackTitle[0] = trackArtist[0] = 0;
        trackPlaying = false;
        trackVersion = trackVersion + 1;
        Serial.printf("[bt] celular desconectado (motivo %d)\n", reason);
        // advertiseOnDisconnect(true) lo vuelve a dejar visible solo.
    }
    void onAuthenticationComplete(NimBLEConnInfo& info) override {
        Serial.printf("[bt] vinculacion %s (cifrado=%d, vinculado=%d)\n",
                      info.isEncrypted() ? "OK" : "FALLO", info.isEncrypted(), info.isBonded());
        // AMS exige la conexión cifrada: recién ahora se puede pedir la canción.
        if (info.isEncrypted()) xTaskCreate(amsSetupTask, "ams", 6144, NULL, 1, NULL);
    }
};

class MediaReportCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
        mediaSubscribed = (subValue != 0);
        Serial.printf("[bt] el celular %s las teclas multimedia\n",
                      mediaSubscribed ? "se suscribio a" : "se desuscribio de");
    }
};

void MediaRemote::init() {
    if (hid != nullptr) return;
    NimBLEDevice::init(DEVICE_NAME);

    // "Just Works": se vincula sin código, como unos auriculares o un control remoto.
    NimBLEDevice::setSecurityAuth(true, false, true);    // bonding, sin MITM, conexión segura
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(new RemoteServerCallbacks());
    server->advertiseOnDisconnect(true);

    hid = new NimBLEHIDDevice(server);
    hid->getInputReport(1);                // teclado (no se usa)
    hid->getOutputReport(1);               // LEDs del teclado (no se usan)
    inputMedia = hid->getInputReport(2);
    inputMedia->setCallbacks(new MediaReportCallbacks());

    hid->setManufacturer("Proyecto JARVIS");
    hid->setPnp(0x02, 0xe502, 0xa111, 0x0210);   // IDs de ESP32-BLE-Keyboard
    hid->setHidInfo(0x00, 0x01);
    hid->setReportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
    hid->setBatteryLevel(100);
    server->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAppearance(HID_KEYBOARD);
    adv->addServiceUUID(hid->getHidService()->getUUID());
    adv->enableScanResponse(true);
    adv->setName(DEVICE_NAME);
    adv->start();
    Serial.println("[bt] visible como \"JARVIS Reloj\"");
}

// Apaga el Bluetooth del todo (para el modo dormir: con el Bluetooth prendido el chip
// no puede entrar en sueño liviano). init() lo vuelve a prender; el iPhone se reconecta solo.
void MediaRemote::stop() {
    if (hid == nullptr) return;
    NimBLEDevice::deinit(true);            // true: libera servidor, servicios y callbacks
    hid = nullptr;
    inputMedia = nullptr;
    connected = false;
    mediaSubscribed = false;
    amsClient = nullptr;
    trackTitle[0] = trackArtist[0] = 0;
    trackPlaying = false;
    trackVersion = trackVersion + 1;
    Serial.println("[bt] apagado");
}

const char* MediaRemote::title() { return trackTitle; }
const char* MediaRemote::artist() { return trackArtist; }
bool MediaRemote::isPlaying() { return trackPlaying; }
uint32_t MediaRemote::trackInfoVersion() { return trackVersion; }

bool MediaRemote::isEnabled() { return hid != nullptr; }
bool MediaRemote::isConnected() { return connected; }

// Una "pulsación": se manda la tecla apretada y enseguida suelta (2 bytes, little endian).
void MediaRemote::press(Key key) {
    Serial.printf("[bt] tecla %d (conectado=%d, suscripto=%d)\n", (int)key, connected, mediaSubscribed);
    if (!connected || inputMedia == nullptr) return;
    uint16_t bits = keyBit(key);
    uint8_t report[2] = { (uint8_t)(bits & 0xFF), (uint8_t)(bits >> 8) };
    inputMedia->setValue(report, 2);
    inputMedia->notify();
    report[0] = report[1] = 0;               // soltar enseguida (como ESP32-BLE-Keyboard)
    inputMedia->setValue(report, 2);
    inputMedia->notify();
}
