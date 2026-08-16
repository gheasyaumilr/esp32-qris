#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <Adafruit_FT6206.h>
#include "QRISArduino.h"
#include <secrets.h>

#define TFT_DC 15
#define TFT_CS 5

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
Adafruit_FT6206 ctp;

Mayar mayar(apikey);
TFTKeypad keypad(tft, ctp);
TFTImageDisplay imageDisplay(tft);

enum AppState {
  STATE_IDLE,
  STATE_LOADING_GET_BALANCE,
  STATE_LOADING_CREATE_QR,
  STATE_LOADING_DOWNLOAD_QR,
  STATE_SHOW_QR_POLLING,
  STATE_SUCCESS,
  STATE_TIMEOUT,
  STATE_WAITING_BACK
};

AppState currentState    = STATE_IDLE;
unsigned long stateStartTime = 0;
unsigned long pollStartTime  = 0;
long transAmount    = 0;
long initialBalance = 0;
String qrUrl        = "";

// ─── Helpers ───────────────────────────────────────────────────────────────

void showMessage(const char* msg, uint16_t color, int y = 140) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(color);
  int x = (240 - (int)strlen(msg) * 18) / 2;
  if (x < 10) x = 10;
  tft.setCursor(x, y);
  tft.print(msg);
}

void drawQrProgress(int percent) {
  static int lastPct = -1;
  if (percent == lastPct) return;
  lastPct = percent;

  const int BAR_X = 20, BAR_Y = 270, BAR_W = 200, BAR_H = 14;
  int filled = map(percent, 0, 100, 0, BAR_W);

  tft.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, ILI9341_DARKGREY);
  tft.fillRect(BAR_X, BAR_Y, filled, BAR_H, ILI9341_GREEN);
  tft.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, ILI9341_WHITE);

  char buf[8];
  sprintf(buf, "%d%%", percent);
  tft.fillRect(96, 290, 50, 14, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  int tx = (240 - (int)strlen(buf) * 6) / 2;
  tft.setCursor(tx, 290);
  tft.print(buf);
}

void drawProgress(int percent) {
  tft.fillRect(0, 80, 240, 160, ILI9341_BLACK);

  char buf[10];
  sprintf(buf, "%d%%", percent);
  tft.setTextSize(5);
  tft.setTextColor(ILI9341_CYAN);
  int x = (240 - (int)strlen(buf) * 30) / 2;
  tft.setCursor(x, 130);
  tft.print(buf);

  int barWidth = map(percent, 0, 100, 0, 200);
  tft.fillRect(20, 190, 200, 20, ILI9341_DARKGREY);
  tft.fillRect(20, 190, barWidth, 20, ILI9341_GREEN);
  tft.drawRect(20, 190, 200, 20, ILI9341_WHITE);
}

void resetToKeypad() {
  currentState = STATE_IDLE;
  tft.fillScreen(0x18C3);
  keypad.reset();
  keypad.draw();
  keypad.updateDisplay();
  transAmount    = 0;
  initialBalance = 0;
  qrUrl          = "";
}

// ─── Setup ─────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  mayar.debug(true);
  Wire.begin(21, 22);
  tft.begin();
  tft.setRotation(0);

  if (!ctp.begin(40)) {
    tft.fillScreen(ILI9341_RED);
    tft.setCursor(30, 140);
    tft.print("TOUCH ERROR");
    while (1);
  }

  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(20, 150);
  tft.print("Connecting WiFi...");

  if (!mayar.begin(ssid, wifiPassword)) {
    tft.fillScreen(ILI9341_RED);
    tft.setCursor(20, 150);
    tft.print("WiFi Failed");
    while (1);
  }

  keypad.begin();

  keypad.onDigit([&](char d) {
    if (currentState == STATE_IDLE) {
      String val = keypad.getValue();
      if (val.length() < 10) {
        if (val == "0") val = "";
        val += d;
        keypad.setValue(val);
      }
    }
  });

  keypad.onDel([&]() {
    if (currentState == STATE_IDLE) {
      String val = keypad.getValue();
      if (val.length()) val.remove(val.length() - 1);
      keypad.setValue(val);
    }
  });

  keypad.onOk([&]() {
    if (currentState == STATE_IDLE) {
      String amountStr = keypad.getValue();
      if (amountStr.isEmpty() || amountStr.toInt() == 0) {
        tft.fillRect(0, 200, 240, 40, ILI9341_BLACK);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(20, 205);
        tft.print("Masukkan jumlah!");
        delay(700);
        keypad.updateDisplay();
        return;
      }
      transAmount  = amountStr.toInt();
      currentState = STATE_LOADING_GET_BALANCE;
      tft.fillScreen(ILI9341_BLACK);
      drawProgress(0);
    }
  });
}

// ─── Loop ──────────────────────────────────────────────────────────────────

