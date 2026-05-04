#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "esp_camera.h"

const char *ssid = "Tasrif";
const char *password = "12345678";

// Replace with your own
const char *BOT_TOKEN = "";
const char *CHAT_ID = "";

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#define UART_RX 14
#define UART_TX 15
#define UART_BAUD 9600

#define ALERT_COOLDOWN_MS 30000
#define LOG_SIZE 8

// DHT fallback values if sensor fails
#define DHT_FALLBACK_TEMP 28.0
#define DHT_FALLBACK_HUM 65.0

HardwareSerial mySerial(2);

float temperature = DHT_FALLBACK_TEMP;
float humidity = DHT_FALLBACK_HUM;
bool fireDetected = false;
bool flameDetected = false;
int gasLevel = 0;
bool motorRunning = false;
bool pumpActive = false;
String robotStatus = "Standby";

unsigned long lastUARTTime = 0;
bool uartHealthy = false;

bool alertFireSent = false;
bool alertClearSent = true;
unsigned long lastAlertTime = 0;

String eventLog[LOG_SIZE];
int logIndex = 0;

WebServer server(80);
WebServer streamServer(81);
bool cameraOK = false;

// Event log
void addLog(String msg)
{
  unsigned long s = millis() / 1000;
  String ts = String(s / 3600) + "h" + String((s % 3600) / 60) + "m";
  eventLog[logIndex % LOG_SIZE] = "[" + ts + "] " + msg;
  logIndex++;
}

// Camera init
void initCamera()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err == ESP_OK)
  {
    cameraOK = true;
    Serial.println("[Camera] OK");
  }
  else
  {
    cameraOK = false;
    Serial.println("[Camera] FAILED");
  }
}

// Stream handler
void handleStream()
{
  if (!cameraOK)
  {
    streamServer.send(503, "text/plain", "No camera");
    return;
  }

  WiFiClient client = streamServer.client();
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: multipart/x-mixed-replace; boundary=frame\r\n");
  client.print("Access-Control-Allow-Origin: *\r\n\r\n");

  while (client.connected())
  {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
      delay(10);
      continue;
    }
    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    esp_camera_fb_return(fb);
    delay(80);
  }
}

// Send Telegram message
void sendTelegram(String msg)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  msg.replace(" ", "%20");
  msg.replace("\n", "%0A");
  msg.replace("!", "%21");

  HTTPClient http;
  String url = String("https://api.telegram.org/bot") + BOT_TOKEN +
               "/sendMessage?chat_id=" + CHAT_ID + "&text=" + msg;

  http.begin(url);
  int code = http.GET();

  if (code > 0)
  {
    Serial.println("[TG] OK " + String(code));
  }
  else
  {
    Serial.println("[TG] FAIL");
  }

  http.end();
}

// Send Telegram photo
void sendTelegramPhoto()
{
  if (!cameraOK || WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    return;
  }

  HTTPClient http;
  String url = String("https://api.telegram.org/bot") + BOT_TOKEN +
               "/sendPhoto?chat_id=" + CHAT_ID;

  http.begin(url);
  http.addHeader("Content-Type", "multipart/form-data; boundary=FB");

  String head = "--FB\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"s.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--FB--\r\n";

  int totalLen = head.length() + fb->len + tail.length();
  uint8_t *buf = (uint8_t *)malloc(totalLen);

  if (buf)
  {
    memcpy(buf, head.c_str(), head.length());
    memcpy(buf + head.length(), fb->buf, fb->len);
    memcpy(buf + head.length() + fb->len, tail.c_str(), tail.length());
    http.POST(buf, totalLen);
    free(buf);
  }

  esp_camera_fb_return(fb);
  http.end();
}

