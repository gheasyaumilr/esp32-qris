#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void testColors() {
  struct { uint32_t color; const char* name; } colors[] = {
    { TFT_RED,     "RED"     },
    { TFT_GREEN,   "GREEN"   },
    { TFT_BLUE,    "BLUE"    },
    { TFT_YELLOW,  "YELLOW"  },
    { TFT_CYAN,    "CYAN"    },
    { TFT_MAGENTA, "MAGENTA" },
    { TFT_WHITE,   "WHITE"   },
  };

  for (auto& c : colors) {
    tft.fillScreen(c.color);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(10, tft.height() / 2 - 10);
    tft.println(c.name);
    Serial.printf("Color: %s\n", c.name);
    delay(800);
  }
}

void testRotations() {
  for (int r = 0; r < 4; r++) {
    tft.setRotation(r);
    tft.fillScreen(TFT_NAVY);
    tft.drawRect(0, 0, tft.width(), tft.height(), TFT_WHITE);

    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("Rotation: ");
    tft.println(r);

    tft.setCursor(10, 35);
    tft.print("W:");
    tft.print(tft.width());
    tft.print(" H:");
    tft.println(tft.height());

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(3);
    tft.setCursor(10, 70);
    tft.println("KELAS ROBOT");

    Serial.printf("Rotation: %d | W:%d H:%d\n", r, tft.width(), tft.height());
    delay(2000);
  }
}

void testTextStyles() {
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Judul
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.println("KELAS ROBOT");

  // Garis pemisah
  tft.drawFastHLine(0, 45, tft.width(), TFT_WHITE);

  // Ukuran teks
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(1);
  tft.setCursor(10, 55);
  tft.println("Size 1: KELAS ROBOT");

  tft.setTextSize(2);
  tft.setCursor(10, 70);
  tft.println("Size 2: KELAS ROBOT");

  tft.setTextSize(3);
  tft.setCursor(10, 95);
  tft.println("Size 3");

  // Warna berbeda
  tft.setTextSize(2);
  tft.setTextColor(TFT_RED);
  tft.setCursor(10, 130);
  tft.println("RED TEXT");

  tft.setTextColor(TFT_CYAN);
  tft.setCursor(10, 155);
  tft.println("CYAN TEXT");

  tft.setTextColor(TFT_MAGENTA);
  tft.setCursor(10, 180);
  tft.println("MAGENTA TEXT");

  delay(3000);
}

void setup() {
  Serial.begin(115200);
  tft.init();
  Serial.println("=== KELAS ROBOT - Display Test ===");

  Serial.println("1. Test Warna...");
  testColors();

  Serial.println("2. Test Rotasi...");
  testRotations();

  Serial.println("3. Test Text Style...");
  testTextStyles();

  // Tampilan akhir
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_GREEN);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(3);
  tft.setCursor(tft.width()/2 - 90, tft.height()/2 - 20);
  tft.println("KELAS ROBOT");
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(tft.width()/2 - 70, tft.height()/2 + 20);
  tft.println("Test Selesai!");

  Serial.println("=== Test Selesai! ===");
}

void loop() {}