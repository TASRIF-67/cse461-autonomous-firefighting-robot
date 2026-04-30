#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// -------------------------------------------------------
// WiFi credentials
// -------------------------------------------------------
const char* ssid     = "Study should be first priority";
const char* password = "iht@739233";

// -------------------------------------------------------
// Telegram config
// -------------------------------------------------------
const char* BOT_TOKEN = "8773277278:AAGQkKPfCAKtXmgHPDlh7_QzvsPiQBp_G5A";
const char* CHAT_ID   = "5259328233";

// -------------------------------------------------------
// AI Thinker ESP32-CAM pin definitions — do NOT change
// -------------------------------------------------------
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

// -------------------------------------------------------
// UART config
// WHEN SWITCHING TO UART:
//   1. Uncomment the three #define lines below
//   2. Uncomment Serial2.begin(...) in setup()
//   3. Uncomment readUART() call in loop()
//   4. DELETE the entire SIMULATION BLOCK in loop()
//   5. Swap DATA_SOURCE string in buildDashboard()
// -------------------------------------------------------
// #define UART_RX   14
// #define UART_TX   15
// #define UART_BAUD 9600

// -------------------------------------------------------
// Sensor variables
// -------------------------------------------------------
float  temperature   = 34.5;
float  humidity      = 62.0;
bool   fireDetected  = false;
bool   flameDetected = false;
int    gasLevel      = 120;
bool   motorRunning  = false;
bool   pumpActive    = false;
String robotStatus   = "Standby";

// -------------------------------------------------------
// Alert state flags
// -------------------------------------------------------
bool alertFireSent  = false;
bool alertClearSent = true;

// -------------------------------------------------------
// Two HTTP servers — port 80 = dashboard, port 81 = stream
// -------------------------------------------------------
WebServer server(80);
WebServer streamServer(81);

// -------------------------------------------------------
// Camera init
// -------------------------------------------------------
bool cameraOK = false;

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
  config.frame_size   = FRAMESIZE_VGA;  // 640x480 — good balance
  config.jpeg_quality = 12;             // 0=best quality, 63=worst
  config.fb_count     = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[Camera] Init FAILED: 0x%x\n", err);
    cameraOK = false;
    return;
  }
  cameraOK = true;
  Serial.println("[Camera] Init OK");
}

// -------------------------------------------------------
// MJPEG stream handler — runs on port 81
// Browser keeps connection open; frames pushed continuously
// -------------------------------------------------------
void handleStream() {
  if (!cameraOK) {
    streamServer.send(503, "text/plain", "Camera not available");
    return;
  }

  WiFiClient client = streamServer.client();
  client.print("HTTP/1.1 200 OK\r\n"
               "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
               "Access-Control-Allow-Origin: *\r\n\r\n");

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { delay(10); continue; }

    client.printf("--frame\r\n"
                  "Content-Type: image/jpeg\r\n"
                  "Content-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    esp_camera_fb_return(fb);
    delay(40);  // ~25 fps — increase delay if WiFi struggles
  }
}

// -------------------------------------------------------
// Telegram sender
// -------------------------------------------------------
void sendTelegram(String message) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  message.replace(" ", "%20");
  message.replace("\n", "%0A");
  message.replace("!", "%21");
  String url = "https://api.telegram.org/bot";
  url += BOT_TOKEN;
  url += "/sendMessage?chat_id=";
  url += CHAT_ID;
  url += "&text=";
  url += message;
  http.begin(url);
  int code = http.GET();
  Serial.println(code > 0
    ? "[Telegram] OK: "    + String(code)
    : "[Telegram] FAIL: "  + http.errorToString(code));
  http.end();
}

