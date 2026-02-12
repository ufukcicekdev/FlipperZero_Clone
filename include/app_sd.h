#ifndef APP_SD_H
#define APP_SD_H

#include <TFT_eSPI.h>
#include "controls.h"
#include "config.h"

void updateSDApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState, int& scrollY);

#endif