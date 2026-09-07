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
#include "VibrationMotor.h"

HardwareSerial DYSerial(DY_UART_NUM);
DY1703A player(DYSerial);
BatteryMonitor battery(BATTERY_ADC_PIN, BATTERY_R_TOP, BATTERY_R_BOTTOM, BATTERY_ENABLE_PIN);
VibrationMotor motor(MOTOR_PIN);
AsyncWebServer server(WEB_SERVER_PORT);
Preferences preferences;

struct TrackButton {
    uint8_t pin;
    uint8_t trackNumber;
    bool lastState;
    unsigned long lastChangeMs;
};

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

String deviceName;
String apSsid;
String staSsid;
String staPassword;

bool isInputOnlyPin(uint8_t pin) {
    for (uint8_t p : INPUT_ONLY_PINS) {
        if (p == pin) return true;
    }
    return false;
}

void setupButtons() {
    for (uint8_t i = 0; i < 8; i++) {
        buttons[i].pin = TRACK_BUTTON_PINS[i];
        buttons[i].trackNumber = i + 1;
        buttons[i].lastState = HIGH;
        buttons[i].lastChangeMs = 0;
        pinMode(buttons[i].pin, isInputOnlyPin(buttons[i].pin) ? INPUT : INPUT_PULLUP);
    }
}

void loadSettings() {
    preferences.begin("settings", false);

    deviceName = preferences.getString("device", "DY1703A Player");
    apSsid = preferences.getString("apssid", WIFI_AP_SSID);
    staSsid = preferences.getString("ssid", WIFI_STA_SSID);
    staPassword = preferences.getString("wpass", WIFI_STA_PASS);

    if (deviceName.length() == 0) deviceName = "DY1703A Player";
    if (apSsid.length() == 0) apSsid = WIFI_AP_SSID;

    Serial.printf("Nastavenia: zariadenie='%s', AP SSID='%s', STA SSID='%s'\n",
                  deviceName.c_str(), apSsid.c_str(), staSsid.c_str());
}

void saveWiFiSettings(const String &newDeviceName, const String &newApSsid,
                      const String &newStaSsid, const String &newStaPassword) {
    deviceName = newDeviceName;
    apSsid = newApSsid;
    staSsid = newStaSsid;
    staPassword = newStaPassword;

    if (deviceName.length() == 0) deviceName = "DY1703A Player";
    if (apSsid.length() == 0) apSsid = WIFI_AP_SSID;

    preferences.putString("device", deviceName);
    preferences.putString("apssid", apSsid);
    preferences.putString("ssid", staSsid);
    preferences.putString("wpass", staPassword);
}

void setupWebServer();

void startAP() {
    Serial.printf("Spúšťam WiFi AP '%s'...\n", apSsid.c_str());

    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    bool ok = WiFi.softAP(apSsid.c_str(), WIFI_AP_PASS);

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

    setupWebServer();

    Serial.printf("WiFi AP: %s, IP: %s, heslo: %s\n",
                  apSsid.c_str(), WiFi.softAPIP().toString().c_str(), WIFI_AP_PASS);
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

    // Ak máme uložené SSID, skús najprv domácu sieť.
    if (staSsid.length() > 0) {
        if (startSTA()) return;
    }

    // Bez uloženého SSID alebo pri neúspešnom STA pripojení použijeme AP.
    startAP();
}

void stopWiFi() {
    if (!wifiActive) return;

    Serial.println(F("WiFi nečinné -> vypínam WiFi a povoľujem sleep"));
    if (wifiIsSta) {
        WiFi.disconnect(true, true);
    } else {
        WiFi.softAPdisconnect(true);
    }
    WiFi.mode(WIFI_OFF);
    wifiActive = false;
    wifiIsSta = false;
    wifiConnectDeadlineMs = 0;
    wifiDisconnectGraceDeadlineMs = 0;
}