// -------------------------------------------------------
// Alert logic — one message per event, one all-clear after
// FIXED: danger now only triggers on actual fire/flame,
//        gasLevel alone no longer sends an alert
// -------------------------------------------------------
void checkAndAlert() {
  bool danger = fireDetected || flameDetected;   // FIX: removed gasLevel > 200

  if (danger && !alertFireSent) {
    String msg = "%F0%9F%94%A5 FIRE ALERT - Robot activated%0A";
    msg += "Temp: "    + String(temperature, 1) + " C%0A";
    msg += "Gas: "     + String(gasLevel)        + " ppm%0A";
    msg += "Fire IR: " + String(fireDetected  ? "YES" : "No") + "%0A";
    msg += "Flame: "   + String(flameDetected ? "YES" : "No") + "%0A";
    msg += "Status: "  + robotStatus;
    sendTelegram(msg);
    alertFireSent  = true;
    alertClearSent = false;
  }

  if (!danger && !alertClearSent) {
    sendTelegram("%E2%9C%85 All Clear - No fire detected. Robot returning to Standby.");
    alertClearSent = true;
    alertFireSent  = false;
  }
}

// -------------------------------------------------------
// UART parser — uncomment entire function when Arduino wired
// -------------------------------------------------------
// void readUART() {
//   if (Serial2.available()) {
//     String line = Serial2.readStringUntil('\n');
//     line.trim();
//     if (line.length() == 0) return;
//     int i0 = line.indexOf(',');
//     int i1 = line.indexOf(',', i0 + 1);
//     int i2 = line.indexOf(',', i1 + 1);
//     int i3 = line.indexOf(',', i2 + 1);
//     int i4 = line.indexOf(',', i3 + 1);
//     int i5 = line.indexOf(',', i4 + 1);
//     int i6 = line.indexOf(',', i5 + 1);
//     if (i0<0||i1<0||i2<0||i3<0||i4<0||i5<0||i6<0) return;
//     temperature   = line.substring(0,      i0).toFloat();
//     humidity      = line.substring(i0+1,   i1).toFloat();
//     fireDetected  = line.substring(i1+1,   i2).toInt();
//     flameDetected = line.substring(i2+1,   i3).toInt();
//     gasLevel      = line.substring(i3+1,   i4).toInt();
//     motorRunning  = line.substring(i4+1,   i5).toInt();
//     pumpActive    = line.substring(i5+1,   i6).toInt();
//     robotStatus   = line.substring(i6+1);
//   }
// }

