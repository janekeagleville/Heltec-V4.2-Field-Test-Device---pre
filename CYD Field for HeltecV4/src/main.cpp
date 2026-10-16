#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <Preferences.h>
#include <math.h>
#include "config.h"
#include "protocol.h"
#include "spectrum.h"
#include "geo.h"
#include "maphires.h"
#include "mapwaw.h"
#include "splash.h"

TFT_eSPI tft; HardwareSerial radio(2); mesh::Framer framing; int uiScreen=-1;bool dirty=false;
void sendFrame(const uint8_t* p,size_t len);
// Aktywny test lacza: pakiet bezposredni do wybranego wezla z zadaniem ACK; RSSI/SNR to parametry ACK odebranego przez Trackera.
struct Probe{bool on=true,pending=false;uint32_t node=0,id=0,sentAt=0,nextAt=0,ok=0,lost=0,seq=0,rttSum=0;int32_t rtt=-1;};
Probe probe;
struct Node {uint32_t id=0,last=0,count=0;char name[40]={};int rssi=0;float snr=0;int lo=0,hi=0;double sum=0;int16_t hist[HISTORY]={};unsigned used=0,head=0;bool mqtt=false;int hops=-1;int32_t lat=0,lon=0;bool hasPos=false;};
Node nodes[MAX_NODES];uint32_t rxBytes=0,frames=0,baudNow=RADIO_BAUD,baudSince=0;uint32_t myId=0,configId=0,lastFrame=0,lastRequest=0,lastHeartbeat=0,lastDraw=0,lastFlush=0;
bool configured=false,everFrame=false,logging=false,sdReady=false,detail=false;unsigned page=0;int selected=-1;
File csv;char logPath[64]={};uint32_t logNode=0,clkEpoch=0,clkAt=0,uiMsgUntil=0;const char* uiMsg="";uint32_t written=0,parseErrors=0,logErrors=0;mesh::Position ownPosition;bool ownPositionLive=false;uint32_t positionAt=0;
mesh::DeviceMetrics dev;uint32_t devAt=0;mesh::LoraCfg lora;mesh::PosCfg posCfg;uint32_t gpsAt=0;mesh::ChannelInfo chans[8];bool home=true,specScr=false,mapScr=false;int mapSel=-1;uint32_t mapSelId=0;unsigned mapPage=0,mapPages=1;const int ZL[]={1,2,3,4,6,8,12},ZN=7,MAP_ZOOM_MAX=12,MAPW_ZOOM_MIN=6;int mapZoom=1;float mapCX=-1,mapCY=-1;unsigned specWin=0;spec::SampleRing specRing;spec::UtilRing utilRing;uint32_t utilAt=0;uint32_t chatCount=0;
// Wiadomosci tekstowe: pierscien 40 ostatnich w RAM (kanal 0-7, 8 = wiadomosc bezposrednia do tego noda). unread = nieprzeczytane, fresh = podswietlone w oknie czatu.
struct Msg{uint32_t from=0,at=0;uint8_t ch=0;bool unread=false,fresh=false;char text[100]={};};
const int MSGN=40;Msg msgs[MSGN];unsigned msgHead=0,msgCnt=0;bool chatScr=false;int chatCh=0;unsigned chatPage=0;
bool invertScr=false;Preferences prefs;uint16_t calX0=300,calX1=3800,calY0=300,calY1=3800;bool calSwap=true;
Node* node(uint32_t id){if(!id)return nullptr;for(auto &n:nodes)if(n.id==id)return &n;for(auto &n:nodes)if(!n.id){n=Node{};n.id=id;return &n;}Node* oldest=&nodes[0];for(auto &n:nodes)if(uint32_t(millis()-n.last)>uint32_t(millis()-oldest->last))oldest=&n;*oldest=Node{};oldest->id=id;return oldest;}
void user(Node* n,const uint8_t* p,size_t len){if(!n)return;mesh::Reader r(p,len);mesh::Field f;char name[40]={},shortName[12]={};while(r.next(f)){if(f.tag==2&&f.wire==2)mesh::copyText(name,sizeof(name),f);if(f.tag==3&&f.wire==2)mesh::copyText(shortName,sizeof(shortName),f);}if(r.ok()){if(name[0])strlcpy(n->name,name,sizeof(n->name));else if(shortName[0])strlcpy(n->name,shortName,sizeof(n->name));}}
void updatePosition(const uint8_t* p,size_t len,bool live){mesh::Position pos;if(mesh::position(p,len,pos)&&pos.valid()){ownPosition=pos;positionAt=millis();ownPositionLive=live;}}
void nodeInfo(const uint8_t* p,size_t len){mesh::Reader r(p,len);mesh::Field f;uint32_t id=0;const uint8_t *u=nullptr,*pos=nullptr;size_t ul=0,pl=0;mesh::Field dm;while(r.next(f)){if(f.tag==1&&f.wire==0)id=f.value;if(f.tag==2&&f.wire==2){u=f.bytes;ul=f.size;}if(f.tag==3&&f.wire==2){pos=f.bytes;pl=f.size;}if(f.tag==6&&f.wire==2)dm=f;}if(!r.ok()){++parseErrors;return;}Node* n=node(id);if(id==myId&&dm.bytes&&mesh::deviceMetrics(dm.bytes,dm.size,dev))devAt=millis();if(u)user(n,u,ul);if(pos&&n){mesh::Position pp;if(mesh::position(pos,pl,pp)&&pp.valid()&&(pp.lat||pp.lon)){n->lat=pp.lat;n->lon=pp.lon;n->hasPos=true;}}if(id==myId&&pos)updatePosition(pos,pl,configured);}
void clockSeen(uint32_t e){if(e>1700000000UL&&e<4000000000UL){clkEpoch=e;clkAt=millis();}}
bool clockNow(uint32_t& e){if(!clkEpoch)return false;e=clkEpoch+(millis()-clkAt)/1000;return true;}
void chatAdd(uint32_t from,unsigned ch,const uint8_t* p,size_t n){char raw[200]={};size_t k=n<sizeof(raw)-1?n:sizeof(raw)-1;memcpy(raw,p,k);Msg* m;if(msgCnt<MSGN)m=&msgs[(msgHead+msgCnt++)%MSGN];else{m=&msgs[msgHead];msgHead=(msgHead+1)%MSGN;}
 *m=Msg{};m->from=from;m->at=millis();m->ch=uint8_t(ch);mesh::displayName(raw,m->text,sizeof m->text);if(!m->text[0])strlcpy(m->text,"(no text)",sizeof m->text);bool open=chatScr&&chatCh==int(ch);m->unread=!open;m->fresh=true;++chatCount;if(open)chatPage=0;}
unsigned unreadN(unsigned ch){unsigned c=0;for(unsigned i=0;i<msgCnt;i++){const Msg& m=msgs[(msgHead+i)%MSGN];if(m.ch==ch&&m.unread)c++;}return c;}
unsigned chatTotal(unsigned ch){unsigned c=0;for(unsigned i=0;i<msgCnt;i++)if(msgs[(msgHead+i)%MSGN].ch==ch)c++;return c;}
// Lista kanalow do pokazania: skonfigurowane na nodzie lub z wiadomosciami, plus DM (8) gdy cos przyszlo.
unsigned chList(unsigned* o){unsigned n=0;for(unsigned c=0;c<9;c++){bool has=chatTotal(c)>0;if(c==8?has:(chans[c].role||chans[c].name[0]||has))o[n++]=c;}return n;}
String chName(unsigned ch){char b[32]={};if(ch==8)return String("DIRECT MSG");if(chans[ch].name[0])mesh::displayName(chans[ch].name,b,sizeof b);else if(ch==0&&lora.has)strlcpy(b,mesh::presetName(lora.preset),sizeof b);if(!b[0])snprintf(b,sizeof b,"CH %u",ch);return String(b);}
void openChat(unsigned ch){chatScr=true;home=false;chatCh=int(ch);chatPage=0;for(unsigned i=0;i<msgCnt;i++){Msg& m=msgs[(msgHead+i)%MSGN];if(m.ch==ch)m.unread=false;}uiScreen=-1;}
void leaveSub(){if(chatScr)for(unsigned i=0;i<msgCnt;i++)msgs[(msgHead+i)%MSGN].fresh=false;specScr=false;mapScr=false;chatScr=false;home=true;}
// Dotkniecie kafelka CHAT: wiersz kanalu otwiera jego wiadomosci, naglowek kafelka otwiera kanal z najnowsza wiadomoscia.
void chatTap(int y){unsigned o[9];unsigned n=chList(o);if(!n)return;if(n>5)n=5;int idx=(y-146)/12;if(y<146)idx=-1;else if(idx>=int(n))idx=int(n)-1;unsigned ch=o[idx<0?0:idx];if(idx<0&&msgCnt)ch=msgs[(msgHead+msgCnt-1)%MSGN].ch;openChat(ch);}
void flash(const char* m,uint32_t ms=3500){uiMsg=m;uiMsgUntil=millis()+ms;}
void logPacket(const mesh::Packet& p,const Node& n,int32_t rtt){if(!logging||!csv)return;if(logNode&&p.from!=logNode)return;char lat[24]="",lon[24]="",age[24]="",rttText[16]="";if(rtt>=0)snprintf(rttText,sizeof(rttText),"%ld",(long)rtt);bool valid=ownPosition.valid();if(valid){snprintf(lat,sizeof(lat),"%.7f",ownPosition.lat*1e-7);snprintf(lon,sizeof(lon),"%.7f",ownPosition.lon*1e-7);snprintf(age,sizeof(age),"%lu",(unsigned long)((millis()-positionAt)/1000));}
 // GPS coordinates belong to the local Tracker, never the remote sender.
 char gpsAge[24]="";if(valid&&ownPosition.epoch&&p.epoch>=ownPosition.epoch)snprintf(gpsAge,sizeof(gpsAge),"%lu",(unsigned long)(p.epoch-ownPosition.epoch));
 size_t bytes=csv.printf("%lu,%lu,%08lx,%08lx,%d,%.2f,%d,%u,%u,%u,%s,%s,%s,%u,%u,%lu,%s,%s\n",(unsigned long)millis(),(unsigned long)p.epoch,(unsigned long)p.from,(unsigned long)p.id,p.rssi,p.snr,n.hops,p.relay,p.mqtt,p.transport,lat,lon,age,valid&&ownPositionLive,valid?ownPosition.source:0,(unsigned long)(valid?ownPosition.epoch:0),gpsAge,rttText);
 if(!bytes||csv.getWriteError()){++logErrors;logging=false;csv.close();flash("SD ERROR");}else ++written;}
