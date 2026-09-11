#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "config.h"
#include "DY1703A.h"
#include "BatteryMonitor.h"

HardwareSerial DYSerial(DY_UART_NUM);
DY1703A player(DYSerial);
BatteryMonitor battery(BATTERY_ADC_PIN, BATTERY_R_TOP, BATTERY_R_BOTTOM, BATTERY_ENABLE_PIN);
AsyncWebServer server(WEB_SERVER_PORT);
DNSServer dnsServer;
Preferences preferences;

struct TrackButton { uint8_t pin; uint8_t trackNumber; bool lastState; unsigned long lastChangeMs; };
TrackButton buttons[8];

uint8_t currentVolume = 20;
uint8_t currentTrack = 0;
bool currentPlaying = false;
uint32_t batteryMv = 0;
uint8_t batteryPct = 0;
unsigned long lastActivityMs = 0;

bool wifiActive = false;
bool webServerStarted = false;
bool wifiIsSta = false;
unsigned long wifiConnectDeadlineMs = 0;
unsigned long wifiDisconnectGraceDeadlineMs = 0;
bool wifiRestartPending = false;
unsigned long wifiRestartAtMs = 0;

String deviceName;
String apSsid;
String apPassword;
String staSsid;
String staPassword;
String settingsPassword;
String trackNames[8];
bool vibrationEnabled[8];

String settingsSessionToken;
String restoreBuffer;
bool restoreUploadOk = false;

const char *DEFAULT_DEVICE = "DY1703A Player";
const char *DEFAULT_AP_SSID = WIFI_AP_SSID;
const char *DEFAULT_AP_PASSWORD = WIFI_AP_PASS;
const char *DEFAULT_SETTINGS_PASSWORD = "12345";

bool isInputOnlyPin(uint8_t pin) {
    for (uint8_t p : INPUT_ONLY_PINS) if (p == pin) return true;
    return false;
}

void setMotor(bool on) {
    digitalWrite(MOTOR_PIN, on ? MOTOR_ACTIVE_LEVEL :
                 (MOTOR_ACTIVE_LEVEL == HIGH ? LOW : HIGH));
}

void setupButtons() {
    for (uint8_t i = 0; i < 8; i++) {
        buttons[i].pin = TRACK_BUTTON_PINS[i];
        buttons[i].trackNumber = i + 1;
        buttons[i].lastState = HIGH;
        buttons[i].lastChangeMs = 0;
        pinMode(buttons[i].pin, isInputOnlyPin(buttons[i].pin) ? INPUT : INPUT_PULLUP);
    }
    pinMode(MOTOR_PIN, OUTPUT);
    setMotor(false);
}

void loadSettings() {
    preferences.begin("settings", false);
    deviceName = preferences.getString("device", DEFAULT_DEVICE);
    apSsid = preferences.getString("apssid", DEFAULT_AP_SSID);
    apPassword = preferences.getString("appass", DEFAULT_AP_PASSWORD);
    staSsid = preferences.getString("ssid", WIFI_STA_SSID);
    staPassword = preferences.getString("wpass", WIFI_STA_PASS);
    settingsPassword = preferences.getString("setpass", DEFAULT_SETTINGS_PASSWORD);
    currentVolume = (uint8_t)constrain(preferences.getUChar("volume", 20), 0, 30);

    if (deviceName.length() == 0) deviceName = DEFAULT_DEVICE;
    if (apSsid.length() == 0) apSsid = DEFAULT_AP_SSID;
    if (apPassword.length() < 8 || apPassword.length() > 63) apPassword = DEFAULT_AP_PASSWORD;
    if (settingsPassword.length() == 0) settingsPassword = DEFAULT_SETTINGS_PASSWORD;

    for (uint8_t i = 0; i < 8; i++) {
        char key[8];
        snprintf(key, sizeof(key), "trk%u", i + 1);
        String defaultName = "Skladba " + String(i + 1);
        trackNames[i] = preferences.getString(key, defaultName.c_str());
        if (trackNames[i].length() == 0) trackNames[i] = defaultName;
        if (trackNames[i].length() > 40) trackNames[i] = trackNames[i].substring(0, 40);

        char vkey[8];
        snprintf(vkey, sizeof(vkey), "vib%u", i + 1);
        vibrationEnabled[i] = preferences.getBool(vkey, false);
    }

    Serial.printf("Nastavenia: zariadenie='%s', AP SSID='%s', STA SSID='%s', hlasitost=%u\n",
                  deviceName.c_str(), apSsid.c_str(), staSsid.c_str(), currentVolume);
}

