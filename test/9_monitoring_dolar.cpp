#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ======================================================
// KONFIGURASI
// ======================================================

#define WIFI_SSID         "isi SSID WiFi"
#define WIFI_PASS         "isi password WiFi"

#define REFRESH_MS        300000
#define TIME_UPDATE_MS    1000

#define TIMEZONE_OFFSET   (7 * 3600)
#define NTP_SERVER1       "pool.ntp.org"
#define NTP_SERVER2       "time.nist.gov"

#define SD_CS             5

// ======================================================
// TOUCH
// ======================================================

#define TOUCH_CS          33
#define TOUCH_IRQ         36

#define TOUCH_CLK         25
#define TOUCH_MISO        39
#define TOUCH_MOSI        32

// ======================================================
// BACKLIGHT
// ======================================================

#define BL_PIN            TFT_BL
#define BL_CHANNEL        0
#define BL_FREQ           5000
#define BL_RESOLUTION     8

#define BL_FULL           255
#define BL_DIM            0

#define BL_FADE_STEP      5
#define BL_FADE_DELAY_MS  15

#define IDLE_TIMEOUT_MS   120000

// ======================================================
// STACK
// ======================================================

#define STACK_CLOCK       2048
#define STACK_FETCH       6144
#define STACK_SD          3072
#define STACK_DISPLAY     4096
#define STACK_TOUCH       2048

// ======================================================
// DISPLAY
// ======================================================

#define SCREEN_ROTATION   1

// ======================================================
// SHARED STATE
// ======================================================

typedef struct {

  float currentRate;
  float prevRate;

  float history[100];
  int historyCount;

  char timeStr[9];
  char dateStr[32];
  char lastUpdate[6];

  bool timeSynced;
  bool sdOk;
  bool wifiOk;

  bool needRedraw;
  bool rateUpdated;

  bool screenDimmed;

  unsigned long lastTouchMs;

  int currentBrightness;

} AppState;

// ======================================================
// GLOBAL
// ======================================================

AppState state;

SemaphoreHandle_t stateMutex;
SemaphoreHandle_t displayMutex;

TFT_eSPI tft = TFT_eSPI();

SPIClass touchSPI = SPIClass(VSPI);

XPT2046_Touchscreen ts(
  TOUCH_CS,
  TOUCH_IRQ
);

WiFiClientSecure client;

// ======================================================
// DEBUG
// ======================================================

void debug(String msg) {

  Serial.print(millis());
  Serial.print(": ");
  Serial.println(msg);
}

// ======================================================
// BACKLIGHT
// ======================================================

void initBacklight() {

  ledcSetup(
    BL_CHANNEL,
    BL_FREQ,
    BL_RESOLUTION
  );

  ledcAttachPin(
    BL_PIN,
    BL_CHANNEL
  );

  ledcWrite(
    BL_CHANNEL,
    BL_FULL
  );

  debug("Backlight init OK");
}

// ======================================================
// FADE BRIGHTNESS
// ======================================================

void fadeBrightness(
  int from,
  int to
) {

  int step =
    (to > from)
      ? BL_FADE_STEP
      : -BL_FADE_STEP;

  int current = from;

  while (
    (step > 0 && current < to) ||
    (step < 0 && current > to)
  ) {

    current += step;

    current = constrain(
      current,
      0,
      255
    );

    ledcWrite(
      BL_CHANNEL,
      current
    );

    vTaskDelay(
      pdMS_TO_TICKS(
        BL_FADE_DELAY_MS
      )
    );
  }

  ledcWrite(
    BL_CHANNEL,
    to
  );

  xSemaphoreTake(
    stateMutex,
    portMAX_DELAY
  );

  state.currentBrightness = to;

  xSemaphoreGive(
    stateMutex
  );
}

// ======================================================
// HISTORY
// ======================================================