void probeLost(){probe.pending=false;++probe.lost;probe.nextAt=max<uint32_t>(probe.sentAt+PROBE_INTERVAL_MS,uint32_t(millis())+2000);dirty=true;}
void packet(const uint8_t* p,size_t len){mesh::Packet m;if(!mesh::packet(p,len,m)){++parseErrors;return;}clockSeen(m.epoch);int32_t rtt=-1;
 // Lokalny Tracker zglasza bledy kolejki/retransmisji; potwierdzenie "przyjeto do wyslania" (NONE) jest pomijane.
 if(m.from==myId){if(m.port==3&&m.payload)updatePosition(m.payload,m.payloadSize,true);if(m.port==67&&m.payload&&mesh::telemetry(m.payload,m.payloadSize,dev))devAt=millis();
  if(m.port==5&&m.payload&&probe.pending&&m.requestId==probe.id){unsigned e;bool ack;if(mesh::routing(m.payload,m.payloadSize,e,ack)&&ack&&e!=0)probeLost();}return;}
 // ACK z wezla docelowego: wynik testu (RSSI/SNR liczy sie jak kazdy odebrany pakiet nizej).
 if(m.port==5&&m.payload&&probe.pending&&m.from==probe.node&&m.requestId==probe.id){unsigned e;bool ack;if(mesh::routing(m.payload,m.payloadSize,e,ack)&&ack){
   if(e==0){rtt=int32_t(millis()-probe.sentAt);probe.rtt=rtt;probe.rttSum+=rtt;++probe.ok;probe.pending=false;probe.nextAt=max<uint32_t>(probe.sentAt+PROBE_INTERVAL_MS,uint32_t(millis())+2000);dirty=true;}else probeLost();}}
 if(m.port==4&&m.payload)user(node(m.from),m.payload,m.payloadSize);
 if(m.port==3&&m.payload){Node* c=node(m.from);mesh::Position pp;if(c&&mesh::position(m.payload,m.payloadSize,pp)&&pp.valid()&&(pp.lat||pp.lon)){c->lat=pp.lat;c->lon=pp.lon;c->hasPos=true;dirty=true;}}
 if(m.port==1&&m.payload&&m.payloadSize&&(m.to==0xFFFFFFFFu||!myId||m.to==myId)){chatAdd(m.from,(m.to!=0xFFFFFFFFu&&myId&&m.to==myId)?8:(m.channel<8?m.channel:0),m.payload,m.payloadSize);dirty=true;}
 // Exclude local transmissions, replay without RSSI and MQTT ingress.
 if(!m.hasRssi||m.mqtt||m.transport>4||!m.from||!isfinite(m.snr)||m.rssi < -200||m.rssi>20)return;
 Node* n=node(m.from);if(!n)return;n->last=millis();n->rssi=m.rssi;n->snr=m.snr;n->mqtt=m.mqtt;
 // Nonzero initial hop count can be interpreted; zero is ambiguous on old firmware.
 n->hops=m.hasHopStart&&m.hopStart>0&&m.hopStart>=m.hopLimit?int(m.hopStart-m.hopLimit):-1;
 if(!n->count)n->lo=n->hi=m.rssi;n->lo=min(n->lo,int(m.rssi));n->hi=max(n->hi,int(m.rssi));++n->count;n->sum+=m.rssi;
 if(m.hasSnr)specRing.add(millis(),m.rssi,m.snr);n->hist[n->head]=m.rssi;n->head=(n->head+1)%HISTORY;n->used=min(n->used+1,HISTORY);logPacket(m,*n,rtt);if(detail&&selected>=0&&n==&nodes[selected])dirty=true;
}
void fromRadio(const uint8_t* p,size_t len){mesh::Reader r(p,len);mesh::Field f;
 // Validate whole envelope before interpreting any nested data.
 while(r.next(f)){}if(!r.ok()){++parseErrors;return;}
 lastFrame=millis();everFrame=true;++frames;mesh::Reader data(p,len);while(data.next(f)){
 if(f.tag==2&&f.wire==2)packet(f.bytes,f.size);
 if(f.tag==3&&f.wire==2){mesh::Reader info(f.bytes,f.size);mesh::Field a;uint32_t id=0;while(info.next(a))if(a.tag==1&&a.wire==0)id=a.value;if(info.ok()&&id){if(myId&&myId!=id){ownPosition=mesh::Position{};ownPositionLive=false;}myId=id;}}
 if(f.tag==4&&f.wire==2)nodeInfo(f.bytes,f.size);
 if(f.tag==5&&f.wire==2){mesh::LoraCfg c;if(mesh::loraConfig(f.bytes,f.size,c))lora=c;mesh::PosCfg pc;if(mesh::posConfig(f.bytes,f.size,pc))posCfg=pc;}
 if(f.tag==10&&f.wire==2){mesh::ChannelInfo c;if(mesh::channel(f.bytes,f.size,c))chans[c.index]=c;}
 if(f.tag==7&&f.wire==0&&f.value==configId)configured=true;
 if(f.tag==8&&f.wire==0&&f.value){configured=false;ownPositionLive=false;ownPosition=mesh::Position{};lastRequest=0;}
}}
void sendFrame(const uint8_t* p,size_t len){uint8_t head[]={0x94,0xc3,uint8_t(len>>8),uint8_t(len)};radio.write(head,4);radio.write(p,len);}
// Wlacza/wylacza GPS w nodzie (Config.position.gps_mode). Wezel zapisuje konfiguracje i restartuje sie; potem odswiezamy dane z Trackera.
void gpsSet(unsigned mode){if(!configured||!myId){flash("NO LINK");return;}if(!posCfg.has){flash("NO GPS CFG");return;}uint8_t buf[200];uint32_t id=esp_random();if(!id)id=1;size_t n=mesh::buildGpsMode(buf,sizeof buf,myId,id,posCfg,mode);if(!n){flash("GPS CFG ERR");return;}
 sendFrame(buf,n);posCfg.mode=mode;gpsAt=millis();flash(mode?"GPS ON SENT":"GPS OFF SENT",6000);dirty=true;}
void probeSend(uint32_t now){uint8_t buf[96];uint32_t id=esp_random();if(!id)id=1;size_t len=mesh::buildProbe(buf,sizeof buf,probe.node,id,uint16_t(probe.seq),PROBE_PORT,PROBE_CHANNEL,PROBE_HOP_LIMIT);if(!len)return;
 sendFrame(buf,len);probe.id=id;probe.sentAt=now;probe.pending=true;++probe.seq;dirty=true;}
void probeStart(uint32_t nodeId){probe=Probe{};probe.node=nodeId;probe.nextAt=millis();dirty=true;}
void probeTick(uint32_t now){if(!detail||selected<0||!probe.node)return;
 if(probe.pending&&now-probe.sentAt>PROBE_TIMEOUT_MS)probeLost();
 if(probe.on&&configured&&!probe.pending&&int32_t(now-probe.nextAt)>=0)probeSend(now);}
void requestConfig(){configId=esp_random();if(!configId)configId=1;uint8_t p[6];size_t len=0;p[len++]=0x18;uint32_t v=configId;do{p[len++]=(v&127)|(v>127?128:0);v>>=7;}while(v);sendFrame(p,len);lastRequest=millis();}
// Meshtastic serial defaults to 38400 baud when serial.baud is not set; until the first valid frame arrives, cycle through common speeds.
void nextBaud(uint32_t now){static const uint32_t b[]={115200,38400,9600,57600};unsigned i=0;for(unsigned k=0;k<4;k++)if(b[k]==baudNow)i=k;baudNow=b[(i+1)%4];radio.end();radio.setRxBufferSize(4096);radio.begin(baudNow,SERIAL_8N1,RADIO_RX,RADIO_TX);baudSince=now;requestConfig();}
// Karta SD: HSPI zajmuje ekran, SD ma wlasne VSPI; przy bledzie inicjalizacji probuje nizszych taktowan (4 MHz, 1 MHz).
bool sdMount(){static bool spiUp=false;if(spiUp)SPI.end();SPI.begin(18,19,23,SD_CS);spiUp=true;static const uint32_t hz[]={10000000,4000000,1000000,400000};for(unsigned i=0;i<4;i++){SD.end();bool ok=SD.begin(SD_CS,SPI,hz[i]);uint8_t t=ok?SD.cardType():CARD_NONE;Serial.printf("SD: %lu Hz begin=%d typ=%u\n",(unsigned long)hz[i],ok?1:0,(unsigned)t);if(ok&&t!=CARD_NONE){Serial.printf("SD: OK, %llu MB\n",(unsigned long long)(SD.cardSize()/1048576ULL));return true;}}SD.end();return false;}
// Nazwa pliku: "<node name>_DDMMYY_HH-MM-SS.csv" (FAT nie dopuszcza ':', wiec godzina ma myslniki).
void logStem(uint32_t id,char* out,size_t cap){char base[40]="ALL";if(id){Node* n=node(id);char tmp[48]="";size_t k=n&&n->name[0]?mesh::displayName(n->name,tmp,sizeof(tmp)):0;if(!k)snprintf(tmp,sizeof(tmp),"!%08lx",(unsigned long)id);char safe[40];if(!mesh::fileSafe(tmp,safe,24))snprintf(safe,sizeof(safe),"!%08lx",(unsigned long)id);strlcpy(base,safe,sizeof(base));}
 uint32_t e;if(clockNow(e)){mesh::Civil c=mesh::civilLocal(e);snprintf(out,cap,"%s_%02u%02u%02u_%02u-%02u-%02u",base,c.d,c.mo,unsigned(c.y%100),c.h,c.mi,c.s);}
 else{uint32_t q=prefs.getULong("logseq",0)+1;prefs.putULong("logseq",q);snprintf(out,cap,"%s_NO-TIME_%03lu",base,(unsigned long)(q%1000));}}