void saveSettings(const String &newDeviceName, const String &newApSsid,
                  const String &newApPassword, const String &newStaSsid,
                  const String &newStaPassword, const String &newSettingsPassword,
                  uint8_t newVolume, const String newTrackNames[8],
                  const bool newVibrationEnabled[8]) {
    deviceName = newDeviceName.length() ? newDeviceName : DEFAULT_DEVICE;
    apSsid = newApSsid.length() ? newApSsid : DEFAULT_AP_SSID;
    apPassword = newApPassword;
    staSsid = newStaSsid;
    staPassword = newStaPassword;
    settingsPassword = newSettingsPassword.length() ? newSettingsPassword : DEFAULT_SETTINGS_PASSWORD;
    currentVolume = constrain(newVolume, 0, 30);

    if (deviceName.length() > 32) deviceName = deviceName.substring(0, 32);
    if (apSsid.length() > 32) apSsid = apSsid.substring(0, 32);
    if (apPassword.length() < 8 || apPassword.length() > 63) apPassword = DEFAULT_AP_PASSWORD;
    if (settingsPassword.length() > 32) settingsPassword = settingsPassword.substring(0, 32);

    preferences.putString("device", deviceName);
    preferences.putString("apssid", apSsid);
    preferences.putString("appass", apPassword);
    preferences.putString("ssid", staSsid);
    preferences.putString("wpass", staPassword);
    preferences.putString("setpass", settingsPassword);
    preferences.putUChar("volume", currentVolume);

    for (uint8_t i = 0; i < 8; i++) {
        String name = newTrackNames[i];
        name.trim();
        if (name.length() == 0) name = "Skladba " + String(i + 1);
        if (name.length() > 40) name = name.substring(0, 40);
        trackNames[i] = name;
        vibrationEnabled[i] = newVibrationEnabled[i];

        char key[8]; snprintf(key, sizeof(key), "trk%u", i + 1);
        preferences.putString(key, trackNames[i]);
        char vkey[8]; snprintf(vkey, sizeof(vkey), "vib%u", i + 1);
        preferences.putBool(vkey, vibrationEnabled[i]);
    }
}

void resetToDefaults() {
    String defaults[8];
    bool noVibration[8];
    for (uint8_t i = 0; i < 8; i++) {
        defaults[i] = "Skladba " + String(i + 1);
        noVibration[i] = false;
    }
    saveSettings(DEFAULT_DEVICE, DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD,
                 "", "", DEFAULT_SETTINGS_PASSWORD, 20, defaults, noVibration);
    player.setVolume(currentVolume);
}

String htmlEscape(const String &value) {
    String s = value;
    s.replace("&", "&amp;"); s.replace("<", "&lt;"); s.replace(">", "&gt;");
    s.replace("\"", "&quot;"); s.replace("'", "&#39;");
    return s;
}

String jsonEscape(const String &value) {
    String s = value;
    s.replace("\\", "\\\\"); s.replace("\"", "\\\"");
    s.replace("\r", "\\r"); s.replace("\n", "\\n");
    return s;
}

bool settingsAuthorized(AsyncWebServerRequest *request) {
    if (!request->hasHeader("Cookie")) return false;
    String cookie = request->getHeader("Cookie")->value();
    return settingsSessionToken.length() > 0 &&
           cookie.indexOf("DYSESSION=" + settingsSessionToken) >= 0;
}

void redirectToLogin(AsyncWebServerRequest *request) {
    request->redirect("/settings-login");
}

String makeBackup() {
    String out;
    out.reserve(5000);
    out += "# DY1703A Player configuration\n";
    out += "version=1\n";
    out += "device=" + deviceName + "\n";
    out += "apssid=" + apSsid + "\n";
    out += "appass=" + apPassword + "\n";
    out += "ssid=" + staSsid + "\n";
    out += "wpass=" + staPassword + "\n";
    out += "setpass=" + settingsPassword + "\n";
    out += "volume=" + String(currentVolume) + "\n";
    for (uint8_t i = 0; i < 8; i++) {
        out += "trk" + String(i + 1) + "=" + trackNames[i] + "\n";
        out += "vib" + String(i + 1) + "=" + String(vibrationEnabled[i] ? 1 : 0) + "\n";
    }
    return out;
}

bool parseBackup(const String &data) {
    String newDevice = DEFAULT_DEVICE, newApSsid = DEFAULT_AP_SSID;
    String newApPassword = DEFAULT_AP_PASSWORD, newStaSsid, newStaPassword;
    String newSettingsPassword = DEFAULT_SETTINGS_PASSWORD;
    String newTrackNames[8];
    bool newVibration[8];
    int newVolume = 20;

    for (uint8_t i = 0; i < 8; i++) {
        newTrackNames[i] = "Skladba " + String(i + 1);
        newVibration[i] = false;
    }

    int start = 0;
    while (start < data.length()) {
        int end = data.indexOf('\n', start);
        if (end < 0) end = data.length();
        String line = data.substring(start, end);
        line.trim();
        start = end + 1;
        if (line.length() == 0 || line.startsWith("#")) continue;

        int eq = line.indexOf('=');
        if (eq <= 0) continue;
        String key = line.substring(0, eq);
        String value = line.substring(eq + 1);

        if (key == "device") newDevice = value;
        else if (key == "apssid") newApSsid = value;
        else if (key == "appass") newApPassword = value;
        else if (key == "ssid") newStaSsid = value;
        else if (key == "wpass") newStaPassword = value;
        else if (key == "setpass") newSettingsPassword = value;
        else if (key == "volume") newVolume = value.toInt();
        else if (key.startsWith("trk")) {
            int n = key.substring(3).toInt();
            if (n >= 1 && n <= 8) newTrackNames[n - 1] = value;
        } else if (key.startsWith("vib")) {
            int n = key.substring(3).toInt();
            if (n >= 1 && n <= 8) newVibration[n - 1] = value.toInt() != 0;
        }
    }

    newDevice.trim(); newApSsid.trim(); newApPassword.trim();
    newStaSsid.trim(); newSettingsPassword.trim();
    if (newDevice.length() == 0 || newApSsid.length() == 0) return false;
    if (newApPassword.length() < 8 || newApPassword.length() > 63) return false;
    if (newSettingsPassword.length() == 0) return false;

    newVolume = constrain(newVolume, 0, 30);
    saveSettings(newDevice, newApSsid, newApPassword, newStaSsid,
                 newStaPassword, newSettingsPassword, (uint8_t)newVolume,
                 newTrackNames, newVibration);
    player.setVolume(currentVolume);
    return true;
}

