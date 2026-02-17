#ifndef APP_GAMES_H
#define APP_GAMES_H

#include <TFT_eSPI.h>
#include "config.h"
#include "controls.h"

// Oyun uygulamasını başlatan ve yöneten fonksiyon
void updateGamesApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState);

#endif