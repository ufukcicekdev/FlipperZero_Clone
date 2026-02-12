#ifndef SD_BROWSER_H
#define SD_BROWSER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <vector>
#include "config.h" // AppState tanımı buradan gelecek

// fileList'e her yerden güvenli erişim için extern tanımı
extern std::vector<String> fileList;
extern bool isSDMounted; // SD kart durumunu takip etmek için

// Fonksiyonlar
void initSD();
void loadSDFiles();
void drawSDCardBrowser(TFT_eSprite* spr, int selectedIdx, int scrollOffset);
int getSDFileListSize();
void loadFilesByExtension(const char* folder, const char* ext, bool clear = true);

#endif