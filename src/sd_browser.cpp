#include "sd_browser.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>

std::vector<String> fileList;
bool isSDMounted = false;

void initSD() {
    pinMode(SD_CS, OUTPUT); // Make sure SD_CS is defined in config.h!
    digitalWrite(SD_CS, HIGH); 

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, SPI, 4000000)) {
        Serial.println("SD Kart Baslatilamadi! Baglantilari kontrol edin.");
        isSDMounted = false;
        return;
    }
    isSDMounted = true;
    Serial.println("SD Kart Basariyla Baslatildi.");
}

void loadSDFiles() {
    loadFilesByExtension("/", ""); // Kök dizindeki tüm dosyaları yükle
}

int getSDFileListSize() {
    return fileList.size();
}

void loadFilesByExtension(const char* folder, const char* ext, bool clear) {
    if (!isSDMounted) {
        Serial.println("SD Kart takili degil veya mount edilmemis.");
        return; 
    }
    if (clear) fileList.clear(); 
    File root = SD.open(folder);
    if (!root) {
        Serial.print("Klasor acilamadi: ");
        Serial.println(folder);
        return;
    }

    String extUpper = String(ext);
    extUpper.toUpperCase(); 

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String fileName = file.name();
            
            // Sadece dosya adını al (Yol bilgisini temizle)
            int lastSlash = fileName.lastIndexOf('/');
            if (lastSlash >= 0) fileName = fileName.substring(lastSlash + 1);
            
            // Gizli dosyaları ('.' ile başlayan) atla
            if (fileName.startsWith(".") || fileName.indexOf("/.") != -1) { file = root.openNextFile(); continue; }

            if (fileName.endsWith(ext) || fileName.endsWith(extUpper)) {
                fileList.push_back(fileName);
            }
        }
        file = root.openNextFile();
    }
    root.close();
}

void drawSDCardBrowser(TFT_eSprite* spr, int selectedIdx, int scrollOffset) {
    spr->fillSprite(TFT_BLACK);
    
    // --- DOSYA LİSTESİ ---
    // Dosyaları en üstten başlatıyoruz
    for (int i = 0; i < (int)fileList.size(); i++) {
        int y = 10 + (i * 35) - scrollOffset;

        // Alt barın (280) altında kalıyorsa çizme
        if (y > -40 && y < 280) {
            if (i == selectedIdx) {
                spr->fillRect(0, y, 240, 30, 0x3411);
                spr->setTextColor(TFT_YELLOW);
            } else {
                spr->setTextColor(TFT_WHITE);
            }
            
            spr->drawString(fileList[i], 15, y + 8, 2);
            spr->drawFastHLine(10, y + 32, 220, 0x2104);
        }
    }

    // --- SABİT ALT BAR (Geri Dönüş Alanı) ---
    spr->fillRoundRect(60, 292, 120, 25, 5, TFT_RED);
    spr->setTextColor(TFT_WHITE, TFT_RED);
    spr->drawCentreString("CIKIS", 120, 297, 2);
}