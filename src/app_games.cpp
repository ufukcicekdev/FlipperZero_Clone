#include "app_games.h"
#include "menu.h"
#include "sound.h"
#include <vector>

// Oyun Uygulaması Durumları
enum GameAppState {
    GAMES_MENU,
    GAMES_FLAPPY_INIT,
    GAMES_FLAPPY_PLAY,
    GAMES_FLAPPY_PAUSED,
    GAMES_FLAPPY_GAMEOVER
};

static GameAppState gameState = GAMES_MENU;
static int selectedGameIdx = 0;
static int pauseMenuSel = 0;
static const char* gameList[] = { "Flappy Bird", "Yilan (Yakinda)" };
static const int gameCount = 2;

// --- FLAPPY BIRD DEĞİŞKENLERİ ---
static float birdY = 160;
static float birdV = 0;
static const float gravity = 0.6;
static const float jumpForce = -7.0;
static const int birdX = 60;
static const int birdRadius = 8;

struct Pipe {
    int x;
    int gapY; // Boşluğun üst kenarı (veya merkezi)
    bool passed;
};

static std::vector<Pipe> pipes;
static int score = 0;
static int highScore = 0;
static int pipeSpeed = 3;
static int pipeInterval = 140; // Borular arası mesafe
static int pipeGapSize = 90;   // Boru açıklığı
static int pipeWidth = 36;
static bool lastDragging = false; // Dokunmatik algılama için

