#ifndef RF_TERMOMETR_h
#define RF_TERMOMETR_h


// размер кольцевого буфера должен быть достаточно большим, чтобы поместиться
// данные между двумя последовательными сигналами синхронизации


#define RF_PIN  27 // D27 is interrupt 1
#define BIT_FRAME_SAMPLE 36 //36 бит в посылке



bool isSync(unsigned int idx); 
void handler();
bool printSerialToRfData(unsigned int syncIndex1, unsigned int syncIndex2,int * buffer,const int count );
void rfPlotter();
#endif