#include "app_photo.h"
#include "sd_browser.h"
#include "menu.h"
#include "sound.h"
#include <TJpg_Decoder.h>
#include <PNGdec.h>
#include <SD.h> // SD kütüphanesini açıkça ekle

static int selectedFileIndex = 0;
static bool isViewing = false; // Görüntüleme modunda mıyız?
static TFT_eSprite* pSpr = nullptr; // Callback için sprite işaretçisi

// PNG Çizim Ofsetleri
static int16_t png_x_offset = 0;
static int16_t png_y_offset = 0;

// JPEG Dekoder Callback Fonksiyonu
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap){
   if (pSpr == nullptr) return 0;
   if ( y >= pSpr->height() ) return 0;
   pSpr->pushImage(x, y, w, h, bitmap);
   return 1;
}

// PNG Dekoder Değişkenleri ve Fonksiyonları
static PNG png;

int pngDraw(PNGDRAW *pDraw) {
  if (pSpr) {
    uint16_t usPixels[pDraw->iWidth];
    png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
    pSpr->pushImage(png_x_offset, png_y_offset + pDraw->y, pDraw->iWidth, 1, usPixels);
  }
  return 1;
}

void updatePhotoApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState, int& scrollY) {
    pSpr = spr; // Sprite işaretçisini güncelle

    if (!appLoaded) {
        // Dekoder ayarları
        TJpgDec.setJpgScale(1);
        TJpgDec.setSwapBytes(true); // RENK DUZELTMESI: Genellikle true olmalidir
        TJpgDec.setCallback(tft_output);

        // IMAGES klasöründeki resim dosyalarını yükle
        loadFilesByExtension("/IMAGES", ".jpg", true);  // İlkinde listeyi temizle
        loadFilesByExtension("/IMAGES", ".jpeg", false); // Sonrakilerde ekle
        loadFilesByExtension("/IMAGES", ".png", false);
        loadFilesByExtension("/IMAGES", ".bmp", false);
        
        selectedFileIndex = 0;
        isViewing = false;
        appLoaded = true;
        scrollY = 0;
        needsRedraw = true;
    }

    if (fileList.empty()) {
        // Dosya yoksa uyarı
        if (needsRedraw) {
            drawAppScreen(spr, "FOTO", 0x07E0, "IMAGES Klasoru Bos\nveya SD Kart Yok");
            spr->pushSprite(0, 0);
            needsRedraw = false;
        }
        // Çıkış
        if ((res.clicked && res.y > 260) || res.tbPress) {
            playSystemSound(SOUND_BACK);
            currentState = STATE_MENU;
            appLoaded = false;
            needsRedraw = true;
            delay(200);
        }
    } 
    else if (isViewing) {
        // --- GÖRÜNTÜLEME MODU ---
        
        if (needsRedraw) {
            Serial.println("--- Goruntuleme Modu Cizimi Basliyor ---");
            // Önce siyah ekran ve yükleniyor mesajı
            spr->fillSprite(TFT_BLACK);
            spr->setTextColor(TFT_WHITE, TFT_BLACK);
            spr->drawCentreString("Yukleniyor...", 120, 140, 2);
            spr->pushSprite(0, 0);
            Serial.println("Yukleniyor ekrani basildi.");

            String path = "/IMAGES/" + fileList[selectedFileIndex];
            
            // Dosya var mı kontrol et
            Serial.print("Dosya yolu: ");
            Serial.println(path);
            if (!SD.exists(path)) {
                spr->fillSprite(TFT_BLACK);
                spr->setTextColor(TFT_RED, TFT_BLACK);
                spr->drawCentreString("Dosya Bulunamadi!", 120, 140, 2);
                spr->pushSprite(0, 0);
                Serial.println("Dosya bulunamadi!");
            }
            else if (path.endsWith(".jpg") || path.endsWith(".jpeg") || path.endsWith(".JPG") || path.endsWith(".JPEG")) {
                // Resmi çiz (drawFsJpg kullanarak SD nesnesini açıkça belirtiyoruz)
                Serial.println("JPG Yukleniyor...");
                
                // Dosyayı belleğe oku (SD uyumluluğu için en garanti yöntem)
                File jpgFile = SD.open(path);
                if (jpgFile) {
                    size_t jpgSize = jpgFile.size();
                    uint8_t *jpgBuf = (uint8_t*)malloc(jpgSize);
                    if (jpgBuf) {
                        jpgFile.read(jpgBuf, jpgSize);
                        jpgFile.close();
                        
                        uint16_t w = 0, h = 0;
                        if (TJpgDec.getJpgSize(&w, &h, jpgBuf, jpgSize) == 0) {

                            // 2. Ölçekleme Faktörünü Hesapla (1, 2, 4, 8)
                            uint8_t scale = 1;
                            if (w > 0 && h > 0) {
                                // Ekran: 240x290 (Alt bar hariç)
                                if (w > 240 || h > 290) scale = 2;
                                if (w > 480 || h > 580) scale = 4;
                                if (w > 960 || h > 1160) scale = 8;
                            }
                            TJpgDec.setJpgScale(scale);

                            // 3. Ortalama Pozisyonunu Hesapla
                            int16_t scaled_w = w / scale;
                            int16_t scaled_h = h / scale;
                            int16_t x_pos = (240 - scaled_w) / 2;
                            int16_t y_pos = (290 - scaled_h) / 2;
                            
                            if (x_pos < 0) x_pos = 0;
                            if (y_pos < 0) y_pos = 0;

                            Serial.printf("Cizim Konumu: X=%d Y=%d (Boyut: %dx%d)\n", x_pos, y_pos, scaled_w, scaled_h);

                            TJpgDec.drawJpg(x_pos, y_pos, jpgBuf, jpgSize);
                        } else {
                            Serial.println("JPG Boyutu Okunamadi!");
                            spr->setTextColor(TFT_RED, TFT_BLACK);
                            spr->drawCentreString("JPG Hata", 120, 160, 2);
                        }
                        free(jpgBuf);
                    } else {
                        Serial.println("Bellek Yetersiz!");
                        jpgFile.close();
                        spr->setTextColor(TFT_RED, TFT_BLACK);
                        spr->drawCentreString("Bellek Dolu", 120, 160, 2);
                    }
                } else {
                    Serial.println("Dosya Acilamadi!");
                    spr->setTextColor(TFT_RED, TFT_BLACK);
                    spr->drawCentreString("Dosya Hatasi", 120, 160, 2);
                }
            } else if (path.endsWith(".png") || path.endsWith(".PNG")) {
                Serial.println("PNG Cizimi Baslatiliyor...");
                
                // Dosyayı belleğe oku (SD uyumluluğu için en garanti yöntem)
                File pngFile = SD.open(path);
                if (pngFile) {
                    size_t pngSize = pngFile.size();
                    uint8_t *pngBuf = (uint8_t*)malloc(pngSize);
                    if (pngBuf) {
                        pngFile.read(pngBuf, pngSize);
                        pngFile.close();
                        
                        int16_t rc = png.openRAM(pngBuf, pngSize, pngDraw);
                        if (rc == PNG_SUCCESS) {
                            // PNG Ortalama
                            int32_t w = png.getWidth();
                            int32_t h = png.getHeight();
                            png_x_offset = (240 - w) / 2;
                            png_y_offset = (290 - h) / 2;
                            if (png_x_offset < 0) png_x_offset = 0;
                            if (png_y_offset < 0) png_y_offset = 0;

                            png.decode(NULL, 0);
                            png.close();
                        } else {
                            spr->setTextColor(TFT_RED, TFT_BLACK);
                            spr->drawCentreString("PNG Decode Error!", 120, 160, 2);
                            String err = "Err: " + String(rc);
                            spr->drawCentreString(err, 120, 180, 2);
                        }
                        free(pngBuf);
                    } else {
                        Serial.println("Bellek Yetersiz!");
                        pngFile.close();
                        spr->setTextColor(TFT_RED, TFT_BLACK);
                        spr->drawCentreString("Bellek Dolu", 120, 160, 2);
                    }
                } else {
                    Serial.println("Dosya Acilamadi!");
                    spr->setTextColor(TFT_RED, TFT_BLACK);
                    spr->drawCentreString("Dosya Hatasi", 120, 160, 2);
                }
            } else {
                spr->fillSprite(TFT_BLACK);
                spr->setTextColor(TFT_WHITE);


                spr->drawCentreString("Format Desteklenmiyor", 120, 140, 2);
                spr->drawCentreString(fileList[selectedFileIndex], 120, 160, 2);
            }
            
            // Alt bilgi (Dosya adı)
            spr->fillRect(0, 290, 240, 30, 0x0000);
            spr->setTextColor(TFT_WHITE, 0x0000);
            spr->drawCentreString(fileList[selectedFileIndex], 120, 297, 2);

            spr->pushSprite(0, 0);
            needsRedraw = false;
            // Serial.println("--- Cizim Tamamlandi ---");
        }

        // Kontroller (Geri Dön veya Sonraki Resim)
        if (res.tbPress || res.clicked) {
            isViewing = false; // Listeye dön
            needsRedraw = true;
            delay(200);
        }
        if (res.tbRight) {
            selectedFileIndex++;
            if (selectedFileIndex >= (int)fileList.size()) selectedFileIndex = 0;
            needsRedraw = true;
        }
        if (res.tbLeft) {
            selectedFileIndex--;
            if (selectedFileIndex < 0) selectedFileIndex = (int)fileList.size() - 1;
            needsRedraw = true;
        }
    }
    else {
        // --- LİSTE NAVİGASYONU ---
        if (res.tbDown) {
            selectedFileIndex++;
            if (selectedFileIndex >= (int)fileList.size()) selectedFileIndex = 0;
            if ((selectedFileIndex * 35) > (scrollY + 200)) scrollY = (selectedFileIndex * 35) - 200;
            if (scrollY < 0) scrollY = 0;
            needsRedraw = true;
        }
        if (res.tbUp) {
            selectedFileIndex--;
            if (selectedFileIndex < 0) selectedFileIndex = (int)fileList.size() - 1;
            if ((selectedFileIndex * 35) < scrollY) scrollY = selectedFileIndex * 35;
            needsRedraw = true;
        }

        // Dokunmatik Kaydırma
        if (res.dragging) {
            scrollY += res.scrollY;
            if (scrollY < 0) scrollY = 0;
            int maxScroll = (fileList.size() * 35) - 200;
            if (maxScroll < 0) maxScroll = 0;
            if (scrollY > maxScroll) scrollY = maxScroll;
            needsRedraw = true;
        }

        // Dokunmatik Seçim
        if (res.clicked) {
            // Serial.printf("FOTO Tiklama: X=%d Y=%d\n", res.x, res.y);
            if (res.y < 260) {
                int clickedIndex = (res.y - 10 + scrollY) / 35;
                // Serial.printf("Hesaplanan Index: %d (Liste Boyutu: %d)\n", clickedIndex, (int)fileList.size());
                if (clickedIndex >= 0 && clickedIndex < (int)fileList.size()) {
                    // Serial.println("Gecerli resim secildi, goruntuleme moduna geciliyor.");
                    selectedFileIndex = clickedIndex;
                    isViewing = true; // Görüntüleme moduna geç
                    needsRedraw = true;
                    return; // ÖNEMLİ: Fonksiyondan çık, yoksa aşağıdaki çizim kodu needsRedraw'ı false yapar!
                }
            }
        }
        
        // Trackball ile Seçim
        if (res.tbPress) {
            isViewing = true;
            needsRedraw = true;
            delay(200);
            return; // ÖNEMLİ: Fonksiyondan çık
        }

        // Çizim
        if (needsRedraw) {
            drawSDCardBrowser(spr, selectedFileIndex, scrollY);
            spr->pushSprite(0, 0);
            needsRedraw = false;
        }

        // Çıkış
        if (res.clicked && res.y > 260) {
            currentState = STATE_MENU;
            appLoaded = false;
            needsRedraw = true;
            delay(200);
        }
    }
}