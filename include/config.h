#pragma once

// --- UART k DY1703A ---
#define DY_UART_NUM     2
#define DY_TX_PIN       17   // ESP32 TX2 -> DY1703A IO1 (RXD)
#define DY_RX_PIN       16   // ESP32 RX2 <- DY1703A IO0 (TXD)
#define DY_BAUD_RATE    9600

// --- Tlačidlá pre priamu voľbu skladby (aktívne LOW, INPUT_PULLUP) ---
// 2x 4-tlačidlová klávesnica = 8 tlačidiel, každé na vlastnom GPIO.
// Stlačenie tlačidla N spustí skladbu č. N (1-8).
// GPIO12 zámerne vynechané (boot strapping pin, riziko problémov pri štarte).
constexpr uint8_t TRACK_BUTTON_PINS[8] = {13, 14, 27, 26, 25, 33, 32, 15};
#define BTN_DEBOUNCE_MS 40

// --- WiFi / web rozhranie ---
#define WIFI_AP_SSID    "DY1703A-Player"
#define WIFI_AP_PASS    "zmen-toto-heslo"
#define WEB_SERVER_PORT 80

// --- BLE ---
#define BLE_DEVICE_NAME "DY1703A-Player"

// --- Batéria (odporový delič napätia) ---
#define BATTERY_ADC_PIN   34      // ADC1, input-only pin, vhodný na meranie
#define BATTERY_R_TOP     100000.0f  // 100kΩ, BAT+ -> ADC pin
#define BATTERY_R_BOTTOM  100000.0f  // 100kΩ, ADC pin -> GND
#define BATTERY_READ_INTERVAL_MS 10000
