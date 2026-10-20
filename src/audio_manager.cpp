#include "audio_manager.h"
#include "config.h"
#include <driver/i2s.h>
#include <HTTPClient.h>
#include <WiFi.h>

#define I2S_MIC_PORT I2S_NUM_0
#define I2S_SPK_PORT I2S_NUM_1
#define RECORD_TIME_SEC 4
#define SAMPLE_RATE 16000
#define BUFFER_SIZE (SAMPLE_RATE * RECORD_TIME_SEC * 2)

int AudioManager::masterVolume = 10;
bool AudioManager::isRecording = false;
static uint8_t* audioBuffer = NULL;

// --- CAMBIA 192.168.X.X POR TU IP LOCAL ---
const char* n8n_audio_webhook = "http://192.168.0.57:5678/webhook/upload-audio";

void AudioManager::init() {
    gpio_hold_dis((gpio_num_t)I2S_SPK_DOUT);
    pinMode(I2S_SPK_DOUT, OUTPUT);

    i2s_config_t i2s_mic_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false
    };
    i2s_pin_config_t mic_pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = I2S_MIC_CLK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_DATA
    };
    i2s_driver_install(I2S_MIC_PORT, &i2s_mic_config, 0, NULL);
    i2s_set_pin(I2S_MIC_PORT, &mic_pin_config);

    i2s_config_t i2s_spk_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 6,
        .dma_buf_len = 256,
        .use_apll = false
    };
    i2s_pin_config_t spk_pin_config = {
        .bck_io_num = I2S_SPK_BCLK,
        .ws_io_num = I2S_SPK_LRCK,
        .data_out_num = I2S_SPK_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_driver_install(I2S_SPK_PORT, &i2s_spk_config, 0, NULL);
    i2s_set_pin(I2S_SPK_PORT, &spk_pin_config);
}

void AudioManager::playTone(int volume) {
    const int pcmLen = 512;
    int16_t toneBuf[pcmLen];
    
    int16_t maxAmp = 32700 * (volume / 10.0f);

    for (int i = 0; i < pcmLen; i += 2) {
        int16_t sample = ((i / 32) % 2 == 0) ? maxAmp : -maxAmp;
        toneBuf[i]     = sample;
        toneBuf[i + 1] = sample;
    }
    
    size_t bytesWritten;
    i2s_write(I2S_SPK_PORT, toneBuf, sizeof(toneBuf), &bytesWritten, portMAX_DELAY);
}

void AudioManager::stopTone() {
    i2s_zero_dma_buffer(I2S_SPK_PORT);
}

void AudioManager::recordAndSend() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (audioBuffer == NULL) {
        audioBuffer = (uint8_t*) malloc(BUFFER_SIZE);
    }
    if (audioBuffer == NULL) return;

    isRecording = true;
    size_t bytesRead = 0;
    i2s_read(I2S_MIC_PORT, audioBuffer, BUFFER_SIZE, &bytesRead, portMAX_DELAY);
    isRecording = false;

    HTTPClient http;
    http.begin(n8n_audio_webhook);
    http.addHeader("Content-Type", "audio/wav");
    http.POST(audioBuffer, bytesRead);
    http.end();
}