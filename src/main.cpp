#include <Arduino.h>
#include <WiFi.h>
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
String staSsid;
String staPassword;
String trackNames[8];
bool vibrationEnabled[8];

bool isInputOnlyPin(uint8_t pin) {
    for (uint8_t p : INPUT_ONLY_PINS) if (p == pin) return true;
    return false;
}

void setMotor(bool on) {
    digitalWrite(MOTOR_PIN, on ? MOTOR_ACTIVE_LEVEL : (MOTOR_ACTIVE_LEVEL == HIGH ? LOW : HIGH));
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
    deviceName = preferences.getString("device", "DY1703A Player");
    apSsid = preferences.getString("apssid", WIFI_AP_SSID);
    staSsid = preferences.getString("ssid", WIFI_STA_SSID);
    staPassword = preferences.getString("wpass", WIFI_STA_PASS);
    currentVolume = (uint8_t)constrain(preferences.getUChar("volume", 20), 0, 30);
    if (deviceName.length() == 0) deviceName = "DY1703A Player";
    if (apSsid.length() == 0) apSsid = WIFI_AP_SSID;

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
                  const String &newStaSsid, const String &newStaPassword,
                  uint8_t newVolume, const String newTrackNames[8],
                  const bool newVibrationEnabled[8]) {
    deviceName = newDeviceName.length() ? newDeviceName : "DY1703A Player";
    apSsid = newApSsid.length() ? newApSsid : WIFI_AP_SSID;
    staSsid = newStaSsid;
    staPassword = newStaPassword;
    currentVolume = constrain(newVolume, 0, 30);
    if (deviceName.length() > 32) deviceName = deviceName.substring(0, 32);
    if (apSsid.length() > 32) apSsid = apSsid.substring(0, 32);

    preferences.putString("device", deviceName);
    preferences.putString("apssid", apSsid);
    preferences.putString("ssid", staSsid);
    preferences.putString("wpass", staPassword);
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

void setupWebServer();

void startAP() {
    Serial.printf("Spúšťam WiFi AP '%s'...\n", apSsid.c_str());
    WiFi.mode(WIFI_AP); WiFi.setSleep(false);
    bool ok = WiFi.softAP(apSsid.c_str(), WIFI_AP_PASS);
    if (!ok) { Serial.println(F("CHYBA: WiFi AP sa nepodarilo spustiť")); WiFi.mode(WIFI_OFF); wifiActive = false; return; }
    wifiActive = true; wifiIsSta = false;
    wifiConnectDeadlineMs = millis() + WIFI_CONNECT_TIMEOUT_MS;
    wifiDisconnectGraceDeadlineMs = 0;
    setupWebServer();
    Serial.printf("WiFi AP: %s, IP: %s, heslo: %s\n", apSsid.c_str(), WiFi.softAPIP().toString().c_str(), WIFI_AP_PASS);
}

bool startSTA() {
    if (staSsid.length() == 0) return false;
    Serial.printf("Pripájam sa na WiFi '%s'...\n", staSsid.c_str());
    WiFi.mode(WIFI_STA); WiFi.setSleep(false); WiFi.begin(staSsid.c_str(), staPassword.c_str());
    unsigned long deadline = millis() + WIFI_STA_TIMEOUT_MS;
    while (WiFi.status() != WL_CONNECTED && (long)(millis() - deadline) < 0) { delay(100); Serial.print('.'); }
    Serial.println();
    if (WiFi.status() != WL_CONNECTED) { Serial.println(F("WiFi STA: pripojenie zlyhalo -> prepínam na AP")); WiFi.disconnect(true, true); WiFi.mode(WIFI_OFF); return false; }
    wifiActive = true; wifiIsSta = true; wifiConnectDeadlineMs = 0; wifiDisconnectGraceDeadlineMs = 0;
    setupWebServer();
    Serial.printf("WiFi STA pripojená, IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

void startWiFi() { if (wifiActive) return; if (staSsid.length() > 0 && startSTA()) return; startAP(); }

void stopWiFi() {
    if (!wifiActive) return;
    Serial.println(F("WiFi nečinné -> vypínam WiFi a povoľujem sleep"));
    if (wifiIsSta) WiFi.disconnect(true, true); else WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF); wifiActive = false; wifiIsSta = false; wifiConnectDeadlineMs = 0; wifiDisconnectGraceDeadlineMs = 0;
}

String htmlEscape(const String &value) {
    String s = value; s.replace("&", "&amp;"); s.replace("<", "&lt;"); s.replace(">", "&gt;"); s.replace("\"", "&quot;"); s.replace("'", "&#39;"); return s;
}
String jsonEscape(const String &value) {
    String s = value; s.replace("\\", "\\\\"); s.replace("\"", "\\\""); s.replace("\r", "\\r"); s.replace("\n", "\\n"); return s;
}

void setupWebServer() {
    if (webServerStarted) return;
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = R"HTML(<!doctype html><html lang="sk"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>)HTML" + htmlEscape(deviceName) + R"HTML(</title>
<style>
body{font-family:Arial,sans-serif;max-width:520px;margin:auto;padding:12px;background:#f4f4f4;color:#222}
h1{text-align:center;font-size:24px;margin:8px 0 14px}.card{background:#fff;border-radius:14px;padding:15px;margin:10px 0;box-shadow:0 2px 8px #0001}
h3{margin:0 0 12px}.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
button{font-size:19px;padding:16px 10px;border:0;border-radius:12px;background:#ddd;cursor:pointer}button:active{transform:scale(.98)}.active{outline:4px solid #555}
.playbar{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}.playbar button{padding:14px 5px}.controls{display:flex;gap:9px;align-items:center}.controls button{flex:0 0 58px;padding:12px}.controls input{flex:1}
.status{text-align:center;font-size:17px}.small{text-align:center;color:#666;font-size:14px;margin-top:7px}.battery{height:18px;background:#ddd;border-radius:10px;overflow:hidden}.bar{height:100%;width:0;background:#555}
.settingsBtn{width:100%;font-size:17px;background:#eee;padding:12px;margin-top:4px}#settings{display:none}.submenu{width:100%;font-size:16px;background:#eee;padding:12px;margin-top:8px;text-align:left}#trackSettings{display:none}.field{margin:11px 0}.field label{display:block;font-size:14px;font-weight:bold;margin-bottom:5px}.field input{box-sizing:border-box;width:100%;padding:11px;border:1px solid #bbb;border-radius:9px;font-size:16px}
.passwordRow{display:flex;gap:7px}.passwordRow input{flex:1}.showPass{flex:0 0 auto;font-size:15px;padding:9px 11px}.save{width:100%;background:#ccc;margin-top:8px}.back{width:100%;background:#eee;margin-top:8px;padding:12px;font-size:16px}.msg{text-align:center;margin-top:10px;min-height:20px;font-size:14px}
.trackList{display:flex;flex-direction:column;gap:6px;margin-top:8px}.trackBtn{width:94%;align-self:center;font-size:17px;padding:9px 12px;text-align:left}.trackIcon{display:inline-block;width:28px;text-align:center;margin-right:8px}.trackOption{display:flex;align-items:center;gap:8px;margin:0}.trackOption input{width:20px;height:20px;flex:0 0 auto}.trackOption label{margin:0;font-weight:normal;font-size:16px;display:flex;align-items:center;gap:7px}.trackFieldRow{display:flex;align-items:center;gap:8px}.trackFieldRow .trackName{flex:1;min-width:0}
</style></head><body>
<h1 id="title"></h1>
<div class="card status"><div id="state">Stav: --</div><div id="track">Skladba: --</div></div>
<div class="card"><h3>Skladby</h3><div class="trackList" id="tracks"></div></div>
<div class="card"><h3>Ovládanie</h3><div class="playbar"><button onclick="cmd('/api/play')">▶ Play</button><button onclick="cmd('/api/pause')">⏸ Pauza</button><button onclick="cmd('/api/stop')">■ Stop</button></div></div>
<div class="card"><h3>Hlasitosť</h3><div class="controls"><button onclick="volume(-1)">−</button><input id="vol" type="range" min="0" max="30" value="20" oninput="setVolume(this.value)"><button onclick="volume(1)">+</button></div><div class="small" id="volText">20</div></div>
<div class="card"><h3>Batéria</h3><div class="battery"><div class="bar" id="bar"></div></div><div class="small" id="bat">--</div></div>
<div class="card"><button class="settingsBtn" onclick="toggleSettings()">⚙ Nastavenia</button><div id="settings"><h3 style="margin-top:15px">Nastavenia zariadenia</h3>
<div class="field"><label>Názov zariadenia</label><input id="device" maxlength="32"></div>
<div class="field"><label>SSID zariadenia (AP)</label><input id="apssid" maxlength="32"></div>
<div class="field"><label>Domáca Wi-Fi SSID</label><input id="ssid" maxlength="64"></div>
<div class="field"><label>Domáca Wi-Fi heslo</label><div class="passwordRow"><input id="wpass" type="password" maxlength="64" autocomplete="new-password"><button class="showPass" type="button" onclick="togglePass()">👁 Zobraziť</button></div></div>
<button class="submenu" type="button" onclick="toggleTrackSettings()">🎵 Názvy skladieb a vibrácia</button>
<div id="trackSettings"><div class="small">Vpravo od názvu zaškrtnite vibráciu pre skladby, pri ktorých má motorček vibrovať počas prehrávania.</div><div id="trackFields"></div></div>
<div class="small">Zmeny sa uložia až po kliknutí na tlačidlo Uložiť nastavenia.</div>
<button class="save" onclick="saveSettings()">💾 Uložiť nastavenia</button>
<button class="back" type="button" onclick="discardSettings()">↩ Späť – neuložiť zmeny</button><div class="msg" id="msg"></div>
</div></div>
<script>
const tracks=document.getElementById('tracks');const trackFields=document.getElementById('trackFields');
for(let i=1;i<=8;i++){
 let b=document.createElement('button');b.className='trackBtn';b.innerHTML='<span class="trackIcon" id="ti'+i+'">◻</span><span id="tnmain'+i+'">Skladba '+i+'</span>';b.onclick=()=>cmd('/api/play?track='+i);b.id='t'+i;tracks.appendChild(b);
 let f=document.createElement('div');f.className='field';f.innerHTML='<div class="trackFieldRow"><input class="trackName" id="tn'+i+'" maxlength="40"><div class="trackOption"><input type="checkbox" id="tv'+i+'"><label for="tv'+i+'">📳</label></div></div>';trackFields.appendChild(f);
}
async function cmd(u){try{await fetch(u);update()}catch(e){}}
async function volume(d){let v=Number(document.getElementById('vol').value)+d;v=Math.max(0,Math.min(30,v));setVolume(v)}
async function setVolume(v){document.getElementById('vol').value=v;document.getElementById('volText').textContent=v;try{await fetch('/api/volume?value='+v);update()}catch(e){}}
function toggleSettings(){let e=document.getElementById('settings');e.style.display=e.style.display==='block'?'none':'block';if(e.style.display==='block')loadSettings()}
function toggleTrackSettings(){let e=document.getElementById('trackSettings');e.style.display=e.style.display==='block'?'none':'block'}
function togglePass(){let e=document.getElementById('wpass'),b=document.querySelector('.showPass');if(e.type==='password'){e.type='text';b.textContent='🙈 Skryť'}else{e.type='password';b.textContent='👁 Zobraziť'}}
function discardSettings(){window.location.href='/'}
async function loadSettings(){try{let r=await fetch('/api/settings',{cache:'no-store'}),s=await r.json();document.getElementById('device').value=s.device;document.getElementById('apssid').value=s.apssid;document.getElementById('ssid').value=s.ssid;for(let i=1;i<=8;i++){document.getElementById('tn'+i).value=s.tracks[i-1];document.getElementById('tv'+i).checked=!!s.vibration[i-1]}}catch(e){}}
async function saveSettings(){let p=new URLSearchParams();p.append('device',document.getElementById('device').value);p.append('apssid',document.getElementById('apssid').value);p.append('ssid',document.getElementById('ssid').value);p.append('volume',document.getElementById('vol').value);for(let i=1;i<=8;i++){p.append('track'+i,document.getElementById('tn'+i).value);if(document.getElementById('tv'+i).checked)p.append('vib'+i,'1')}let pw=document.getElementById('wpass').value;if(pw!=='')p.append('wpass',pw);let b=document.querySelector('.save');b.disabled=true;document.getElementById('msg').textContent='Ukladám...';try{let r=await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p});let text=await r.text();document.getElementById('msg').textContent=text;if(r.ok)setTimeout(()=>{window.location.href='/'},3000)}catch(e){b.disabled=false;document.getElementById('msg').textContent='Chyba komunikácie s ESP32'}}
async function update(){try{let r=await fetch('/api/status',{cache:'no-store'}),s=await r.json();document.getElementById('title').textContent=s.device;document.getElementById('state').textContent='Stav: '+(s.playing?'▶ PREHRÁVA SA':'■ STOP');document.getElementById('track').textContent='Skladba: '+(s.track?s.track:'—');document.getElementById('vol').value=s.volume;document.getElementById('volText').textContent=s.volume;document.getElementById('bar').style.width=s.battery+'%';document.getElementById('bat').textContent=s.battery+' % · '+s.voltage+' mV';if(s.names)for(let i=1;i<=8;i++){document.getElementById('tnmain'+i).textContent=s.names[i-1];document.getElementById('ti'+i).textContent=s.vibration&&s.vibration[i-1]?'📳':'◻';document.getElementById('t'+i).classList.toggle('active',s.playing&&s.track===i)}}catch(e){}}
update();setInterval(update,1000);
</script></body></html>)HTML";
        request->send(200,"text/html; charset=utf-8",html);
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String namesJson="[", vibJson="[";
        for(uint8_t i=0;i<8;i++){if(i){namesJson+=",";vibJson+=",";}namesJson+="\""+jsonEscape(trackNames[i])+"\"";vibJson+=vibrationEnabled[i]?"true":"false";}
        namesJson+="]";vibJson+="]";
        String json="{\"device\":\""+jsonEscape(deviceName)+"\",\"playing\":"+String(currentPlaying?"true":"false")+",\"track\":"+String(currentTrack)+",\"volume\":"+String(currentVolume)+",\"battery\":"+String(batteryPct)+",\"voltage\":"+String(batteryMv)+",\"names\":"+namesJson+",\"vibration\":"+vibJson+"}";
        request->send(200,"application/json",json);
    });

    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        String tracksJson="[",vibJson="[";
        for(uint8_t i=0;i<8;i++){if(i){tracksJson+=",";vibJson+=",";}tracksJson+="\""+jsonEscape(trackNames[i])+"\"";vibJson+=vibrationEnabled[i]?"true":"false";}
        tracksJson+="]";vibJson+="]";
        String json="{\"device\":\""+jsonEscape(deviceName)+"\",\"apssid\":\""+jsonEscape(apSsid)+"\",\"ssid\":\""+jsonEscape(staSsid)+"\",\"volume\":"+String(currentVolume)+",\"tracks\":"+tracksJson+",\"vibration\":"+vibJson+"}";
        request->send(200,"application/json",json);
    });

    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
        String newDevice=request->hasParam("device",true)?request->getParam("device",true)->value():deviceName;
        String newApSsid=request->hasParam("apssid",true)?request->getParam("apssid",true)->value():apSsid;
        String newStaSsid=request->hasParam("ssid",true)?request->getParam("ssid",true)->value():staSsid;
        String newPassword=staPassword;if(request->hasParam("wpass",true))newPassword=request->getParam("wpass",true)->value();
        int newVolume=request->hasParam("volume",true)?request->getParam("volume",true)->value().toInt():currentVolume;
        newDevice.trim();newApSsid.trim();newStaSsid.trim();if(newDevice.length()==0)newDevice="DY1703A Player";if(newApSsid.length()==0)newApSsid=WIFI_AP_SSID;if(newApSsid.length()>32)newApSsid=newApSsid.substring(0,32);if(newDevice.length()>32)newDevice=newDevice.substring(0,32);newVolume=constrain(newVolume,0,30);
        String newTrackNames[8];bool newVibrationEnabled[8];
        for(uint8_t i=0;i<8;i++){char param[8];snprintf(param,sizeof(param),"track%u",i+1);newTrackNames[i]=request->hasParam(param,true)?request->getParam(param,true)->value():trackNames[i];newTrackNames[i].trim();if(newTrackNames[i].length()==0)newTrackNames[i]="Skladba "+String(i+1);if(newTrackNames[i].length()>40)newTrackNames[i]=newTrackNames[i].substring(0,40);snprintf(param,sizeof(param),"vib%u",i+1);newVibrationEnabled[i]=request->hasParam(param,true);}
        saveSettings(newDevice,newApSsid,newStaSsid,newPassword,(uint8_t)newVolume,newTrackNames,newVibrationEnabled);player.setVolume(currentVolume);lastActivityMs=millis();wifiRestartPending=true;wifiRestartAtMs=millis()+2500UL;
        request->send(200,"text/plain","Nastavenia uložené. ESP32 sa reštartuje a stránka sa obnoví...");
    });

    server.on("/api/play",HTTP_GET,[](AsyncWebServerRequest *request){if(request->hasParam("track")){int track=request->getParam("track")->value().toInt();if(track>=1&&track<=8){currentTrack=track;player.playTrack(currentTrack);}}else player.play();lastActivityMs=millis();request->send(200,"text/plain","OK");});
    server.on("/api/stop",HTTP_GET,[](AsyncWebServerRequest *request){player.stop();currentPlaying=false;setMotor(false);lastActivityMs=millis();request->send(200,"text/plain","OK");});
    server.on("/api/pause",HTTP_GET,[](AsyncWebServerRequest *request){player.pause();currentPlaying=false;setMotor(false);lastActivityMs=millis();request->send(200,"text/plain","OK");});
    server.on("/api/volume",HTTP_GET,[](AsyncWebServerRequest *request){if(request->hasParam("value")){int value=request->getParam("value")->value().toInt();currentVolume=constrain(value,0,30);player.setVolume(currentVolume);lastActivityMs=millis();}request->send(200,"text/plain","OK");});
    server.begin();webServerStarted=true;
}

