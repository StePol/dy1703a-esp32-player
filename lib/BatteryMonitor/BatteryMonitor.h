#pragma once

#include <Arduino.h>

// Meranie napätia batérie cez odporový delič na ADC pin ESP32.
// Delič zníži napätie batérie (max ~4.2V pri Li-ion) na bezpečný
// rozsah pre ADC (ESP32 ADC odporúčaný rozsah cca 150-2450mV pri 11dB
// atenuácii pre najlepšiu presnosť).
//
// Zapojenie (pozri docs/wiring.md):
//   BAT+ ---[R_TOP]---+---[R_BOTTOM]--- GND
//                      |
//                    ADC pin (BATTERY_ADC_PIN)
//
// Namerané napätie na ADC pine: V_adc = V_bat * R_BOTTOM / (R_TOP + R_BOTTOM)

class BatteryMonitor {
public:
    BatteryMonitor(uint8_t adcPin, float rTopOhms, float rBottomOhms);

    void begin();

    // Vráti napätie batérie v milivoltoch (po prepočte cez pomer deliča).
    uint32_t readVoltageMv();

    // Hrubý odhad percenta nabitia (lineárna aproximácia medzi
    // minVoltageMv a maxVoltageMv) - Li-ion vybíjacia krivka nie je
    // lineárna, takže presnosť je orientačná, nie laboratórna.
    uint8_t readPercent(uint32_t minVoltageMv = 3000, uint32_t maxVoltageMv = 4200);

private:
    uint8_t _adcPin;
    float _dividerRatio; // rBottom / (rTop + rBottom)
};
