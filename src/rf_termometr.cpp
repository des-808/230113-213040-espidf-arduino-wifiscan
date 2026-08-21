#include "rf_termometr.h"
#include <Arduino.h>

#define RING_BUFFER_SIZE  256
unsigned long timings[RING_BUFFER_SIZE];
unsigned int syncIndex1 = 0;  // индекс первого синхросигнала
unsigned int syncIndex2 = 0;  // индекс второго синхросигнала
bool received = false;
int counts = 36;
bool is_rf_post = false;
int bufer[36];


#define SYNC_LENGTH  3880 // 3.86 ms+-1000
#define SEP_LENGTH   520  // 0.54 ms+-100// Разделитель между битами
#define BIT1_LENGTH  1920// 2.43 ms
#define BIT0_LENGTH  945 // 1.47 ms

/*#define  BIT1_LENGTH0  520
 #define  BIT1_LENGTH1  2430 
#define  BIT0_LENGTH0  540
#define  BIT0_LENGTH1  1470*/
//const unsigned int SYNC_LENGTH  9000    // Синхросигнал (~9 мс)

// Окна захвата для Manchester-кода (протокол OS v2.1)
// Значения подобраны согласно стр. 16-18 PDF + запас на температурный дрейф
/* #define  BIT_SHORT_MIN  350 
#define  BIT_SHORT_MAX  750 // Охватывает 540 +/- 150-200

#define  BIT1_LONG_MIN  2100
#define  BIT1_LONG_MAX  2600 // Охватывает 2430

#define  BIT0_LONG_MIN  1250
#define  BIT0_LONG_MAX  1750 // Охватывает 1470 */




// detect if a sync signal is present
/* bool isSync(unsigned int idx) {
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
} */

// --- ОБНОВЛЕННАЯ ФУНКЦИЯ СИНХРОНИЗАЦИИ ---
// Убрана проверка digitalRead(), оставлены только длительности
bool isSync(unsigned int idx) {
  unsigned long t0 = timings[(idx + RING_BUFFER_SIZE - 1) % RING_BUFFER_SIZE]; // Предыдущий интервал (должен быть коротким ~500мкс)
  unsigned long t1 = timings[idx];                                             // Текущий интервал (должен быть длинным ~9000мкс)

  if (t0 > (SEP_LENGTH - 100) && t0 < (SEP_LENGTH + 100) &&
      t1 > (SYNC_LENGTH - 400) && t1 < (SYNC_LENGTH + 400)) {
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

  // Игнорируем импульсы короче 50 мкс (это радиопомехи/дребезг)
  if ((time - lastTime) < 50) {
    return;
  }
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
       if (t1>(BIT1_LENGTH-100) && t1<(BIT1_LENGTH+100)) {
        buffer[cc] = 1;//Serial.print("1");
       } else if (t1>(BIT0_LENGTH-100) && t1<(BIT0_LENGTH+100)) {
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
/*
// --- РАСКОММЕНТИРОВАННАЯ И ДОРАБОТАННАЯ ФУНКЦИЯ ДЕКОДИРОВАНИЯ ---
 bool printSerialToRfData(unsigned int syncIndex1, unsigned int syncIndex2, int* buffer, const int count) {
    
    int cc = 0;
    
    for (size_t i = syncIndex1; i != syncIndex2 && cc < count; i = (i + 2) % RING_BUFFER_SIZE, cc++) {
        unsigned long t0 = timings[i];
        unsigned long t1 = timings[(i + 1) % RING_BUFFER_SIZE];
        
        // Логика OR (||) здесь критически важна. 
        // Из-за работы АРУ приемника может укоротиться как пауза (t0), так и сам импульс (t1).
        // Если хотя бы один из них попал в окно бита "1" — это единица.
        if ((t0 >= BIT1_LONG_MIN && t0 <= BIT1_LONG_MAX) ||
            (t1 >= BIT1_LONG_MIN && t1 <= BIT1_LONG_MAX)) {
            buffer[cc] = 1;
        } 
        // Аналогично для нуля
        else if ((t0 >= BIT0_LONG_MIN && t0 <= BIT0_LONG_MAX) ||
                 (t1 >= BIT0_LONG_MIN && t1 <= BIT0_LONG_MAX)) {
            buffer[cc] = 0;
        } 
        // Если ни одно условие не подошло — помечаем ошибку или ставим 0
        else {
            buffer[cc] = 0; 
            // Для отладки можно вернуть false сразу при первой ошибке timing-а:
            // return false; 
        }
    }
    
    return (cc > 0);
} */

/* bool printSerialToRfData(unsigned int syncIndex1, unsigned int syncIndex2, int *buffer, const int count) {
    int cc = 0;
    
    // Для Oregon v2.1 мы анализируем ПАРЫ импульсов (короткий + длинный)
    for (size_t i = syncIndex1; i != syncIndex2 && cc < count; ) {
        // Берем пару значений из кольцевого буфера
        unsigned long t_short = timings[i];
        unsigned long t_long = timings[(i + 1) % RING_BUFFER_SIZE];
        
        // Сдвигаем индекс сразу на 2 позиции для следующей пары
        i = (i + 2) % RING_BUFFER_SIZE;

        // ЗАЩИТА ОТ ШУМА: 
        // Короткий промежуток (t0) должен быть строго около 500мкс. 
        // Если радио за стеной дает нам тут мусор (как ваши 124, 347, 74... до индекса 31),
        // мы просто пропускаем эти точки, пока не найдем стабильную структуру Manchester.
        if (t_short < 350 || t_short > 700) {
            continue; // Ждем следующую пару фронтов
        }

        // ДЕКОДИРОВАНИЕ БИТА ПО МАНЧЕСТЕРУ:
        // В протоколе v2.1 бит состоит из двух полупериодов.
        // Логика: если второй импульс значительно длиннее первого -> Бит 1
        //         если они примерно равны (оба короткие)       -> Бит 0
        
        // Эталонные значения из вашего RAW-лога:
        // Bit 1: ~500 us (LOW) + ~1940 us (HIGH)
        // Bit 0: ~500 us (LOW) + ~960 us (HIGH)
        
        float ratio = (float)t_long / (float)t_short;

        if (ratio > 3.0) {          // 1940 / 500 = ~3.8
            buffer[cc] = 1;
        } else if (ratio > 1.5) {   // 960 / 500 = ~1.9
            buffer[cc] = 0;
        } else {
            // Если оба импульса длинные или оба аномально короткие - ошибка структуры
            return false; 
        }
        
        cc++;
    }

    // Проверка, что мы набрали нужное количество информационных бит
    if (cc < count) return false;

    return true;
} */
void rfPlotter() {
     Serial.println("\n========== RAW TIMINGS ==========");
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
    Serial.println("---------------------"); 
    
     int nonZero = 0;
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
    } 
    
    // Формат 3: Сырой список (только значения)
    Serial.println("\nRAW_LIST");
    for (int i = 0; i < RING_BUFFER_SIZE; i++) {
        if (timings[i] != 0) {
            Serial.print(timings[i]);
            Serial.print(",");
        }
    }
    Serial.println("\n================================");
}