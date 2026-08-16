#ifndef QRISARDUINO_H
#define QRISARDUINO_H

#include <Arduino.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <HTTPClient.h>
#endif

#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <PNGdec.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include <functional>

#define MAYAR_BASE_URL "https://api.mayar.id/hl/v1"

// ─── Response struct ────────────────────────────────────────────────────────

struct QRISResponse {
  int    status_code    = 0;
  String message        = "";
  bool   success        = false;
  String url            = "";
  long   amount         = 0;
  String reference_id   = "";
  long   balance        = 0;
  long   balance_pending= 0;
  long   balance_active = 0;
  String rawJson        = "";
};

// ─── Mayar API client ───────────────────────────────────────────────────────

class Mayar {
public:
  explicit Mayar(const String& apiKey);

  void setTimeout(int seconds);
  void setInsecure(bool insecure = true);
  void setAutoReconnect(bool enable);
  void debug(bool enable);

  bool begin(const char* ssid, const char* password, int timeoutSec = 30);
  bool isConnected();
  bool reconnect();

  QRISResponse balance();
  QRISResponse createQris(long amount);

  bool waitForPayment(long initialBalance,
                      unsigned long pollIntervalMs,
                      unsigned long timeoutMs,
                      std::function<void(long currentBalance, unsigned long elapsed)> callback = nullptr);

  static String resizeImageUrl(const String& url, int newSize);

private:
  String _apiKey;
  int    _timeout;
  bool   _insecure;
  bool   _autoReconnect;
  bool   _debug;
  String _ssid;
  String _password;

  bool         _ensureWiFi();
  QRISResponse _request(const String& method, const String& endpoint, const String& payload = "");
  QRISResponse _parseResponse(int httpCode, const String& body);
};

// ─── TFT numeric keypad ─────────────────────────────────────────────────────

class TFTKeypad {
public:
  TFTKeypad(Adafruit_ILI9341& display, Adafruit_FT6206& touch);

  void   begin();
  void   draw();
  void   setValue(const String& val);
  String getValue();
  void   reset();
  void   updateDisplay();

  void onDigit(std::function<void(char digit)> callback);
  void onDel(std::function<void()> callback);
  void onOk(std::function<void()> callback);

  void process();

private:
  Adafruit_ILI9341* _tft;
  Adafruit_FT6206*  _ctp;
  String            _value;
  unsigned long     _lastTouch;
  bool              _wasReleased;

  std::function<void(char)> _digitCb;
  std::function<void()>     _delCb;
  std::function<void()>     _okCb;

  static const int  SCREEN_W, SCREEN_H, DISP_H;
  static const int  KEY_PAD, KEY_GAP, KEY_W, KEY_H;
  static const int  KEY_START_X, KEY_START_Y, DEBOUNCE_MS;
  static const char KEY_LABEL[4][3];

  bool hit(int px, int py, int row, int col);
  void drawKey(int row, int col, bool pressed);
};

// ─── TFT PNG image display ──────────────────────────────────────────────────

class TFTImageDisplay {
public:
  explicit TFTImageDisplay(Adafruit_GFX& display);

  /**
   * Download and render a PNG image.
   *
   * @param url          Full HTTP/HTTPS URL of the image.
   * @param targetWidth  Pixel width to render (0 = full screen width).
   * @param targetHeight Pixel height to render (0 = full screen height).
   * @param timeoutSec   Per-attempt timeout in seconds.
   * @param progressCb   Optional callback receiving download progress 0-100.
   * @return true on success, false on failure.
   *
   * The image is rendered at the position set by setOffset().
   * If setOffset() was never called the image is horizontally centred and
   * placed at y = (screen_height - targetH) / 2.
   * Call setOffset() BEFORE show() to place the image precisely.
   */
  bool show(const String& url,
            int targetWidth  = 0,
            int targetHeight = 0,
            int timeoutSec   = 10,
            std::function<void(int percent)> progressCb = nullptr);

  /** Lock the top-left render position. */
  void setOffset(int x, int y);

  /** Return to automatic centering. */
  void resetOffset();

private:
  Adafruit_GFX* _display;
  int  _targetW        = 0;
  int  _targetH        = 0;
  int  _origW          = 0;
  int  _origH          = 0;
  int  _offsetX        = 0;
  int  _offsetY        = 0;
  bool _offsetOverride = false;
  PNG  _png;

  static TFTImageDisplay* _instance;
  static int _pngDrawCallback(PNGDRAW* pDraw);
  int        _internalDraw(PNGDRAW* pDraw);

  bool downloadAndDecode(const String& url,
                         int timeoutSec,
                         std::function<void(int percent)> progressCb);
};

#endif // QRISARDUINO_H
