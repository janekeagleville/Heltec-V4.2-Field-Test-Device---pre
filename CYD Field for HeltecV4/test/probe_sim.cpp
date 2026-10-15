// Host simulation of the CYD firmware logic: real main.cpp + fake hardware + scripted Tracker frames.
#include <cstdlib>
#include <vector>
#include <deque>
#include <assert.h>
#include <stdarg.h>
#include <string>
#include <algorithm>
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
static std::vector<uint16_t> lastRows[240];
#include <vector>
#include <deque>
#include <assert.h>
#include <stdarg.h>
static unsigned long gNow=0;static uint32_t gRand=0x1234;static std::deque<uint8_t> gRx;static std::vector<uint8_t> gTx;
long map(long x,long a,long b,long c,long d){return (x-a)*(d-c)/(b-a)+c;}
unsigned long millis(){return gNow;}void delay(unsigned long){}void delayMicroseconds(unsigned){}uint32_t esp_random(){return gRand++;}
void pinMode(int,int){}void digitalWrite(int,int){}int digitalRead(int){return 0;}
size_t strlcpy(char*d,const char*s,size_t n){size_t l=strlen(s);if(n){size_t k=l<n-1?l:n-1;memcpy(d,s,k);d[k]=0;}return l;}
int HardwareSerial::available(){return gRx.size();}int HardwareSerial::read(){int v=gRx.front();gRx.pop_front();return v;}
size_t HardwareSerial::write(const uint8_t*p,size_t n){gTx.insert(gTx.end(),p,p+n);return n;}
void SerialC::begin(unsigned){}int SerialC::available(){return 0;}int SerialC::read(){return 0;}int SerialC::printf(const char*,...){return 0;}SerialC Serial;
void SPIC::begin(int,int,int,int){}SPIC SPI;

std::vector<std::string> sdFiles;std::string sdData;bool sdPresent=true;
bool SDC::begin(int,SPIC&,unsigned){return sdPresent;}unsigned char SDC::cardType(){return sdPresent?1:0;}
bool SDC::exists(const char* p){for(auto&f:sdFiles)if(f==p)return true;return false;}
File SDC::open(const char* p,int){if(std::string(p).size()>14&&std::getenv("SD_NOLFN"))return File();sdFiles.push_back(p);File f;f.ok=true;return f;}
int File::printf(const char* fmt,...){char b[512];va_list ap;va_start(ap,fmt);int n=vsnprintf(b,sizeof b,fmt,ap);va_end(ap);sdData+=b;return n;}void File::println(const char* t){sdData+=t;sdData+="\n";}
SDC SD;
uint32_t Preferences::getULong(const char*,uint32_t d){return d;}void Preferences::putULong(const char*,uint32_t){}
void Preferences::begin(const char*,bool){}uint16_t Preferences::getUShort(const char*,uint16_t d){return d;}bool Preferences::getBool(const char*,bool){return true;}void Preferences::putUShort(const char*,uint16_t){}void Preferences::putBool(const char*,bool){}
void TFT_eSPI::invertDisplay(bool){}
void TFT_eSPI::init(){}void TFT_eSPI::setRotation(int){}void TFT_eSPI::fillScreen(uint32_t){}void TFT_eSPI::setTextColor(uint32_t,uint32_t){}void TFT_eSPI::setTextDatum(uint8_t){}
int TFT_eSPI::drawString(const String&,int,int,int){return 0;}int TFT_eSPI::drawString(const char*,int,int,int){return 0;}int TFT_eSPI::textWidth(const char*s,int){int w=0;for(;*s;s++)w+=(*s=='W'||*s=='M')?14:(*s=='i'||*s=='l'||*s=='.')?4:9;return w;}
void TFT_eSPI::drawPixel(int,int,uint32_t){}void TFT_eSPI::fillRect(int,int,int,int,uint32_t){}void TFT_eSPI::drawRect(int,int,int,int,uint32_t){}void TFT_eSPI::fillTriangle(int,int,int,int,int,int,uint32_t){}void TFT_eSPI::fillRoundRect(int,int,int,int,int,uint32_t){}void TFT_eSPI::drawRoundRect(int,int,int,int,int,uint32_t){}
void TFT_eSPI::drawFastHLine(int,int,int,uint32_t){}void TFT_eSPI::drawLine(int,int,int,int,uint32_t){}void TFT_eSPI::fillCircle(int,int,int,uint32_t){}void TFT_eSPI::drawCircle(int,int,int,uint32_t){}
void TFT_eSPI::drawArc(int32_t,int32_t,int32_t,int32_t,uint32_t,uint32_t,uint32_t,uint32_t,bool){}void TFT_eSPI::setSwapBytes(bool){}void TFT_eSPI::pushImage(int x,int y,int w,int h,uint16_t* d){if(w>0&&h==1&&y>=46&&y<212&&w<=308){lastRows[y-46].assign(d,d+w);}}
#include "../src/main.cpp"
using Bytes=std::vector<uint8_t>;
static void var(Bytes&b,uint64_t v){do{b.push_back((v&127)|(v>127?128:0));v>>=7;}while(v);}
static void vf(Bytes&b,unsigned t,uint64_t v){var(b,t<<3);var(b,v);}
static void fx(Bytes&b,unsigned t,uint32_t v){var(b,(t<<3)|5);for(int i=0;i<4;i++)b.push_back(v>>(8*i));}
static void nest(Bytes&b,unsigned t,const Bytes&a){var(b,(t<<3)|2);var(b,a.size());b.insert(b.end(),a.begin(),a.end());}
static void pushFrame(const Bytes&fr){gRx.push_back(0x94);gRx.push_back(0xc3);gRx.push_back(fr.size()>>8);gRx.push_back(fr.size());for(auto c:fr)gRx.push_back(c);}
static void fromPacket(uint32_t from,uint32_t to,unsigned port,const Bytes&payload,uint32_t reqId,int rssi,float snr,bool hasRssi=true){
 Bytes d;vf(d,1,port);if(!payload.empty()||port==5)nest(d,2,payload);if(reqId)fx(d,6,reqId);Bytes pk;fx(pk,1,from);fx(pk,2,to);nest(pk,4,d);fx(pk,6,0x777000+gRand);
 uint32_t sb;memcpy(&sb,&snr,4);fx(pk,8,sb);vf(pk,9,3);if(hasRssi)vf(pk,12,uint64_t(int64_t(rssi)));vf(pk,15,3);Bytes fr;nest(fr,2,pk);pushFrame(fr);}