// Fire alert logic
void checkAndAlert()
{
  bool danger = false;
  if (fireDetected || flameDetected)
  {
    danger = true;
  }

  unsigned long now = millis();

  if (danger)
  {
    if (!alertFireSent || now - lastAlertTime > ALERT_COOLDOWN_MS)
    {
      String msg = "%F0%9F%94%A5 FIRE ALERT%0A";
      msg += "Temp: " + String(temperature, 1) + "C%0A";
      msg += "Gas: " + String(gasLevel) + "ppm%0A";
      msg += "Status: " + robotStatus;

      sendTelegram(msg);
      sendTelegramPhoto();

      alertFireSent = true;
      alertClearSent = false;
      lastAlertTime = now;
      addLog("Fire alert sent");
    }
  }
  else if (!alertClearSent)
  {
    sendTelegram("%E2%9C%85 All Clear - Robot returning to Standby.");
    alertClearSent = true;
    alertFireSent = false;
    addLog("All-clear sent");
  }
}

// Read sensor data from UART
void readUART()
{
  if (!mySerial.available())
  {
    return;
  }

  String line = mySerial.readStringUntil('\n');
  line.trim();

  if (line.length() == 0)
  {
    return;
  }

  int i0 = line.indexOf(',');
  int i1 = line.indexOf(',', i0 + 1);
  int i2 = line.indexOf(',', i1 + 1);
  int i3 = line.indexOf(',', i2 + 1);
  int i4 = line.indexOf(',', i3 + 1);
  int i5 = line.indexOf(',', i4 + 1);
  int i6 = line.indexOf(',', i5 + 1);

  if (i0 < 0 || i1 < 0 || i2 < 0 || i3 < 0 || i4 < 0 || i5 < 0 || i6 < 0)
  {
    return;
  }

  float t = line.substring(0, i0).toFloat();
  float h = line.substring(i0 + 1, i1).toFloat();
  int g = line.substring(i3 + 1, i4).toInt();

  // basic sanity check on other fields
  if (g < 0 || g > 10000)
  {
    return;
  }

  // if DHT is working use its values, otherwise keep fallback
  if (t > 0.0 && t < 120.0 && h > 0.0 && h <= 100.0)
  {
    temperature = t;
    humidity = h;
  }
  else
  {
    // DHT probably failed, keep whatever we had (starts as fallback)
    temperature = temperature;
    humidity = humidity;
  }

  if (line.substring(i1 + 1, i2).toInt() == 1)
  {
    fireDetected = true;
  }
  else
  {
    fireDetected = false;
  }

  if (line.substring(i2 + 1, i3).toInt() == 1)
  {
    flameDetected = true;
  }
  else
  {
    flameDetected = false;
  }

  gasLevel = g;

  if (line.substring(i4 + 1, i5).toInt() == 1)
  {
    motorRunning = true;
  }
  else
  {
    motorRunning = false;
  }

  if (line.substring(i5 + 1, i6).toInt() == 1)
  {
    pumpActive = true;
  }
  else
  {
    pumpActive = false;
  }

  robotStatus = line.substring(i6 + 1);
  lastUARTTime = millis();
  uartHealthy = true;
}

