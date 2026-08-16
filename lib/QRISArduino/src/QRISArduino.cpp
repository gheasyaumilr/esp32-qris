#include "QRISArduino.h"

// ══════════════════════════════════════════════════════════════════════════════
//  Mayar
// ══════════════════════════════════════════════════════════════════════════════

Mayar::Mayar(const String &apiKey)
    : _apiKey(apiKey), _timeout(10), _insecure(true), _autoReconnect(true),
      _debug(false) {}

void Mayar::setTimeout(int seconds) { _timeout = seconds; }
void Mayar::setInsecure(bool insecure) { _insecure = insecure; }
void Mayar::setAutoReconnect(bool enable) { _autoReconnect = enable; }
void Mayar::debug(bool enable) { _debug = enable; }

bool Mayar::begin(const char *ssid, const char *password, int timeoutSec) {
  _ssid = ssid;
  _password = password;
  if (_debug) {
    Serial.print("\n[WiFi] Connecting to ");
    Serial.println(ssid);
  }
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < (unsigned long)timeoutSec * 1000UL) {
    delay(500);
    if (_debug)
      Serial.print(".");
  }
  bool ok = (WiFi.status() == WL_CONNECTED);
  if (_debug) {
    Serial.println();
    if (ok) {
      Serial.print("[WiFi] Connected! IP: ");
      Serial.println(WiFi.localIP());
    } else
      Serial.println("[WiFi] Connection failed.");
  }
  return ok;
}

bool Mayar::isConnected() { return WiFi.status() == WL_CONNECTED; }

bool Mayar::reconnect() {
  if (_ssid.length() == 0)
    return false;
  if (_debug)
    Serial.println("\n[WiFi] Reconnecting...");
  WiFi.disconnect();
  delay(100);
  WiFi.begin(_ssid.c_str(), _password.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL)
    delay(500);
  bool ok = (WiFi.status() == WL_CONNECTED);
  if (_debug)
    Serial.println(ok ? "[WiFi] Reconnected." : "[WiFi] Reconnection failed.");
  return ok;
}

bool Mayar::_ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return true;
  if (!_autoReconnect)
    return false;
  if (_debug)
    Serial.println("\n[WiFi] Connection lost, reconnecting...");
  return reconnect();
}

QRISResponse Mayar::balance() {
  if (_debug)
    Serial.println("\n[Mayar] Checking balance...");
  return _request("GET", "/balance");
}

QRISResponse Mayar::createQris(long amount) {
  if (_debug) {
    Serial.print("\n[Mayar] Creating QRIS for Rp ");
    Serial.println(amount);
  }
  JsonDocument doc;
  doc["amount"] = amount;
  String payload;
  serializeJson(doc, payload);
  return _request("POST", "/qrcode/create", payload);
}

QRISResponse Mayar::_request(const String &method, const String &endpoint,
                             const String &payload) {
  QRISResponse result;
  result.success = false;
  result.status_code = 0;

  if (!_ensureWiFi()) {
    result.message = "WiFi tidak terhubung";
    return result;
  }

  WiFiClientSecure client;
  if (_insecure)
    client.setInsecure();

  HTTPClient http;
  String url = String(MAYAR_BASE_URL) + endpoint;
  http.begin(client, url);
  http.setTimeout(_timeout * 1000);
  http.addHeader("Authorization", "Bearer " + _apiKey);
  http.addHeader("Content-Type", "application/json");

  int httpCode = (method == "GET") ? http.GET() : http.POST(payload);

  if (httpCode > 0) {
    String body = http.getString();
    result = _parseResponse(httpCode, body);
  } else {
    result.message = "Server connection failed";
    if (_debug) {
      Serial.print("[Mayar] Network error: ");
      Serial.println(http.errorToString(httpCode));
    }
  }
  http.end();
  return result;
}

