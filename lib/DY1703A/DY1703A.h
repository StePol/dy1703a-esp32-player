#pragma once
#include <Arduino.h>
class DY1703A{
public:
 explicit DY1703A(HardwareSerial& serial);
 void begin(unsigned long baud,uint8_t rxPin,uint8_t txPin);
 void play();void pause();void stop();void next();void previous();
 void playTrack(uint8_t trackNumber);void setVolume(uint8_t level);
 uint8_t checkPlayState();bool isPlaying();void poll();
private:
 HardwareSerial& _serial; bool _traceNextStateResponse=false;
 void sendCommand(uint8_t cmd,const uint8_t* data=nullptr,uint8_t len=0);
};
