#pragma once

#include <Arduino.h>

// Jednoduchý spínač pre vibračný motorček (3V DC) cez tranzistor.
// GPIO -> [1kΩ] -> báza tranzistora (NPN, napr. S8050/2N2222)
// Motor+  -> 3.3V
// Motor-  -> kolektor tranzistora
// Emitor  -> GND
// Flyback dióda (1N4148) paralelne cez motor (katóda na +3.3V stranu)
// chráni tranzistor pred napäťovými špičkami pri vypnutí.

class VibrationMotor {
public:
    explicit VibrationMotor(uint8_t controlPin);

    void begin();
    void on();
    void off();
    void set(bool state);
    bool isOn() const;

private:
    uint8_t _pin;
    bool _state;
};