void loop() {
  keypad.process();

  switch (currentState) {

    case STATE_IDLE: break;

    case STATE_LOADING_GET_BALANCE: {
      drawProgress(20);
      QRISResponse bal = mayar.balance();
      if (bal.success && bal.balance >= 0) {
        initialBalance = bal.balance;
        currentState   = STATE_LOADING_CREATE_QR;
      } else {
        showMessage("Gagal ambil saldo", ILI9341_RED);
        delay(1500);
        resetToKeypad();
      }
      break;
    }

    case STATE_LOADING_CREATE_QR: {
      drawProgress(50);
      QRISResponse qr = mayar.createQris(transAmount);
      if (qr.success && qr.url.length() > 0 && qr.url != "null") {
        qrUrl        = qr.url;
        currentState = STATE_LOADING_DOWNLOAD_QR;
      } else {
        showMessage("Gagal buat QR", ILI9341_RED);
        delay(1500);
        resetToKeypad();
      }
      break;
    }

    case STATE_LOADING_DOWNLOAD_QR: {
      tft.fillScreen(ILI9341_BLACK);

      // Judul
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_YELLOW);
      tft.setCursor(45, 8);
      tft.print("SCAN QRIS");

      // Area QR: 200×200, center horizontal, y=55 (jarak dari judul ~31px)
      const int QR_SIZE = 200;
      const int QR_X    = (240 - QR_SIZE) / 2;  // = 20
      const int QR_Y    = 55;

      tft.fillRect(QR_X, QR_Y, QR_SIZE, QR_SIZE, ILI9341_BLACK);

      // Label download
      tft.setTextSize(1);
      tft.setTextColor(ILI9341_CYAN);
      tft.setCursor(50, 258);
      tft.print("Mengunduh QR...");

      String resizedUrl = Mayar::resizeImageUrl(qrUrl, QR_SIZE);
      imageDisplay.setOffset(QR_X, QR_Y);

      auto progressCb = [&](int pct) {
        drawQrProgress(pct);
      };

      bool ok = imageDisplay.show(resizedUrl, QR_SIZE, QR_SIZE, 20, progressCb);
      imageDisplay.resetOffset();

      if (ok) {
        tft.fillRect(0, 258, 240, 62, ILI9341_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_YELLOW);
        int tw = 22 * 6;
        tft.setCursor((240 - tw) / 2, 308);
        tft.print("Menunggu pembayaran...");

        currentState  = STATE_SHOW_QR_POLLING;
        pollStartTime = millis();
      } else {
        showMessage("Gagal muat QR", ILI9341_RED);
        delay(1500);
        resetToKeypad();
      }
      break;
    }

    case STATE_SHOW_QR_POLLING: {
      static unsigned long lastPoll = 0;
      unsigned long now = millis();

      if (now - lastPoll >= 3000) {
        lastPoll = now;

        static bool dot = false;
        tft.fillRect(225, 306, 12, 12, ILI9341_BLACK);
        tft.fillCircle(231, 312, 4, dot ? ILI9341_GREEN : ILI9341_DARKGREY);
        dot = !dot;

        QRISResponse bal = mayar.balance();
        if (bal.success && bal.balance > initialBalance) {
          currentState = STATE_SUCCESS;
        } else if (now - pollStartTime > 300000UL) {
          currentState = STATE_TIMEOUT;
        }
      }
      break;
    }

    case STATE_SUCCESS: {
      tft.fillScreen(ILI9341_BLACK);

      tft.setTextSize(3);
      tft.setTextColor(ILI9341_GREEN);
      tft.setCursor(30, 90);
      tft.print("BERHASIL!");

      tft.setTextSize(2);
      tft.setTextColor(ILI9341_WHITE);
      char amtBuf[24];
      sprintf(amtBuf, "Rp %ld", transAmount);
      int ax = (240 - (int)strlen(amtBuf) * 12) / 2;
      tft.setCursor(ax, 145);
      tft.print(amtBuf);

      tft.setTextSize(1);
      tft.setTextColor(ILI9341_CYAN);
      tft.setCursor(30, 215);
      tft.print("Sentuh layar untuk kembali");

      currentState   = STATE_WAITING_BACK;
      stateStartTime = millis();
      break;
    }

    case STATE_TIMEOUT: {
      tft.fillScreen(ILI9341_BLACK);

      tft.setTextSize(3);
      tft.setTextColor(ILI9341_RED);
      tft.setCursor(40, 130);
      tft.print("TIMEOUT");

      tft.setTextSize(1);
      tft.setTextColor(ILI9341_CYAN);
      tft.setCursor(30, 215);
      tft.print("Sentuh layar untuk kembali");

      currentState   = STATE_WAITING_BACK;
      stateStartTime = millis();
      break;
    }

    case STATE_WAITING_BACK: {
      if (millis() - stateStartTime > 180000UL) {
        resetToKeypad();
      } else if (ctp.touched()) {
        delay(200);
        resetToKeypad();
      }
      break;
    }
  }

  delay(10);
}