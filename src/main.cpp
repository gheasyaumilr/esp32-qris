#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "QRISArduino.h"
#include <secrets.h>

// ============================================================
// TOUCH PIN
// ============================================================

#define relay 22
int durasi = 0;

#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

// ============================================================
// TFT
// Pin TFT mengikuti platformio.ini
//
// SCLK = GPIO 14
// MOSI = GPIO 13
// MISO = GPIO 12
// CS   = GPIO 15
// DC   = GPIO 2
// RST  = -1
// ============================================================

TFT_eSPI tft = TFT_eSPI();

// ============================================================
// TOUCH SPI
// Menggunakan HSPI agar tidak bentrok dengan SPI TFT
// ============================================================

SPIClass touchSPI(HSPI);

XPT2046_Touchscreen touch(
  TOUCH_CS,
  TOUCH_IRQ
);

// ============================================================
// MAYAR / QRIS
// ============================================================

Mayar mayar(apikey);

TFTKeypad keypad(
  tft,
  touch
);

TFTImageDisplay imageDisplay(
  tft
);

// ============================================================
// APPLICATION STATE
// ============================================================

enum AppState {
  STATE_IDLE,
  STATE_LOADING_GET_BALANCE,
  STATE_LOADING_CREATE_QR,
  STATE_LOADING_DOWNLOAD_QR,
  STATE_SHOW_QR_POLLING,
  STATE_SUCCESS,
  STATE_TIMEOUT,
  STATE_WAITING_BACK
};

AppState currentState = STATE_IDLE;

// ============================================================
// GLOBAL VARIABLES
// ============================================================

unsigned long stateStartTime = 0;
unsigned long pollStartTime  = 0;

long transAmount    = 0;
long initialBalance = 0;

String qrUrl = "";

// ============================================================
// DISPLAY HELPER
// ============================================================

void showMessage(
  const char* msg,
  uint16_t color,
  int y = 140
) {

  tft.fillScreen(
    TFT_BLACK
  );

  tft.setTextSize(3);

  tft.setTextColor(
    color,
    TFT_BLACK
  );

  int textWidth =
    tft.textWidth(msg);

  int x =
    (tft.width() - textWidth) / 2;

  if (x < 10) {
    x = 10;
  }

  tft.setCursor(
    x,
    y
  );

  tft.print(msg);
}

// ============================================================
// QR DOWNLOAD PROGRESS
// ============================================================

void drawQrProgress(
  int percent
) {

  static int lastPct = -1;

  if (percent == lastPct) {
    return;
  }

  lastPct = percent;

  const int BAR_X = 20;
  const int BAR_Y = 270;
  const int BAR_W = 200;
  const int BAR_H = 14;

  int filled =
    map(
      percent,
      0,
      100,
      0,
      BAR_W
    );

  // Background
  tft.fillRect(
    BAR_X,
    BAR_Y,
    BAR_W,
    BAR_H,
    TFT_DARKGREY
  );

  // Progress
  tft.fillRect(
    BAR_X,
    BAR_Y,
    filled,
    BAR_H,
    TFT_GREEN
  );

  // Border
  tft.drawRect(
    BAR_X,
    BAR_Y,
    BAR_W,
    BAR_H,
    TFT_WHITE
  );

  char buf[8];

  sprintf(
    buf,
    "%d%%",
    percent
  );

  tft.fillRect(
    90,
    290,
    60,
    14,
    TFT_BLACK
  );

  tft.setTextSize(1);

  tft.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  int textWidth =
    tft.textWidth(buf);

  int tx =
    (tft.width() - textWidth) / 2;

  tft.setCursor(
    tx,
    290
  );

  tft.print(buf);
}

// ============================================================
// GENERAL PROGRESS
// ============================================================

void drawProgress(
  int percent
) {

  tft.fillRect(
    0,
    80,
    tft.width(),
    160,
    TFT_BLACK
  );

  char buf[10];

  sprintf(
    buf,
    "%d%%",
    percent
  );

  tft.setTextSize(5);

  tft.setTextColor(
    TFT_CYAN,
    TFT_BLACK
  );

  int textWidth =
    tft.textWidth(buf);

  int x =
    (tft.width() - textWidth) / 2;

  tft.setCursor(
    x,
    130
  );

  tft.print(buf);

  int barWidth =
    map(
      percent,
      0,
      100,
      0,
      200
    );

  // Background
  tft.fillRect(
    20,
    190,
    200,
    20,
    TFT_DARKGREY
  );

  // Progress
  tft.fillRect(
    20,
    190,
    barWidth,
    20,
    TFT_GREEN
  );

  // Border
  tft.drawRect(
    20,
    190,
    200,
    20,
    TFT_WHITE
  );
}