void addHistory(
  float value
) {

  if (
    state.historyCount < 100
  ) {

    state.history[
      state.historyCount++
    ] = value;

  } else {

    for (
      int i = 0;
      i < 99;
      i++
    ) {

      state.history[i] =
        state.history[i + 1];
    }

    state.history[99] = value;
  }
}

// ======================================================
// SD INIT
// ======================================================

void initSD() {

  debug("Init SD...");

  if (!SD.begin(SD_CS)) {

    debug("SD GAGAL");

    return;
  }

  xSemaphoreTake(
    stateMutex,
    portMAX_DELAY
  );

  state.sdOk = true;

  xSemaphoreGive(
    stateMutex
  );

  debug("SD OK");
}

// ======================================================
// SAVE HISTORY
// ======================================================

void saveHistoryToSD() {

  xSemaphoreTake(
    stateMutex,
    portMAX_DELAY
  );

  bool ok =
    state.sdOk;

  int cnt =
    state.historyCount;

  float snap[100];

  memcpy(
    snap,
    state.history,
    sizeof(float) * cnt
  );

  xSemaphoreGive(
    stateMutex
  );

  if (!ok) {
    return;
  }

  File file =
    SD.open(
      "/rate_history.csv",
      FILE_WRITE
    );

  if (!file) {
    return;
  }

  for (
    int i = 0;
    i < cnt;
    i++
  ) {

    file.print(
      snap[i]
    );

    if (
      i < cnt - 1
    ) {

      file.print(",");
    }
  }

  file.println();

  file.close();

  debug(
    "SD: history saved"
  );
}

// ======================================================
// LOAD HISTORY
// ======================================================

void loadHistoryFromSD() {

  xSemaphoreTake(
    stateMutex,
    portMAX_DELAY
  );

  bool ok =
    state.sdOk;

  xSemaphoreGive(
    stateMutex
  );

  if (!ok) {
    return;
  }

  File file =
    SD.open(
      "/rate_history.csv",
      FILE_READ
    );

  if (!file) {

    debug(
      "SD: no history file"
    );

    return;
  }

  String data =
    file.readString();

  file.close();

  data.trim();

  if (
    data.length() == 0
  ) {
    return;
  }

  xSemaphoreTake(
    stateMutex,
    portMAX_DELAY
  );

  state.historyCount = 0;

  int idx = 0;

  while (
    idx < (int)data.length() &&
    state.historyCount < 100
  ) {

    int comma =
      data.indexOf(
        ',',
        idx
      );

    if (
      comma == -1
    ) {

      comma =
        data.length();
    }

    float val =
      data.substring(
        idx,
        comma
      ).toFloat();

    if (
      val > 0
    ) {

      state.history[
        state.historyCount++
      ] = val;
    }

    idx =
      comma + 1;

    if (
      comma >=
      (int)data.length()
    ) {

      break;
    }
  }

  xSemaphoreGive(
    stateMutex
  );

  debug(
    "SD: loaded " +
    String(state.historyCount) +
    " points"
  );
}

// ======================================================
// WIFI
// ======================================================

void connectWiFi() {

  debug(
    "Connecting WiFi..."
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASS
  );

  int tries = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    tries < 30
  ) {

    vTaskDelay(
      pdMS_TO_TICKS(500)
    );

    tries++;
  }

  bool ok =
    WiFi.status() ==
    WL_CONNECTED;

  xSemaphoreTake(
    stateMutex,
    portMAX_DELAY
  );

  state.wifiOk = ok;

  xSemaphoreGive(
    stateMutex
  );

  if (ok) {

    debug(
      "WiFi OK: " +
      WiFi.localIP().toString()
    );

  } else {

    debug(
      "WiFi GAGAL"
    );
  }
}

// ======================================================
// NTP
// ======================================================

