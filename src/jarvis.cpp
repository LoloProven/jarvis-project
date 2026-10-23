#include "jarvis.h"
#include "config.h"
#include "audio_manager.h"
#include "text_ascii.h"
#include "wifi_portal.h"
#include "alarm_manager.h"
#include "step_counter.h"
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
static uint8_t* voice = nullptr;                       // voz de la respuesta, en la PSRAM
static const size_t MAX_VOICE_BYTES = 24000 * 2 * 40;  // 40 s a 24 kHz (1,9 MB)
static const size_t PREBUFFER = 24000 * 2 * 7 / 10;    // 0,7 s de voz antes de empezar a sonar
static const size_t PLAY_CHUNK = 4096;                 // ~85 ms por pedazo
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

// Alarmas del reloj para n8n: "07:00,08:30 (apagada)" o "ninguna".
static String alarmsText() {
    String s;
    for (int i = 0; i < AlarmManager::alarmCount; i++) {
        char buf[24];
        const AlarmItem& a = AlarmManager::alarmList[i];
        snprintf(buf, sizeof(buf), "%s%02d:%02d%s", i ? "," : "", a.hour, a.minute, a.active ? "" : " (apagada)");
        s += buf;
    }
    return s.length() ? s : String("ninguna");
}

// Órdenes que manda n8n cuando JARVIS usa una herramienta del reloj, separadas por ";":
// "ALARMA 07:00" (poner), "BORRAR 07:00", "TIMER 600" (segundos, arranca ya),
// "CRONO INICIAR" / "CRONO PAUSAR" / "CRONO RESET".
static void runActions(const String& actions) {
    int start = 0;
    while (start < (int)actions.length()) {
        int end = actions.indexOf(';', start);
        if (end < 0) end = actions.length();
        String a = actions.substring(start, end);
        a.trim();
        start = end + 1;
        int h, m;
        unsigned long secs;
        if (sscanf(a.c_str(), "ALARMA %d:%d", &h, &m) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) {
            if (AlarmManager::findAlarm(h, m) < 0) AlarmManager::addAlarm(h, m);
        } else if (sscanf(a.c_str(), "BORRAR %d:%d", &h, &m) == 2) {
            AlarmManager::deleteAlarm(AlarmManager::findAlarm(h, m));   // -1: no hace nada
        } else if (sscanf(a.c_str(), "TIMER %lu", &secs) == 1 && secs > 0 && secs <= 24UL * 3600) {
            AlarmManager::startTimer(secs);
        } else if (a == "CRONO INICIAR" && !AlarmManager::isChronoRunning()) {
            AlarmManager::toggleChrono();          // igual que el botón INICIAR: también la sesión
            StepCounter::sessionStart();
        } else if (a == "CRONO PAUSAR" && AlarmManager::isChronoRunning()) {
            AlarmManager::toggleChrono();
            StepCounter::sessionPause();
        } else if (a == "CRONO RESET") {
            AlarmManager::resetChrono();
            StepCounter::sessionReset();
        }
    }
}

Jarvis::Result Jarvis::ask(const char* profile, bool (*keepRecording)(),
                           void (*onStatus)(const char*), void (*onAnswer)()) {
    httpCode = 0;
    // Recién despierto el WiFi puede estar volviendo: se le dan unos segundos.
    if (WiFi.status() != WL_CONNECTED) {
        onStatus("Conectando WiFi...");
        WiFiPortal::connectNow();
        for (int i = 0; i < 75 && WiFi.status() != WL_CONNECTED; i++) delay(200);   // hasta 15 s
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
    http.addHeader("X-Alarmas", alarmsText());         // para "apagá la alarma de las 7"
    const char* keys[] = { "X-Pregunta", "X-Respuesta", "X-Accion" };
    http.collectHeaders(keys, 3);
    unsigned long tSent = millis();                    // para medir dónde se va el tiempo
    httpCode = http.POST(wav, WAV_HEADER + samples * 2);
    unsigned long tAnswer = millis();                  // subir el audio + n8n completo
    if (httpCode != 200) {
        Serial.printf("[jarvis] error HTTP %d\n", httpCode);
        http.end();
        return SERVER_ERROR;
    }
    copyHeader(http, "X-Pregunta", questionText, sizeof(questionText));
    copyHeader(http, "X-Respuesta", answerText, sizeof(answerText));
    String actions = http.header("X-Accion");
    Serial.printf("[jarvis] \"%s\" -> \"%s\" [%s]\n", questionText, answerText, actions.c_str());
    runActions(actions);
    onAnswer();

    // La voz llega en el cuerpo, con "colchón": se empieza a reproducir apenas hay PREBUFFER
    // bytes (~0,7 s) y el resto se sigue bajando entre pedazo y pedazo mientras suena. Si la red
    // se atrasa y el colchón se vacía, se espera (antes se bajaba entera primero: más lento;
    // y reproducirla tal cual llegaba se trababa por los tirones del túnel).
    if (!voice) voice = (uint8_t*)ps_malloc(MAX_VOICE_BYTES);
    WiFiClient* stream = http.getStreamPtr();
    int remaining = http.getSize();                    // -1 si no se sabe el largo
    size_t got = 0, played = 0;
    bool done = !voice, playing = false;
    int stalls = 0;
    unsigned long lastData = millis(), firstSound = 0;
    while (!done || (playing && played + 1 < got)) {
        // 1) Bajar lo que haya llegado (sin esperar).
        if (!done) {
            size_t avail = stream->available();
            if (avail > 0) {
                size_t want = min(avail, MAX_VOICE_BYTES - got);
                if (remaining > 0) want = min(want, (size_t)remaining);
                int n = stream->read(voice + got, want);
                if (n > 0) {
                    lastData = millis();
                    got += n;
                    if (remaining > 0) remaining -= n;
                }
            }
            if (remaining == 0 || got >= MAX_VOICE_BYTES || millis() - lastData > 5000 ||
                (avail == 0 && !http.connected())) done = true;
        }
        // 2) Arrancar cuando el colchón está lleno (o ya llegó todo).
        if (!playing && (got >= PREBUFFER || done)) {
            AudioManager::beginPlayback();
            playing = true;
            firstSound = millis();
        }
        // 3) Reproducir un pedazo si hay; si el colchón se vació, esperar a la red.
        if (playing) {
            size_t ready = (got - played) & ~(size_t)1;
            if (ready >= PLAY_CHUNK || (done && ready > 0)) {
                size_t n = min(PLAY_CHUNK, ready);
                AudioManager::playPcm24k((const int16_t*)(voice + played), n / 2);
                played += n;
            } else if (!done) {
                if (ready == 0 && played > 0) stalls++;
                delay(2);
            }
        } else {
            delay(2);
        }
    }
    http.end();
    if (playing) AudioManager::endPlayback();
    unsigned long tEnd = millis();
    Serial.printf("[jarvis] tiempos: grabar->respuesta %lu ms, ->primera voz %lu ms, voz %u KB, trabas %d, total %lu ms\n",
                  tAnswer - tSent, firstSound ? firstSound - tSent : 0, (unsigned)(got / 1024), stalls, tEnd - tSent);
    return OK;
}

const char* Jarvis::question() { return questionText; }
const char* Jarvis::answer() { return answerText; }
int Jarvis::lastHttpCode() { return httpCode; }