void stopLog(){if(csv){csv.flush();csv.close();}logging=false;}
void startLog(uint32_t id){if(!sdReady)sdReady=sdMount();if(!sdReady){++logErrors;flash("NO SD");return;}
 char stem[56];logStem(id,stem,sizeof(stem));snprintf(logPath,sizeof(logPath),"/%s.csv",stem);for(unsigned i=2;i<100&&SD.exists(logPath);i++)snprintf(logPath,sizeof(logPath),"/%s-%u.csv",stem,i);
 csv=SD.open(logPath,FILE_WRITE);
 if(!csv){for(unsigned i=1;i<10000;i++){snprintf(logPath,sizeof(logPath),"/LOG%05u.csv",i);if(!SD.exists(logPath)){csv=SD.open(logPath,FILE_WRITE);break;}}if(csv)flash("8.3 NAME");}
 if(!csv){++logErrors;sdReady=false;flash("SD ERROR");return;}
 csv.clearWriteError();csv.println("uptime_ms,rx_epoch,sender_id,packet_id,rssi_dbm,snr_db,hops,relay_low_byte,via_mqtt,transport,tracker_lat,tracker_lon,position_receipt_age_s,position_live,position_source,position_epoch,gps_age_s,probe_rtt_ms");
 if(csv.getWriteError()){++logErrors;csv.close();flash("SD ERROR");return;}
 logging=true;logNode=id;written=0;lastFlush=millis();Serial.printf("LOG: %s\n",logPath);}
// ZAPIS: na ekranie pracy zapisuje wybrany node, na liscie - caly ruch (plik "ALL_...").
void toggleLog(){if(logging){stopLog();return;}startLog(detail&&selected>=0?nodes[selected].id:0);}
// Bit-banged XPT2046 uses its own pins, leaving TFT HSPI and SD VSPI independent.
uint16_t touchADC(uint8_t cmd){digitalWrite(TOUCH_CS,LOW);for(int b=7;b>=0;b--){digitalWrite(TOUCH_MOSI,(cmd>>b)&1);digitalWrite(TOUCH_CLK,HIGH);delayMicroseconds(2);digitalWrite(TOUCH_CLK,LOW);}delayMicroseconds(8);uint16_t v=0;for(int b=0;b<16;b++){digitalWrite(TOUCH_CLK,HIGH);delayMicroseconds(2);v=(v<<1)|digitalRead(TOUCH_MISO);digitalWrite(TOUCH_CLK,LOW);}digitalWrite(TOUCH_CS,HIGH);return (v>>3)&4095;}
bool rawTouch(uint16_t& x,uint16_t& y){uint16_t z=touchADC(0xb0);if(z<150)return false;uint32_t a=0,b=0;for(int i=0;i<5;i++){a+=touchADC(0xd0);b+=touchADC(0x90);}x=a/5;y=b/5;return x>50&&x<4050&&y>50&&y<4050;}
bool touch(int& x,int& y){uint16_t a,b;if(!rawTouch(a,b))return false;int u=calSwap?b:a,v=calSwap?a:b;x=constrain(map(u,calX0,calX1,0,319),0L,319L);y=constrain(map(v,calY0,calY1,0,239),0L,239L);return true;}
void calibrate(){uiScreen=-1;uint16_t xs[4],ys[4];const int cx[]={25,295,295,25},cy[]={25,25,215,215};
 for(int i=0;i<4;i++){tft.fillScreen(TFT_BLACK);tft.setTextColor(TFT_WHITE,TFT_BLACK);tft.drawString("Touch the crosshair",80,100,2);tft.drawLine(cx[i]-10,cy[i],cx[i]+10,cy[i],TFT_YELLOW);tft.drawLine(cx[i],cy[i]-10,cx[i],cy[i]+10,TFT_YELLOW);uint16_t a,b;while(rawTouch(a,b)){delay(10);}while(!rawTouch(a,b)){while(radio.available())framing.feed(radio.read(),millis(),fromRadio);delay(5);}xs[i]=a;ys[i]=b;delay(250);}
 calSwap=abs(int(ys[1])-int(ys[0]))>abs(int(xs[1])-int(xs[0]));int lx=calSwap?(ys[0]+ys[3])/2:(xs[0]+xs[3])/2;int rx=calSwap?(ys[1]+ys[2])/2:(xs[1]+xs[2])/2;int ty=calSwap?(xs[0]+xs[1])/2:(ys[0]+ys[1])/2;int by=calSwap?(xs[2]+xs[3])/2:(ys[2]+ys[3])/2;
 calX0=constrain(lx-(rx-lx)*25/270,0,4095);calX1=constrain(lx+(rx-lx)*294/270,0,4095);calY0=constrain(ty-(by-ty)*25/190,0,4095);calY1=constrain(ty+(by-ty)*214/190,0,4095);
 if(abs(int(calX1)-int(calX0))<200||abs(int(calY1)-int(calY0))<200){calX0=calY0=300;calX1=calY1=3800;return;}
 prefs.putUShort("x0",calX0);prefs.putUShort("x1",calX1);prefs.putUShort("y0",calY0);prefs.putUShort("y1",calY1);prefs.putBool("swap",calSwap);prefs.putBool("valid",true);
}
unsigned order[MAX_NODES],orderCount=0;void sortNodes(){orderCount=0;for(unsigned i=0;i<MAX_NODES;i++)if(nodes[i].count)order[orderCount++]=i;for(unsigned i=1;i<orderCount;i++){unsigned v=order[i],j=i;while(j&&uint32_t(millis()-nodes[v].last)<uint32_t(millis()-nodes[order[j-1]].last)){order[j]=order[j-1];--j;}order[j]=v;}if(page*5>=orderCount)page=0;}
// ===================== UI 320x240 (poziomo) =====================
#define C565(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))
const uint16_t cBG=C565(0,34,22),cHDR=C565(0,18,12),cPANEL=C565(6,56,40),cLINE=C565(30,170,120),cTRACK=C565(14,84,60),
 cTURQ=C565(0,255,224),cGREEN=C565(30,255,120),cAMBER=C565(255,190,0),cRED=C565(255,56,44),cDIM=C565(140,230,200),cWHITE=0xFFFF,
 cOKBG=C565(4,78,46),cWARNBG=C565(80,54,0),cBADBG=C565(86,14,10),cGREY=C565(170,190,182);
enum Quality{Q_NONE,Q_GOOD,Q_FAIR,Q_BAD};
// Ocena ostatniego odcinka radiowego (progi w config.h).
Quality quality(const Node& n){if(!n.count)return Q_NONE;if(n.snr>=SNR_GOOD&&n.rssi>=RSSI_GOOD)return Q_GOOD;if(n.snr>=SNR_FAIR&&n.rssi>=RSSI_FAIR)return Q_FAIR;return Q_BAD;}
uint16_t qColor(Quality q){return q==Q_GOOD?cGREEN:q==Q_FAIR?cAMBER:q==Q_BAD?cRED:cGREY;}
uint16_t qBg(Quality q){return q==Q_GOOD?cOKBG:q==Q_FAIR?cWARNBG:q==Q_BAD?cBADBG:cPANEL;}
const char* qText(Quality q){return q==Q_GOOD?"READABLE":q==Q_FAIR?"MARGINAL":q==Q_BAD?"UNREADABLE":"NO DATA";}
uint16_t gradient(float t){static const float stop[]={0,.45f,.8f,1};static const uint8_t c[4][3]={{255,56,44},{255,190,0},{30,255,120},{0,255,224}};
 t=constrain(t,0.f,1.f);int i=0;while(i<2&&t>stop[i+1])++i;float u=(t-stop[i])/(stop[i+1]-stop[i]);uint8_t k[3];for(int j=0;j<3;j++)k[j]=c[i][j]+(c[i+1][j]-c[i][j])*u;return C565(k[0],k[1],k[2]);}
void txt(const String& s,int x,int y,int font,uint16_t fg,uint16_t bg,uint8_t datum=TL_DATUM){tft.setTextDatum(datum);tft.setTextColor(fg,bg);tft.drawString(s,x,y,font);tft.setTextDatum(TL_DATUM);}
// Name fitted to a pixel width (font 2): transliterated to ASCII, cut with ".." so it never overlaps neighbouring columns.
String fitName(const Node& n,int maxW){char buf[48];size_t len=n.name[0]?mesh::displayName(n.name,buf,sizeof(buf)):0;if(!len)snprintf(buf,sizeof(buf),"!%08lx",(unsigned long)n.id);
 String s(buf);if(tft.textWidth(s,2)<=maxW)return s;while(s.length()>1){s.remove(s.length()-1,1);String t=s;t.trim();t+="..";if(tft.textWidth(t,2)<=maxW)return t;}return String("..");}
