/**
 * QRISArduino - Contoh: Buat QRIS
 * 
 * Contoh ini mendemonstrasikan cara membuat kode QRIS
 * menggunakan Mayar API dari ESP8266 / ESP32.
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

void setup() {
  Serial.begin(115200);
  delay(500);

  // Koneksi WiFi
  Serial.print("Menghubungkan ke WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung! IP: " + WiFi.localIP().toString());

  // Buat QRIS sebesar Rp 15.000
  Serial.println("\nMembuat kode QRIS...");
  QRISResponse qris = api.createQris(15000);

  if (qris.success) {
    Serial.println("✅ QRIS berhasil dibuat!");
    Serial.println("   URL     : " + qris.url);
    Serial.println("   Nominal : Rp " + String(qris.amount));
    Serial.println("   Ref ID  : " + qris.reference_id);
  } else {
    Serial.println("❌ Gagal membuat QRIS");
    Serial.println("   Status  : " + String(qris.status_code));
    Serial.println("   Pesan   : " + qris.message);
  }
}

void loop() {
  // Tidak ada yang perlu dilakukan di loop
}
