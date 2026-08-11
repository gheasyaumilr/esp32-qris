#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ========================================
// TOUCH PIN
// ========================================
#define TOUCH_CS   33
#define TOUCH_IRQ  36

#define TOUCH_CLK  25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32

// ========================================
// TOUCH SPI
// Gunakan HSPI agar tidak bentrok dengan TFT
// ========================================
SPIClass mySpi = SPIClass(HSPI);

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// ========================================
// TFT
// ========================================
TFT_eSPI tft = TFT_eSPI();

// ========================================
// HASIL KALIBRASI TOUCHSCREEN
// ========================================
#define TS_MINX 404
#define TS_MAXX 3595

#define TS_MINY 573
#define TS_MAXY 3527

// ========================================
// DRAWING
// ========================================
int lastX = -1;
int lastY = -1;

uint16_t drawColor = TFT_WHITE;

// ========================================
// DRAW COLOR PALETTE
// ========================================
void drawColorPalette() {

  int btnW = tft.width() / 6;
  int btnH = 30;

  int y = tft.height() - btnH;

  tft.fillRect(
    btnW * 0,
    y,
    btnW,
    btnH,
    TFT_WHITE
  );

  tft.fillRect(
    btnW * 1,
    y,
    btnW,
    btnH,
    TFT_RED
  );

  tft.fillRect(
    btnW * 2,
    y,
    btnW,
    btnH,
    TFT_GREEN
  );

  tft.fillRect(
    btnW * 3,
    y,
    btnW,
    btnH,
    TFT_BLUE
  );

  tft.fillRect(
    btnW * 4,
    y,
    btnW,
    btnH,
    TFT_YELLOW
  );

  tft.fillRect(
    btnW * 5,
    y,
    btnW,
    btnH,
    TFT_DARKGREY
  );

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);

  tft.setCursor(
    btnW * 5 + 5,
    y + 10
  );

  tft.println("CLEAR");
}

// ========================================
// HEADER
// ========================================
void drawHeader() {

  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);

  tft.setCursor(10, 5);

  tft.println("KELAS ROBOT - Draw!");
}

// ========================================
// SETUP
// ========================================
void setup() {

  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println("==============================");
  Serial.println("KELAS ROBOT - TOUCH DEBUG");
  Serial.println("==============================");

  // ======================================
  // BACKLIGHT
  // ======================================

  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  // ======================================
  // TFT
  // ======================================

  tft.init();
  tft.setRotation(1);

  Serial.print("TFT Width  : ");
  Serial.println(tft.width());

  Serial.print("TFT Height : ");
  Serial.println(tft.height());

  // ======================================
  // TOUCH SPI - HSPI
  // ======================================

  mySpi.begin(
    TOUCH_CLK,
    TOUCH_MISO,
    TOUCH_MOSI,
    TOUCH_CS
  );

  ts.begin(mySpi);
  ts.setRotation(1);

  Serial.println("Touch SPI : HSPI");
  Serial.println("Touch CS  : GPIO 33");
  Serial.println("Touch IRQ : GPIO 36");
  Serial.println("Touch CLK : GPIO 25");
  Serial.println("Touch MISO: GPIO 39");
  Serial.println("Touch MOSI: GPIO 32");

  // ======================================
  // SCREEN
  // ======================================

  tft.fillScreen(TFT_BLACK);

  drawHeader();
  drawColorPalette();

  Serial.println("==============================");
  Serial.println("Touchscreen Ready!");
  Serial.println("Sentuh 4 sudut layar.");
  Serial.println("==============================");
}

// ========================================
// LOOP
// ========================================
void loop() {

  if (ts.tirqTouched() && ts.touched()) {

    TS_Point p = ts.getPoint();

    // ====================================
    // PRESSURE FILTER
    // ====================================

    if (p.z < 100) {
      return;
    }

    // ====================================
    // RAW -> SCREEN
    // ====================================
    //
    // Sementara kita gunakan:
    //
    // X : MIN -> MAX
    // Y : MIN -> MAX
    //
    // supaya bisa mengecek orientasi.
    // ====================================

    int x = map(
      p.x,
      TS_MINX,
      TS_MAXX,
      0,
      tft.width() - 1
    );

    int y = map(
      p.y,
      TS_MINY,
      TS_MAXY,
      0,
      tft.height() - 1
    );

    // ====================================
    // LIMIT
    // ====================================

    x = constrain(
      x,
      0,
      tft.width() - 1
    );

    y = constrain(
      y,
      0,
      tft.height() - 1
    );

    // ====================================
    // SERIAL DEBUG
    // ====================================

    Serial.printf(
      "RAW X=%d Y=%d Z=%d  ->  SCREEN X=%d Y=%d\n",
      p.x,
      p.y,
      p.z,
      x,
      y
    );

    // ====================================
    // PALETTE
    // ====================================

    int btnW = tft.width() / 6;
    int btnY = tft.height() - 30;

    if (y >= btnY) {

      int btn = x / btnW;

      switch (btn) {

        case 0:
          drawColor = TFT_WHITE;
          Serial.println("COLOR = WHITE");
          break;

        case 1:
          drawColor = TFT_RED;
          Serial.println("COLOR = RED");
          break;

        case 2:
          drawColor = TFT_GREEN;
          Serial.println("COLOR = GREEN");
          break;

        case 3:
          drawColor = TFT_BLUE;
          Serial.println("COLOR = BLUE");
          break;

        case 4:
          drawColor = TFT_YELLOW;
          Serial.println("COLOR = YELLOW");
          break;

        case 5:

          tft.fillScreen(TFT_BLACK);

          drawHeader();
          drawColorPalette();

          lastX = -1;
          lastY = -1;

          Serial.println("CANVAS = CLEAR");

          break;
      }

      lastX = -1;
      lastY = -1;

      delay(150);

      return;
    }

    // ====================================
    // DRAW AREA
    // ====================================

    if (y > 30) {

      if (
        lastX != -1 &&
        lastY != -1
      ) {

        tft.drawLine(
          lastX,
          lastY,
          x,
          y,
          drawColor
        );

      } else {

        tft.fillCircle(
          x,
          y,
          2,
          drawColor
        );
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