void initNTP() {

  debug(
    "Init NTP..."
  );

  configTime(
    TIMEZONE_OFFSET,
    0,
    NTP_SERVER1,
    NTP_SERVER2
  );

  struct tm timeinfo;

  bool synced = false;

  unsigned long start =
    millis();

  while (
    millis() - start < 10000
  ) {

    if (
      getLocalTime(
        &timeinfo
      )
    ) {

      synced = true;

      break;
    }

    vTaskDelay(
      pdMS_TO_TICKS(500)
    );
  }

  xSemaphoreTake(
    stateMutex,
    portMAX_DELAY
  );

  state.timeSynced =
    synced;

  xSemaphoreGive(
    stateMutex
  );

  debug(
    synced
      ? "NTP OK"
      : "NTP GAGAL"
  );
}

// ======================================================
// FETCH USD → IDR
// ======================================================

float fetchRate() {

  debug(
    "Fetching rate..."
  );

  client.setInsecure();

  HTTPClient http;

  http.begin(
    client,
    "https://open.er-api.com/v6/latest/USD"
  );

  http.setTimeout(
    10000
  );

  int code =
    http.GET();

  if (
    code != 200
  ) {

    http.end();

    debug(
      "HTTP error: " +
      String(code)
    );

    return 0;
  }

  String payload =
    http.getString();

  http.end();

  JsonDocument doc;

  DeserializationError error =
    deserializeJson(
      doc,
      payload
    );

  if (error) {

    debug(
      "JSON error"
    );

    return 0;
  }

  float rate =
    doc["rates"]["IDR"]
      .as<float>();

  debug(
    "Rate: " +
    String(rate)
  );

  return rate;
}

// ======================================================
// GRADIENT
// ======================================================

template<typename T>
void drawGradient(
  T& d
) {

  int w =
    tft.width();

  int h =
    tft.height();

  // True black penuh (bukan gradient navy lagi).
  // Piksel hitam pekat = OFF di panel OLED -> hemat daya
  // dan kontras tinggi ala dark mode.
  d.fillRect(
    0,
    0,
    w,
    h,
    TFT_BLACK
  );
}

// ======================================================
// MINI GRAPH
// ======================================================

template<typename T>
void drawMiniGraph(
  T& d,
  float* hist,
  int cnt
) {

  if (
    cnt < 2
  ) {

    return;
  }

  int gx = 18;
  int gy = 162;
  int gw = 284;
  int gh = 41;

  float minV =
    hist[0];

  float maxV =
    hist[0];

  for (
    int i = 1;
    i < cnt;
    i++
  ) {

    if (
      hist[i] < minV
    ) {

      minV =
        hist[i];
    }

    if (
      hist[i] > maxV
    ) {

      maxV =
        hist[i];
    }
  }

  if (
    (maxV - minV) < 50
  ) {

    maxV += 25;
    minV -= 25;
  }

  float stepX =
    (float)gw /
    (cnt - 1);

  for (
    int i = 0;
    i < cnt;
    i++
  ) {

    int y =
      gy +
      gh -
      map(
        (long)(hist[i] * 100),
        (long)(minV * 100),
        (long)(maxV * 100),
        0,
        gh
      );

    int x =
      gx +
      (int)(
        i * stepX
      );

    d.drawFastVLine(
      x,
      y,
      gy + gh - y,
      d.color565(
        26,
        26,
        26
      )
    );
  }

  for (
    int i = 0;
    i < cnt - 1;
    i++
  ) {

    int x1 =
      gx +
      (int)(
        i * stepX
      );

    int x2 =
      gx +
      (int)(
        (i + 1) * stepX
      );

    int y1 =
      gy +
      gh -
      map(
        (long)(hist[i] * 100),
        (long)(minV * 100),
        (long)(maxV * 100),
        0,
        gh
      );

    int y2 =
      gy +
      gh -
      map(
        (long)(hist[i + 1] * 100),
        (long)(minV * 100),
        (long)(maxV * 100),
        0,
        gh
      );

    uint16_t color =
      (
        hist[i + 1] >= hist[i]
      )
        ? d.color565(
            200,
            40,
            40
          )
        : d.color565(
            40,
            200,
            40
          );

    d.drawLine(
      x1,
      y1,
      x2,
      y2,
      color
    );

    d.fillCircle(
      x2,
      y2,
      2,
      color
    );
  }
}

