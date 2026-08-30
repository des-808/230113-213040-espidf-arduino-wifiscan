#include "wifi_manager.h"
#include <string.h>

#define EEPROM_MAGIC 0xA5
#define ENCRYPT_KEY 0x7A  // Ключ шифрования XOR

// Простое XOR шифрование/дешифрование пароля
static void encryptString(char* str, int len) {
  for (int i = 0; i < len && str[i] != '\0'; i++) {
    str[i] ^= ENCRYPT_KEY;
  }
}

static void xorStrings(char* str, int len) {
  encryptString(str, len);
}

void WifiManager::begin() {
  EEPROM.begin(EEPROM_SIZE);
  loadFromEEPROM();
  currentMode = WIFI_MODE_STATION;
  String mac = WiFi.macAddress();
  softapSSID = "ThermoAP_" + mac.substring(mac.length()-4);
  Serial.print("[WifiManager] Loaded ");
  Serial.print(networkCount);
  Serial.println(" saved networks");
}

void WifiManager::loadFromEEPROM() {
  networkCount = 0;
  
  // Проверяем магическое число
  uint8_t magic;
  EEPROM.get(EEPROM_WIFI_START, magic);
  
  if (magic != EEPROM_MAGIC) {
    Serial.println("[WifiManager] No saved networks found");
    networkCount = 0;
    return;
  }
  
  EEPROM.get(EEPROM_WIFI_START + 1, networkCount);
  
  if (networkCount < 0 || networkCount > MAX_SAVED_NETWORKS) {
    networkCount = 0;
    return;
  }
  
  // Загружаем сети
  int offset = EEPROM_WIFI_START + 2;
  for (int i = 0; i < networkCount; i++) {
    EEPROM.get(offset, savedNetworks[i]);
    // Дешифруем пароль - только реальную строку
    encryptString(savedNetworks[i].password, strlen(savedNetworks[i].password) + 1);
    // Нуль-терминация на всякий случай
    savedNetworks[i].ssid[SSID_MAX_LEN - 1] = '\0';
    savedNetworks[i].password[PASS_MAX_LEN - 1] = '\0';
    offset += sizeof(SavedNetwork);
  }
}

void WifiManager::saveToEEPROM() {
  // Записываем магическое число
  EEPROM.put(EEPROM_WIFI_START, (uint8_t)EEPROM_MAGIC);
  EEPROM.put(EEPROM_WIFI_START + 1, networkCount);
  
  // Записываем сети
  int offset = EEPROM_WIFI_START + 2;
  for (int i = 0; i < networkCount; i++) {
    EEPROM.put(offset, savedNetworks[i]);
    offset += sizeof(SavedNetwork);
  }
  
  // Принудительный commit в NVS/EEPROM
  if (EEPROM.commit()) {
    Serial.print("[WifiManager] Saved ");
    Serial.print(networkCount);
    Serial.println(" networks to EEPROM - SUCCESS");
  } else {
    Serial.println("[WifiManager] EEPROM commit FAILED!");
  }
}

