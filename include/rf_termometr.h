#ifndef RF_TERMOMETR_h
#define RF_TERMOMETR_h

#include <Arduino.h>
// размер кольцевого буфера должен быть достаточно большим, чтобы поместиться
// данные между двумя последовательными сигналами синхронизации
#define RING_BUFFER_SIZE  256
#define SYNC_LENGTH  3860 // 3.86 ms+-1000
#define SEP_LENGTH   540  // 0.54 ms+-100
#define BIT1_LENGTH  2430// 2.43 ms
#define BIT0_LENGTH  1470 // 1.47 ms

const unsigned int BIT1_LENGTH0 = 540;
const unsigned int BIT1_LENGTH1 = 2430;
const unsigned int BIT0_LENGTH0 = 540;
const unsigned int BIT0_LENGTH1 = 1470;

#define RF_PIN  27 // D27 is interrupt 1
#define BIT_FRAME_SAMPLE 36 //36 бит в посылке



bool isSync(unsigned int idx); 
void handler();
bool printSerialToRfData(unsigned int syncIndex1, unsigned int syncIndex2,int * buffer,const int count );
void rfPlotter();
#endif