void panel(int x,int y,int w,int h){tft.fillRoundRect(x,y,w,h,6,cPANEL);tft.drawRoundRect(x,y,w,h,6,cLINE);}
void button(int x,int y,int w,const String& s,uint16_t fg,uint16_t bg){tft.fillRoundRect(x,y,w,18,6,bg);tft.drawRoundRect(x,y,w,18,6,fg);txt(s,x+w/2,y+9,2,fg,bg,MC_DATUM);}
void splash(){tft.setSwapBytes(true);uint16_t row[SPLASH_W];for(unsigned j=0;j<SPLASH_H;j++){for(unsigned i=0;i<SPLASH_W;i++)row[i]=pgm_read_word(splashImg+j*SPLASH_W+i);tft.pushImage(0,j,SPLASH_W,1,row);}}
// Ikona satelity 16x14: korpus, dwa panele sloneczne z podzialem i antena z kropka (oznacza przyciski GPS).
void satIcon(int x,int y,uint16_t c){tft.fillRect(x+8,y+6,6,7,c);tft.fillRect(x,y+7,7,5,c);tft.fillRect(x+15,y+7,7,5,c);tft.drawFastVLine(x+3,y+7,5,cHDR);tft.drawFastVLine(x+18,y+7,5,cHDR);tft.drawFastVLine(x+11,y+2,4,c);tft.fillCircle(x+11,y+1,2,c);tft.drawLine(x+14,y+3,x+17,y,c);tft.drawLine(x+15,y+5,x+20,y+1,c);}
void gpsBtn(int x,int w,const char* l,bool act,uint16_t col){tft.fillRoundRect(x,4,w,15,6,act?col:cHDR);tft.drawRoundRect(x,4,w,15,6,col);txt(l,x+w/2,11,1,act?cBG:col,act?col:cHDR,MC_DATUM);}
void drawHeader(bool alive){
 tft.fillRect(0,0,320,22,cHDR);tft.drawFastHLine(0,22,320,cLINE);
 tft.drawLine(9,13,15,7,cTURQ);tft.drawLine(15,7,21,14,cTURQ);
 const int nx[]={9,15,21},ny[]={13,7,14};for(int i=0;i<3;i++){tft.fillCircle(nx[i],ny[i],3,cTURQ);tft.fillCircle(nx[i],ny[i],1,cHDR);}
 {const int f=tft.textWidth("LoRaWAN Field Test",2)<=204?2:1,y=f==2?3:7;txt("LoRaWAN",29,y,f,cWHITE,cHDR);int w=tft.textWidth("LoRaWAN",f);txt("Field Test",29+w+(f==2?6:5),y,f,cTURQ,cHDR);}
 bool ok=configured&&alive;uint16_t c=ok?cGREEN:cAMBER,bg=ok?cOKBG:cWARNBG;const char* s=configured?(alive?"API OK":"API ?"):"CONNECTING";
 int pw=configured?58:tft.textWidth(s,1)+22,px=312-pw;tft.fillRoundRect(px,4,pw,15,7,bg);tft.drawRoundRect(px,4,pw,15,7,c);tft.fillCircle(px+9,11,3,c);txt(s,px+17,7,1,c,bg);
 // przyciski GPS noda: aktywny stan wypelniony (wg Config.position.gps_mode)
 if(configured){satIcon(161,3,cDIM);gpsBtn(187,28,"ON",posCfg.has&&posCfg.mode==1,cGREEN);gpsBtn(218,32,"OFF",posCfg.has&&posCfg.mode==0,cAMBER);}}
void drawGps(){bool v=ownPosition.valid();bool live=v&&ownPositionLive;uint16_t c=live?cGREEN:cAMBER,bg=live?cOKBG:cWARNBG;
 tft.fillRoundRect(8,26,304,16,8,bg);tft.drawRoundRect(8,26,304,16,8,c);tft.fillCircle(19,34,3,c);
 String s=v?String(ownPosition.lat*1e-7,5)+", "+String(ownPosition.lon*1e-7,5)+"  "+String((millis()-positionAt)/1000)+"s"+(ownPositionLive?"":" CACHE"):String(posCfg.has&&posCfg.mode==0?"GPS is switched OFF":posCfg.has&&posCfg.mode==2?"GPS not present":"GPS: no node position");
 txt(s,28,26,2,c,bg);}
void drawGauge(const Node& n,Quality q){
 const int px=8,py=71,pw=152,ph=79,cx=84,cy=139,ro=53,ri=43;panel(px,py,pw,ph);bool mine=probe.node==n.id;uint16_t dc=!mine||!probe.on?cGREY:probe.pending?cAMBER:((millis()/500)&1?cTURQ:cTRACK);tft.fillCircle(15,80,3,dc);txt("RSSI",22,76,1,cDIM,cPANEL);
 const char* t=qText(q);uint16_t c=qColor(q),bg=qBg(q);int w=6*strlen(t)+22,bx=px+pw-6-w;
 tft.fillRoundRect(bx,74,w,13,6,bg);tft.drawRoundRect(bx,74,w,13,6,c);tft.fillCircle(bx+8,80,2,c);txt(t,bx+14,76,1,c,bg);
 tft.drawArc(cx,cy,ro,ri,90,270,cTRACK,cPANEL,true);
 float f=constrain((n.rssi-GAUGE_MIN)/float(GAUGE_MAX-GAUGE_MIN),0.f,1.f),endA=90+f*180;
 for(int k=0;k<18;k++){float a0=90+k*10,a1=a0+10;if(a0>=endA)break;if(a1>endA)a1=endA;if(a1-a0<1)break;tft.drawArc(cx,cy,ro,ri,(uint32_t)a0,(uint32_t)ceilf(a1),gradient(k/17.f),cPANEL,true);}
 float ar=endA*DEG_TO_RAD,rm=(ro+ri)/2.f;tft.fillCircle(cx-rm*sinf(ar),cy+rm*cosf(ar),3,cWHITE);
 txt(String(n.rssi),cx,125,4,cWHITE,cPANEL,MC_DATUM);txt("dBm",cx,145,1,cTURQ,cPANEL,MC_DATUM);
 txt(String(GAUGE_MIN),cx-50,145,1,cDIM,cPANEL,MC_DATUM);txt(String(GAUGE_MAX),cx+50,145,1,cDIM,cPANEL,MC_DATUM);}
void drawSnr(const Node& n){panel(168,71,144,38);txt("SNR",176,76,1,cDIM,cPANEL);uint16_t c=n.snr>=SNR_GOOD?cTURQ:n.snr>=SNR_FAIR?cAMBER:cRED;
 txt(String(n.snr,1),208,76,4,c,cPANEL);txt("dB",286,88,2,cDIM,cPANEL);tft.fillRoundRect(176,104,128,3,1,cTRACK);
 float f=constrain((n.snr-SNR_MIN)/float(SNR_MAX-SNR_MIN),0.f,1.f);if(f>0)tft.fillRoundRect(176,104,max(2,int(128*f)),3,1,c);}
void drawHops(const Node& n){panel(168,112,144,38);txt("HOPS",176,117,1,cDIM,cPANEL);
 String hs=n.hops<0?String("?"):n.hops==0?String("DIRECT"):String(min(n.hops,HOP_MAX))+"/"+String(HOP_MAX)+" hop";txt(hs,304,115,2,cWHITE,cPANEL,TR_DATUM);
 int lit=n.hops<0?0:min(n.hops,HOP_MAX);for(int i=0;i<HOP_MAX;i++){int x=184+int(i*19.5f);if(i<HOP_MAX-1)tft.drawFastHLine(x+4,140,16,i<lit-1?cTURQ:cLINE);if(i<lit)tft.fillCircle(x,140,4,cTURQ);else{tft.fillCircle(x,140,4,cTRACK);tft.drawCircle(x,140,4,cLINE);}}}
void drawStats(const Node& n){const char* L[]={"N","MIN dBm","MAX dBm","AVG dBm"};String V[]={String(n.count),String(n.lo),String(n.hi),String(n.sum/n.count,1)};
 for(int i=0;i<4;i++){int x=8+i*77;panel(x,153,70,26);txt(L[i],x+35,158,1,cDIM,cPANEL,MC_DATUM);txt(V[i],x+35,170,2,cWHITE,cPANEL,MC_DATUM);}}
void drawChart(const Node& n){panel(8,182,304,30);txt("RSSI dBm, last "+String(n.used),16,185,1,cDIM,cPANEL);if(probe.node==n.id){uint32_t tot=probe.ok+probe.lost;String t=String(probe.on?"TEST ":"PAUSED ")+String(probe.ok)+"/"+String(tot);if(probe.rtt>=0)t+=" RTT "+String(probe.rtt/1000.0,1)+"s";txt(t,304,185,1,probe.lost&&probe.lost*4>tot?cAMBER:cTURQ,cPANEL,TR_DATUM);}
 const int x1=302,y0=195,y1=208,span=282;auto ym=[&](int v){return y1-(constrain(v,GAUGE_MIN,GAUGE_MAX)-GAUGE_MIN)*(y1-y0)/(GAUGE_MAX-GAUGE_MIN);};
 tft.drawFastHLine(16,ym(RSSI_GOOD),288,cTRACK);unsigned start=(n.head+HISTORY-n.used)%HISTORY;int px=0,py=0;
 for(unsigned i=0;i<n.used;i++){int x=x1-int((n.used-1-i)*span/(HISTORY-1)),y=ym(n.hist[(start+i)%HISTORY]);if(i)tft.drawLine(px,py,x,y,cTURQ);px=x;py=y;}
 if(n.used)tft.fillCircle(px,py,2,cWHITE);}
void drawNav(bool detailView){tft.fillRect(0,214,320,26,cHDR);tft.drawFastHLine(0,214,320,cLINE);
 button(8,218,94,(specScr||mapScr||chatScr)?"DASHBOARD":home?"NODES":detailView?"LIST":"NEXT",cTURQ,cOKBG);
 if(logging)button(113,218,94,String("STOP ")+String(written),cRED,cBADBG);else if(millis()<uiMsgUntil)button(113,218,94,uiMsg,cAMBER,cWARNBG);else button(113,218,94,"LOG",cTURQ,cOKBG);
 {const char* WL[]={"TIME 1m","TIME 10m","TIME 1h"};unsigned cpg=max(1U,(chatTotal(chatCh)+3)/4);String r=chatScr?(cpg>1?String("OLDER ")+String(chatPage+1)+"/"+String(cpg):String("CHAT")):specScr?String(WL[specWin]):mapScr?(mapZoom>1?String("ZOOM OUT"):mapPages>1?String("PAGE ")+String(mapPage+1)+"/"+String(mapPages):String("MAP")):home?String("CAL"):String("DASHBOARD");button(218,218,94,r,cTURQ,cOKBG);}}