QRISResponse Mayar::_parseResponse(int httpCode, const String &body) {
  QRISResponse result;
  result.rawJson = body;
  result.success = false;
  result.status_code = httpCode;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    result.message = "Unrecognized server response";
    return result;
  }

  result.status_code = doc["statusCode"] | httpCode;
  result.message = doc["messages"].as<String>();
  result.success = (result.status_code == 200);

  JsonObject data = doc["data"];
  if (!data.isNull()) {
    result.url = data["url"].as<String>();
    result.amount = data["amount"].as<long>();
    result.reference_id = data["referenceId"].as<String>();
    result.balance = data["balance"].as<long>();
    result.balance_pending = data["balancePending"].as<long>();
    result.balance_active = data["balanceActive"].as<long>();
  }

  if (_debug) {
    if (result.success) {
      if (result.balance > 0) {
        Serial.print("[Mayar] Balance: Rp ");
        Serial.print(result.balance);
        Serial.print(" (active: Rp ");
        Serial.print(result.balance_active);
        Serial.println(")");
      }
      if (result.url.length() > 0 && result.url != "null") {
        Serial.println("[Mayar] QRIS created successfully.");
        Serial.print("[Mayar] URL: ");
        Serial.println(result.url);
      }
    } else {
      Serial.print("[Mayar] Failed. Server message: ");
      Serial.println(result.message);
    }
  }
  return result;
}

String Mayar::resizeImageUrl(const String &url, int newSize) {
  int start = url.indexOf("/resized/");
  if (start == -1)
    return url;
  start += 9; // skip "/resized/"
  int end = url.indexOf("/", start);
  if (end == -1)
    return url;
  return url.substring(0, start) + String(newSize) + url.substring(end);
}

bool Mayar::waitForPayment(long initialBalance, unsigned long pollIntervalMs,
                           unsigned long timeoutMs,
                           std::function<void(long, unsigned long)> callback) {
  unsigned long start = millis();
  if (_debug)
    Serial.println("\n[Mayar] Waiting for payment...");

  while (millis() - start < timeoutMs) {
    QRISResponse bal = balance();
    if (bal.success) {
      long current = bal.balance;
      unsigned long elapsed = millis() - start;
      if (_debug) {
        Serial.print("[Mayar] Polling... balance: Rp ");
        Serial.print(current);
        Serial.print(" (");
        Serial.print(elapsed / 1000);
        Serial.println("s)");
      }
      if (current > initialBalance) {
        if (_debug) {
          Serial.print("[Mayar] Payment received! Rp ");
          Serial.println(current);
        }
        if (callback)
          callback(current, elapsed);
        return true;
      }
      if (callback)
        callback(current, elapsed);
    } else {
      if (_debug)
        Serial.println("[Mayar] Balance check failed, retrying...");
    }
    delay(pollIntervalMs);
  }

  if (_debug)
    Serial.println("[Mayar] Payment timed out.");
  return false;
}

// ══════════════════════════════════════════════════════════════════════════════
//  TFTKeypad
// ══════════════════════════════════════════════════════════════════════════════

const int TFTKeypad::SCREEN_W = 240;
const int TFTKeypad::SCREEN_H = 320;
const int TFTKeypad::DISP_H = 102;
const int TFTKeypad::KEY_PAD = 8;
const int TFTKeypad::KEY_GAP = 3;
const int TFTKeypad::KEY_W = 68;
const int TFTKeypad::KEY_H = 47;
const int TFTKeypad::KEY_START_X = KEY_PAD;
const int TFTKeypad::KEY_START_Y = DISP_H + 10;
const int TFTKeypad::DEBOUNCE_MS = 60;
const char TFTKeypad::KEY_LABEL[4][3] = {
    {'1', '2', '3'}, {'4', '5', '6'}, {'7', '8', '9'}, {'C', '0', 'O'}};

#define KP_C_BG 0x18C3
#define KP_C_DISPLAY 0x2945
#define KP_C_KEY 0x39C7
#define KP_C_KEY_DARK 0x2945
#define KP_C_PRESS 0x1082
#define KP_C_ACCENT 0x039F
#define KP_C_TEXT 0xFFFF
#define KP_C_LABEL 0x8C51
#define KP_C_DIV 0x4208

TFTKeypad::TFTKeypad(Adafruit_ILI9341 &display, Adafruit_FT6206 &touch)
    : _tft(&display), _ctp(&touch), _value(""), _lastTouch(0),
      _wasReleased(true) {}

void TFTKeypad::begin() {
  _tft->fillScreen(KP_C_BG);
  updateDisplay();
  draw();
}
void TFTKeypad::draw() {
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 3; c++)
      drawKey(r, c, false);
}

