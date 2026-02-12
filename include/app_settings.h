#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <TFT_eSPI.h>
#include "controls.h"
#include "config.h"

void updateSettingsApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState);

#endif