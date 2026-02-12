#include "app_sd.h"
#include "sd_browser.h"

void updateSDApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState, int& scrollY) {
    if (!appLoaded) {
        loadSDFiles();
        appLoaded = true;
        scrollY = 0;
        needsRedraw = true;
    }

    if (res.dragging) {
        scrollY += res.scrollY;
        if (scrollY < 0) scrollY = 0;
        // Basit bir sınır kontrolü (dosya sayısı * 35 piksel)
        int maxScroll = (getSDFileListSize() * 35) - 200;
        if (maxScroll < 0) maxScroll = 0;
        if (scrollY > maxScroll) scrollY = maxScroll;
        needsRedraw = true;
    }

    // SD Browser'da Geri Tuşu Altta
    if (res.clicked && res.y > 260) { // Alan genişletildi
        currentState = STATE_MENU;
        appLoaded = false; // Çıkarken durumu sıfırla
        needsRedraw = true;
        delay(200);
    }

    // Trackball ile çıkış
    if (res.tbPress) {
        currentState = STATE_MENU;
        appLoaded = false;
        needsRedraw = true;
        delay(200);
    }

    if (needsRedraw) {
        drawSDCardBrowser(spr, -1, scrollY);
        spr->pushSprite(0, 0);
        needsRedraw = false;
    }
}