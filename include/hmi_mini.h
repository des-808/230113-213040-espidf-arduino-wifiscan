#ifndef HMI_MINI_h
#define HMI_MINI_h
#include <Arduino.h>


void comandEnd(HardwareSerial &serial);
String intToString(int tmp,int sistema_shislenyya);

void sendString(HardwareSerial &serial,String dev, String tmp);
void sendComand(HardwareSerial &serial,String dev);
//void sendIntComand(HardwareSerial &serial,String dev,int tmp);

void sendInt(HardwareSerial &serial,String dev, int tmp);
#endif