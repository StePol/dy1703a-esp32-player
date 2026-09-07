#pragma once

// --- UART k DY1703A ---
#define DY_UART_NUM     2
#define DY_TX_PIN       17   // ESP32 TX2 -> DY1703A IO1 (RXD)
#define DY_RX_PIN       16   // ESP32 RX2 <- DY1703A IO0 (TXD)
#define DY_BAUD_RATE    9600

// --- Tlačidlá pre priamu voľbu skladby (aktívne LOW) ---
// 2x 4-tlačidlová klávesnica = 8 tlačidiel, každé na vlastnom GPIO.
// Stlačenie tlačidla N spustí skladbu č. N (1-8).
// GPIO12 zámerne vynechané (boot strapping pin, riziko problémov pri štarte).
//
// GPIO35 je input-only pin (rovnako ako GPIO34) - NEMÁ interný pull-up,
// preto potrebuje EXTERNÝ pull-up rezistor 10kΩ medzi GPIO35 a 3.3V
// (pozri docs/wiring.md). Ostatné piny majú INPUT_PULLUP zapnutý interne.
constexpr uint8_t TRACK_BUTTON_PINS[8] = {13, 14, 27, 26, 25, 33, 32, 35};

// Piny bez interného pull-up rezistora (input-only: 34,35,36,39) -
// firmvér pre ne nastaví len INPUT, nie INPUT_PULLUP.
constexpr uint8_t INPUT_ONLY_PIN_COUNT = 1;
constexpr uint8_t INPUT_ONLY_PINS[INPUT_ONLY_PIN_COUNT] = {35};

#define BTN_DEBOUNCE_MS 40

// --- WiFi / web rozhranie ---
#define WIFI_AP_SSID    "DY1703A-Player"
#define WIFI_AP_PASS    "zmen-toto-heslo"
#define WEB_SERVER_PORT 80

// --- BLE ---
#define BLE_DEVICE_NAME "DY1703A-Player"

// --- Batéria (spínaný odporový delič napätia) ---
#define BATTERY_ADC_PIN     34      // ADC1, input-only pin
#define BATTERY_ENABLE_PIN  23      // ovláda Q2 (BS170) -> Q1 (BSS84) spínač
#define BATTERY_R_TOP       100000.0f  // 100kΩ
#define BATTERY_R_BOTTOM    100000.0f  // 100kΩ
#define BATTERY_READ_INTERVAL_MS 10000

// --- Vibračný motorček (cez NPN tranzistor) ---
#define MOTOR_PIN 4
#define MOTOR_POLL_INTERVAL_MS 300

// --- Light sleep pri nečinnosti ---
#define INACTIVITY_TIMEOUT_MS         30000UL  // 30s bez tlačidla/hrania -> spánok
#define LIGHT_SLEEP_CHECK_INTERVAL_MS 5000UL    // periodické budenie ako poistka
