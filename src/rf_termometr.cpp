#include "rf_termometr.h"
#include <Arduino.h>

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
// --- ОТЛАДОЧНАЯ ПЕРЕМЕННАЯ ---
volatile int sync_detected_count = 0;
volatile int received_count = 0;

bool isSync(unsigned int idx) {
  unsigned long t0 = timings[(idx + RING_BUFFER_SIZE - 1) % RING_BUFFER_SIZE];
  unsigned long t1 = timings[idx];

  // Широкие окна захвата (учитываем помехи от WiFi на ESP32)
  // t0 ~ 520us (разделитель), t1 ~ 3880us (синхросигнал)
  // ДОБАВЛЕНО: расширены пороги для компенсации WiFi-помех
  if (t0 > 400 && t0 <700 &&
      t1 > 3500 && t1 < 4300) {
    sync_detected_count++;
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
  static bool lastPinState = LOW;
  
  // Получаем текущее время и состояние пина
  bool pinState = digitalRead(RF_PIN);
  
  // расчет времени с момента последнего изменения
  long time = micros();

  // Игнорируем импульсы короче 50 мкс (это радиопомехи/дребезг)
  if ((time - lastTime) < 100) {
    return;
  }
  duration = time - lastTime;
  lastTime = time;
  
  // Игнорируем очень короткие импульсы после успешного приёма (фильтр помех)
  if (received == true && duration < 1000) {
    return;
  }
  
  // хранить данные в кольцевом буфере
  ringIndex = (ringIndex + 1) % RING_BUFFER_SIZE;
  timings[ringIndex] = duration;
  
  // обнаружить синхронизирующий сигнал
  if (isSync(ringIndex)) {
    syncCount++;
    // синхронизация в первый раз, запись индекса буфера
    if (syncCount == 1) {
      syncIndex1 = (ringIndex+1) % RING_BUFFER_SIZE;
    } 
    else if (syncCount == 2) {
      // во второй раз наблюдается синхронизация, начинается преобразование битов
      syncIndex2 = (ringIndex+1) % RING_BUFFER_SIZE;
      unsigned int changeCount = (syncIndex2 < syncIndex1) ? (syncIndex2+RING_BUFFER_SIZE - syncIndex1) : (syncIndex2 - syncIndex1);
      // changeCount должен быть 74 -- 36 бит x 2 + 2 для синхронизации
      if (changeCount != (BIT_FRAME_SAMPLE*2)+2) {
        syncCount = 0;
        syncIndex1 = 0;
        syncIndex2 = 0;
      } 
      else {
        received = true;
        syncCount = 0;  // Сбрасываем счётчик синхросигналов для следующего приёма
      }
    }
    else if (syncCount > 2) {
      // Если синхросигналов больше 2 - сбрасываем счётчик
      syncCount = 0;
    }
  }
}



bool printSerialToRfData(unsigned int syncIndex1, unsigned int syncIndex2,int *buffer,const int count ){
       int cc = 0;
 for( size_t i=syncIndex1; i!=syncIndex2; i=(i+2)%RING_BUFFER_SIZE,cc++) {
      unsigned long t0 = timings[i], t1 = timings[(i+1)%RING_BUFFER_SIZE];
      // Расширенные окна для надёжности (учитываем помехи от WiFi)
      if (t0 > (SEP_LENGTH - 200) && t0 < (SEP_LENGTH + 200)) {
       if (t1 > (BIT1_LENGTH - 300) && t1 < (BIT1_LENGTH + 300)) {
        buffer[cc] = 1;
       } else if (t1 > (BIT0_LENGTH - 200) && t1 < (BIT0_LENGTH + 200)) {
         buffer[cc] = 0;
       }
       } else {
       cc = 0;
       return false;
       }
  } 
  cc = 0;
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

