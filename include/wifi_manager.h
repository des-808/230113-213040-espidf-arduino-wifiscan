#ifndef WIFI_MANAGER_h
#define WIFI_MANAGER_h

#include <WiFi.h>
#include <EEPROM.h>

#define EEPROM_SIZE 512
#define MAX_SAVED_NETWORKS 3
#define EEPROM_WIFI_START 0
#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64

#define WIFI_MODE_STATION 0
#define WIFI_MODE_SOFTAP  1

struct SavedNetwork {
  char ssid[SSID_MAX_LEN];
  char password[PASS_MAX_LEN];
};

class WifiManager {
public:
  void begin();
  
  // Главная функция: пытается подключиться к сохранённой сети, если нет - запускает SoftAP
  void autoConnect();
  
  // Добавляет сеть в EEPROM (макс MAX_SAVED_NETWORKS)
  // Если autoConnect=true, автоматически переподключается к сети
  bool saveNetwork(const char* ssid, const char* password, bool autoConnect = false);
  
  // Принудительное сохранение в EEPROM (public для периодического сохранения)
  void saveToEEPROM();
  
  // Удаляет все сохранённые сети
  void clearSavedNetworks();
  
  // Получает количество сохранённых сетей
  int getSavedNetworkCount();
  
  // Получает сохранённую сеть по индексу
  bool getSavedNetwork(int index, char* ssid, char* password, int ssidBufSize, int passBufSize);
  
  // Текущий режим работы
  int getMode() { return currentMode; }
  
  // Получает SSID текущей точки (если SoftAP)
  const char* getSoftAPSSID() { return softapSSID.c_str(); }
  
  // Получает IP адрес
  String getIP();
  
  // Проверяет, подключен ли к STA
  bool isStationConnected();
  
  // Сканирует сети и возвращает список найденных
  int scanNetworks();
  
  // Переключение в режим Station (подключение к сохранённой WiFi сети)
  bool switchToStation();
  
  // Переключение в режим SoftAP (создание точки доступа)
  bool switchToSoftAP();

private:
  SavedNetwork savedNetworks[MAX_SAVED_NETWORKS];
  int networkCount;
  int currentMode;
  String softapSSID;
  
  void loadFromEEPROM();
  bool connectToNetwork(int index);
};

#endif
