#include "sound.h"
#include "driver/i2s.h"
#include "config.h"
#include "settings.h"
#include <math.h>

#define SAMPLE_RATE 44100
#define I2S_NUM I2S_NUM_0

static bool i2s_installed = false;

void initAudio() {
    if (i2s_installed) return; // Zaten açıksa işlem yapma

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256, // Tamponu büyüttük, daha stabil ses için
        .use_apll = false,  // S3'te kararlılık için APLL'i kapattık
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM); // Başlangıçta gürültüyü önlemek için buffer'ı temizle
    i2s_installed = true;
}

void deinitAudio() {
    if (!i2s_installed) return; // Zaten kapalıysa işlem yapma
    i2s_driver_uninstall(I2S_NUM);
    i2s_installed = false;
}

void playTone(int freq, int durationMs) {
    if (!settings_soundEnabled) {
        Serial.println("Ses kapali (Ayarlardan acin)");
        return;
    }
    
    size_t bytes_written;
    int samples = (SAMPLE_RATE * durationMs) / 1000;
    // Küçük bir buffer ile ses üretimi
    int16_t *buffer = (int16_t *)malloc(samples * 2 * sizeof(int16_t));
    if (!buffer) {
        Serial.println("Ses icin bellek ayrilamadi!");
        return;
    }
    
    // Ses seviyesi ayarı (Logaritmik yerine basit lineer ölçekleme)
    double volume = (double)settings_soundVolume / 10.0; 
    int16_t amplitude = 15000 * volume; // Sesi duyulabilir yapmak için genliği 5 katına çıkardık

    for (int i = 0; i < samples; i++) {
        double t = (double)i / SAMPLE_RATE;
        int16_t val = (int16_t)(sin(2 * PI * freq * t) * amplitude);
        buffer[i * 2] = val;     // Sol Kanal
        buffer[i * 2 + 1] = val; // Sağ Kanal
    }

    // I2S'e yaz (Bloklayıcı işlem, ama süreler kısa olduğu için sorun olmaz)
    i2s_write(I2S_NUM, buffer, samples * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    // Serial.printf("Ses calindi: %d Hz, %d ms\n", freq, durationMs); // Log kirliliğini önlemek için kapatıldı
    free(buffer);
    
    // Ses bitiminde çıt sesini önlemek için kısa bir sessizlik
    // i2s_zero_dma_buffer(I2S_NUM); // Gerekirse açılabilir
}

void playSystemSound(SystemSound snd) {
    if (!settings_soundEnabled) return;

    switch (settings_soundType) {
        case 0: // Default (Sade Bip)
            if (snd == SOUND_SCROLL) playTone(400, 15);
            if (snd == SOUND_CLICK) playTone(800, 40);
            if (snd == SOUND_ENTER) playTone(1000, 60);
            if (snd == SOUND_BACK) playTone(300, 40);
            break;
        case 1: // Retro (8-bit Oyun)
            if (snd == SOUND_SCROLL) playTone(150, 20);
            if (snd == SOUND_CLICK) { playTone(1200, 30); }
            if (snd == SOUND_ENTER) { playTone(800, 40); playTone(1200, 60); }
            if (snd == SOUND_BACK) { playTone(300, 50); playTone(150, 50); }
            break;
        case 2: // Sci-Fi (Fütüristik)
            if (snd == SOUND_SCROLL) playTone(2000, 10);
            if (snd == SOUND_CLICK) playTone(2500, 30);
            if (snd == SOUND_ENTER) { playTone(1500, 30); playTone(3000, 50); }
            if (snd == SOUND_BACK) playTone(500, 60);
            break;
    }
}