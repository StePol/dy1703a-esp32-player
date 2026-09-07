#include <Arduino.h>
#include "config.h"
#include "DY1703A.h"

// ============================================================================
// TESTOVACÍ FIRMVÉR — overenie UART komunikácie s DY1703A
//
// Cez sériový monitor (115200 baud) zadávaj príkazy:
//   1-8   prehraj skladbu č. 1-8
//   p     pauza
//   r     pokračovať v prehrávaní
//   s     stop
//   n     ďalšia skladba
//   b     predchádzajúca skladba
//   +/-   hlasitosť hore/dole
//   ?     táto nápoveda
//
// Každú sekundu sa navyše vypíše aktuálny stav modulu (dotaz cez UART),
// aby bolo vidieť, či komunikácia funguje obojsmerne.
// ============================================================================

HardwareSerial DYSerial(DY_UART_NUM);
DY1703A player(DYSerial);

uint8_t currentVolume = 20;
unsigned long lastStatusMs = 0;

void printHelp() {
    Serial.println();
    Serial.println(F("=== DY1703A UART test ==="));
    Serial.println(F("1-8  prehrat skladbu 1-8"));
    Serial.println(F("p    pauza"));
    Serial.println(F("r    pokracovat v prehravani"));
    Serial.println(F("s    stop"));
    Serial.println(F("n    dalsia skladba"));
    Serial.println(F("b    predchadzajuca skladba"));
    Serial.println(F("+/-  hlasitost hore/dole"));
    Serial.println(F("?    tato napoveda"));
    Serial.println(F("========================="));
}

void printPlayState(uint8_t state) {
    Serial.print(F("Stav modulu: "));
    switch (state) {
        case 0:    Serial.println(F("stop")); break;
        case 1:    Serial.println(F("hra")); break;
        case 2:    Serial.println(F("pauza")); break;
        case 0xFF: Serial.println(F("bez odpovede (timeout) - skontroluj zapojenie TX/RX a CON piny")); break;
        default:   Serial.printf("neznamy kod 0x%02X\n", state); break;
    }
}

void handleSerialInput() {
    if (!Serial.available()) return;

    char c = Serial.read();

    if (c >= '1' && c <= '8') {
        uint8_t track = c - '0';
        Serial.printf("-> prehravam skladbu %u\n", track);
        player.playTrack(track);
    } else if (c == 'p') {
        Serial.println(F("-> pauza"));
        player.pause();
    } else if (c == 'r') {
        Serial.println(F("-> pokracovanie prehravania"));
        player.play();
    } else if (c == 's') {
        Serial.println(F("-> stop"));
        player.stop();
    } else if (c == 'n') {
        Serial.println(F("-> dalsia skladba"));
        player.next();
    } else if (c == 'b') {
        Serial.println(F("-> predchadzajuca skladba"));
        player.previous();
    } else if (c == '+') {
        currentVolume = (currentVolume < 30) ? currentVolume + 1 : 30;
        player.setVolume(currentVolume);
        Serial.printf("-> hlasitost %u\n", currentVolume);
    } else if (c == '-') {
        currentVolume = (currentVolume > 0) ? currentVolume - 1 : 0;
        player.setVolume(currentVolume);
        Serial.printf("-> hlasitost %u\n", currentVolume);
    } else if (c == '?') {
        printHelp();
    }
    // ostatné znaky (napr. Enter/newline) ignorujeme
}

void setup() {
    Serial.begin(115200);
    delay(300);

    player.begin(DY_BAUD_RATE, DY_RX_PIN, DY_TX_PIN);
    player.setVolume(currentVolume);

    printHelp();
}

void loop() {
    handleSerialInput();

    unsigned long now = millis();
    if (now - lastStatusMs >= 1000) {
        lastStatusMs = now;
        uint8_t state = player.checkPlayState();
        printPlayState(state);
    }
}
