#ifndef CONTROLS_H
#define CONTROLS_H
#include <TFT_eSPI.h>

struct ControlResult {
    bool clicked;
    bool dragging;
    bool released;
    int scrollX; // Yana veya dikey kaydırma farkı
    int scrollY; // Dikey kaydırma farkı
    int x, y;
    // Trackball Durumları
    bool tbUp;
    bool tbDown;
    bool tbLeft;
    bool tbRight;
    bool tbPress;
};

class Controls {
public:
    static void init();
    static ControlResult update(TFT_eSPI* tft);
};
#endif