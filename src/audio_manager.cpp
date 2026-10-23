#include "audio_manager.h"
#include "config.h"
#include <ESP_I2S.h>
#include <Wire.h>
#include "es8311.h"

// Audio por el códec ES8311, igual que el ejemplo 08_ES8311 de Waveshare:
// un solo bus I2S estándar (parlante y micrófono) y el códec configurado por I2C.
#define SAMPLE_RATE 16000
#define CODEC_VOLUME 85                 // 0-100, el mismo que usa el ejemplo
#define MIC_GAIN (es8311_mic_gain_t)3   // 0-7

int AudioManager::masterVolume = 10;
bool AudioManager::isRecording = false;
static I2SClass i2s;
static bool audioReady = false;

static bool codecInit() {
    // Usa el I2C 0 (Wire), que ya inició PowerManager::init() en los pines 15/14.
    es8311_handle_t es = es8311_create(0, ES8311_ADDR);
    if (!es) return false;
    const es8311_clock_config_t clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = SAMPLE_RATE * 256,
        .sample_frequency = SAMPLE_RATE
    };
    if (es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return false;
    es8311_sample_frequency_config(es, clk.mclk_frequency, clk.sample_frequency);
    es8311_microphone_config(es, false);
    es8311_voice_volume_set(es, CODEC_VOLUME, NULL);
    es8311_microphone_gain_set(es, MIC_GAIN);
    return true;
}

// --- MICRÓFONO: ES7210 ---
// En esta placa el micrófono NO pasa por el ES8311 (ese es solo el parlante): va al ES7210,
// que comparte el I2C y los relojes del I2S y entrega la voz por GPIO 42. Sin configurarlo
// se leían puros ceros. Secuencia del driver oficial de Espressif (esp-bsp/es7210), para
// 16 kHz con MCLK = 256 x 16 kHz, I2S de 16 bits y ganancia de 30 dB.
static bool es7210Write(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

static bool micInit() {
    uint8_t addr = 0;
    for (uint8_t a = 0x40; a <= 0x43 && !addr; a++) {       // según cómo estén A0/A1
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) addr = a;
    }
    if (!addr) return false;
    static const uint8_t SEQ[][2] = {
        {0x00, 0xFF}, {0x00, 0x32},                          // reset
        {0x09, 0x30}, {0x0A, 0x30},                          // tiempos de arranque
        {0x23, 0x2A}, {0x22, 0x0A}, {0x21, 0x2A}, {0x20, 0x0A},   // filtros pasa-altos
        {0x11, 0x60}, {0x12, 0x00},                          // I2S estándar, 16 bits, sin TDM
        {0x40, 0xC3},                                        // alimentación analógica
        {0x41, 0x70}, {0x42, 0x70},                          // polarización del micrófono 2,87 V
        {0x43, 0x1A}, {0x44, 0x1A}, {0x45, 0x1A}, {0x46, 0x1A},   // ganancia 30 dB
        {0x47, 0x08}, {0x48, 0x08}, {0x49, 0x08}, {0x4A, 0x08},   // prender MIC1-4
        {0x07, 0x20}, {0x02, 0xC1}, {0x04, 0x01}, {0x05, 0x00},   // 16 kHz con MCLK 4,096 MHz
        {0x06, 0x04},                                        // apagar el DLL
        {0x4B, 0x0F}, {0x4C, 0x0F},                          // polarización, ADC y PGA
        {0x00, 0x71}, {0x00, 0x41},                          // arrancar
    };
    for (auto& r : SEQ) {
        if (!es7210Write(addr, r[0], r[1])) return false;
    }
    Serial.printf("[audio] microfono ES7210 listo (0x%02X)\n", addr);
    return true;
}

void AudioManager::init() {
    // Amplificador apagado mientras no suena nada (evita ruido de fondo).
    pinMode(AUDIO_PA_EN, OUTPUT);
    digitalWrite(AUDIO_PA_EN, LOW);

    i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCLK);
    if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
        Serial.println("[audio] no se pudo iniciar I2S");
        return;
    }
    if (!codecInit()) {
        Serial.println("[audio] no se pudo iniciar el ES8311");
        return;
    }
    if (!micInit()) Serial.println("[audio] no se encontro el microfono (ES7210)");
    audioReady = true;
}

