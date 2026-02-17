#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "controls.h"
#include "menu.h"
#include "sd_browser.h"
#include "app_clock.h"
#include "app_music.h"
#include "app_sd.h"
#include "app_settings.h"
#include "app_photo.h"
#include "app_nfc.h"
#include "app_pomodoro.h"
#include "app_games.h"
#include "sound.h"
#include "settings.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

AppState currentState = STATE_MENU;
int menuScrollX = 0;
int activeAppIndex = -1;
int selectedMenuIndex = -1; // Trackball ile seçilen ikon
bool needsRedraw = true;

// SD Browser Değişkenleri
int sdScrollY = 0;
bool appLoaded = false; // sdLoaded yerine genel bir isim kullandık


void setup() {
    Serial.begin(115200);
    
    // SPI Bus'ı SD kart pinleriyle başlat (TFT ve SD paylaşıyorsa bu önemlidir)
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    tft.init();
    tft.setRotation(2); // Dikey Mod
    uint16_t cal[5] = {387, 3523, 298, 3474, 1}; // Varsayılan rotasyona (1) döndük
    tft.setTouch(cal);
    spr.createSprite(240, 320);
    Controls::init(); // Trackball pinlerini başlat
    initAudio();      // Ses sistemini başlat
    
    //Serial.println("Ses testi basliyor...");
    //playTone(440, 1000); // 1 saniye boyunca 440Hz (La) sesi çal
    
    initSD(); // SD Kartı başlat
    Serial.println("Sistem Hazir.");
}

