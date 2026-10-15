#pragma once
// Rzutowanie pozycji na tlo mapy Mazowsza i odleglosc (haversine).
#include <math.h>
#include <stdint.h>
#include "mapdata.h"
namespace geo {
inline bool inMask(int x,int y){if(x<0||y<0||x>=int(MAP_W)||y>=int(MAP_H))return false;unsigned i=unsigned(y)*MAP_W+unsigned(x);return (pgm_read_byte(mapMask+(i>>3))>>(i&7))&1;}
// lat/lon w 1e-7 stopnia; true gdy punkt lezy w granicach wojewodztwa mazowieckiego (wg maski mapy). (0,0) = brak pozycji.
inline bool project(int32_t lat7,int32_t lon7,int& x,int& y){if(!lat7&&!lon7)return false;double lat=lat7*1e-7,lon=lon7*1e-7;
 double fx=(lon-MAP_LON_MIN)/(MAP_LON_MAX-MAP_LON_MIN)*MAP_W,fy=(MAP_LAT_MAX-lat)/(MAP_LAT_MAX-MAP_LAT_MIN)*MAP_H;if(fx<0||fy<0||fx>=MAP_W||fy>=MAP_H)return false;x=int(fx);y=int(fy);return inMask(x,y);}
// Pozycja z ulamkiem piksela (bez maski) - do rysowania w powiekszeniu.
inline bool projectF(int32_t lat7,int32_t lon7,float& fx,float& fy){if(!lat7&&!lon7)return false;double lat=lat7*1e-7,lon=lon7*1e-7;
 double x=(lon-MAP_LON_MIN)/(MAP_LON_MAX-MAP_LON_MIN)*MAP_W,y=(MAP_LAT_MAX-lat)/(MAP_LAT_MAX-MAP_LAT_MIN)*MAP_H;if(x<0||y<0||x>=MAP_W||y>=MAP_H)return false;fx=float(x);fy=float(y);return true;}
inline float distKm(int32_t la1,int32_t lo1,int32_t la2,int32_t lo2){const double r=M_PI/180e7;double p1=la1*r,p2=la2*r,dl=(lo2-lo1)*r,dp=p2-p1;double a=sin(dp/2)*sin(dp/2)+cos(p1)*cos(p2)*sin(dl/2)*sin(dl/2);return float(12742.0*asin(sqrt(a)));}
}
