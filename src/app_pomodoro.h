#ifndef APP_POMODORO_H
#define APP_POMODORO_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "controls.h"
#include "menu.h" // AppState tanımı için

// Pomodoro uygulamasını güncelleyen ana fonksiyon
void updatePomodoroApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState);

#endif