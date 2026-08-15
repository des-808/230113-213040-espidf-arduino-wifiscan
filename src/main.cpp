#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"
#include "driver/uart.h"
#include "sdkconfig.h"
#include "arduino.h"
#include "ntp.h"
#include "esp_timer.h"
#include <Ticker.h> 
//#include <WiFi.h>
////////////////////
//#include "esp_netif_sntp.h"
//#include "lwip/ip_addr.h" 
//#include "esp_sntp.h"
////////////////////
#include "EEPROM.h"
#include <stdint.h>
#include "WiFiScan.h"
//#define DEBUG
//#define DEBUG_hdc1080
//#define DEBUG_SERIAL_STRING_ARR_BUFER
#define DEBUG_STRING_ARR_BUFER
#include "hmi_mini.h" 
#include "rf_termometr.h"
#include <nahs-Bricks-Lib-HDC1080.h>

HDC1080_SerialNumber sn;
Ticker periodicTicker;
Ticker onceTicker;
#define EEPROM_SIZE 512
#define Serial_hmi Serial2
#define STACK_SIZE 2048
#define DELAY_1second 1000
#define DELAY_1minutes 60000
#define DELAY_1hour 360000
#define DELAY_24hours 8640000
NTP ntp(3);
String incStr;
void wifiScan();
void wifi_auto_connect();
void ntp_status();
void upload_clock_hmi();
void hdc1080_read_to_send_serial();
void hdc1080_read_to_send_HMI();
void send_termo_out_to_hmi(String byte_arr,String batery,int ch,int temp,int humiditu);
void restart_attachInterrupt();

//extern unsigned long timings[RING_BUFFER_SIZE];
extern unsigned int syncIndex1;  // индекс первого синхросигнала
extern unsigned int syncIndex2;  // индекс второго синхросигнала
extern bool received;
extern int counts;
extern bool is_rf_post;
extern int bufer[];

bool ssid_ok=false;
bool password_ok=false;
bool boolean_password=false;
bool boolean_ssid=false;
uint8_t sBufSerial_hmi[64];
uint8_t sBufSerial[64];
int bytesArraySerial_hmi=0;
int count = 0;
bool read_buf_serial_hmi_bool =false;
bool boolean_xz = false;
bool xx = false;
//u_int8_t pinR = 8;
String ssid;     
String password;
char buff[64];
int intssid; 

int humidity = 0;
int ch = 0;
int temp = 0;
int temp0 = 0;
int temp1 = 0;
int bat = 0;
int datchik = 0;
String batery = "";
String message = "";
String a_battery[3] = {"0%","50%","100%"};
String byte_arr = "";

struct myStructAutoWiFi {
  char ssid[16];
  char pass[16];
  byte mode;
} tmpStruct;

myStructAutoWiFi wifi_struct;

void ntp_status_serial(HardwareSerial Serial){
int stat = ntp.status();
   switch(stat){     
       case 0:  Serial.println("всё ок");                               break;
       case 1:  Serial.println("не запущен UDP");                       break;
       case 2:  Serial.println("не подключен WiFi");                    break;
       case 3:  Serial.println("ошибка подключения к серверу");         break;
       case 4:  Serial.println("ошибка отправки пакета");               break;
       case 5:  Serial.println("таймаут ответа сервера");               break;
       case 6:  Serial.println("получен некорректный ответ сервера");   break;
       default:break;
    }
}
int ntp_status_int(){
int stat = ntp.status();
   switch(stat){
       case 0:  sendString(Serial2,"t15.txt" ,"всё ок");                               break;
       case 1:  sendString(Serial2,"t15.txt" ,"не запущен UDP");                       break;
       case 2:  sendString(Serial2,"t15.txt" ,"не подключен WiFi");                    break;
       case 3:  sendString(Serial2,"t15.txt" ,"ошибка подключения к серверу");         break;
       case 4:  sendString(Serial2,"t15.txt" ,"ошибка отправки пакета");               break;
       case 5:  sendString(Serial2,"t15.txt" ,"таймаут ответа сервера");               break;
       case 6:  sendString(Serial2,"t15.txt" ,"получен некорректный ответ сервера");   break;
       default:break;
    }
    return stat;
}

