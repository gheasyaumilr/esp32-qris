#include <Arduino.h>
#include <TFT_eSPI.h>

#define LDR_PIN 34

TFT_eSPI tft = TFT_eSPI();

// ========================================
// BAR
// ========================================

const int barX = 20;
const int barY = 60;
const int barW = 280;
const int barH = 24;

// ========================================
// CACHE
// ========================================

int lastFillW = -1;
int lastRaw = -1;
int lastPercent = -1;

// ========================================
// SETUP
// ========================================

void setup() {

  Serial.begin(115200);

  // ADC ESP32 = 12 bit
  analogReadResolution(12);

  pinMode(LDR_PIN, INPUT);

  // ======================================
  // TFT
  // ======================================

  tft.init();

  // Landscape
  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);

  // Text
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  // ======================================
  // TITLE
  // ======================================

  tft.drawCentreString(
    "LDR Light Level",
    tft.width() / 2,
    10,
    2
  );

  // ======================================
  // BAR
  // ======================================

  tft.drawRect(
    barX,
    barY,
    barW,
    barH,
    TFT_WHITE
  );

  // ======================================
  // LABEL
  // ======================================

  tft.drawString(
    "Raw:",
    barX,
    barY + barH + 10,
    2
  );

  tft.drawString(
    "Light:",
    barX,
    barY + barH + 34,
    2
  );

  Serial.println("=== LDR TEST ===");

  Serial.print("TFT Width  : ");
  Serial.println(tft.width());

  Serial.print("TFT Height : ");
  Serial.println(tft.height());
}

// ========================================
// LOOP
// ========================================

void loop() {

  // ======================================
  // READ LDR
  // ======================================

  int raw = analogRead(LDR_PIN);

  // ======================================
  // CONVERT TO PERCENT
  // ======================================
  //
  // Raw 0    = terang
  // Raw 1000 = gelap
  //
  // Jika karakteristik LDR kamu berbeda,
  // angka 1000 bisa disesuaikan.
  // ======================================

  int percent = map(
    raw,
    0,
    1000,
    100,
    0
  );

  percent = constrain(
    percent,
    0,
    100
  );

  // ======================================
  // BAR WIDTH
  // ======================================

  int fillW = map(
    percent,
    0,
    100,
    0,
    barW - 4
  );

  // ======================================
  // UPDATE BAR
  // ======================================

  if (fillW != lastFillW) {

    // Pertama kali
    if (lastFillW == -1) {

      tft.fillRect(
        barX + 2,
        barY + 2,
        fillW,
        barH - 4,
        TFT_GREEN
      );

    }

    // Tambah bar
    else if (fillW > lastFillW) {

      tft.fillRect(
        barX + 2 + lastFillW,
        barY + 2,
        fillW - lastFillW,
        barH - 4,
        TFT_GREEN
      );

    }

    // Kurangi bar
    else {

      tft.fillRect(
        barX + 2 + fillW,
        barY + 2,
        lastFillW - fillW,
        barH - 4,
        TFT_BLACK
      );
    }

    lastFillW = fillW;
  }

  // ======================================
  // UPDATE RAW
  // ======================================

  if (raw != lastRaw) {

    tft.drawString(
      String(raw) + "    ",
      barX + 72,
      barY + barH + 10,
      2
    );

    lastRaw = raw;
  }

  // ======================================
  // UPDATE PERCENT
  // ======================================

  if (percent != lastPercent) {

    tft.drawString(
      String(percent) + "%   ",
      barX + 72,
      barY + barH + 34,
      2
    );

    lastPercent = percent;
  }

  // ======================================
  // SERIAL MONITOR
  // ======================================

  Serial.printf(
    "LDR Raw: %d | Light: %d%%\n",
    raw,
    percent
  );

  delay(50);
}