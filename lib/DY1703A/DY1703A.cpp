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
constexpr uint8_t CMD_PREV     = 0x05;
constexpr uint8_t CMD_NEXT     = 0x06;
constexpr uint8_t CMD_PLAY_NUM = 0x07;
constexpr uint8_t CMD_VOLUME   = 0x13;

const char *commandName(uint8_t cmd) {
    switch (cmd) {
        case CMD_CHECK_PLAY_STATE: return "DOTAZ NA STAV";
        case CMD_PLAY:             return "PLAY";
        case CMD_PAUSE:            return "PAUZA";
        case CMD_STOP:             return "STOP";
        case CMD_PREV:             return "PREDCHADZAJUCA";
        case CMD_NEXT:             return "DALSIA";
        case CMD_PLAY_NUM:         return "PREHRAJ SKLADBU";
        case CMD_VOLUME:           return "HLASITOST";
        default:                   return "NEZNAMY PRIKAZ";
    }
}

void printFrame(const char *prefix, const uint8_t *frame, uint8_t len) {
    Serial.print(prefix);
    for (uint8_t i = 0; i < len; i++) {
        if (i) Serial.print(' ');
        if (frame[i] < 0x10) Serial.print('0');
        Serial.print(frame[i], HEX);
    }
    Serial.println();
}
}

DY1703A::DY1703A(HardwareSerial &serial) : _serial(serial) {}

void DY1703A::begin(unsigned long baud, uint8_t rxPin, uint8_t txPin) {
    _serial.begin(baud, SERIAL_8N1, rxPin, txPin);
    Serial.printf("[DY1703A] UART inicializovany: %lu baud, RX=%u, TX=%u\n", baud, rxPin, txPin);
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

    Serial.printf("[DY1703A TX] %s | ", commandName(cmd));
    printFrame("HEX: ", frame, idx);
    _serial.write(frame, idx);
}

void DY1703A::play() {
    _traceNextStateResponse = true;
    sendCommand(CMD_PLAY);
}

void DY1703A::pause() {
    _traceNextStateResponse = true;
    sendCommand(CMD_PAUSE);
}

void DY1703A::stop() {
    _traceNextStateResponse = true;
    sendCommand(CMD_STOP);
}

void DY1703A::next() {
    _traceNextStateResponse = true;
    sendCommand(CMD_NEXT);
}

void DY1703A::previous() {
    _traceNextStateResponse = true;
    sendCommand(CMD_PREV);
}

void DY1703A::playTrack(uint8_t trackNumber) {
    // Specified Song: AA 07 02 S.N.H S.N.L SM — číslo skladby je 16-bitové,
    // vysoký bajt prvý. Pri max. 8 skladbách stačí S.N.H = 0.
    uint8_t data[2] = { 0x00, trackNumber };
    _traceNextStateResponse = true;
    Serial.printf("[DY1703A] Spúšťam skladbu č. %u\n", trackNumber);
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
        if (_traceNextStateResponse) {
            _traceNextStateResponse = false;
            const char *stateText = "NEZNAMY STAV";
            switch (buf[3]) {
                case 0x00: stateText = "STOP - nehra"; break;
                case 0x01: stateText = "PLAY - prehrava"; break;
                case 0x02: stateText = "PAUZA"; break;
            }
            Serial.printf("[DY1703A RX] Odpoved na dotaz o stave | HEX: ");
            printFrame("", buf, 5);
            Serial.printf("[DY1703A RX] Stav: 0x%02X -> %s\n", buf[3], stateText);
        }
        return buf[3];
    }

    if (_traceNextStateResponse) {
        _traceNextStateResponse = false;
        if (idx > 0) {
            Serial.printf("[DY1703A RX] Neplatna/neuplna odpoved (%u/5 bajtov) | HEX: ", idx);
            printFrame("", buf, idx);
        } else {
            Serial.println("[DY1703A RX] Ziadna odpoved do 100 ms (timeout)");
        }
    }

    return 0xFF;
}

bool DY1703A::isPlaying() {
    return checkPlayState() == 0x01;
}

void DY1703A::poll() {
    while (_serial.available()) {
        _serial.read(); // Odpovede na stav sa spracuvaju v checkPlayState().
    }
}