// -------------------------------------------------------
// Dashboard HTML — stored as char array (safer than const char*
// with raw string literals for large HTML blobs on ESP32)
// CAM_IP placeholder replaced at runtime with real IP
// -------------------------------------------------------
const char dashboardHTML[] =
"<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<title>FireBot Command Center</title>"
"<link href=\"https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Barlow:wght@400;600;700&display=swap\" rel=\"stylesheet\">"
"<style>"
"  :root {"
"    --bg:        #080c10;"
"    --surface:   #0d1117;"
"    --border:    #1c2535;"
"    --accent:    #e83a2f;"
"    --amber:     #f59e0b;"
"    --green:     #10b981;"
"    --blue:      #3b82f6;"
"    --muted:     #4b5563;"
"    --text:      #e2e8f0;"
"    --subtext:   #64748b;"
"    --mono:      'Share Tech Mono', monospace;"
"    --sans:      'Barlow', sans-serif;"
"  }"
"  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }"
"  body {"
"    font-family: var(--sans);"
"    background: var(--bg);"
"    color: var(--text);"
"    min-height: 100vh;"
"    padding: 0;"
"    overflow-x: hidden;"
"  }"
"  body::before {"
"    content: '';"
"    position: fixed;"
"    inset: 0;"
"    background: repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,0,0,0.07) 2px,rgba(0,0,0,0.07) 4px);"
"    pointer-events: none;"
"    z-index: 9999;"
"  }"
"  .topbar {"
"    display: flex;"
"    align-items: center;"
"    justify-content: space-between;"
"    padding: 14px 28px;"
"    background: var(--surface);"
"    border-bottom: 1px solid var(--border);"
"    position: sticky;"
"    top: 0;"
"    z-index: 100;"
"  }"
"  .topbar-left { display: flex; align-items: center; gap: 12px; }"
"  .logo-icon {"
"    width: 36px; height: 36px;"
"    background: var(--accent);"
"    border-radius: 8px;"
"    display: flex; align-items: center; justify-content: center;"
"    font-size: 18px;"
"    box-shadow: 0 0 16px rgba(232,58,47,0.5);"
"  }"
"  .logo-text { font-family: var(--mono); font-size: 15px; letter-spacing: 2px; color: var(--text); }"
"  .logo-sub { font-size: 10px; color: var(--subtext); letter-spacing: 1px; text-transform: uppercase; margin-top: 1px; }"
"  .topbar-right { display: flex; align-items: center; gap: 20px; }"
"  .status-pill { display: flex; align-items: center; gap: 6px; font-family: var(--mono); font-size: 11px; color: var(--subtext); letter-spacing: 1px; text-transform: uppercase; }"
"  .pulse-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--green); box-shadow: 0 0 0 0 rgba(16,185,129,0.6); animation: pulse 2s infinite; }"
"  .pulse-dot.red   { background: var(--accent); box-shadow: 0 0 0 0 rgba(232,58,47,0.6); }"
"  .pulse-dot.amber { background: var(--amber);  box-shadow: 0 0 0 0 rgba(245,158,11,0.6); }"
"  @keyframes pulse { 0% { box-shadow: 0 0 0 0 currentColor; } 70% { box-shadow: 0 0 0 6px transparent; } 100% { box-shadow: 0 0 0 0 transparent; } }"
"  .alert-banner { display: none; background: linear-gradient(90deg,#7f1d1d,#991b1b); border-bottom: 2px solid var(--accent); padding: 10px 28px; font-family: var(--mono); font-size: 13px; letter-spacing: 2px; color: #fca5a5; text-align: center; animation: flashBanner 1s infinite alternate; }"
"  .alert-banner.active { display: block; }"
"  @keyframes flashBanner { from { opacity: 1; } to { opacity: 0.6; } }"
"  .main { display: grid; grid-template-columns: 1fr 340px; grid-template-rows: auto auto; gap: 16px; padding: 20px 28px; max-width: 1280px; margin: 0 auto; }"
"  @media (max-width: 900px) { .main { grid-template-columns: 1fr; } }"
"  .camera-panel { grid-row: 1 / 3; background: var(--surface); border: 1px solid var(--border); border-radius: 12px; overflow: hidden; display: flex; flex-direction: column; }"
"  .panel-header { display: flex; align-items: center; justify-content: space-between; padding: 12px 16px; border-bottom: 1px solid var(--border); }"
"  .panel-title { font-family: var(--mono); font-size: 11px; letter-spacing: 2px; color: var(--subtext); text-transform: uppercase; display: flex; align-items: center; gap: 8px; }"
"  .rec-badge { background: var(--accent); color: white; font-size: 9px; font-family: var(--mono); letter-spacing: 1px; padding: 2px 6px; border-radius: 3px; animation: flashBanner 1s infinite alternate; }"
"  .camera-feed { flex: 1; position: relative; background: #000; min-height: 280px; }"
"  .camera-feed img { width: 100%; height: 100%; object-fit: cover; display: block; }"
"  .cam-overlay { position: absolute; inset: 0; pointer-events: none; }"
"  .cam-overlay::before, .cam-overlay::after { content: ''; position: absolute; width: 20px; height: 20px; border-color: rgba(232,58,47,0.6); border-style: solid; }"
"  .cam-overlay::before { top: 12px; left: 12px; border-width: 2px 0 0 2px; }"
"  .cam-overlay::after  { bottom: 12px; right: 12px; border-width: 0 2px 2px 0; }"
"  .cam-hud { position: absolute; bottom: 10px; left: 12px; font-family: var(--mono); font-size: 10px; color: rgba(232,58,47,0.7); letter-spacing: 1px; line-height: 1.8; }"
"  .cam-offline { position: absolute; inset: 0; display: flex; flex-direction: column; align-items: center; justify-content: center; background: #050810; color: var(--muted); font-family: var(--mono); font-size: 12px; letter-spacing: 2px; gap: 10px; }"
"  .cam-offline-icon { font-size: 32px; opacity: 0.4; }"
"  .sensor-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }"
"  .sensor-card { background: var(--surface); border: 1px solid var(--border); border-radius: 10px; padding: 14px; position: relative; overflow: hidden; transition: border-color 0.3s, background 0.3s; }"
"  .sensor-card::after { content: ''; position: absolute; top: 0; left: 0; right: 0; height: 2px; background: var(--border); transition: background 0.3s; }"
"  .sensor-card.alert  { border-color: rgba(232,58,47,0.4); background: #130a09; }"
"  .sensor-card.alert::after  { background: var(--accent); }"
"  .sensor-card.active { border-color: rgba(16,185,129,0.3); }"
"  .sensor-card.active::after { background: var(--green); }"
"  .sensor-card.warn   { border-color: rgba(245,158,11,0.3); }"
"  .sensor-card.warn::after   { background: var(--amber); }"
"  .sc-label { font-size: 10px; font-family: var(--mono); color: var(--subtext); letter-spacing: 1.5px; text-transform: uppercase; margin-bottom: 10px; }"
"  .sc-value { font-family: var(--mono); font-size: 28px; font-weight: 700; color: var(--text); line-height: 1; }"
"  .sc-unit  { font-size: 11px; color: var(--subtext); margin-top: 4px; font-family: var(--mono); }"
"  .sc-icon  { position: absolute; top: 12px; right: 14px; font-size: 18px; opacity: 0.25; }"
"  .status-panel { background: var(--surface); border: 1px solid var(--border); border-radius: 10px; padding: 16px; }"
"  .status-row { display: flex; align-items: center; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid var(--border); font-size: 13px; }"
"  .status-row:last-child { border-bottom: none; }"
"  .status-key { font-family: var(--mono); font-size: 10px; color: var(--subtext); letter-spacing: 1.5px; text-transform: uppercase; }"
"  .status-val { font-family: var(--mono); font-size: 12px; color: var(--text); display: flex; align-items: center; gap: 6px; }"
"  .badge { padding: 2px 8px; border-radius: 4px; font-size: 10px; font-family: var(--mono); letter-spacing: 1px; text-transform: uppercase; }"
"  .badge.green { background: rgba(16,185,129,0.15); color: var(--green); border: 1px solid rgba(16,185,129,0.3); }"
"  .badge.red   { background: rgba(232,58,47,0.15);  color: var(--accent); border: 1px solid rgba(232,58,47,0.3); animation: flashBanner 0.8s infinite alternate; }"
"  .badge.amber { background: rgba(245,158,11,0.15); color: var(--amber); border: 1px solid rgba(245,158,11,0.3); }"
"  .badge.muted { background: rgba(75,85,99,0.2);    color: var(--muted);  border: 1px solid rgba(75,85,99,0.3); }"
"  .footer { text-align: center; padding: 14px; font-family: var(--mono); font-size: 10px; color: var(--muted); letter-spacing: 1px; border-top: 1px solid var(--border); margin-top: 4px; }"
"</style>"
"</head>"
"<body>"
"<div class=\"alert-banner BANNER_CLASS\" id=\"alertBanner\">"
"  &#9888; &nbsp; FIRE DETECTED &mdash; ROBOT ENGAGED &mdash; EXTINGUISHING IN PROGRESS &nbsp; &#9888;"
"</div>"
"<div class=\"topbar\">"
"  <div class=\"topbar-left\">"
"    <div class=\"logo-icon\">&#128293;</div>"
"    <div>"
"      <div class=\"logo-text\">FIREBOT</div>"
"      <div class=\"logo-sub\">Command Center v2.0</div>"
"    </div>"
"  </div>"
"  <div class=\"topbar-right\">"
"    <div class=\"status-pill\"><div class=\"pulse-dot WIFI_DOT\"></div>WiFi</div>"
"    <div class=\"status-pill\"><div class=\"pulse-dot CAM_DOT\"></div>Camera</div>"
"    <div class=\"status-pill\"><div class=\"pulse-dot STATUS_DOT_COLOR\"></div>ROBOT_STATUS_UPPER</div>"
"  </div>"
"</div>"
"<div class=\"main\">"
"  <div class=\"camera-panel\">"
"    <div class=\"panel-header\">"
"      <div class=\"panel-title\">&#128247; Live Feed CAM_REC_BADGE</div>"
"      <div style=\"font-family:var(--mono);font-size:10px;color:var(--subtext);\">VGA &middot; MJPEG &middot; ~25fps</div>"
"    </div>"
"    <div class=\"camera-feed\">"
"      CAM_FEED_HTML"
"      <div class=\"cam-overlay\"></div>"
"      <div class=\"cam-hud\">RES: 640&#215;480<br>SRC: ESP32-CAM<br>STREAM: :81</div>"
"    </div>"
"  </div>"
"  <div class=\"sensor-grid\">"
"    <div class=\"sensor-card TEMP_CLASS\"><div class=\"sc-label\">Temperature</div><div class=\"sc-value\">TEMP_VAL</div><div class=\"sc-unit\">&#176;C</div><div class=\"sc-icon\">&#127777;</div></div>"
"    <div class=\"sensor-card HUM_CLASS\"><div class=\"sc-label\">Humidity</div><div class=\"sc-value\">HUM_VAL</div><div class=\"sc-unit\">% RH</div><div class=\"sc-icon\">&#128167;</div></div>"
"    <div class=\"sensor-card GAS_CLASS\"><div class=\"sc-label\">Gas Level</div><div class=\"sc-value\">GAS_VAL</div><div class=\"sc-unit\">ppm</div><div class=\"sc-icon\">&#9729;</div></div>"
"    <div class=\"sensor-card FIRE_CLASS\"><div class=\"sc-label\">Fire IR</div><div class=\"sc-value\">FIRE_VAL</div><div class=\"sc-unit\">IR detection</div><div class=\"sc-icon\">&#128262;</div></div>"
"    <div class=\"sensor-card FLAME_CLASS\"><div class=\"sc-label\">Flame</div><div class=\"sc-value\">FLAME_VAL</div><div class=\"sc-unit\">analog sensor</div><div class=\"sc-icon\">&#128293;</div></div>"
"    <div class=\"sensor-card MOTOR_CLASS\"><div class=\"sc-label\">Motors</div><div class=\"sc-value\">MOTOR_VAL</div><div class=\"sc-unit\">drive system</div><div class=\"sc-icon\">&#9881;</div></div>"
"  </div>"
"  <div class=\"status-panel\">"
"    <div class=\"panel-header\" style=\"padding:0 0 12px 0;border-bottom:1px solid var(--border);margin-bottom:4px;\">"
"      <div class=\"panel-title\">System Status</div>"
"    </div>"
"    <div class=\"status-row\"><div class=\"status-key\">Robot State</div><div class=\"status-val\"><span class=\"badge ROBOT_BADGE\">ROBOT_STATUS</span></div></div>"
"    <div class=\"status-row\"><div class=\"status-key\">Water Pump</div><div class=\"status-val\"><span class=\"badge PUMP_BADGE\">PUMP_VAL</span></div></div>"
"    <div class=\"status-row\"><div class=\"status-key\">ESP32-CAM</div><div class=\"status-val\"><span class=\"badge green\">Online</span></div></div>"
"    <div class=\"status-row\"><div class=\"status-key\">Camera Feed</div><div class=\"status-val\"><span class=\"badge CAM_BADGE\">CAM_STATUS</span></div></div>"
"    <div class=\"status-row\"><div class=\"status-key\">Data Source</div><div class=\"status-val\"><span class=\"badge amber\">DATA_SOURCE_BADGE</span></div></div>"
"    <div class=\"status-row\"><div class=\"status-key\">Telegram</div><div class=\"status-val\"><span class=\"badge green\">Active</span></div></div>"
"    <div class=\"status-row\" style=\"border-bottom:none;\"><div class=\"status-key\">Uptime</div><div class=\"status-val\" id=\"uptime\" style=\"font-family:var(--mono);font-size:11px;\">UPTIME_VAL</div></div>"
"  </div>"
"</div>"
"<div class=\"footer\">FIREBOT ESP32-CAM &middot; DATA_FOOTER &middot; Auto-refresh 3s</div>"
"<script>"
"  var base = UPTIME_BASE;"
"  function fmt(s) {"
"    var h = Math.floor(s/3600), m = Math.floor((s%3600)/60), sec = s%60;"
"    return [h,m,sec].map(function(v){ return String(v).padStart(2,'0'); }).join(':');"
"  }"
"  setInterval(function(){"
"    base++;"
"    var el = document.getElementById('uptime');"
"    if(el) el.textContent = fmt(base);"
"  }, 1000);"
"</script>"
"<meta http-equiv=\"refresh\" content=\"3\">"
"</body>"
"</html>";

