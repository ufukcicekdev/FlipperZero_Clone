#ifndef APP_CLOCK_H
#define APP_CLOCK_H

#include <TFT_eSPI.h>
#include "controls.h"
#include "config.h"

// Saat uygulamasını güncelleyen fonksiyon
void updateClockApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState);

#endif