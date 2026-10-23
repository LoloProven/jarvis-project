#include "notifications.h"
#include "text_ascii.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>

// ANCS (Apple Notification Center Service). El iPhone avisa por Notification Source que
// hay una notificación (8 bytes: evento, banderas, categoría, cantidad y su número, UID);
// el texto se pide escribiendo en Control Point y llega por Data Source, a veces partido
// en varios pedazos.
static const NimBLEUUID ANCS_SERVICE("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
static const NimBLEUUID ANCS_NOTIFICATION_SOURCE("9FBF120D-6301-42D9-8C58-25E699A21DBD");
static const NimBLEUUID ANCS_CONTROL_POINT("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
static const NimBLEUUID ANCS_DATA_SOURCE("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");

static const uint8_t EVENT_ADDED = 0, EVENT_REMOVED = 2;
static const uint8_t FLAG_PRE_EXISTING = 1 << 2;    // ya estaba antes de conectarse
static const uint8_t CATEGORY_INCOMING_CALL = 1;
static const uint8_t ATTR_APP = 0, ATTR_TITLE = 1, ATTR_MESSAGE = 3;
static const uint8_t CMD_GET_ATTRIBUTES = 0, CMD_ACTION = 2, ACTION_NEGATIVE = 1;

struct Pending { uint32_t uid; uint8_t category; };

static NimBLERemoteCharacteristic* volatile controlPoint = nullptr;
static SemaphoreHandle_t cpLock = nullptr;           // controlPoint se usa desde dos tareas
static QueueHandle_t pendingQueue = nullptr;         // notificaciones nuevas a pedir el texto
static QueueHandle_t readyQueue = nullptr;           // notificaciones completas para mostrar
static SemaphoreHandle_t answered = nullptr;         // llegó la respuesta completa
static volatile bool callRinging = false;
static volatile uint32_t callUid = 0;

// Respuesta en armado (Data Source) y la categoría de lo que se pidió.
static uint8_t dsBuf[512];
static size_t dsLen = 0;
static uint8_t askedCategory = 0;

static uint32_t readUid(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

// "net.whatsapp.WhatsApp" -> "WhatsApp". Las de Apple con nombre en castellano.
static void appName(const uint8_t* id, size_t len, char* out, size_t outLen) {
    struct { const char* id; const char* name; } known[] = {
        {"net.whatsapp.WhatsApp", "WhatsApp"},
        {"com.apple.MobileSMS", "Mensajes"},
        {"com.apple.mobilephone", "Telefono"},
        {"com.apple.mobilemail", "Mail"},
        {"com.apple.mobilecal", "Calendario"},
        {"com.google.Gmail", "Gmail"},
        {"com.burbn.instagram", "Instagram"},
    };
    for (auto& k : known) {
        if (strlen(k.id) == len && memcmp(k.id, id, len) == 0) {
            strlcpy(out, k.name, outLen);
            return;
        }
    }
    size_t start = 0;                                  // si no, lo que va después del último punto
    for (size_t i = 0; i < len; i++) if (id[i] == '.') start = i + 1;
    toAscii(id + start, len - start, out, outLen);
}

// Junta los pedazos de Data Source hasta tener los 3 datos pedidos (app, título, mensaje).
static void onDataSource(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    if (dsLen + len > sizeof(dsBuf)) dsLen = 0;       // algo salió mal: se descarta
    memcpy(dsBuf + dsLen, data, len);
    dsLen += len;
    if (dsLen < 5 || dsBuf[0] != CMD_GET_ATTRIBUTES) { if (dsLen >= 5) dsLen = 0; return; }

    Notifications::Notice n = {};
    size_t pos = 5;
    for (int got = 0; got < 3; got++) {
        if (pos + 3 > dsLen) return;                    // falta el resto: llega en otro pedazo
        uint8_t attr = dsBuf[pos];
        size_t alen = dsBuf[pos + 1] | (dsBuf[pos + 2] << 8);
        if (pos + 3 + alen > dsLen) return;
        const uint8_t* val = dsBuf + pos + 3;
        if (attr == ATTR_APP) appName(val, alen, n.app, sizeof(n.app));
        else if (attr == ATTR_TITLE) toAscii(val, alen, n.title, sizeof(n.title));
        else if (attr == ATTR_MESSAGE) toAscii(val, alen, n.message, sizeof(n.message));
        pos += 3 + alen;
    }
    dsLen = 0;
    n.call = (askedCategory == CATEGORY_INCOMING_CALL);
    Serial.printf("[ancs] %s | %s | %s\n", n.app, n.title, n.message);
    xQueueSend(readyQueue, &n, 0);
    xSemaphoreGive(answered);
}

static void onNotificationSource(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    if (len < 8) return;
    uint8_t event = data[0], flags = data[1], category = data[2];
    uint32_t uid = readUid(data + 4);
    if (event == EVENT_ADDED) {
        if (flags & FLAG_PRE_EXISTING) return;         // las viejas no se muestran
        if (category == CATEGORY_INCOMING_CALL) { callUid = uid; callRinging = true; }
        Pending p = { uid, category };
        xQueueSend(pendingQueue, &p, 0);
    } else if (event == EVENT_REMOVED && callRinging && uid == callUid) {
        callRinging = false;                           // atendió, cortó o se perdió la llamada
    }
}

// Pide el texto de cada notificación nueva, de a una (la respuesta tiene que terminar
// de llegar antes de pedir la siguiente).
static void ancsWorker(void*) {
    Pending p;
    for (;;) {
        if (xQueueReceive(pendingQueue, &p, portMAX_DELAY) != pdTRUE) continue;
        xSemaphoreTake(answered, 0);                  // descarta un aviso viejo
        xSemaphoreTake(cpLock, portMAX_DELAY);
        bool sent = false;
        NimBLERemoteCharacteristic* cp = controlPoint;
        if (cp) {
            askedCategory = p.category;
            dsLen = 0;
            uint8_t cmd[] = { CMD_GET_ATTRIBUTES,
                              (uint8_t)p.uid, (uint8_t)(p.uid >> 8), (uint8_t)(p.uid >> 16), (uint8_t)(p.uid >> 24),
                              ATTR_APP,
                              ATTR_TITLE, 40, 0,
                              ATTR_MESSAGE, 120, 0 };
            sent = cp->writeValue(cmd, sizeof(cmd), true);
        }
        xSemaphoreGive(cpLock);
        if (sent) xSemaphoreTake(answered, pdMS_TO_TICKS(2000));
    }
}

void Notifications::attach(NimBLEClient* c) {
    if (!cpLock) {
        cpLock = xSemaphoreCreateMutex();
        answered = xSemaphoreCreateBinary();
        pendingQueue = xQueueCreate(8, sizeof(Pending));
        readyQueue = xQueueCreate(4, sizeof(Notice));
        xTaskCreate(ancsWorker, "ancs", 4096, NULL, 1, NULL);
    }
    NimBLERemoteService* svc = c->getService(ANCS_SERVICE);
    NimBLERemoteCharacteristic* ns = svc ? svc->getCharacteristic(ANCS_NOTIFICATION_SOURCE) : nullptr;
    NimBLERemoteCharacteristic* ds = svc ? svc->getCharacteristic(ANCS_DATA_SOURCE) : nullptr;
    NimBLERemoteCharacteristic* cp = svc ? svc->getCharacteristic(ANCS_CONTROL_POINT) : nullptr;
    if (!ns || !ds || !cp) {
        Serial.println("[ancs] no se encontro el servicio de notificaciones del iPhone");
        return;
    }
    // Primero Data Source: así ninguna respuesta se pierde.
    if (!ds->subscribe(true, onDataSource) || !ns->subscribe(true, onNotificationSource)) {
        Serial.println("[ancs] el iPhone no dejo suscribirse (falta el permiso?)");
        return;
    }
    xSemaphoreTake(cpLock, portMAX_DELAY);
    controlPoint = cp;
    xSemaphoreGive(cpLock);
    Serial.println("[ancs] suscripto a las notificaciones");
}

// Sin tomar cpLock: se llama desde la desconexión (tarea del Bluetooth), y si otra tarea
// está esperando la respuesta de una escritura, esa respuesta la entrega justamente la
// tarea del Bluetooth: se trabarían las dos.
void Notifications::detach() {
    if (!cpLock) return;
    controlPoint = nullptr;
    callRinging = false;
    xQueueReset(pendingQueue);
}

bool Notifications::take(Notice& out) {
    return readyQueue && xQueueReceive(readyQueue, &out, 0) == pdTRUE;
}

bool Notifications::callActive() { return callRinging; }

void Notifications::rejectCall() {
    if (!cpLock || !callRinging) return;
    uint32_t uid = callUid;
    uint8_t cmd[] = { CMD_ACTION, (uint8_t)uid, (uint8_t)(uid >> 8), (uint8_t)(uid >> 16), (uint8_t)(uid >> 24),
                      ACTION_NEGATIVE };
    xSemaphoreTake(cpLock, portMAX_DELAY);
    NimBLERemoteCharacteristic* cp = controlPoint;
    if (cp) cp->writeValue(cmd, sizeof(cmd), true);
    xSemaphoreGive(cpLock);
    callRinging = false;
}
