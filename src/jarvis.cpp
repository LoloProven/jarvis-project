#include "jarvis.h"
#include "config.h"
#include "audio_manager.h"
#include "text_ascii.h"
#include "wifi_portal.h"
#include "alarm_manager.h"
#include "step_counter.h"
#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClientSecure.h>

// --- CÓMO VA UNA PREGUNTA (pensado para que responda lo antes posible) ---
// Hay dos hilos que trabajan a la vez y nunca se esperan entre sí:
//  - Este (el loop): graba mientras BOOT está apretado y después hace sonar la voz.
//  - netTask (otra tarea): apenas se aprieta BOOT abre la conexión cifrada con n8n (por el
//    túnel de ngrok) y SUBE LA VOZ MIENTRAS HABLÁS, en pedazos (HTTP "chunked"). Al soltar
//    BOOT solo falta el último pedazo. Después baja la respuesta a la PSRAM.
// Antes se grababa todo, recién ahí se conectaba y subía (~8 s por el hotspot) y la misma
// tarea bajaba y reproducía (se trababa cuando se ponía a descifrar un pedazo grande).
// El WAV sale con el largo en 0xFFFFFFFF (no se sabe al empezar): n8n lo corrige
// ("Preparar audio") antes de mandarlo a Groq.

static const int RATE = 16000;
static const int MAX_SECONDS = 20;                    // 640 KB en la PSRAM
static const size_t MAX_SAMPLES = RATE * MAX_SECONDS;
static const size_t MIN_SAMPLES = RATE / 2;          // menos de medio segundo: se descarta
static const size_t WAV_HEADER = 44;

static uint8_t* wav = nullptr;                         // encabezado WAV + audio, en la PSRAM
static uint8_t* voice = nullptr;                       // voz de la respuesta, en la PSRAM
static const size_t MAX_VOICE_BYTES = 24000 * 2 * 40;  // 40 s a 24 kHz (1,9 MB)
static const size_t PREBUFFER = 24000 * 2 * 4 / 10;    // 0,4 s de voz antes de empezar a sonar
static const size_t PLAY_CHUNK = 2048;                 // ~43 ms por pedazo
static char questionText[160] = "";
static char answerText[400] = "";
static int httpCode = 0;

// Estado compartido entre el loop y netTask.
enum NetPhase { NET_CONNECTING, NET_UPLOADING, NET_WAITING, NET_HEADERS, NET_DONE, NET_FAILED };
static volatile NetPhase phase;
static volatile size_t recordedSamples;                // cuánto grabó el loop hasta ahora
static volatile bool recordingDone, abortUpload;
static volatile size_t voiceGot;                       // cuánto de la voz ya bajó
static String actionsText;
static char profileName[24];
static String alarmsHeader;
static TaskHandle_t netTaskHandle = nullptr;

