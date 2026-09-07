#include "VibrationMotor.h"

VibrationMotor::VibrationMotor(uint8_t controlPin) : _pin(controlPin), _state(false) {}

void VibrationMotor::begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
}

void VibrationMotor::on() {
    digitalWrite(_pin, HIGH);
    _state = true;
}

void VibrationMotor::off() {
    digitalWrite(_pin, LOW);
    _state = false;
}

void VibrationMotor::set(bool state) {
    state ? on() : off();
}

bool VibrationMotor::isOn() const {
    return _state;
}
