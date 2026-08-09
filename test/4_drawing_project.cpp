#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, 36);
TFT_eSPI tft = TFT_eSPI();

#define TS_MINX 309
#define TS_MAXX 3804
#define TS_MINY 310
#define TS_MAXY 3760

int lastX = -1, lastY = -1;
uint16_t drawColor = TFT_WHITE;

void drawColorPalette() {
  int btnW = tft.width() / 6;
  int btnH = 30;
  int y = tft.height() - btnH;

  tft.fillRect(btnW * 0, y, btnW, btnH, TFT_WHITE);
  tft.fillRect(btnW * 1, y, btnW, btnH, TFT_RED);
  tft.fillRect(btnW * 2, y, btnW, btnH, TFT_GREEN);
  tft.fillRect(btnW * 3, y, btnW, btnH, TFT_BLUE);
  tft.fillRect(btnW * 4, y, btnW, btnH, TFT_YELLOW);
  tft.fillRect(btnW * 5, y, btnW, btnH, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(btnW * 5 + 5, y + 10);
  tft.println("CLEAR");
}

void setup() {
  Serial.begin(115200);

  mySpi.begin(25, 39, 32, TOUCH_CS);
  ts.begin();
  ts.setRotation(1);

  tft.init();
  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  tft.println("KELAS ROBOT - Draw!");
  drawColorPalette();
}

void loop() {
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();

    if (p.z < 100) return;

    int x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
    int y = map(p.y, TS_MAXY, TS_MINY, 0, tft.height());

    x = constrain(x, 0, tft.width() - 1);
    y = constrain(y, 0, tft.height() - 1);

    int btnW = tft.width() / 6;
    int btnY = tft.height() - 30;

    if (y >= btnY) {
      int btn = x / btnW;
      switch (btn) {
        case 0: drawColor = TFT_WHITE;  break;
        case 1: drawColor = TFT_RED;    break;
        case 2: drawColor = TFT_GREEN;  break;
        case 3: drawColor = TFT_BLUE;   break;
        case 4: drawColor = TFT_YELLOW; break;
        case 5:
          tft.fillScreen(TFT_BLACK);
          tft.setTextColor(TFT_YELLOW);
          tft.setTextSize(2);
          tft.setCursor(10, 5);
          tft.println("KELAS ROBOT - Draw!");
          drawColorPalette();
          lastX = -1; lastY = -1;
          break;
      }
      lastX = -1; lastY = -1;
      return;
    }

    if (y > 30) {
      if (lastX != -1 && lastY != -1) {
        tft.drawLine(lastX, lastY, x, y, drawColor);
      } else {
        tft.fillCircle(x, y, 2, drawColor);
      }
      lastX = x;
      lastY = y;
    }

    delay(10);
  } else {
    lastX = -1;
    lastY = -1;
  }
}