// Build dashboard HTML
String buildDashboard()
{
  unsigned long secs = millis() / 1000;

  bool danger = false;
  if (fireDetected || flameDetected)
  {
    danger = true;
  }

  uartHealthy = false;
  if (millis() - lastUARTTime < 5000 && lastUARTTime > 0)
  {
    uartHealthy = true;
  }

  String tCls = "sc";
  if (temperature > 45)
  {
    tCls = "sc wn";
  }

  String gCls = "sc";
  if (gasLevel > 200)
  {
    gCls = "sc al";
  }
  else if (gasLevel > 150)
  {
    gCls = "sc wn";
  }

  String fiCls = "sc ac";
  if (fireDetected)
  {
    fiCls = "sc al";
  }

  String flCls = "sc ac";
  if (flameDetected)
  {
    flCls = "sc al";
  }

  String mCls = "sc";
  if (motorRunning)
  {
    mCls = "sc ac";
  }

  String fiVal = "Clear";
  if (fireDetected)
  {
    fiVal = "FIRE";
  }

  String flVal = "Clear";
  if (flameDetected)
  {
    flVal = "FLAME";
  }

  String mVal = "Idle";
  if (motorRunning)
  {
    mVal = "Active";
  }

  String pVal = "Off";
  if (pumpActive)
  {
    pVal = "Spraying";
  }

  String pBdg = "m";
  if (pumpActive)
  {
    pBdg = "r";
  }

  String rBdg = "g";
  if (robotStatus == "Fighting Fire")
  {
    rBdg = "r";
  }

  String sDot = "";
  if (robotStatus == "Fighting Fire")
  {
    sDot = "r";
  }

  String wDot = "";
  if (WiFi.status() != WL_CONNECTED)
  {
    wDot = "r";
  }

  String uDot = "";
  if (!uartHealthy)
  {
    uDot = "r";
  }

  String uaBdg = "r";
  if (uartHealthy)
  {
    uaBdg = "g";
  }

  String uaStat = "No Data";
  if (uartHealthy)
  {
    uaStat = "Live";
  }

  String banner = "";
  if (danger)
  {
    banner = "on";
  }

  String upVal = String(secs / 3600) + "h " + String((secs % 3600) / 60) + "m " + String(secs % 60) + "s";

  String camFeed = "";
  String cDot = "";
  String cBdg = "";
  String cStat = "";
  String recBdg = "";

  if (cameraOK)
  {
    String ip = WiFi.localIP().toString();
    camFeed = "<img src='http://" + ip + ":81/stream' "
                                         "onerror=\"this.style.display='none';"
                                         "document.getElementById('co').style.display='flex';\">"
                                         "<div class='coff' id='co' style='display:none'>"
                                         "<div style='font-size:28px;opacity:.4'>&#128247;</div>UNAVAILABLE</div>";
    cDot = "";
    cBdg = "g";
    cStat = "Streaming";
    recBdg = "<span class='rec'>&#9679; REC</span>";
  }
  else
  {
    camFeed = "<div class='coff'>"
              "<div style='font-size:28px;opacity:.4'>&#128247;</div>OFFLINE</div>";
    cDot = "r";
    cBdg = "r";
    cStat = "Offline";
    recBdg = "";
  }

  String logs = "";
  for (int i = 0; i < LOG_SIZE; i++)
  {
    int idx = (logIndex - 1 - i + LOG_SIZE * 2) % LOG_SIZE;
    if (eventLog[idx].length() > 0)
    {
      logs += "<div class='le'>" + eventLog[idx] + "</div>";
    }
  }

  if (logs.length() == 0)
  {
    logs = "<div class='le'>No events yet.</div>";
  }

  String p = "";

  p += "<!DOCTYPE html><html lang='en'><head>";
  p += "<meta charset='UTF-8'>";
  p += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  p += "<title>FireBot</title>";
  p += "<link href='https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Barlow:wght@400;600&display=swap' rel='stylesheet'>";
  p += "<style>";
  p += ":root{--bg:#080c10;--sur:#0d1117;--brd:#1c2535;--red:#e83a2f;--amb:#f59e0b;--grn:#10b981;--blu:#3b82f6;--mut:#4b5563;--txt:#e2e8f0;--sub:#64748b;--mono:'Share Tech Mono',monospace;--sans:'Barlow',sans-serif}";
  p += "*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}";
  p += "body{font-family:var(--sans);background:var(--bg);color:var(--txt);min-height:100vh;overflow-x:hidden}";
  p += "body::before{content:'';position:fixed;inset:0;background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,0,0,.07) 2px,rgba(0,0,0,.07) 4px);pointer-events:none;z-index:9999}";
  p += ".topbar{display:flex;align-items:center;justify-content:space-between;padding:12px 20px;background:var(--sur);border-bottom:1px solid var(--brd);position:sticky;top:0;z-index:100}";
  p += ".tl{display:flex;align-items:center;gap:10px}";
  p += ".logo{width:32px;height:32px;background:var(--red);border-radius:8px;display:flex;align-items:center;justify-content:center;font-size:16px;box-shadow:0 0 14px rgba(232,58,47,.5)}";
  p += ".lt{font-family:var(--mono);font-size:14px;letter-spacing:2px}";
  p += ".ls{font-size:9px;color:var(--sub);letter-spacing:1px;text-transform:uppercase;margin-top:1px}";
  p += ".tr{display:flex;align-items:center;gap:16px}";
  p += ".pill{display:flex;align-items:center;gap:5px;font-family:var(--mono);font-size:10px;color:var(--sub);letter-spacing:1px;text-transform:uppercase}";
  p += ".dot{width:7px;height:7px;border-radius:50%;background:var(--grn);animation:pulse 2s infinite}";
  p += ".dot.r{background:var(--red)}.dot.a{background:var(--amb)}";
  p += "@keyframes pulse{0%{box-shadow:0 0 0 0 currentColor}70%{box-shadow:0 0 0 5px transparent}100%{box-shadow:0 0 0 0 transparent}}";
  p += ".ubtn{padding:3px 9px;border-radius:4px;font-size:9px;font-family:var(--mono);letter-spacing:1px;text-transform:uppercase;cursor:pointer;border:1px solid;transition:all .3s;background:none}";
  p += ".banner{display:none;background:linear-gradient(90deg,#7f1d1d,#991b1b);border-bottom:2px solid var(--red);padding:8px 20px;font-family:var(--mono);font-size:12px;letter-spacing:2px;color:#fca5a5;text-align:center;animation:fl 1s infinite alternate}";
  p += ".banner.on{display:block}";
  p += "@keyframes fl{from{opacity:1}to{opacity:.6}}";
  p += ".main{display:grid;grid-template-columns:1fr 320px;gap:14px;padding:16px 20px;max-width:1200px;margin:0 auto}";
  p += "@media(max-width:860px){.main{grid-template-columns:1fr}}";
  p += ".cam-panel{grid-row:1/3;background:var(--sur);border:1px solid var(--brd);border-radius:10px;overflow:hidden;display:flex;flex-direction:column}";
  p += ".ph{display:flex;align-items:center;justify-content:space-between;padding:10px 14px;border-bottom:1px solid var(--brd)}";
  p += ".pt{font-family:var(--mono);font-size:10px;letter-spacing:2px;color:var(--sub);text-transform:uppercase;display:flex;align-items:center;gap:7px}";
  p += ".rec{background:var(--red);color:#fff;font-size:8px;font-family:var(--mono);letter-spacing:1px;padding:2px 5px;border-radius:3px;animation:fl 1s infinite alternate}";
  p += ".cf{flex:1;position:relative;background:#000;min-height:240px}";
  p += ".cf img{width:100%;height:100%;object-fit:cover;display:block}";
  p += ".ov{position:absolute;inset:0;pointer-events:none}";
  p += ".ov::before,.ov::after{content:'';position:absolute;width:18px;height:18px;border-color:rgba(232,58,47,.6);border-style:solid}";
  p += ".ov::before{top:10px;left:10px;border-width:2px 0 0 2px}";
  p += ".ov::after{bottom:10px;right:10px;border-width:0 2px 2px 0}";
  p += ".hud{position:absolute;bottom:8px;left:10px;font-family:var(--mono);font-size:9px;color:rgba(232,58,47,.7);letter-spacing:1px;line-height:1.8}";
  p += ".coff{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;background:#050810;color:var(--mut);font-family:var(--mono);font-size:11px;letter-spacing:2px;gap:8px}";
  p += ".sg{display:grid;grid-template-columns:1fr 1fr;gap:8px}";
  p += ".sc{background:var(--sur);border:1px solid var(--brd);border-radius:8px;padding:12px;position:relative;overflow:hidden;transition:border-color .3s,background .3s}";
  p += ".sc::after{content:'';position:absolute;top:0;left:0;right:0;height:2px;background:var(--brd);transition:background .3s}";
  p += ".sc.al{border-color:rgba(232,58,47,.4);background:#130a09}.sc.al::after{background:var(--red)}";
  p += ".sc.ac::after{background:var(--grn)}";
  p += ".sc.wn{border-color:rgba(245,158,11,.3)}.sc.wn::after{background:var(--amb)}";
  p += ".sl{font-size:9px;font-family:var(--mono);color:var(--sub);letter-spacing:1.5px;text-transform:uppercase;margin-bottom:8px}";
  p += ".sv{font-family:var(--mono);font-size:26px;font-weight:700;line-height:1}";
  p += ".su{font-size:10px;color:var(--sub);margin-top:3px;font-family:var(--mono)}";
  p += ".si{position:absolute;top:10px;right:12px;font-size:16px;opacity:.22}";
  p += ".sp{background:var(--sur);border:1px solid var(--brd);border-radius:8px;padding:14px}";
  p += ".sr{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--brd);font-size:12px}";
  p += ".sr:last-child{border-bottom:none}";
  p += ".sk{font-family:var(--mono);font-size:9px;color:var(--sub);letter-spacing:1.5px;text-transform:uppercase}";
  p += ".sv2{font-family:var(--mono);font-size:11px;display:flex;align-items:center;gap:5px}";
  p += ".b{padding:2px 7px;border-radius:3px;font-size:9px;font-family:var(--mono);letter-spacing:1px;text-transform:uppercase}";
  p += ".b.g{background:rgba(16,185,129,.15);color:var(--grn);border:1px solid rgba(16,185,129,.3)}";
  p += ".b.r{background:rgba(232,58,47,.15);color:var(--red);border:1px solid rgba(232,58,47,.3);animation:fl .8s infinite alternate}";
  p += ".b.a{background:rgba(245,158,11,.15);color:var(--amb);border:1px solid rgba(245,158,11,.3)}";
  p += ".b.m{background:rgba(75,85,99,.2);color:var(--mut);border:1px solid rgba(75,85,99,.3)}";
  p += ".lp{background:var(--sur);border:1px solid var(--brd);border-radius:8px;padding:14px;margin:0 20px 14px;max-width:1200px;margin-left:auto;margin-right:auto}";
  p += ".le{font-family:var(--mono);font-size:9px;color:var(--sub);padding:3px 0;border-bottom:1px solid rgba(255,255,255,.03)}";
  p += ".footer{text-align:center;padding:12px;font-family:var(--mono);font-size:9px;color:var(--mut);letter-spacing:1px;border-top:1px solid var(--brd);margin-top:4px}";
  p += "</style></head><body>";

  // fire banner
  p += "<div class='banner " + banner + "'>&#9888; &nbsp; FIRE DETECTED &mdash; ROBOT ENGAGED &nbsp; &#9888;</div>";

  // top bar
  p += "<div class='topbar'>";
  p += "<div class='tl'><div class='logo'>&#128293;</div>";
  p += "<div><div class='lt'>FIREBOT</div><div class='ls'>Command Center v2.1</div></div></div>";
  p += "<div class='tr'>";
  p += "<div class='pill'><div class='dot " + wDot + "'></div>WiFi</div>";
  p += "<div class='pill'><div class='dot " + cDot + "'></div>Cam</div>";
  p += "<div class='pill'><div class='dot " + uDot + "'></div>UART</div>";
  p += "<div class='pill'><div class='dot " + sDot + "'></div>" + robotStatus + "</div>";
  p += "</div></div>";

  // main grid
  p += "<div class='main'>";

  // camera panel
  p += "<div class='cam-panel'>";
  p += "<div class='ph'><div class='pt'>&#128247; Live Feed " + recBdg + "</div>";
  p += "<div style='font-family:var(--mono);font-size:9px;color:var(--sub)'>QVGA &middot; MJPEG</div></div>";
  p += "<div class='cf'>" + camFeed + "<div class='ov'></div>";
  p += "<div class='hud'>320&#215;240<br>ESP32-CAM<br>:81</div></div></div>";

  // sensor cards
  p += "<div class='sg'>";
  p += "<div class='" + tCls + "'><div class='sl'>Temperature</div><div class='sv'>" + String(temperature, 1) + "</div><div class='su'>&#176;C</div><div class='si'>&#127777;</div></div>";
  p += "<div class='sc'><div class='sl'>Humidity</div><div class='sv'>" + String(humidity, 1) + "</div><div class='su'>% RH</div><div class='si'>&#128167;</div></div>";
  p += "<div class='" + gCls + "'><div class='sl'>Gas</div><div class='sv'>" + String(gasLevel) + "</div><div class='su'>ppm</div><div class='si'>&#9729;</div></div>";
  p += "<div class='" + fiCls + "'><div class='sl'>Fire IR</div><div class='sv'>" + fiVal + "</div><div class='su'>IR sensor</div><div class='si'>&#128262;</div></div>";
  p += "<div class='" + flCls + "'><div class='sl'>Flame</div><div class='sv'>" + flVal + "</div><div class='su'>analog</div><div class='si'>&#128293;</div></div>";
  p += "<div class='" + mCls + "'><div class='sl'>Motors</div><div class='sv'>" + mVal + "</div><div class='su'>drive</div><div class='si'>&#9881;</div></div>";
  p += "</div>";

  // system panel
  p += "<div class='sp'>";
  p += "<div class='ph' style='padding:0 0 10px 0;border-bottom:1px solid var(--brd);margin-bottom:4px'><div class='pt'>System</div></div>";
  p += "<div class='sr'><div class='sk'>Robot</div><div class='sv2'><span class='b " + rBdg + "'>" + robotStatus + "</span></div></div>";
  p += "<div class='sr'><div class='sk'>Pump</div><div class='sv2'><span class='b " + pBdg + "'>" + pVal + "</span></div></div>";
  p += "<div class='sr'><div class='sk'>Camera</div><div class='sv2'><span class='b " + cBdg + "'>" + cStat + "</span></div></div>";
  p += "<div class='sr'><div class='sk'>UART</div><div class='sv2'><span class='b " + uaBdg + "'>" + uaStat + "</span></div></div>";
  p += "<div class='sr'><div class='sk'>Telegram</div><div class='sv2'><span class='b g'>Active</span></div></div>";
  p += "<div class='sr'><div class='sk'>Uptime</div><div class='sv2' id='up' style='font-size:10px'>" + upVal + "</div></div>";
  p += "</div>";
  p += "</div>"; // end main

  // event log
  p += "<div class='lp'>";
  p += "<div class='ph' style='padding:0 0 8px 0;border-bottom:1px solid var(--brd);margin-bottom:6px'><div class='pt'>&#128221; Events</div></div>";
  p += logs;
  p += "</div>";

  p += "<div class='footer'>FIREBOT &middot; Auto-refresh 5s</div>";

  // uptime script
  p += "<script type='text/javascript'>";
  p += "var b=" + String(secs) + ";";
  p += "function fmt(s){";
  p += "var h=Math.floor(s/3600);";
  p += "var m=Math.floor((s%3600)/60);";
  p += "var x=s%60;";
  p += "var pad=function(v){var t=String(v);return t.length<2?'0'+t:t;};";
  p += "return pad(h)+':'+pad(m)+':'+pad(x);";
  p += "}";
  p += "setInterval(function(){b++;var e=document.getElementById('up');if(e)e.textContent=fmt(b);},1000);";
  p += "</script>";

  p += "<meta http-equiv='refresh' content='5'>";
  p += "</body></html>";

  return p;
}

// Web handlers
void handleRoot()
{
  String page = buildDashboard();
  server.send(200, "text/html", page);
}

void handleNotFound()
{
  server.send(404, "text/plain", "Not found");
}

// WiFi watchdog
void maintainWiFi()
{
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 10000)
  {
    return;
  }
  lastCheck = millis();

  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }
}

// Setup
void setup()
{
  Serial.begin(115200);
  delay(500);

  initCamera();

  mySerial.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
  Serial.println("[UART] RX=14 TX=15");

  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  streamServer.on("/stream", handleStream);
  streamServer.begin();

  addLog("Boot OK");
  sendTelegram("%F0%9F%9F%A2 FireBot online: http://" + WiFi.localIP().toString());
}

// Main loop
void loop()
{
  server.handleClient();
  streamServer.handleClient();
  maintainWiFi();
  readUART();
  checkAndAlert();
}