void handlePendingWiFiRestart(){if(!wifiRestartPending||(long)(millis()-wifiRestartAtMs)<0)return;wifiRestartPending=false;setMotor(false);Serial.println(F("Používateľ uložil nastavenia -> reštartujem ESP32"));Serial.flush();delay(50);ESP.restart();}

void handleWiFi(){if(!wifiActive)return;if(wifiIsSta){if(WiFi.status()!=WL_CONNECTED){Serial.println(F("WiFi STA bolo odpojené -> skúšam znovu AP"));stopWiFi();startAP();return;}lastActivityMs=millis();return;}unsigned long now=millis();uint8_t clients=WiFi.softAPgetStationNum();if(clients>0){wifiDisconnectGraceDeadlineMs=0;lastActivityMs=now;return;}if(wifiConnectDeadlineMs!=0&&(long)(now-wifiConnectDeadlineMs)<0)return;if(wifiDisconnectGraceDeadlineMs==0)wifiDisconnectGraceDeadlineMs=now+WIFI_DISCONNECT_GRACE_MS;if((long)(now-wifiDisconnectGraceDeadlineMs)>=0){stopWiFi();lastActivityMs=now;}}

bool handleButtons(){unsigned long now=millis();bool pressed=false;for(auto &b:buttons){bool state=digitalRead(b.pin);if(state!=b.lastState&&(now-b.lastChangeMs)>BTN_DEBOUNCE_MS){b.lastChangeMs=now;b.lastState=state;if(state==LOW){currentTrack=b.trackNumber;player.playTrack(b.trackNumber);startWiFi();pressed=true;}}}return pressed;}

