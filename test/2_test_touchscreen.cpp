#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ===============================
// TOUCH PIN
// ===============================
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

// ===============================
// TFT
// ===============================
TFT_eSPI tft = TFT_eSPI();

// ===============================
// TOUCH SPI
// Gunakan HSPI agar tidak bentrok
// dengan SPI TFT
// ===============================
SPIClass touchSPI(HSPI);

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void setup() {

  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("==============================");
  Serial.println("KELAS ROBOT - TOUCH TEST");
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

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("KELAS ROBOT");

  tft.drawFastHLine(
    0,
    35,
    tft.width(),
    TFT_WHITE
  );

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 45);
  tft.println("Touch Test");

  tft.setCursor(10, 70);
  tft.println("Sentuh layar...");

  // ===============================
  // TOUCH SPI - HSPI
  // ===============================
  touchSPI.begin(
    TOUCH_CLK,
    TOUCH_MISO,
    TOUCH_MOSI,
    TOUCH_CS
  );

  // ===============================
  // INIT XPT2046
  // ===============================
  ts.begin(touchSPI);
  ts.setRotation(1);

  Serial.println("TFT OK");
  Serial.println("Touch SPI = HSPI");
  Serial.println("Touch CS  = GPIO 33");
  Serial.println("Touch IRQ = GPIO 36");
  Serial.println("Touch MOSI = GPIO 32");
  Serial.println("Touch MISO = GPIO 39");
  Serial.println("Touch CLK  = GPIO 25");
  Serial.println("==============================");
  Serial.println("Sentuh layar...");
}

void loop() {

  if (ts.touched()) {

    TS_Point p = ts.getPoint();

    // ===============================
    // SERIAL
    // ===============================
    Serial.printf(
      "RAW X=%d Y=%d Z=%d\n",
      p.x,
      p.y,
      p.z
    );

    // ===============================
    // DISPLAY
    // ===============================
    tft.fillRect(
      0,
      100,
      tft.width(),
      140,
      TFT_BLACK
    );

    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);

    tft.setCursor(10, 110);
    tft.printf("RAW X: %d", p.x);

    tft.setCursor(10, 135);
    tft.printf("RAW Y: %d", p.y);

    tft.setCursor(10, 160);
    tft.printf("Z: %d", p.z);

    delay(100);
  }
}