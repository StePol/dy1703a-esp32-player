#include "DY1703A.h"
namespace{
constexpr uint8_t START_BYTE=0xAA,CMD_CHECK_PLAY_STATE=0x01,CMD_PLAY=0x02,CMD_PAUSE=0x03,CMD_STOP=0x04,CMD_PREV=0x05,CMD_NEXT=0x06,CMD_PLAY_NUM=0x07,CMD_VOLUME=0x13;
const char* commandName(uint8_t cmd){switch(cmd){case CMD_CHECK_PLAY_STATE:return "DOTAZ NA STAV";case CMD_PLAY:return "PLAY";case CMD_PAUSE:return "PAUZA";case CMD_STOP:return "STOP";case CMD_PREV:return "PREDCHADZAJUCA";case CMD_NEXT:return "DALSIA";case CMD_PLAY_NUM:return "PREHRAJ SKLADBU";case CMD_VOLUME:return "HLASITOST";default:return "NEZNAMY PRIKAZ";}}
void printFrame(const char* prefix,const uint8_t* frame,uint8_t len){Serial.print(prefix);for(uint8_t i=0;i<len;i++){if(i)Serial.print(' ');if(frame[i]<0x10)Serial.print('0');Serial.print(frame[i],HEX);}Serial.println();}
}
DY1703A::DY1703A(HardwareSerial& serial):_serial(serial){}
void DY1703A::begin(unsigned long baud,uint8_t rxPin,uint8_t txPin){_serial.begin(baud,SERIAL_8N1,rxPin,txPin);Serial.printf("[DY1703A] UART inicializovany: %lu baud, RX=%u, TX=%u\n",baud,rxPin,txPin);}
void DY1703A::sendCommand(uint8_t cmd,const uint8_t* data,uint8_t len){uint8_t frame[22];uint8_t idx=0;frame[idx++]=START_BYTE;frame[idx++]=cmd;frame[idx++]=len;uint16_t checksum=START_BYTE+cmd+len;for(uint8_t i=0;i<len;i++){frame[idx++]=data[i];checksum+=data[i];}frame[idx++]=static_cast<uint8_t>(checksum&0xFF);if(cmd!=CMD_CHECK_PLAY_STATE){Serial.printf("[DY1703A TX] %s | ",commandName(cmd));printFrame("HEX: ",frame,idx);}_serial.write(frame,idx);}
void DY1703A::play(){sendCommand(CMD_PLAY);}
void DY1703A::pause(){sendCommand(CMD_PAUSE);}
void DY1703A::stop(){sendCommand(CMD_STOP);}
void DY1703A::next(){sendCommand(CMD_NEXT);}
void DY1703A::previous(){sendCommand(CMD_PREV);}
void DY1703A::playTrack(uint8_t trackNumber){uint8_t data[2]={0x00,trackNumber};Serial.printf("[DY1703A] Spúšťam skladbu č. %u\n",trackNumber);sendCommand(CMD_PLAY_NUM,data,2);}
void DY1703A::setVolume(uint8_t level){if(level>30)level=30;uint8_t data[1]={level};sendCommand(CMD_VOLUME,data,1);}
uint8_t DY1703A::checkPlayState(){while(_serial.available())_serial.read();sendCommand(CMD_CHECK_PLAY_STATE);uint8_t buf[5];uint8_t idx=0;unsigned long start=millis();while(idx<5&&(millis()-start)<100){if(_serial.available())buf[idx++]=_serial.read();}if(idx==5&&buf[0]==START_BYTE&&buf[1]==CMD_CHECK_PLAY_STATE)return buf[3];return 0xFF;}
bool DY1703A::isPlaying(){return checkPlayState()==0x01;}
void DY1703A::poll(){while(_serial.available())_serial.read();}