// -------------------------------------------------------
// Build dashboard — replaces all placeholders with live values
// -------------------------------------------------------
String buildDashboard() {
  String page = String(dashboardHTML);

  // Uptime in seconds
  unsigned long secs = millis() / 1000;
  page.replace("UPTIME_VAL",  String(secs / 3600) + "h " +
                               String((secs % 3600) / 60) + "m " +
                               String(secs % 60) + "s");
  page.replace("UPTIME_BASE", String(secs));

  // Sensor values
  page.replace("TEMP_VAL", String(temperature, 1));
  page.replace("HUM_VAL",  String(humidity, 1));
  page.replace("GAS_VAL",  String(gasLevel));

  // Fire IR
  page.replace("FIRE_VAL",   fireDetected ? "FIRE" : "Clear");
  page.replace("FIRE_CLASS", fireDetected ? "sensor-card alert" : "sensor-card active");

  // Flame
  page.replace("FLAME_VAL",   flameDetected ? "FLAME" : "Clear");
  page.replace("FLAME_CLASS", flameDetected ? "sensor-card alert" : "sensor-card active");

  // Gas — warn above 150, alert above 200
  page.replace("GAS_CLASS", gasLevel > 200 ? "sensor-card alert"
                           : gasLevel > 150 ? "sensor-card warn"
                           : "sensor-card");

  // Temperature warning above 45°C
  page.replace("TEMP_CLASS", temperature > 45 ? "sensor-card warn" : "sensor-card");
  page.replace("HUM_CLASS",  "sensor-card");

  // Motors
  page.replace("MOTOR_VAL",   motorRunning ? "Active" : "Idle");
  page.replace("MOTOR_CLASS", motorRunning ? "sensor-card active" : "sensor-card");

  // Pump
  page.replace("PUMP_VAL",   pumpActive ? "Spraying" : "Off");
  page.replace("PUMP_BADGE", pumpActive ? "red" : "muted");

  // Robot status badge
  String robotBadge = "green";
  if      (robotStatus == "Fighting Fire") robotBadge = "red";
  else if (robotStatus == "Standby")       robotBadge = "green";
  page.replace("ROBOT_BADGE",        robotBadge);
  page.replace("ROBOT_STATUS",       robotStatus);
  page.replace("ROBOT_STATUS_UPPER", robotStatus);

  // Top-bar status dot colour
  String statusDot = "amber";
  if (robotStatus == "Fighting Fire") statusDot = "red";
  if (robotStatus == "Standby")       statusDot = "green";
  page.replace("STATUS_DOT_COLOR", statusDot);
  page.replace("WIFI_DOT", "green");

  // Alert banner — only fire/flame triggers it (gas alone does not)
  bool danger = fireDetected || flameDetected;
  page.replace("BANNER_CLASS", danger ? "active" : "");

  // Camera — FIX: CAM_BADGE placeholder now correctly replaced with
  //               just the class name value, matching the HTML template
  if (cameraOK) {
    String ip = WiFi.localIP().toString();
    String imgTag = "<img src=\"http://" + ip + ":81/stream\" "
                    "alt=\"Camera stream\" "
                    "onerror=\"this.style.display='none';"
                    "document.getElementById('camOffline').style.display='flex';\">";
    String offlineDiv = "<div class='cam-offline' id='camOffline' style='display:none;'>"
                        "<div class='cam-offline-icon'>&#128247;</div>"
                        "STREAM UNAVAILABLE</div>";
    page.replace("CAM_FEED_HTML",  imgTag + offlineDiv);
    page.replace("CAM_DOT",        "green");
    page.replace("CAM_BADGE",      "green");   // FIX: was "badge green" — class attr already has "badge"
    page.replace("CAM_STATUS",     "Streaming");
    page.replace("CAM_REC_BADGE",  "<span class='rec-badge'>&#9679; REC</span>");
  } else {
    String offlineDiv = "<div class='cam-offline'>"
                        "<div class='cam-offline-icon'>&#128247;</div>"
                        "CAMERA OFFLINE</div>";
    page.replace("CAM_FEED_HTML",  offlineDiv);
    page.replace("CAM_DOT",        "red");
    page.replace("CAM_BADGE",      "red");     // FIX: was "badge red"
    page.replace("CAM_STATUS",     "Offline");
    page.replace("CAM_REC_BADGE",  "");
  }

  // Footer data source
  // -------------------------------------------------------
  // WHEN SWITCHING TO UART: change "Simulated" → "UART Live"
  // and update DATA_FOOTER similarly
  // -------------------------------------------------------
  page.replace("DATA_SOURCE_BADGE", "Simulated");
  page.replace("DATA_FOOTER",       "Simulated test values — UART pending");

  return page;
}