void setupWebServer();

void startAP() {
    Serial.printf("Spúšťam WiFi AP '%s'...\n", apSsid.c_str());
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);

    bool ok = WiFi.softAP(apSsid.c_str(), apPassword.c_str());
    if (!ok) {
        Serial.println(F("CHYBA: WiFi AP sa nepodarilo spustiť"));
        WiFi.mode(WIFI_OFF);
        wifiActive = false;
        return;
    }

    wifiActive = true;
    wifiIsSta = false;
    wifiConnectDeadlineMs = millis() + WIFI_CONNECT_TIMEOUT_MS;
    wifiDisconnectGraceDeadlineMs = 0;

    dnsServer.start(53, "*", WiFi.softAPIP());
    setupWebServer();

    Serial.printf("WiFi AP: %s, IP: %s, heslo: %s\n",
                  apSsid.c_str(), WiFi.softAPIP().toString().c_str(), apPassword.c_str());
}

bool startSTA() {
    if (staSsid.length() == 0) return false;

    Serial.printf("Pripájam sa na WiFi '%s'...\n", staSsid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(staSsid.c_str(), staPassword.c_str());

    unsigned long deadline = millis() + WIFI_STA_TIMEOUT_MS;
    while (WiFi.status() != WL_CONNECTED && (long)(millis() - deadline) < 0) {
        delay(100);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("WiFi STA: pripojenie zlyhalo -> prepínam na AP"));
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        return false;
    }

    wifiActive = true;
    wifiIsSta = true;
    wifiConnectDeadlineMs = 0;
    wifiDisconnectGraceDeadlineMs = 0;
    setupWebServer();

    Serial.printf("WiFi STA pripojená, IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

void startWiFi() {
    if (wifiActive) return;
    if (staSsid.length() > 0 && startSTA()) return;
    startAP();
}

void stopWiFi() {
    if (!wifiActive) return;
    Serial.println(F("WiFi nečinné -> vypínam WiFi a povoľujem sleep"));

    if (!wifiIsSta) dnsServer.stop();
    if (wifiIsSta) WiFi.disconnect(true, true);
    else WiFi.softAPdisconnect(true);

    WiFi.mode(WIFI_OFF);
    wifiActive = false;
    wifiIsSta = false;
    wifiConnectDeadlineMs = 0;
    wifiDisconnectGraceDeadlineMs = 0;
}

void setupWebServer() {
    if (webServerStarted) return;

    // Hlavná stránka
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = R"HTML(<!doctype html><html lang="sk"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>)HTML" + htmlEscape(deviceName) + R"HTML(</title>
<style>
body{font-family:Arial,sans-serif;max-width:520px;margin:auto;padding:12px;background:#f4f4f4;color:#222}
h1{text-align:center;font-size:24px;margin:8px 0 14px}.card{background:#fff;border-radius:14px;padding:15px;margin:10px 0;box-shadow:0 2px 8px #0001}
h3{margin:0 0 12px}.trackList{display:flex;flex-direction:column;gap:6px;margin-top:8px}
button{font-size:19px;padding:16px 10px;border:0;border-radius:12px;background:#ddd;cursor:pointer}
button:active{transform:scale(.98)}.active{outline:4px solid #555}
.playbar{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}.playbar button{padding:14px 5px}
.controls{display:flex;gap:9px;align-items:center}.controls button{flex:0 0 58px;padding:12px}.controls input{flex:1}
.setvol{flex:0 0 64px!important;font-size:15px!important;padding:12px 5px!important}
.status{text-align:center;font-size:17px}.small{text-align:center;color:#666;font-size:14px;margin-top:7px}
.battery{height:18px;background:#ddd;border-radius:10px;overflow:hidden}.bar{height:100%;width:0;background:#555}
.settingsBtn{width:100%;font-size:17px;background:#eee;padding:12px;margin-top:4px}
.trackBtn{width:94%;align-self:center;font-size:17px;padding:9px 12px;text-align:left}
.trackIcon{display:inline-block;width:28px;text-align:center;margin-right:8px}
.msg{text-align:center;margin-top:10px;min-height:20px;font-size:14px}
</style></head><body>
<h1 id="title"></h1>
<div class="card status"><div id="state">Stav: --</div><div id="track">Skladba: --</div></div>
<div class="card"><h3>Skladby</h3><div class="trackList" id="tracks"></div></div>
<div class="card"><h3>Ovládanie</h3><div class="playbar">
<button onclick="cmd('/api/play')">▶ Play</button><button onclick="cmd('/api/pause')">⏸ Pauza</button><button onclick="cmd('/api/stop')">■ Stop</button>
</div></div>
<div class="card"><h3>Hlasitosť</h3><div class="controls">
<button onclick="volume(-1)">−</button><input id="vol" type="range" min="0" max="30" value="20" oninput="document.getElementById('volText').textContent=this.value">
<button onclick="volume(1)">+</button><button class="setvol" onclick="saveVolume()">SET</button>
</div><div class="small" id="volText">20</div></div>
<div class="card"><h3>Batéria</h3><div class="battery"><div class="bar" id="bar"></div></div><div class="small" id="bat">--</div></div>
<div class="card"><button class="settingsBtn" onclick="location.href='/settings'">⚙ Nastavenia</button></div>
<script>
const tracks=document.getElementById('tracks');
for(let i=1;i<=8;i++){
 let b=document.createElement('button');b.className='trackBtn';
 b.innerHTML='<span class="trackIcon" id="ti'+i+'">◻</span><span id="tnmain'+i+'">Skladba '+i+'</span>';
 b.onclick=()=>cmd('/api/play?track='+i);b.id='t'+i;tracks.appendChild(b);
}
async function cmd(u){try{await fetch(u);update()}catch(e){}}
async function volume(d){let v=Number(document.getElementById('vol').value)+d;v=Math.max(0,Math.min(30,v));document.getElementById('vol').value=v;document.getElementById('volText').textContent=v;try{await fetch('/api/volume?value='+v)}catch(e){}}
async function saveVolume(){let v=document.getElementById('vol').value;try{let r=await fetch('/api/volume/set?value='+v);document.getElementById('volText').textContent=await r.text();setTimeout(update,500)}catch(e){}}
async function update(){try{let r=await fetch('/api/status',{cache:'no-store'}),s=await r.json();
document.getElementById('title').textContent=s.device;
document.getElementById('state').textContent='Stav: '+(s.playing?'▶ PREHRÁVA SA':'■ STOP');
document.getElementById('track').textContent='Skladba: '+(s.track?s.track:'—');
if(document.activeElement.id!=='vol'){document.getElementById('vol').value=s.volume}
if(document.activeElement.id!=='vol')document.getElementById('volText').textContent=s.volume;
document.getElementById('bar').style.width=s.battery+'%';document.getElementById('bat').textContent=s.battery+' % · '+s.voltage+' mV';
if(s.names)for(let i=1;i<=8;i++){document.getElementById('tnmain'+i).textContent=s.names[i-1];document.getElementById('ti'+i).textContent=s.vibration&&s.vibration[i-1]?'📳':'◻';document.getElementById('t'+i).classList.toggle('active',s.playing&&s.track===i)}
}catch(e){}}
update();setInterval(update,1000);
</script></body></html>)HTML";
        request->send(200, "text/html; charset=utf-8", html);
    });

    // Captive portal – všetky DNS požiadavky v AP smerujú na ESP32 a bežné OS probe URL
    // sú presmerované na hlavnú stránku.
    const char *portalPaths[] = {"/generate_204","/gen_204","/hotspot-detect.html","/connecttest.txt","/ncsi.txt","/fwlink"};
    for (const char *path : portalPaths) {
        server.on(path, HTTP_ANY, [](AsyncWebServerRequest *request) {
            request->redirect("http://192.168.4.1/");
        });
    }

    // Priamy vstup na /settings je chránený heslom.
    server.on("/settings-login", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = R"HTML(<!doctype html><html lang="sk"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Nastavenia</title><style>
body{font-family:Arial,sans-serif;max-width:430px;margin:auto;padding:18px;background:#f4f4f4}
.card{background:#fff;border-radius:14px;padding:20px;box-shadow:0 2px 8px #0001}h2{text-align:center}
input,button{box-sizing:border-box;width:100%;padding:13px;margin-top:10px;border-radius:9px;border:1px solid #bbb;font-size:17px}
button{border:0;background:#ddd}.msg{text-align:center;min-height:22px;margin-top:12px}
</style></head><body><div class="card"><h2>⚙ Nastavenia</h2>
<div>Heslo pre vstup do nastavení</div><input id="pw" type="password" autofocus>
<button onclick="login()">Prihlásiť</button><div class="msg" id="msg"></div></div>
<script>
async function login(){let p=new URLSearchParams();p.append('password',document.getElementById('pw').value);
let r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p});
if(r.ok)location.href='/settings';else document.getElementById('msg').textContent=await r.text()}
document.getElementById('pw').addEventListener('keydown',e=>{if(e.key==='Enter')login()});
</script></body></html>)HTML";
        request->send(200, "text/html; charset=utf-8", html);
    });

    server.on("/api/login", HTTP_POST, [](AsyncWebServerRequest *request) {
        String pw = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
        if (pw == settingsPassword) {
            settingsSessionToken = String((uint32_t)ESP.getEfuseMac(), HEX) + String(millis(), HEX);
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
            response->addHeader("Set-Cookie", "DYSESSION=" + settingsSessionToken + "; Path=/; SameSite=Strict");
            request->send(response);
        } else {
            request->send(401, "text/plain", "Nesprávne heslo");
        }
    });

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!settingsAuthorized(request)) { redirectToLogin(request); return; }

        String html = R"HTML(<!doctype html><html lang="sk"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Nastavenia</title>
<style>
body{font-family:Arial,sans-serif;max-width:560px;margin:auto;padding:12px;background:#f4f4f4;color:#222}
h1{font-size:23px;text-align:center}.card{background:#fff;border-radius:14px;padding:15px;margin:10px 0;box-shadow:0 2px 8px #0001}
h3{margin:4px 0 12px}.field{margin:11px 0}.field>label{display:block;font-size:14px;font-weight:bold;margin-bottom:5px}
input{box-sizing:border-box;width:100%;padding:11px;border:1px solid #bbb;border-radius:9px;font-size:16px}
.passwordRow{display:flex;gap:7px}.passwordRow input{flex:1}.showPass{flex:0 0 auto;font-size:14px;padding:9px 11px}
.submenu{width:100%;font-size:16px;background:#eee;padding:12px;margin-top:8px;text-align:left;border:0;border-radius:9px}
#trackSettings{display:none}.trackFieldRow{display:flex;align-items:center;gap:8px;margin:8px 0}.trackName{flex:1;min-width:0}
.trackOption{display:flex;align-items:center;gap:5px}.trackOption input{width:20px;height:20px}
.action{width:100%;padding:12px;border:0;border-radius:9px;background:#ddd;font-size:16px;margin-top:7px}
.warn{background:#ddd}.msg{text-align:center;min-height:22px;margin-top:10px;font-size:14px}
a{display:block;text-align:center;margin:12px 0;color:#333}.file{margin-top:10px}
</style></head><body>
<h1>⚙ Nastavenia</h1>
<div class="card"><h3>Zariadenie a Wi-Fi</h3>
<div class="field"><label>Názov zariadenia</label><input id="device" maxlength="32"></div>
<div class="field"><label>SSID zariadenia (AP)</label><input id="apssid" maxlength="32"></div>
<div class="field"><label>Heslo AP (min. 8 znakov)</label><div class="passwordRow"><input id="appass" type="password" maxlength="63"><button class="showPass" type="button" onclick="toggle('appass',this)">👁</button></div></div>
<div class="field"><label>Domáca Wi-Fi SSID</label><input id="ssid" maxlength="64"></div>
<div class="field"><label>Domáca Wi-Fi heslo</label><div class="passwordRow"><input id="wpass" type="password" maxlength="64"><button class="showPass" type="button" onclick="toggle('wpass',this)">👁</button></div></div>
</div>
<div class="card"><h3>Zariadenie a WIFI</h3>
<div class="field"><label>Heslo pre vstup do nastavení</label><div class="passwordRow"><input id="setpass" type="password" maxlength="32"><button class="showPass" type="button" onclick="toggle('setpass',this)">👁</button></div></div>
</div>
<div class="card"><h3>Názvy skladieb a vibrácia</h3>
<button class="submenu" type="button" onclick="toggleTracks()">🎵 Nastavenie skladieb</button>
<div id="trackSettings"><div id="trackFields"></div></div>
</div>
<div class="card"><h3>Údržba</h3>
<a href="/api/backup">⬇ Zápis konfigurácie</a>
<div>Obnovenie konfigurácie:</div><input class="file" id="restore" type="file" accept=".txt,.cfg,.conf">
<button class="action" onclick="restore()">⬆ Obnovenie konfigurácie</button>
<button class="action warn" onclick="defaults()">↺ Obnovenie výrobných nastavení</button>
<div class="msg" id="msg"></div></div>
<div class="card"><h3>Príkazy</h3>
<button class="action" onclick="saveSettings()">💾 Uložiť a reštart</button>
<button class="action" onclick="location.href='/'">↩ Návrat bez uloženia</button>
</div>
<script>
const tf=document.getElementById('trackFields');
for(let i=1;i<=8;i++){
 let row=document.createElement('div');row.className='trackFieldRow';
 row.innerHTML='<input class="trackName" id="tn'+i+'" maxlength="40" placeholder="Skladba '+i+'"><div class="trackOption"><input type="checkbox" id="tv'+i+'"><label for="tv'+i+'">📳</label></div>';
 tf.appendChild(row);
}
function toggle(id,b){let e=document.getElementById(id);e.type=e.type==='password'?'text':'password';b.textContent=e.type==='password'?'👁':'🙈'}
function toggleTracks(){let e=document.getElementById('trackSettings');e.style.display=e.style.display==='block'?'none':'block'}
async function load(){let r=await fetch('/api/settings',{cache:'no-store'});if(!r.ok){location.href='/settings-login';return}let s=await r.json();
document.getElementById('device').value=s.device;document.getElementById('apssid').value=s.apssid;document.getElementById('appass').value=s.appass;document.getElementById('ssid').value=s.ssid;document.getElementById('wpass').value=s.wpass;document.getElementById('setpass').value=s.setpass;
for(let i=1;i<=8;i++){document.getElementById('tn'+i).value=s.tracks[i-1];document.getElementById('tv'+i).checked=!!s.vibration[i-1]}}
async function saveSettings(){let p=new URLSearchParams();p.append('device',document.getElementById('device').value);p.append('apssid',document.getElementById('apssid').value);p.append('appass',document.getElementById('appass').value);p.append('ssid',document.getElementById('ssid').value);p.append('wpass',document.getElementById('wpass').value);p.append('setpass',document.getElementById('setpass').value);
for(let i=1;i<=8;i++){p.append('track'+i,document.getElementById('tn'+i).value);if(document.getElementById('tv'+i).checked)p.append('vib'+i,'1')}
let b=document.querySelector('.action');b.disabled=true;document.getElementById('msg').textContent='Ukladám...';
try{let r=await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p});document.getElementById('msg').textContent=await r.text();if(r.ok)setTimeout(()=>location.href='/',3000)}catch(e){b.disabled=false;document.getElementById('msg').textContent='Chyba komunikácie'}}
async function defaults(){if(!confirm('Naozaj obnoviť výrobné nastavenie? Vymaže sa aj Wi-Fi STA a názvy skladieb.'))return;let r=await fetch('/api/defaults',{method:'POST'});document.getElementById('msg').textContent=await r.text();if(r.ok)setTimeout(()=>location.href='/',3000)}
async function restore(){let f=document.getElementById('restore').files[0];if(!f){document.getElementById('msg').textContent='Vyberte súbor konfigurácie.';return}let fd=new FormData();fd.append('file',f);document.getElementById('msg').textContent='Obnovujem...';let r=await fetch('/api/restore',{method:'POST',body:fd});document.getElementById('msg').textContent=await r.text();if(r.ok)setTimeout(()=>location.href='/',3000)}
load();
</script></body></html>)HTML";
        request->send(200, "text/html; charset=utf-8", html);
    });

    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!settingsAuthorized(request)) { request->send(401, "text/plain", "Neautorizované"); return; }
        String tracksJson="[", vibJson="[";
        for (uint8_t i=0;i<8;i++) {
            if(i){tracksJson+=",";vibJson+=",";}
            tracksJson+="\"" + jsonEscape(trackNames[i]) + "\"";
            vibJson += vibrationEnabled[i] ? "true" : "false";
        }
        tracksJson+="]";vibJson+="]";
        String json="{\"device\":\"" + jsonEscape(deviceName) +
                    "\",\"apssid\":\"" + jsonEscape(apSsid) +
                    "\",\"appass\":\"" + jsonEscape(apPassword) +
                    "\",\"ssid\":\"" + jsonEscape(staSsid) +
                    "\",\"wpass\":\"" + jsonEscape(staPassword) +
                    "\",\"setpass\":\"" + jsonEscape(settingsPassword) +
                    "\",\"tracks\":" + tracksJson +
                    ",\"vibration\":" + vibJson + "}";
        request->send(200, "application/json", json);
    });

    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!settingsAuthorized(request)) { request->send(401, "text/plain", "Neautorizované"); return; }

        String newDevice=request->hasParam("device",true)?request->getParam("device",true)->value():deviceName;
        String newApSsid=request->hasParam("apssid",true)?request->getParam("apssid",true)->value():apSsid;
        String newApPassword=request->hasParam("appass",true)?request->getParam("appass",true)->value():apPassword;
        String newStaSsid=request->hasParam("ssid",true)?request->getParam("ssid",true)->value():staSsid;
        String newStaPassword=request->hasParam("wpass",true)?request->getParam("wpass",true)->value():staPassword;
        String newSettingsPassword=request->hasParam("setpass",true)?request->getParam("setpass",true)->value():settingsPassword;

        newDevice.trim();newApSsid.trim();newApPassword.trim();newStaSsid.trim();newSettingsPassword.trim();
        if(newDevice.length()==0)newDevice=DEFAULT_DEVICE;
        if(newApSsid.length()==0)newApSsid=DEFAULT_AP_SSID;
        if(newApPassword.length()<8||newApPassword.length()>63){request->send(400,"text/plain","Heslo AP musí mať 8 až 63 znakov.");return;}
        if(newSettingsPassword.length()==0){request->send(400,"text/plain","Heslo pre nastavenia nesmie byť prázdne.");return;}

        String newTrackNames[8];bool newVibration[8];
        for(uint8_t i=0;i<8;i++){
            char param[10];snprintf(param,sizeof(param),"track%u",i+1);
            newTrackNames[i]=request->hasParam(param,true)?request->getParam(param,true)->value():trackNames[i];
            newTrackNames[i].trim();
            if(newTrackNames[i].length()==0)newTrackNames[i]="Skladba "+String(i+1);
            snprintf(param,sizeof(param),"vib%u",i+1);
            newVibration[i]=request->hasParam(param,true);
        }

        saveSettings(newDevice,newApSsid,newApPassword,newStaSsid,newStaPassword,
                     newSettingsPassword,currentVolume,newTrackNames,newVibration);
        lastActivityMs=millis();
        wifiRestartPending=true;
        wifiRestartAtMs=millis()+2500UL;
        request->send(200,"text/plain","Nastavenia uložené. ESP32 sa reštartuje a stránka sa obnoví...");
    });

    server.on("/api/defaults", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!settingsAuthorized(request)) { request->send(401, "text/plain", "Neautorizované"); return; }
        resetToDefaults();
        lastActivityMs=millis();
        wifiRestartPending=true;
        wifiRestartAtMs=millis()+2500UL;
        request->send(200,"text/plain","Výrobné nastavenie uložené. ESP32 sa reštartuje...");
    });

    server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!settingsAuthorized(request)) { redirectToLogin(request); return; }
        AsyncWebServerResponse *response=request->beginResponse(200,"text/plain; charset=utf-8",makeBackup());
        response->addHeader("Content-Disposition","attachment; filename=dy1703a-config.txt");
        request->send(response);
    });

    server.on("/api/restore", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!settingsAuthorized(request)) { request->send(401,"text/plain","Neautorizované"); return; }
        if (!restoreUploadOk || restoreBuffer.length()==0) {
            request->send(400,"text/plain","Konfiguráciu sa nepodarilo načítať.");
            return;
        }
        if (!parseBackup(restoreBuffer)) {
            request->send(400,"text/plain","Neplatný konfiguračný súbor alebo heslo AP nemá 8 až 63 znakov.");
            restoreBuffer="";
            restoreUploadOk=false;
            return;
        }
        restoreBuffer="";
        restoreUploadOk=false;
        lastActivityMs=millis();
        wifiRestartPending=true;
        wifiRestartAtMs=millis()+2500UL;
        request->send(200,"text/plain","Konfigurácia obnovená. ESP32 sa reštartuje...");
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if(index==0){restoreBuffer="";restoreUploadOk=true;}
        if(restoreBuffer.length()+len>12000){restoreUploadOk=false;return;}
        for(size_t i=0;i<len;i++)restoreBuffer+=(char)data[i];
        if(final)Serial.printf("Konfiguračný súbor '%s' načítaný (%u B)\n",filename.c_str(),(unsigned)restoreBuffer.length());
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String namesJson="[",vibJson="[";
        for(uint8_t i=0;i<8;i++){
            if(i){namesJson+=",";vibJson+=",";}
            namesJson+="\"" + jsonEscape(trackNames[i]) + "\"";
            vibJson+=vibrationEnabled[i]?"true":"false";
        }
        namesJson+="]";vibJson+="]";
        String json="{\"device\":\"" + jsonEscape(deviceName) +
                    "\",\"playing\":" + String(currentPlaying?"true":"false") +
                    ",\"track\":" + String(currentTrack) +
                    ",\"volume\":" + String(currentVolume) +
                    ",\"battery\":" + String(batteryPct) +
                    ",\"voltage\":" + String(batteryMv) +
                    ",\"names\":" + namesJson +
                    ",\"vibration\":" + vibJson + "}";
        request->send(200,"application/json",json);
    });

    server.on("/api/play",HTTP_GET,[](AsyncWebServerRequest *request){
        if(request->hasParam("track")){
            int track=request->getParam("track")->value().toInt();
            if(track>=1&&track<=8){currentTrack=track;player.playTrack(currentTrack);}
        } else player.play();
        lastActivityMs=millis();
        request->send(200,"text/plain","OK");
    });
    server.on("/api/stop",HTTP_GET,[](AsyncWebServerRequest *request){
        player.stop();currentPlaying=false;setMotor(false);lastActivityMs=millis();
        request->send(200,"text/plain","OK");
    });
    server.on("/api/pause",HTTP_GET,[](AsyncWebServerRequest *request){
        player.pause();currentPlaying=false;setMotor(false);lastActivityMs=millis();
        request->send(200,"text/plain","OK");
    });
    server.on("/api/volume",HTTP_GET,[](AsyncWebServerRequest *request){
        if(request->hasParam("value")){
            int value=request->getParam("value")->value().toInt();
            currentVolume=constrain(value,0,30);
            player.setVolume(currentVolume);
            lastActivityMs=millis();
        }
        request->send(200,"text/plain",String(currentVolume));
    });

    server.on("/api/volume/set",HTTP_GET,[](AsyncWebServerRequest *request){
        if(request->hasParam("value")){
            int value=request->getParam("value")->value().toInt();
            currentVolume=constrain(value,0,30);
            player.setVolume(currentVolume);
            preferences.putUChar("volume",currentVolume);
            lastActivityMs=millis();
        }
        request->send(200,"text/plain","Hlasitosť uložená: "+String(currentVolume));
    });

    server.begin();
    webServerStarted=true;
}

