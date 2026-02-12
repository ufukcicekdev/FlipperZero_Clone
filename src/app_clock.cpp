#include "app_clock.h"
#include <WiFi.h>
#include <time.h>
#include "menu.h"
#include "settings.h"
#include <math.h>

// Saat dilimi yapıları sadece bu dosyada lazım
struct TimeZone {
    const char* name;
    int offset;
};

const TimeZone timeZones[] = {
    {"Turkey", 3},   {"Germany", 1}, {"UK", 0},
    {"New York", -5},{"Tokyo", 9},   {"China", 8}
};
const int numTimeZones = sizeof(timeZones) / sizeof(timeZones[0]);
static int currentTimezoneIdx = 0; // Varsayılan: Turkey

// Wi-Fi Durum Yönetimi için Statik Değişkenler
static int wifiState = 0; // 0: Başlangıç, 1: Bağlanıyor, 2: Bağlı, 3: Hata
static unsigned long wifiTimer = 0;

void updateClockApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState) {
    if (!appLoaded) {
        wifiState = 0; // Durumu sıfırla
        appLoaded = true;
        needsRedraw = true;
    }

    if (wifiState == 0) {
        if (WiFi.status() == WL_CONNECTED) {
            configTime(timeZones[currentTimezoneIdx].offset * 3600, 0, "pool.ntp.org");
            wifiState = 2; // Zaten bağlı
            needsRedraw = true;
        } else {
            WiFi.mode(WIFI_STA);
            WiFi.setTxPower(WIFI_POWER_8_5dBm); // Düşük güç
            WiFi.disconnect(true); // Eski bağlantıyı temizle
            delay(50); 
            WiFi.begin(WIFI_SSID, WIFI_PASS);
            wifiTimer = millis();
            wifiState = 1; // Bağlanıyor moduna geç
            needsRedraw = true;
        }
    }
    else if (wifiState == 1) {
        if (WiFi.status() == WL_CONNECTED) {
            configTime(timeZones[currentTimezoneIdx].offset * 3600, 0, "pool.ntp.org");
            wifiState = 2; // Başarılı
            needsRedraw = true;
        } else if (millis() - wifiTimer > 8000) { // 8 saniye zaman aşımı
            wifiState = 3; // Başarısız
            needsRedraw = true;
        }
    }

    if (wifiState == 2) { // Sadece bağlıyken değiştir
        if (res.tbRight || res.tbUp) { 
            currentTimezoneIdx++; 
            if (currentTimezoneIdx >= numTimeZones) currentTimezoneIdx = 0;
            configTime(timeZones[currentTimezoneIdx].offset * 3600, 0, "pool.ntp.org"); 
            needsRedraw = true; 
        }
        if (res.tbLeft || res.tbDown) { 
            currentTimezoneIdx--; 
            if (currentTimezoneIdx < 0) currentTimezoneIdx = numTimeZones - 1;
            configTime(timeZones[currentTimezoneIdx].offset * 3600, 0, "pool.ntp.org"); 
            needsRedraw = true; 
        }
    }

    struct tm timeinfo;
    static int lastSecond = -1;
    
    if (wifiState == 2) {
        if (getLocalTime(&timeinfo, 10)) {
            if (timeinfo.tm_sec != lastSecond) {
                lastSecond = timeinfo.tm_sec;
                needsRedraw = true;
            }
        }
    }

    if (needsRedraw) {
        spr->fillSprite(TFT_BLACK);

        if (wifiState == 1) {
            drawAppScreen(spr, "SAAT", 0x0000, "Baglaniyor...");
        } 
        else if (wifiState == 3) {
            drawAppScreen(spr, "SAAT", 0x0000, "WiFi Hatasi!");
        }
        else if (wifiState == 2) {
            if(!getLocalTime(&timeinfo, 10)){
                drawAppScreen(spr, "SAAT", 0x0000, "Guncelleniyor...");
            } else {
                spr->fillRectVGradient(0, 0, 240, 320, 0x1020, 0x0000);

                if (settings_clockType == 0) {
                    spr->setTextColor(0x07E0, 0x1020); // Parlak Yeşil
                    
                    char timeStr[6];
                    strftime(timeStr, 6, "%H:%M", &timeinfo);
                    spr->setTextSize(1);
                    spr->drawCentreString(timeStr, 120, 80, 7);

                    char secStr[10];
                    strftime(secStr, 10, ":%S", &timeinfo);
                    spr->setTextColor(TFT_WHITE, 0x0000);
                    spr->drawCentreString(secStr, 120, 140, 4);
                } 
                else {
                    int cx = 120;
                    int cy = 140;
                    int r = 80;
                    float deg2rad = 0.0174532925;

                    spr->drawCircle(cx, cy, r, TFT_WHITE);
                    spr->drawCircle(cx, cy, r-1, TFT_WHITE);
                    
                    for (int i = 0; i < 12; i++) {
                        float angle = i * 30 * deg2rad;
                        spr->drawLine(cx + (r-10)*sin(angle), cy - (r-10)*cos(angle), cx + (r-2)*sin(angle), cy - (r-2)*cos(angle), TFT_WHITE);
                    }

                    float hAngle = (timeinfo.tm_hour % 12 + timeinfo.tm_min / 60.0) * 30 * deg2rad;
                    spr->drawWideLine(cx, cy, cx + (r*0.5)*sin(hAngle), cy - (r*0.5)*cos(hAngle), 4, TFT_WHITE, TFT_BLACK);

                    float mAngle = (timeinfo.tm_min + timeinfo.tm_sec / 60.0) * 6 * deg2rad;
                    spr->drawWideLine(cx, cy, cx + (r*0.75)*sin(mAngle), cy - (r*0.75)*cos(mAngle), 3, TFT_CYAN, TFT_BLACK);

                    float sAngle = timeinfo.tm_sec * 6 * deg2rad;
                    int sx = cx + (r*0.85)*sin(sAngle);
                    int sy = cy - (r*0.85)*cos(sAngle);
                    spr->drawLine(cx, cy, sx, sy, TFT_RED);
                    spr->fillCircle(cx, cy, 3, TFT_RED);
                }

                char dateStr[20];
                strftime(dateStr, 20, "%d %B %Y", &timeinfo);
                spr->setTextColor(TFT_SILVER, 0x0000);
                spr->drawCentreString(dateStr, 120, 180, 2);

                String zoneInfo = String(timeZones[currentTimezoneIdx].name) + " (UTC" + (timeZones[currentTimezoneIdx].offset >= 0 ? "+" : "") + String(timeZones[currentTimezoneIdx].offset) + ")";
                spr->setTextColor(TFT_SKYBLUE, 0x0000);
                spr->drawCentreString(zoneInfo, 120, 210, 2);

                int secWidth = (timeinfo.tm_sec * 240) / 60;
                spr->fillRect(0, 265, secWidth, 4, 0x07E0);
                spr->drawFastHLine(0, 265, 240, 0x3333); // Çubuk yolu

                if (res.dragging && res.y > 260) {
                    spr->fillRoundRect(60, 285, 120, 30, 5, TFT_WHITE);
                    spr->setTextColor(TFT_RED, TFT_WHITE);
                } else {
                    spr->fillRoundRect(60, 285, 120, 30, 5, TFT_RED);
                    spr->setTextColor(TFT_WHITE, TFT_RED);
                }
                spr->drawCentreString("CIKIS", 120, 297, 2);
            }
        }
        spr->pushSprite(0, 0);
        needsRedraw = false;
    }
    
    if (res.clicked && res.y > 260) { // Alan genişletildi
        currentState = STATE_MENU;
        appLoaded = false; // Çıkarken durumu sıfırla
        needsRedraw = true;
        delay(200);
    }

    if (res.tbPress) {
        currentState = STATE_MENU;
        appLoaded = false;
        needsRedraw = true;
        delay(200);
    }
}