void wifiScan() {
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    //sendString_2("t15","scan done");
    if (n == 0) {
        sendString(Serial2,"t15","no networks found");
    } else {
        sendString(Serial2,"t15.txt","netw found "+intToString(n,10));
        for (int i = 0; i < 14; ++i) {//i<n
            String ff = intToString(i,10);//10 = десятиричная система счисления;
            String xz = "t"+ String(ff)+".txt";
                if(i<n){
                    // Print SSID and RSSI for each network found
                    String tmp=(WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*";
                    sendString(Serial2,xz, WiFi.SSID(i)+" ("+WiFi.RSSI(i)+")"+tmp);
                    delay(10);
                }
                else{
                    sendString(Serial2,xz, "                              ");delay(10);
                }
        }
    }
}
//void UART_RX_IRQ(){Serial.print("irq");}
void wifi_auto_connect(){

}
void serialEventRun () {
  if (Serial2.available ()) {
    // получаем новый байт:
    byte inByte = (byte) Serial2.read ();
    sBufSerial_hmi[bytesArraySerial_hmi] = inByte;
     bytesArraySerial_hmi++;     
    if(inByte==0xff){count++;
        if(count==3){count=0;
        read_buf_serial_hmi_bool = true;
        }
    }
  }
}  

void read_buf_serial_hmi(){
    #ifdef DEBUG
    for(int i = 0;i<bytesArraySerial_hmi;i++){Serial.write(sBufSerial_hmi[i]);}
    #endif
    if(boolean_password){for(int i =1,j=0;i<bytesArraySerial_hmi-3;i++,j++){buff[j]= sBufSerial_hmi[i];}password = String(buff);boolean_password = false;password_ok=true;}
    if(boolean_ssid){ssid = WiFi.SSID(sBufSerial_hmi[1]);boolean_ssid = false;ssid_ok=true;intssid=sBufSerial_hmi[1];}
    if((sBufSerial_hmi[1]==0x00)&&(sBufSerial_hmi[2]==0x01)&&(sBufSerial_hmi[3]==0x01)){sendString(Serial2,"t0.txt","Idite naxuy.. ya vas ne znayu!!");}
    else if((sBufSerial_hmi[1]==0x02)&&(sBufSerial_hmi[2]==0x12)&&(sBufSerial_hmi[3]==0x01)){wifiScan();sendInt(Serial2,"va1.val",0);sendInt(Serial2,"z0.val",0);}//skan
    else if((sBufSerial_hmi[1]==0x02)&&(sBufSerial_hmi[2]==0x13)&&(sBufSerial_hmi[3]==0x01)){boolean_xz = true;}//autoscan z0.val
    else if((sBufSerial_hmi[1]==0x02)&&(sBufSerial_hmi[2]==0x01)&&(sBufSerial_hmi[3]==0x00)){boolean_xz = false;}//exit
    else if((sBufSerial_hmi[1]==0x04)&&(sBufSerial_hmi[2]==0x06)&&(sBufSerial_hmi[3]==0x01)){boolean_password = true;}//exit
    else if((sBufSerial_hmi[1]==0x04)&&(sBufSerial_hmi[2]==0x06)&&(sBufSerial_hmi[3]==0x00)){boolean_ssid = true;}//
    else if((sBufSerial_hmi[1]==0x01)&&(sBufSerial_hmi[2]==0x0f)&&(sBufSerial_hmi[3]==0x01)){upload_clock_hmi();}///printh 65 01 0F 01 FF FF FF
    else if((sBufSerial_hmi[1]==0x00)&&(sBufSerial_hmi[2]==0x00)&&(sBufSerial_hmi[3]==0x01)){send_termo_out_to_hmi(byte_arr,batery, ch, temp, humidity);}///printh 65 00 00 01 FF FF FF 
    //else if((sBufSerial_hmi[1]==0x00)&&(sBufSerial_hmi[2]==0x24)&&(sBufSerial_hmi[3]==0x01)){hdc1080_read_to_send_HMI();}///printh 65 00 24 01 FF FF FF 
    else if((sBufSerial_hmi[1]==0x05)&&(sBufSerial_hmi[2]==0x0C)&&(sBufSerial_hmi[3]==0x00)){WiFi.disconnect();sendComand(Serial2,"page page0");sendString(Serial2,"wifiConnect.txt", "wifi not connected");sendInt(Serial2,"va10.val",0);sendInt(Serial2,"tm2.en",1);}//exit
    else if((sBufSerial_hmi[1]==0x05)&&(sBufSerial_hmi[2]==0x00)&&(sBufSerial_hmi[3]==0x01)){
       //char localip = WiFi.localIP();
       sendString(Serial2,"t1.txt", WiFi.SSID());
       sendString(Serial2,"t3.txt", String(WiFi.localIP().toString().c_str()));
       //sendInt(Serial2,"n0",localip);
       sendString(Serial2,"t5.txt", WiFi.macAddress());
       sendString(Serial2,"t7.txt", "zjalupa");
       sendString(Serial2,"t9.txt", String(WiFi.channel()));
       sendString(Serial2,"t11.txt", String(WiFi.RSSI(intssid)));
       
       //Serial.println(localip);
        //Serial.println(WiFi.localIP().toString().c_str());
        //Serial.println(WiFi.channel());
        //Serial.println(WiFi.SSID());
        //Serial.println(WiFi.dnsIP());
        //Serial.println(WiFi.encryptionType(intssid));
        //Serial.println(WiFi.macAddress());

    }//wifi info
    //else if((sBufSerial2[1]==0x02)&&(sBufSerial2[2]==0x02)&&(sBufSerial2[3]==0x01)){sendComand_2("page page0");sendString_2("t3.txt", WiFi.SSID(0));}//  02 02 01  /0
    bytesArraySerial_hmi = 0;
    for(int i = 0;i<64;i++)sBufSerial_hmi[i]=0xFF;
    for(int i = 0;i<64;i++)buff[i]=0x00;
    }

void vTaskNTPsunc( void * pvParameters )
{
    while(true){
        if(WiFi.status() == WL_CONNECTED) { 
            ntp.updateNow();   
            //Serial.println();
            ntp_status();
            #ifdef DEBUG
            //Serial.print("синхронизация времени ");Serial.print(" ");Serial.println(String(ntp.ms())+"ms");
            Serial.println(ntp.timeString());
            Serial.println(ntp.dateString());
            Serial.println();
            #endif
        }
        vTaskDelay(DELAY_24hours);
    }
}

void upload_clock_hmi(){
 ntp.updateNow(); 
 ntp_status_serial(Serial);
 String time = ntp.timeString();
 String date = ntp.dateString();
 int hour = time.substring(0,2).toInt() ;
 int minutes = time.substring(3,5).toInt() ;
 int seconds = time.substring(6,8).toInt() ;
sendInt(Serial_hmi,"rtc3",hour);
sendInt(Serial_hmi,"rtc4",minutes);
sendInt(Serial_hmi,"rtc5",seconds);
}
void hdc1080_read_to_send_serial(){
#ifdef DEBUG_hdc1080
  Serial.println(HDC1080.isConnected());
  Serial.println(HDC1080.snToString(sn));
  Serial.println(HDC1080.getT());
  Serial.println(HDC1080.getH());
  HDC1080.triggerRead();
#endif
}
void hdc1080_read_to_send_HMI(){
    if(HDC1080.isConnected()==true){
        //Serial.println(HDC1080.isConnected());
        //Serial.println(HDC1080.snToString(sn));
        sendInt(Serial2,"x0.val",HDC1080.getT()*10);
        sendInt(Serial2,"n3.val",HDC1080.getH());
        HDC1080.triggerRead();
    }else{
        sendInt(Serial2,"x0.val",-99);
        sendInt(Serial2,"n3.val",9);
    }
}

template <class T>T getFirst(T *tmpStruct)// возвращает пкрвый элемент структуры
{
    return tmpStruct->ssid;
}
template <class T>T getSecond(T *tmpStruct)// возвращает второй элемент структуры
{
    return tmpStruct->pass;
}
void eeprom_write(){}
void eeprom_read(){ }

void send_termo_out_to_hmi(String byte_arr,String batery,int ch,int temp,int humiditu){
        sendString(Serial2,"t14.txt",byte_arr);
        sendString(Serial2,"t15.txt",batery);
        sendInt(Serial2,"n6.val",ch+1);
        sendInt(Serial2,"x1.val",temp);
        sendInt(Serial2,"n4.val", humiditu);
}

void setup() {
    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.begin(115200);
    Serial2.begin(115200);
    pinMode(RF_PIN, INPUT);
    attachInterrupt(RF_PIN, handler, CHANGE);

    //ntp.begin();
    //ntp.setPeriod(60);
    //static uint8_t ucParameterToPass;
    //TaskHandle_t xHandle = NULL;
    //xTaskCreatePinnedToCore( vTaskNTPsunc, "NTPSunhronize", STACK_SIZE, &ucParameterToPass, tskIDLE_PRIORITY, &xHandle, 0 );
    //xTaskCreatePinnedToCore( vTaskNTPsunc, "NTPSunhronize", STACK_SIZE, &ucParameterToPass, 5, &xHandle, 0 );
    //configASSERT( xHandle );
    Wire.begin();
    delay(15);
    Serial.println(HDC1080.begin());
    HDC1080.getSN(sn);
    HDC1080.triggerRead();
    periodicTicker.attach_ms(5000, hdc1080_read_to_send_HMI);
    EEPROM.begin(EEPROM_SIZE);
  
    sendInt(Serial2,"va10.val",0);
    sendString(Serial2,"wifiConnect.txt", "wifi not connected");sendInt(Serial2,"va10.val",0);sendInt(Serial2,"tm2.en",1);
}


void loop() {
//char xz[] = getFirst( wifi_struct);
    if(boolean_xz==true){boolean_xz=false; wifiScan();}
    if(read_buf_serial_hmi_bool){read_buf_serial_hmi();read_buf_serial_hmi_bool = false;}
    if(ssid_ok&&password_ok){
        detachInterrupt(RF_PIN);
        ssid_ok=false;
        password_ok=false;
        WiFi.begin((const char*) ssid.c_str(), (const char*) password.c_str());
        int counter =0;
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            counter++;
            if(counter==20){WiFi.disconnect();break;}
            sendString(Serial2,"wifiConnect.txt","Connecting to WiFi..");
            Serial.println("Connecting to WiFi..");
        }
        Serial.println(WiFi.broadcastIP());
        //Serial.println(WiFi.channel());
        //Serial.println(WiFi.SSID());
        //Serial.println(WiFi.dnsIP());
        //Serial.println(WiFi.encryptionType(intssid));
        //Serial.println(WiFi.macAddress());
        //Serial.write((const char*) password.c_str());
        sendString(Serial2,"wifiConnect.txt","connect to "+ssid);sendInt(Serial2,"tm2.en",1);
        sendInt(Serial2,"va10.val",1);
        //Serial.write((const char*) ssid.c_str());
        attachInterrupt(RF_PIN, handler, CHANGE);// re-enable interrupt
    }
    delay(10);
//rfPlotter();
//////////////////////////////////////////////////////////////////////////////////////////////
if (received == true) {
    // disable interrupt to avoid new data corrupting the buffer
    detachInterrupt(RF_PIN);
    // loop over buffer data
    is_rf_post = printSerialToRfData(syncIndex1,syncIndex2,bufer,count);
    if(is_rf_post){
        received = false;
        is_rf_post=false;
        #ifdef DEBUG_SERIAL_STRING_ARR_BUFER
        for( int i = 0;i<counts;i++){Serial.print(bufer[i]); }Serial.println("");
        #endif
        byte_arr = "";
        for( int i = 0;i<counts;i++){byte_arr += (String)bufer[i]; }
        #ifdef DEBUG_STRING_ARR_BUFER
            Serial.print(byte_arr);Serial.println();
        #endif
            
        humidity = ch = bat = temp = 0;
        for(int i = 0, c = 7; i <= 7 ;  i++,c--){bitWrite(datchik,  c, bufer[i]==1?1:0);}    
        for(int i = 8, c = 1; i <= 9 ;  i++,c--){bitWrite(bat,      c, bufer[i]==1?1:0);}batery = a_battery[bat];
        for(int i = 10,c = 1; i <= 11;  i++,c--){bitWrite(ch,       c, bufer[i]==1?1:0);}
        for(int i = 12,c = 11;i <= 23;  i++,c--){bitWrite(temp,     c, bufer[i]==1?1:0);}
        if(bufer[12]==1){temp = ((0b11111111111111111111000000000000|temp)); message = "Temp:-";}else{message = "Temp: "; }temp0=temp/10;temp1=temp%10;
        for(int i = 28,c = 7; i <=counts;  i++,c--){bitWrite(humidity, c, bufer[i]==1?1:0);}
        send_termo_out_to_hmi(byte_arr,batery, ch, temp, humidity);
        Serial.print("Datchik:"); Serial.print(datchik); Serial.print(", ");
        Serial.print("Batery:");  Serial.print(batery);  Serial.print(", ");   
        Serial.print("Chanel:");  Serial.print(ch+1);    Serial.print(", ");
        Serial.print(message);    Serial.print(temp0);   Serial.print("."); Serial.print(temp1); Serial.print(", ");
        Serial.print("Humidity:");Serial.print(humidity);Serial.print("%"); Serial.println("");
    }
    onceTicker.once_ms(1000, restart_attachInterrupt);
  }
/////////////////////////////////////////////////////////////////////////////////////////////
}
void restart_attachInterrupt(){
    attachInterrupt(RF_PIN, handler, CHANGE);// re-enable interrupt
    received = false;
    syncIndex1 = 0;
    syncIndex2 = 0;
}