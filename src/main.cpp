#include <Arduino.h>
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

struct TrackButton {
    uint8_t pin;
    uint8_t trackNumber; // 1-8
    bool lastState;
    unsigned long lastChangeMs;
};

TrackButton buttons[8];

uint8_t currentVolume = 20;
unsigned long lastActivityMs = 0;

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
                Serial.printf("Tlačidlo %u -> skladba %u\n", b.pin, b.trackNumber);
                pressed = true;
            }
        }
    }
    return pressed;
}

// Ak je zariadenie dosť dlho nečinné (žiadne tlačidlo, nič nehrá),
// uspí ESP32 do light sleep. Preberá sa buď na ktoromkoľvek tlačidle
// (GPIO LOW), alebo periodicky ako poistka (LIGHT_SLEEP_CHECK_INTERVAL_MS).
void maybeEnterLightSleep() {
    unsigned long now = millis();

    if (now - lastActivityMs < INACTIVITY_TIMEOUT_MS) return;

    motor.off(); // pre istotu vypnúť pred spánkom

    Serial.println(F("Nečinnosť -> light sleep"));
    Serial.flush();

    esp_sleep_enable_gpio_wakeup();
    for (uint8_t pin : TRACK_BUTTON_PINS) {
        gpio_wakeup_enable(static_cast<gpio_num_t>(pin), GPIO_INTR_LOW_LEVEL);
    }
    esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_CHECK_INTERVAL_MS * 1000ULL);

    esp_light_sleep_start();

    // --- prebudenie ---
    for (uint8_t pin : TRACK_BUTTON_PINS) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(pin));
    }

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        Serial.println(F("Prebudenie tlačidlom"));
        lastActivityMs = millis(); // reálna aktivita, ostaň hore
    } else {
        Serial.println(F("Periodické prebudenie (kontrola stavu)"));
        // lastActivityMs zámerne nemeníme - ak stále nič, ďalší loop() opäť zaspí
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

    bool playing = false;

    if (now - lastMotorPoll >= MOTOR_POLL_INTERVAL_MS) {
        lastMotorPoll = now;
        playing = player.isPlaying();
        motor.set(playing);
        if (playing) {
            lastActivityMs = now; // kým hrá skladba, nezaspávaj
        }
    }

    if (now - lastBatteryRead >= BATTERY_READ_INTERVAL_MS) {
        lastBatteryRead = now;
        uint32_t mv = battery.readVoltageMv();
        uint8_t pct = battery.readPercent();
        Serial.printf("Batéria: %lu mV (~%u%%)\n", mv, pct);
        // TODO: sprístupniť tieto hodnoty aj cez web/BLE (lib/AudioWeb, lib/AudioBLE)
    }

    maybeEnterLightSleep();

    // TODO: obsluha web serveru / BLE eventov
}
