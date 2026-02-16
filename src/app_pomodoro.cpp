#include "app_pomodoro.h"
#include "sound.h"

enum PomodoroState {
    POMO_SETUP,
    POMO_RUNNING,
    POMO_PAUSED,
    POMO_FINISHED
};

static PomodoroState pomoState = POMO_SETUP;
static int targetDurationMinutes = 30; // Varsayılan 30 dk
static unsigned long startTime = 0;
static unsigned long elapsedTimeBeforePause = 0;
static unsigned long lastTick = 0;

void updatePomodoroApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState) {
    // Uygulama ilk açıldığında değişkenleri sıfırla
    if (!appLoaded) {
        pomoState = POMO_SETUP;
        targetDurationMinutes = 30;
        elapsedTimeBeforePause = 0;
        appLoaded = true;
        needsRedraw = true;
    }

    // --- MANTIK ---
    if (pomoState == POMO_RUNNING) {
        // Her saniye ekranı güncelle
        if (millis() - lastTick >= 1000) {
            lastTick = millis();
            needsRedraw = true;
            
            unsigned long currentElapsed = (millis() - startTime) + elapsedTimeBeforePause;
            unsigned long totalMs = (unsigned long)targetDurationMinutes * 60 * 1000;

            if (currentElapsed >= totalMs) {
                pomoState = POMO_FINISHED;
                needsRedraw = true;
                playSystemSound(SOUND_ENTER); // Bitiş sesi
            }
        }
    } else if (pomoState == POMO_FINISHED) {
         // Bittiğinde periyodik sinyal ver (Her saniye bip)
         if (millis() - lastTick >= 1000) {
            lastTick = millis();
            playSystemSound(SOUND_ENTER); 
         }
    }

    // --- KONTROLLER ---
    if (pomoState == POMO_SETUP) {
        // Süre Ayarlama
        if (res.tbUp) {
            targetDurationMinutes += POMODORO_STEP_MIN;
            if (targetDurationMinutes > 120) targetDurationMinutes = 120;
            needsRedraw = true;
        }
        if (res.tbDown) {
            targetDurationMinutes -= POMODORO_STEP_MIN;
            if (targetDurationMinutes < 1) targetDurationMinutes = 1;
            needsRedraw = true;
        }
        if (res.tbRight) {
             targetDurationMinutes++;
             if (targetDurationMinutes > 120) targetDurationMinutes = 120;
             needsRedraw = true;
        }
         if (res.tbLeft) {
             targetDurationMinutes--;
             if (targetDurationMinutes < 1) targetDurationMinutes = 1;
             needsRedraw = true;
        }

        // Başlat (Trackball veya Dokunmatik Buton)
        bool startClicked = (res.clicked && res.y >= 220 && res.y < 260);
        if (res.tbPress || startClicked) {
            pomoState = POMO_RUNNING;
            startTime = millis();
            elapsedTimeBeforePause = 0;
            lastTick = millis();
            needsRedraw = true;
            playSystemSound(SOUND_ENTER);
            delay(200);
        }
    } 
    else if (pomoState == POMO_RUNNING) {
        // Duraklat
        bool pauseClicked = (res.clicked && res.y >= 220 && res.y < 260);
        if (res.tbPress || pauseClicked) {
            pomoState = POMO_PAUSED;
            elapsedTimeBeforePause += (millis() - startTime);
            needsRedraw = true;
            playSystemSound(SOUND_SCROLL);
            delay(200);
        }
    } 
    else if (pomoState == POMO_PAUSED) {
        // Devam Et
        bool resumeClicked = (res.clicked && res.y >= 220 && res.y < 260);
        if (res.tbPress || resumeClicked) {
            pomoState = POMO_RUNNING;
            startTime = millis();
            needsRedraw = true;
            playSystemSound(SOUND_ENTER);
            delay(200);
        }
    } 
    else if (pomoState == POMO_FINISHED) {
        // Bitir / Başa Dön
        if (res.clicked || res.tbPress) {
            pomoState = POMO_SETUP;
            needsRedraw = true;
            delay(200);
        }
    }

    // Çıkış (Geri Butonu - En Alt)
    if (res.clicked && res.y > 260) {
         currentState = STATE_MENU;
         appLoaded = false;
         needsRedraw = true;
         playSystemSound(SOUND_BACK);
         delay(200);
         return;
    }
    
    // --- ÇİZİM ---
    if (needsRedraw) {
        spr->fillSprite(TFT_BLACK);
        
        // Başlık
        spr->fillRect(0, 0, 240, 30, 0xF800); // Kırmızı Başlık
        spr->setTextColor(TFT_WHITE, 0xF800);
        spr->drawCentreString("POMODORO", 120, 5, 2);

        spr->setTextColor(TFT_WHITE, TFT_BLACK);

        if (pomoState == POMO_SETUP) {
            spr->drawCentreString("SURE AYARLA", 120, 50, 2);
            
            String timeStr = String(targetDurationMinutes) + " dk";
            spr->setTextSize(2); // Büyük font
            spr->drawCentreString(timeStr, 120, 90, 4); 
            spr->setTextSize(1);

            spr->setTextColor(TFT_SILVER, TFT_BLACK);
            String helpStr = "Yukari/Asagi: +/- " + String(POMODORO_STEP_MIN) + "dk";
            spr->drawCentreString(helpStr, 120, 150, 2);
            spr->drawCentreString("Sag/Sol: +/- 1dk", 120, 170, 2);

            // Başlat Butonu
            spr->fillRoundRect(40, 220, 160, 40, 5, 0x07E0); // Yeşil
            spr->setTextColor(TFT_BLACK, 0x07E0);
            spr->drawCentreString("BASLAT", 120, 230, 2);
        } 
        else if (pomoState == POMO_RUNNING || pomoState == POMO_PAUSED) {
            unsigned long currentElapsed = elapsedTimeBeforePause;
            if (pomoState == POMO_RUNNING) {
                currentElapsed += (millis() - startTime);
            }
            
            unsigned long totalMs = (unsigned long)targetDurationMinutes * 60 * 1000;
            unsigned long remaining = (totalMs > currentElapsed) ? (totalMs - currentElapsed) : 0;

            int remMin = remaining / 60000;
            int remSec = (remaining % 60000) / 1000;

            char timeBuf[10];
            sprintf(timeBuf, "%02d:%02d", remMin, remSec);

            // Geri sayım
            spr->setTextSize(1); // Ekrana sığması için boyut 1 yapıldı
            spr->setTextColor(pomoState == POMO_PAUSED ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
            spr->drawCentreString(timeBuf, 120, 80, 7); // Y konumu biraz yukarı alındı

            // Progress Bar
            int barWidth = 200;
            int progress = (currentElapsed * barWidth) / totalMs;
            if (progress > barWidth) progress = barWidth;
            
            int barY = 150; // Bar konumu ayarlandı
            spr->drawRect(20, barY, barWidth, 15, TFT_WHITE);
            if (progress > 4) spr->fillRect(22, barY + 2, progress - 4, 11, 0xF800);

            if (pomoState == POMO_PAUSED) {
                spr->setTextColor(TFT_YELLOW, TFT_BLACK);
                spr->drawCentreString("DURAKLATILDI", 120, 180, 2);
                
                spr->fillRoundRect(40, 220, 160, 40, 5, 0x07E0);
                spr->setTextColor(TFT_BLACK, 0x07E0);
                spr->drawCentreString("DEVAM ET", 120, 230, 2);
            } else {
                spr->fillRoundRect(40, 220, 160, 40, 5, TFT_YELLOW);
                spr->setTextColor(TFT_BLACK, TFT_YELLOW);
                spr->drawCentreString("DURAKLAT", 120, 230, 2);
            }
        }
        else if (pomoState == POMO_FINISHED) {
            spr->fillScreen(0xF800); // Tam ekran kırmızı
            spr->setTextColor(TFT_WHITE, 0xF800);
            spr->drawCentreString("SURE DOLDU!", 120, 120, 4);
            spr->drawCentreString("Durdurmak icin tikla", 120, 180, 2);
        }

        // Geri Butonu (Alt Bar)
        if (pomoState != POMO_FINISHED) {
             spr->fillRoundRect(60, 285, 120, 30, 5, 0x2124);
             spr->setTextColor(TFT_WHITE, 0x2124);
             spr->drawCentreString("CIKIS", 120, 292, 2);
        }

        spr->pushSprite(0, 0);
        needsRedraw = false;
    }
}