void drawDetail(const Node& n){Quality q=quality(n);char id[16];snprintf(id,sizeof(id),"!%08lx",(unsigned long)n.id);
 panel(8,45,304,22);txt("NODE",16,52,1,cDIM,cPANEL);int idW=tft.textWidth(id,2);txt(fitName(n,304-idW-8-54),54,48,2,cWHITE,cPANEL);txt(id,304,48,2,cTURQ,cPANEL,TR_DATUM);
 drawGauge(n,q);drawSnr(n);drawHops(n);drawStats(n);drawChart(n);}
void drawList(){sortNodes();unsigned pages=max(1U,(orderCount+4)/5);
 tft.fillRect(0,44,320,24,cBG);txt("NODE",24,49,1,cDIM,cBG);txt("RSSI",122,49,1,cDIM,cBG);txt("SNR",190,49,1,cDIM,cBG);txt("AGE",262,49,1,cDIM,cBG);txt(String(page+1)+"/"+String(pages),312,49,1,cDIM,cBG,TR_DATUM);
 for(unsigned i=0;i<5;i++){int y=70+i*27;if(page*5+i>=orderCount||!orderCount){tft.fillRect(0,y,320,27,cBG);continue;}
  Node &n=nodes[order[page*5+i]];Quality q=quality(n);char id[16];snprintf(id,sizeof(id),"!%08lx",(unsigned long)n.id);panel(6,y,308,25);tft.fillCircle(17,y+12,4,qColor(q));
  txt(fitName(n,118-28-4),28,y+4,2,cWHITE,cPANEL);String rv=String(n.rssi),sv=String(n.snr,1);txt(rv,122,y+4,2,qColor(q),cPANEL);txt("dBm",122+tft.textWidth(rv,2)+2,y+12,1,cDIM,cPANEL);txt(sv,190,y+4,2,cTURQ,cPANEL);txt("dB",190+tft.textWidth(sv,2)+2,y+12,1,cDIM,cPANEL);txt(String((millis()-n.last)/1000)+"s",262,y+4,2,cDIM,cPANEL);}
 if(!orderCount){panel(6,70,308,118);txt("Waiting for node data...",16,78,2,cWHITE,cPANEL);
  txt(String("Baud ")+String((unsigned long)baudNow)+"  RX "+String((unsigned long)rxBytes)+" B",16,100,2,cTURQ,cPANEL);
  txt(String("Frames ")+String((unsigned long)frames)+"  Errors "+String((unsigned long)framing.errors),16,120,2,cDIM,cPANEL);
  const char *h1,*h2;if(!rxBytes){h1="No bytes from node. Check: node TX->IO35,";h2="node RX<-IO27, common GND; serial.mode PROTO";}else if(!frames){h1="Bytes received, no valid frames.";h2="Wrong baud rate or noise - scanning rates...";}else{h1="Connected to node (API OK).";h2="Waiting for packets from mesh nodes.";}
  txt(h1,16,146,1,cAMBER,cPANEL);txt(h2,16,160,1,cAMBER,cPANEL);}}
// ---- Pulpit: kafelki ze statusem wezla (dane z telemetrii, konfiguracji LoRa i kanalow Trackera) ----
void tile(int x,int y,const char* title,uint16_t accent,const char* more=nullptr){panel(x,y,98,78);tft.fillCircle(x+9,y+10,2,accent);txt(title,x+16,y+6,1,cDIM,cPANEL);if(more)txt(more,x+92,y+6,1,cTURQ,cPANEL,TR_DATUM);}
String upText(uint32_t s){char b[16];if(s>=86400)snprintf(b,sizeof b,"%lud%luh",(unsigned long)(s/86400),(unsigned long)(s%86400/3600));else if(s>=3600)snprintf(b,sizeof b,"%luh%02lum",(unsigned long)(s/3600),(unsigned long)(s%3600/60));else snprintf(b,sizeof b,"%lum",(unsigned long)(s/60));return String(b);}
void drawHome(){const int X[]={6,109,212},Y[]={46,130};char b[48];uint32_t now=millis();
 // BATERIA
 {tile(X[0],Y[0],"BATTERY",cGREEN);if(dev.has&&dev.battery>=0){bool usb=dev.battery>100;uint16_t c=usb?cTURQ:dev.battery<20?cRED:dev.battery<40?cAMBER:cGREEN;if(usb)snprintf(b,sizeof b,"USB");else snprintf(b,sizeof b,"%d%%",dev.battery);txt(b,X[0]+8,Y[0]+20,4,c,cPANEL);
   if(dev.voltage>0){snprintf(b,sizeof b,"%.2f V",dev.voltage);txt(b,X[0]+8,Y[0]+52,2,cWHITE,cPANEL);}}
  else{txt("--",X[0]+8,Y[0]+20,4,cGREY,cPANEL);txt("no telemetry",X[0]+8,Y[0]+56,1,cAMBER,cPANEL);}}
 // NODY
 {tile(X[1],Y[0],"NODES",cTURQ,"MAP >");unsigned heard=0,known=0,act=0;for(auto&n:nodes){if(n.id)++known;if(n.count){++heard;if(now-n.last<900000)++act;}}
  snprintf(b,sizeof b,"%u",heard);txt(b,X[1]+8,Y[0]+20,4,cWHITE,cPANEL);txt("heard",X[1]+8,Y[0]+50,1,cDIM,cPANEL);snprintf(b,sizeof b,"%u in 15 min",act);txt(b,X[1]+8,Y[0]+60,1,cTURQ,cPANEL);snprintf(b,sizeof b,"known: %u",known);txt(b,X[1]+8,Y[0]+69-1,1,cDIM,cPANEL);}
 // RADIO
 {tile(X[2],Y[0],"RADIO",cAMBER,"NOISE >");mesh::Modem m;if(lora.has&&mesh::modem(lora,chans[0].name,m)){const char* pn=lora.usePreset?mesh::presetName(lora.preset):"MANUAL";txt(pn,X[2]+8,Y[0]+18,2,cWHITE,cPANEL);
   if(m.mhz>0){snprintf(b,sizeof b,"%.3f",m.mhz);txt(b,X[2]+8,Y[0]+36,2,cTURQ,cPANEL);txt("MHz",X[2]+8+tft.textWidth(b,2)+3,Y[0]+40,1,cDIM,cPANEL);}
   else txt(mesh::regionName(lora.region),X[2]+8,Y[0]+36,2,cTURQ,cPANEL);
   snprintf(b,sizeof b,"SF%u BW%g kHz",m.sf,(double)m.bwKhz);txt(b,X[2]+8,Y[0]+56,1,cDIM,cPANEL);txt(mesh::regionName(lora.region),X[2]+8,Y[0]+67,1,cDIM,cPANEL);}
  else{txt("--",X[2]+8,Y[0]+20,4,cGREY,cPANEL);txt("no LoRa config",X[2]+8,Y[0]+56,1,cAMBER,cPANEL);}}
 // KANAL
 {tile(X[0],Y[1],"CHANNEL",cGREEN,"NOISE >");if(dev.has){uint16_t c=dev.chUtil>50?cRED:dev.chUtil>25?cAMBER:cGREEN;snprintf(b,sizeof b,"%.0f%%",dev.chUtil);txt(b,X[0]+8,Y[1]+20,4,c,cPANEL);snprintf(b,sizeof b,"Tx %.1f%%",dev.airTx);txt(b,X[0]+8,Y[1]+52,2,cWHITE,cPANEL);}
  else{txt("--",X[0]+8,Y[1]+20,4,cGREY,cPANEL);txt("no telemetry",X[0]+8,Y[1]+56,1,cAMBER,cPANEL);}
  if(chans[0].role==1||chans[0].name[0]){snprintf(b,sizeof b,"%.14s",chans[0].name[0]?chans[0].name:mesh::presetName(lora.preset));txt(b,X[0]+8,Y[1]+68,1,cDIM,cPANEL);}}
 // CZAT: lista kanalow z liczba nowych wiadomosci (dotkniecie wiersza otwiera czytnik)
 {unsigned tot=0;for(unsigned c=0;c<9;c++)tot+=unreadN(c);tile(X[1],Y[1],"CHAT",cTURQ);if(tot){snprintf(b,sizeof b,"%u new",tot);txt(b,X[1]+92,Y[1]+6,1,cAMBER,cPANEL,TR_DATUM);}else if(chatCount){snprintf(b,sizeof b,"%lu",(unsigned long)chatCount);txt(b,X[1]+92,Y[1]+6,1,cDIM,cPANEL,TR_DATUM);}
  unsigned o[9];unsigned n=chList(o);if(!n)txt("no channels",X[1]+8,Y[1]+24,1,cDIM,cPANEL);
  for(unsigned i=0;i<n&&i<5;i++){unsigned u=unreadN(o[i]);char nm[24];strlcpy(nm,chName(o[i]).c_str(),sizeof nm);if(o[i]==8)snprintf(b,sizeof b,"DM");else snprintf(b,sizeof b,"%u %.8s",o[i],nm);
   int yy=Y[1]+19+i*12;txt(b,X[1]+8,yy,1,u?cWHITE:cDIM,cPANEL);snprintf(b,sizeof b,"%u",u);txt(b,X[1]+92,yy,1,u?cAMBER:cGREY,cPANEL,TR_DATUM);}}
 // TRACKER
 {tile(X[2],Y[1],"DEVICE",cAMBER);if(myId){const char* nm=nullptr;for(auto &n:nodes)if(n.id==myId){if(n.name[0])nm=n.name;break;}
   if(nm){snprintf(b,sizeof b,"%.15s",nm);txt(b,X[2]+8,Y[1]+19,1,cWHITE,cPANEL);}
   snprintf(b,sizeof b,"!%08lx",(unsigned long)myId);txt(b,X[2]+8,Y[1]+(nm?30:20),nm?1:2,nm?cDIM:cWHITE,cPANEL);}else txt("--",X[2]+8,Y[1]+20,2,cGREY,cPANEL);
  if(dev.has&&dev.uptime){txt("up "+upText(dev.uptime+(now-devAt)/1000),X[2]+8,Y[1]+42,2,cTURQ,cPANEL);}
  snprintf(b,sizeof b,"%lu baud",(unsigned long)baudNow);txt(b,X[2]+8,Y[1]+58,1,cDIM,cPANEL);snprintf(b,sizeof b,"frames %lu",(unsigned long)frames);txt(b,X[2]+8,Y[1]+68,1,cDIM,cPANEL);}}
