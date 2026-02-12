#include "app_music.h"
#include "sd_browser.h"
#include "menu.h"
#include "config.h"
#include "settings.h"
#include <SD.h>
#include "Audio.h" // GEREKLİ KÜTÜPHANE: ESP32-audioI2S (Schreibfaul1)
#include <WiFi.h>  // WiFi fonksiyonları için
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <semphr.h> // Mutex için gerekli

// sound.cpp'den gelen fonksiyonlar
extern void deinitAudio();
extern void initAudio();

static Audio* audio = nullptr; // İşaretçi olarak tanımla
static int selectedFileIndex = 0;
static int currentVolume = 10;
static String playingFile = "";
static volatile bool isPlaying = false; // Çekirdekler arası erişim için volatile
static bool songFinished = false;

// Ses işlemi için Görev Tutucusu
TaskHandle_t audioTaskHandle = NULL;
SemaphoreHandle_t audioMutex = NULL; // Erişim güvenliği için Mutex

// Şarkı bittiğinde bu fonksiyon otomatik çağrılır (Audio kütüphanesi özelliği)
void audio_eof_mp3(const char* info) {
    (void)info;
    songFinished = true; // Ana döngüde bir sonraki şarkıya geçmesi için bayrağı kaldır
}

// --- SES GÖREVİ (Core 0 üzerinde çalışacak) ---
void audioTask(void *parameter) {
    while (true) {
        // Mutex ile güvenli erişim
        if (audioMutex && xSemaphoreTake(audioMutex, portMAX_DELAY)) {
            if (audio && isPlaying) {
                audio->loop();
            }
            xSemaphoreGive(audioMutex);
        }
        // Eğer çalmıyorsa işlemciyi yormamak için daha uzun bekle
        if (!isPlaying) {
            vTaskDelay(10);
        }
        // Çalıyorsa delay koymuyoruz veya çok kısa (1 tick) koyuyoruz ki buffer dolabilsin
        else {
            vTaskDelay(1); 
        }
    }
}

void updateMusicApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState, int& scrollY) {
    if (!appLoaded) {
        WiFi.mode(WIFI_OFF); // WiFi'yi kapat (Paraziti ve takılmayı önler)
        deinitAudio(); // Sistem seslerini kapat (I2S çakışmasını önle)
        
        // SD Kart kontrolü (Eğer başta başarısız olduysa veya çıkarıldıysa tekrar dene)
        if (!isSDMounted) initSD();

        // Mutex oluştur
        if (audioMutex == NULL) audioMutex = xSemaphoreCreateMutex();

        // Audio nesnesini sıfırdan oluştur
        if (audio != nullptr) delete audio;
        audio = new Audio();
        
        audio->setPinout(I2S_BCK, I2S_WS, I2S_DOUT);
        audio->forceMono(true); // MAX98357A mono amfi oldugu icin stereo sesi birlestir
        currentVolume = settings_soundVolume * 2; // 1-10 ayarını 2-20 aralığına çek
        audio->setVolume(currentVolume);
        
        // Ses Görevini Başlat (Core 0'da)
        xTaskCreatePinnedToCore(
            audioTask,      // Görev fonksiyonu
            "AudioTask",    // Görev adı
            10000,          // Stack boyutu (MP3 için geniş tutalım)
            NULL,           // Parametre
            2,              // Öncelik (Yüksek)
            &audioTaskHandle, // Tutucu
            0               // Core 0 (Sistem çekirdeği)
        );

        loadFilesByExtension("/MP3", ".mp3"); // MP3 klasöründeki .mp3'leri yükle
        selectedFileIndex = 0;
        playingFile = "";
        songFinished = false;
        appLoaded = true;
        scrollY = 0;
        needsRedraw = true;
    }

    // --- OTOMATİK ŞARKI GEÇİŞİ ---
    if (songFinished) {
        songFinished = false;
        selectedFileIndex++;
        if (selectedFileIndex >= (int)fileList.size()) selectedFileIndex = 0;
        
        // Yeni şarkıyı başlat
        playingFile = fileList[selectedFileIndex];
        
        // Görevi duraklat, dosya aç, devam et (Çakışmayı önlemek için)
        if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
            if (audio && isSDMounted) audio->connecttoFS(SD, ("/MP3/" + playingFile).c_str());
            xSemaphoreGive(audioMutex);
        }
        
        isPlaying = true;
        needsRedraw = true;
    }

    if (fileList.empty()) {
        // Dosya yoksa uyarı ekranı göster
        if (needsRedraw) {
            drawAppScreen(spr, "MUZIK", 0x780F, "MP3 Klasoru Bos\nveya SD Kart Yok"); 
            spr->pushSprite(0, 0);
            needsRedraw = false;
        }
        // ÇIKIŞ (Dosya yoksa)
        if (res.clicked && res.y > 250) {
            if (audioMutex) {
                vSemaphoreDelete(audioMutex);
                audioMutex = NULL;
            }
            if (audioTaskHandle) {
                vTaskDelete(audioTaskHandle); // Görevi öldür
                audioTaskHandle = NULL;
            }
            if (audio) { delete audio; audio = nullptr; }
            initAudio();
            currentState = STATE_MENU;
            appLoaded = false;
            needsRedraw = true;
            delay(200);
        }
    } else {
        // --- KONTROLLER ---
        
        // 1. Listede Gezinme (Yukarı/Aşağı)
        if (res.tbDown) {
            selectedFileIndex++;
            if (selectedFileIndex >= (int)fileList.size()) selectedFileIndex = 0;
            // Listeyi seçime göre kaydır
            if ((selectedFileIndex * 35) > (scrollY + 130)) scrollY = (selectedFileIndex * 35) - 130; // Liste alanı kısaldı
            if (scrollY < 0) scrollY = 0;
            needsRedraw = true;
        }
        if (res.tbUp) {
            selectedFileIndex--;
            if (selectedFileIndex < 0) selectedFileIndex = (int)fileList.size() - 1;
            // Listeyi seçime göre kaydır
            if ((selectedFileIndex * 35) < scrollY) scrollY = selectedFileIndex * 35;
            needsRedraw = true;
        }

        // 2. Ses Kontrolü (Sol/Sağ)
        if (res.tbRight) {
            currentVolume++;
            if (currentVolume > 21) currentVolume = 21;
            if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
                if (audio) audio->setVolume(currentVolume);
                xSemaphoreGive(audioMutex);
            }
            needsRedraw = true;
        }
        if (res.tbLeft) {
            currentVolume--;
            if (currentVolume < 0) currentVolume = 0;
            if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
                if (audio) audio->setVolume(currentVolume);
                xSemaphoreGive(audioMutex);
            }
            needsRedraw = true;
        }

        // --- DOKUNMATİK KONTROLLER ---
        // Listeyi kaydırma (Sürükleme)
        if (res.dragging) {
            scrollY += res.scrollY;
            if (scrollY < 0) scrollY = 0;
            int maxScroll = (fileList.size() * 35) - 170; // Liste yüksekliği azaldı
            if (maxScroll < 0) maxScroll = 0;
            if (scrollY > maxScroll) scrollY = maxScroll;
            needsRedraw = true;
        }

        if (res.clicked) {
            // 1. LİSTE ALANI (Üst Kısım)
            if (res.y < 170) {
                int clickedIndex = (res.y - 10 + scrollY) / 35;
                if (clickedIndex >= 0 && clickedIndex < (int)fileList.size()) {
                    selectedFileIndex = clickedIndex;
                    
                    // Listeden seçince direkt çal (Toggle yapma - Sorunu çözen kısım)
                    String selectedName = fileList[selectedFileIndex];
                    if (selectedName == playingFile && audio && audio->isRunning()) {
                        // Zaten bu şarkı çalıyor, duraklatma yapma! 
                        // (Kullanıcı "duruyor" dediği için burayı boş bırakıyoruz, çalmaya devam etsin)
                    } else {
                        // Farklı şarkı veya durmuş şarkı -> Başlat
                        playingFile = selectedName;
                        if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
                            if (audio && isSDMounted) audio->connecttoFS(SD, ("/MP3/" + playingFile).c_str());
                            xSemaphoreGive(audioMutex);
                        }
                        isPlaying = true;
                    }
                    needsRedraw = true;
                    // res.tbPress = true; // BURAYI KALDIRDIK (Aşağıdaki toggle mantığına girmesin diye)
                }
            }
            // 2. PLAYER KONTROLLERİ (Alt Kısım)
            else {
                // ÇIKIŞ BUTONU (En Alt)
                if (res.y > 260) { // Alanı daha da genişlettik (Garanti olsun)
                    if (audioTaskHandle) { vTaskDelete(audioTaskHandle); audioTaskHandle = NULL; }
                    if (audioMutex) { vSemaphoreDelete(audioMutex); audioMutex = NULL; }
                    if (audio) { audio->stopSong(); delete audio; audio = nullptr; }
                    initAudio();
                    currentState = STATE_MENU;
                    appLoaded = false;
                    needsRedraw = true;
                    delay(200);
                    return; // Fonksiyondan çık
                }
                // PLAYER BUTONLARI (Orta Alt)
                else if (res.y > 180 && res.y <= 260) { // Alanı yukarı doğru genişlettik
                    // GERİ (PREV)
                    if (res.x < 80) {
                        selectedFileIndex--;
                        if (selectedFileIndex < 0) selectedFileIndex = (int)fileList.size() - 1;
                        res.tbPress = true; // Yeni şarkıyı çal
                    }
                    // İLERİ (NEXT)
                    else if (res.x > 160) {
                        selectedFileIndex++;
                        if (selectedFileIndex >= (int)fileList.size()) selectedFileIndex = 0;
                        res.tbPress = true; // Yeni şarkıyı çal
                    }
                    // OYNAT / DURAKLAT (PLAY/PAUSE) - ORTA
                    else {
                        // Eğer zaten çalan şarkı seçiliyse duraklatır, değilse yenisini çalar
                        res.tbPress = true; 
                    }
                    needsRedraw = true;
                }
            }
        }

        // 3. Oynat / Duraklat (Tıklama)
        if (res.tbPress) {
            String selectedName = fileList[selectedFileIndex];
            
            if (selectedName == playingFile && audio && audio->isRunning()) {
                // Aynı şarkıysa Duraklat/Devam Et
                if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
                    audio->pauseResume();
                    isPlaying = !isPlaying;
                    xSemaphoreGive(audioMutex);
                }
            } else {
                // Yeni şarkı çal
                playingFile = selectedName;
                
                if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
                    if (audio) audio->connecttoFS(SD, ("/MP3/" + playingFile).c_str());
                    xSemaphoreGive(audioMutex);
                }
                
                isPlaying = true;
            }
            needsRedraw = true;
        }

        if (needsRedraw) {
            // Arka planı temizle (Sadece liste kısmını)
            spr->fillRect(0, 0, 240, 170, TFT_BLACK);
            drawSDCardBrowser(spr, selectedFileIndex, scrollY); // Listeyi çiz
            
            // --- MODERN PLAYER ARAYÜZÜ (Alt Kısım) ---
            int playerY = 170; // Player'ı yukarı taşıdık
            int playerH = 150;
            uint16_t bgCol = 0x1084; // Koyu Cyan/Gri karışımı
            uint16_t barBg = 0x0000;
            uint16_t barFill = 0x07E0; // Yeşil

            spr->fillRect(0, playerY, 240, playerH, bgCol);
            spr->drawFastHLine(0, playerY, 240, TFT_WHITE);

            // Şarkı İsmi
            spr->setTextColor(TFT_WHITE, bgCol);
            String dispName = playingFile.length() > 0 ? playingFile : "Muzik Secin";
            if (dispName.length() > 20) dispName = dispName.substring(0, 17) + "...";
            spr->drawCentreString(dispName, 120, playerY + 8, 4);

            // Süre ve İlerleme Bilgileri
            int curTime = 0;
            int totTime = 0;
            if (audio && isPlaying) {
                // Mutex ile güvenli okuma
                if (xSemaphoreTake(audioMutex, 10)) { // Kısa bekleme
                    curTime = audio->getAudioCurrentTime();
                    totTime = audio->getAudioFileDuration();
                    xSemaphoreGive(audioMutex);
                }
            }

            // İlerleme Çubuğu
            int barX = 20;
            int barY = playerY + 35;
            int barW = 200;
            int barH = 8;
            int fillW = 0;
            if (totTime > 0) fillW = (curTime * barW) / totTime;
            if (fillW > barW) fillW = barW;

            spr->fillRoundRect(barX, barY, barW, barH, 4, barBg);
            spr->fillRoundRect(barX, barY, fillW, barH, 4, barFill);
            spr->drawRoundRect(barX, barY, barW, barH, 4, TFT_WHITE);

            // Süre Metni (00:00 / 03:45)
            char timeStr[20];
            sprintf(timeStr, "%02d:%02d / %02d:%02d", curTime/60, curTime%60, totTime/60, totTime%60);
            spr->drawCentreString(timeStr, 120, barY + 12, 2);

            // --- KONTROL BUTONLARI ---
            int btnY = playerY + 85; // Butonları aşağı kaydırdık (Çakışmayı önlemek için)
            
            // Basılı tutulan alanı kontrol et
            bool pressPrev = (res.dragging || res.clicked) && (res.y > 180 && res.y <= 270) && (res.x < 80);
            bool pressNext = (res.dragging || res.clicked) && (res.y > 180 && res.y <= 270) && (res.x > 160);
            bool pressPlay = (res.dragging || res.clicked) && (res.y > 180 && res.y <= 270) && (res.x >= 80 && res.x <= 160);

            // Geri Butonu (Sol)
            if (pressPrev) spr->fillTriangle(50, btnY, 70, btnY-15, 70, btnY+15, TFT_GREEN);
            else           spr->fillTriangle(50, btnY, 70, btnY-15, 70, btnY+15, TFT_WHITE);
            
            // Oynat/Duraklat (Orta)
            if (isPlaying) {
                // Pause ikonu (İki çubuk)
                uint16_t pCol = pressPlay ? TFT_GREEN : TFT_WHITE;
                spr->fillRect(110, btnY-15, 8, 30, pCol);
                spr->fillRect(122, btnY-15, 8, 30, pCol);
            } else {
                // Play ikonu (Üçgen)
                if (pressPlay) spr->fillTriangle(110, btnY-15, 110, btnY+15, 135, btnY, TFT_GREEN);
                else           spr->fillTriangle(110, btnY-15, 110, btnY+15, 135, btnY, TFT_WHITE);
            }

            // İleri Butonu (Sağ)
            if (pressNext) spr->fillTriangle(190, btnY, 170, btnY-15, 170, btnY+15, TFT_GREEN);
            else           spr->fillTriangle(190, btnY, 170, btnY-15, 170, btnY+15, TFT_WHITE);

            // ÇIKIŞ BUTONU (En Alt)
            // Basılı tutuluyorsa rengi değiştir (Geri bildirim)
            if (res.dragging && res.y > 260) {
                spr->fillRoundRect(60, 285, 120, 30, 5, TFT_WHITE);
                spr->setTextColor(TFT_RED, TFT_WHITE);
            } else {
                spr->fillRoundRect(60, 285, 120, 30, 5, TFT_RED);
                spr->setTextColor(TFT_WHITE, TFT_RED);
            }
            spr->drawCentreString("CIKIS", 120, 297, 2);

            spr->pushSprite(0, 0);
            needsRedraw = false;
        }
    }
}