void loop() {
    ControlResult res = Controls::update(&tft);

    if (currentState == STATE_MENU) {
        // --- TRACKBALL NAVİGASYON ---
        if (res.tbRight) {
            selectedMenuIndex++;
            if (selectedMenuIndex >= totalItems) selectedMenuIndex = 0;
            playSystemSound(SOUND_SCROLL);
            needsRedraw = true;
        }
        if (res.tbLeft) {
            selectedMenuIndex--;
            if (selectedMenuIndex < 0) selectedMenuIndex = totalItems - 1;
            playSystemSound(SOUND_SCROLL);
            needsRedraw = true;
        }
        if (res.tbDown) {
            selectedMenuIndex += 2; // Alt satıra geç
            if (selectedMenuIndex >= totalItems) selectedMenuIndex %= totalItems; // Başa sar
            playSystemSound(SOUND_SCROLL);
            needsRedraw = true;
        }
        if (res.tbUp) {
            selectedMenuIndex -= 2; // Üst satıra geç
            if (selectedMenuIndex < 0) selectedMenuIndex += totalItems; // Sona sar
            playSystemSound(SOUND_SCROLL);
            needsRedraw = true;
        }
        
        // Seçili ikon değiştiyse sayfayı ona göre kaydır
        if (needsRedraw && selectedMenuIndex != -1) {
            int page = selectedMenuIndex / 6;
            menuScrollX = page * 240;
        }

        // Trackball Tıklama veya Dokunmatik Tıklama
        bool enterApp = res.tbPress;

        if (res.dragging) {
            // Kaydırmayı kolaylaştırmak için hızı 1.5 katına çıkarıyoruz
            menuScrollX += (res.scrollX * 3) / 2;
            if (menuScrollX < 0) menuScrollX = 0;
            // Toplam sayfa sayısına göre maksimum kaydırma limiti
            int maxScroll = ((totalItems - 1) / 6) * 240;
            if (menuScrollX > maxScroll) menuScrollX = maxScroll;
            // Sürüklerken sürekli ses çıkmasın diye buraya ses koymuyoruz
            needsRedraw = true;
        }

        if (res.released) {
            // Parmağı kaldırınca en yakın sayfaya yapış
            int page = (menuScrollX + 120) / 240;
            menuScrollX = page * 240;
            needsRedraw = true;
        }

        if (res.clicked) {
            selectedMenuIndex = -1; // Dokunmatik kullanınca seçimi kaldır
            // Alt kısımdaki oklara tıklandı mı? (Alanı düzelttik: y > 285)
            if (res.y > 285) {
                if (res.x < 100) { // Sol Ok Alanı
                    menuScrollX -= 240;
                    if (menuScrollX < 0) menuScrollX = 0;
                    playSystemSound(SOUND_SCROLL);
                    needsRedraw = true;
                } else if (res.x > 140) { // Sağ Ok Alanı
                    int maxScroll = ((totalItems - 1) / 6) * 240;
                    menuScrollX += 240;
                    if (menuScrollX > maxScroll) menuScrollX = maxScroll;
                    playSystemSound(SOUND_SCROLL);
                    needsRedraw = true;
                }
            } else {
                // Menü ikonlarına tıklama
                int idx = getMenuIndex(res.x, res.y, menuScrollX); 
                if (idx != -1) {
                    selectedMenuIndex = idx;
                    enterApp = true;
                }
            }
        }

        if (enterApp && selectedMenuIndex != -1) {
            activeAppIndex = selectedMenuIndex;
            currentState = STATE_APP;
            playSystemSound(SOUND_ENTER);
            needsRedraw = true;
            delay(500); // Gecikmeyi artırdık: Trackball'dan parmağınızı çekmeniz için zaman tanır
        }

        if (needsRedraw) {
            drawMenu(&spr, menuScrollX, selectedMenuIndex);
            spr.pushSprite(0, 0);
            needsRedraw = false;
        }
    } 
    else if (currentState == STATE_APP) {
        // SD CARD Uygulaması Özel Durumu
        if (menuItems[activeAppIndex].name == "SD KART") {
            updateSDApp(&spr, res, appLoaded, needsRedraw, currentState, sdScrollY);
        } 
        // MUSIC Uygulaması Özel Durumu
        else if (menuItems[activeAppIndex].name == "MUZIK") {
            updateMusicApp(&spr, res, appLoaded, needsRedraw, currentState, sdScrollY);
        } 
        // PHOTO Uygulaması
        else if (menuItems[activeAppIndex].name == "FOTO") {
            updatePhotoApp(&spr, res, appLoaded, needsRedraw, currentState, sdScrollY);
        }
        // NFC Uygulaması
        else if (menuItems[activeAppIndex].name == "NFC") {
            updateNFCApp(&spr, res, appLoaded, needsRedraw, currentState, sdScrollY);
        }
        // CLOCK Uygulaması
        else if (menuItems[activeAppIndex].name == "SAAT") {
            updateClockApp(&spr, res, appLoaded, needsRedraw, currentState);
        }
        // AYARLAR Uygulaması
        else if (menuItems[activeAppIndex].name == "AYARLAR") {
            updateSettingsApp(&spr, res, appLoaded, needsRedraw, currentState);
        }
        // POMODORO Uygulaması
        else if (menuItems[activeAppIndex].name == "POMODORO") {
            updatePomodoroApp(&spr, res, appLoaded, needsRedraw, currentState);
        }
        // OYUNLAR Uygulaması
        else if (menuItems[activeAppIndex].name == "OYUNLAR") {
            updateGamesApp(&spr, res, appLoaded, needsRedraw, currentState);
        }
        else {
            // Diğer Uygulamalar (Standart Ekran)
            if (needsRedraw) {
                bool isPressed = (res.dragging && res.y > 250);
                drawAppScreen(&spr, menuItems[activeAppIndex].name, menuItems[activeAppIndex].color, "Yapim Asamasinda", isPressed);
                spr.pushSprite(0, 0);
                needsRedraw = false;
            }
            if (res.clicked && res.y > 250) { 
                playSystemSound(SOUND_BACK);
                currentState = STATE_MENU;
                needsRedraw = true;
                delay(200);
            }
            // Diğer uygulamalar için Trackball ile çıkış (Artık buraya taşıdık)
            if (res.tbPress) {
                playSystemSound(SOUND_BACK);
                currentState = STATE_MENU;
                needsRedraw = true;
                delay(200);
            }
        }
    }
}