// ---- Mapa nodow: tlo Mazowsza (mapdata.h) + wezly z pozycja; lista po prawej ----
struct MapEnt{int idx,x,y;float km,fx,fy;};MapEnt mapList[MAX_NODES];unsigned mapCount=0,mapOutside=0;
void mapBuild(){mapCount=0;mapOutside=0;bool me=ownPosition.valid();uint32_t now=millis();
 for(unsigned i=0;i<MAX_NODES;i++){const Node& n=nodes[i];if(!n.id||!n.hasPos)continue;int x,y;if(!geo::project(n.lat,n.lon,x,y)){++mapOutside;continue;}float fx=x+.5f,fy=y+.5f;geo::projectF(n.lat,n.lon,fx,fy);mapList[mapCount++]={int(i),x,y,me?geo::distKm(ownPosition.lat,ownPosition.lon,n.lat,n.lon):-1.f,fx,fy};}
 // sluchane przed niesluchanymi, wsrod sluchanych najswiezsze, wsrod niesluchanych najblizsze
 auto before=[&](const MapEnt& a,const MapEnt& b){const Node &p=nodes[a.idx],&q=nodes[b.idx];bool hp=p.count>0,hq=q.count>0;if(hp!=hq)return hp;if(hp)return uint32_t(now-p.last)<uint32_t(now-q.last);return a.km<b.km;};
 for(unsigned i=1;i<mapCount;i++){MapEnt v=mapList[i];unsigned j=i;while(j&&before(v,mapList[j-1])){mapList[j]=mapList[j-1];--j;}mapList[j]=v;}
 mapSel=-1;if(mapSelId)for(unsigned i=0;i<mapCount;i++)if(nodes[mapList[i].idx].id==mapSelId)mapSel=int(i);
 mapPages=max(1U,(mapCount+8)/9);if(mapPage>=mapPages)mapPage=0;}
// Widok mapy: zoom 1 = cala mapa + lista; zoom 2/3 = powiekszony wycinek na pelnej szerokosci, srodek w mapCX/mapCY (domyslnie wybrany wezel, potem Tracker).
void mapCenter(float& cx,float& cy){cx=MAP_W/2.f;cy=MAP_H/2.f;float fx,fy;if(mapSel>=0&&mapSel<int(mapCount)){cx=mapList[mapSel].fx;cy=mapList[mapSel].fy;}else if(ownPosition.valid()&&geo::projectF(ownPosition.lat,ownPosition.lon,fx,fy)){cx=fx;cy=fy;}}
// Zrodlo obrazu: zoom 1 = mapImg (jednostka U=1), zoom>1 = mapHi (U=MAPH_S, 4x gestsze); vx/vy w jednostkach U.
bool mapWaw(){return mapZoom>=MAPW_ZOOM_MIN;}
int zoomStep(int d){int i=0;for(int k=0;k<ZN;k++)if(ZL[k]<=mapZoom)i=k;return ZL[constrain(i+d,0,ZN-1)];}
int mapU(){return mapWaw()?MAPW_S:mapZoom>1?int(MAPH_S):1;}
void mapView(int& vx,int& vy){const int z=mapZoom,U=mapU(),W=z>1?308:int(MAP_W),H=166;int sw=(W*U+z-1)/z,sh=(H*U+z-1)/z;float cx=mapCX,cy=mapCY;if(cx<0)mapCenter(cx,cy);
 int lx=0,ly=0,hx=int(MAP_W)*U,hy=int(MAP_H)*U;if(mapWaw()){lx=MAPW_X0;ly=MAPW_Y0;hx=lx+MAPW_W;hy=ly+MAPW_H;}   // zoom maks.: tylko wycinek Warszawy
 vx=constrain(int(cx*U-sw/2.f+.5f),lx,max(lx,hx-sw));vy=constrain(int(cy*U-sh/2.f+.5f),ly,max(ly,hy-sh));}
// Ustaw srodek widoku na faktycznie wyswietlany (po przycieciu do mapy), np. przed zmiana zoomu lub przesunieciem.
void mapRecentre(){int vx,vy;mapView(vx,vy);const int z=mapZoom,U=mapU(),W=z>1?308:int(MAP_W);mapCX=(vx+((W*U+z-1)/z)/2.f)/U;mapCY=(vy+((166*U+z-1)/z)/2.f)/U;}
// Wchodzac w zoom maks. srodek musi lezec w wycinku Warszawy: zostaje, jesli juz tam jest; inaczej wybrany wezel / Tracker / centrum Warszawy.
void mapWawSnap(){auto in=[&](float fx,float fy){return fx*MAPW_S>MAPW_X0+30&&fx*MAPW_S<MAPW_X0+MAPW_W-30&&fy*MAPW_S>MAPW_Y0+30&&fy*MAPW_S<MAPW_Y0+MAPW_H-30;};if(mapCX>=0&&in(mapCX,mapCY))return;float fx,fy;
 if(mapSel>=0&&mapSel<int(mapCount)&&in(mapList[mapSel].fx,mapList[mapSel].fy)){mapCX=mapList[mapSel].fx;mapCY=mapList[mapSel].fy;return;}
 if(ownPosition.valid()&&geo::projectF(ownPosition.lat,ownPosition.lon,fx,fy)&&in(fx,fy)){mapCX=fx;mapCY=fy;return;}
 if(geo::projectF(522297000,210122000,fx,fy)){mapCX=fx;mapCY=fy;}}
void drawMap(bool force){static uint32_t key=0;uint32_t now=millis();mapBuild();
 uint32_t k=mapCount*977u+mapOutside*31u+mapPage*7u+uint32_t(mapSel+2)*131u+(now/5000u)+1;for(unsigned i=0;i<mapCount;i++)k=k*33u+uint32_t(mapList[i].x*200+mapList[i].y)+nodes[mapList[i].idx].count;
 uint32_t ok=ownPosition.valid()?uint32_t(ownPosition.lat^ownPosition.lon):0;k^=ok;k=k*31u+uint32_t(mapZoom)*53u+uint32_t(mapCX*8)*17u+uint32_t(mapCY*8);if(!force&&k==key)return;key=k;
 const int z=mapZoom,ox=6,oy=46,W=z>1?308:int(MAP_W),H=166;int vx,vy;mapView(vx,vy);
 tft.setSwapBytes(true);uint16_t row[308];
 const int U=mapU();
 for(int j=0;j<H;j++){const unsigned sr=unsigned(vy+(j*U)/z);if(mapWaw()){const unsigned base=(sr-MAPW_Y0)*unsigned(MAPW_W)+unsigned(vx-MAPW_X0);for(int i=0;i<W;i++){unsigned id=base+unsigned((i*U)/z);uint8_t b=pgm_read_byte(mapW+(id>>1));row[i]=pgm_read_word(mapWPal+((id&1)?(b>>4):(b&15)));}}else if(z==1){const uint16_t* src=mapImg+sr*MAP_W+vx;for(int i=0;i<W;i++)row[i]=pgm_read_word(src+i);}else{const unsigned base=sr*MAPH_W+unsigned(vx);for(int i=0;i<W;i++){unsigned id=base+unsigned((i*U)/z);uint8_t b=pgm_read_byte(mapHi+(id>>1));row[i]=pgm_read_word(mapPal+((id&1)?(b>>4):(b&15)));}}tft.pushImage(ox,oy+j,W,1,row);}
 Serial.printf("MAP zoom=%d src=%s view=%d,%d\n",z,mapWaw()?"WARSAW":z>1?"HIRES":"MAZOWSZE",vx,vy);
 auto sx=[&](float fx){return ox+int((fx-vx/float(U))*z);};auto sy=[&](float fy){return oy+int((fy-vy/float(U))*z);};
 auto vis=[&](int x,int y){return x>=ox+9&&x<ox+W-9&&y>=oy+9&&y<oy+H-9;};
 for(unsigned i=0;i<mapCount;i++){const Node& n=nodes[mapList[i].idx];int x=sx(mapList[i].fx),y=sy(mapList[i].fy);if(!vis(x,y))continue;uint16_t c=n.count?qColor(quality(n)):cGREY;tft.fillCircle(x,y,4,cHDR);tft.fillCircle(x,y,3,c);if(!n.count)tft.fillCircle(x,y,1,cHDR);}
 {float fx,fy;if(ownPosition.valid()&&geo::projectF(ownPosition.lat,ownPosition.lon,fx,fy)){int x=sx(fx),y=sy(fy);if(vis(x,y)){tft.drawCircle(x,y,6,cWHITE);tft.drawCircle(x,y,5,cHDR);tft.fillCircle(x,y,2,cTURQ);}}}
 if(mapSel>=0&&mapSel<int(mapCount)){const MapEnt& e=mapList[mapSel];const Node& n=nodes[e.idx];int x=sx(e.fx),y=sy(e.fy);if(vis(x,y)){tft.drawCircle(x,y,7,cAMBER);tft.drawCircle(x,y,8,cAMBER);
  char nm[24];size_t len=n.name[0]?mesh::displayName(n.name,nm,sizeof(nm)):0;if(!len)snprintf(nm,sizeof(nm),"!%08lx",(unsigned long)n.id);int w=6*strlen(nm)+6,lx=constrain(x-w/2,ox+1,ox+W-w-1),ly=y>oy+22?y-22:y+10;
  tft.fillRoundRect(lx,ly,w,12,3,cHDR);tft.drawRoundRect(lx,ly,w,12,3,cAMBER);txt(nm,lx+3,ly+2,1,cWHITE,cHDR);}}
 // przyciski zoomu w lewym dolnym rogu mapy
 {auto zb=[&](int bx,const char* t,bool on){tft.fillRoundRect(bx,192,24,18,3,cHDR);tft.drawRoundRect(bx,192,24,18,3,on?cTURQ:cLINE);txt(t,bx+12,201,2,on?cTURQ:cDIM,cHDR,MC_DATUM);};
  if(z>1){zb(8,"-",true);zb(36,"+",z<MAP_ZOOM_MAX);txt(String(z)+"x"+(mapWaw()?" WARSAW":""),66,194,2,cTURQ,cHDR);}else zb(8,"+",true);}
 if(z>1){auto ab=[&](int bx,int by,int dx,int dy){tft.fillRoundRect(bx,by,20,16,3,cHDR);tft.drawRoundRect(bx,by,20,16,3,cTURQ);int cx=bx+10,cy=by+8;tft.fillTriangle(cx+dx*5+(dx?0:dy*0),cy+dy*5,cx-dx*3+(dy?-5:0)*1,cy-dy*3+(dx?-5:0),cx-dx*3+(dy?5:0),cy-dy*3+(dx?5:0),cTURQ);};
  ab(272,152,0,-1);ab(250,170,-1,0);ab(294,170,1,0);ab(272,188,0,1);return;}
 panel(170,46,144,166);txt("NODES ON MAP",178,51,1,cDIM,cPANEL);char b[24];snprintf(b,sizeof b,"%u",mapCount);txt(b,308,51,1,cTURQ,cPANEL,TR_DATUM);
 if(!mapCount){txt("No node positions.",178,76,1,cWHITE,cPANEL);txt("Nodes send position",178,90,1,cDIM,cPANEL);txt("every ~15 min.",178,102,1,cDIM,cPANEL);}
 for(unsigned r=0;r<9;r++){unsigned e=mapPage*9+r;if(e>=mapCount)break;const Node& n=nodes[mapList[e].idx];int y=62+r*16;if(int(e)==mapSel)tft.fillRect(172,y-1,140,16,cOKBG);
  uint16_t c=n.count?qColor(quality(n)):cGREY;tft.fillCircle(180,y+7,3,c);txt(fitName(n,70),188,y,2,cWHITE,int(e)==mapSel?cOKBG:cPANEL);
  if(mapList[e].km>=0){snprintf(b,sizeof b,"%.0fkm",mapList[e].km);txt(b,311,y+4,1,cDIM,int(e)==mapSel?cOKBG:cPANEL,TR_DATUM);}}
 if(mapOutside){snprintf(b,sizeof b,"off map: %u",mapOutside);txt(b,178,201,1,cDIM,cPANEL);}}