bool WifiManager::saveNetwork(const char* ssid, const char* password, bool autoConnect) {
  if (!ssid || strlen(ssid) == 0) return false;
  
  // Проверяем, есть ли уже такая сеть
  for (int i = 0; i < networkCount; i++) {
    if (strcmp(savedNetworks[i].ssid, ssid) == 0) {
      // Обновляем существующую
      strncpy(savedNetworks[i].password, password ? password : "", PASS_MAX_LEN - 1);
      savedNetworks[i].password[PASS_MAX_LEN - 1] = '\0';
      // Шифруем только реальную строку (до null-терминатора)
      encryptString(savedNetworks[i].password, strlen(savedNetworks[i].password) + 1);
      saveToEEPROM();
      // Дешифруем обратно для использования в памяти
      encryptString(savedNetworks[i].password, strlen(savedNetworks[i].password) + 1);
      Serial.printf("[WifiManager] Updated network: %s\n", ssid);
      
      // Если включено autoConnect - переподключаемся (независимо от текущего режима)
      if (autoConnect) {
        Serial.printf("[WifiManager] Reconnecting to updated network: %s\n", ssid);
        return switchToStation();
      }
      return true;
    }
  }
  
  // Добавляем новую (если есть место)
  if (networkCount >= MAX_SAVED_NETWORKS) {
    Serial.println("[WifiManager] EEPROM full, removing oldest network");
    // Сдвигаем массив (удаляем первую)
    for (int i = 0; i < MAX_SAVED_NETWORKS - 1; i++) {
      savedNetworks[i] = savedNetworks[i + 1];
    }
    networkCount = MAX_SAVED_NETWORKS - 1;
  }
  
  // Добавляем новую сеть в конец
  strncpy(savedNetworks[networkCount].ssid, ssid, SSID_MAX_LEN - 1);
  savedNetworks[networkCount].ssid[SSID_MAX_LEN - 1] = '\0';
  
  // Правильно обрабатываем пароль
  if (password && strlen(password) > 0) {
    strncpy(savedNetworks[networkCount].password, password, PASS_MAX_LEN - 1);
    savedNetworks[networkCount].password[PASS_MAX_LEN - 1] = '\0';
    // Шифруем только реальную строку
    encryptString(savedNetworks[networkCount].password, strlen(savedNetworks[networkCount].password) + 1);
  } else {
    // Пустой пароль
    savedNetworks[networkCount].password[0] = '\0';
    memset(savedNetworks[networkCount].password + 1, 0, PASS_MAX_LEN - 2);
  }
  
  networkCount++;
  saveToEEPROM();
  
  // Дешифруем пароль обратно в память для использования (в памяти всегда plain text)
  encryptString(savedNetworks[networkCount - 1].password, strlen(savedNetworks[networkCount - 1].password) + 1);
  
  Serial.printf("[WifiManager] Saved network: %s\n", ssid);
  
  // Если включено autoConnect - переподключаемся к новой сети
  if (autoConnect) {
    Serial.printf("[WifiManager] Connecting to new network: %s\n", ssid);
    return switchToStation();
  }
  
  return true;
}

void WifiManager::clearSavedNetworks() {
  networkCount = 0;
  saveToEEPROM();
  Serial.println("[WifiManager] All saved networks cleared");
  
  // Автоматически переключаемся в SoftAP
  switchToSoftAP();
}

int WifiManager::getSavedNetworkCount() {
  return networkCount;
}

bool WifiManager::getSavedNetwork(int index, char* ssid, char* password, int ssidBufSize, int passBufSize) {
  if (index < 0 || index >= networkCount) return false;
  
  strncpy(ssid, savedNetworks[index].ssid, ssidBufSize - 1);
  ssid[ssidBufSize - 1] = '\0';
  
  if (password && passBufSize > 0) {
    strncpy(password, savedNetworks[index].password, passBufSize - 1);
    password[passBufSize - 1] = '\0';
  }
  
  return true;
}

bool WifiManager::connectToNetwork(int index) {
  if (index < 0 || index >= networkCount) return false;
  
  char ssid[SSID_MAX_LEN];
  char password[PASS_MAX_LEN];
  getSavedNetwork(index, ssid, password, sizeof(ssid), sizeof(password));
  
  Serial.printf("[WifiManager] Connecting to: %s\n", ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  if (strlen(password) > 0) {
    WiFi.begin(ssid, password);
  } else {
    WiFi.begin(ssid);
  }
  
  // Ожидание подключения (до 15 секунд)
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
    yield(); // Разрешаем обработать прерывания
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WifiManager] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.printf("[WifiManager] Failed to connect to: %s\n", ssid);
    return false;
  }
}

