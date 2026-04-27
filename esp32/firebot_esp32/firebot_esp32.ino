/*
 * ============================================================
 *  FireBot — ESP32-CAM Sketch
 *  firebot_esp32.ino
 * ============================================================
 *  Hardware: AI Thinker ESP32-CAM
 *
 *  This sketch is the wireless communication layer of FireBot.
 *  It handles:
 *    - WiFi connection and reconnection
 *    - UART receive from Arduino UNO (sensor telemetry packets)
 *    - HTTP web server: serves /data as JSON for the dashboard
 *    - Telegram bot alerts on fire detection
 *    - OV2640 camera capture and photo sending via Telegram
 *
 *  UART packet format from Arduino:
 *    T:28.5,H:62.3,D:45.0,F:1,S:0\n
 *
 *  JSON response at GET /data:
 *    {"temperature":28.5,"humidity":62.3,"distance":45.0,"button":true}
 *
 *  Libraries required (install before compiling):
 *    - ESPAsyncWebServer  (me-no-dev, install from GitHub as ZIP)
 *    - AsyncTCP           (me-no-dev, install from GitHub as ZIP)
 *    - UniversalTelegramBot (Brian Lough, Library Manager)
 *    - ArduinoJson        (Benoit Blanchon, Library Manager)
 *
 *  Board: AI Thinker ESP32-CAM
 *  Flash: UART via USB-TTL adapter (see software-setup.md)
 *
 *  ⚠️  Fill in your credentials in the CONFIG section below.
 * ============================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "esp_camera.h"

// ════════════════════════════════════════════════════════════
//  CONFIG — Fill these in before uploading
// ════════════════════════════════════════════════════════════

const char* WIFI_SSID    = "YourWiFiName";
const char* WIFI_PASS    = "YourWiFiPassword";
const String BOT_TOKEN   = "123456789:ABCDefGhIJKlmNoPQRsTUVwxyZ";
const String CHAT_ID     = "987654321";

// ════════════════════════════════════════════════════════════
//  CAMERA PIN MAP — AI Thinker ESP32-CAM
//  Do not change unless using a different ESP32-CAM module.
// ════════════════════════════════════════════════════════════

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ── Tunable ─────────────────────────────────────────────────

#define WIFI_RETRY_INTERVAL_MS   5000   // retry WiFi every 5s if disconnected
#define TELEGRAM_COOLDOWN_MS    30000   // minimum ms between Telegram alerts
#define UART_BAUD               9600    // must match Arduino sketch

// ── Global Objects ───────────────────────────────────────────

AsyncWebServer    server(80);
WiFiClientSecure  secureClient;
UniversalTelegramBot bot(BOT_TOKEN, secureClient);

// ── Sensor Data (updated from UART) ─────────────────────────

struct SensorData {
  float temperature = 0.0;
  float humidity    = 0.0;
  float distance    = 0.0;
  bool  flame       = false;
  bool  smoke       = false;
} sensorData;

// ── State ────────────────────────────────────────────────────

bool  wasFlameActive          = false; // tracks rising edge for Telegram
unsigned long lastTelegramMsg = 0;
unsigned long lastWifiRetry   = 0;

// ════════════════════════════════════════════════════════════
//  setup()
// ════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(UART_BAUD);  // UART to Arduino UNO

  // Camera init
  initCamera();

  // WiFi
  connectWiFi();

  // Telegram — skip certificate verification for simplicity
  // For production, load the Telegram root CA instead.
  secureClient.setInsecure();

  // Web server routes
  setupWebServer();
  server.begin();

  // Startup Telegram message
  if (WiFi.status() == WL_CONNECTED) {
    String msg = "🤖 *FireBot online*\nIP: `" + WiFi.localIP().toString() + "`";
    bot.sendMessage(CHAT_ID, msg, "Markdown");
  }
}

// ════════════════════════════════════════════════════════════
//  loop()
// ════════════════════════════════════════════════════════════

void loop() {

  // ── Parse incoming UART from Arduino ──────────────────
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      parseTelemetry(line);
    }
  }

  // ── Fire alert — send Telegram on rising edge only ────
  bool fireNow = sensorData.flame || sensorData.smoke;
  if (fireNow && !wasFlameActive) {
    // Rising edge — new fire/smoke event
    if (millis() - lastTelegramMsg >= TELEGRAM_COOLDOWN_MS) {
      sendFireAlert();
      lastTelegramMsg = millis();
    }
  }
  if (!fireNow && wasFlameActive) {
    // Falling edge — fire extinguished
    if (millis() - lastTelegramMsg >= TELEGRAM_COOLDOWN_MS) {
      bot.sendMessage(CHAT_ID,
        "✅ *Fire extinguished.* FireBot resuming patrol.", "Markdown");
      lastTelegramMsg = millis();
    }
  }
  wasFlameActive = fireNow;

  // ── WiFi watchdog — reconnect if dropped ──────────────
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiRetry >= WIFI_RETRY_INTERVAL_MS) {
      WiFi.reconnect();
      lastWifiRetry = millis();
    }
  }
}

// ════════════════════════════════════════════════════════════
//  UART PARSER
// ════════════════════════════════════════════════════════════

/*
 *  parseTelemetry()
 *  Parses the Arduino data packet into the sensorData struct.
 *
 *  Expected format:  T:28.5,H:62.3,D:45.0,F:1,S:0
 *  Fields are comma-separated key:value pairs.
 */
