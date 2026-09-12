#pragma once
#include <Arduino.h>
class BatteryMonitor{
public:
 BatteryMonitor(uint8_t adcPin,float rTopOhms,float rBottomOhms,uint8_t enablePin);
 void begin(); uint32_t readVoltageMv();
 uint8_t readPercent(uint32_t minVoltageMv=3000,uint32_t maxVoltageMv=4200);
private:
 uint8_t _adcPin; uint8_t _enablePin; float _dividerRatio;
};