String htmlEscape(const String &value) {
    String s = value;
    s.replace("&", "&amp;");
    s.replace("<", "&lt;");
    s.replace(">", "&gt;");
    s.replace("\"", "&quot;");
    s.replace("'", "&#39;");
    return s;
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
button{font-size:19px;padding:16px 10px;border:0;border-radius:12px;background:#ddd;cursor:pointer}
button:active{transform:scale(.98)}.active{outline:4px solid #555}
.playbar{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}.playbar button{padding:14px 5px}
.controls{display:flex;gap:9px;align-items:center}.controls button{flex:0 0 58px;padding:12px}.controls input{flex:1}
.status{text-align:center;font-size:17px}.small{text-align:center;color:#666;font-size:14px;margin-top:7px}
.battery{height:18px;background:#ddd;border-radius:10px;overflow:hidden}.bar{height:100%;width:0;background:#555}
.settingsBtn{width:100%;font-size:17px;background:#eee;padding:12px;margin-top:4px}
#settings{display:none}.field{margin:11px 0}.field label{display:block;font-size:14px;font-weight:bold;margin-bottom:5px}.field input{box-sizing:border-box;width:100%;padding:11px;border:1px solid #bbb;border-radius:9px;font-size:16px}
.save{width:100%;background:#ccc;margin-top:8px}.msg{text-align:center;margin-top:10px;min-height:20px;font-size:14px}
</style></head><body>
<h1 id="title"></h1>
<div class="card status"><div id="state">Stav: --</div><div id="track">Skladba: --</div></div>
<div class="card"><h3>Skladby</h3><div class="grid" id="tracks"></div></div>
<div class="card"><h3>Ovládanie</h3><div class="playbar">
<button onclick="cmd('/api/play')">▶ Play</button><button onclick="cmd('/api/pause')">⏸ Pauza</button><button onclick="cmd('/api/stop')">■ Stop</button>
</div></div>
<div class="card"><h3>Hlasitosť</h3><div class="controls"><button onclick="volume(-1)">−</button><input id="vol" type="range" min="0" max="30" value="20" oninput="setVolume(this.value)"><button onclick="volume(1)">+</button></div><div class="small" id="volText">20</div></div>
<div class="card"><h3>Batéria</h3><div class="battery"><div class="bar" id="bar"></div></div><div class="small" id="bat">--</div></div>
<div class="card"><button class="settingsBtn" onclick="toggleSettings()">⚙ Nastavenia</button>
<div id="settings"><h3 style="margin-top:15px">Nastavenia zariadenia</h3>
<div class="field"><label>Názov zariadenia</label><input id="device" maxlength="32"></div>
<div class="field"><label>SSID zariadenia (AP)</label><input id="apssid" maxlength="32"></div>
<div class="field"><label>Domáca Wi-Fi SSID</label><input id="ssid" maxlength="64"></div>
<div class="field"><label>Domáca Wi-Fi heslo</label><input id="wpass" type="password" maxlength="64" autocomplete="new-password"></div>
<div class="small">Ak je domáca Wi-Fi vyplnená, ESP32 sa po štarte najprv pokúsi pripojiť do tejto siete. Pri neúspechu automaticky vytvorí vlastnú Wi-Fi sieť.</div>
<button class="save" onclick="saveSettings()">💾 Uložiť nastavenia</button><div class="msg" id="msg"></div>
</div></div>
<script>
const tracks=document.getElementById('tracks');for(let i=1;i<=8;i++){let b=document.createElement('button');b.textContent='Skladba '+i;b.onclick=()=>cmd('/api/play?track='+i);b.id='t'+i;tracks.appendChild(b)}
async function cmd(u){try{await fetch(u);update()}catch(e){}}
async function volume(d){let v=Number(document.getElementById('vol').value)+d;v=Math.max(0,Math.min(30,v));setVolume(v)}
async function setVolume(v){document.getElementById('vol').value=v;document.getElementById('volText').textContent=v;try{await fetch('/api/volume?value='+v);update()}catch(e){}}
function toggleSettings(){let e=document.getElementById('settings');e.style.display=e.style.display==='block'?'none':'block';if(e.style.display==='block')loadSettings()}
async function loadSettings(){try{let r=await fetch('/api/settings',{cache:'no-store'}),s=await r.json();document.getElementById('device').value=s.device;document.getElementById('apssid').value=s.apssid;document.getElementById('ssid').value=s.ssid;document.getElementById('wpass').value=''}catch(e){}}
async function saveSettings(){let p=new URLSearchParams();p.append('device',document.getElementById('device').value);p.append('apssid',document.getElementById('apssid').value);p.append('ssid',document.getElementById('ssid').value);let pw=document.getElementById('wpass').value;if(pw!=='')p.append('wpass',pw);try{let r=await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p});let t=await r.text();document.getElementById('msg').textContent=t;setTimeout(()=>location.reload(),1800)}catch(e){document.getElementById('msg').textContent='Chyba pri ukladaní'}}
async function update(){try{let r=await fetch('/api/status',{cache:'no-store'}),s=await r.json();document.getElementById('title').textContent=s.device;document.getElementById('state').textContent='Stav: '+(s.playing?'▶ PREHRÁVA SA':'■ STOP');document.getElementById('track').textContent='Skladba: '+(s.track?s.track:'—');document.getElementById('vol').value=s.volume;document.getElementById('volText').textContent=s.volume;document.getElementById('bar').style.width=s.battery+'%';document.getElementById('bat').textContent=s.battery+' % · '+s.voltage+' mV';for(let i=1;i<=8;i++)document.getElementById('t'+i).className=(s.playing&&s.track===i)?'active':''}catch(e){}}
update();setInterval(update,1000);
</script></body></html>)HTML";
        request->send(200, "text/html; charset=utf-8", html);
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"device\":\"" + htmlEscape(deviceName) +
                      "\",\"playing\":" + String(currentPlaying ? "true" : "false") +
                      ",\"track\":" + String(currentTrack) +
                      ",\"volume\":" + String(currentVolume) +
                      ",\"battery\":" + String(batteryPct) +
                      ",\"voltage\":" + String(batteryMv) + "}";
        request->send(200, "application/json", json);
    });

    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"device\":\"" + htmlEscape(deviceName) +
                      "\",\"apssid\":\"" + htmlEscape(apSsid) +
                      "\",\"ssid\":\"" + htmlEscape(staSsid) + "\"}";
        request->send(200, "application/json", json);
    });

    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
        String newDevice = request->hasParam("device", true) ? request->getParam("device", true)->value() : deviceName;
        String newApSsid = request->hasParam("apssid", true) ? request->getParam("apssid", true)->value() : apSsid;
        String newStaSsid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : staSsid;
        String newPassword = staPassword;
        if (request->hasParam("wpass", true)) newPassword = request->getParam("wpass", true)->value();

        newDevice.trim();
        newApSsid.trim();
        newStaSsid.trim();

        if (newDevice.length() == 0) newDevice = "DY1703A Player";
        if (newApSsid.length() == 0) newApSsid = WIFI_AP_SSID;

        // ESP32 SoftAP s heslom vyžaduje SSID a heslo s rozumnou dĺžkou.
        if (newApSsid.length() > 32) newApSsid = newApSsid.substring(0, 32);
        if (newDevice.length() > 32) newDevice = newDevice.substring(0, 32);

        saveWiFiSettings(newDevice, newApSsid, newStaSsid, newPassword);
        lastActivityMs = millis();
        request->send(200, "text/plain", "Nastavenia uložené. ESP32 reštartuje WiFi...");

        // Po odoslaní odpovede znovu inicializujeme WiFi podľa nových údajov.
        // Krátky delay dá HTTP odpovedi čas odísť klientovi.
        delay(250);
        stopWiFi();
        startWiFi();
    });

    server.on("/api/play", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("track")) {
            int track = request->getParam("track")->value().toInt();
            if (track >= 1 && track <= 8) {
                currentTrack = static_cast<uint8_t>(track);
                player.playTrack(currentTrack);
            }
        } else {
            player.play();
        }
        lastActivityMs = millis();
        request->send(200, "text/plain", "OK");
    });

    server.on("/api/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
        player.stop();
        currentPlaying = false;
        motor.off();
        lastActivityMs = millis();
        request->send(200, "text/plain", "OK");
    });

    server.on("/api/pause", HTTP_GET, [](AsyncWebServerRequest *request) {
        player.pause();
        currentPlaying = false;
        motor.off();
        lastActivityMs = millis();
        request->send(200, "text/plain", "OK");
    });

    server.on("/api/volume", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("value")) {
            int value = request->getParam("value")->value().toInt();
            currentVolume = static_cast<uint8_t>(constrain(value, 0, 30));
            player.setVolume(currentVolume);
            lastActivityMs = millis();
        }
        request->send(200, "text/plain", "OK");
    });

    server.begin();
    webServerStarted = true;
}

