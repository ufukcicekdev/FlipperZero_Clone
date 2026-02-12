#ifndef APP_SYSTEM_H
#define APP_SYSTEM_H

#include <TFT_eSPI.h>
#include "controls.h"
#include "config.h"

void updateSystemApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState);

#endif