// ---- Widmo / szum: estymata z pakietow (szum = RSSI - SNR), zajetosc kanalu z telemetrii ----
void drawSpec(bool force){static uint32_t key=0;uint32_t now=millis();const uint32_t W[]={60000,600000,3600000};const char* WN[]={"1 min","10 min","1 h"};uint32_t win=W[specWin];
 uint32_t k=specRing.total*131u+specWin*1000003u+utilRing.used*7u+(now/5000u)+1;if(!force&&k==key)return;key=k;
 spec::Stats st=spec::stats(specRing,now,win);char b[24];
 const char* L[]={"NOISE dBm","FLOOR dBm","CH UTIL","PKT/MIN","AIR TX"};String V[5];
 V[0]=st.hasNoise?String(st.now):String("--");V[1]=st.hasNoise?String(st.floor):String("--");V[2]=dev.has?String(dev.chUtil,0)+"%":String("--");V[3]=String(st.perMin);V[4]=dev.has?String(dev.airTx,1)+"%":String("--");
 for(int i=0;i<5;i++){int x=6+i*62;panel(x,46,58,28);txt(L[i],x+29,53,1,cDIM,cPANEL,MC_DATUM);uint16_t c=cWHITE;if(i==2&&dev.has)c=dev.chUtil>50?cRED:dev.chUtil>25?cAMBER:cGREEN;txt(V[i],x+29,65,2,c,cPANEL,MC_DATUM);}
 panel(6,78,308,130);txt("dBm",306,79,1,cDIM,cPANEL,TR_DATUM);const int x0=36,x1=306,yt=86,yb=160,w=x1-x0;
 auto ym=[&](int v){return yb-(constrain(v,GAUGE_MIN,GAUGE_MAX)-GAUGE_MIN)*(yb-yt)/(GAUGE_MAX-GAUGE_MIN);};
 for(int v=-40;v>=-120;v-=20){int y=ym(v);for(int x=x0;x<=x1;x+=6)tft.drawPixel(x,y,cTRACK);snprintf(b,sizeof b,"%d",v);txt(b,32,y,1,cDIM,cPANEL,MR_DATUM);}
 for(unsigned i=0;i<specRing.used;i++){const spec::Sample& s=specRing.at(i);uint32_t age=now-s.t;if(age>win)continue;int x=x1-int(uint64_t(age)*w/win);
  if(s.rssi!=spec::NONE)tft.fillRect(x-1,ym(s.rssi)-1,2,2,cAMBER);}
 // szum: linia poziomu (10. percentyl), lamana przez kolejne odczyty i wyrazne kropki 3x3 nad kropkami RSSI
 if(st.hasNoise){int yf=ym(st.floor);for(int x=x0;x<=x1;x+=4)tft.drawFastHLine(x,yf,2,cTURQ);snprintf(b,sizeof b,"floor %d",st.floor);txt(b,x0+3,yf<yb-10?yf+2:yf-10,1,cTURQ,cPANEL);
  int lx=-1,ly=0;for(unsigned i=0;i<specRing.used;i++){const spec::Sample& s=specRing.at(i);if(now-s.t>win||s.noise==spec::NONE)continue;int x=x1-int(uint64_t(now-s.t)*w/win),y=ym(s.noise);if(lx>=0)tft.drawLine(lx,ly,x,y,cTURQ);lx=x;ly=y;}
  for(unsigned i=0;i<specRing.used;i++){const spec::Sample& s=specRing.at(i);if(now-s.t>win||s.noise==spec::NONE)continue;tft.fillRect(x1-int(uint64_t(now-s.t)*w/win)-1,ym(s.noise)-1,3,3,cWHITE);}}
 if(!st.n)txt("Waiting for mesh packets...",(x0+x1)/2,(yt+yb)/2,2,cDIM,cPANEL,MC_DATUM);
 else if(!st.hasNoise)txt("no noise estimate: signals too strong",(x0+x1)/2,(yt+yb)/2+14,1,cAMBER,cPANEL,MC_DATUM);
 tft.fillCircle(40,167,2,cTURQ);txt("noise (RSSI-SNR)",46,163,1,cDIM,cPANEL);tft.fillCircle(150,167,2,cAMBER);txt("signal",156,163,1,cDIM,cPANEL);txt(String("time ")+WN[specWin],306,163,1,cWHITE,cPANEL,TR_DATUM);
 txt("CHANNEL",8,185,1,cDIM,cPANEL);const int sy0=176,sh=18;unsigned cols=win/10000;
 for(unsigned a=0;a<cols&&a<utilRing.used;a++){float p=utilRing.pctAgo(a);int xr=x1-int(uint64_t(a)*w/cols),xl=x1-int(uint64_t(a+1)*w/cols);int h=p<=0?1:max(1,int(p*sh/100));uint16_t c=p>50?cRED:p>25?cAMBER:cGREEN;tft.fillRect(xl,sy0+sh-h,max(1,xr-xl),h,c);}
 snprintf(b,sizeof b,"-%s",WN[specWin]);txt(b,x0,197,1,cDIM,cPANEL);txt("now",x1,197,1,cDIM,cPANEL,TR_DATUM);}
// Okno czytania wiadomosci jednego kanalu: najnowsze u gory, 4 na strone, tekst zawijany do 2 wierszy po 50 znakow.
void drawChat(){tft.fillRect(0,46,320,166,cBG);unsigned tot=chatTotal(chatCh),pages=max(1U,(tot+3)/4);if(chatPage>=pages)chatPage=pages-1;char b[64];
 txt(chName(chatCh),8,47,2,cTURQ,cBG);snprintf(b,sizeof b,"%u msg  %u/%u",tot,chatPage+1,pages);txt(b,312,52,1,cDIM,cBG,TR_DATUM);tft.drawFastHLine(6,65,308,cLINE);
 if(!tot)txt("no messages on this channel yet",8,90,2,cDIM,cBG);
 unsigned seen=0,shown=0;for(int i=int(msgCnt)-1;i>=0&&shown<4;i--){const Msg& m=msgs[(msgHead+i)%MSGN];if(m.ch!=chatCh)continue;if(seen++<chatPage*4)continue;int y=68+int(shown++)*35;
  char who[40]={};const Node* sn=nullptr;for(auto &n:nodes)if(n.id==m.from){sn=&n;break;}if(sn&&sn->name[0])mesh::displayName(sn->name,who,sizeof who);if(!who[0])snprintf(who,sizeof who,"!%08lx",(unsigned long)m.from);who[28]=0;
  if(m.fresh)tft.fillCircle(4,y+4,2,cAMBER);txt(who,10,y,1,m.fresh?cAMBER:cTURQ,cBG);
  uint32_t a=(millis()-m.at)/1000;if(a<60)snprintf(b,sizeof b,"%lus",(unsigned long)a);else if(a<3600)snprintf(b,sizeof b,"%lum",(unsigned long)(a/60));else snprintf(b,sizeof b,"%luh",(unsigned long)(a/3600));txt(b,312,y,1,cDIM,cBG,TR_DATUM);
  char l1[52]={},l2[52]={};const char* t=m.text;size_t n=strlen(t),k=n;if(n>50){k=50;while(k>32&&t[k]!=' ')k--;if(t[k]!=' ')k=50;}memcpy(l1,t,k);const char* r=t+k;while(*r==' ')r++;size_t rn=strlen(r);if(rn>50){memcpy(l2,r,50);l2[48]='.';l2[49]='.';}else memcpy(l2,r,rn);
  txt(l1,8,y+11,1,cWHITE,cBG);if(l2[0])txt(l2,8,y+21,1,cWHITE,cBG);}}