void TFTKeypad::updateDisplay() {
  _tft->fillRoundRect(10, 16, SCREEN_W - 20, 74, 16, KP_C_DISPLAY);
  _tft->setTextSize(1);
  _tft->setTextColor(KP_C_LABEL);
  _tft->setCursor(22, 26);
  _tft->print(F("Jumlah Pembayaran (Rp)"));

  String disp = _value.isEmpty() ? "0" : _value;
  _tft->setTextSize(3);
  _tft->setTextColor(KP_C_TEXT);
  int16_t bx, by;
  uint16_t bw, bh;
  _tft->getTextBounds(disp.c_str(), 0, 0, &bx, &by, &bw, &bh);
  int xPos = SCREEN_W - 22 - bw - bx;
  if (xPos < 22)
    xPos = 22;
  _tft->fillRect(22, 46, SCREEN_W - 44, 40, KP_C_DISPLAY);
  _tft->setCursor(xPos, 52);
  _tft->print(disp);

  _tft->drawFastHLine(15, DISP_H - 4, SCREEN_W - 30, KP_C_DIV);
}

void TFTKeypad::setValue(const String &val) {
  _value = val;
  updateDisplay();
}
String TFTKeypad::getValue() { return _value; }
void TFTKeypad::reset() {
  _value = "";
  updateDisplay();
}

void TFTKeypad::onDigit(std::function<void(char)> cb) { _digitCb = cb; }
void TFTKeypad::onDel(std::function<void()> cb) { _delCb = cb; }
void TFTKeypad::onOk(std::function<void()> cb) { _okCb = cb; }

void TFTKeypad::process() {
  if (!_ctp->touched()) {
    _wasReleased = true;
    delay(3);
    return;
  }
  if (!_wasReleased) {
    delay(3);
    return;
  }

  unsigned long now = millis();
  if (now - _lastTouch < (unsigned long)DEBOUNCE_MS) {
    delay(3);
    return;
  }

  _wasReleased = false;
  _lastTouch = now;

  TS_Point p = _ctp->getPoint();
  int tx = constrain(SCREEN_W - p.x, 0, SCREEN_W - 1);
  int ty = constrain(SCREEN_H - p.y, 0, SCREEN_H - 1);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      if (!hit(tx, ty, r, c))
        continue;

      drawKey(r, c, true);
      delay(8);
      drawKey(r, c, false);
      char k = KEY_LABEL[r][c];

      if (k >= '0' && k <= '9') {
        if (_digitCb) {
          _digitCb(k);
        } else {
          if (_value.length() < 10) {
            if (_value == "0")
              _value = "";
            _value += k;
            updateDisplay();
          }
        }
      } else if (k == 'C') {
        if (_delCb) {
          _delCb();
        } else {
          if (_value.length()) {
            _value.remove(_value.length() - 1);
            updateDisplay();
          }
        }
      } else if (k == 'O') {
        if (_okCb)
          _okCb();
      }
      return;
    }
  }
}

bool TFTKeypad::hit(int px, int py, int row, int col) {
  int x = KEY_START_X + col * (KEY_W + KEY_GAP);
  int y = KEY_START_Y + row * (KEY_H + KEY_GAP);
  return (px >= x && px < x + KEY_W && py >= y && py < y + KEY_H);
}

void TFTKeypad::drawKey(int row, int col, bool pressed) {
  int x = KEY_START_X + col * (KEY_W + KEY_GAP);
  int y = KEY_START_Y + row * (KEY_H + KEY_GAP);
  char k = KEY_LABEL[row][col];

  uint16_t bg = pressed                  ? KP_C_PRESS
                : (k == 'O')             ? KP_C_ACCENT
                : (k == 'C' || k == '0') ? KP_C_KEY_DARK
                                         : KP_C_KEY;
  uint16_t fg = (k == 'O') ? 0xFFFF : KP_C_TEXT;

  _tft->fillRoundRect(x, y, KEY_W, KEY_H, 12, bg);

  const char *str;
  char single[2] = {k, 0};
  if (k == 'C')
    str = "DEL";
  else if (k == 'O')
    str = "OK";
  else
    str = single;

  _tft->setTextSize((k == 'O') ? 2 : 3);
  _tft->setTextColor(fg);

  int16_t bx, by;
  uint16_t bw, bh;
  _tft->getTextBounds(str, 0, 0, &bx, &by, &bw, &bh);
  _tft->setCursor(x + (KEY_W - bw) / 2 - bx, y + (KEY_H - bh) / 2 - by + 1);
  _tft->print(str);
}

// ══════════════════════════════════════════════════════════════════════════════
//  TFTImageDisplay
// ══════════════════════════════════════════════════════════════════════════════

TFTImageDisplay *TFTImageDisplay::_instance = nullptr;

