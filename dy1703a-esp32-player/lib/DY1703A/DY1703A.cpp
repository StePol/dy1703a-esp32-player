#include "DY1703A.h"

// Rámec príkazu podľa bežného DY-SV17F/DYPlayer protokolu:
// [0xAA] [CMD] [LEN] [DATA...] [CHECKSUM]
// Checksum = súčet všetkých predchádzajúcich bajtov (mod 256).
// Over si presné kódy príkazov podľa datasheetu svojho modulu — tieto
// hodnoty vychádzajú z bežne používaného DY-SV17F setu a sú len štartovací bod.

namespace {
constexpr uint8_t START_BYTE   = 0xAA;
constexpr uint8_t CMD_PLAY     = 0x02;
constexpr uint8_t CMD_PAUSE    = 0x03;
constexpr uint8_t CMD_STOP     = 0x04;
constexpr uint8_t CMD_NEXT     = 0x05;
constexpr uint8_t CMD_PREV     = 0x06;
constexpr uint8_t CMD_PLAY_NUM = 0x07;
constexpr uint8_t CMD_VOLUME   = 0x13;
}

DY1703A::DY1703A(HardwareSerial &serial) : _serial(serial) {}

void DY1703A::begin(unsigned long baud) {
    _serial.begin(baud, SERIAL_8N1, DY_RX_PIN, DY_TX_PIN);
}

void DY1703A::sendCommand(uint8_t cmd, const uint8_t *data, uint8_t len) {
    uint8_t frame[6 + 16];
    uint8_t idx = 0;

    frame[idx++] = START_BYTE;
    frame[idx++] = cmd;
    frame[idx++] = len;

    uint16_t checksum = START_BYTE + cmd + len;
    for (uint8_t i = 0; i < len; i++) {
        frame[idx++] = data[i];
        checksum += data[i];
    }
    frame[idx++] = static_cast<uint8_t>(checksum & 0xFF);

    _serial.write(frame, idx);
}

void DY1703A::play()     { sendCommand(CMD_PLAY); }
void DY1703A::pause()    { sendCommand(CMD_PAUSE); }
void DY1703A::stop()     { sendCommand(CMD_STOP); }
void DY1703A::next()     { sendCommand(CMD_NEXT); }
void DY1703A::previous() { sendCommand(CMD_PREV); }

void DY1703A::playTrack(uint8_t trackNumber) {
    uint8_t data[1] = { trackNumber };
    sendCommand(CMD_PLAY_NUM, data, 1);
}

void DY1703A::setVolume(uint8_t level) {
    if (level > 30) level = 30;
    uint8_t data[1] = { level };
    sendCommand(CMD_VOLUME, data, 1);
}

void DY1703A::poll() {
    while (_serial.available()) {
        _serial.read(); // TODO: parsovanie stavových odpovedí modulu
    }
}