void handleRoot()     { server.send(200, "text/html", buildDashboard()); }
void handleNotFound() { server.send(404, "text/plain", "404 Not Found"); }


void setup() {
  Serial.begin(115200);
  delay(500);

  // Init camera before WiFi
  initCamera();

  // -------------------------------------------------------
  // WHEN SWITCHING TO UART: uncomment these two lines
  // -------------------------------------------------------
  // Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
  // Serial.println("[UART] Serial2 started RX=16 TX=17");
  // -------------------------------------------------------

  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n[WiFi] Connected!");
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());

  // Dashboard server
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Dashboard on port 80");

  // Stream server
  streamServer.on("/stream", handleStream);
  streamServer.begin();
  Serial.println("[HTTP] Stream on port 81");

  Serial.println("[URL]  http://" + WiFi.localIP().toString() + "/");
  Serial.println("[URL]  http://" + WiFi.localIP().toString() + ":81/stream");

  sendTelegram("%F0%9F%9F%A2 FireBot online. Dashboard: http://" +
               WiFi.localIP().toString());
}


void loop() {
  server.handleClient();
  streamServer.handleClient();

  // -------------------------------------------------------
  // WHEN SWITCHING TO UART: uncomment the line below
  // -------------------------------------------------------
  // readUART();
  // -------------------------------------------------------

  // -------------------------------------------------------
  // SIMULATION BLOCK
  // WHEN SWITCHING TO UART: DELETE from here...
  // -------------------------------------------------------
  static unsigned long lastSim = 0;
  if (millis() - lastSim > 5000) {
    temperature   = 30.0 + random(0, 100) / 10.0;
    humidity      = 55.0 + random(0, 80)  / 10.0;
    gasLevel      = 100  + random(0, 200);
    fireDetected  = (gasLevel > 250);
    flameDetected = (gasLevel > 280);
    motorRunning  = fireDetected;
    pumpActive    = fireDetected;
    robotStatus   = fireDetected ? "Fighting Fire" : "Standby";
    lastSim       = millis();
  }
  // -------------------------------------------------------
  // ...to here (delete entire SIMULATION BLOCK above)
  // -------------------------------------------------------

  checkAndAlert();
}
