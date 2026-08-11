#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ===============================
// TOUCH
// ===============================
#define TOUCH_CS 33
#define TOUCH_IRQ 36

#define TOUCH_CLK  25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32

// Gunakan HSPI untuk touch
SPIClass mySpi = SPIClass(HSPI);

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

TFT_eSPI tft = TFT_eSPI();

// ===============================
// CALIBRATION
// ===============================
int minX = 9999;
int maxX = 0;

int minY = 9999;
int maxY = 0;

int step = 0;

const char* stepNames[] = {
  "TOP-LEFT",
  "TOP-RIGHT",
  "BOTTOM-LEFT",
  "BOTTOM-RIGHT"
};

const int crossX[] = {
  20,
  300,
  20,
  300
};

const int crossY[] = {
  20,
  20,
  220,
  220
};

// ===============================
// DRAW CROSS
// ===============================
void drawCross(int x, int y, uint16_t color) {

  tft.drawLine(
    x - 10,
    y,
    x + 10,
    y,
    color
  );

  tft.drawLine(
    x,
    y - 10,
    x,
    y + 10,
    color
  );

  tft.drawCircle(
    x,
    y,
    5,
    color
  );
}

// ===============================
// DRAW SCREEN
// ===============================
void drawScreen() {

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 90);
  tft.println("KELAS ROBOT");

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);

  tft.setCursor(10, 115);
  tft.println("Touch Calibration");

  tft.setCursor(10, 130);

  tft.printf(
    "Step %d/4: Sentuh %s",
    step + 1,
    stepNames[step]
  );

  for (int i = 0; i < 4; i++) {

    drawCross(
      crossX[i],
      crossY[i],
      i == step ? TFT_RED : TFT_DARKGREY
    );
  }
}

// ===============================
// SETUP
// ===============================
void setup() {

  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println("==============================");
  Serial.println("KELAS ROBOT");
  Serial.println("XPT2046 CALIBRATION");
  Serial.println("==============================");

  // ===============================
  // BACKLIGHT 3.5"
  // ===============================

  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  // ===============================
  // TFT
  // ===============================

  tft.init();
  tft.setRotation(1);

  // ===============================
  // TOUCH SPI - HSPI
  // ===============================

  mySpi.begin(
    TOUCH_CLK,
    TOUCH_MISO,
    TOUCH_MOSI,
    TOUCH_CS
  );

  // PENTING:
  // gunakan SPI yang sudah kita konfigurasi
  ts.begin(mySpi);

  ts.setRotation(1);

  // ===============================
  // SCREEN
  // ===============================

  drawScreen();

  Serial.println("=== Touch Calibration ===");
  Serial.println("Sentuh setiap tanda merah.");
}

// ===============================
// LOOP
// ===============================
void loop() {

  if (step >= 4) {
    return;
  }

  if (
    ts.tirqTouched() &&
    ts.touched()
  ) {

    TS_Point p = ts.getPoint();

    // ===============================
    // FILTER PRESSURE
    // ===============================

    if (p.z < 100) {
      return;
    }

    Serial.printf(
      "Step %d (%s): RAW X=%d Y=%d Z=%d\n",
      step + 1,
      stepNames[step],
      p.x,
      p.y,
      p.z
    );

    // ===============================
    // MIN / MAX X
    // ===============================

    if (p.x < minX) {
      minX = p.x;
    }

    if (p.x > maxX) {
      maxX = p.x;
    }

    // ===============================
    // MIN / MAX Y
    // ===============================

    if (p.y < minY) {
      minY = p.y;
    }

    if (p.y > maxY) {
      maxY = p.y;
    }

    // ===============================
    // TANDA BERHASIL
    // ===============================

    drawCross(
      crossX[step],
      crossY[step],
      TFT_GREEN
    );

    step++;

    // ===============================
    // SELESAI
    // ===============================

    if (step >= 4) {

      tft.fillScreen(TFT_BLACK);

      tft.setTextColor(TFT_GREEN);
      tft.setTextSize(2);
      tft.setCursor(10, 10);

      tft.println("Calibration Done!");

      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);

      tft.setCursor(10, 40);
      tft.println(
        "Masukkan nilai ini ke code:"
      );

      tft.setCursor(10, 60);
      tft.printf(
        "#define TS_MINX %d",
        minX
      );

      tft.setCursor(10, 75);
      tft.printf(
        "#define TS_MAXX %d",
        maxX
      );

      tft.setCursor(10, 90);
      tft.printf(
        "#define TS_MINY %d",
        minY
      );

      tft.setCursor(10, 105);
      tft.printf(
        "#define TS_MAXY %d",
        maxY
      );

      // ===============================
      // SERIAL
      // ===============================

      Serial.println();
      Serial.println("=== HASIL KALIBRASI ===");

      Serial.printf(
        "#define TS_MINX %d\n",
        minX
      );

      Serial.printf(
        "#define TS_MAXX %d\n",
        maxX
      );

      Serial.printf(
        "#define TS_MINY %d\n",
        minY
      );

      Serial.printf(
        "#define TS_MAXY %d\n",
        maxY
      );

      Serial.println(
        "======================="
      );

    } else {

      delay(500);

      drawScreen();
    }

    delay(300);
  }
}