#pragma once
#include <Arduino.h>
struct Preferences{void begin(const char*,bool);uint16_t getUShort(const char*,uint16_t);bool getBool(const char*,bool);void putUShort(const char*,uint16_t);void putBool(const char*,bool);uint32_t getULong(const char*,uint32_t);void putULong(const char*,uint32_t);};
