#ifndef WEB_SERVER_h
#define WEB_SERVER_h

#include <WebServer.h>
#include "wifi_manager.h"

// Callback для получения строки времени (из NTP)
typedef const char* (*GetTimeStringFunc)();

// Глобальные функции для веб-сервера
bool webServerStart(WifiManager* wm);
void webServerSetTimeCallback(GetTimeStringFunc getTimeString);
void webServerStop();
void webServerHandleClient();
bool webServerIsStarted();

#endif