void WifiManager::autoConnect() {
  Serial.println("[WifiManager] Starting auto connect...");
  Serial.print("[WifiManager] Loaded networks: ");
  Serial.println(networkCount);
  
  if (networkCount == 0) {
    // Нет сохранённых сетей - запускаем SoftAP
    Serial.println("[WifiManager] No saved networks, starting AP mode");
    Serial.print("  AP SSID: ");
    Serial.println(softapSSID);
    
    // Включаем AP+STA — STA нужно для работы веб-сервера ESP32
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    
    bool apStarted = WiFi.softAP(softapSSID.c_str(), nullptr);
    Serial.print("  AP started: ");
    Serial.println(apStarted ? "YES" : "NO");
    
    delay(200);
    Serial.printf("  AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    currentMode = WIFI_MODE_SOFTAP;
    return;
  }
  
  // Пробуем подключиться к сохранённым сетям по порядку
  for (int i = 0; i < networkCount; i++) {
    Serial.printf("[WifiManager] Trying network %d: %s\n", i, savedNetworks[i].ssid);
    
    if (connectToNetwork(i)) {
      currentMode = WIFI_MODE_STATION;
      Serial.printf("[WifiManager] Connected to: %s\n", savedNetworks[i].ssid);
      return;  // Успешно подключились
    }
  }
  
  // Ни одна сеть не подошла - запускаем SoftAP
  Serial.println("[WifiManager] All networks failed, starting AP mode");
  
  WiFi.mode(WIFI_AP_STA);
  delay(100);
  
  bool apStarted = WiFi.softAP(softapSSID.c_str(), nullptr);
  Serial.print("  AP started: ");
  Serial.println(apStarted ? "YES" : "NO");
  
  delay(200);
  Serial.printf("  AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  
  currentMode = WIFI_MODE_SOFTAP;
}

String WifiManager::getIP() {
  if (currentMode == WIFI_MODE_STATION && WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  } else if (currentMode == WIFI_MODE_SOFTAP) {
    return WiFi.softAPIP().toString();
  }
  return "not connected";
}

bool WifiManager::isStationConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

int WifiManager::scanNetworks() {
  return WiFi.scanNetworks();
}

bool WifiManager::switchToStation() {
  Serial.println("[WifiManager] Switching to Station mode...");
  
  // Останавливаем SoftAP если активен
  WiFi.softAPdisconnect(true);
  delay(100);
  
  // Переключаемся в режим STA
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Сначала пробуем подключиться к последней сохранённой сети (самая новая)
  if (networkCount > 0) {
    int lastIdx = networkCount - 1;
    Serial.printf("[WifiManager] Priority connect to latest: %s (index %d)\n", savedNetworks[lastIdx].ssid, lastIdx);
    
    if (connectToNetwork(lastIdx)) {
      currentMode = WIFI_MODE_STATION;
      Serial.printf("[WifiManager] Connected to: %s\n", savedNetworks[lastIdx].ssid);
      return true;
    }
  }
  
  // Если последняя не подключилась - пробуем остальные
  for (int i = 0; i < networkCount - 1; i++) {
    if (i == networkCount - 1) continue; // уже пробовали
    Serial.printf("[WifiManager] Trying backup network %d: %s\n", i, savedNetworks[i].ssid);
    
    if (connectToNetwork(i)) {
      currentMode = WIFI_MODE_STATION;
      Serial.printf("[WifiManager] Connected to backup: %s\n", savedNetworks[i].ssid);
      return true;
    }
  }
  
  // Ни одна сеть не подошла - автоматически переключаемся в SoftAP
  Serial.println("[WifiManager] All networks failed, auto-switching to SoftAP...");
  return switchToSoftAP();
}

bool WifiManager::switchToSoftAP() {
  Serial.println("[WifiManager] Switching to SoftAP mode...");
  
  // Отключаемся от STA если подключены
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    delay(100);
  }
  
  // Переключаемся в режим AP+STA
  WiFi.mode(WIFI_AP_STA);
  delay(100);
  
  // Запускаем точку доступа
  bool apStarted = WiFi.softAP(softapSSID.c_str(), nullptr);
  if (!apStarted) {
    Serial.println("[WifiManager] Failed to start SoftAP");
    return false;
  }
  
  delay(200);
  Serial.printf("[WifiManager] SoftAP started! IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("[WifiManager] AP SSID: %s\n", softapSSID.c_str());
  
  currentMode = WIFI_MODE_SOFTAP;
  return true;
}
