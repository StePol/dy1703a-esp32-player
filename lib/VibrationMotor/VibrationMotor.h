#pragma once
#include <Arduino.h>
class VibrationMotor{
public:
 explicit VibrationMotor(uint8_t controlPin);
 void begin();void on();void off();void set(bool state);bool isOn()const;
private:
 uint8_t _pin;bool _state;
};
