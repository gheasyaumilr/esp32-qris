#include <Arduino.h>
#include <TFT_eSPI.h>

#define LDR_PIN 34

TFT_eSPI tft = TFT_eSPI();

const int barX = 20;
const int barY = 60;
const int barW = 280;
const int barH = 24;

int lastFillW = -1;
int lastRaw = -1;
int lastPercent = -1;

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  pinMode(LDR_PIN, INPUT);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.drawCentreString("LDR Light Level", tft.width() / 2, 10, 2);

  tft.drawRect(barX, barY, barW, barH, TFT_WHITE);

  tft.drawString("Raw:", barX, barY + barH + 10, 2);
  tft.drawString("Light:", barX, barY + barH + 34, 2);
}

void loop() {

  int raw = analogRead(LDR_PIN);

  int percent = map(raw, 0, 1000, 100, 0);
  percent = constrain(percent, 0, 100);

  int fillW = map(percent, 0, 100, 0, barW - 4);

  // Update bar hanya jika berubah
  if (fillW != lastFillW) {

    // Tambah bar
    if (fillW > lastFillW) {
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

  // Update text hanya jika berubah
  if (raw != lastRaw) {
    tft.drawString(String(raw) + "    ", barX + 72, barY + barH + 10, 2);
    lastRaw = raw;
  }

  if (percent != lastPercent) {
    tft.drawString(String(percent) + "%   ", barX + 72, barY + barH + 34, 2);
    lastPercent = percent;
  }

  delay(50);
}