void parseTelemetry(const String& line) {
  // Parse each field by finding "KEY:VALUE" pairs
  sensorData.temperature = extractFloat(line, "T:");
  sensorData.humidity    = extractFloat(line, "H:");
  sensorData.distance    = extractFloat(line, "D:");
  sensorData.flame       = (extractInt(line, "F:") == 1);
  sensorData.smoke       = (extractInt(line, "S:") == 1);
}

/*
 *  extractFloat()
 *  Finds "key" in the string and returns the float value after it.
 *  Returns 0.0 if not found.
 */
float extractFloat(const String& s, const String& key) {
  int idx = s.indexOf(key);
  if (idx == -1) return 0.0;
  return s.substring(idx + key.length()).toFloat();
}

/*
 *  extractInt()
 *  Same as extractFloat but returns an int.
 */
int extractInt(const String& s, const String& key) {
  int idx = s.indexOf(key);
  if (idx == -1) return 0;
  return s.substring(idx + key.length()).toInt();
}

// ════════════════════════════════════════════════════════════
//  WEB SERVER
// ════════════════════════════════════════════════════════════

/*
 *  setupWebServer()
 *  Defines the HTTP routes. The dashboard polls /data every 1.5s.
 *
 *  GET /data   → JSON sensor snapshot
 *  GET /        → minimal status page
 */
void setupWebServer() {

  // CORS header so browser can fetch from any origin
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  // /data — JSON endpoint polled by the dashboard
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest* req) {
    String json = buildJsonResponse();
    req->send(200, "application/json", json);
  });

  // / — simple status page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    String html = "<h2>FireBot Online</h2>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<p><a href='/data'>Sensor Data JSON</a></p>";
    req->send(200, "text/html", html);
  });

  // 404 fallback
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "text/plain", "Not found");
  });
}

/*
 *  buildJsonResponse()
 *  Serialises the current sensorData struct into a JSON string.
 *  The dashboard expects this exact format.
 */
String buildJsonResponse() {
  StaticJsonDocument<200> doc;
  doc["temperature"] = sensorData.temperature;
  doc["humidity"]    = sensorData.humidity;
  doc["distance"]    = sensorData.distance;
  doc["button"]      = sensorData.flame || sensorData.smoke;

  String output;
  serializeJson(doc, output);
  return output;
}

// ════════════════════════════════════════════════════════════
//  TELEGRAM ALERTS
// ════════════════════════════════════════════════════════════

/*
 *  sendFireAlert()
 *  Captures a photo with the OV2640 camera, then sends both
 *  an alert text message and the photo to the Telegram chat.
 */
void sendFireAlert() {
  // Build alert message
  String msg = "🔥 *FIRE DETECTED — FireBot responding!*\n\n";
  msg += "🌡 Temp: " + String(sensorData.temperature, 1) + "°C\n";
  msg += "💧 Humidity: " + String(sensorData.humidity, 0) + "%\n";
  msg += "📡 Distance: " + String(sensorData.distance, 1) + " cm\n";
  msg += "🔥 Flame: " + String(sensorData.flame ? "YES" : "no") + "\n";
  msg += "💨 Smoke: " + String(sensorData.smoke  ? "YES" : "no") + "\n";

  // Send text alert first
  bot.sendMessage(CHAT_ID, msg, "Markdown");

  // Capture and send photo
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) {
    bot.sendPhoto(CHAT_ID, fb->buf, fb->len, "fire_alert.jpg", "image/jpeg");
    esp_camera_fb_return(fb);
  } else {
    bot.sendMessage(CHAT_ID, "⚠️ Camera capture failed.", "");
  }
}

// ════════════════════════════════════════════════════════════
//  WIFI
// ════════════════════════════════════════════════════════════

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  // Even if connection failed we continue — watchdog in loop() retries
}

// ════════════════════════════════════════════════════════════
//  CAMERA INIT — AI Thinker ESP32-CAM
// ════════════════════════════════════════════════════════════

/*
 *  initCamera()
 *  Configures and initialises the OV2640 camera.
 *  Uses JPEG format at SVGA resolution (800×600) for
 *  a good balance of image quality and Telegram upload speed.
 *  Lower to VGA (640×480) if sends are too slow.
 */
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_SVGA;  // 800x600
  config.jpeg_quality = 12;              // 0-63, lower = better quality
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  // Camera init failure is non-fatal — robot continues without photo alerts
}
