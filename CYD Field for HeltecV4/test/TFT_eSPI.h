#pragma once
#include <Arduino.h>
#define TFT_BLACK 0
#define TFT_WHITE 0xFFFF
#define TFT_YELLOW 0xFFE0
#define TL_DATUM 0
#define TR_DATUM 2
#define MC_DATUM 4
#define MR_DATUM 5
struct TFT_eSPI{void init();void invertDisplay(bool);void setRotation(int);void fillScreen(uint32_t);void setTextColor(uint32_t,uint32_t);void setTextDatum(uint8_t);int drawString(const String&,int,int,int);int drawString(const char*,int,int,int);int textWidth(const char*,int);int textWidth(const String&s,int f){return textWidth(s.c_str(),f);}
 void drawPixel(int,int,uint32_t);void fillRect(int,int,int,int,uint32_t);void drawRect(int,int,int,int,uint32_t);void fillRoundRect(int,int,int,int,int,uint32_t);void fillTriangle(int,int,int,int,int,int,uint32_t);void drawRoundRect(int,int,int,int,int,uint32_t);void drawFastHLine(int,int,int,uint32_t);void drawLine(int,int,int,int,uint32_t);void fillCircle(int,int,int,uint32_t);void drawCircle(int,int,int,uint32_t);
 void drawArc(int32_t,int32_t,int32_t,int32_t,uint32_t,uint32_t,uint32_t,uint32_t,bool=true);void setSwapBytes(bool);void pushImage(int,int,int,int,uint16_t*);};
