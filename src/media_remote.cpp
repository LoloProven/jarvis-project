#include "media_remote.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>

// Armado igual que la librería ESP32-BLE-Keyboard (T-vK), que funciona con iPhone:
// un teclado (reporte 1, que no se usa pero iOS lo espera) más las teclas multimedia
// (reporte 2, 16 bits). La primera versión, solo multimedia y con el código de fabricante
// de Apple del ejemplo Server_Gamepad, se vinculaba pero el iPhone ignoraba las teclas.
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
    // --- Teclas multimedia (reporte 2, 16 bits) ---
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01,       // Consumer / Consumer Control / Collection
    0x85, 0x02,                               //   Report ID (2)
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x07,
    0x09, 0xB5,                               //   bit 0: siguiente
    0x09, 0xB6,                               //   bit 1: anterior
    0x09, 0xB7,                               //   bit 2: stop
    0x09, 0xCD,                               //   bit 3: play/pausa
    0x09, 0xE2,                               //   bit 4: mute
    0x09, 0xE9,                               //   bit 5: volumen +
    0x09, 0xEA,                               //   bit 6: volumen -
    0x81, 0x02,
    0x95, 0x09, 0x81, 0x01,                   //   relleno hasta 16 bits
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

static BLEHIDDevice* hid = nullptr;
static BLECharacteristic* inputKeyboard = nullptr;
static BLECharacteristic* inputMedia = nullptr;
static volatile bool connected = false;
static volatile bool mediaSubscribed = false;   // el celular pidió recibir las teclas

class RemoteServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        connected = true;
        Serial.println("[bt] celular conectado");
    }
    void onDisconnect(BLEServer*) override {
        connected = false;
        mediaSubscribed = false;
        Serial.println("[bt] celular desconectado");
        BLEDevice::startAdvertising();       // vuelve a quedar visible para reconectarse
    }
};

class RemoteSecurityCallbacks : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() override { return 0; }
    void onPassKeyNotify(uint32_t) override {}
    bool onSecurityRequest() override { return true; }
    bool onConfirmPIN(uint32_t) override { return true; }
    void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
        Serial.printf("[bt] vinculacion %s (cifrado=%d, vinculado=%d)\n",
                      desc->sec_state.encrypted ? "OK" : "FALLO",
                      desc->sec_state.encrypted, desc->sec_state.bonded);
    }
};

class MediaReportCallbacks : public BLECharacteristicCallbacks {
    void onSubscribe(BLECharacteristic*, ble_gap_conn_desc*, uint16_t subValue) override {
        mediaSubscribed = (subValue != 0);
        Serial.printf("[bt] el celular %s las teclas multimedia\n",
                      mediaSubscribed ? "se suscribio a" : "se desuscribio de");
    }
};

void MediaRemote::init() {
    BLEDevice::init(DEVICE_NAME);

    // "Just Works": se vincula sin código, como unos auriculares o un control remoto.
    BLESecurity* security = new BLESecurity();
    security->setCapability(ESP_IO_CAP_NONE);
    security->setAuthenticationMode(true, false, true);   // bonding, sin MITM, conexión segura
    BLEDevice::setSecurityCallbacks(new RemoteSecurityCallbacks());

    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new RemoteServerCallbacks());

    hid = new BLEHIDDevice(server);
    inputKeyboard = hid->inputReport(1);
    hid->outputReport(1);                  // LEDs del teclado (no se usan)
    inputMedia = hid->inputReport(2);
    inputMedia->setCallbacks(new MediaReportCallbacks());

    hid->manufacturer()->setValue("Proyecto JARVIS");
    hid->pnp(0x02, 0xe502, 0xa111, 0x0210);   // IDs de ESP32-BLE-Keyboard (antes eran los de Apple)
    hid->hidInfo(0x00, 0x01);
    hid->reportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
    hid->setBatteryLevel(100);
    hid->startServices();

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->setAppearance(HID_KEYBOARD);
    adv->addServiceUUID(hid->hidService()->getUUID());
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();
    Serial.println("[bt] visible como \"JARVIS Reloj\"");
}

bool MediaRemote::isConnected() { return connected; }

// Una "pulsación": se manda la tecla apretada y enseguida suelta (2 bytes, little endian).
void MediaRemote::press(Key key) {
    Serial.printf("[bt] tecla %d (conectado=%d, suscripto=%d)\n", (int)key, connected, mediaSubscribed);
    if (!connected || inputMedia == nullptr) return;
    uint16_t bits = keyBit(key);
    uint8_t report[2] = { (uint8_t)(bits & 0xFF), (uint8_t)(bits >> 8) };
    inputMedia->setValue(report, 2);
    inputMedia->notify();
    delay(30);
    report[0] = report[1] = 0;
    inputMedia->setValue(report, 2);
    inputMedia->notify();
}