// ======================================================
// DRAW UI
// ======================================================

template<typename T>
void drawUITo(
  T& d,
  AppState* s
) {

  drawGradient(
    d
  );

  // ====================================
  // TITLE
  // ====================================

  d.setTextColor(
    TFT_CYAN
  );

  d.setTextSize(2);

  d.setCursor(
    10,
    8
  );

  d.print(
    "USD  ->  IDR"
  );

  // ====================================
  // WIFI
  // ====================================

  d.setTextSize(1);

  d.fillCircle(
    248,
    12,
    3,
    s->wifiOk
      ? d.color565(
          0,
          200,
          0
        )
      : d.color565(
          200,
          0,
          0
        )
  );

  d.setTextColor(
    d.color565(
      150,
      150,
      150
    )
  );

  d.setCursor(
    253,
    8
  );

  d.print(
    "WiFi"
  );

  // ====================================
  // SD
  // ====================================

  d.fillCircle(
    178,
    12,
    3,
    s->sdOk
      ? d.color565(
          0,
          200,
          0
        )
      : d.color565(
          200,
          0,
          0
        )
  );

  d.setTextColor(
    d.color565(
      150,
      150,
      150
    )
  );

  d.setCursor(
    183,
    8
  );

  d.print(
    "SD"
  );

  // ====================================
  // DATE
  // ====================================

  d.setTextColor(
    TFT_YELLOW
  );

  d.setCursor(
    10,
    28
  );

  d.print(
    s->dateStr
  );

  // ====================================
  // TIME
  // ====================================

  d.setTextColor(
    TFT_WHITE
  );

  d.setCursor(
    260,
    44
  );

  d.print(
    s->timeStr
  );

  // ====================================
  // RATE CARD
  // ====================================

  uint16_t cardBg =
    TFT_BLACK;

  if (
    s->prevRate > 0 &&
    s->prevRate !=
    s->currentRate
  ) {

    float diff =
      s->currentRate -
      s->prevRate;

    cardBg =
      (diff > 0)
        ? d.color565(
            22,
            5,
            5
          )
        : d.color565(
            5,
            18,
            5
          );
  }

  d.fillRoundRect(
    10,
    68,
    300,
    78,
    8,
    cardBg
  );

  d.drawRoundRect(
    10,
    68,
    300,
    78,
    8,
    d.color565(
      35,
      35,
      35
    )
  );

  d.fillRect(
    13,
    76,
    3,
    62,
    d.color565(
      0,
      160,
      220
    )
  );

  d.setTextColor(
    TFT_WHITE
  );

  d.setTextSize(4);

  d.setCursor(
    25,
    88
  );

  d.printf(
    "Rp %0.0f",
    s->currentRate
  );

  // ====================================
  // RATE DIFFERENCE
  // ====================================

  d.setTextSize(2);

  if (
    s->prevRate > 0 &&
    s->prevRate !=
    s->currentRate
  ) {

    float diff =
      s->currentRate -
      s->prevRate;

    d.setTextColor(
      diff >= 0
        ? TFT_RED
        : TFT_GREEN
    );

    d.setCursor(
      25,
      128
    );

    if (
      diff >= 0
    ) {

      d.printf(
        "^ +%0.0f",
        diff
      );

    } else {

      d.printf(
        "v %0.0f",
        diff
      );
    }
  }

  // ====================================
  // GRAPH CARD
  // ====================================

  d.fillRoundRect(
    10,
    155,
    300,
    55,
    8,
    TFT_BLACK
  );

  d.drawRoundRect(
    10,
    155,
    300,
    55,
    8,
    d.color565(
      35,
      35,
      35
    )
  );

  d.fillRect(
    13,
    163,
    3,
    39,
    d.color565(
      0,
      130,
      180
    )
  );

  drawMiniGraph(
    d,
    s->history,
    s->historyCount
  );

  // ====================================
  // SOURCE
  // ====================================

  d.setTextColor(
    d.color565(
      85,
      85,
      85
    )
  );

  d.setTextSize(1);

  d.setCursor(
    10,
    222
  );

  d.print(
    "Source: open.er-api.com"
  );

  d.setCursor(
    220,
    222
  );

  d.print(
    "Upd: "
  );

  d.print(
    s->lastUpdate
  );
}

