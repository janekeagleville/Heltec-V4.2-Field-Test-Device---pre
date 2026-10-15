#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <string>
#include <vector>
#define FILE_WRITE 1
#define CARD_NONE 0
extern std::vector<std::string> sdFiles;extern std::string sdData;extern bool sdPresent;
struct File{bool ok=false;operator bool()const{return ok;}int printf(const char*,...);void println(const char*);void flush(){}void close(){ok=false;}bool getWriteError(){return false;}void clearWriteError(){}};
struct SDC{bool begin(int,SPIC&,unsigned);void end(){}unsigned char cardType();unsigned long long cardSize(){return 0;}bool exists(const char*);File open(const char*,int);};extern SDC SD;
