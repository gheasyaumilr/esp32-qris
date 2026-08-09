#include <Arduino.h>

#define LED_RED   4
#define LED_GREEN 16
#define LED_BLUE  17

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_RED,   OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE,  OUTPUT);

  // Default semua mati (active LOW)
  digitalWrite(LED_RED,   HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE,  HIGH);

  Serial.println("=== RGB LED Test ===");
}

void setRGB(bool r, bool g, bool b) {
  digitalWrite(LED_RED,   r ? LOW : HIGH);
  digitalWrite(LED_GREEN, g ? LOW : HIGH);
  digitalWrite(LED_BLUE,  b ? LOW : HIGH);
}

void loop() {
  Serial.println("RED");
  setRGB(true, false, false);
  delay(1000);

  Serial.println("GREEN");
  setRGB(false, true, false);
  delay(1000);

  Serial.println("BLUE");
  setRGB(false, false, true);
  delay(1000);

  Serial.println("YELLOW (R+G)");
  setRGB(true, true, false);
  delay(1000);

  Serial.println("CYAN (G+B)");
  setRGB(false, true, true);
  delay(1000);

  Serial.println("MAGENTA (R+B)");
  setRGB(true, false, true);
  delay(1000);

  Serial.println("WHITE (R+G+B)");
  setRGB(true, true, true);
  delay(1000);

  Serial.println("OFF");
  setRGB(false, false, false);
  delay(1000);
}