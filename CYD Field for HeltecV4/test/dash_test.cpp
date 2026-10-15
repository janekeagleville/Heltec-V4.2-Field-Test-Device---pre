// Testy parsowania pulpitu: telemetria, konfiguracja LoRa, kanaly, obliczanie czestotliwosci.
#include <cassert>
#include <cstdio>
#include <vector>
#include <cmath>
#include <string>
#include "protocol.h"
#include "spectrum.h"
#include "geo.h"
typedef std::vector<uint8_t> B;
static void var(B& b,uint64_t v){do{b.push_back((v&127)|(v>127?128:0));v>>=7;}while(v);}
static void vf(B& b,int t,uint64_t v){var(b,t<<3);var(b,v);}
static void ff(B& b,int t,float f){var(b,t<<3|5);uint32_t u;memcpy(&u,&f,4);for(int i=0;i<4;i++)b.push_back(u>>(8*i));}
static void nest(B& b,int t,const B& d){var(b,t<<3|2);var(b,d.size());b.insert(b.end(),d.begin(),d.end());}
int main(){
 {B dm;vf(dm,1,87);ff(dm,2,4.05f);ff(dm,3,12.5f);ff(dm,4,1.25f);vf(dm,5,3700);B t;ff(t,1,0);nest(t,2,dm);
  mesh::DeviceMetrics d;assert(mesh::telemetry(t.data(),t.size(),d)&&d.has&&d.battery==87&&fabs(d.voltage-4.05)<1e-4&&fabs(d.chUtil-12.5)<1e-4&&fabs(d.airTx-1.25)<1e-4&&d.uptime==3700);
  // aktualizacja czesciowa: tylko poziom kanalu, bateria zostaje
  B dm2;ff(dm2,3,30.f);assert(mesh::deviceMetrics(dm2.data(),dm2.size(),d)&&d.battery==87&&fabs(d.chUtil-30)<1e-4);
  // wartosci spoza zakresu sa ignorowane, zasilanie zewnetrzne = 101
  B dm3;vf(dm3,1,101);ff(dm3,3,900.f);assert(mesh::deviceMetrics(dm3.data(),dm3.size(),d)&&d.battery==101&&fabs(d.chUtil-30)<1e-4);
  // telemetria bez device_metrics (np. srodowisko) i uciete dane
  B env;B e;ff(e,1,22.f);nest(env,3,e);mesh::DeviceMetrics x;assert(!mesh::telemetry(env.data(),env.size(),x)&&!x.has);
  B cut=t;cut.pop_back();mesh::DeviceMetrics y;assert(!mesh::telemetry(cut.data(),cut.size(),y)&&!y.has);}
 {B l;vf(l,1,1);vf(l,7,1);B c;nest(c,6,l);mesh::LoraCfg g;assert(mesh::loraConfig(c.data(),c.size(),g)&&g.has&&g.usePreset&&g.preset==0&&g.region==1);
  mesh::Modem m;assert(mesh::modem(g,"",m)&&m.sf==11&&m.bwKhz==250&&fabs(m.mhz-906.875)<1e-3);
  g.region=3;assert(mesh::modem(g,"",m)&&fabs(m.mhz-869.525)<1e-3);
  g.region=1;g.preset=5;assert(mesh::modem(g,"",m)&&m.sf==8);
  g.region=9;assert(mesh::modem(g,"",m)&&m.mhz==0&&m.sf==8);          // region bez tabeli czestotliwosci
  g.overrideFreq=910.525f;assert(mesh::modem(g,"",m)&&fabs(m.mhz-910.525)<1e-3);
  g.overrideFreq=0;g.region=1;g.channelNum=1;g.preset=0;assert(mesh::modem(g,"",m)&&fabs(m.mhz-902.125)<1e-3);
  mesh::LoraCfg r;r.usePreset=false;assert(!mesh::modem(r,"",m));r.bw=125;r.sf=9;assert(mesh::modem(r,"",m)&&m.sf==9&&m.bwKhz==125);
  g.preset=42;assert(!mesh::modem(g,"",m));
  B bad;nest(bad,5,B{1,2});mesh::LoraCfg z;assert(!mesh::loraConfig(bad.data(),bad.size(),z)&&!z.has);}
 {B set;B nm{'P','L','-','M','Z'};nest(set,3,nm);B ch;vf(ch,1,0);nest(ch,2,set);vf(ch,3,1);mesh::ChannelInfo c;assert(mesh::channel(ch.data(),ch.size(),c)&&c.index==0&&c.role==1&&std::string(c.name)=="PL-MZ");
  B badIdx;vf(badIdx,1,9);assert(!mesh::channel(badIdx.data(),badIdx.size(),c));
  // inna nazwa kanalu zmienia slot (hash)
  mesh::LoraCfg g;g.usePreset=true;g.region=1;mesh::Modem a,b2;mesh::modem(g,"",a);mesh::modem(g,"PL-MZ",b2);assert(a.mhz!=b2.mhz&&b2.mhz>=902&&b2.mhz<=928);}
 {spec::SampleRing r;r.add(1000,-90,6.f);r.add(2000,-100,-8.f);r.add(3000,-80,10.f);r.add(4000,-60,-30.f);      // 4. szum = -30 -> poza zakresem
  spec::Stats st=spec::stats(r,5000,60000);assert(st.n==4&&st.hasNoise&&st.now==-90&&st.floor==-96&&st.perMin==4);   // szumy: -96,-92,-90,(brak)
  st=spec::stats(r,5000,1500);assert(st.n==1&&!st.hasNoise);                                          // tylko ostatnia probka (t=4000), bez szumu
  st=spec::stats(r,100000,60000);assert(st.n==0&&!st.hasNoise&&st.perMin==0);                         // po oknie
  spec::SampleRing big;for(unsigned i=0;i<1000;i++)big.add(i*100,-110,5.f);assert(big.used==spec::SampleRing::N&&big.total==1000);
  st=spec::stats(big,100000,3600000);assert(st.n==spec::SampleRing::N&&st.now==-115&&st.floor==-115&&st.perMin==240);
  spec::SampleRing w;w.add(4294967000u,-100,5.f);w.add(100u,-100,5.f);st=spec::stats(w,200u,1000);assert(st.n==2);   // przejscie przez 0 licznika ms
  spec::UtilRing u;u.add(12.5f);u.add(250.f);u.add(-3.f);assert(u.pctAgo(0)==0&&u.pctAgo(1)==100&&u.pctAgo(2)==12.5f);}
 {int x,y;auto E7=[](double v){return int32_t(v*1e7);};
  assert(geo::project(E7(52.2297),E7(21.0122),x,y)&&x>60&&x<110&&y>60&&y<110);     // Warszawa
  assert(geo::project(E7(51.4027),E7(21.1471),x,y)&&y>110);                         // Radom
  assert(geo::project(E7(52.5463),E7(19.7065),x,y));                                 // Plock
  assert(!geo::project(E7(51.7592),E7(19.4560),x,y));                                // Lodz: inne wojewodztwo
  assert(!geo::project(E7(52.52),E7(13.40),x,y));                                    // Berlin: poza oknem mapy
  assert(!geo::project(0,0,x,y)&&!geo::project(E7(54.4),E7(18.6),x,y));
  float d=geo::distKm(E7(52.2297),E7(21.0122),E7(51.4027),E7(21.1471));assert(d>90&&d<96);
  assert(fabs(geo::distKm(E7(10),E7(10),E7(10),E7(10)))<1e-3);}
 puts("PASS: dashboard parsing");}