void handlePendingWiFiRestart(){
    if(!wifiRestartPending||(long)(millis()-wifiRestartAtMs)<0)return;
    wifiRestartPending=false;
    setMotor(false);
    Serial.println(F("Používateľ uložil konfiguráciu -> reštartujem ESP32"));
    Serial.flush();
    delay(50);
    ESP.restart();
}

void handleWiFi(){
    if(!wifiActive)return;
    if(wifiIsSta){
        if(WiFi.status()!=WL_CONNECTED){
            Serial.println(F("WiFi STA bolo odpojené -> skúšam znovu AP"));
            stopWiFi();startAP();return;
        }
        lastActivityMs=millis();
        return;
    }

    dnsServer.processNextRequest();
    unsigned long now=millis();
    uint8_t clients=WiFi.softAPgetStationNum();

    if(clients>0){wifiDisconnectGraceDeadlineMs=0;lastActivityMs=now;return;}
    if(wifiConnectDeadlineMs!=0&&(long)(now-wifiConnectDeadlineMs)<0)return;
    if(wifiDisconnectGraceDeadlineMs==0)wifiDisconnectGraceDeadlineMs=now+WIFI_DISCONNECT_GRACE_MS;
    if((long)(now-wifiDisconnectGraceDeadlineMs)>=0){stopWiFi();lastActivityMs=now;}
}

