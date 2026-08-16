#include <WiFi.h>
#include <HTTPClient.h>
#include <mbedtls/md.h>
#include <ArduinoJson.h>
#include <time.h>

// 1. Konfigurasi WiFi
const char* ssid = "Kelas Robot";
const char* password = "kumaha aa we";

// 2. Konfigurasi API DOKU
const char* host = "https://api-sandbox.doku.com";
const char* endpoint = "/snap-adapter/b2b/v1.0/qr/qr-mpm-generate";
const char* tokenEndpoint = "/authorization/v1.0/access-token/b2b";

// 3. Kredensial Sandbox / Production dari DOKU Dashboard
const char* clientId = "BRN-0265-1786704032234";
const char* clientSecret = "SK-0fNz7a6IkI2YD4fHAmJJ";

// TODO: Ganti dengan MID (mall ID) DOKU yang numeric, contoh "2115"
const char* merchantId = "xxxxx";

String accessToken = "";

// ==== Fungsi kriptografi (SHA-256 & HMAC-SHA512) ====

String sha256Hex(String data) {
    uint8_t hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)data.c_str(), data.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    String hexResult = "";
    for (int i = 0; i < 32; i++) {
        char buf[3];
        sprintf(buf, "%02x", hash[i]);
        hexResult += buf;
    }
    return hexResult;
}

String hmacSha512Hex(String data, String key) {
    uint8_t hmacResult[64];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA512), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key.c_str(), key.length());
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)data.c_str(), data.length());
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    String hexResult = "";
    for (int i = 0; i < 64; i++) {
        char buf[3];
        sprintf(buf, "%02x", hmacResult[i]);
        hexResult += buf;
    }
    return hexResult;
}

// ==== Waktu (NTP, zona WIB = UTC+7) ====

String getCurrentTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "1970-01-01T00:00:00+07:00";
    }
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+07:00", &timeinfo);
    return String(buf);
}

// ==== Ambil B2B Access Token dari DOKU ====

void getAccessToken() {
    HTTPClient http;
    http.begin(String(host) + String(tokenEndpoint));

    String timestamp = getCurrentTimestamp();

    // Request Body token harus persis seperti ini (sudah minified)
    String jsonBody = String("{\"grantType\":\"client_credentials\",")
        + "\"clientId\":\"" + clientId + "\","
        + "\"clientSecret\":\"" + clientSecret + "\"}";

    // Get Token: stringToSign TANPA AccessToken
    String stringToSign = "POST:" + String(tokenEndpoint) + ":"
        + sha256Hex(jsonBody) + ":" + timestamp;
    String signature = hmacSha512Hex(stringToSign, String(clientSecret));

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-PARTNER-ID", clientId);
    http.addHeader("X-TIMESTAMP", timestamp);
    http.addHeader("X-SIGNATURE", signature);

    Serial.println("Mengambil access token...");
    int code = http.POST(jsonBody);

    if (code > 0) {
        String response = http.getString();
        Serial.print("Token HTTP code: ");
        Serial.println(code);

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, response);
        if (!err) {
            accessToken = doc["accessToken"] | "";
            if (!accessToken.isEmpty()) {
                Serial.println("Access token berhasil didapat.");
            } else {
                Serial.print("Token response: ");
                Serial.println(response);
            }
        } else {
            Serial.print("Gagal parse token JSON: ");
            Serial.println(response);
        }
    } else {
        Serial.print("Error ambil token: ");
        Serial.println(code);
        Serial.println(http.getString());
    }

    http.end();
}

// ==== Generate QRIS ====

void generateQRIS() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi terputus!");
        return;
    }

    if (accessToken.isEmpty()) {
        getAccessToken();
        if (accessToken.isEmpty()) {
            Serial.println("Gagal: tidak ada access token.");
            return;
        }
    }

    HTTPClient http;
    http.begin(String(host) + String(endpoint));

    String partnerReferenceNo = "INV-" + String(millis());

    // Request Body minified, tanpa spasi/enter
    String jsonPayload = String("{\"partnerReferenceNo\":\"") + partnerReferenceNo
        + "\",\"amount\":{\"value\":\"10000.00\",\"currency\":\"IDR\"},"
        + "\"merchantId\":\"" + merchantId + "\","
        + "\"terminalId\":\"A01\","
        + "\"additionalInfo\":{\"postalCode\":\"12190\",\"feeType\":\"1\"}}";

    String timestamp = getCurrentTimestamp();

    // X-EXTERNAL-ID harus numeric & unik dalam 1 hari
    String externalId = String(millis()) + String(micros());

    // SNAP: stringToSign HARUS menyertakan AccessToken di tengah
    String stringToSign = "POST:" + String(endpoint) + ":" + accessToken + ":"
        + sha256Hex(jsonPayload) + ":" + timestamp;
    String signature = hmacSha512Hex(stringToSign, String(clientSecret));

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-PARTNER-ID", clientId);
    http.addHeader("X-EXTERNAL-ID", externalId);
    http.addHeader("X-TIMESTAMP", timestamp);
    http.addHeader("X-SIGNATURE", signature);
    http.addHeader("Authorization", "Bearer " + accessToken);
    http.addHeader("CHANNEL-ID", "H2H");

    Serial.println("Mengirim request generate QRIS...");
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        Serial.print("Response JSON: ");
        Serial.println(response);

        // TODO: parse field "qrContent" untuk ditampilkan ke layar TFT
    } else {
        Serial.print("Error saat mengirim POST: ");
        Serial.println(httpResponseCode);
        Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
}

void setup() {
    Serial.begin(115200);

    // Konfigurasi zona waktu WIB (UTC+7)
    configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Terhubung!");

    // Tunggu sinkronisasi waktu NTP (maks 10 detik)
    int ntpTry = 0;
    while (time(nullptr) < 100000 && ntpTry < 20) {
        delay(500);
        ntpTry++;
    }
    Serial.print("Waktu: ");
    Serial.println(getCurrentTimestamp());

    generateQRIS();
}

void loop() {
    delay(1000);
}
