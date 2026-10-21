#include "media_remote.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>

// Armado como el ejemplo Server_Gamepad de la librería BLE del core (BLEHIDDevice).
static const char* DEVICE_NAME = "JARVIS Reloj";

// Descriptor HID: un reporte (ID 1) de 1 byte con 5 teclas multimedia ("Consumer
// Control"), un bit por tecla, en el mismo orden que MediaRemote::Key.
static const uint8_t REPORT_MAP[] = {
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1 bit)
    0x95, 0x05,        //   Report Count (5)
    0x09, 0xB6,        //   Usage (Scan Previous Track)  -> bit 0: PREV
    0x09, 0xCD,        //   Usage (Play/Pause)           -> bit 1: PLAY_PAUSE
    0x09, 0xB5,        //   Usage (Scan Next Track)      -> bit 2: NEXT
    0x09, 0xE9,        //   Usage (Volume Increment)     -> bit 3: VOL_UP
    0x09, 0xEA,        //   Usage (Volume Decrement)     -> bit 4: VOL_DOWN
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0x95, 0x03,        //   Report Count (3)             -> relleno hasta 8 bits
    0x81, 0x01,        //   Input (Constant)
    0xC0               // End Collection
};

static BLEHIDDevice* hid = nullptr;
static BLECharacteristic* input = nullptr;
static volatile bool connected = false;

class RemoteServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override { connected = true; }
    void onDisconnect(BLEServer*) override {
        connected = false;
        BLEDevice::startAdvertising();       // vuelve a quedar visible para reconectarse
    }
};

void MediaRemote::init() {
    BLEDevice::init(DEVICE_NAME);

    // "Just Works": se vincula sin código, como unos auriculares o un control remoto.
    BLESecurity* security = new BLESecurity();
    security->setCapability(ESP_IO_CAP_NONE);
    security->setAuthenticationMode(true, false, true);   // bonding, sin MITM, conexión segura

    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new RemoteServerCallbacks());

    hid = new BLEHIDDevice(server);
    hid->manufacturer()->setValue("Proyecto JARVIS");
    hid->pnp(0x02, 0x05ac, 0x820a, 0x0110);
    hid->hidInfo(0x00, 0x01);
    hid->reportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
    input = hid->inputReport(1);
    hid->setBatteryLevel(100);
    hid->startServices();

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->setAppearance(HID_KEYBOARD);
    adv->addServiceUUID(hid->hidService()->getUUID());
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);
    adv->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();
}

bool MediaRemote::isConnected() { return connected; }

// Una "pulsación": se manda la tecla apretada y enseguida suelta.
void MediaRemote::press(Key key) {
    if (!connected || input == nullptr) return;
    uint8_t report = 1 << key;
    input->setValue(&report, 1);
    input->notify();
    delay(30);
    report = 0;
    input->setValue(&report, 1);
    input->notify();
}
