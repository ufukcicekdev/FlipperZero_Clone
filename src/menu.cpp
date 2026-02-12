#include "menu.h"
#include "icons.h" // İkonları dahil et

// GERÇEK TANIMLAMA BURADA (Hata veren kısım düzeltildi)
// "SISTEM" kaldırıldı, "AYARLAR" kaldı
const MenuItem menuItems[] = {
    {"MUZIK", 0x780F, icon_music}, {"FOTO", 0x07E0, icon_photo}, {"NFC", 0x051D, icon_nfc}, {"NOTLAR", 0x7BEF, NULL},
    {"AYARLAR", 0x01CF, NULL}, {"GPIO", 0xFDA0, NULL}, {"SAAT", 0x07FF, NULL}, {"WEB", 0xF81F, NULL}, {"SD KART", 0xEFE0, NULL},
    {"OYUNLAR", 0x7800, NULL}, {"HAKKINDA", 0xFFFF, NULL}
};
const int totalItems = 11; // Sayı aynı kaldı çünkü VIDEO yerine NFC koydum (veya artırabilirsin)

void drawMenu(TFT_eSprite* spr, int scrollX, int selectedIdx) {
    spr->fillSprite(TFT_BLACK);
    for (int i = 0; i < totalItems; i++) {
        int page = i / 6;
        int localIdx = i % 6;
        int col = localIdx % 2;
        int row = localIdx / 2;
        
        int x = 15 + (col * 115) + (page * 240) - scrollX;
        int y = 25 + (row * 85);

        if (x > -110 && x < 240) {
            spr->fillRoundRect(x, y, 100, 75, 10, menuItems[i].color);
            
            if (menuItems[i].icon != NULL) {
                // İkon varsa çiz (Siyah [0x0000] rengi şeffaf yap)
                // İkonu ortalamak için: (100 - 48) / 2 = 26
                spr->pushImage(x + 26, y + 5, ICON_W, ICON_H, (uint16_t*)menuItems[i].icon, (uint16_t)0x0000);
                spr->setTextColor(TFT_WHITE);
                spr->drawCentreString(menuItems[i].name, x + 50, y + 58, 2); // Yazıyı alta kaydır
            } else {
                spr->setTextColor(TFT_WHITE);
                spr->drawCentreString(menuItems[i].name, x + 50, y + 30, 2);
            }

            // Eğer bu ikon seçiliyse etrafına çerçeve çiz
            if (i == selectedIdx) {
                spr->drawRoundRect(x-2, y-2, 104, 79, 12, TFT_WHITE);
                spr->drawRoundRect(x-3, y-3, 106, 81, 12, TFT_WHITE); // Kalınlık için
            }
        }
    }

    // --- ALT OKLAR (Navigasyon) ---
    spr->setTextColor(TFT_WHITE);
    // Sol Ok (Eğer en başta değilsek çiz)
    if (scrollX > 0) {
        spr->fillTriangle(20, 295, 40, 285, 40, 305, TFT_WHITE);
    }
    // Sağ Ok (Eğer daha fazla sayfa varsa çiz)
    int maxScroll = ((totalItems - 1) / 6) * 240;
    if (scrollX < maxScroll) {
        spr->fillTriangle(220, 295, 200, 285, 200, 305, TFT_WHITE);
    }
}

int getMenuIndex(int x, int y, int scrollX) {
    // 1. Ekranın en üstü ve en altına dokunulduysa menü geçişini iptal et
    if (y < 20 || y > 260) return -1; // Alt sınır: Çıkış butonu alanı (y > 260)

    // 2. Hangi sayfadayız? (Kaydırma miktarına göre sayfa hesabı)
    int page = (scrollX + 120) / 240;
    
    // 3. SÜTUN: Ekranın sol yarısı mı (0-120), sağ yarısı mı (120-240)?
    int col = (x < 120) ? 0 : 1;
    
    // 4. SATIR: Ekranı dikeyde 3 eşit parçaya böldük
    int row = -1;
    if (y >= 20 && y < 105) row = 0;      // ÜST SATIR (Çizim: 25-100)
    else if (y >= 105 && y < 190) row = 1; // ORTA SATIR (Çizim: 110-185)
    else if (y >= 190 && y < 260) row = 2; // ALT SATIR (Çizim: 195-270)

    if (row == -1) return -1;

    // 5. İNDEKS HESABI
    int idx = (page * 6) + (row * 2) + col;
    
    // Debug: Terminalden neyi yanlış anladığını görelim
    // Serial.printf("Dokunulan -> X:%d Y:%d | Sayfa:%d Col:%d Row:%d | Hedef Index:%d\n", x, y, page, col, row, idx);

    return (idx >= 0 && idx < totalItems) ? idx : -1;
}

void drawAppScreen(TFT_eSprite* spr, String title, uint16_t color, String msg, bool isPressed) {
    spr->fillSprite(TFT_BLACK);
    spr->fillRect(0, 0, 240, 50, color);
    spr->setTextColor(TFT_WHITE);
    spr->drawCentreString(title, 120, 15, 4);

    if (msg != "") {
        // Çok satırlı mesaj desteği (Satır satır bölüp ortalayarak yaz)
        int y = 110; // Başlangıç Y konumu
        int start = 0;
        int end = msg.indexOf('\n');
        while (end != -1) {
            spr->drawCentreString(msg.substring(start, end), 120, y, 2);
            y += 20; // Satır aralığı
            start = end + 1;
            end = msg.indexOf('\n', start);
        }
        spr->drawCentreString(msg.substring(start), 120, y, 2);
    }

    // Standart Çıkış Butonu
    if (isPressed) {
        spr->fillRoundRect(60, 285, 120, 30, 5, TFT_WHITE);
        spr->setTextColor(TFT_RED, TFT_WHITE);
    } else {
        spr->fillRoundRect(60, 285, 120, 30, 5, TFT_RED);
        spr->setTextColor(TFT_WHITE, TFT_RED);
    }
    spr->drawCentreString("CIKIS", 120, 297, 2);
}