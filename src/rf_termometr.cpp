#include "rf_termometr.h"

unsigned long timings[RING_BUFFER_SIZE];
unsigned int syncIndex1 = 0;  // индекс первого синхросигнала
unsigned int syncIndex2 = 0;  // индекс второго синхросигнала
bool received = false;
int counts = 36;
bool is_rf_post = false;
int bufer[36];

// detect if a sync signal is present
bool isSync(unsigned int idx) {
  unsigned long t0 = timings[(idx+RING_BUFFER_SIZE-1) % RING_BUFFER_SIZE];
  unsigned long t1 = timings[idx];

  // по датчику температуры сигнал синхронизации
  // составляет примерно 9,0 мс. Учет ошибок
  // оно должно быть в пределах 8,0 мс и 10,0 мс
  if (t0>(SEP_LENGTH-100) && t0<(SEP_LENGTH+100) &&
    t1>(SYNC_LENGTH-1000) && t1<(SYNC_LENGTH+1000) &&
    digitalRead(RF_PIN) == HIGH) {
    return true;
  }
  return false;
}
// Interrupt 1 handler 
void handler() {
  static unsigned long duration = 0;
  static unsigned long lastTime = 0;
  static unsigned int ringIndex = 0;
  static unsigned int syncCount = 0;

  // игнорировать, если мы не обработали предыдущий полученный сигнал
  if (received == true) {return;}
  // расчет времени с момента последнего изменения
  long time = micros();
  duration = time - lastTime;
  lastTime = time;
  // хранить данные в кольцевом буфере
  ringIndex = (ringIndex + 1) % RING_BUFFER_SIZE;
  timings[ringIndex] = duration;
  // обнаружить синхронизирующий сигнал
  if (isSync(ringIndex)) {syncCount ++;
    // синхронизация в первый раз, запись индекса буфера
    if (syncCount == 1) {syncIndex1 = (ringIndex+1) % RING_BUFFER_SIZE;} 
    else if (syncCount == 2) {
      // во второй раз наблюдается синхронизация, начинается преобразование битов
      syncCount = 0;
      syncIndex2 = (ringIndex+1) % RING_BUFFER_SIZE;
      unsigned int changeCount = (syncIndex2 < syncIndex1) ? (syncIndex2+RING_BUFFER_SIZE - syncIndex1) : (syncIndex2 - syncIndex1);
      // changeCount должен быть 66 -- 32 бита x 2 + 2 для синхронизации
      if (changeCount != (BIT_FRAME_SAMPLE*2)+2) {
        received = false;
        syncIndex1 = 0;
        syncIndex2 = 0;
      } 
      else {received = true;}
    }
  }
}



 bool printSerialToRfData(unsigned int syncIndex1, unsigned int syncIndex2,int *buffer,const int count ){
           int cc = 0;
 for( size_t i=syncIndex1; i!=syncIndex2; i=(i+2)%RING_BUFFER_SIZE,cc++) {
      unsigned long t0 = timings[i], t1 = timings[(i+1)%RING_BUFFER_SIZE];
      if (t0>(SEP_LENGTH-100) && t0<(SEP_LENGTH+100)) {
       if (t1>(BIT1_LENGTH-1000) && t1<(BIT1_LENGTH+1000)) {
        buffer[cc] = 1;//Serial.print("1");
       } else if (t1>(BIT0_LENGTH-1000) && t1<(BIT0_LENGTH+1000)) {
         buffer[cc] = 0;//Serial.print("0");
       } else {//Serial.print(" SYNC");  // sync signal
       }
       } else {//=Serial.print("?"); 
       cc = 0;
       return false;// undefined timing
       }
  } 
  cc = 0;
  //#ifdef DEBUG_SERIAL_STRING_ARR_BUFER
    //for( int i = 0;i<counts;i++){Serial.print(bufer[i]); }Serial.println("");
  //#endif
  return true; 
} 

/* bool printSerialToRfData(unsigned int syncIndex1, unsigned int syncIndex2, int* buffer, const int count) {
    const unsigned int BIT1_LENGTH0 = 540;
    const unsigned int BIT1_LENGTH1 = 2430;
    const unsigned int BIT0_LENGTH0 = 540;
    const unsigned int BIT0_LENGTH1 = 1470;
    
    int cc = 0;
    
    for (size_t i = syncIndex1; i != syncIndex2 && cc < count; i = (i + 2) % RING_BUFFER_SIZE, cc++) {
        unsigned long t0 = timings[i];
        unsigned long t1 = timings[(i + 1) % RING_BUFFER_SIZE];
        
        if ((t0 >= (BIT1_LENGTH0 - 150) && t0 <= (BIT1_LENGTH0 + 150)) ||
            (t1 >= (BIT1_LENGTH1 - 150) && t1 <= (BIT1_LENGTH1 + 150))) {
            buffer[cc] = 1;
        } else if ((t0 >= (BIT0_LENGTH0 - 150) && t0 <= (BIT0_LENGTH0 + 150)) ||
                   (t1 >= (BIT0_LENGTH1 - 150) && t1 <= (BIT0_LENGTH1 + 150))) {
            buffer[cc] = 0;
        } else {
            buffer[cc] = 0;
        }
    }
    
    return (cc > 0);
} */

void rfPlotter() {
    /* Serial.println("\n========== RAW TIMINGS ==========");
    Serial.print("Buffer: ");
    Serial.print(RING_BUFFER_SIZE);
    Serial.print(" | Sync1: ");
    Serial.print(syncIndex1);
    Serial.print(" | Sync2: ");
    Serial.print(syncIndex2);
    Serial.print(" | Last: ");
    Serial.println(received ? "YES" : "NO");
    
    // Формат 1: Индекс:Значение
    Serial.println("\n[INDEX]  [VALUE(us)]");
    Serial.println("---------------------"); */
    
    /* int nonZero = 0;
    for (int i = 0; i < RING_BUFFER_SIZE; i++) {
        if (timings[i] != 0) {
            Serial.print("[");
            Serial.print(i, DEC);
            Serial.print("]  ");
            Serial.println(timings[i], DEC);
            nonZero++;
        }
    }
    Serial.print("Non-zero entries: ");
    Serial.println(nonZero);
    
    // Формат 2: CSV для Serial Plotter / Excel
    Serial.println("\nCSV_FORMAT");
    Serial.println("idx,value");
    for (int i = 0; i < RING_BUFFER_SIZE; i++) {
        if (timings[i] != 0) {
            Serial.print(i);
            Serial.print(",");
            Serial.println(timings[i]);
        }
    } */
    
    // Формат 3: Сырой список (только значения)
    //Serial.println("\nRAW_LIST");
    for (int i = 0; i < RING_BUFFER_SIZE; i++) {
        if (timings[i] != 0) {
            Serial.print(timings[i]);
            Serial.print(",");
        }
    }
    //Serial.println("\n================================");
}