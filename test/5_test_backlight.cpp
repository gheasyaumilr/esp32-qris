#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

struct { uint32_t color; const char* name; } colors[] = {
  { TFT_RED,     "RED"     },
  { TFT_GREEN,   "GREEN"   },
  { TFT_BLUE,    "BLUE"    },
  { TFT_YELLOW,  "YELLOW"  },
  { TFT_CYAN,    "CYAN"    },
  { TFT_MAGENTA, "MAGENTA" },
  { TFT_WHITE,   "WHITE"   },
};
const int numColors = 7;
int colorIndex = 0;

void drawScreen(uint32_t color, const char* name, int brightness) {
  sprite.fillSprite(color);
  sprite.setTextColor(TFT_BLACK);
  sprite.setTextSize(3);
  sprite.setCursor(10, 10);
  sprite.println("KELAS ROBOT");
  sprite.drawFastHLine(0, 45, tft.width(), TFT_BLACK);
  sprite.setTextSize(2);
  sprite.setCursor(10, 55);
  sprite.println(name);
  sprite.setCursor(10, 80);
  sprite.printf("Brightness: %d%%", map(brightness, 0, 255, 0, 100));
  sprite.pushSprite(0, 0); // push ke layar sekaligus, tidak berkedip
}

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 255);

  tft.init();
  tft.setRotation(1);

  // Buat sprite seukuran layar
  sprite.createSprite(tft.width(), tft.height());

  Serial.println("=== Backlight + Color Test ===");
}

void loop() {
  uint32_t color = colors[colorIndex].color;
  const char* name = colors[colorIndex].name;
  colorIndex = (colorIndex + 1) % numColors;

  // Fade out
  for (int i = 255; i >= 0; i -= 3) {
    analogWrite(TFT_BL, i);
    drawScreen(color, name, i);
    delay(10);
  }
  delay(500);

  // Fade in
  for (int i = 0; i <= 255; i += 3) {
    analogWrite(TFT_BL, i);
    drawScreen(color, name, i);
    delay(10);
  }
  delay(1000);
}