void handleWiFi() {
    if (!wifiActive) return;

    // STA režim musí zostať aktívny, aby bola stránka dostupná v lokálnej sieti.
    if (wifiIsSta) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println(F("WiFi STA bolo odpojené -> skúšam znovu AP"));
            stopWiFi();
            startAP();
            return;
        }
        lastActivityMs = millis();
        return;
    }

    unsigned long now = millis();
    uint8_t clients = WiFi.softAPgetStationNum();

    if (clients > 0) {
        wifiDisconnectGraceDeadlineMs = 0;
        lastActivityMs = now;
        return;
    }

    if (wifiConnectDeadlineMs != 0 && (long)(now - wifiConnectDeadlineMs) < 0) {
        return;
    }

    if (wifiDisconnectGraceDeadlineMs == 0) {
        wifiDisconnectGraceDeadlineMs = now + WIFI_DISCONNECT_GRACE_MS;
    }

    if ((long)(now - wifiDisconnectGraceDeadlineMs) >= 0) {
        stopWiFi();
        lastActivityMs = now;
    }
}

bool handleButtons() {
    unsigned long now = millis();
    bool pressed = false;

    for (auto &b : buttons) {
        bool state = digitalRead(b.pin);
        if (state != b.lastState && (now - b.lastChangeMs) > BTN_DEBOUNCE_MS) {
            b.lastChangeMs = now;
            b.lastState = state;

            if (state == LOW) {
                player.playTrack(b.trackNumber);
                currentTrack = b.trackNumber;
                Serial.printf("Tlačidlo %u -> skladba %u\n", b.pin, b.trackNumber);
                startWiFi();
                pressed = true;
            }
        }
    }
    return pressed;
}

