#include <Arduino.h>
#include <WiFi.h>
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

struct TrackButton {
    uint8_t pin;
    uint8_t trackNumber; // 1-8
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
unsigned long wifiConnectDeadlineMs = 0;
unsigned long wifiDisconnectGraceDeadlineMs = 0;

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

        // GPIO34/35/36/39 nemajú interný pull-up - potrebujú externý
        // rezistor (pozri docs/wiring.md), inak sa nastaví bežný INPUT_PULLUP.
        pinMode(buttons[i].pin, isInputOnlyPin(buttons[i].pin) ? INPUT : INPUT_PULLUP);
    }
}

void startWiFi() {
    if (wifiActive) return;

    Serial.println(F("Zapínam WiFi AP..."));
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false); // počas webového ovládania nechceme WiFi modem sleep
    bool ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);

    if (!ok) {
        Serial.println(F("CHYBA: WiFi AP sa nepodarilo spustiť"));
        WiFi.mode(WIFI_OFF);
        return;
    }

    wifiActive = true;
    wifiConnectDeadlineMs = millis() + WIFI_CONNECT_TIMEOUT_MS;
    wifiDisconnectGraceDeadlineMs = 0;

    Serial.printf("WiFi AP: %s, IP: %s, pripojenie do %lu s\n",
                  WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str(),
                  WIFI_CONNECT_TIMEOUT_MS / 1000UL);
}

void stopWiFi() {
    if (!wifiActive) return;

    Serial.println(F("WiFi nečinné -> vypínam WiFi a povoľujem sleep"));
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiActive = false;
    wifiConnectDeadlineMs = 0;
    wifiDisconnectGraceDeadlineMs = 0;
}