bool handleButtons(){
    unsigned long now=millis();bool pressed=false;
    for(auto &b:buttons){
        bool state=digitalRead(b.pin);
        if(state!=b.lastState&&(now-b.lastChangeMs)>BTN_DEBOUNCE_MS){
            b.lastChangeMs=now;b.lastState=state;
            if(state==LOW){
                currentTrack=b.trackNumber;
                player.playTrack(b.trackNumber);
                startWiFi();
                pressed=true;
            }
        }
    }
    return pressed;
}

void maybeEnterLightSleep(){
    if(wifiActive)return;
    unsigned long now=millis();
    if(now-lastActivityMs<INACTIVITY_TIMEOUT_MS)return;
    if(currentPlaying)return;
    setMotor(false);
    Serial.println(F("Nečinnosť -> light sleep (WiFi OFF)"));
    Serial.flush();

    esp_sleep_enable_gpio_wakeup();
    for(uint8_t pin:TRACK_BUTTON_PINS)
        gpio_wakeup_enable((gpio_num_t)pin,GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_CHECK_INTERVAL_MS*1000ULL);
    esp_light_sleep_start();

    for(uint8_t pin:TRACK_BUTTON_PINS)
        gpio_wakeup_disable((gpio_num_t)pin);

    if(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_GPIO){
        Serial.println(F("Prebudenie tlačidlom"));
        lastActivityMs=millis();
    }else Serial.println(F("Periodické prebudenie (kontrola stavu)"));
}

