#ifndef APP_PHOTO_H
#define APP_PHOTO_H

#include <TFT_eSPI.h>
#include "controls.h"
#include "config.h"

void updatePhotoApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState, int& scrollY);

#endif