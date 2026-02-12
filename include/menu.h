#ifndef MENU_H
#define MENU_H
#include <TFT_eSPI.h>
#include "config.h"

void drawMenu(TFT_eSprite* spr, int scrollX, int selectedIdx = -1);
void drawAppScreen(TFT_eSprite* spr, String title, uint16_t color, String msg = "", bool isPressed = false);
int getMenuIndex(int x, int y, int scrollX); // 3 parametre: X, Y ve Kaydırma miktarı

#endif