void setup(){
    Serial.begin(115200);
    delay(300);
    setupButtons();
    loadSettings();
    player.begin(DY_BAUD_RATE,DY_RX_PIN,DY_TX_PIN);
    player.setVolume(currentVolume);
    battery.begin();
    settingsSessionToken="";
    lastActivityMs=millis();
    Serial.println(F("DY1703A ESP32 Player – ready"));
    startWiFi();
}

void loop(){
    if(handleButtons())lastActivityMs=millis();
    player.poll();

    static unsigned long lastBatteryRead=0,lastMotorPoll=0;
    unsigned long now=millis();

    if(now-lastMotorPoll>=MOTOR_POLL_INTERVAL_MS){
        lastMotorPoll=now;
        currentPlaying=player.isPlaying();
        setMotor(currentPlaying&&currentTrack>=1&&currentTrack<=8&&vibrationEnabled[currentTrack-1]);
        if(currentPlaying)lastActivityMs=now;
    }

    if(now-lastBatteryRead>=BATTERY_READ_INTERVAL_MS){
        lastBatteryRead=now;
        batteryMv=battery.readVoltageMv();
        batteryPct=battery.readPercent();
        String ip = wifiIsSta ? WiFi.localIP().toString() : (wifiActive ? WiFi.softAPIP().toString() : String("OFF"));
        Serial.printf("Batéria: %lu mV (~%u%%) | IP: %s\n", batteryMv, batteryPct, ip.c_str());
    }

    handlePendingWiFiRestart();
    handleWiFi();
    maybeEnterLightSleep();
}
