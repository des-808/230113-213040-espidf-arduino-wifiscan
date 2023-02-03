#ifndef HMI_MINI_h
#define HMI_MINI_h
void comandEnd(HardwareSerial serial){for(int i = 0;i<3;i++){serial.write(0xff);}}
String intToString(int tmp,int sistema_shislenyya){
    static char buf[17];
    return itoa(tmp, buf, sistema_shislenyya);
}

void sendString(HardwareSerial serial,String dev, String tmp){
    serial.print(dev);
    serial.print("=");
    serial.print("\""+tmp+"\"");
    comandEnd(serial);
}
void sendComand(HardwareSerial serial,String dev){
    serial.print(dev);
    comandEnd(serial);
}

void sendInt(HardwareSerial serial,String dev, int tmp){
    serial.print(dev);
    serial.print("=");
    serial.print(tmp);
    comandEnd(serial);
}
#endif