// ======================================================
// FULL UI
// ======================================================

void drawFullUI(
  AppState* s
) {

  int w =
    tft.width();

  int h =
    tft.height();

  TFT_eSprite spr(
    &tft
  );

  if (
    spr.createSprite(
      w,
      h
    )
  ) {

    drawUITo(
      spr,
      s
    );

    spr.pushSprite(
      0,
      0
    );

    spr.deleteSprite();

  } else {

    drawUITo(
      tft,
      s
    );
  }
}

// ======================================================
// CLOCK ONLY
// ======================================================

void updateClockOnly(
  AppState* s
) {

  tft.fillRect(
    258,
    44,
    54,
    10,
    TFT_BLACK
  );

  tft.setTextColor(
    TFT_WHITE
  );

  tft.setTextSize(1);

  tft.setCursor(
    260,
    44
  );

  tft.printf(
    "%-8s",
    s->timeStr
  );
}

// ======================================================
// TASK CLOCK
// ======================================================

void taskClock(
  void* pv
) {

  TickType_t lastWake =
    xTaskGetTickCount();

  for (;;) {

    xSemaphoreTake(
      stateMutex,
      portMAX_DELAY
    );

    if (
      state.timeSynced
    ) {

      struct tm t;

      if (
        getLocalTime(&t)
      ) {

        char newTime[9];
        char newDate[32];

        strftime(
          newTime,
          sizeof(newTime),
          "%H:%M:%S",
          &t
        );

        strftime(
          newDate,
          sizeof(newDate),
          "%A, %d %B %Y",
          &t
        );

        if (
          strcmp(
            state.timeStr,
            newTime
          ) != 0
        ) {

          strcpy(
            state.timeStr,
            newTime
          );
        }

        if (
          strcmp(
            state.dateStr,
            newDate
          ) != 0
        ) {

          strcpy(
            state.dateStr,
            newDate
          );

          state.needRedraw =
            true;
        }
      }
    }

    xSemaphoreGive(
      stateMutex
    );

    vTaskDelayUntil(
      &lastWake,
      pdMS_TO_TICKS(
        TIME_UPDATE_MS
      )
    );
  }
}

// ======================================================
// TASK FETCH
// ======================================================

void taskFetch(
  void* pv
) {

  vTaskDelay(
    pdMS_TO_TICKS(2000)
  );

  for (;;) {

    if (
      WiFi.status() !=
      WL_CONNECTED
    ) {

      WiFi.disconnect();

      vTaskDelay(
        pdMS_TO_TICKS(1000)
      );

      connectWiFi();
    }

    float rate =
      fetchRate();

    if (
      rate > 0
    ) {

      xSemaphoreTake(
        stateMutex,
        portMAX_DELAY
      );

      state.prevRate =
        state.currentRate;

      state.currentRate =
        rate;

      addHistory(
        rate
      );

      struct tm t;

      if (
        state.timeSynced &&
        getLocalTime(&t)
      ) {

        strftime(
          state.lastUpdate,
          sizeof(state.lastUpdate),
          "%H:%M",
          &t
        );

      } else {

        strcpy(
          state.lastUpdate,
          "--:--"
        );
      }

      state.wifiOk =
        true;

      state.rateUpdated =
        true;

      state.needRedraw =
        true;

      xSemaphoreGive(
        stateMutex
      );
    }

    vTaskDelay(
      pdMS_TO_TICKS(
        REFRESH_MS
      )
    );
  }
}

// ======================================================
// TASK SD
// ======================================================

