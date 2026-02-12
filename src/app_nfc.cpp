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
enum NFCState { NFC_MENU, NFC_READ_WAIT, NFC_WRITE_SELECT, NFC_WRITE_WAIT, NFC_EMULATE_SELECT, NFC_EMULATE_WAIT, NFC_DELETE_SELECT, NFC_DELETE_CONFIRM, NFC_RESULT };
static NFCState nfcState = NFC_MENU;
static String statusMsg = "";
static String writeData = ""; // Yazılacak veri
static int selectedFileIndex = 0;
static unsigned long resultTimer = 0; // Sonuç ekranı için zamanlayıcı
static String currentNfcPath = "/NFC/READ"; // Geçerli dosya yolu
static int animRadius = 0; // Animasyon için sayaç

void updateNFCApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState, int& scrollY) {
    if (!appLoaded) {
        // I2C Başlat
        Wire.begin(NFC_SDA, NFC_SCL);
        Wire.setClock(100000); // I2C hızını 100kHz'e düşür (Hataları azaltır)
        Wire.setTimeOut(1000); // I2C Timeout süresini artır (263 hatası için)
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
            spr->fillRoundRect(20, 60, 200, 40, 5, 0x07E0); // Yeşil (OKU)
            spr->setTextColor(TFT_BLACK, 0x07E0);
            spr->drawCentreString("OKU & KAYDET", 120, 70, 2);

            spr->fillRoundRect(20, 110, 200, 40, 5, 0xF800); // Kırmızı (YAZ)
            spr->setTextColor(TFT_WHITE, 0xF800);
            spr->drawCentreString("KAYITLIYI YAZ", 120, 120, 2);

            spr->fillRoundRect(20, 160, 200, 40, 5, 0x001F); // Mavi (EMULE ET)
            spr->setTextColor(TFT_WHITE, 0x001F);
            spr->drawCentreString("EMULE ET", 120, 170, 2);

            spr->fillRoundRect(20, 210, 200, 40, 5, 0x8000); // Bordo (SIL)
            spr->setTextColor(TFT_WHITE, 0x8000);
            spr->drawCentreString("KAYIT SIL", 120, 220, 2);

            spr->pushSprite(0, 0);
            needsRedraw = false;
        }

        if (res.clicked) {
            if (res.y >= 60 && res.y < 100) { // Oku
                nfcState = NFC_READ_WAIT;
                statusMsg = "Kart Yaklastirin...";
                needsRedraw = true;
            }
            else if (res.y >= 110 && res.y < 150) { // Yaz
                delay(50);
                currentNfcPath = "/NFC/READ";
                loadFilesByExtension(currentNfcPath.c_str(), ".txt", true); 
                if (fileList.empty()) { // Eğer boşsa /NFC klasörüne de bak
                    currentNfcPath = "/NFC";
                    loadFilesByExtension(currentNfcPath.c_str(), ".txt", true);
                }
                nfcState = NFC_WRITE_SELECT;
                scrollY = 0;
                selectedFileIndex = 0;
                needsRedraw = true;
            }
            else if (res.y >= 160 && res.y < 200) { // Emule Et
                delay(50);
                currentNfcPath = "/NFC/READ";
                loadFilesByExtension(currentNfcPath.c_str(), ".txt", true);
                if (fileList.empty()) {
                    currentNfcPath = "/NFC";
                    loadFilesByExtension(currentNfcPath.c_str(), ".txt", true);
                }
                nfcState = NFC_EMULATE_SELECT;
                scrollY = 0;
                selectedFileIndex = 0;
                needsRedraw = true;
            }
            else if (res.y >= 210 && res.y < 250) { // Sil
                delay(50);
                currentNfcPath = "/NFC/READ";
                loadFilesByExtension(currentNfcPath.c_str(), ".txt", true);
                if (fileList.empty()) {
                    currentNfcPath = "/NFC";
                    loadFilesByExtension(currentNfcPath.c_str(), ".txt", true);
                }
                nfcState = NFC_DELETE_SELECT;
                scrollY = 0;
                selectedFileIndex = 0;
                needsRedraw = true;
            }
        }
    }
    // --- OKUMA MODU ---
    else if (nfcState == NFC_READ_WAIT) {
        // Animasyonlu Ekran (Sürekli Çizim)
        // Geri Butonu Kontrolü (Animasyon sırasında çıkış için)
        if (res.clicked && res.y > 260) {
            nfcState = NFC_MENU;
            statusMsg = "Islem Iptal";
            needsRedraw = true;
            delay(200);
            return; // Global çıkış koduna düşmemesi için return
        }

        spr->fillSprite(TFT_BLACK);
        drawAppScreen(spr, "NFC OKU", 0x07E0, "", false); // Mesajı elle çizeceğiz
        
        // Radar Animasyonu
        animRadius = (animRadius + 4) % 80;
        spr->drawCircle(120, 150, animRadius, 0x07E0); // Merkez halka
        spr->drawCircle(120, 150, (animRadius + 25) % 80, 0x03E0); // Orta halka
        spr->drawCircle(120, 150, (animRadius + 50) % 80, 0x01E0); // Dış halka
        spr->setTextColor(TFT_WHITE);
        spr->drawCentreString("Kart Araniyor...", 120, 240, 2);
        spr->pushSprite(0, 0);

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
                // Klasörlerin varlığından emin ol (Yoksa oluştur)
                if (!SD.exists("/NFC")) SD.mkdir("/NFC");
                if (!SD.exists("/NFC/READ")) SD.mkdir("/NFC/READ");

                // Benzersiz Dosya İsmi Oluştur
                String fileName = "/NFC/READ/" + uidStr + ".txt";
                int fileCount = 1;
                while (SD.exists(fileName)) {
                    fileName = "/NFC/READ/" + uidStr + "_" + String(fileCount) + ".txt";
                    fileCount++;
                }

                File f = SD.open(fileName, FILE_WRITE);
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
                    File f = SD.open(currentNfcPath + "/" + fileList[selectedFileIndex]);
                    if (f) {
                        writeData = f.readStringUntil('\n');
                        writeData.trim(); // Boşlukları temizle
                        f.close();
                        nfcState = NFC_WRITE_WAIT;
                        statusMsg = "Karti Yaklastirin\nVeri: " + writeData;
                        needsRedraw = true;
                    }
                } else {
                    statusMsg = "SD Kart Yok!";
                    needsRedraw = true;
                }
            }
        }
        
        // Geri Butonu (Alt Bar - Listeden Geri Dön)
        if (res.clicked && res.y > 260) {
            nfcState = NFC_MENU;
            statusMsg = "PN532 Hazir";
            needsRedraw = true;
            delay(200);
            return;
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
        // Animasyonlu Ekran (Sürekli Çizim)
        // Geri Butonu Kontrolü
        if (res.clicked && res.y > 260) {
            nfcState = NFC_MENU;
            statusMsg = "Islem Iptal";
            needsRedraw = true;
            delay(200);
            return;
        }

        spr->fillSprite(TFT_BLACK);
        drawAppScreen(spr, "NFC YAZ", 0xF800, "", false);
        
        // Radar Animasyonu (Kırmızı)
        animRadius = (animRadius + 4) % 80;
        spr->drawCircle(120, 150, animRadius, 0xF800);
        spr->drawCircle(120, 150, (animRadius + 25) % 80, 0xB800);
        spr->drawCircle(120, 150, (animRadius + 50) % 80, 0x7800);
        spr->setTextColor(TFT_WHITE);
        spr->drawCentreString("Karti Yaklastirin...", 120, 240, 2);
        spr->pushSprite(0, 0);

        uint8_t success;
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
        uint8_t uidLength;
        
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);

        if (success) {
            delay(50); // Yazma işlemi öncesi güç stabilizasyonu
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
    // --- EMÜLASYON İÇİN DOSYA SEÇİMİ ---
    else if (nfcState == NFC_EMULATE_SELECT) {
        // Liste Kontrolleri
        if (res.tbDown) { selectedFileIndex++; if(selectedFileIndex >= (int)fileList.size()) selectedFileIndex=0; needsRedraw=true; }
        if (res.tbUp) { selectedFileIndex--; if(selectedFileIndex < 0) selectedFileIndex=(int)fileList.size()-1; needsRedraw=true; }
        
        if (res.clicked && res.y < 260) {
            int idx = (res.y - 10 + scrollY) / 35;
            if (idx >= 0 && idx < (int)fileList.size()) {
                selectedFileIndex = idx;
                // Dosyadan UID'yi al (Dosya adı UID olduğu için direkt ismini kullanabiliriz)
                String fileName = fileList[selectedFileIndex];
                // Uzantıyı (.txt) kaldır
                int dotIndex = fileName.lastIndexOf('.');
                if (dotIndex != -1) fileName = fileName.substring(0, dotIndex);
                
                writeData = fileName; // UID'yi sakla
                nfcState = NFC_EMULATE_WAIT;
                statusMsg = "Emulasyon Basliyor...";
                needsRedraw = true;
            }
        }
        
        // Geri Butonu
        if (res.clicked && res.y > 260) {
            nfcState = NFC_MENU;
            statusMsg = "PN532 Hazir";
            needsRedraw = true;
            delay(200);
            return;
        }

        if (needsRedraw) {
            if (fileList.empty()) {
                drawAppScreen(spr, "SECIM", 0x001F, "Dosya Bulunamadi\n/NFC/READ Bos");
            } else {
                drawSDCardBrowser(spr, selectedFileIndex, scrollY);
            }
            spr->pushSprite(0, 0);
            needsRedraw = false;
        }
    }
    // --- EMÜLASYON MODU (Cihaz Kart Gibi Davranır) ---
    else if (nfcState == NFC_EMULATE_WAIT) {
        // Geri Butonu Kontrolü
        if (res.clicked && res.y > 260) {
            nfcState = NFC_MENU;
            statusMsg = "Emulasyon Durduruldu";
            needsRedraw = true;
            delay(200);
            return;
        }

        // Animasyonu yavaşlat ve NFC'ye öncelik ver (Ekran çizimi NFC zamanlamasını bozabilir)
        static unsigned long lastDrawTime = 0;
        if (millis() - lastDrawTime > 500) { // 500ms'de bir güncelle (NFC'ye daha çok zaman kalsın)
            lastDrawTime = millis();
            
            spr->fillSprite(TFT_BLACK);
            drawAppScreen(spr, "EMULASYON", 0x001F, "", false);
            
            // Mavi Radar Animasyonu
            animRadius = (animRadius + 4) % 80;
            spr->drawCircle(120, 150, animRadius, 0x001F);
            spr->drawCircle(120, 150, (animRadius + 25) % 80, 0x0010);
            spr->drawCircle(120, 150, (animRadius + 50) % 80, 0x000F);
            
            spr->setTextColor(TFT_WHITE);
            spr->drawCentreString("YAYIN YAPILIYOR", 120, 220, 2);
            spr->setTextColor(TFT_YELLOW);
            spr->drawCentreString("UID: " + writeData, 120, 240, 2);
            
            spr->pushSprite(0, 0);
        }

        // PN532'yi Hedef (Target) moduna al
        // NOT: Standart kütüphanede UID ayarlamak zordur, genellikle rastgele veya sabit UID yayar.
        // Ancak kapı okuyucusu sadece varlık kontrolü yapıyorsa veya Magic Card komutlarını
        // simüle ediyorsak bu işe yarayabilir.
        // AsTarget fonksiyonu okuyucu gelene kadar bloklayabilir, bu yüzden timeout önemlidir.
        
        // Burada basitçe hedef modunu deniyoruz.
        // Eğer okuyucu cihaza veri gönderirse success true döner.
        uint8_t command[] = {0, 0}; // Gelen komut için buffer
        uint8_t cmdLen = sizeof(command);
        
        // Emülasyon şansını artırmak için arka arkaya birkaç kez dene
        // Telefon sinyali gönderdiğinde o an dinliyor olmamız lazım.
        bool success = false;
        for(int i=0; i<3; i++) { 
            if (nfc.AsTarget()) {
                success = true;
                break;
            }
            delay(10); // Kısa bekleme
        }

        if (success) {
            // GÖRSEL GERİ BİLDİRİM (BAŞARILI)
            spr->fillSprite(TFT_BLACK);
            drawAppScreen(spr, "EMULASYON", 0x07E0, "OKUYUCU BAGLANDI!", false); // Yeşil Başlık
            spr->fillCircle(120, 150, 50, 0x07E0); // Yeşil Daire
            spr->setTextColor(TFT_BLACK);
            spr->drawCentreString("OKUNDU", 120, 142, 2);
            spr->pushSprite(0, 0);

            playSystemSound(SOUND_ENTER); // Başarı sesi
            delay(1000); // Kullanıcının görmesi için bekle
        }
    }
    // --- SİLME MODU (Dosya Seçimi) ---
    else if (nfcState == NFC_DELETE_SELECT) {
        if (res.tbDown) { selectedFileIndex++; if(selectedFileIndex >= (int)fileList.size()) selectedFileIndex=0; needsRedraw=true; }
        if (res.tbUp) { selectedFileIndex--; if(selectedFileIndex < 0) selectedFileIndex=(int)fileList.size()-1; needsRedraw=true; }
        
        if (res.clicked && res.y < 260) {
            int idx = (res.y - 10 + scrollY) / 35;
            if (idx >= 0 && idx < (int)fileList.size()) {
                selectedFileIndex = idx;
                nfcState = NFC_DELETE_CONFIRM;
                needsRedraw = true;
            }
        }
        
        if (res.clicked && res.y > 260) {
            nfcState = NFC_MENU;
            statusMsg = "PN532 Hazir";
            needsRedraw = true;
            delay(200);
            return;
        }

        if (needsRedraw) {
            if (fileList.empty()) {
                drawAppScreen(spr, "SIL", 0x8000, "Dosya Bulunamadi");
            } else {
                drawSDCardBrowser(spr, selectedFileIndex, scrollY);
            }
            spr->pushSprite(0, 0);
            needsRedraw = false;
        }
    }
    // --- SİLME ONAY ---
    else if (nfcState == NFC_DELETE_CONFIRM) {
        if (needsRedraw) {
            spr->fillSprite(TFT_BLACK);
            drawAppScreen(spr, "SILINSIN MI?", 0x8000, fileList[selectedFileIndex], false);
            
            // EVET Butonu
            spr->fillRoundRect(20, 180, 90, 40, 5, 0x07E0);
            spr->setTextColor(TFT_BLACK, 0x07E0);
            spr->drawCentreString("EVET", 65, 190, 2);

            // HAYIR Butonu
            spr->fillRoundRect(130, 180, 90, 40, 5, 0xF800);
            spr->setTextColor(TFT_WHITE, 0xF800);
            spr->drawCentreString("HAYIR", 175, 190, 2);

            spr->pushSprite(0, 0);
            needsRedraw = false;
        }

        if (res.clicked) {
            if (res.y >= 180 && res.y < 220) {
                if (res.x < 120) { // EVET
                    String fullPath = currentNfcPath + "/" + fileList[selectedFileIndex];
                    if (SD.remove(fullPath)) {
                        statusMsg = "Dosya Silindi";
                    } else {
                        statusMsg = "Silme Hatasi!";
                    }
                    nfcState = NFC_RESULT;
                    needsRedraw = true;
                } else { // HAYIR
                    nfcState = NFC_DELETE_SELECT;
                    needsRedraw = true;
                }
            }
        }
    }
    // --- SONUÇ EKRANI ---
    else if (nfcState == NFC_RESULT) {
        if (needsRedraw) {
            drawAppScreen(spr, "SONUC", 0x0000, statusMsg);
            spr->pushSprite(0, 0);
            needsRedraw = false;
            resultTimer = millis(); // Zamanlayıcıyı başlat
        }
        // 3 saniye sonra veya tıklayınca menüye dön
        if (millis() - resultTimer > 3000 || res.clicked || res.tbPress) {
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
