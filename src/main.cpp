#include <Arduino.h>
#include "config.h"
#include "DY1703A.h"
#include "BatteryMonitor.h"

HardwareSerial DYSerial(DY_UART_NUM);
DY1703A player(DYSerial);
BatteryMonitor battery(BATTERY_ADC_PIN, BATTERY_R_TOP, BATTERY_R_BOTTOM);

struct TrackButton {
    uint8_t pin;
    uint8_t trackNumber; // 1-8
    bool lastState;
    unsigned long lastChangeMs;
};

TrackButton buttons[8];

uint8_t currentVolume = 20;

void setupButtons() {
    for (uint8_t i = 0; i < 8; i++) {
        buttons[i].pin = TRACK_BUTTON_PINS[i];
        buttons[i].trackNumber = i + 1;
        buttons[i].lastState = HIGH;
        buttons[i].lastChangeMs = 0;
        pinMode(buttons[i].pin, INPUT_PULLUP);
    }
}

void handleButtons() {
    unsigned long now = millis();

    for (auto &b : buttons) {
        bool state = digitalRead(b.pin);
        if (state != b.lastState && (now - b.lastChangeMs) > BTN_DEBOUNCE_MS) {
            b.lastChangeMs = now;
            b.lastState = state;

            if (state == LOW) { // stlačené (aktívne LOW)
                player.playTrack(b.trackNumber);
                Serial.printf("Tlačidlo %u -> skladba %u\n", b.pin, b.trackNumber);
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    setupButtons();

    player.begin(DY_BAUD_RATE);
    player.setVolume(currentVolume);

    battery.begin();

    // TODO: init WiFi/web servera (lib/AudioWeb) a BLE (lib/AudioBLE)
    // podľa toho, ktoré rozhranie chceš mať aktívne súčasne s tlačidlami.

    Serial.println("DY1703A ESP32 Player – ready");
}

void loop() {
    handleButtons();
    player.poll();

    static unsigned long lastBatteryRead = 0;
    unsigned long now = millis();

    if (now - lastBatteryRead >= BATTERY_READ_INTERVAL_MS) {
        lastBatteryRead = now;
        uint32_t mv = battery.readVoltageMv();
        uint8_t pct = battery.readPercent();
        Serial.printf("Batéria: %lu mV (~%u%%)\n", mv, pct);
        // TODO: sprístupniť tieto hodnoty aj cez web/BLE (lib/AudioWeb, lib/AudioBLE)
    }

    // TODO: obsluha web serveru / BLE eventov
}
