#include "BatteryMonitor.h"

BatteryMonitor::BatteryMonitor(uint8_t adcPin, float rTopOhms, float rBottomOhms)
    : _adcPin(adcPin), _dividerRatio(rBottomOhms / (rTopOhms + rBottomOhms)) {}

void BatteryMonitor::begin() {
    analogReadResolution(12);       // 0-4095
    analogSetAttenuation(ADC_11db); // umožní meranie do cca 3.3V na pine
    pinMode(_adcPin, INPUT);
}

uint32_t BatteryMonitor::readVoltageMv() {
    // Priemer z niekoľkých vzoriek kvôli šumu ADC.
    const uint8_t samples = 8;
    uint32_t sumMv = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sumMv += analogReadMilliVolts(_adcPin); // kalibrovaná hodnota z ESP32 Arduino core
        delay(2);
    }
    uint32_t adcMv = sumMv / samples;

    return static_cast<uint32_t>(adcMv / _dividerRatio);
}

uint8_t BatteryMonitor::readPercent(uint32_t minVoltageMv, uint32_t maxVoltageMv) {
    uint32_t v = readVoltageMv();
    if (v <= minVoltageMv) return 0;
    if (v >= maxVoltageMv) return 100;

    return static_cast<uint8_t>((v - minVoltageMv) * 100UL / (maxVoltageMv - minVoltageMv));
}
