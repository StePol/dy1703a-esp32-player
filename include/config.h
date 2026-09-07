#pragma once

// --- UART k DY1703A ---
#define DY_UART_NUM     2
#define DY_TX_PIN       17
#define DY_RX_PIN       16
#define DY_BAUD_RATE    9600

// --- Tlačidlá pre priamu voľbu skladby (aktívne LOW) ---
constexpr uint8_t TRACK_BUTTON_PINS[8] = {13, 14, 27, 26, 25, 33, 32, 35};
constexpr uint8_t INPUT_ONLY_PIN_COUNT = 1;
constexpr uint8_t INPUT_ONLY_PINS[INPUT_ONLY_PIN_COUNT] = {35};
#define BTN_DEBOUNCE_MS 40

// --- WiFi / web rozhranie ---
#define WIFI_AP_SSID    "DY1703A-Player"
#define WIFI_AP_PASS    "password"
#define WEB_SERVER_PORT 80
#define WIFI_STA_SSID   ""
#define WIFI_STA_PASS   ""
#define WIFI_STA_TIMEOUT_MS 15000UL
#define WIFI_CONNECT_TIMEOUT_MS 90000UL
#define WIFI_DISCONNECT_GRACE_MS 60000UL

// --- BLE ---
#define BLE_DEVICE_NAME "DY1703A-Player"

// --- Batéria ---
#define BATTERY_ADC_PIN     34
#define BATTERY_ENABLE_PIN  23
#define BATTERY_R_TOP       100000.0f
#define BATTERY_R_BOTTOM    100000.0f
#define BATTERY_READ_INTERVAL_MS 10000

// --- Vibračný motorček ---
// Motorček je pripojený cez pôvodné dvojtranzistorové zapojenie priamo na ESP32.
// GPIO4 ovláda tranzistorový stupeň. LOG1 motor zapína.
#define MOTOR_PIN 4
#define MOTOR_ACTIVE_LEVEL HIGH
#define MOTOR_POLL_INTERVAL_MS 300

// --- Light sleep pri nečinnosti ---
#define INACTIVITY_TIMEOUT_MS         30000UL
#define LIGHT_SLEEP_CHECK_INTERVAL_MS 5000UL