TFTImageDisplay::TFTImageDisplay(Adafruit_GFX &display)
    : _display(&display), _offsetX(0), _offsetY(0), _offsetOverride(false) {}

bool TFTImageDisplay::show(const String &url, int targetWidth, int targetHeight,
                           int timeoutSec,
                           std::function<void(int percent)> progressCb) {
  _targetW = (targetWidth > 0) ? targetWidth : _display->width();
  _targetH = (targetHeight > 0) ? targetHeight : _display->height();

  // Compute auto offset only when the caller has NOT locked a position.
  // This prevents overwriting a setOffset() call made just before show().
  if (!_offsetOverride) {
    _offsetX = (_display->width() - _targetW) / 2;
    _offsetY = (_display->height() - _targetH) / 2;
  }

  _instance = this;

  // Clear only the render area – leaves surrounding UI intact
  _display->fillRect(_offsetX, _offsetY, _targetW, _targetH, ILI9341_BLACK);

  return downloadAndDecode(url, timeoutSec, progressCb);
}

void TFTImageDisplay::setOffset(int x, int y) {
  _offsetX = x;
  _offsetY = y;
  _offsetOverride = true;
}

void TFTImageDisplay::resetOffset() { _offsetOverride = false; }

bool TFTImageDisplay::downloadAndDecode(
    const String &url, int timeoutSec,
    std::function<void(int percent)> progressCb) {
  // ── Parse URL ──────────────────────────────────────────────────────────────
  String host, path;
  bool useSSL;

  if (url.startsWith("https://")) {
    host = url.substring(8);
    useSSL = true;
  } else if (url.startsWith("http://")) {
    host = url.substring(7);
    useSSL = false;
  } else {
    Serial.println("[Image] Unrecognized URL format.");
    return false;
  }

  int slashIdx = host.indexOf('/');
  if (slashIdx == -1) {
    Serial.println("[Image] Invalid URL.");
    return false;
  }
  path = host.substring(slashIdx);
  host = host.substring(0, slashIdx);

  const size_t MAX_IMG_SIZE = 50000;
  const size_t READ_CHUNK = 512;

  for (int retry = 0; retry < 3; retry++) {
    if (retry == 0) {
      Serial.println("[Image] Downloading QR code...");
    } else {
      Serial.print("[Image] Retrying (");
      Serial.print(retry + 1);
      Serial.println("/3)...");
      delay(2000UL * retry);
      WiFi.disconnect();
      delay(500);
      WiFi.reconnect();
      unsigned long tw = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - tw < 8000)
        delay(200);
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Image] WiFi not ready, skipping retry.");
        continue;
      }
      delay(300);
    }

    // ── Open connection ───────────────────────────────────────────────────
    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    Client *clientPtr = nullptr;

    if (useSSL) {
      secureClient.setInsecure();
      secureClient.setTimeout(timeoutSec);
      if (!secureClient.connect(host.c_str(), 443)) {
        Serial.println("[Image] Could not connect to image server (SSL).");
        secureClient.stop();
        continue;
      }
      clientPtr = &secureClient;
    } else {
      plainClient.setTimeout(timeoutSec);
      if (!plainClient.connect(host.c_str(), 80)) {
        Serial.println("[Image] Could not connect to image server.");
        continue;
      }
      clientPtr = &plainClient;
    }

    // ── Send HTTP GET ─────────────────────────────────────────────────────
    clientPtr->printf("GET %s HTTP/1.1\r\n", path.c_str());
    clientPtr->printf("Host: %s\r\n", host.c_str());
    clientPtr->println("Connection: close");
    clientPtr->println("User-Agent: ESP32");
    clientPtr->println();

    // ── Read status line ──────────────────────────────────────────────────
    unsigned long t = millis();
    String statusLine;
    while (clientPtr->connected() && millis() - t < 5000) {
      if (clientPtr->available()) {
        statusLine = clientPtr->readStringUntil('\n');
        break;
      }
      delay(10);
    }

    if (statusLine.indexOf("200") == -1) {
      if (statusLine.indexOf("301") != -1 || statusLine.indexOf("302") != -1)
        Serial.println("[Image] Redirect received (not followed).");
      else
        Serial.println("[Image] Server rejected the request.");
      clientPtr->stop();
      continue;
    }

    // ── Read headers ──────────────────────────────────────────────────────
    int contentLength = -1;
    t = millis();
    while (clientPtr->connected() && millis() - t < 5000) {
      if (clientPtr->available()) {
        String hdr = clientPtr->readStringUntil('\n');
        hdr.trim();
        if (hdr.length() == 0)
          break; // blank line = end of headers
        String hdrLow = hdr;
        hdrLow.toLowerCase();
        if (hdrLow.startsWith("content-length:"))
          contentLength = hdr.substring(hdr.indexOf(':') + 1).toInt();
      } else {
        delay(5);
      }
    }

    // ── Allocate buffer ───────────────────────────────────────────────────
    size_t bufSize = (contentLength > 0 && contentLength <= (int)MAX_IMG_SIZE)
                         ? (size_t)contentLength
                         : MAX_IMG_SIZE;

    uint8_t *pngBuffer = (uint8_t *)malloc(bufSize);
    if (!pngBuffer) {
      Serial.println("[Image] Malloc failed, retrying...");
      clientPtr->stop();
      delay(500);
      continue;
    }

    // ── Download body ─────────────────────────────────────────────────────
    size_t idx = 0;
    unsigned long dlStart = millis();
    unsigned long lastActivity = millis();
    uint8_t chunk[READ_CHUNK];

    while ((clientPtr->connected() || clientPtr->available()) &&
           idx < bufSize &&
           (millis() - dlStart) < (unsigned long)(timeoutSec * 1000)) {
      int avail = clientPtr->available();
      if (avail > 0) {
        lastActivity = millis();
        int toRead = min((int)(bufSize - idx), min(avail, (int)READ_CHUNK));
        int got = clientPtr->readBytes(chunk, toRead);
        memcpy(pngBuffer + idx, chunk, got);
        idx += got;

        if (progressCb) {
          int pct = (contentLength > 0)
                        ? (int)((unsigned long)idx * 100 / contentLength)
                        : min((int)(idx * 100 / 10000), 99);
          progressCb(pct);
        }

        if (contentLength > 0 && (int)idx >= contentLength)
          break;
      } else {
        if (millis() - lastActivity > 3000) {
          Serial.println("[Image] Stalled, stopping download.");
          break;
        }
        delay(5);
        yield();
      }
    }

    clientPtr->stop();

    if (idx < 100) {
      Serial.println("[Image] Too little data, retrying...");
      free(pngBuffer);
      continue;
    }

    // ── Decode PNG ────────────────────────────────────────────────────────
    int rc =
        _png.openRAM(pngBuffer, (int)idx, TFTImageDisplay::_pngDrawCallback);
    if (rc == PNG_SUCCESS) {
      _origW = _png.getWidth();
      _origH = _png.getHeight();
      // _offsetX/_offsetY are already set (either by setOffset() or by show()).
      // Do NOT touch them here.
      _png.decode(NULL, 0);
      _png.close();
      free(pngBuffer);
      Serial.println("[Image] QR code displayed successfully.");
      if (progressCb)
        progressCb(100);
      return true;
    }

    Serial.print("[Image] PNG decode failed (rc=");
    Serial.print(rc);
    Serial.println("), retrying...");
    free(pngBuffer);
  }

  Serial.println("[Image] Failed to display QR after all retries.");
  return false;
}