void updateGamesApp(TFT_eSprite* spr, ControlResult& res, bool& appLoaded, bool& needsRedraw, AppState& currentState) {
    // --- BAŞLANGIÇ ---
    if (!appLoaded) {
        gameState = GAMES_MENU;
        selectedGameIdx = 0;
        appLoaded = true;
        needsRedraw = true;
    }

    // ============================================================
    // DURUM 1: OYUN MENÜSÜ (LİSTE)
    // ============================================================
    if (gameState == GAMES_MENU) {
        // Navigasyon
        if (res.tbDown) {
            selectedGameIdx++;
            if (selectedGameIdx >= gameCount) selectedGameIdx = 0;
            playSystemSound(SOUND_SCROLL);
            needsRedraw = true;
        }
        if (res.tbUp) {
            selectedGameIdx--;
            if (selectedGameIdx < 0) selectedGameIdx = gameCount - 1;
            playSystemSound(SOUND_SCROLL);
            needsRedraw = true;
        }

        // Seçim
        if (res.tbPress || res.clicked) {
            if (res.clicked && res.y > 260) {
                // Çıkış butonuna tıklandıysa aşağıda işlenecek
            } else {
                if (selectedGameIdx == 0) { // Flappy Bird
                    gameState = GAMES_FLAPPY_INIT;
                    playSystemSound(SOUND_ENTER);
                } else {
                    // Diğer oyunlar (Placeholder)
                    playSystemSound(SOUND_BACK); 
                }
                needsRedraw = true;
                delay(200);
            }
        }

        // Çizim
        if (needsRedraw) {
            spr->fillSprite(TFT_BLACK);
            drawAppScreen(spr, "OYUNLAR", 0x7800, "", false);

            for (int i = 0; i < gameCount; i++) {
                int y = 80 + (i * 50);
                uint16_t color = (i == selectedGameIdx) ? TFT_YELLOW : TFT_WHITE;
                
                spr->fillRoundRect(20, y, 200, 40, 5, 0x2104); // Kutu
                if (i == selectedGameIdx) spr->drawRoundRect(20, y, 200, 40, 5, TFT_WHITE);
                
                spr->setTextColor(color, 0x2104);
                spr->drawCentreString(gameList[i], 120, y + 12, 2);
            }

            spr->pushSprite(0, 0);
            needsRedraw = false;
        }

        // Çıkış
        if ((res.clicked && res.y > 260) || res.tbLeft) {
            currentState = STATE_MENU;
            appLoaded = false;
            needsRedraw = true;
            playSystemSound(SOUND_BACK);
            delay(200);
        }
    }

    // ============================================================
    // DURUM 2: FLAPPY BIRD BAŞLATMA
    // ============================================================
    else if (gameState == GAMES_FLAPPY_INIT) {
        birdY = 160;
        birdV = 0;
        score = 0;
        pipes.clear();
        // İlk boruyu ekle
        pipes.push_back({300, 100 + random(100), false});
        
        gameState = GAMES_FLAPPY_PLAY;
        needsRedraw = true;
    }

    // ============================================================
    // DURUM 3: FLAPPY BIRD OYNANIŞ
    // ============================================================
    else if (gameState == GAMES_FLAPPY_PLAY) {
        // --- GİRİŞ KONTROLÜ (ZIPLAMA) ---
        // Oyundan Çıkış (Sol Tuş ile Menüye Dön)
        if (res.tbLeft) {
            gameState = GAMES_FLAPPY_PAUSED;
            pauseMenuSel = 0; // Varsayılan: Devam Et
            needsRedraw = true;
            playSystemSound(SOUND_ENTER);
            delay(200);
            return;
        }

        // Dokunmatik "basıldığı anı" yakalamak için
        bool touchDown = (res.dragging && !lastDragging);
        lastDragging = res.dragging;

        if (res.tbPress || res.tbUp || res.clicked || touchDown) {
            birdV = jumpForce;
            // playTone(400, 50); // Zıplama sesi (İsteğe bağlı)
        }

        // --- FİZİK ---
        birdV += gravity;
        birdY += birdV;

        // --- BORU HAREKETİ VE OLUŞUMU ---
        for (auto& p : pipes) {
            p.x -= pipeSpeed;
        }

        // Ekrandan çıkan boruları sil
        if (!pipes.empty() && pipes[0].x < -pipeWidth) {
            pipes.erase(pipes.begin());
        }

        // Yeni boru ekle
        if (pipes.back().x < (240 - pipeInterval)) {
            int gapY = 50 + random(150); // Boşluğun Y konumu (50 ile 200 arası)
            pipes.push_back({240, gapY, false});
        }

        // --- ÇARPIŞMA KONTROLÜ ---
        bool collision = false;
        
        // 1. Zemin ve Tavan
        if (birdY > 280 || birdY < 0) collision = true;

        // 2. Borular
        for (auto& p : pipes) {
            // Borunun X aralığında mıyız?
            if ((birdX + birdRadius > p.x) && (birdX - birdRadius < p.x + pipeWidth)) {
                // Borunun Y aralığında mıyız? (Boşlukta DEĞİLSEK çarptık demektir)
                // Üst boru: 0'dan gapY'ye kadar
                // Alt boru: gapY + pipeGapSize'dan aşağıya kadar
                if ((birdY - birdRadius < p.gapY) || (birdY + birdRadius > p.gapY + pipeGapSize)) {
                    collision = true;
                }
            }
            
            // Skor
            if (!p.passed && birdX > p.x + pipeWidth) {
                score++;
                p.passed = true;
                playTone(1000, 50); // Skor sesi
            }
        }

        if (collision) {
            playTone(150, 300); // Çarpma sesi
            if (score > highScore) highScore = score;
            gameState = GAMES_FLAPPY_GAMEOVER;
            delay(500); // Ölünce kısa bir bekleme
        }

        // --- ÇİZİM ---
        spr->fillSprite(0x661F); // Gökyüzü Mavisi (Arka plan)

        // Boruları Çiz
        for (auto& p : pipes) {
            // Üst Boru
            spr->fillRect(p.x, 0, pipeWidth, p.gapY, 0x07E0); // Yeşil
            spr->drawRect(p.x, 0, pipeWidth, p.gapY, 0x0000); // Siyah Çerçeve
            
            // Alt Boru
            int bottomPipeY = p.gapY + pipeGapSize;
            spr->fillRect(p.x, bottomPipeY, pipeWidth, 320 - bottomPipeY, 0x07E0);
            spr->drawRect(p.x, bottomPipeY, pipeWidth, 320 - bottomPipeY, 0x0000);
        }

        // Kuşu Çiz
        spr->fillCircle((int)birdX, (int)birdY, birdRadius, TFT_YELLOW);
        spr->drawCircle((int)birdX, (int)birdY, birdRadius, TFT_BLACK);
        // Göz
        spr->fillCircle((int)birdX + 4, (int)birdY - 2, 2, TFT_WHITE);
        spr->fillCircle((int)birdX + 5, (int)birdY - 2, 1, TFT_BLACK);
        // Gaga
        spr->fillTriangle((int)birdX+4, (int)birdY+2, (int)birdX+12, (int)birdY+4, (int)birdX+4, (int)birdY+6, 0xF800);

        // Zemin
        spr->fillRect(0, 290, 240, 30, 0xD69A); // Toprak rengi
        spr->drawFastHLine(0, 290, 240, 0x0000);

        // Skor
        spr->setTextColor(TFT_WHITE, TFT_BLACK);
        spr->setTextSize(2);
        spr->drawString(String(score), 110, 20);
        spr->setTextSize(1);

        // Sürekli güncelleme için
        spr->pushSprite(0, 0);
        needsRedraw = true; // Oyun döngüsü için sürekli true tutuyoruz
    }

    // ============================================================
    // DURUM 3.5: PAUSE MENU (DURAKLATMA)
    // ============================================================
    else if (gameState == GAMES_FLAPPY_PAUSED) {
        // Navigasyon
        if (res.tbUp || res.tbDown) {
            pauseMenuSel = !pauseMenuSel; // 0 ve 1 arasında geçiş
            playSystemSound(SOUND_SCROLL);
            needsRedraw = true;
        }

        // Seçim
        if (res.tbPress || res.clicked) {
            if (res.clicked) {
                // Tıklama alanlarını yeni kutu düzenine göre güncelledik
                if (res.y >= 120 && res.y < 155) pauseMenuSel = 0;
                else if (res.y >= 155 && res.y < 190) pauseMenuSel = 1;
            }

            if (pauseMenuSel == 0) { // Devam Et
                gameState = GAMES_FLAPPY_PLAY;
                needsRedraw = true;
            } else { // Çıkış
                gameState = GAMES_MENU;
                needsRedraw = true;
                playSystemSound(SOUND_BACK);
            }
            delay(200);
        }

        if (needsRedraw) {
            // Arka planı silmiyoruz (fillSprite YOK), böylece oyun altta görünür
            
            // Kutu Çizimi (Game Over stili)
            int boxX = 40, boxY = 80, boxW = 160, boxH = 120;
            spr->fillRoundRect(boxX, boxY, boxW, boxH, 10, 0x2104); // Koyu arka plan
            spr->drawRoundRect(boxX, boxY, boxW, boxH, 10, TFT_WHITE); // Çerçeve

            spr->setTextColor(TFT_WHITE, 0x2104);
            spr->drawCentreString("DURAKLATILDI", 120, boxY + 15, 2);

            // Devam Et Butonu
            if (pauseMenuSel == 0) {
                spr->fillRoundRect(boxX + 20, boxY + 45, boxW - 40, 25, 5, 0x07E0); // Yeşil
                spr->setTextColor(TFT_BLACK, 0x07E0);
            } else {
                spr->drawRoundRect(boxX + 20, boxY + 45, boxW - 40, 25, 5, TFT_WHITE);
                spr->setTextColor(TFT_WHITE, 0x2104);
            }
            spr->drawCentreString("DEVAM ET", 120, boxY + 50, 2);

            // Çıkış Butonu
            if (pauseMenuSel == 1) {
                spr->fillRoundRect(boxX + 20, boxY + 80, boxW - 40, 25, 5, TFT_RED); // Kırmızı
                spr->setTextColor(TFT_WHITE, TFT_RED);
            } else {
                spr->drawRoundRect(boxX + 20, boxY + 80, boxW - 40, 25, 5, TFT_WHITE);
                spr->setTextColor(TFT_WHITE, 0x2104);
            }
            spr->drawCentreString("CIKIS", 120, boxY + 85, 2);

            spr->pushSprite(0, 0);
            needsRedraw = false;
        }
    }

    // ============================================================
    // DURUM 4: GAME OVER
    // ============================================================
    else if (gameState == GAMES_FLAPPY_GAMEOVER) {
        if (needsRedraw) {
            // Son kare ekranda kalsın, üzerine kutu çizelim
            int boxX = 40, boxY = 80, boxW = 160, boxH = 120;
            spr->fillRoundRect(boxX, boxY, boxW, boxH, 10, 0xE71C); // Açık gri
            spr->drawRoundRect(boxX, boxY, boxW, boxH, 10, TFT_WHITE);

            spr->setTextColor(TFT_RED, 0xE71C);
            spr->drawCentreString("GAME OVER", 120, boxY + 20, 2);

            spr->setTextColor(TFT_BLACK, 0xE71C);
            spr->drawCentreString("Skor: " + String(score), 120, boxY + 50, 2);
            spr->drawCentreString("En Yuksek: " + String(highScore), 120, boxY + 70, 2);

            spr->setTextColor(TFT_BLUE, 0xE71C);
            spr->drawCentreString("Tekrar Oyna ->", 120, boxY + 95, 1);

            spr->pushSprite(0, 0);
            needsRedraw = false;
        }

        // Yeniden Başlat veya Çık
        if (res.clicked || res.tbPress) {
            gameState = GAMES_FLAPPY_INIT;
            needsRedraw = true;
            delay(200);
        }
        if (res.tbLeft) { // Soldaki tuş ile menüye dön
            gameState = GAMES_MENU;
            needsRedraw = true;
            delay(200);
        }
    }
}