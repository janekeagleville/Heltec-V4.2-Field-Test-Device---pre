#pragma once
// Szum i "widmo" liczone z pakietow odebranych przez Trackera. Meshtastic nie udostepnia pomiaru radia,
// wiec poziom szumu to ESTYMATA: szum = RSSI - SNR pakietu (dBm). Dane sa tylko wtedy, gdy w sieci cos nadaje.
#include <stdint.h>
#include <stddef.h>
#include <algorithm>
namespace spec {
constexpr int8_t NONE=INT8_MIN;
struct Sample{uint32_t t=0;int8_t rssi=NONE,noise=NONE;};
struct SampleRing{static constexpr unsigned N=240;Sample s[N];unsigned head=0,used=0;uint32_t total=0;
 // rssi/snr z pakietu; szum poza zakresem -160..-60 dBm uznawany za nieprawdopodobny (np. artefakt zaokraglen)
 void add(uint32_t t,int rssi,float snr){Sample x;x.t=t;x.rssi=int8_t(rssi<-127?-127:rssi>0?0:rssi);int n=int(rssi-snr+(rssi-snr>=0?0.5f:-0.5f));x.noise=(n>=-127&&n<=-60)?int8_t(n):NONE;s[head]=x;head=(head+1)%N;if(used<N)++used;++total;}
 const Sample& at(unsigned i)const{return s[(head+N-used+i)%N];}   // 0 = najstarszy
};
struct Stats{bool hasNoise=false;int now=0,floor=0;unsigned n=0,perMin=0;};
// Okno: ostatnie windowMs; "floor" = 10. percentyl szumu (odporny na pojedyncze odstajace pakiety); perMin = pakiety z ostatnich 60 s.
inline Stats stats(const SampleRing& r,uint32_t now,uint32_t windowMs){Stats o;int8_t v[SampleRing::N];unsigned k=0;
 for(unsigned i=0;i<r.used;i++){const Sample& x=r.at(i);uint32_t age=now-x.t;if(age>windowMs)continue;++o.n;if(age<=60000)++o.perMin;if(x.noise!=NONE){v[k++]=x.noise;o.now=x.noise;o.hasNoise=true;}}
 if(k){unsigned p=(k-1)/10;std::nth_element(v,v+p,v+k);o.floor=v[p];}return o;}
// Historia zajetosci kanalu (%), probka co 10 s, wartosc*2 w uint8 (krok 0,5 %).
struct UtilRing{static constexpr unsigned N=360;uint8_t v[N]={};unsigned head=0,used=0;
 void add(float pct){float c=pct<0?0:pct>100?100:pct;v[head]=uint8_t(c*2+0.5f);head=(head+1)%N;if(used<N)++used;}
 float pctAgo(unsigned a)const{return v[(head+N-1-a)%N]/2.f;}};   // a=0 najnowsza
}
