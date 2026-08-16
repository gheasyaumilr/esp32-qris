# QRISArduino

**[🇬🇧 English](#english) | [🇮🇩 Bahasa Indonesia](#bahasa-indonesia)**

---

## English

Arduino library for QRIS payment integration on **ESP8266 and ESP32** via [Mayar](https://bit.ly/daftar-mayar) API.

This is the Arduino/ESP port of [qrispy](https://github.com/ajangrahmat/qrispy) Python SDK.

### Features

- **QRIS generation** — create a QR code for any payment amount
- **Balance checking** — fetch active, pending, and total balance
- **Payment polling** — automatically detect incoming payments by polling balance
- **QR image display** — download and render the QR PNG image directly on a TFT screen (ILI9341), with retry logic and progress callback
- **Touchscreen keypad** — built-in numeric keypad UI for ILI9341 + FT6206 touch panel, ready to use out of the box
- **WiFi management** — auto-reconnect support with configurable timeout
- **Debug logging** — clean, human-readable Serial output via `.debug(true)`

### Installation

**Arduino Library Manager** (recommended):
1. Open Arduino IDE → Sketch → Include Library → Manage Libraries
2. Search for `QRISArduino`
3. Click Install

**Manual:**
1. Download ZIP from GitHub
2. Arduino IDE → Sketch → Include Library → Add .ZIP Library

### Dependencies

- [ArduinoJson](https://arduinojson.org/) >= 6.x
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit ILI9341](https://github.com/adafruit/Adafruit_ILI9341)
- [Adafruit FT6206](https://github.com/adafruit/Adafruit_FT6206)
- [PNGdec](https://github.com/bitbank2/PNGdec)

### Boards Supported

- ESP8266 (NodeMCU, Wemos D1, etc.)
- ESP32 (DevKit, WROOM, WROVER, etc.)

### Quick Start

```cpp
#include <QRISArduino.h>

Mayar api("YOUR_API_KEY");

void setup() {
  Serial.begin(115200);
  api.debug(true);
  api.begin("YOUR_SSID", "YOUR_PASSWORD");

  // Check balance
  QRISResponse bal = api.balance();
  if (bal.success) {
    Serial.println(bal.balance);       // Total balance
    Serial.println(bal.balance_active); // Active balance
  }

  // Create QRIS for Rp 15,000
  QRISResponse qris = api.createQris(15000);
  if (qris.success) {
    Serial.println(qris.url);    // QR image URL
    Serial.println(qris.amount); // Payment amount
  }
}
```

### Mayar Class — API Reference

| Method | Description |
|--------|-------------|
| `Mayar(apiKey)` | Constructor, pass your Mayar API key |
| `begin(ssid, password)` | Connect to WiFi |
| `balance()` | Fetch account balance |
| `createQris(amount)` | Generate a QRIS QR code for the given amount |
| `waitForPayment(initialBalance, intervalMs, timeoutMs, callback)` | Poll until balance increases or timeout |
| `resizeImageUrl(url, size)` | Resize the QR image URL to a given pixel size |
| `setTimeout(seconds)` | Set HTTP request timeout |
| `setInsecure(bool)` | Skip SSL certificate verification |
| `setAutoReconnect(bool)` | Enable/disable automatic WiFi reconnection |
| `debug(bool)` | Enable/disable Serial debug output |

### QRISResponse Fields

| Field | Type | Description |
|-------|------|-------------|
| `success` | bool | `true` if statusCode == 200 |
| `status_code` | int | HTTP status code from API |
| `message` | String | Message from API |
| `url` | String | QR image URL (createQris) |
| `amount` | long | Payment amount (createQris) |
| `reference_id` | String | Transaction reference ID |
| `balance` | long | Total balance (balance) |
| `balance_pending` | long | Pending balance (balance) |
| `balance_active` | long | Active balance (balance) |
| `rawJson` | String | Raw JSON response |

### TFTKeypad — Touchscreen Keypad

A ready-to-use numeric keypad UI for ILI9341 + FT6206.

```cpp
Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
Adafruit_FT6206  ctp;
TFTKeypad keypad(tft, ctp);

keypad.begin();

keypad.onOk([&]() {
  long amount = keypad.getValue().toInt();
  // proceed with payment...
});

void loop() {
  keypad.process(); // call every loop
}
```

### TFTImageDisplay — QR Image on TFT

Download and render a PNG QR image directly onto an ILI9341 display.

```cpp
TFTImageDisplay imageDisplay(tft);

// Show QR image at 200x200 pixels with progress callback
imageDisplay.show(qris.url, 200, 200, 20, [](int pct) {
  Serial.println(pct);
});
```

Includes automatic retry (up to 3 attempts) and WiFi reconnect between retries.

### Register Mayar

[👉 Sign up at Mayar](https://bit.ly/daftar-mayar) — individual KTP verification, no company required.

---

## Bahasa Indonesia

Library Arduino untuk integrasi pembayaran QRIS di **ESP8266 dan ESP32** via API [Mayar](https://bit.ly/daftar-mayar).

Ini adalah port Arduino/ESP dari Python SDK [qrispy](https://github.com/ajangrahmat/qrispy).

### Fitur

- **Pembuatan QRIS** — buat kode QR untuk nominal pembayaran tertentu
- **Pengecekan saldo** — ambil saldo aktif, pending, dan total akun
- **Polling pembayaran** — deteksi pembayaran masuk secara otomatis dengan polling saldo
- **Tampilan gambar QR** — download dan tampilkan PNG QR langsung ke layar TFT (ILI9341), dilengkapi retry otomatis dan progress callback
- **Keypad touchscreen** — UI keypad angka siap pakai untuk ILI9341 + panel sentuh FT6206
- **Manajemen WiFi** — auto-reconnect dengan timeout yang bisa dikonfigurasi
- **Debug logging** — output Serial yang bersih dan mudah dibaca via `.debug(true)`

### Instalasi

**Via Arduino Library Manager:**
1. Arduino IDE → Sketch → Include Library → Manage Libraries
2. Cari `QRISArduino`
3. Klik Install

**Manual:**
1. Download ZIP dari GitHub
2. Arduino IDE → Sketch → Include Library → Add .ZIP Library

### Dependencies

- [ArduinoJson](https://arduinojson.org/) >= 6.x
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit ILI9341](https://github.com/adafruit/Adafruit_ILI9341)
- [Adafruit FT6206](https://github.com/adafruit/Adafruit_FT6206)
- [PNGdec](https://github.com/bitbank2/PNGdec)

### Board yang Didukung

- ESP8266 (NodeMCU, Wemos D1, dll)
- ESP32 (DevKit, WROOM, WROVER, dll)

### Daftar Mayar

[👉 Daftar di Mayar](https://bit.ly/daftar-mayar) — verifikasi KTP pribadi, tanpa perlu PT.

### Lisensi

MIT License

### Author

Ajang Rahmat — ajangrahmat@gmail.com