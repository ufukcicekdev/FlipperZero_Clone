#include "app_system.h"
#include "settings.h"
#include "menu.h"
#include "sound.h"

static int selectedItem = 0;
const int totalSettings = 3;

void updateSystemApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState) {
    if (!appLoaded) {
        selectedItem = 0;
        appLoaded = true;
        needsRedraw = true;
    }

    // --- Navigasyon ---
    if (res.tbDown) {
        selectedItem++;
        if (selectedItem >= totalSettings) selectedItem = 0;
        playSystemSound(SOUND_SCROLL);
        needsRedraw = true;
    }
    if (res.tbUp) {
        selectedItem--;
        if (selectedItem < 0) selectedItem = totalSettings - 1;
        playSystemSound(SOUND_SCROLL);
        needsRedraw = true;
    }

    // --- Değer Değiştirme ---
    bool valueChanged = false;
    if (res.tbRight || res.tbLeft) {
        int dir = res.tbRight ? 1 : -1;
        
        if (selectedItem == 0) { // Sound ON/OFF
            settings_soundEnabled = !settings_soundEnabled;
        }
        else if (selectedItem == 1) { // Volume
            settings_soundVolume += dir;
            if (settings_soundVolume < 1) settings_soundVolume = 1;
            if (settings_soundVolume > 10) settings_soundVolume = 10;
        }
        else if (selectedItem == 2) { // Sound Type
            settings_soundType += dir;
            if (settings_soundType < 0) settings_soundType = 2;
            if (settings_soundType > 2) settings_soundType = 0;
        }
        valueChanged = true;
        needsRedraw = true;
    }

    if (valueChanged && settings_soundEnabled) {
        playSystemSound(SOUND_CLICK);
    }

    // --- Çizim ---
    if (needsRedraw) {
        spr->fillSprite(TFT_BLACK);
        
        // Başlık
        spr->fillRect(0, 0, 240, 40, 0x4208);
        spr->setTextColor(TFT_WHITE);
        spr->drawCentreString("SISTEM AYARLARI", 120, 10, 4);

        // Liste
        int startY = 60;
        int gap = 50;

        // 1. Ses Durumu
        if (selectedItem == 0) spr->fillRoundRect(10, startY, 220, 40, 5, 0x3333);
        spr->setTextColor(selectedItem == 0 ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        spr->drawString("Ses:", 20, startY + 10, 4);
        spr->drawRightString(settings_soundEnabled ? "ACIK" : "KAPALI", 220, startY + 10, 4);

        // 2. Ses Seviyesi
        if (selectedItem == 1) spr->fillRoundRect(10, startY + gap, 220, 40, 5, 0x3333);
        spr->setTextColor(selectedItem == 1 ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        spr->drawString("Seviye:", 20, startY + gap + 10, 4);
        
        // Seviye barı çizimi
        int barW = settings_soundVolume * 8;
        spr->fillRect(130, startY + gap + 10, 80, 20, 0x2104); // Arka plan
        spr->fillRect(130, startY + gap + 10, barW, 20, TFT_GREEN); // Doluluk

        // 3. Ses Tipi
        if (selectedItem == 2) spr->fillRoundRect(10, startY + gap*2, 220, 40, 5, 0x3333);
        spr->setTextColor(selectedItem == 2 ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        spr->drawString("Paket:", 20, startY + gap*2 + 10, 4);
        
        String typeName = "Default";
        if (settings_soundType == 1) typeName = "Retro";
        if (settings_soundType == 2) typeName = "Sci-Fi";
        spr->drawRightString(typeName, 220, startY + gap*2 + 10, 4);

        // Geri Butonu
        spr->fillRect(0, 270, 240, 50, 0x2104);
        spr->fillRoundRect(10, 275, 100, 40, 8, TFT_RED);
        spr->drawCentreString("BACK", 60, 285, 2);
        
        spr->pushSprite(0, 0);
        needsRedraw = false;
    }

    // Çıkış
    if (res.clicked && res.y > 250) { 
        playSystemSound(SOUND_BACK);
        currentState = STATE_MENU;
        appLoaded = false;
        needsRedraw = true;
        delay(200);
    }
    if (res.tbPress) {
        playSystemSound(SOUND_BACK);
        currentState = STATE_MENU;
        appLoaded = false;
        needsRedraw = true;
        delay(200);
    }
}