void draw(){bool alive=everFrame&&(millis()-lastFrame<90000);int screen=(detail&&selected>=0&&nodes[selected].count)?1:mapScr?4:specScr?3:chatScr?5:home?2:0;
 bool changed=false;if(screen!=uiScreen){tft.fillScreen(cBG);uiScreen=screen;changed=true;}
 drawHeader(alive);drawGps();if(screen==1)drawDetail(nodes[selected]);else if(screen==2)drawHome();else if(screen==3)drawSpec(changed);else if(screen==4)drawMap(changed);else if(screen==5)drawChat();else drawList();drawNav(screen==1);
 Serial.printf("baud=%lu rx=%lu frames=%lu ",(unsigned long)baudNow,(unsigned long)rxBytes,(unsigned long)frames);Serial.printf("API=%d nodes=%u csv=%lu frame_errors=%u parse_errors=%lu log_errors=%lu\n",configured,orderCount,(unsigned long)written,framing.errors,(unsigned long)parseErrors,(unsigned long)logErrors);
}
void setup(){Serial.begin(115200);pinMode(BACKLIGHT,OUTPUT);digitalWrite(BACKLIGHT,HIGH);pinMode(TOUCH_CS,OUTPUT);digitalWrite(TOUCH_CS,HIGH);pinMode(TOUCH_CLK,OUTPUT);digitalWrite(TOUCH_CLK,LOW);pinMode(TOUCH_MOSI,OUTPUT);pinMode(TOUCH_MISO,INPUT);pinMode(0,INPUT_PULLUP);
 tft.init();tft.setRotation(1);prefs.begin("touch",false);invertScr=prefs.getBool("inv",false);tft.invertDisplay(invertScr);splash();delay(1800);baudNow=prefs.getULong("baud",RADIO_BAUD);radio.setRxBufferSize(4096);radio.begin(baudNow,SERIAL_8N1,RADIO_RX,RADIO_TX);baudSince=millis();calX0=prefs.getUShort("x0",300);calX1=prefs.getUShort("x1",3800);calY0=prefs.getUShort("y0",300);calY1=prefs.getUShort("y1",3800);calSwap=prefs.getBool("swap",true);
 sdReady=sdMount();if(!prefs.getBool("valid",false))calibrate();requestConfig();draw();}
void loop(){if(Serial.available()){char cmd=Serial.read();if(cmd=='l')toggleLog();if(cmd=='c')calibrate();if(cmd=='t'){clockSeen(uint32_t(Serial.parseInt()));}if(cmd=='r'){configured=false;requestConfig();}}while(radio.available()){framing.feed(radio.read(),millis(),fromRadio);++rxBytes;}uint32_t now=millis();
 if(!everFrame){if(now-baudSince>8000)nextBaud(now);else if(now-lastRequest>3500)requestConfig();}
 if(gpsAt&&now-gpsAt>12000){gpsAt=0;configured=false;}
 static bool baudSaved=false;if(configured&&!baudSaved){baudSaved=true;if(prefs.getULong("baud",0)!=baudNow)prefs.putULong("baud",baudNow);}if((!configured&&now-lastRequest>15000)||(everFrame&&now-lastFrame>90000&&now-lastRequest>90000)){configured=false;requestConfig();}
 if(configured&&now-lastHeartbeat>30000){const uint8_t hb[]={0x3a,0};sendFrame(hb,sizeof(hb));lastHeartbeat=now;}
 static bool pressed=false;static uint32_t pressedAt=0;int x,y;if(touch(x,y)){if(!pressed&&now-pressedAt>250){pressed=true;pressedAt=now;if(y<22&&configured&&x>=184&&x<252){gpsSet(x<217?1:0);}else if(y<22){invertScr=!invertScr;prefs.putBool("inv",invertScr);tft.invertDisplay(invertScr);uiScreen=-1;}else if(y>=212){if(x<100){if(specScr||mapScr||chatScr){leaveSub();}else if(home)home=false;else if(detail)detail=false;else page=(page+1)%max(1U,(orderCount+4)/5);}else if(x<247)toggleLog();else if(specScr)specWin=(specWin+1)%3;else if(chatScr){unsigned cp=max(1U,(chatTotal(chatCh)+3)/4);chatPage=(chatPage+1)%cp;}else if(mapScr){if(mapZoom>1){mapZoom=1;mapCX=-1;uiScreen=-1;}else mapPage=(mapPage+1)%mapPages;}else if(home)calibrate();else{home=true;detail=false;}}else if(home&&!specScr&&y>=46&&y<208){int row=y<125?0:1,col=x<104?0:x<207?1:2;if((row==0&&col==2)||(row==1&&col==0)){specScr=true;home=false;}else if(row==1&&col==1){chatTap(y);}else if(row==0&&col==1){mapScr=true;home=false;mapSel=-1;mapSelId=0;mapPage=0;mapZoom=1;mapCX=-1;}}else if(mapScr&&y>=46&&y<212){mapBuild();int hit=-1;bool zbtn=false;
  if(y>=190&&x>=6&&x<62){if(mapZoom>1&&x<34){mapRecentre();mapZoom=zoomStep(-1);if(mapZoom==1)mapCX=-1;zbtn=true;}else if(x>=(mapZoom>1?34:6)&&mapZoom<MAP_ZOOM_MAX){if(mapCX<0)mapCenter(mapCX,mapCY);if(mapZoom>1)mapRecentre();mapZoom=zoomStep(1);if(mapWaw())mapWawSnap();zbtn=true;}if(zbtn)uiScreen=-1;}
  // kursory: krzyzak w prawym dolnym rogu przesuwa widok o 60 pikseli ekranu (tylko po powiekszeniu)
  if(!zbtn&&mapZoom>1&&x>=250&&y>=152&&y<204){int col=(x-250)/22,dx=0,dy=0;if(y<169){if(col==1)dy=-1;}else if(y<187){if(col==0)dx=-1;else if(col==2)dx=1;}else if(col==1)dy=1;if(dx||dy){mapRecentre();mapCX+=dx*60.f/mapZoom;mapCY+=dy*60.f/mapZoom;zbtn=true;}}
  if(!zbtn){int vx,vy;mapView(vx,vy);if(mapZoom>1||x<168){int best=14*14;for(unsigned i=0;i<mapCount;i++){int dx=x-(6+int((mapList[i].fx-vx/float(mapU()))*mapZoom)),dy=y-(46+int((mapList[i].fy-vy/float(mapU()))*mapZoom)),d=dx*dx+dy*dy;if(d<best){best=d;hit=i;}}}else if(y>=62){unsigned r=(y-62)/16;unsigned e=mapPage*9+r;if(r<9&&e<mapCount)hit=e;}
  if(hit>=0){if(hit==mapSel&&nodes[mapList[hit].idx].count){selected=mapList[hit].idx;detail=true;mapScr=false;home=false;probeStart(nodes[selected].id);}else{mapSel=hit;mapSelId=nodes[mapList[hit].idx].id;mapPage=hit/9;}}else{mapSel=-1;mapSelId=0;if(mapZoom>1){mapCX=vx/float(mapU())+(x-6)/float(mapZoom);mapCY=vy/float(mapU())+(y-46)/float(mapZoom);}}}}else if(chatScr&&y>=46&&y<212){unsigned cp=max(1U,(chatTotal(chatCh)+3)/4);chatPage=y<130?(chatPage?chatPage-1:cp-1):(chatPage+1)%cp;}else if(specScr&&y>=78&&y<208){specWin=(specWin+1)%3;}else if(!home&&!specScr&&!detail&&y>=70&&y<207){unsigned row=(y-70)/27;if(row<5&&page*5+row<orderCount){selected=order[page*5+row];detail=true;probeStart(nodes[selected].id);}}else if(detail&&x<164&&y>=70&&y<151){probe.on=!probe.on;if(probe.on)probe.nextAt=now;}lastDraw=0;}}else pressed=false;
 // BOOT short press changes page; long press starts/stops logging (useful without touch).
 static uint32_t bootAt=0;static bool bootHeld=false;if(!digitalRead(0)){if(!bootHeld){bootHeld=true;bootAt=now;}}else if(bootHeld){bootHeld=false;if(now-bootAt>1000)toggleLog();else if(specScr||mapScr||chatScr){leaveSub();}else if(home)home=false;else{detail=false;page=(page+1)%max(1U,(orderCount+4)/5);}lastDraw=0;}
 if(logging&&now-lastFlush>2000){csv.flush();lastFlush=now;if(csv.getWriteError()){++logErrors;logging=false;csv.close();flash("SD ERROR");}}
 probeTick(now);if(dev.has&&now-utilAt>=10000){utilAt=now;utilRing.add(dev.chUtil);}
 if(!lastDraw||dirty||now-lastDraw>=(detail?500u:1000u)){dirty=false;draw();lastDraw=now;}delay(2);}
