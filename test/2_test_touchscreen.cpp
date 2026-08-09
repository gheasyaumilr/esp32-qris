#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#define TOUCH_CS  33
#define TOUCH_IRQ 36

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void setup() {
  Serial.begin(115200);

  ts.begin();
  ts.setRotation(1);

  tft.init();
  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("KELAS ROBOT");
  tft.drawFastHLine(0, 35, tft.width(), TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 45);
  tft.println("Touch Test");
  tft.setCursor(10, 70);
  tft.println("Sentuh layar...");

  Serial.println("=== Touch Test Ready ===");
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    Serial.printf("RAW X=%d Y=%d Z=%d\n", p.x, p.y, p.z);

    tft.fillRect(0, 100, tft.width(), 140, TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);
    tft.setCursor(10, 110);
    tft.printf("RAW X: %d", p.x);
    tft.setCursor(10, 135);
    tft.printf("RAW Y: %d", p.y);
    tft.setCursor(10, 160);
    tft.printf("Z: %d", p.z);

    delay(50);
  }
}