void maybeEnterLightSleep(){if(wifiActive)return;unsigned long now=millis();if(now-lastActivityMs<INACTIVITY_TIMEOUT_MS)return;if(currentPlaying)return;setMotor(false);Serial.println(F("Nečinnosť -> light sleep (WiFi OFF)"));Serial.flush();esp_sleep_enable_gpio_wakeup();for(uint8_t pin:TRACK_BUTTON_PINS)gpio_wakeup_enable((gpio_num_t)pin,GPIO_INTR_LOW_LEVEL);esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_CHECK_INTERVAL_MS*1000ULL);esp_light_sleep_start();for(uint8_t pin:TRACK_BUTTON_PINS)gpio_wakeup_disable((gpio_num_t)pin);if(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_GPIO){Serial.println(F("Prebudenie tlačidlom"));lastActivityMs=millis();}else Serial.println(F("Periodické prebudenie (kontrola stavu)"));}

void setup(){Serial.begin(115200);delay(300);setupButtons();loadSettings();player.begin(DY_BAUD_RATE,DY_RX_PIN,DY_TX_PIN);player.setVolume(currentVolume);battery.begin();lastActivityMs=millis();Serial.println(F("DY1703A ESP32 Player – ready"));startWiFi();}

void loop(){if(handleButtons())lastActivityMs=millis();player.poll();static unsigned long lastBatteryRead=0,lastMotorPoll=0;unsigned long now=millis();if(now-lastMotorPoll>=MOTOR_POLL_INTERVAL_MS){lastMotorPoll=now;currentPlaying=player.isPlaying();setMotor(currentPlaying&&currentTrack>=1&&currentTrack<=8&&vibrationEnabled[currentTrack-1]);if(currentPlaying)lastActivityMs=now;}if(now-lastBatteryRead>=BATTERY_READ_INTERVAL_MS){lastBatteryRead=now;batteryMv=battery.readVoltageMv();batteryPct=battery.readPercent();Serial.printf("Batéria: %lu mV (~%u%%)\n",batteryMv,batteryPct);}handlePendingWiFiRestart();handleWiFi();maybeEnterLightSleep();}