void maybeEnterLightSleep() {
    if (wifiActive) return;

    unsigned long now = millis();
    if (now - lastActivityMs < INACTIVITY_TIMEOUT_MS) return;
    if (currentPlaying) return;

    motor.off();

    Serial.println(F("Nečinnosť -> light sleep (WiFi OFF)"));
    Serial.flush();

    esp_sleep_enable_gpio_wakeup();
    for (uint8_t pin : TRACK_BUTTON_PINS) {
        gpio_wakeup_enable(static_cast<gpio_num_t>(pin), GPIO_INTR_LOW_LEVEL);
    }
    esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_CHECK_INTERVAL_MS * 1000ULL);

    esp_light_sleep_start();

    for (uint8_t pin : TRACK_BUTTON_PINS) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(pin));
    }

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        Serial.println(F("Prebudenie tlačidlom"));
        lastActivityMs = millis();
    } else {
        Serial.println(F("Periodické prebudenie (kontrola stavu)"));
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);

    setupButtons();
    loadSettings();

    player.begin(DY_BAUD_RATE, DY_RX_PIN, DY_TX_PIN);
    player.setVolume(currentVolume);

    battery.begin();
    motor.begin();

    lastActivityMs = millis();

    Serial.println(F("DY1703A ESP32 Player – ready"));

    // Pri štarte skús domácu WiFi; ak nie je nastavená alebo sa nepripojí,
    // automaticky vytvor AP pre konfiguráciu a ovládanie.
    startWiFi();
}

void loop() {
    if (handleButtons()) {
        lastActivityMs = millis();
    }

    player.poll();

    static unsigned long lastBatteryRead = 0;
    static unsigned long lastMotorPoll = 0;
    unsigned long now = millis();

    if (now - lastMotorPoll >= MOTOR_POLL_INTERVAL_MS) {
        lastMotorPoll = now;
        currentPlaying = player.isPlaying();
        motor.set(currentPlaying);
        if (currentPlaying) lastActivityMs = now;
    }

    if (now - lastBatteryRead >= BATTERY_READ_INTERVAL_MS) {
        lastBatteryRead = now;
        batteryMv = battery.readVoltageMv();
        batteryPct = battery.readPercent();
        Serial.printf("Batéria: %lu mV (~%u%%)\n", batteryMv, batteryPct);
    }

    handleWiFi();
    maybeEnterLightSleep();
}
