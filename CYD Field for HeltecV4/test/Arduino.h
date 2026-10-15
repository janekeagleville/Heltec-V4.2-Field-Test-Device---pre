#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <math.h>
#include <algorithm>
#define PROGMEM
#define pgm_read_word(p) (*(p))
#define pgm_read_byte(p) (*(p))
#define constrain(a,lo,hi) ((a)<(lo)?(lo):((a)>(hi)?(hi):(a)))
using std::min;
using std::max;

#define DEG_TO_RAD 0.017453292519943295769236907684886
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define SERIAL_8N1 0
class String{public:std::string s;String(){}String(const char*c):s(c){}String(int v):s(std::to_string(v)){}String(unsigned v):s(std::to_string(v)){}String(long v):s(std::to_string(v)){}String(unsigned long v):s(std::to_string(v)){}String(double v,int d=2){char b[40];snprintf(b,40,"%.*f",d,v);s=b;}
 String substring(unsigned a,unsigned b)const{return String(s.substr(a,b-a).c_str());}String operator+(const String&o)const{String r;r.s=s+o.s;return r;}String operator+(const char*c)const{String r;r.s=s+c;return r;}friend String operator+(const char*c,const String&o){String r;r.s=std::string(c)+o.s;return r;}const char*c_str()const{return s.c_str();}String& operator+=(const String&o){s+=o.s;return *this;}String& operator+=(const char*c){s+=c;return *this;}unsigned length()const{return s.size();}void remove(unsigned i,unsigned n){s.erase(i,n);}void trim(){while(!s.empty()&&s.back()==' ')s.pop_back();while(!s.empty()&&s.front()==' ')s.erase(0,1);}};
long map(long,long,long,long,long);
unsigned long millis();void delay(unsigned long);void delayMicroseconds(unsigned);uint32_t esp_random();
void pinMode(int,int);void digitalWrite(int,int);int digitalRead(int);size_t strlcpy(char*,const char*,size_t);
struct HardwareSerial{HardwareSerial(int){}void setRxBufferSize(size_t){}void begin(unsigned,int,int,int){}void end(){}int available();int read();size_t write(const uint8_t*,size_t);};
struct SerialC{long parseInt(){return 0;}void begin(unsigned);int available();int read();int printf(const char*,...);};extern SerialC Serial;
