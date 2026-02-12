#include "controls.h"
#include "config.h"

static int sX, sY, lastX, lastY;
static int startX, startY;
static bool isDown = false;

// Trackball önceki durumları (Kenar algılama için)
static int lastTbState = 0; // Bitmask: 0:Up, 1:Down, 2:Left, 3:Right, 4:Btn
static unsigned long lastTbTime = 0; // Debounce için zamanlayıcı

void Controls::init() {
    pinMode(TR_UP, INPUT_PULLUP);
    pinMode(TR_DWN, INPUT_PULLUP);
    pinMode(TR_LFT, INPUT_PULLUP);
    pinMode(TR_RHT, INPUT_PULLUP);
    pinMode(TR_BTN, INPUT_PULLUP);
}

ControlResult Controls::update(TFT_eSPI* tft) {
    ControlResult res = {false, false, false, 0, 0, 0, 0, false, false, false, false, false};
    uint16_t tx, ty;
    // Hassasiyet eşiğini düşürdük (100 -> 50) algılamayı iyileştirmek için
    bool touched = tft->getTouch(&tx, &ty, 50);

    if (touched) {
        // DOKUNMATİK DÜZELTMESİ (Swap & Scale):
        // ty (0-320) -> Screen X (0-240) [Ters]
        // tx (0-240) -> Screen Y (0-320) [Düz]
        
        int screenX = map(ty, 0, 320, 240, 0);
        int screenY = map(tx, 0, 240, 0, 320);

        // Sınır kontrolleri
        if (screenX < 0) screenX = 0;
        if (screenX > 240) screenX = 240;
        if (screenY < 0) screenY = 0;
        if (screenY > 320) screenY = 320;

        if (!isDown) { 
            sX = screenX; sY = screenY; 
            startX = screenX; startY = screenY;
            isDown = true; 
        }
        res.dragging = true;
        res.scrollX = sX - screenX;
        res.scrollY = sY - screenY;
        sX = screenX;
        sY = screenY;
        lastX = screenX;
        lastY = screenY;
    } else if (isDown) {
        res.released = true;
        // Hassasiyeti artırdık: 60 pikselden az hareketi tıklama say (Çok daha kolay tıklama)
        if (abs(startX - sX) < 60 && abs(startY - sY) < 60) {
            res.clicked = true;
        }
        res.x = lastX;
        res.y = lastY;
        isDown = false;
    }

    // --- TRACKBALL OKUMA ---
    // Pinler INPUT_PULLUP olduğu için basılınca LOW (0) olur.
    // Sadece basıldığı anı (Düşen kenar) yakalıyoruz.
    
    int currentTbState = 0;
    if (digitalRead(TR_UP) == LOW) currentTbState |= (1 << 0);
    if (digitalRead(TR_DWN) == LOW) currentTbState |= (1 << 1);
    if (digitalRead(TR_LFT) == LOW) currentTbState |= (1 << 2);
    if (digitalRead(TR_RHT) == LOW) currentTbState |= (1 << 3);
    if (digitalRead(TR_BTN) == LOW) currentTbState |= (1 << 4);

    // Hassasiyeti azaltmak için zaman kontrolü (100ms bekleme)
    unsigned long now = millis();
    if (now - lastTbTime > TRACKBALL_SENSITIVITY) {
        bool action = false;
        if ((currentTbState & (1<<0)) && !(lastTbState & (1<<0))) { res.tbUp = true; action = true; }
        if ((currentTbState & (1<<1)) && !(lastTbState & (1<<1))) { res.tbDown = true; action = true; }
        if ((currentTbState & (1<<2)) && !(lastTbState & (1<<2))) { res.tbLeft = true; action = true; }
        if ((currentTbState & (1<<3)) && !(lastTbState & (1<<3))) { res.tbRight = true; action = true; }
        if ((currentTbState & (1<<4)) && !(lastTbState & (1<<4))) { res.tbPress = true; action = true; }
        if (action) lastTbTime = now;
    }

    lastTbState = currentTbState;

    return res;
}