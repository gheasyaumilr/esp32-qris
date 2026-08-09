#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>

#define SD_CS 5

TFT_eSPI tft = TFT_eSPI();

void printStatus(String msg, uint16_t color = TFT_WHITE) {
  static int y = 40;
  tft.setTextColor(color);
  tft.setTextSize(1);
  tft.setCursor(10, y);
  tft.println(msg);
  Serial.println(msg);
  y += 15;
  if (y > tft.height() - 15) y = 40;
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("KelasRobot: SD Card");

  // Init SD Card
  if (!SD.begin(SD_CS)) {
    printStatus("SD Card GAGAL!", TFT_RED);
    printStatus("Cek kartu SD!", TFT_RED);
    return;
  }
  printStatus("SD Card OK!", TFT_GREEN);

  // Info SD Card
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  printStatus("Size: " + String(cardSize) + " MB", TFT_CYAN);

  // Write file
  printStatus("Writing file...", TFT_WHITE);
  File file = SD.open("/kelas_robot.txt", FILE_WRITE);
  if (file) {
    file.println("Hello dari KELAS ROBOT!");
    file.println("CYD ESP32 SD Card Test");
    file.println("Write sukses!");
    file.close();
    printStatus("Write OK!", TFT_GREEN);
  } else {
    printStatus("Write GAGAL!", TFT_RED);
    return;
  }

  // Read file
  printStatus("Reading file...", TFT_WHITE);
  file = SD.open("/kelas_robot.txt", FILE_READ);
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      printStatus(line, TFT_CYAN);
    }
    file.close();
    printStatus("Read OK!", TFT_GREEN);
  } else {
    printStatus("Read GAGAL!", TFT_RED);
  }

  // Delete file
  if (SD.remove("/kelas_robot.txt")) {
    printStatus("File dihapus OK!", TFT_GREEN);
  }

  printStatus("Test Selesai!", TFT_YELLOW);
}

void loop() {}