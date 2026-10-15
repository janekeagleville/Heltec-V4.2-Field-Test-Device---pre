#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace mesh {
struct Field { unsigned tag=0, wire=0; uint64_t value=0; const uint8_t* bytes=nullptr; size_t size=0; };
class Reader {
 const uint8_t* p; size_t n; bool good=true;
 bool var(uint64_t &v) { v=0; for(unsigned i=0;i<10;i++){if(!n) return good=false; uint8_t b=*p++; --n; if(i==9 && b>1) return good=false; v|=uint64_t(b&127)<<(7*i); if(!(b&128))return true;}return good=false; }
 public:
 Reader(const uint8_t* b,size_t len):p(b),n(len){}
 bool ok()const{return good;}
 bool next(Field& f){ if(!good||!n)return false; f=Field{};uint64_t key;if(!var(key)||!(key>>3)||key>>3>0x1fffffff)return good=false;f.tag=key>>3;f.wire=key&7;
 if(f.wire==0)return var(f.value);
 if(f.wire==1||f.wire==5){size_t k=f.wire==1?8:4;if(n<k)return good=false;for(size_t i=0;i<k;i++)f.value|=uint64_t(p[i])<<(8*i);p+=k;n-=k;return true;}
 if(f.wire==2){uint64_t len;if(!var(len)||len>n)return good=false;f.bytes=p;f.size=size_t(len);p+=f.size;n-=f.size;return true;}return good=false; }
};
inline float asFloat(uint64_t v){uint32_t bits=uint32_t(v);float f;memcpy(&f,&bits,4);return f;}
inline void copyText(char* dst,size_t cap,const Field& f){if(!cap)return;size_t k=f.size<cap-1?f.size:cap-1;memcpy(dst,f.bytes,k);dst[k]=0;}
// Display-safe name: the TFT bitmap fonts are ASCII-only, so UTF-8 is transliterated (Polish and common
// Latin letters), emoji/symbols/unknown scripts are dropped, controls become spaces, runs of spaces collapse,
// and a multi-byte sequence cut off by the buffer end is discarded. Returns the output length (0 = nothing printable).
inline const char* translit(uint32_t c){
 switch(c){case 0x105:return "a";case 0x107:return "c";case 0x119:return "e";case 0x142:return "l";case 0x144:return "n";case 0xF3:return "o";case 0x15B:return "s";case 0x17A:case 0x17C:return "z";
  case 0x104:return "A";case 0x106:return "C";case 0x118:return "E";case 0x141:return "L";case 0x143:return "N";case 0xD3:return "O";case 0x15A:return "S";case 0x179:case 0x17B:return "Z";
  case 0xE4:case 0xE0:case 0xE1:case 0xE2:case 0xE3:case 0xE5:return "a";case 0xE8:case 0xE9:case 0xEA:case 0xEB:return "e";case 0xEC:case 0xED:case 0xEE:case 0xEF:return "i";
  case 0xF2:case 0xF4:case 0xF5:case 0xF6:return "o";case 0xF9:case 0xFA:case 0xFB:case 0xFC:return "u";case 0xE7:return "c";case 0xF1:return "n";case 0xDF:return "ss";
  case 0xC4:case 0xC0:case 0xC1:case 0xC2:case 0xC3:case 0xC5:return "A";case 0xC8:case 0xC9:case 0xCA:case 0xCB:return "E";case 0xCC:case 0xCD:case 0xCE:case 0xCF:return "I";
  case 0xD2:case 0xD4:case 0xD5:case 0xD6:return "O";case 0xD9:case 0xDA:case 0xDB:case 0xDC:return "U";case 0xC7:return "C";case 0xD1:return "N";
  case 0xA0:return " ";case 0x2013:case 0x2014:return "-";case 0x2018:case 0x2019:return "'";case 0x201C:case 0x201D:return "\"";}
 return nullptr;}
