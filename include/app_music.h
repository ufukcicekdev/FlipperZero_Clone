#ifndef APP_MUSIC_H
#define APP_MUSIC_H

#include <TFT_eSPI.h>
#include "controls.h"
#include "config.h"

void updateMusicApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState, int& scrollY);

#endif