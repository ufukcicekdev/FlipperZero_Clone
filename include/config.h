#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>


enum AppState { STATE_MENU, STATE_APP };

struct MenuItem {
    String name;
    uint16_t color;
    const uint16_t* icon; // İkon verisi için işaretçi (NULL olabilir)
};

// SADECE İLAN EDİYORUZ (Değerleri burada verme!)
extern const MenuItem menuItems[];
extern const int totalItems;


// --- WIFI AYARLARI ---
#define WIFI_SSID "TurkTelekom_TPD54A_2.4GHz"
#define WIFI_PASS "Lsb3U3vCycK4"

#define SCREEN_ROTATION 2// Dikey mod
#define MENU_COLS 2       // Yan yana 2 ikon
#define ICON_WIDTH 100
#define ICON_HEIGHT 80

// --- TRACKBALL PİNLERİ ---
#define TR_UP    45
#define TR_DWN   47
#define TR_LFT   46
#define TR_RHT   7
#define TR_BTN   38
#define TRACKBALL_SENSITIVITY 150 // Ms cinsinden gecikme (Yüksek = Daha yavaş/Az hassas)

// --- SD KART PİNLERİ ---
#define SD_SCK  41
#define SD_MISO 42
#define SD_MOSI 2
#define SD_CS   21

// --- I2S (MAX98357A) PİNLERİ ---
// Bağlantılar:
// VIN  -> 5V (veya 3.3V)
// GND  -> GND
// LRC  -> 17 (I2S_WS)
// BCLK -> 18 (I2S_BCK)
// DIN  -> 16 (I2S_DOUT)
#define I2S_BCK  18
#define I2S_WS   17
#define I2S_DOUT 16

// --- NFC (PN532 I2C) PİNLERİ ---
#define NFC_SDA 8
#define NFC_SCL 9
#define NFC_IRQ -1 // IRQ baglamadiysaniz -1 yapin (I2C'de stabilite icin onerilir)
#define NFC_RST -1 // RST baglamadiysaniz -1 yapin

// --- POMODORO AYARLARI ---
#define POMODORO_STEP_MIN 1 // Yukari/Asagi tuslari ile artis miktari (Dakika)

#endif