int TFTImageDisplay::_pngDrawCallback(PNGDRAW *pDraw) {
  return _instance ? _instance->_internalDraw(pDraw) : 0;
}

int TFTImageDisplay::_internalDraw(PNGDRAW *pDraw) {
  uint16_t *lineBuffer = (uint16_t *)malloc(_origW * sizeof(uint16_t));
  if (!lineBuffer)
    return 0;

  _png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);

  float scaleX = (float)_targetW / (float)_origW;
  float scaleY = (float)_targetH / (float)_origH;

  int srcY = pDraw->y;
  int targetY_start = (int)(srcY * scaleY);
  int targetY_end = (int)((srcY + 1) * scaleY) - 1;
  if (targetY_end < targetY_start)
    targetY_end = targetY_start;

  for (int ty = targetY_start; ty <= targetY_end; ty++) {
    int screenY = _offsetY + ty;
    if (screenY < 0 || screenY >= _display->height())
      continue;

    for (int tx = 0; tx < _targetW; tx++) {
      int srcX = (int)(tx / scaleX);
      if (srcX >= _origW)
        srcX = _origW - 1;

      int screenX = _offsetX + tx;
      if (screenX >= 0 && screenX < _display->width())
        _display->drawPixel(screenX, screenY, lineBuffer[srcX]);
    }
  }

  free(lineBuffer);
  return 1;
}
