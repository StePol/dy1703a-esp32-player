#include "DY1703A.h"

// Rámec príkazu podľa oficiálneho datasheetu SEN-17-096 (docs/SEN-17-096-DataSheet.pdf):
// [0xAA] [CMD] [LEN] [DATA...] [CHECKSUM]
// Checksum = nízkych 8 bitov súčtu všetkých predchádzajúcich bajtov (start kód + dáta).

namespace {
constexpr uint8_t START_BYTE   = 0xAA;
constexpr uint8_t CMD_CHECK_PLAY_STATE = 0x01;
constexpr uint8_t CMD_PLAY     = 0x02;
constexpr uint8_t CMD_PAUSE    = 0x03;
constexpr uint8_t CMD_STOP     = 0x04;
constexpr uint8_t CMD_PREV     = 0x05; // datasheet: Previous = AA 05 00 AF
constexpr uint8_t CMD_NEXT     = 0x06; // datasheet: Next     = AA 06 00 B0
constexpr uint8_t CMD_PLAY_NUM = 0x07; // Specified Song: AA 07 02 S.N.H S.N.L SM
constexpr uint8_t CMD_VOLUME   = 0x13; // Set Volume: AA 13 01 VOL SM
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
    // Specified Song: AA 07 02 S.N.H S.N.L SM — číslo skladby je 16-bitové,
    // vysoký bajt prvý. Pri max. 8 skladbách stačí S.N.H = 0.
    uint8_t data[2] = { 0x00, trackNumber };
    sendCommand(CMD_PLAY_NUM, data, 2);
}

void DY1703A::setVolume(uint8_t level) {
    if (level > 30) level = 30;
    uint8_t data[1] = { level };
    sendCommand(CMD_VOLUME, data, 1);
}

uint8_t DY1703A::checkPlayState() {
    // Vyprázdni prípadné staré dáta v RX bufferi pred novým dotazom.
    while (_serial.available()) {
        _serial.read();
    }

    sendCommand(CMD_CHECK_PLAY_STATE);

    // Odpoveď podľa datasheetu: AA 01 01 <stav> <checksum> (5 bajtov)
    // stav: 00 = stop, 01 = play, 02 = pause
    uint8_t buf[5];
    uint8_t idx = 0;
    unsigned long start = millis();

    while (idx < 5 && (millis() - start) < 100) {
        if (_serial.available()) {
            buf[idx++] = _serial.read();
        }
    }

    if (idx == 5 && buf[0] == START_BYTE && buf[1] == CMD_CHECK_PLAY_STATE) {
        return buf[3];
    }

    return 0xFF; // chyba / timeout / žiadna odpoveď
}

bool DY1703A::isPlaying() {
    return checkPlayState() == 0x01; // 01 = play (podľa datasheetu)
}

void DY1703A::poll() {
    while (_serial.available()) {
        _serial.read(); // TODO: parsovanie stavových odpovedí modulu
    }
}