void taskSD(
  void* pv
) {

  vTaskDelay(
    pdMS_TO_TICKS(5000)
  );

  for (;;) {

    saveHistoryToSD();

    vTaskDelay(
      pdMS_TO_TICKS(60000)
    );
  }
}

// ======================================================
// TASK DISPLAY
// ======================================================

void taskDisplay(
  void* pv
) {

  xSemaphoreTake(
    stateMutex,
    portMAX_DELAY
  );

  AppState snap =
    state;

  state.needRedraw =
    false;

  xSemaphoreGive(
    stateMutex
  );

  xSemaphoreTake(
    displayMutex,
    portMAX_DELAY
  );

  drawFullUI(
    &snap
  );

  xSemaphoreGive(
    displayMutex
  );

  char lastTimeDrawn[9] =
    "";

  for (;;) {

    xSemaphoreTake(
      stateMutex,
      portMAX_DELAY
    );

    bool doRedraw =
      state.needRedraw;

    bool isDimmed =
      state.screenDimmed;

    bool timeChanged =
      strcmp(
        lastTimeDrawn,
        state.timeStr
      ) != 0;

    AppState snap2 =
      state;

    if (
      doRedraw
    ) {

      state.needRedraw =
        false;
    }

    xSemaphoreGive(
      stateMutex
    );

    if (
      !isDimmed
    ) {

      xSemaphoreTake(
        displayMutex,
        portMAX_DELAY
      );

      if (
        doRedraw
      ) {

        drawFullUI(
          &snap2
        );

        strcpy(
          lastTimeDrawn,
          snap2.timeStr
        );

      } else if (
        timeChanged
      ) {

        updateClockOnly(
          &snap2
        );

        strcpy(
          lastTimeDrawn,
          snap2.timeStr
        );
      }

      xSemaphoreGive(
        displayMutex
      );
    }

    vTaskDelay(
      pdMS_TO_TICKS(250)
    );
  }
}

// ======================================================
// TASK TOUCH + BACKLIGHT
// ======================================================

void taskTouch(
  void* pv
) {

  for (;;) {

    bool touched =
      ts.touched();

    xSemaphoreTake(
      stateMutex,
      portMAX_DELAY
    );

    bool wasDimmed =
      state.screenDimmed;

    unsigned long idleMs =
      millis() -
      state.lastTouchMs;

    xSemaphoreGive(
      stateMutex
    );

    // ====================================
    // TOUCH
    // ====================================

    if (
      touched
    ) {

      xSemaphoreTake(
        stateMutex,
        portMAX_DELAY
      );

      state.lastTouchMs =
        millis();

      xSemaphoreGive(
        stateMutex
      );

      // ==================================
      // WAKE SCREEN
      // ==================================

      if (
        wasDimmed
      ) {

        debug(
          "Touch: wake up screen"
        );

        fadeBrightness(
          BL_DIM,
          BL_FULL
        );

        xSemaphoreTake(
          stateMutex,
          portMAX_DELAY
        );

        state.screenDimmed =
          false;

        state.needRedraw =
          true;

        xSemaphoreGive(
          stateMutex
        );

        // Tunggu jari dilepas
        while (
          ts.touched()
        ) {

          vTaskDelay(
            pdMS_TO_TICKS(50)
          );
        }
      }
    }

    // ====================================
    // IDLE TIMEOUT
    // ====================================

    if (
      !wasDimmed &&
      idleMs >=
      IDLE_TIMEOUT_MS
    ) {

      debug(
        "Idle timeout: dimming screen"
      );

      fadeBrightness(
        BL_FULL,
        BL_DIM
      );

      xSemaphoreTake(
        stateMutex,
        portMAX_DELAY
      );

      state.screenDimmed =
        true;

      xSemaphoreGive(
        stateMutex
      );
    }

    vTaskDelay(
      pdMS_TO_TICKS(50)
    );
  }
}

// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(
    115200
  );

  delay(500);

  debug(
    "=== STARTING ==="
  );

  // ====================================
  // STATE
  // ====================================

  memset(
    &state,
    0,
    sizeof(state)
  );

  strcpy(
    state.timeStr,
    "00:00:00"
  );

  strcpy(
    state.dateStr,
    "Unknown, 01 Jan 2000"
  );

  strcpy(
    state.lastUpdate,
    "--:--"
  );

  state.currentRate =
    16000;

  state.prevRate =
    16000;

  state.screenDimmed =
    false;

  state.lastTouchMs =
    millis();

  state.currentBrightness =
    BL_FULL;

  // ====================================
  // MUTEX
  // ====================================

  stateMutex =
    xSemaphoreCreateMutex();

  displayMutex =
    xSemaphoreCreateMutex();

  // ====================================
  // BACKLIGHT
  // ====================================

  initBacklight();

  // ====================================
  // TFT
  // ====================================

  tft.init();

  // PENTING:
  // LANDSCAPE 320x240
  tft.setRotation(
    SCREEN_ROTATION
  );

  tft.fillScreen(
    TFT_BLACK
  );

  tft.setTextColor(
    TFT_CYAN
  );

  tft.setTextSize(2);

  tft.setCursor(
    60,
    100
  );

  tft.print(
    "Booting..."
  );

  Serial.print(
    "TFT Width  : "
  );

  Serial.println(
    tft.width()
  );

  Serial.print(
    "TFT Height : "
  );

  Serial.println(
    tft.height()
  );

  debug(
    "TFT OK"
  );

  // ====================================
  // TOUCH SPI
  // ====================================

  touchSPI.begin(
    TOUCH_CLK,
    TOUCH_MISO,
    TOUCH_MOSI,
    TOUCH_CS
  );

  ts.begin(
    touchSPI
  );

  // Samakan dengan TFT
  ts.setRotation(
    SCREEN_ROTATION
  );

  debug(
    "Touch OK"
  );

  // ====================================
  // WIFI
  // ====================================

  connectWiFi();

  // ====================================
  // NTP
  // ====================================

  initNTP();

  // ====================================
  // SD
  // ====================================

  delay(200);

  initSD();

  if (
    state.sdOk
  ) {

    loadHistoryFromSD();
  }

  // ====================================
  // FETCH AWAL
  // ====================================

  float rate =
    fetchRate();

  if (
    rate > 0
  ) {

    state.currentRate =
      rate;

    state.prevRate =
      rate;

    addHistory(
      rate
    );

    struct tm t;

    if (
      state.timeSynced &&
      getLocalTime(&t)
    ) {

      strftime(
        state.timeStr,
        sizeof(state.timeStr),
        "%H:%M:%S",
        &t
      );

      strftime(
        state.dateStr,
        sizeof(state.dateStr),
        "%A, %d %B %Y",
        &t
      );

      strftime(
        state.lastUpdate,
        sizeof(state.lastUpdate),
        "%H:%M",
        &t
      );
    }
  }

  // ====================================
  // FINAL STATE
  // ====================================

  state.wifiOk =
    (
      WiFi.status() ==
      WL_CONNECTED
    );

  state.needRedraw =
    true;

  state.lastTouchMs =
    millis();

  debug(
    "Free heap: " +
    String(
      ESP.getFreeHeap()
    )
  );

  // ====================================
  // TASKS
  // ====================================

  xTaskCreatePinnedToCore(
    taskClock,
    "Clock",
    STACK_CLOCK,
    NULL,
    3,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    taskFetch,
    "Fetch",
    STACK_FETCH,
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    taskSD,
    "SD",
    STACK_SD,
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    taskDisplay,
    "Display",
    STACK_DISPLAY,
    NULL,
    2,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    taskTouch,
    "Touch",
    STACK_TOUCH,
    NULL,
    3,
    NULL,
    1
  );

  debug(
    "Tasks launched"
  );
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  vTaskDelay(
    pdMS_TO_TICKS(
      10000
    )
  );
}