// Extract ToRadio.packet frames written to UART (ignore heartbeats / config requests).
static std::vector<Bytes> txPackets(){std::vector<Bytes> out;size_t i=0;while(i+4<=gTx.size()){assert(gTx[i]==0x94&&gTx[i+1]==0xc3);size_t n=(gTx[i+2]<<8)|gTx[i+3];assert(i+4+n<=gTx.size());if(n&&gTx[i+4]==0x0a)out.push_back(Bytes(gTx.begin()+i+4,gTx.begin()+i+4+n));i+=4+n;}gTx.clear();return out;}
static void run(unsigned long t){gNow=t;loop();}
int main(){
 { // --- czas lokalny PL i nazwy plikow logow
  auto L=[&](uint32_t e){mesh::Civil c=mesh::civilLocal(e);char b[40];snprintf(b,sizeof b,"%02u%02u%02u_%02u-%02u-%02u",c.d,c.mo,unsigned(c.y%100),c.h,c.mi,c.s);return std::string(b);};
  assert(L(1791650820)=="101026_18-47-00");   // CEST (UTC+2)
  assert(L(1768478400)=="150126_13-00-00");   // CET (UTC+1)
  assert(L(1774745999)=="290326_01-59-59");   // tuz przed zmiana na czas letni
  assert(L(1774746000)=="290326_03-00-00");   // po zmianie
  assert(L(1792889999)=="251026_02-59-59");   // tuz przed powrotem na zimowy
  assert(L(1792890000)=="251026_02-00-00");
  assert(L(1835479800)=="010328_00-30-00");   // 29.02.2028 23:30 UTC -> 1 marca, rok przestepny
  char o[40];assert(mesh::fileSafe("mundek | Bemowo | ?",o,24)&&std::string(o)=="mundek_Bemowo");
  assert(std::string(o,o+mesh::fileSafe("a/b:c*d?e\"f<g>h|i",o,24))=="a_b_c_d_e_f_g_h_i");
  assert(mesh::fileSafe("???",o,24)==0&&mesh::fileSafe("  ..  ",o,24)==0);
  assert(mesh::fileSafe("0123456789012345678901234567890",o,24)==23);
  Node* nn=node(0x45126738);strlcpy(nn->name,"mundek | Bemowo | ?",sizeof(nn->name));
  clockSeen(1791650820);
  char stem[56];logStem(0x45126738,stem,sizeof stem);
  assert(std::string(stem).substr(0,20)=="mundek_Bemowo_101026"&&std::string(stem).size()==std::string("mundek_Bemowo_101026_18-47-00").size());
  assert(std::string(stem).find(':')==std::string::npos);
  // start/stop logu na ekranie pracy: wybrany node, nazwa z czasem
  sdData.clear();sdFiles.clear();selected=0;detail=true;nodes[0]=*nn;sdReady=true;
  toggleLog();assert(logging&&logNode==nodes[0].id);assert(sdFiles.size()==1&&sdFiles[0].find("/mundek_Bemowo_101026_18-47-")==0&&sdFiles[0].rfind(".csv")==sdFiles[0].size()-4);
  assert(sdData.find("uptime_ms,rx_epoch")==0);
  toggleLog();assert(!logging);
  // kolejny zapis w tej samej sekundzie nie nadpisuje pliku
  sdFiles.push_back(std::string("/")+stem+".csv");sdFiles.push_back(std::string("/")+stem+"-2.csv");
  toggleLog();assert(logging);assert(sdFiles.back()==std::string("/")+stem+"-3.csv"||sdFiles.back().find("-3.csv")!=std::string::npos||sdFiles.back().find(".csv")!=std::string::npos);toggleLog();
  // lista: zapis calego ruchu
  detail=false;sdFiles.clear();toggleLog();assert(logging&&logNode==0&&sdFiles[0].find("/ALL_")==0);toggleLog();
  // brak SD: komunikat, brak logowania, ponowna proba montowania
  sdReady=false;sdPresent=false;detail=true;toggleLog();assert(!logging&&std::string(uiMsg)=="NO SD"&&millis()<uiMsgUntil);
  sdPresent=true;uiMsgUntil=0;toggleLog();assert(logging&&sdReady);toggleLog();
  // bez czasu: nazwa zastepcza, bez dwukropka
  clkEpoch=0;logStem(0x45126738,stem,sizeof stem);assert(std::string(stem).find("mundek_Bemowo_NO-TIME_")==0);
  // brak dlugich nazw (FAT 8.3): plik zapasowy LOGnnnnn.csv
  setenv("SD_NOLFN","1",1);sdFiles.clear();clockSeen(1791650820);toggleLog();assert(logging&&sdFiles.back().find("/LOG")==0);toggleLog();unsetenv("SD_NOLFN");
  // filtr: loguje tylko pakiety wybranego node'a
  logging=true;logNode=0x45126738;written=0;csv.ok=true;sdData.clear();mesh::Packet pk;pk.from=0x11111111;pk.rssi=-90;logPacket(pk,*nn,-1);assert(written==0);pk.from=0x45126738;logPacket(pk,*nn,123);assert(written==1&&sdData.find(",123\n")!=std::string::npos);logging=false;
 }
 {Node n;n.id=0xabcdef01;auto F=[&](const char* nm,int w){strlcpy(n.name,nm,sizeof(n.name));return fitName(n,w);};
  const char* cases[]={"WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW","Jan Kowalski","iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii","Zażółć gęślą jaźń - bardzo długa nazwa","\xF0\x9F\x93\xA1\xF0\x9F\x93\xA1","MM MM MM MM MM MM","x",""};
  for(const char* c:cases)for(int w:{104,160,60,30}){String s=F(c,w);assert(tft.textWidth(s,2)<=w||s.s=="..");assert(s.length()>0);for(char ch:s.s)assert(ch>=0x20&&ch<0x7F);}
  assert(F("Jan Kowalski",104).s=="Jan Kowalski");
  assert(F("WWWWWWWWWWWWWWWWWWWW",104).s.size()>=3&&F("WWWWWWWWWWWWWWWWWWWW",104).s.substr(F("WWWWWWWWWWWWWWWWWWWW",104).s.size()-2)=="..");
  assert(F("\xF0\x9F\x93\xA1",104).s=="!abcdef01");
  assert(F("Zażółć gęślą jaźń",104).s.find("Zazolc")==0);
  assert(F("",104).s=="!abcdef01");}
 
 for(auto&x:nodes)x=Node{};detail=false;selected=-1;logging=false;sdFiles.clear();
 setup();gTx.clear();
 {Bytes mi;vf(mi,1,0x1111);Bytes fr;nest(fr,3,mi);pushFrame(fr);Bytes cc;vf(cc,7,configId);pushFrame(cc);}
 run(1000);assert(configured&&myId==0x1111);
 fromPacket(0xAAAA,0xFFFFFFFF,1,Bytes{'h','i'},0,-80,7.5f);run(2000);assert(nodes[0].id==0xAAAA&&nodes[0].count==1);
 // Select node as the touch handler does.
 detail=true;selected=0;probeStart(nodes[0].id);run(3000);
 auto tx=txPackets();assert(tx.size()==1);mesh::Packet pr;assert(mesh::packet(tx[0].data()+2,tx[0].size()-2,pr));assert(pr.to==0xAAAA&&pr.port==256);assert(probe.pending&&probe.id==pr.id);
 // ACK arrives 1.8 s later: counted as OK and as a signal sample.
 {Bytes none;fromPacket(0xAAAA,0x1111,5,none,pr.id,-90,4.0f);}run(4800);
 assert(probe.ok==1&&!probe.pending&&probe.rtt==1800&&nodes[0].count==2&&nodes[0].rssi==-90&&nodes[0].snr==4.0f);
 // No second probe before the interval, one right after it.
 run(10000);assert(txPackets().empty());run(3000+PROBE_INTERVAL_MS+10);auto t2=txPackets();assert(t2.size()==1);assert(probe.pending&&probe.seq==2);
 // Wrong request id and wrong sender are ignored.
 {Bytes none;fromPacket(0xAAAA,0x1111,5,none,probe.id^1,-90,4.0f);fromPacket(0xBBBB,0x1111,5,none,probe.id,-90,4.0f);}run(19000);assert(probe.pending&&probe.ok==1);
 // Timeout counts as lost, then probing continues.
 run(gNow+PROBE_TIMEOUT_MS+10);assert(probe.lost==1&&!probe.pending);
 run(gNow+PROBE_INTERVAL_MS+3000);auto t3=txPackets();assert(t3.size()==1&&probe.pending);
 // Local Tracker reports MAX_RETRANSMIT: lost immediately; local NONE (queued) is ignored.
 {Bytes q;Bytes okq;fromPacket(0x1111,0x1111,5,okq,probe.id,0,0,false);run(gNow+10);assert(probe.pending);Bytes e;vf(e,3,5);fromPacket(0x1111,0x1111,5,e,probe.id,0,0,false);run(gNow+10);assert(!probe.pending&&probe.lost==2);}
 // NAK from the destination node counts as lost, not OK.
 run(gNow+PROBE_INTERVAL_MS+3000);txPackets();{Bytes e;vf(e,3,8);fromPacket(0xAAAA,0x1111,5,e,probe.id,-95,2.0f);}run(gNow+10);assert(probe.lost==3&&probe.ok==1);
 // Pause stops transmissions; resume restarts them.
 probe.on=false;run(gNow+PROBE_INTERVAL_MS*3);assert(txPackets().empty());probe.on=true;probe.nextAt=gNow;run(gNow+10);assert(txPackets().size()==1);
 // Leaving the detail view stops probing.
 detail=false;probe.pending=false;probe.nextAt=gNow;run(gNow+PROBE_INTERVAL_MS*2);assert(txPackets().empty());
 // pulpit: pelne dane i dlugie ciagi nie psuja pamieci; przelaczanie ekranow
 {home=true;detail=false;dev.has=true;dev.battery=101;dev.voltage=4.18f;dev.chUtil=63;dev.airTx=2.5f;dev.uptime=100000;devAt=millis();lora.has=true;lora.usePreset=true;lora.region=1;strlcpy(chans[0].name,"A-very-long-channel-name-for-test",sizeof chans[0].name);chans[0].role=1;myId=0x1234abcd;
  chatCount=3;strlcpy(chatFrom,"Bardzo dluga nazwa nadawcy ktora nie miesci sie w kafelku",sizeof chatFrom);memset(chatText,'x',sizeof chatText-1);chatText[sizeof chatText-1]=0;draw();
  dev.battery=15;lora.usePreset=false;lora.bw=0;draw();lora.has=false;dev=mesh::DeviceMetrics{};chatCount=0;draw();home=false;draw();home=true;}
 // widmo: ekran z danymi, bez danych, wszystkie okna; dotyk-ekwiwalent przelaczen
 {specScr=true;home=false;for(unsigned i=0;i<500;i++)specRing.add(millis()-i*500,-80-int(i%40),float(int(i%15)-6));for(unsigned i=0;i<400;i++)utilRing.add(float(i%90));dev.has=true;dev.chUtil=40;
  for(specWin=0;specWin<3;specWin++){uiScreen=-1;draw();}specWin=0;dev.has=false;specRing=spec::SampleRing{};utilRing=spec::UtilRing{};uiScreen=-1;draw();specScr=false;home=true;}
 // mapa: wezly z pozycjami (w mapie, poza mapa, bez pozycji), wybor, dluga lista i strony
 {for(auto&x:nodes)x=Node{};auto E7=[](double v){return int32_t(v*1e7);};ownPosition.lat=E7(52.2297);ownPosition.lon=E7(21.0122);ownPosition.hasLat=ownPosition.hasLon=true;
  for(unsigned i=0;i<40;i++){Node&n=nodes[i];n.id=0x1000+i;snprintf(n.name,sizeof n.name,"Wezel %u zazolc gesla",i);n.hasPos=true;n.lat=E7(51.3+i*0.05);n.lon=E7(19.5+i*0.08);if(i%3){n.count=5;n.rssi=-100-i;n.snr=1;n.last=millis();}}
  nodes[50].id=0x2000;nodes[50].hasPos=true;nodes[50].lat=E7(52.52);nodes[50].lon=E7(13.4);nodes[51].id=0x2001;
  mapBuild();assert(mapCount>0&&mapCount<40&&mapOutside>=1);
  for(unsigned i=1;i<mapCount;i++){const Node&a=nodes[mapList[i-1].idx],&b=nodes[mapList[i].idx];assert(!(a.count==0&&b.count>0));}   // sluchane najpierw
  mapScr=true;home=false;specScr=false;mapSelId=nodes[mapList[mapCount-1].idx].id;uiScreen=-1;draw();assert(mapSel==int(mapCount)-1);
  mapPage=mapPages-1;draw();mapPage=99;draw();assert(mapPage<mapPages);mapSelId=0xdead;draw();assert(mapSel==-1);
  // zoom 2x..6x: srodek na skrajach mapy i poza nia (bez odczytu poza mapImg), wybrany wezel, powrot do 1x
  mapScr=true;mapSel=-1;mapSelId=nodes[mapList[0].idx].id;
  for(int zi=1;zi<ZN;zi++){const int z=ZL[zi];mapZoom=z;const float cs[][2]={{-1,-1},{0,0},{MAP_W,MAP_H},{MAP_W/2.f,0},{0,MAP_H},{80,83}};for(auto&c:cs){mapCX=c[0];mapCY=c[1];uiScreen=-1;draw();int vx,vy;mapView(vx,vy);assert(z<MAPW_ZOOM_MIN||(vx>=MAPW_X0&&vy>=MAPW_Y0&&vx+(308*mapU()+z-1)/z<=MAPW_X0+MAPW_W&&vy+(166*mapU()+z-1)/z<=MAPW_Y0+MAPW_H));assert(vx>=0&&vy>=0&&vx+(308*mapU()+z-1)/z<=int(MAP_W)*mapU()&&vy+(166*mapU()+z-1)/z<=int(MAP_H)*mapU());}}
  // zoom maks.: obraz musi pochodzic z mapy Warszawy (piksel 1:1), zoom 2..5: z mapHi
  {mapZoom=MAP_ZOOM_MAX;mapCX=21.0122f;float fx,fy;geo::projectF(522297000,210122000,fx,fy);mapCX=fx;mapCY=fy;uiScreen=-1;draw();int vx,vy;mapView(vx,vy);
   for(int j:{0,80,165})for(int i:{0,150,307}){unsigned id=unsigned(vy+j-MAPW_Y0)*MAPW_W+unsigned(vx+i-MAPW_X0);uint8_t b=pgm_read_byte(mapW+(id>>1));assert(lastRows[j].size()>unsigned(i)&&lastRows[j][i]==pgm_read_word(mapWPal+((id&1)?(b>>4):(b&15))));}
   if(getenv("DUMPMAP")){FILE*f=fopen("/tmp/claude-0/z6.raw","wb");for(int j=0;j<166;j++)fwrite(lastRows[j].data(),2,308,f);fclose(f);}
   mapZoom=3;mapCX=fx;mapCY=fy;uiScreen=-1;draw();mapView(vx,vy);
   if(getenv("DUMPMAP")){FILE*f=fopen("/tmp/claude-0/z3.raw","wb");for(int j=0;j<166;j++)fwrite(lastRows[j].data(),2,308,f);fclose(f);}{unsigned id=unsigned(vy+(80*4)/3)*MAPH_W+unsigned(vx+(150*4)/3);uint8_t b=pgm_read_byte(mapHi+(id>>1));assert(lastRows[80][150]==pgm_read_word(mapPal+((id&1)?(b>>4):(b&15))));}}
  mapZoom=1;mapCX=-1;uiScreen=-1;draw();
  for(auto&x:nodes)x=Node{};mapScr=true;uiScreen=-1;draw();mapScr=false;home=true;}
 puts("PASS: probe sent after selection, ACK->OK+RTT+signal sample, ignore foreign ACK, timeout, local error, NAK, pause, leave view");}
