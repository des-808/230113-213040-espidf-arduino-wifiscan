#include "web_server.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Arduino.h>
#include <pgmspace.h>
#include "wifi_manager.h"
#include <esp_wifi.h>
#include <time.h>
#include <sys/time.h>

// Глобальные переменные
static WebServer* g_server = nullptr;
static WifiManager* g_wm = nullptr;
static bool g_started = false;
static GetTimeStringFunc g_getTimeString = nullptr;

// ============================================================
// ТЕСТОВАЯ СТРАНИЦА С ЧАСАМИ (показывается при подключении к WiFi)
// ============================================================
static const char HTML_TEST[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Thermo - Live Clock</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',Tahoma,sans-serif;background:linear-gradient(135deg,#0f2027,#203a43,#2c5364);min-height:100vh;display:flex;justify-content:center;align-items:center;flex-direction:column;color:#fff}
.container{text-align:center;padding:40px;background:rgba(255,255,255,0.08);border-radius:24px;backdrop-filter:blur(10px);box-shadow:0 8px 32px rgba(0,0,0,0.3)}
h1{font-size:28px;margin-bottom:20px;opacity:0.7;font-weight:300}
#clock{font-size:96px;font-weight:700;letter-spacing:4px;text-shadow:0 0 20px rgba(100,200,255,0.5);font-variant-numeric:tabular-nums}
#date{font-size:24px;margin-top:10px;opacity:0.8}
#info{margin-top:30px;padding-top:20px;border-top:1px solid rgba(255,255,255,0.2);font-size:14px;opacity:0.6}
.status-dot{display:inline-block;width:12px;height:12px;border-radius:50%;background:#00ff88;margin-right:8px;animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.4}}
</style>
</head>
<body>
<div class="container">
<h1><span class="status-dot"></span>ESP32 Thermo - Connected</h1>
<div id="clock">--:--:--</div>
<div id="date">Loading...</div>
<div id="info">
  <div>WiFi: <span id="ssid">-</span></div>
  <div>IP: <span id="ip">-</span></div>
  <div>Uptime: <span id="uptime">-</span></div>
</div>
</div>
<script>
function updateClock(){
  fetch('/api/time').then(r=>r.json()).then(d=>{
    document.getElementById('clock').textContent=d.time;
    document.getElementById('date').textContent=d.date;
    document.getElementById('ssid').textContent=d.ssid;
    document.getElementById('ip').textContent=d.ip;
    document.getElementById('uptime').textContent=d.uptime;
  }).catch(()=>{});
}
updateClock();setInterval(updateClock,1000);
</script>
</body>
</html>
)HTML";

// ============================================================
// ОСНОВНАЯ HTML СТРАНИЦА (конфигурация WiFi) в PROGMEM
// ============================================================
static const char HTML_INDEX[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Thermo - WiFi Setup</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',Tahoma,sans-serif;background:linear-gradient(135deg,#667eea,#764ba2);min-height:100vh;display:flex;justify-content:center;align-items:flex-start;padding:20px}
.container{background:#fff;border-radius:16px;box-shadow:0 20px 60px rgba(0,0,0,0.3);padding:30px;max-width:600px;width:100%;margin-top:20px}
h1{color:#333;text-align:center;margin-bottom:10px;font-size:24px}
.status{padding:10px;border-radius:8px;margin-bottom:20px;font-weight:bold}
.status-softap{background:#fff3cd;color:#856404}
.status-station{background:#d4edda;color:#155724}
.section{margin-bottom:25px;padding:20px;border:1px solid #e0e0e0;border-radius:10px;background:#fafafa}
.section h2{color:#555;margin-bottom:15px;font-size:18px;border-bottom:2px solid #667eea;padding-bottom:8px}
label{display:block;margin-bottom:5px;color:#666;font-weight:500}
input[type="text"],input[type="password"]{width:100%;padding:12px;margin-bottom:12px;border:2px solid #ddd;border-radius:8px;font-size:14px}
input:focus{outline:none;border-color:#667eea}
button{padding:12px 24px;border:none;border-radius:8px;font-size:14px;font-weight:bold;cursor:pointer;margin:5px}
.btn-primary{background:linear-gradient(135deg,#667eea,#764ba2);color:#fff}
.btn-danger{background:linear-gradient(135deg,#ff6b6b,#ee5a24);color:#fff}
.btn-scan{background:linear-gradient(135deg,#1dd1a1,#10ac84);color:#fff}
.network-list{list-style:none;margin-top:10px}
.network-item{display:flex;justify-content:space-between;padding:10px 15px;margin:5px 0;background:#fff;border:1px solid #e0e0e0;border-radius:8px;cursor:pointer}
.network-item:hover{border-color:#667eea;background:#f0f0ff}
.info-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.info-item{padding:10px;background:#fff;border-radius:8px;border:1px solid #e0e0e0}
.info-item .label{font-size:12px;color:#888}
.info-item .value{font-size:16px;font-weight:bold;color:#333;margin-top:4px}
.message{padding:10px;border-radius:8px;margin:10px 0;text-align:center;display:none}
.success{background:#d4edda;color:#155724;display:block}
.error{background:#f8d7da;color:#721c24;display:block}
.center{text-align:center}
</style>
</head>
<body>
<div class="container">
<h1>ESP32 Thermo WiFi Setup</h1>
<div id="statusBar" class="status"></div>
<div class="section">
<h2>Device Info</h2>
<div class="info-grid">
<div class="info-item"><div class="label">Mode</div><div class="value" id="modeInfo">-</div></div>
<div class="info-item"><div class="label">IP Address</div><div class="value" id="ipInfo">-</div></div>
<div class="info-item"><div class="label">MAC</div><div class="value" id="macInfo">-</div></div>
<div class="info-item"><div class="label">Saved</div><div class="value" id="savedCount">0</div></div>
</div>
</div>
<div class="section">
<h2>Add WiFi Network</h2>
<label>SSID:</label>
<input type="text" id="ssidInput" placeholder="WiFi name">
<label>Password:</label>
<input type="password" id="passInput" placeholder="Password (optional)">
<div class="center">
<button class="btn-primary" onclick="saveNetwork()">Save</button>
</div>
</div>
<div class="section">
<h2>Scan Networks</h2>
<button class="btn-scan" onclick="scanNetworks()">Scan</button>
<ul id="scanResults" class="network-list"></ul>
</div>
<div class="section">
<h2>Saved Networks</h2>
<button class="btn-danger" onclick="clearNetworks()">Clear All</button>
</div>
<div class="section">
<h2>Switch Mode</h2>
<button class="btn-primary" onclick="switchMode('station')">Station (WiFi Client)</button>
<button class="btn-scan" onclick="switchMode('softap')">SoftAP (Access Point)</button>
</div>
<div id="message" class="message"></div>
</div>
<script>
const API='/api';
function msg(t,c){const m=document.getElementById('message');m.textContent=t;m.className='message '+c;setTimeout(()=>{m.className='message'},5000)}
function updateInfo(){
fetch(API+'/info').then(r=>r.json()).then(d=>{
document.getElementById('modeInfo').textContent=d.mode==='softap'?'SoftAP':'STA';
document.getElementById('ipInfo').textContent=d.ip;
document.getElementById('macInfo').textContent=d.mac;
document.getElementById('savedCount').textContent=d.saved_count;
const bar=document.getElementById('statusBar');
if(d.mode==='softap'){bar.className='status status-softap';bar.textContent='AP Mode: '+d.ap_ssid}else{bar.className='status status-station';bar.textContent='Connected to: '+d.sta_ssid}
}).catch(e=>console.error(e));
}
function saveNetwork(){
const ssid=document.getElementById('ssidInput').value.trim();
const pass=document.getElementById('passInput').value;
if(!ssid){msg('Enter SSID!','error');return}
fetch(API+'/save?ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass))
.then(r=>r.json()).then(d=>{
msg(d.success?'Network saved!':'Error: '+d.error,d.success?'success':'error');
if(d.success){document.getElementById('ssidInput').value='';document.getElementById('passInput').value='';updateInfo()}
});
}
function scanNetworks(){
const r=document.getElementById('scanResults');r.innerHTML='<li class="network-item">Scanning...</li>';
fetch(API+'/scan').then(r=>r.json()).then(d=>{
r.innerHTML='';if(!d.networks.length){r.innerHTML='<li class="network-item">No networks</li>';return}
d.networks.forEach(n=>{const li=document.createElement('li');li.className='network-item';
li.innerHTML='<span>'+(n.lock?'\ud83d\udd12 ':'')+n.ssid+'</span><span>'+n.rssi+'dBm</span>';
li.onclick=()=>{document.getElementById('ssidInput').value=n.ssid;n.lock?document.getElementById('passInput').focus():saveNetwork()};
r.appendChild(li)});
});
}
function clearNetworks(){
  if(!confirm('Delete all saved networks?'))return;
  fetch(API+'/clear').then(r=>r.json()).then(d=>{msg(d.success?'All deleted!':'Error',d.success?'success':'error');updateInfo()});
}
function switchMode(mode){
  fetch(API+'/switch?mode='+mode).then(r=>r.json()).then(d=>{
    if(d.success){msg('Switched to '+d.mode+'!','success');updateInfo();
      // Обновляем URL если режим изменился
      if(mode==='softap' && window.location.hostname !== d.ip.split('.')[0]){
        setTimeout(()=>window.location.href='http://'+d.ip,2000);
      }
    }else{msg('Error: '+d.error,'error')}
  });
  }
updateInfo();setInterval(updateInfo,5000);
</script>
</body>
</html>
)HTML";

// ============================================================
// ОБРАБОТЧИКИ
// ============================================================

// Получить страницу в виде String из PROGMEM
static String readFromPROGMEM(const char* ptr) {
  String html;
  size_t len = strlen_P(ptr);
  html.reserve(len);
  for (size_t i = 0; i < len; i++) {
    html += (char)pgm_read_byte_near(ptr + i);
  }
  return html;
}

void handleRoot() {
  if (!g_server || !g_wm) return;
  
  String html;
  
  // Если подключён к WiFi (Station mode) → показываем страницу с часами
  if (g_wm->getMode() == WIFI_MODE_STATION && WiFi.status() == WL_CONNECTED) {
    html = readFromPROGMEM(HTML_TEST);
    Serial.println("[WebServer] Serving test page (Station mode)");
  } else {
    // Иначе → показываем страницу конфигурации WiFi
    html = readFromPROGMEM(HTML_INDEX);
    Serial.println("[WebServer] Serving WiFi config page (SoftAP mode)");
  }
  
  g_server->send(200, "text/html", html);
}

void handleInfo() {
  if (!g_server || !g_wm) return;
  String json = "{";
  json += "\"mode\":\"" + String(g_wm->getMode() == WIFI_MODE_SOFTAP ? "softap" : "sta") + "\",";
  json += "\"ip\":\"" + g_wm->getIP() + "\",";
  if (g_wm->getMode() == WIFI_MODE_SOFTAP) {
    json += "\"ap_ssid\":\"" + String(g_wm->getSoftAPSSID()) + "\",";
    json += "\"sta_ssid\":\"disconnected\",";
  } else {
    json += "\"ap_ssid\":\"N/A\",";
    json += "\"sta_ssid\":\"" + String(WiFi.SSID()) + "\",";
  }
  json += "\"mac\":\"" + String(WiFi.macAddress()) + "\",";
  json += "\"saved_count\":" + String(g_wm->getSavedNetworkCount());
  json += "}";
  g_server->send(200, "application/json", json);
}

void handleTime() {
  if (!g_server || !g_wm) return;
  
  String timeStr, dateStr;
  
  // Если есть callback NTP — используем его
  if (g_getTimeString) {
    timeStr = String(g_getTimeString());
    // Извлекаем время из строки "HH:MM:SS"
    if (timeStr.length() >= 8) {
      timeStr = timeStr.substring(0, 8);
    }
    // Получаем дату из NTP (если формат "YYYY-MM-DD HH:MM:SS" или "DD.MM.YYYY")
    dateStr = String(g_getTimeString());
    if (dateStr.length() > 8) {
      dateStr = dateStr.substring(9);
      // Убираем время, оставляем только дату
      int spacePos = dateStr.indexOf(' ');
      if (spacePos > 0) {
        dateStr = dateStr.substring(0, spacePos);
      }
    }
  } else {
    // Fallback: системное время
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    char timeBuf[9];
    char dateBuf[20];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", timeinfo);
    strftime(dateBuf, sizeof(dateBuf), "%d.%m.%Y", timeinfo);
    timeStr = String(timeBuf);
    dateStr = String(dateBuf);
  }
  
  // Форматируем uptime
  unsigned long secs = millis() / 1000;
  unsigned long days = secs / 86400;
  unsigned long hours = (secs % 86400) / 3600;
  unsigned long mins = (secs % 3600) / 60;
  unsigned long sec = secs % 60;
  
  char uptimeBuf[30];
  if (days > 0) {
    snprintf(uptimeBuf, sizeof(uptimeBuf), "%lu d %02lu:%02lu:%02lu", days, hours, mins, sec);
  } else {
    snprintf(uptimeBuf, sizeof(uptimeBuf), "%02lu:%02lu:%02lu", hours, mins, sec);
  }
  
  String json = "{";
  json += "\"time\":\"" + timeStr + "\",";
  json += "\"date\":\"" + dateStr + "\",";
  json += "\"ssid\":\"" + WiFi.SSID() + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"uptime\":\"" + String(uptimeBuf) + "\"";
  json += "}";
  
  g_server->send(200, "application/json", json);
}

void handleSave() {
  if (!g_server || !g_wm) return;
  String ssid = g_server->arg("ssid");
  String pass = g_server->arg("pass");
  String switchArg = g_server->arg("switch");
  if (ssid.length() == 0) {
    g_server->send(200, "application/json", "{\"success\":false,\"error\":\"Empty SSID\"}");
    return;
  }
  // Автопереключение к сохранённой сети
  bool result = g_wm->saveNetwork(ssid.c_str(), pass.length() > 0 ? pass.c_str() : nullptr, true);
  String json = "{\"success\":" + String(result ? "true" : "false");
  if (result) {
    json += ",\"mode\":\"" + String(g_wm->getMode() == WIFI_MODE_STATION ? "station" : "softap") + "\"";
  } else {
    json += ",\"error\":\"Connection failed\"";
  }
  json += "}";
  g_server->send(200, "application/json", json);
}

void handleScan() {
  if (!g_server || !g_wm) return;
  
  // Сохраняем текущий режим WiFi
  wifi_mode_t originalMode = WiFi.getMode();
  
  // Переключаемся в AP_STA для сканирования
  WiFi.mode(WIFI_AP_STA);
  delay(50);
  
  int n = WiFi.scanNetworks();
  String json = "{\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    bool lock = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"lock\":" + String(lock ? "true" : "false");
    json += "}";
  }
  json += "]}";
  WiFi.scanDelete();
  
  // Восстанавливаем исходный режим
  WiFi.mode(originalMode);
  delay(50);
  
  g_server->send(200, "application/json", json);
}

void handleClear() {
  if (!g_server || !g_wm) return;
  g_wm->clearSavedNetworks();
  g_server->send(200, "application/json", "{\"success\":true}");
}

void handleSwitch() {
  if (!g_server || !g_wm) return;
  String mode = g_server->arg("mode");
  bool result = false;
  
  if (mode == "station") {
    result = g_wm->switchToStation();
  } else if (mode == "softap") {
    result = g_wm->switchToSoftAP();
  } else {
    g_server->send(200, "application/json", "{\"success\":false,\"error\":\"Unknown mode. Use 'station' or 'softap'\"}");
    return;
  }
  
  String json = "{\"success\":" + String(result ? "true" : "false");
  if (result) {
    json += ",\"mode\":\"" + String(g_wm->getMode() == WIFI_MODE_STATION ? "station" : "softap") + "\"";
    json += ",\"ip\":\"" + g_wm->getIP() + "\"";
  } else {
    json += ",\"error\":\"Switch failed\"";
  }
  json += "}";
  g_server->send(200, "application/json", json);
}

// ============================================================
// PUBLIC API
// ============================================================

bool webServerStart(WifiManager* wm) {
  if (g_started) return true;
  
  g_wm = wm;
  g_server = new WebServer(80);
  
  g_server->on("/", HTTP_GET, handleRoot);
  g_server->on("/api/info", HTTP_GET, handleInfo);
  g_server->on("/api/time", HTTP_GET, handleTime);
  g_server->on("/api/save", HTTP_GET, handleSave);
  g_server->on("/api/switch", HTTP_GET, handleSwitch);
  g_server->on("/api/scan", HTTP_GET, handleScan);
  g_server->on("/api/clear", HTTP_GET, handleClear);
  
  g_server->begin();
  g_started = true;
  
  Serial.println("[WebServer] HTTP server started on port 80");
  Serial.print("[WebServer] Server IP: ");
  Serial.println(WiFi.softAPIP().toString().c_str());
  
  return true;
}

void webServerSetTimeCallback(GetTimeStringFunc getTimeString) {
  g_getTimeString = getTimeString;
  if (g_getTimeString) {
    Serial.println("[WebServer] Time callback registered (NTP)");
  }
}

void webServerStop() {
  if (!g_server) return;
  delete g_server;
  g_server = nullptr;
  g_started = false;
  Serial.println("[WebServer] HTTP server stopped");
}

void webServerHandleClient() {
  if (g_server) g_server->handleClient();
}

bool webServerIsStarted() {
  return g_started;
}