// ============================================================
// RESET TO KEYPAD
// ============================================================

void resetToKeypad() {

  currentState =
    STATE_IDLE;

  transAmount =
    0;

  initialBalance =
    0;

  qrUrl =
    "";

  tft.fillScreen(
    TFT_NAVY
  );

  keypad.reset();

  keypad.draw();

  keypad.updateDisplay();
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH);

  delay(500);

  Serial.println();
  Serial.println(
    "================================"
  );

  Serial.println(
    "     KELAS ROBOT - QRIS MAYAR"
  );

  Serial.println(
    "================================"
  );

  // ==========================================================
  // BACKLIGHT
  //
  // Sesuai kode Touch Test:
  // GPIO 27
  // ==========================================================

  pinMode(
    27,
    OUTPUT
  );

  digitalWrite(
    27,
    HIGH
  );

  // ==========================================================
  // TFT
  // ==========================================================

  Serial.println(
    "Initializing TFT..."
  );

  tft.init();

  // Portrait 240x320 (rotasi 0) - layout keypad & QR didesain portrait
  tft.setRotation(0);

  tft.fillScreen(
    TFT_BLACK
  );

  Serial.println(
    "TFT OK"
  );

  // ==========================================================
  // TOUCH SPI
  //
  // HSPI
  //
  // CLK  = 25
  // MISO = 39
  // MOSI = 32
  // CS   = 33
  // ==========================================================

  Serial.println(
    "Initializing Touch SPI..."
  );

  touchSPI.begin(
    TOUCH_CLK,
    TOUCH_MISO,
    TOUCH_MOSI,
    TOUCH_CS
  );

  // ==========================================================
  // XPT2046
  // ==========================================================

  if (!touch.begin(touchSPI)) {

    Serial.println(
      "ERROR: XPT2046 tidak terdeteksi!"
    );

    tft.fillScreen(
      TFT_RED
    );

    tft.setTextColor(
      TFT_WHITE,
      TFT_RED
    );

    tft.setTextSize(2);

    tft.setCursor(
      30,
      140
    );

    tft.print(
      "TOUCH ERROR"
    );

    while (true) {
      delay(100);
    }
  }

  // Rotasi touch harus sama dengan rotasi TFT
  touch.setRotation(0);

  Serial.println(
    "Touch OK"
  );

  Serial.println(
    "Touch SPI = HSPI"
  );

  Serial.println(
    "Touch CS  = GPIO 33"
  );

  Serial.println(
    "Touch IRQ = GPIO 36"
  );

  Serial.println(
    "Touch MOSI = GPIO 32"
  );

  Serial.println(
    "Touch MISO = GPIO 39"
  );

  Serial.println(
    "Touch CLK  = GPIO 25"
  );

  Serial.println(
    "================================"
  );

  // ==========================================================
  // MAYAR DEBUG
  // ==========================================================

  mayar.debug(true);

  // ==========================================================
  // WIFI
  // ==========================================================

  tft.fillScreen(
    TFT_BLACK
  );

  tft.setTextSize(2);

  tft.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  tft.setCursor(
    20,
    150
  );

  tft.print(
    "Connecting WiFi..."
  );

  Serial.println(
    "Connecting WiFi..."
  );

  if (
    !mayar.begin(
      ssid,
      wifiPassword
    )
  ) {

    Serial.println(
      "WiFi Failed"
    );

    tft.fillScreen(
      TFT_RED
    );

    tft.setTextColor(
      TFT_WHITE,
      TFT_RED
    );

    tft.setTextSize(2);

    tft.setCursor(
      20,
      150
    );

    tft.print(
      "WiFi Failed"
    );

    while (true) {
      delay(100);
    }
  }

  Serial.println(
    "WiFi Connected"
  );

  // ==========================================================
  // KEYPAD
  // ==========================================================

  keypad.begin();

  // ----------------------------------------------------------
  // KALIBRASI TOUCH (rotasi 0 / portrait 240x320)
  //
  // Nilai kalibrasi dari test/3_kalibrasi_touchscreen (rotasi 1):
  //   TS_MINX=404 TS_MAXX=3603 TS_MINY=634 TS_MAXY=3568
  //
  // Ditransformasi ke rotasi 0 (XPT2046 rot0: x=4095-y, y=x):
  //   X: 4095-3568=527 .. 4095-634=3461
  //   Y: 404 .. 3603
  //
  // Jika sumbu terbalik (touch kiri-kanan/atas-bawah tertukar),
  // tukar nilai min/max, mis: setCalibration(3461, 527, 3603, 404).
  // ----------------------------------------------------------

  keypad.setCalibration(
    527,
    3461,
    404,
    3603
  );

  keypad.setDebug(true); // ganti false setelah touch benar

  // ==========================================================
  // DIGIT BUTTON
  // ==========================================================

  keypad.onDigit(
    [&](char d) {

      if (
        currentState !=
        STATE_IDLE
      ) {
        return;
      }

      String val =
        keypad.getValue();

      if (
        val.length() < 10
      ) {

        if (
          val == "0"
        ) {
          val = "";
        }

        val += d;

        keypad.setValue(
          val
        );
      }
    }
  );

  // ==========================================================
  // DELETE BUTTON
  // ==========================================================

  keypad.onDel(
    [&]() {

      if (
        currentState !=
        STATE_IDLE
      ) {
        return;
      }

      String val =
        keypad.getValue();

      if (
        val.length()
      ) {

        val.remove(
          val.length() - 1
        );
      }

      keypad.setValue(
        val
      );
    }
  );

  // ==========================================================
  // OK BUTTON
  // ==========================================================

  keypad.onOk(
    [&]() {

      if (
        currentState !=
        STATE_IDLE
      ) {
        return;
      }

      String amountStr =
        keypad.getValue();

      if (
        amountStr.isEmpty() ||
        amountStr.toInt() == 0
      ) {

        tft.fillRect(
          0,
          200,
          tft.width(),
          40,
          TFT_BLACK
        );

        tft.setTextColor(
          TFT_RED,
          TFT_BLACK
        );

        tft.setTextSize(2);

        tft.setCursor(
          20,
          205
        );

        tft.print(
          "Masukkan jumlah!"
        );

        delay(700);

        keypad.updateDisplay();

        return;
      }

      transAmount =
        amountStr.toInt();

      Serial.print(
        "Transaction Amount: Rp "
      );

      Serial.println(
        transAmount
      );

      currentState =
        STATE_LOADING_GET_BALANCE;

      tft.fillScreen(
        TFT_BLACK
      );

      drawProgress(0);
    }
  );

  // ==========================================================
  // SHOW KEYPAD
  // ==========================================================

  resetToKeypad();

  Serial.println(
    "System Ready"
  );
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  // ==========================================================
  // PROCESS TOUCH / KEYPAD
  // ==========================================================

  keypad.process();

  // ==========================================================
  // STATE MACHINE
  // ==========================================================

  switch (
    currentState
  ) {

    // ========================================================
    // IDLE
    // ========================================================

    case STATE_IDLE:

      break;


    // ========================================================
    // GET BALANCE
    // ========================================================

    case STATE_LOADING_GET_BALANCE: {

      drawProgress(20);

      Serial.println(
        "Getting balance..."
      );

      QRISResponse bal =
        mayar.balance();

      if (
        bal.success &&
        bal.balance >= 0
      ) {

        initialBalance =
          bal.balance;

        Serial.print(
          "Initial Balance: "
        );

        Serial.println(
          initialBalance
        );

        currentState =
          STATE_LOADING_CREATE_QR;

      } else {

        Serial.println(
          "Failed to get balance"
        );

        showMessage(
          "Gagal ambil saldo",
          TFT_RED
        );

        delay(1500);

        resetToKeypad();
      }

      break;
    }


    // ========================================================
    // CREATE QR
    // ========================================================

    case STATE_LOADING_CREATE_QR: {

      drawProgress(50);

      Serial.print(
        "Creating QRIS: Rp "
      );

      Serial.println(
        transAmount
      );

      QRISResponse qr =
        mayar.createQris(
          transAmount
        );

      if (
        qr.success &&
        qr.url.length() > 0 &&
        qr.url != "null"
      ) {

        qrUrl =
          qr.url;

        Serial.println(
          "QRIS Created"
        );

        Serial.print(
          "QR URL: "
        );

        Serial.println(
          qrUrl
        );

        currentState =
          STATE_LOADING_DOWNLOAD_QR;

      } else {

        Serial.println(
          "Failed to create QR"
        );

        showMessage(
          "Gagal buat QR",
          TFT_RED
        );

        delay(1500);

        resetToKeypad();
      }

      break;
    }


    // ========================================================
    // DOWNLOAD QR
    // ========================================================

    case STATE_LOADING_DOWNLOAD_QR: {

      tft.fillScreen(
        TFT_BLACK
      );

      // ------------------------------------------------------
      // TITLE
      // ------------------------------------------------------

      tft.setTextSize(2);

      tft.setTextColor(
        TFT_YELLOW,
        TFT_BLACK
      );

      const char* title =
        "SCAN QRIS";

      int titleWidth =
        tft.textWidth(
          title
        );

      tft.setCursor(
        (tft.width() - titleWidth) / 2,
        8
      );

      tft.print(
        title
      );

      // ------------------------------------------------------
      // QR SIZE
      // ------------------------------------------------------

      const int QR_SIZE =
        200;

      const int QR_X =
        (tft.width() - QR_SIZE) / 2;

      const int QR_Y =
        55;

      tft.fillRect(
        QR_X,
        QR_Y,
        QR_SIZE,
        QR_SIZE,
        TFT_BLACK
      );

      // ------------------------------------------------------
      // DOWNLOAD TEXT
      // ------------------------------------------------------

      tft.setTextSize(1);

      tft.setTextColor(
        TFT_CYAN,
        TFT_BLACK
      );

      const char* msg =
        "Mengunduh QR...";

      int msgWidth =
        tft.textWidth(
          msg
        );

      tft.setCursor(
        (tft.width() - msgWidth) / 2,
        258
      );

      tft.print(
        msg
      );

      // ------------------------------------------------------
      // RESIZE IMAGE URL
      // ------------------------------------------------------

      String resizedUrl =
        Mayar::resizeImageUrl(
          qrUrl,
          QR_SIZE
        );

      imageDisplay.setOffset(
        QR_X,
        QR_Y
      );

      // ------------------------------------------------------
      // DOWNLOAD PROGRESS
      // ------------------------------------------------------

      auto progressCb =
        [&](int pct) {

          drawQrProgress(
            pct
          );
        };

      // ------------------------------------------------------
      // SHOW QR
      // ------------------------------------------------------

      bool ok =
        imageDisplay.show(
          resizedUrl,
          QR_SIZE,
          QR_SIZE,
          20,
          progressCb
        );

      imageDisplay.resetOffset();

      // ------------------------------------------------------
      // QR SUCCESS
      // ------------------------------------------------------

      if (ok) {

        tft.fillRect(
          0,
          258,
          tft.width(),
          62,
          TFT_BLACK
        );

        tft.setTextSize(1);

        tft.setTextColor(
          TFT_YELLOW,
          TFT_BLACK
        );

        const char* waiting =
          "Menunggu pembayaran...";

        int waitingWidth =
          tft.textWidth(
            waiting
          );

        tft.setCursor(
          (tft.width() - waitingWidth) / 2,
          308
        );

        tft.print(
          waiting
        );

        currentState =
          STATE_SHOW_QR_POLLING;

        pollStartTime =
          millis();

      } else {

        Serial.println(
          "Failed to download QR"
        );

        showMessage(
          "Gagal muat QR",
          TFT_RED
        );

        delay(1500);

        resetToKeypad();
      }

      break;
    }


    // ========================================================
    // POLLING PAYMENT
    // ========================================================

    case STATE_SHOW_QR_POLLING: {

      static unsigned long lastPoll =
        0;

      unsigned long now =
        millis();

      // Poll setiap 3 detik
      if (
        now - lastPoll >=
        3000
      ) {

        lastPoll =
          now;

        // ----------------------------------------------------
        // ANIMATION
        // ----------------------------------------------------

        static bool dot =
          false;

        tft.fillRect(
          225,
          306,
          12,
          12,
          TFT_BLACK
        );

        tft.fillCircle(
          231,
          312,
          4,
          dot
            ? TFT_GREEN
            : TFT_DARKGREY
        );

        dot =
          !dot;

        // ----------------------------------------------------
        // CHECK BALANCE
        // ----------------------------------------------------

        Serial.println(
          "Checking payment..."
        );

        QRISResponse bal =
          mayar.balance();

        if (
          bal.success &&
          bal.balance >
          initialBalance
        ) {

          Serial.println(
            "PAYMENT SUCCESS!"
          );

          currentState =
            STATE_SUCCESS;

        } else if (
          now - pollStartTime >
          300000UL
        ) {

          Serial.println(
            "PAYMENT TIMEOUT"
          );

          currentState =
            STATE_TIMEOUT;
        }
      }

      break;
    }


    // ========================================================
    // SUCCESS
    // ========================================================

    case STATE_SUCCESS: {

      tft.fillScreen(
        TFT_BLACK
      );

      // ------------------------------------------------------
      // TITLE
      // ------------------------------------------------------

      tft.setTextSize(3);

      tft.setTextColor(
        TFT_GREEN,
        TFT_BLACK
      );

      const char* success =
        "BERHASIL!";

      int successWidth =
        tft.textWidth(
          success
        );

      tft.setCursor(
        (tft.width() - successWidth) / 2,
        90
      );

      tft.print(
        success
      );

      // ------------------------------------------------------
      // AMOUNT
      // ------------------------------------------------------

      tft.setTextSize(2);

      tft.setTextColor(
        TFT_WHITE,
        TFT_BLACK
      );

      char amtBuf[24];

      sprintf(
        amtBuf,
        "Rp %ld",
        transAmount
      );

      int amountWidth =
        tft.textWidth(
          amtBuf
        );

      tft.setCursor(
        (tft.width() - amountWidth) / 2,
        145
      );

      tft.print(
        amtBuf
      );

      //LOGIKA KETIKA PEMBAYARAN BERHASIL, AKAN MENYALAKAN RELAY SELAMA 10 DETIK PER 1000 RUPIAH
                durasi = transAmount * 10;
          //1000 ruiah = 10 detik
                    //nyalakan relay
          digitalWrite(relay, LOW);
          delay(durasi);
          digitalWrite(relay, HIGH);

      // ------------------------------------------------------
      // BACK MESSAGE
      // ------------------------------------------------------

      tft.setTextSize(1);

      tft.setTextColor(
        TFT_CYAN,
        TFT_BLACK
      );

      const char* back =
        "Sentuh layar untuk kembali";

      int backWidth =
        tft.textWidth(
          back
        );

      tft.setCursor(
        (tft.width() - backWidth) / 2,
        215
      );

      tft.print(
        back
      );

      currentState =
        STATE_WAITING_BACK;

      stateStartTime =
        millis();

      break;
    }


    // ========================================================
    // TIMEOUT
    // ========================================================

    case STATE_TIMEOUT: {

      tft.fillScreen(
        TFT_BLACK
      );

      // ------------------------------------------------------
      // TITLE
      // ------------------------------------------------------

      tft.setTextSize(3);

      tft.setTextColor(
        TFT_RED,
        TFT_BLACK
      );

      const char* timeout =
        "TIMEOUT";

      int timeoutWidth =
        tft.textWidth(
          timeout
        );

      tft.setCursor(
        (tft.width() - timeoutWidth) / 2,
        130
      );

      tft.print(
        timeout
      );

      // ------------------------------------------------------
      // BACK MESSAGE
      // ------------------------------------------------------

      tft.setTextSize(1);

      tft.setTextColor(
        TFT_CYAN,
        TFT_BLACK
      );

      const char* back =
        "Sentuh layar untuk kembali";

      int backWidth =
        tft.textWidth(
          back
        );

      tft.setCursor(
        (tft.width() - backWidth) / 2,
        215
      );

      tft.print(
        back
      );

      currentState =
        STATE_WAITING_BACK;

      stateStartTime =
        millis();

      break;
    }


    // ========================================================
    // WAITING BACK
    // ========================================================

    case STATE_WAITING_BACK: {

      // ------------------------------------------------------
      // AUTO RESET 3 MENIT
      // ------------------------------------------------------

      if (
        millis() - stateStartTime >
        180000UL
      ) {

        resetToKeypad();

      }

      // ------------------------------------------------------
      // TOUCH
      // ------------------------------------------------------

      else if (
        touch.touched()
      ) {

        delay(200);

        resetToKeypad();
      }

      break;
    }
  }

  delay(10);
}
