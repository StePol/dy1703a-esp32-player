#include "BatteryMonitor.h"

namespace {
constexpr uint8_t SAMPLES = 8;
constexpr uint16_t SWITCH_SETTLE_MS = 5; // čas na ustálenie po zopnutí Q1/Q2
}

BatteryMonitor::BatteryMonitor(uint8_t adcPin, float rTopOhms, float rBottomOhms, uint8_t enablePin)
    : _adcPin(adcPin), _enablePin(enablePin),
      _dividerRatio(rBottomOhms / (rTopOhms + rBottomOhms)) {}

void BatteryMonitor::begin() {
    analogReadResolution(12);       // 0-4095
    analogSetAttenuation(ADC_11db); // umožní meranie do cca 3.3V na pine
    pinMode(_adcPin, INPUT);

    pinMode(_enablePin, OUTPUT);
    digitalWrite(_enablePin, LOW); // delič defaultne odpojený (šetrí energiu)
}

uint32_t BatteryMonitor::readVoltageMv() {
    digitalWrite(_enablePin, HIGH); // zopne Q2 -> Q1 -> pripojí delič
    delay(SWITCH_SETTLE_MS);

    uint32_t sumMv = 0;
    for (uint8_t i = 0; i < SAMPLES; i++) {
        sumMv += analogReadMilliVolts(_adcPin); // kalibrovaná hodnota z ESP32 Arduino core
        delay(2);
    }

    digitalWrite(_enablePin, LOW); // hneď vypni, aby delič opäť neťahal prúd

    uint32_t adcMv = sumMv / SAMPLES;
    return static_cast<uint32_t>(adcMv / _dividerRatio);
}

uint8_t BatteryMonitor::readPercent(uint32_t minVoltageMv, uint32_t maxVoltageMv) {
    uint32_t v = readVoltageMv();
    if (v <= minVoltageMv) return 0;
    if (v >= maxVoltageMv) return 100;

    return static_cast<uint8_t>((v - minVoltageMv) * 100UL / (maxVoltageMv - minVoltageMv));
}
