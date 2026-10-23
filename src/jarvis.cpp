#include "jarvis.h"
#include "config.h"
#include "audio_manager.h"
#include "text_ascii.h"
#include "wifi_portal.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

static const int RATE = 16000;
static const int MAX_SECONDS = 20;                    // 640 KB en la PSRAM
static const size_t MAX_SAMPLES = RATE * MAX_SECONDS;
static const size_t MIN_SAMPLES = RATE / 2;          // menos de medio segundo: se descarta
static const size_t WAV_HEADER = 44;

static uint8_t* wav = nullptr;                         // encabezado WAV + audio, en la PSRAM
static char questionText[160] = "";
static char answerText[400] = "";
static int httpCode = 0;

static void put32(uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void put16(uint8_t* p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

// Encabezado de un WAV PCM mono de 16 bits: sin esto el audio llega como ruido sin formato.
static void writeWavHeader(uint8_t* h, size_t samples) {
    uint32_t dataBytes = samples * 2;
    memcpy(h, "RIFF", 4);      put32(h + 4, 36 + dataBytes);
    memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16);         put16(h + 20, 1);       put16(h + 22, 1);   // PCM, mono
    put32(h + 24, RATE);       put32(h + 28, RATE * 2);
    put16(h + 32, 2);          put16(h + 34, 16);
    memcpy(h + 36, "data", 4); put32(h + 40, dataBytes);
}

static void copyHeader(HTTPClient& http, const char* name, char* out, size_t outLen) {
    String v = http.header(name);
    toAscii((const uint8_t*)v.c_str(), v.length(), out, outLen);
}

Jarvis::Result Jarvis::ask(const char* profile, bool (*keepRecording)(),
                           void (*onStatus)(const char*), void (*onAnswer)()) {
    httpCode = 0;
    // Recién despierto el WiFi puede estar volviendo: se le dan unos segundos.
    if (WiFi.status() != WL_CONNECTED) {
        onStatus("Conectando WiFi...");
        WiFiPortal::resume();
        for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(200);
        if (WiFi.status() != WL_CONNECTED) return NO_WIFI;
    }
    if (!wav) wav = (uint8_t*)ps_malloc(WAV_HEADER + MAX_SAMPLES * 2);
    if (!wav) return SERVER_ERROR;

    onStatus("Escuchando...");
    size_t samples = AudioManager::record((int16_t*)(wav + WAV_HEADER), MAX_SAMPLES, keepRecording);
    if (samples < MIN_SAMPLES) return TOO_SHORT;
    writeWavHeader(wav, samples);

    onStatus("Pensando...");
    // n8n se publica en internet con un túnel de ngrok (solo HTTPS): así anda también con los
    // datos del celular. No se verifica el certificado (alcanza con cifrar); quien protege a
    // JARVIS es la clave X-Jarvis-Clave, sin ella n8n rechaza el pedido.
    NetworkClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    http.begin(tls, JARVIS_URL);
    http.setConnectTimeout(8000);
    http.setTimeout(45000);                            // la IA y la voz tardan unos segundos
    http.addHeader("Content-Type", "audio/wav");
    http.addHeader("X-Perfil", profile);
    http.addHeader("X-Jarvis-Clave", JARVIS_KEY);
    http.addHeader("ngrok-skip-browser-warning", "1"); // si no, ngrok gratis puede mandar su aviso
    const char* keys[] = { "X-Pregunta", "X-Respuesta" };
    http.collectHeaders(keys, 2);
    httpCode = http.POST(wav, WAV_HEADER + samples * 2);
    if (httpCode != 200) {
        Serial.printf("[jarvis] error HTTP %d\n", httpCode);
        http.end();
        return SERVER_ERROR;
    }
    copyHeader(http, "X-Pregunta", questionText, sizeof(questionText));
    copyHeader(http, "X-Respuesta", answerText, sizeof(answerText));
    Serial.printf("[jarvis] \"%s\" -> \"%s\"\n", questionText, answerText);
    onAnswer();

    // La voz llega en el cuerpo: se reproduce a medida que llega.
    WiFiClient* stream = http.getStreamPtr();
    int remaining = http.getSize();                    // -1 si no se sabe el largo
    uint8_t buf[1024 + 1];
    size_t have = 0;                                   // un byte suelto que quedó de antes
    unsigned long lastData = millis();
    AudioManager::beginPlayback();
    while (remaining != 0 && millis() - lastData < 5000) {
        size_t avail = stream->available();
        if (avail == 0) {
            if (!http.connected()) break;
            delay(2);
            continue;
        }
        size_t want = min(avail, sizeof(buf) - 1 - have);
        if (remaining > 0) want = min(want, (size_t)remaining);
        int n = stream->read(buf + have, want);
        if (n <= 0) continue;
        lastData = millis();
        if (remaining > 0) remaining -= n;
        size_t total = have + n;
        AudioManager::playPcm24k((const int16_t*)buf, total / 2);
        have = total % 2;
        if (have) buf[0] = buf[total - 1];
    }
    AudioManager::endPlayback();
    http.end();
    return OK;
}

const char* Jarvis::question() { return questionText; }
const char* Jarvis::answer() { return answerText; }
int Jarvis::lastHttpCode() { return httpCode; }