inline size_t displayName(const char* in,char* out,size_t cap){
 if(!cap)return 0;size_t o=0;bool space=true;
 auto put=[&](const char* t){for(;*t&&o+1<cap;t++){if(*t==' '){if(space)continue;space=true;}else space=false;out[o++]=*t;}};
 for(size_t i=0;in&&in[i];){uint8_t b=uint8_t(in[i]);
  if(b<0x80){char t[2]={b>=0x20&&b!=0x7F?char(b):' ',0};put(t);i++;continue;}
  unsigned n=(b>=0xF0&&b<=0xF4)?3:(b>=0xE0)?2:(b>=0xC2&&b<0xE0)?1:0;
  if(!n){i++;continue;}
  uint32_t c=n==1?(b&0x1F):n==2?(b&0x0F):(b&0x07);unsigned k=0;
  while(k<n&&in[i+1+k]&&(uint8_t(in[i+1+k])&0xC0)==0x80){c=(c<<6)|(uint8_t(in[i+1+k])&0x3F);k++;}
  i+=1+k;if(k<n)continue; // truncated or malformed sequence
  const char* t=translit(c);if(t)put(t);}
 while(o&&out[o-1]==' ')o--;out[o]=0;return o;}
// Czas lokalny dla Polski (CET/CEST wg regul UE) z epoch UTC; bez bibliotek czasu, do nazw plikow logow.
struct Civil{int y;unsigned mo,d,h,mi,s;};
inline int64_t daysFromCivil(int y,unsigned m,unsigned d){y-=m<=2;int era=(y>=0?y:y-399)/400;unsigned yoe=unsigned(y-era*400);unsigned doy=(153*(m>2?m-3:m+9)+2)/5+d-1;unsigned doe=yoe*365+yoe/4-yoe/100+doy;return int64_t(era)*146097+int64_t(doe)-719468;}
inline void civilFromDays(int64_t z,int& y,unsigned& m,unsigned& d){z+=719468;int64_t era=(z>=0?z:z-146096)/146097;unsigned doe=unsigned(z-era*146097);unsigned yoe=(doe-doe/1460+doe/36524-doe/146096)/365;y=int(yoe)+int(era)*400;unsigned doy=doe-(365*yoe+yoe/4-yoe/100);unsigned mp=(5*doy+2)/153;d=doy-(153*mp+2)/5+1;m=mp<10?mp+3:mp-9;y+=m<=2;}
inline int64_t lastSundayDay(int y,unsigned month){int64_t last=daysFromCivil(y,month,31);return last-((last+4)%7);}  // dla miesiecy 31-dniowych (marzec, pazdziernik)
inline int32_t warsawOffset(uint32_t utc){int y;unsigned m,d;civilFromDays(utc/86400,y,m,d);int64_t s=lastSundayDay(y,3)*86400+3600,e=lastSundayDay(y,10)*86400+3600;return (int64_t(utc)>=s&&int64_t(utc)<e)?7200:3600;}
inline Civil civilLocal(uint32_t utc){uint32_t t=utc+uint32_t(warsawOffset(utc));int y;unsigned m,d;civilFromDays(t/86400,y,m,d);uint32_t r=t%86400;return Civil{y,m,d,r/3600,(r%3600)/60,r%60};}
// Nazwa bezpieczna dla FAT: litery, cyfry, "-_.!" ; reszta -> '_', powtorzone '_' zwijane, brzegi przyciete, max cap-1 znakow.
inline size_t fileSafe(const char* in,char* out,size_t cap){if(!cap)return 0;size_t o=0;bool us=true;for(size_t i=0;in&&in[i]&&o+1<cap;i++){char c=in[i];bool ok=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'||c=='!';if(ok){out[o++]=c;us=false;}else if(!us){out[o++]='_';us=true;}}while(o&&(out[o-1]=='_'||out[o-1]=='.'))o--;out[o]=0;return o;}
struct Position { int32_t lat=0,lon=0;uint32_t epoch=0;uint8_t source=0;bool hasLat=false,hasLon=false; bool valid()const{return hasLat&&hasLon&&lat>=-900000000&&lat<=900000000&&lon>=-1800000000&&lon<=1800000000;} };
inline bool position(const uint8_t* b,size_t len,Position& out){Position p;Reader r(b,len);Field f;while(r.next(f)){if(f.tag==1&&f.wire==5){p.lat=int32_t(f.value);p.hasLat=true;}if(f.tag==2&&f.wire==5){p.lon=int32_t(f.value);p.hasLon=true;}if(f.tag==4&&f.wire==5)p.epoch=f.value;if(f.tag==5&&f.wire==0)p.source=f.value;if(f.tag==7&&f.wire==5&&f.value)p.epoch=f.value;}if(!r.ok())return false;out=p;return true;}
struct Packet {uint32_t from=0,to=0,id=0,requestId=0,epoch=0;int32_t rssi=0;float snr=0;unsigned hopStart=0,hopLimit=0,relay=0,port=0,transport=0;bool hasRssi=false,hasSnr=false,hasHopStart=false,mqtt=false;const uint8_t* payload=nullptr;size_t payloadSize=0;};
inline bool packet(const uint8_t* b,size_t len,Packet& out){Packet p;Reader r(b,len);Field f;while(r.next(f)){
 if(f.tag==1&&f.wire==5)p.from=f.value;if(f.tag==2&&f.wire==5)p.to=f.value;if(f.tag==6&&f.wire==5)p.id=f.value;if(f.tag==7&&f.wire==5)p.epoch=f.value;
 if(f.tag==8&&f.wire==5){p.snr=asFloat(f.value);p.hasSnr=true;}if(f.tag==12&&f.wire==0){p.rssi=int32_t(f.value);p.hasRssi=true;}
 if(f.tag==9&&f.wire==0)p.hopLimit=f.value;if(f.tag==15&&f.wire==0){p.hopStart=f.value;p.hasHopStart=true;}
 if(f.tag==14&&f.wire==0)p.mqtt=f.value;if(f.tag==19&&f.wire==0)p.relay=f.value;if(f.tag==21&&f.wire==0)p.transport=f.value;
 if(f.tag==4&&f.wire==2){Reader d(f.bytes,f.size);Field a;while(d.next(a)){if(a.tag==1&&a.wire==0)p.port=a.value;if(a.tag==2&&a.wire==2){p.payload=a.bytes;p.payloadSize=a.size;}if(a.tag==6&&a.wire==5)p.requestId=a.value;}if(!d.ok())return false;}
 }if(!r.ok())return false;out=p;return true;}
// Routing payload (ROUTING_APP): returns false on malformed data. err=0 means ACK (Routing.Error NONE);
// traceroute variants (fields 1,2) are not an ACK and set isAck=false.
inline bool routing(const uint8_t* b,size_t len,unsigned& err,bool& isAck){Reader r(b,len);Field f;err=0;isAck=true;while(r.next(f)){if(f.tag==3&&f.wire==0)err=unsigned(f.value);if((f.tag==1||f.tag==2)&&f.wire==2)isAck=false;}return r.ok();}
// ToRadio{packet=MeshPacket{to,decoded=Data{portnum,payload"MZ"+seq},id,want_ack}}: direct test packet that asks the
// destination for a routing ACK. portnum may exceed 127 (varint); hopLimit 0 keeps the node default. Returns length or 0.
inline size_t buildProbe(uint8_t* out,size_t cap,uint32_t to,uint32_t id,uint16_t seq,unsigned port,unsigned channel,unsigned hopLimit){
 if(cap<64||!to||!id)return 0;uint8_t d[16];size_t dl=0;
 d[dl++]=0x08;{uint32_t v=port;do{d[dl++]=(v&127)|(v>127?128:0);v>>=7;}while(v);}
 d[dl++]=0x12;d[dl++]=4;d[dl++]='M';d[dl++]='Z';d[dl++]=uint8_t(seq>>8);d[dl++]=uint8_t(seq);
 uint8_t m[48];size_t n=0;
 m[n++]=0x15;for(unsigned i=0;i<4;i++)m[n++]=uint8_t(to>>(8*i));
 if(channel&&channel<8){m[n++]=0x18;m[n++]=uint8_t(channel);}
 m[n++]=0x22;m[n++]=uint8_t(dl);memcpy(m+n,d,dl);n+=dl;
 m[n++]=0x35;for(unsigned i=0;i<4;i++)m[n++]=uint8_t(id>>(8*i));
 if(hopLimit&&hopLimit<8){m[n++]=0x48;m[n++]=uint8_t(hopLimit);}
 m[n++]=0x50;m[n++]=1;
 size_t k=0;out[k++]=0x0a;out[k++]=uint8_t(n);memcpy(out+k,m,n);return k+n;}
// ---- Pulpit: telemetria lokalnego wezla, konfiguracja LoRa, kanaly ----
// DeviceMetrics (Telemetry.device_metrics lub NodeInfo.device_metrics): aktualizuje tylko pola obecne w wiadomosci.
// battery 101 = zasilanie zewnetrzne (USB).
struct DeviceMetrics{bool has=false;int battery=-1;float voltage=0,chUtil=0,airTx=0;uint32_t uptime=0;};
inline bool deviceMetrics(const uint8_t* b,size_t len,DeviceMetrics& out){DeviceMetrics d=out;Reader r(b,len);Field f;
 while(r.next(f)){if(f.tag==1&&f.wire==0&&f.value<=150)d.battery=int(f.value);if(f.tag==2&&f.wire==5){float v=asFloat(f.value);if(v>=0&&v<20)d.voltage=v;}
  if(f.tag==3&&f.wire==5){float v=asFloat(f.value);if(v>=0&&v<=100)d.chUtil=v;}if(f.tag==4&&f.wire==5){float v=asFloat(f.value);if(v>=0&&v<=100)d.airTx=v;}
  if(f.tag==5&&f.wire==0)d.uptime=uint32_t(f.value);}
 if(!r.ok())return false;d.has=true;out=d;return true;}
// Telemetry (port 67): time=1, device_metrics=2. Inne warianty (srodowisko, zasilanie) sa pomijane i zwracaja false.
inline bool telemetry(const uint8_t* b,size_t len,DeviceMetrics& out){Reader r(b,len);Field f;while(r.next(f))if(f.tag==2&&f.wire==2)return deviceMetrics(f.bytes,f.size,out);return false;}
struct LoraCfg{bool has=false,usePreset=false;unsigned preset=0,bw=0,sf=0,region=0,channelNum=0;float freqOffset=0,overrideFreq=0;};
// FromRadio.config (Config): lora=6 {use_preset=1,modem_preset=2,bandwidth=3,spread_factor=4,region=7,channel_num=11,override_frequency=14,frequency_offset=6}.
inline bool loraConfig(const uint8_t* b,size_t len,LoraCfg& out){Reader r(b,len);Field f;const uint8_t* l=nullptr;size_t ll=0;bool found=false;
 while(r.next(f))if(f.tag==6&&f.wire==2){l=f.bytes;ll=f.size;found=true;}
 if(!r.ok()||!found)return false;LoraCfg c;Reader q(l,ll);while(q.next(f)){if(f.tag==1&&f.wire==0)c.usePreset=f.value!=0;if(f.tag==2&&f.wire==0)c.preset=unsigned(f.value);if(f.tag==3&&f.wire==0)c.bw=unsigned(f.value);
  if(f.tag==4&&f.wire==0)c.sf=unsigned(f.value);if(f.tag==6&&f.wire==5)c.freqOffset=asFloat(f.value);if(f.tag==7&&f.wire==0)c.region=unsigned(f.value);if(f.tag==11&&f.wire==0)c.channelNum=unsigned(f.value);if(f.tag==14&&f.wire==5)c.overrideFreq=asFloat(f.value);}
 if(!q.ok())return false;c.has=true;out=c;return true;}
inline const char* presetName(unsigned p){static const char* n[]={"LongFast","LongSlow","VLongSlow","MediumSlow","MediumFast","ShortSlow","ShortFast","LongMod","ShortTurbo"};return p<9?n[p]:"?";}
inline const char* regionName(unsigned r){static const char* n[]={"UNSET","US","EU433","EU868","CN","JP","ANZ","KR","TW","RU","IN","NZ865","TH","LORA24","UA433","UA868","MY433","MY919","SG923"};return r<19?n[r]:"?";}
// Parametry modemu (SF, BW w kHz) i czestotliwosc srodkowa kanalu w MHz. Czestotliwosc tylko dla regionow US, EU433, EU868, ANZ
// (lub gdy ustawiono override_frequency); w pozostalych mhz=0. channelName = nazwa kanalu glownego ("" = nazwa presetu).
struct Modem{unsigned sf=0;float bwKhz=0;float mhz=0;};
inline bool modem(const LoraCfg& c,const char* channelName,Modem& m){
 static const unsigned sf[]={11,12,12,10,9,8,7,11,7};static const float bw[]={250,125,62.5f,250,250,250,250,125,500};
 Modem o;if(c.usePreset){if(c.preset>=9)return false;o.sf=sf[c.preset];o.bwKhz=bw[c.preset];}else{if(!c.bw||!c.sf)return false;o.sf=c.sf;o.bwKhz=c.bw==62?62.5f:float(c.bw);}
 if(c.overrideFreq>0){o.mhz=c.overrideFreq+c.freqOffset;m=o;return true;}
 float start=0,end=0;switch(c.region){case 1:start=902;end=928;break;case 2:start=433;end=434;break;case 3:start=869.4f;end=869.65f;break;case 6:start=915;end=928;break;default:break;}
 if(end>start){const char* nm=(channelName&&channelName[0])?channelName:presetName(c.preset);unsigned n=unsigned((end-start)/(o.bwKhz/1000.f)+0.0001f);if(!n)n=1;
  uint32_t h=5381;for(const char* p=nm;*p;p++)h=((h<<5)+h)+uint8_t(*p);unsigned slot=c.channelNum?(c.channelNum-1)%n:h%n;
  o.mhz=start+o.bwKhz/2000.f+slot*o.bwKhz/1000.f+c.freqOffset;}
 m=o;return true;}
// FromRadio.channel (Channel): index=1, settings=2{name=3}, role=3 (1=PRIMARY, 2=SECONDARY).
struct ChannelInfo{unsigned index=0,role=0;char name[24]={};};
inline bool channel(const uint8_t* b,size_t len,ChannelInfo& out){ChannelInfo c;Reader r(b,len);Field f;while(r.next(f)){if(f.tag==1&&f.wire==0)c.index=unsigned(f.value);if(f.tag==3&&f.wire==0)c.role=unsigned(f.value);
 if(f.tag==2&&f.wire==2){Reader s(f.bytes,f.size);Field g;while(s.next(g))if(g.tag==3&&g.wire==2)copyText(c.name,sizeof c.name,g);if(!s.ok())return false;}}
 if(!r.ok()||c.index>7)return false;out=c;return true;}
// Streaming frame parser: 94 C3, big-endian length, then protobuf.
class Framer {unsigned state=0;size_t length=0,used=0;uint8_t buf[512];uint32_t last=0;
public: unsigned errors=0;
void reset(){state=0;length=used=0;}
template<class Callback>void feed(uint8_t b,uint32_t now,Callback cb){if(state&&uint32_t(now-last)>1500){++errors;reset();}last=now;
 switch(state){case 0:if(b==0x94)state=1;break;case 1:if(b==0xc3)state=2;else state=b==0x94?1:0;break;case 2:length=size_t(b)<<8;state=3;break;case 3:length|=b;if(!length||length>sizeof(buf)){++errors;reset();}else{used=0;state=4;}break;case 4:buf[used++]=b;if(used==length){cb(buf,length);reset();}break;}}
};
}
