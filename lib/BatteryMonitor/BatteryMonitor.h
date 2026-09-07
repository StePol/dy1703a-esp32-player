#pragma once

#include <Arduino.h>

// Meranie napätia batérie cez SPÍNANÝ odporový delič na ADC pin ESP32.
// Delič je od batérie odpojený pomocou vysokostranového spínača
// (P-MOSFET Q1, napr. BSS84) a zapína sa len na krátky okamih počas
// merania cez N-MOSFET (Q2, napr. BS170) ovládaný z GPIO — takže mimo
// merania delič neťahá žiadny prúd z batérie (dôležité pre dlhú výdrž
// pri battery-powered zariadení).
//
// Zapojenie (pozri docs/wiring.md):
//
//   BAT+ ---[Q1 S]  Q1 (P-MOSFET, napr. BSS84)
//            [Q1 D]---[R_TOP]---+---[R_BOTTOM]--- GND
//                                |
//                              ADC pin (BATTERY_ADC_PIN)
//
//   Q1 gate ---[R_GATE_PULLUP]--- BAT+   (default OFF: gate=source, Vgs=0)
//   Q1 gate ---[Q2 D]
//              [Q2 S] --- GND
//              [Q2 G] ---[1kΩ]--- ENABLE_PIN (GPIO)
//   Q2 gate ---[R_Q2_PULLDOWN]--- GND     (default OFF)
//
// Keď ENABLE_PIN = HIGH: Q2 zopne, stiahne gate Q1 na GND -> Q1 zopne
// -> delič je pripojený na batériu a dá sa merať.
// Keď ENABLE_PIN = LOW (pokoj): Q2 vypnutý, Q1 gate ťahaný na BAT+
// cez R_GATE_PULLUP -> Q1 vypnutý -> nulový pokojový prúd deličom.
//
// Namerané napätie na ADC pine: V_adc = V_bat * R_BOTTOM / (R_TOP + R_BOTTOM)

class BatteryMonitor {
public:
    BatteryMonitor(uint8_t adcPin, float rTopOhms, float rBottomOhms, uint8_t enablePin);

    void begin();

    // Vráti napätie batérie v milivoltoch. Interne zopne spínač deliča,
    // počká na ustálenie a spínač znova vypne, aby sa šetrila energia.
    uint32_t readVoltageMv();

    // Hrubý odhad percenta nabitia (lineárna aproximácia medzi
    // minVoltageMv a maxVoltageMv) - Li-ion vybíjacia krivka nie je
    // lineárna, takže presnosť je orientačná, nie laboratórna.
    uint8_t readPercent(uint32_t minVoltageMv = 3000, uint32_t maxVoltageMv = 4200);

private:
    uint8_t _adcPin;
    uint8_t _enablePin;
    float _dividerRatio; // rBottom / (rTop + rBottom)
};
