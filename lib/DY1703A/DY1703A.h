#pragma once

#include <Arduino.h>

// Jednoduchý UART driver pre DY1703A / DY-SV17F modul.
// Modul v UART móde akceptuje jednoduché príkazové rámce cez sériovú linku.
// Presné hexadecimálne kódy príkazov si over podľa datasheetu tvojho kusu
// modulu (líšia sa mierne medzi revíziami DY-SV17F firmvéru).

class DY1703A {
public:
    explicit DY1703A(HardwareSerial &serial);

    void begin(unsigned long baud = 9600);

    void play();
    void pause();
    void stop();
    void next();
    void previous();

    // Prehrá konkrétnu skladbu podľa čísla (1..255 v Integrated móde,
    // resp. 1..8 v Independent móde — závisí od nastavenia CON pinov).
    void playTrack(uint8_t trackNumber);

    void setVolume(uint8_t level); // 0-30

    // Zavolaj pravidelne v loop() ak chceš spracovávať odpovede modulu
    // (napr. potvrdenie stavu prehrávania).
    void poll();

private:
    HardwareSerial &_serial;

    void sendCommand(uint8_t cmd, const uint8_t *data = nullptr, uint8_t len = 0);
};
