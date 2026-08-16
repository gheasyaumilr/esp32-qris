/**
 * QRISArduino - Contoh: Cek Saldo
 * 
 * Contoh ini mendemonstrasikan cara mengecek saldo akun Mayar
 * dari ESP8266 / ESP32, dengan polling otomatis setiap 30 detik.
 * 
 * Dependencies:
 *   - QRISArduino (library ini)
 *   - ArduinoJson >= 6.x
 */

#include <QRISArduino.h>

// ⚙️ Ganti dengan kredensial WiFi dan API Key kamu
const char* WIFI_SSID    = "NAMA_WIFI_KAMU";
const char* WIFI_PASS    = "PASSWORD_WIFI_KAMU";
const char* MAYAR_APIKEY = "API_KEY_MAYAR_KAMU";

Mayar api(MAYAR_APIKEY);

unsigned long lastCheck = 0;
const unsigned long INTERVAL = 30000; // 30 detik

void cekSaldo() {
  Serial.println("\n[Cek Saldo]");
  QRISResponse resp = api.balance();

  if (resp.success) {
    Serial.println("✅ Berhasil!");
    Serial.println("   Saldo Aktif   : Rp " + String(resp.balance_active));
    Serial.println("   Saldo Pending : Rp " + String(resp.balance_pending));
    Serial.println("   Total Saldo   : Rp " + String(resp.balance));
  } else {
    Serial.println("❌ Gagal cek saldo");
    Serial.println("   Status : " + String(resp.status_code));
    Serial.println("   Pesan  : " + resp.message);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.print("Menghubungkan ke WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung! IP: " + WiFi.localIP().toString());

  cekSaldo(); // Langsung cek saat pertama kali
  lastCheck = millis();
}

void loop() {
  if (millis() - lastCheck >= INTERVAL) {
    cekSaldo();
    lastCheck = millis();
  }
}