static void put32(uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void put16(uint8_t* p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

// Encabezado de un WAV PCM mono de 16 bits: sin esto el audio llega como ruido sin formato.
static void writeWavHeader(uint8_t* h, uint32_t dataBytes) {
    memcpy(h, "RIFF", 4);      put32(h + 4, dataBytes == 0xFFFFFFFF ? dataBytes : 36 + dataBytes);
    memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16);         put16(h + 20, 1);       put16(h + 22, 1);   // PCM, mono
    put32(h + 24, RATE);       put32(h + 28, RATE * 2);
    put16(h + 32, 2);          put16(h + 34, 16);
    memcpy(h + 36, "data", 4); put32(h + 40, dataBytes);
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

// --- RED (netTask) ---
static bool writeAll(NetworkClientSecure& c, const uint8_t* p, size_t n) {
    while (n > 0) {
        int w = c.write(p, n);
        if (w <= 0) return false;
        p += w;
        n -= w;
    }
    return true;
}

static bool sendChunk(NetworkClientSecure& c, const uint8_t* p, size_t n) {
    char head[12];
    int h = snprintf(head, sizeof(head), "%X\r\n", (unsigned)n);
    return writeAll(c, (const uint8_t*)head, h) && (n == 0 || writeAll(c, p, n)) &&
           writeAll(c, (const uint8_t*)"\r\n", 2);
}

// Lee una línea (sin el \r\n) con tiempo máximo.
static bool readLine(NetworkClientSecure& c, String& line, unsigned long timeoutMs) {
    line = "";
    unsigned long t0 = millis();
    while (millis() - t0 < timeoutMs) {
        if (!c.available()) {
            if (!c.connected()) return false;
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        char ch = c.read();
        if (ch == '\n') return true;
        if (ch != '\r') line += ch;
        if (line.length() > 1500) return false;
    }
    return false;
}

static void copyAscii(const String& v, char* out, size_t outLen) {
    toAscii((const uint8_t*)v.c_str(), v.length(), out, outLen);
}

static void netRun() {
    // "https://host/ruta" -> host y ruta.
    String url = JARVIS_URL;
    int hostStart = url.indexOf("://") + 3;
    int pathStart = url.indexOf('/', hostStart);
    String host = url.substring(hostStart, pathStart);
    String path = url.substring(pathStart);

    NetworkClientSecure c;
    c.setInsecure();               // no se verifica el certificado: lo protege la clave
    c.setTimeout(15);
    bool connected = c.connect(host.c_str(), 443);
    if (!connected) {
        char err[100];
        c.lastError(err, sizeof(err));
        Serial.printf("[jarvis] fallo la conexion (%s), heap libre %u; reintento\n", err,
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        vTaskDelay(pdMS_TO_TICKS(300));
        connected = c.connect(host.c_str(), 443);
    }
    if (!connected) {
        char err[100];
        c.lastError(err, sizeof(err));
        Serial.printf("[jarvis] no pude conectar con el tunel (%s)\n", err);
        httpCode = -1;
        phase = NET_FAILED;
        return;
    }
    String req = "POST " + path + " HTTP/1.1\r\nHost: " + host + "\r\n"
                 "Content-Type: audio/wav\r\nTransfer-Encoding: chunked\r\n"
                 "X-Perfil: " + String(profileName) + "\r\n"
                 "X-Jarvis-Clave: " + String(JARVIS_KEY) + "\r\n"
                 "X-Alarmas: " + alarmsHeader + "\r\n"
                 "ngrok-skip-browser-warning: 1\r\nConnection: close\r\n\r\n";
    bool ok = writeAll(c, (const uint8_t*)req.c_str(), req.length()) && sendChunk(c, wav, WAV_HEADER);
    phase = NET_UPLOADING;

    // Subir lo grabado a medida que llega (de a ~0,25 s).
    size_t sent = 0;
    while (ok && !abortUpload) {
        size_t have = recordedSamples;
        if (have > sent && (have - sent >= RATE / 4 || recordingDone)) {
            ok = sendChunk(c, wav + WAV_HEADER + sent * 2, (have - sent) * 2);
            sent = have;
        } else if (recordingDone) {
            break;
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    if (abortUpload || !ok) {
        c.stop();
        httpCode = ok ? 0 : -1;
        phase = NET_FAILED;
        return;
    }
    ok = sendChunk(c, nullptr, 0);                     // fin del cuerpo
    phase = NET_WAITING;

    // Respuesta: línea de estado y encabezados (n8n tarda unos segundos en mandarlos).
    String line;
    long contentLength = -1;
    bool chunked = false;
    if (!ok || !readLine(c, line, 45000) || line.length() < 12) {
        httpCode = -1;
        c.stop();
        phase = NET_FAILED;
        return;
    }
    httpCode = line.substring(9, 12).toInt();
    while (readLine(c, line, 5000) && line.length() > 0) {
        int colon = line.indexOf(':');
        if (colon < 0) continue;
        String name = line.substring(0, colon);
        name.toLowerCase();
        String value = line.substring(colon + 1);
        value.trim();
        if (name == "x-pregunta") copyAscii(value, questionText, sizeof(questionText));
        else if (name == "x-respuesta") copyAscii(value, answerText, sizeof(answerText));
        else if (name == "x-accion") actionsText = value;
        else if (name == "content-length") contentLength = value.toInt();
        else if (name == "transfer-encoding" && value.indexOf("chunked") >= 0) chunked = true;
    }
    if (httpCode != 200) {
        c.stop();
        phase = NET_FAILED;
        return;
    }
    phase = NET_HEADERS;

    // Cuerpo = voz (PCM mono 24 kHz). Puede venir con largo fijo o "chunked".
    size_t got = 0;
    long chunkLeft = chunked ? 0 : contentLength;      // -1 = hasta que cierre
    unsigned long lastData = millis();
    while (got < MAX_VOICE_BYTES && millis() - lastData < 5000) {
        if (chunked && chunkLeft == 0) {
            if (got > 0) readLine(c, line, 2000);      // el \r\n que cierra el pedazo anterior
            if (!readLine(c, line, 5000)) break;
            chunkLeft = strtol(line.c_str(), nullptr, 16);
            if (chunkLeft == 0) break;                 // último pedazo
        }
        if (!chunked && chunkLeft == 0) break;
        size_t avail = c.available();
        if (avail == 0) {
            if (!c.connected()) break;
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        size_t want = min(avail, MAX_VOICE_BYTES - got);
        if (chunkLeft > 0) want = min(want, (size_t)chunkLeft);
        int n = c.read(voice + got, want);
        if (n <= 0) continue;
        lastData = millis();
        got += n;
        voiceGot = got;
        if (chunkLeft > 0) chunkLeft -= n;
    }
    c.stop();
    phase = NET_DONE;
}

// La tarea solo llama a netRun() y se borra. Así, al salir de netRun() (por cualquier
// return) se liberan sus objetos (Strings, la conexión TLS). Antes netRun era la tarea misma
// y terminaba con vTaskDelete() en el medio: los objetos nunca se liberaban y la memoria
// interna se iba fragmentando, hasta que TLS no podía conectar ("SSL - Memory allocation
// failed", con ~48 KB libres).
static void netTask(void*) {
    netRun();
    vTaskDelete(NULL);
}

Jarvis::Result Jarvis::ask(const char* profile, bool (*keepRecording)(),
                           void (*onStatus)(const char*), void (*onAnswer)()) {
    httpCode = 0;
    // Recién despierto el WiFi puede estar volviendo, o esperando un rato largo entre intentos
    // (sin red conocida): se le pide que pruebe ya y se le dan unos segundos.
    if (WiFi.status() != WL_CONNECTED) {
        onStatus("Conectando WiFi...");
        WiFiPortal::connectNow();
        for (int i = 0; i < 75 && WiFi.status() != WL_CONNECTED; i++) delay(200);   // hasta 15 s
        if (WiFi.status() != WL_CONNECTED) return NO_WIFI;
    }
    if (!wav) wav = (uint8_t*)ps_malloc(WAV_HEADER + MAX_SAMPLES * 2);
    if (!voice) voice = (uint8_t*)ps_malloc(MAX_VOICE_BYTES);
    if (!wav || !voice) return SERVER_ERROR;

    // Memoria interna (la que usa TLS): si baja pregunta a pregunta, hay una pérdida.
    Serial.printf("[jarvis] memoria interna libre %u, bloque mas grande %u\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    // Arranca la red en paralelo (se conecta mientras empezás a hablar).
    writeWavHeader(wav, 0xFFFFFFFF);
    strlcpy(profileName, profile, sizeof(profileName));
    alarmsHeader = alarmsText();
    questionText[0] = answerText[0] = 0;
    actionsText = "";
    recordedSamples = 0;
    voiceGot = 0;
    recordingDone = abortUpload = false;
    phase = NET_CONNECTING;
    xTaskCreatePinnedToCore(netTask, "jarvisNet", 12288, NULL, 2, &netTaskHandle, 0);

    onStatus("Escuchando...");
    unsigned long tStart = millis();
    size_t samples = AudioManager::record((int16_t*)(wav + WAV_HEADER), MAX_SAMPLES, keepRecording,
                                          &recordedSamples);
    unsigned long tReleased = millis();
    if (samples < MIN_SAMPLES) abortUpload = true;
    recordingDone = true;
    if (abortUpload) {
        while (phase != NET_FAILED && phase != NET_DONE) delay(10);
        return TOO_SHORT;
    }

    onStatus("Pensando...");
    while (phase != NET_HEADERS && phase != NET_DONE && phase != NET_FAILED) delay(10);
    unsigned long tAnswer = millis();
    if (phase == NET_FAILED || httpCode != 200) {
        Serial.printf("[jarvis] error HTTP %d\n", httpCode);
        while (phase != NET_FAILED && phase != NET_DONE) delay(10);
        return SERVER_ERROR;
    }
    Serial.printf("[jarvis] \"%s\" -> \"%s\" [%s]\n", questionText, answerText, actionsText.c_str());
    runActions(actionsText);
    onAnswer();

    // Voz: arranca con un colchón chico y sigue mientras netTask baja el resto.
    size_t played = 0;
    bool playing = false;
    int stalls = 0;
    unsigned long firstSound = 0;
    for (;;) {
        bool done = (phase == NET_DONE || phase == NET_FAILED);
        size_t got = voiceGot;
        if (!playing && (got >= PREBUFFER || (done && got > 0))) {
            AudioManager::beginPlayback();
            playing = true;
            firstSound = millis();
        }
        if (playing) {
            size_t ready = (got - played) & ~(size_t)1;
            if (ready >= PLAY_CHUNK || (done && ready > 0)) {
                size_t n = min(PLAY_CHUNK, ready);
                AudioManager::playPcm24k((const int16_t*)(voice + played), n / 2);
                played += n;
                continue;
            }
            if (done) break;
            if (ready == 0) stalls++;
        } else if (done) {
            break;
        }
        delay(3);
    }
    if (playing) AudioManager::endPlayback();
    Serial.printf("[jarvis] tiempos: hablaste %lu ms; al soltar BOOT -> respuesta %lu ms, -> primera voz %lu ms; voz %u KB, trabas %d\n",
                  tReleased - tStart, tAnswer - tReleased, firstSound ? firstSound - tReleased : 0,
                  (unsigned)(voiceGot / 1024), stalls);
    return OK;
}

const char* Jarvis::question() { return questionText; }
const char* Jarvis::answer() { return answerText; }
int Jarvis::lastHttpCode() { return httpCode; }
