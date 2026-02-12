#ifndef APP_NFC_H
#define APP_NFC_H

#include <TFT_eSPI.h>
#include "controls.h"
#include "config.h"

void updateNFCApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState, int& scrollY);

#endif