void AudioManager::playTone(int volume) {
    if (!audioReady) return;
    digitalWrite(AUDIO_PA_EN, HIGH);

    const int pcmLen = 512;              // 256 cuadros estéreo (L, R)
    int16_t toneBuf[pcmLen];
    int16_t maxAmp = 30000 * (volume / 10.0f);

    // Onda cuadrada de 500 Hz: cambia de signo cada 16 cuadros a 16 kHz.
    for (int i = 0; i < pcmLen; i += 2) {
        int16_t sample = ((i / 32) % 2 == 0) ? maxAmp : -maxAmp;
        toneBuf[i]     = sample;
        toneBuf[i + 1] = sample;
    }
    i2s.write((const uint8_t*)toneBuf, sizeof(toneBuf));
}

void AudioManager::stopTone() {
    if (!audioReady) return;
    // Un poco de silencio para vaciar lo que quedó en el buffer y después apagar el amplificador.
    int16_t silence[512] = {0};
    for (int i = 0; i < 4; i++) i2s.write((const uint8_t*)silence, sizeof(silence));
    digitalWrite(AUDIO_PA_EN, LOW);
}

size_t AudioManager::record(int16_t* out, size_t maxSamples, bool (*keepGoing)()) {
    if (!audioReady) return 0;
    // El I2S trae estéreo y el micrófono va por un solo canal: tomando solo el izquierdo
    // llegaban puros ceros. Se guarda, de cada cuadro, la muestra más fuerte de las dos
    // (con un canal mudo es exactamente el otro). Mono, 16 bits, 16 kHz.
    isRecording = true;
    int16_t chunk[512];
    i2s.readBytes((char*)chunk, sizeof(chunk));   // lo viejo del buffer (y el "clic" del botón)
    size_t outSamples = 0;
    uint32_t nonZeroL = 0, nonZeroR = 0;
    while (outSamples < maxSamples && keepGoing()) {
        size_t n = i2s.readBytes((char*)chunk, sizeof(chunk));
        if (n == 0) break;
        size_t frames = n / 4;
        for (size_t f = 0; f < frames && outSamples < maxSamples; f++) {
            int16_t l = chunk[f * 2], r = chunk[f * 2 + 1];
            if (l) nonZeroL++;
            if (r) nonZeroR++;
            out[outSamples++] = (abs(l) >= abs(r)) ? l : r;
        }
    }
    isRecording = false;
    Serial.printf("[audio] grabadas %u muestras (con sonido: izq %u, der %u)\n",
                  (unsigned)outSamples, (unsigned)nonZeroL, (unsigned)nonZeroR);
    return outSamples;
}

// Voz que llega de n8n: mono a 24 kHz. El I2S anda a 16 kHz, así que de cada 3 muestras
// salen 2 (la primera y el promedio de las otras dos), en estéreo y al volumen de la alarma.
static int16_t group[3];                   // grupo de 3 muestras en armado (entre llamadas)
static int groupCount = 0;

void AudioManager::beginPlayback() {
    groupCount = 0;
    if (audioReady) digitalWrite(AUDIO_PA_EN, HIGH);
}

void AudioManager::playPcm24k(const int16_t* in, size_t n) {
    if (!audioReady) return;
    int16_t out[256];                      // 128 cuadros estéreo
    size_t o = 0;
    float gain = masterVolume / 10.0f;
    for (size_t i = 0; i < n; i++) {
        group[groupCount++] = in[i];
        if (groupCount < 3) continue;
        groupCount = 0;
        int16_t a = group[0] * gain;
        int16_t b = ((group[1] + group[2]) / 2) * gain;
        out[o++] = a; out[o++] = a;        // izquierdo y derecho
        out[o++] = b; out[o++] = b;
        if (o == 256) {
            i2s.write((const uint8_t*)out, sizeof(out));
            o = 0;
        }
    }
    if (o > 0) i2s.write((const uint8_t*)out, o * sizeof(int16_t));
}

void AudioManager::endPlayback() {
    stopTone();                            // silencio y apaga el amplificador
}