void setupWebServer() {
    if (webServerStarted) return;

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        const char html[] PROGMEM = R"HTML(
<!doctype html><html lang="sk"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>DY1703A Player</title>
<style>
body{font-family:Arial,sans-serif;max-width:520px;margin:auto;padding:16px;background:#f4f4f4;color:#222}
h1{text-align:center;font-size:25px}.card{background:white;border-radius:14px;padding:16px;margin:12px 0;box-shadow:0 2px 8px #0001}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}button{font-size:20px;padding:18px;border:0;border-radius:12px;background:#ddd}
button:active{transform:scale(.98)}.active{outline:4px solid #555}.controls{display:flex;gap:10px;align-items:center}.controls button{flex:0 0 70px}.controls input{flex:1}
.status{text-align:center;font-size:18px}.battery{height:18px;background:#ddd;border-radius:10px;overflow:hidden}.bar{height:100%;width:0;background:#555}.small{text-align:center;color:#666;font-size:14px;margin-top:8px}
</style></head><body>
<h1>DY1703A Player</h1>
<div class="card status"><div id="state">Stav: --</div><div id="track">Skladba: --</div></div>
<div class="card"><h3>Skladby</h3><div class="grid" id="tracks"></div></div>
<div class="card"><h3>Hlasitosť</h3><div class="controls"><button onclick="volume(-1)">−</button><input id="vol" type="range" min="0" max="30" value="20" oninput="setVolume(this.value)"><button onclick="volume(1)">+</button></div><div class="small" id="volText">20</div></div>
<div class="card"><h3>Prehrávanie</h3><div class="grid"><button onclick="cmd('/api/play')">▶ Play</button><button onclick="cmd('/api/pause')">⏸ Pauza</button><button onclick="cmd('/api/stop')">■ Stop</button></div></div>
<div class="card"><h3>Batéria</h3><div class="battery"><div class="bar" id="bar"></div></div><div class="small" id="bat">--</div></div>
<script>
const tracks=document.getElementById('tracks');for(let i=1;i<=8;i++){let b=document.createElement('button');b.textContent='Skladba '+i;b.onclick=()=>cmd('/api/play?track='+i);b.id='t'+i;tracks.appendChild(b)}
async function cmd(u){try{await fetch(u);update()}catch(e){}}
async function volume(d){let v=Number(document.getElementById('vol').value)+d;v=Math.max(0,Math.min(30,v));setVolume(v)}
async function setVolume(v){document.getElementById('vol').value=v;document.getElementById('volText').textContent=v;try{await fetch('/api/volume?value='+v);update()}catch(e){}}
async function update(){try{let r=await fetch('/api/status',{cache:'no-store'}),s=await r.json();document.getElementById('state').textContent='Stav: '+(s.playing?'▶ PREHRÁVA SA':'■ STOP');document.getElementById('track').textContent='Skladba: '+(s.track?s.track:'—');document.getElementById('vol').value=s.volume;document.getElementById('volText').textContent=s.volume;document.getElementById('bar').style.width=s.battery+'%';document.getElementById('bat').textContent=s.battery+' % · '+s.voltage+' mV';for(let i=1;i<=8;i++)document.getElementById('t'+i).className=(s.playing&&s.track===i)?'active':''}catch(e){}}
update();setInterval(update,1000);
</script></body></html>)HTML";
        request->send_P(200, "text/html; charset=utf-8", html);
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"playing\":" + String(currentPlaying ? "true" : "false") +
                      ",\"track\":" + String(currentTrack) +
                      ",\"volume\":" + String(currentVolume) +
                      ",\"battery\":" + String(batteryPct) +
                      ",\"voltage\":" + String(batteryMv) + "}";
        request->send(200, "application/json", json);
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

// Spracovanie WiFi/AP časovača.
// Kým je pripojený aspoň jeden mobil, zariadenie zostáva hore bez sleepu.
// Po odpojení posledného klienta má používateľ ešte WIFI_DISCONNECT_GRACE_MS.
void handleWiFi() {
    if (!wifiActive) return;

    unsigned long now = millis();
    uint8_t clients = WiFi.softAPgetStationNum();

    if (clients > 0) {
        wifiDisconnectGraceDeadlineMs = 0;
        lastActivityMs = now; // WiFi klient = aktívne zariadenie, nespi
        return;
    }

    // Nikto ešte nebol pripojený: používateľ má 90 s na pripojenie.
    if (wifiConnectDeadlineMs != 0 && (long)(now - wifiConnectDeadlineMs) < 0) {
        return;
    }

    // Po prvom timeout-e už používateľ nebol pripojený.
    // Dajme krátku grace periódu pred vypnutím, aby nedošlo k hraničnému stavu.
    if (wifiDisconnectGraceDeadlineMs == 0) {
        wifiDisconnectGraceDeadlineMs = now + WIFI_DISCONNECT_GRACE_MS;
    }

    if ((long)(now - wifiDisconnectGraceDeadlineMs) >= 0) {
        stopWiFi();
        lastActivityMs = now;
    }
}

// Vráti true, ak bolo v tomto volaní stlačené aspoň jedno tlačidlo.
bool handleButtons() {
    unsigned long now = millis();
    bool pressed = false;

    for (auto &b : buttons) {
        bool state = digitalRead(b.pin);
        if (state != b.lastState && (now - b.lastChangeMs) > BTN_DEBOUNCE_MS) {
            b.lastChangeMs = now;
            b.lastState = state;

            if (state == LOW) { // stlačené (aktívne LOW)
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

// Ak je zariadenie dosť dlho nečinné a WiFi nie je aktívna,
// uspí ESP32 do light sleep. Preberá sa na ktoromkoľvek tlačidle
// (GPIO LOW) alebo periodicky ako poistka.
void maybeEnterLightSleep() {
    if (wifiActive) return; // WiFi musí zostať hore, aby bol web spoľahlivo dostupný

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
        // WiFi sa zapne v handleButtons() po skutočnom detekovaní stlačenia.
    } else {
        Serial.println(F("Periodické prebudenie (kontrola stavu)"));
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);

    setupButtons();

    player.begin(DY_BAUD_RATE, DY_RX_PIN, DY_TX_PIN);
    player.setVolume(currentVolume);

    battery.begin();
    motor.begin();

    setupWebServer();
    WiFi.mode(WIFI_OFF);

    lastActivityMs = millis();

    Serial.println(F("DY1703A ESP32 Player – ready"));
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
        if (currentPlaying) {
            lastActivityMs = now; // kým hrá skladba, nezaspávaj
        }
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
