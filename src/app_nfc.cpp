#include "app_nfc.h"
#include "menu.h"
#include "sound.h"
#include "sd_browser.h"
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <SD.h>

// Global NFC Nesnesi
// IRQ ve RST pinleri config.h'da -1 ise kütüphane bunları kullanmaz (0xFF olarak gider)
static Adafruit_PN532 nfc(NFC_IRQ, NFC_RST);
static bool nfcInitialized = false;

// Uygulama Durumları
enum NFCState { NFC_MENU, NFC_READ_WAIT, NFC_WRITE_SELECT, NFC_WRITE_WAIT, NFC_RESULT };
static NFCState nfcState = NFC_MENU;
static String statusMsg = "";
static String writeData = ""; // Yazılacak veri
static int selectedFileIndex = 0;

void updateNFCApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState, int& scrollY) {
    if (!appLoaded) {
        // I2C Başlat
        Wire.begin(NFC_SDA, NFC_SCL);
        nfc.begin();
        
        uint32_t versiondata = nfc.getFirmwareVersion();
        if (!versiondata) {
            statusMsg = "PN532 Bulunamadi!";
            nfcInitialized = false;
        } else {
            nfc.SAMConfig(); // Kart okuma modu
            statusMsg = "PN532 Hazir";
            nfcInitialized = true;
        }
        
        // SD Kartta klasör oluştur
        if (!isSDMounted) initSD(); // Kart sonradan takıldıysa veya koptuysa tekrar dene
        
        if (isSDMounted) {
            if (!SD.exists("/NFC")) SD.mkdir("/NFC");
            if (!SD.exists("/NFC/READ")) SD.mkdir("/NFC/READ");
        }

        nfcState = NFC_MENU;
        appLoaded = true;
        needsRedraw = true;
    }

    // --- MENÜ MODU ---
    if (nfcState == NFC_MENU) {
        if (needsRedraw) {
            drawAppScreen(spr, "NFC", 0x051D, statusMsg);
            
            // Butonlar
            spr->fillRoundRect(20, 140, 200, 40, 5, 0x07E0); // Yeşil
            spr->setTextColor(TFT_BLACK, 0x07E0);
            spr->drawCentreString("OKU & KAYDET", 120, 150, 2);

            spr->fillRoundRect(20, 200, 200, 40, 5, 0xF800); // Kırmızı
            spr->setTextColor(TFT_WHITE, 0xF800);
            spr->drawCentreString("KAYITLIYI YAZ", 120, 210, 2);

            spr->pushSprite(0, 0);
            needsRedraw = false;
        }

        if (res.clicked) {
            if (res.y >= 140 && res.y < 190) { // Oku
                nfcState = NFC_READ_WAIT;
                statusMsg = "Kart Yaklastirin...";
                needsRedraw = true;
            }
            else if (res.y >= 200 && res.y < 250) { // Yaz
                loadFilesByExtension("/NFC/READ", ".txt"); // Kayıtlıları listele
                nfcState = NFC_WRITE_SELECT;
                scrollY = 0;
                selectedFileIndex = 0;
                needsRedraw = true;
            }
        }
    }
    // --- OKUMA MODU ---
    else if (nfcState == NFC_READ_WAIT) {
        if (needsRedraw) {
            drawAppScreen(spr, "NFC OKU", 0x07E0, statusMsg);
            spr->pushSprite(0, 0);
            needsRedraw = false;
        }

        // Kart Tara
        uint8_t success;
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
        uint8_t uidLength;
        
        // Bloklamasız okuma için timeout'u kısa tutabiliriz ama kütüphane blokluyor.
        // Basitlik için burada bekliyoruz (loop döngüsünü biraz geciktirir)
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);

        if (success) {
            playSystemSound(SOUND_ENTER);
            String uidStr = "";
            for (uint8_t i = 0; i < uidLength; i++) {
                if(uid[i] < 0x10) uidStr += "0";
                uidStr += String(uid[i], HEX);
            }
            uidStr.toUpperCase();

            // Veri Okuma (Mifare Classic Blok 4 - Örnek)
            uint8_t data[16];
            String dataStr = "Bos";
            if (uidLength == 4) { // Mifare Classic genelde 4 byte UID
                if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF")) {
                    if (nfc.mifareclassic_ReadDataBlock(4, data)) {
                        dataStr = "";
                        for(int i=0; i<16; i++) {
                            if (data[i] >= 32 && data[i] <= 126) dataStr += (char)data[i];
                            else dataStr += ".";
                        }
                    }
                }
            }

            // SD Karta Kaydet
            if (isSDMounted) {
                File f = SD.open("/NFC/READ/" + uidStr + ".txt", FILE_WRITE);
                if (f) {
                    if (f.println(dataStr) > 0) {
                        f.flush(); // Veriyi karta yazmaya zorla
                        statusMsg = "Kaydedildi:\n" + uidStr;
                    } else {
                        statusMsg = "Yazma Basarisiz!";
                    }
                    f.close();
                } else {
                    statusMsg = "SD Yazma Hatasi!";
                }
            } else {
                statusMsg = "SD Kart Yok!";
            }
            
            nfcState = NFC_RESULT;
            needsRedraw = true;
        }
    }
    // --- YAZMA İÇİN DOSYA SEÇİMİ ---
    else if (nfcState == NFC_WRITE_SELECT) {
        // Liste Kontrolleri
        if (res.tbDown) { selectedFileIndex++; if(selectedFileIndex >= (int)fileList.size()) selectedFileIndex=0; needsRedraw=true; }
        if (res.tbUp) { selectedFileIndex--; if(selectedFileIndex < 0) selectedFileIndex=(int)fileList.size()-1; needsRedraw=true; }
        
        if (res.clicked && res.y < 260) {
            int idx = (res.y - 10 + scrollY) / 35;
            if (idx >= 0 && idx < (int)fileList.size()) {
                selectedFileIndex = idx;
                // Dosyayı Oku
                if (isSDMounted) {
                    File f = SD.open("/NFC/READ/" + fileList[selectedFileIndex]);
                    if (f) {
                        writeData = f.readStringUntil('\n');
                        writeData.trim(); // Boşlukları temizle
                        f.close();
                        nfcState = NFC_WRITE_WAIT;
                        statusMsg = "Kart Yaklastirin...\nYazilacak: " + writeData;
                        needsRedraw = true;
                    }
                } else {
                    statusMsg = "SD Kart Yok!";
                    needsRedraw = true;
                }
            }
        }

        if (needsRedraw) {
            if (fileList.empty()) {
                drawAppScreen(spr, "SECIM", 0xF800, "Dosya Bulunamadi\n/NFC/READ Bos");
            } else {
                drawSDCardBrowser(spr, selectedFileIndex, scrollY);
            }
            spr->pushSprite(0, 0);
            needsRedraw = false;
        }
    }
    // --- YAZMA MODU ---
    else if (nfcState == NFC_WRITE_WAIT) {
        if (needsRedraw) {
            drawAppScreen(spr, "NFC YAZ", 0xF800, statusMsg);
            spr->pushSprite(0, 0);
            needsRedraw = false;
        }

        uint8_t success;
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
        uint8_t uidLength;
        
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);

        if (success) {
            bool writeSuccess = false;
            if (uidLength == 4) {
                // Blok 4'e yazmayı dene
                if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF")) {
                    uint8_t data[16] = {0};
                    // Stringi byte dizisine çevir
                    for (int i = 0; i < 16 && i < writeData.length(); i++) {
                        data[i] = writeData[i];
                    }
                    if (nfc.mifareclassic_WriteDataBlock(4, data)) {
                        writeSuccess = true;
                    }
                }
            }

            if (writeSuccess) {
                playSystemSound(SOUND_ENTER);
                statusMsg = "Yazma Basarili!";
            } else {
                playSystemSound(SOUND_BACK); // Hata sesi
                statusMsg = "Yazma Hatasi!\nKart Desteklenmiyor";
            }
            nfcState = NFC_RESULT;
            needsRedraw = true;
        }
    }
    // --- SONUÇ EKRANI ---
    else if (nfcState == NFC_RESULT) {
        if (needsRedraw) {
            drawAppScreen(spr, "SONUC", 0x0000, statusMsg);
            spr->pushSprite(0, 0);
            needsRedraw = false;
        }
        // Tıklayınca menüye dön
        if (res.clicked || res.tbPress) {
            nfcState = NFC_MENU;
            statusMsg = "PN532 Hazir";
            needsRedraw = true;
            delay(200);
        }
    }

    // Çıkış
    if (res.clicked && res.y > 260) {
        currentState = STATE_MENU;
        appLoaded = false;
        needsRedraw = true;
        delay(200);
    }
}
