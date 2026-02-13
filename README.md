# 📟 Flipper Zero Clone (ESP32-S3)

Bu proje, **ESP32-S3** mikrodenetleyici kullanılarak geliştirilen, Flipper Zero'dan ilham alan açık kaynaklı bir "multitool" cihazıdır. Şık ve akıcı (sayfalı) bir arayüze, dokunmatik ve trackball desteğine, ayrıca NFC okuma/yazma/emülasyon yeteneklerine sahiptir.

## ✨ Öne Çıkan Özellikler
* **Akıcı Arayüz:** Sayfa geçişli, 2x6 ikonlu dinamik menü sistemi.
* **NFC Desteği:** PN532 modülü ile kart okuma, yazma ve emülasyon.
* **Multimedya:** MAX98357A I2S amfi desteği ile ses çıkışı.
* **Giriş Birimleri:** Hem Dokunmatik ekran hem de Trackball ile tam kontrol.
* **Depolama:** Dosya yönetimi ve veri kaydı için SD Kart desteği.

---

## 🛠 Donanım Bağlantıları (Pinout)

Projenin kararlı çalışması için aşağıdaki pin tanımlamalarını baz alabilirsiniz:

### 📺 Ekran & Dokunmatik (SPI2 - FSPI)
| Bileşen | Pin (GPIO) | Not |
| :--- | :--- | :--- |
| **TFT_MISO** | 13 | T_DO & SD MISO |
| **TFT_MOSI** | 11 | T_DIN & LCD SDI |
| **TFT_SCLK** | 12 | T_CLK & LCD SCK |
| **TFT_CS** | 10 | Ekran Seçim |
| **TFT_DC** | 14 | Data / Command (RS) |
| **TFT_RST** | 15 | Reset |
| **TOUCH_CS** | 5 | Dokunmatik Seçim |

### 🛰 NFC (PN532 I2C)
| Bileşen | Pin (GPIO) |
| :--- | :--- |
| **SDA** | 8 |
| **SCL** | 9 |

### 📂 SD Kart (SPI)
| Bileşen | Pin (GPIO) |
| :--- | :--- |
| **SD_SCK** | 41 |
| **SD_MISO** | 42 |
| **SD_MOSI** | 2 |
| **SD_CS** | 21 |

### 🔊 Ses (I2S MAX98357A)
| Bileşen | Pin (GPIO) |
| :--- | :--- |
| **BCLK** | 18 |
| **LRC (WS)** | 17 |
| **DIN (DOUT)** | 16 |

### 🕹 Trackball Girişi
| Yön | Pin (GPIO) |
| :--- | :--- |
| **UP** | 45 |
| **DOWN** | 47 |
| **LEFT** | 46 |
| **RIGHT** | 7 |
| **BUTTON** | 38 |

---

## ⚙️ Kurulum ve Yapılandırma
1. **PlatformIO** kullanarak projeyi açın.
2. `config.h` dosyasından WiFi ayarlarınızı düzenleyin:
   ```cpp
   #define WIFI_SSID "SSID_ADINIZ"
   #define WIFI_PASS "SIFRENIZ"