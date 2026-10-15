#include "protocol.h"
#include <assert.h>
#include <vector>
#include <stdio.h>
#include <string>
using Bytes=std::vector<uint8_t>;
void var(Bytes& b,uint64_t v){do{b.push_back((v&127)|(v>127?128:0));v>>=7;}while(v);}
void vfield(Bytes& b,unsigned tag,uint64_t v){var(b,tag<<3);var(b,v);}
void fixed(Bytes& b,unsigned tag,uint32_t v){var(b,(tag<<3)|5);for(unsigned i=0;i<4;i++)b.push_back(v>>(8*i));}
void nested(Bytes& b,unsigned tag,const Bytes& a){var(b,(tag<<3)|2);var(b,a.size());b.insert(b.end(),a.begin(),a.end());}
int main(){
 {char o[48];auto D=[&](const char* in,size_t cap=48){mesh::displayName(in,o,cap);return std::string(o);};
  assert(D("Jan Kowalski")=="Jan Kowalski");
  assert(D("Zażółć gęślą jaźń")=="Zazolc gesla jazn");
  assert(D("ŁÓDŹ ŻÓŁW")=="LODZ ZOLW");
  assert(D("  a   b  ")=="a b");
  assert(D("Węzeł \xF0\x9F\x93\xA1 Radio")=="Wezel Radio");           // emoji dropped, spaces collapsed
  assert(D("\xF0\x9F\x93\xA1")=="");                                   // emoji only -> empty (caller falls back to id)
  assert(D("tab\there\nx")=="tab here x");
  assert(D("abc\xC5")=="abc");                                            // truncated 2-byte sequence at end
  assert(D("ab\xC5\x82")=="abl");
  assert(D("\xFF\xFEok")=="ok");                                        // invalid bytes
  assert(D("\xE2\x82\xAC")=="");                                        // euro sign has no glyph -> dropped
  assert(D("ABCDEFGHIJ",5)=="ABCD");                                      // respects capacity
  assert(D("",5)==""&&D(nullptr)=="");
  char z[1]={'x'};assert(mesh::displayName("a",z,0)==0&&z[0]=='x');}
 Bytes b;fixed(b,1,0x12345678);fixed(b,6,123);fixed(b,7,1700000000);float snr=-7.25;uint32_t bits;memcpy(&bits,&snr,4);fixed(b,8,bits);vfield(b,12,uint64_t(int64_t(-115)));vfield(b,15,3);vfield(b,9,2);vfield(b,21,1);mesh::Packet p;assert(mesh::packet(b.data(),b.size(),p));assert(p.from==0x12345678&&p.rssi==-115&&p.snr==-7.25&&p.hopStart-p.hopLimit==1&&p.transport==1);
 Bytes zero;vfield(zero,12,0);assert(mesh::packet(zero.data(),zero.size(),p)&&p.hasRssi&&p.rssi==0);Bytes none;assert(mesh::packet(none.data(),0,p)&&!p.hasRssi);
 Bytes pos;fixed(pos,1,uint32_t(-330000000));fixed(pos,2,210000000);fixed(pos,4,1700000000);mesh::Position q;assert(mesh::position(pos.data(),pos.size(),q)&&q.valid()&&q.lat==-330000000&&q.lon==210000000);Bytes partial;fixed(partial,1,0);assert(mesh::position(partial.data(),partial.size(),q)&&!q.valid());Bytes origin;fixed(origin,1,0);fixed(origin,2,0);assert(mesh::position(origin.data(),origin.size(),q)&&q.valid());
 uint8_t bad[]={0x12,0x80};mesh::Reader r(bad,2);mesh::Field f;assert(!r.next(f)&&!r.ok());uint8_t overflow[]={0x08,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x02};mesh::Reader ov(overflow,sizeof overflow);assert(!ov.next(f)&&!ov.ok());
 for(size_t i=1;i<b.size();i++){mesh::Reader truncated(b.data(),i);while(truncated.next(f)){} /* fuzz all prefix lengths under sanitizers */ }
 mesh::Framer frame;unsigned calls=0;Bytes stream={0x55,0x94,0x94,0xc3,0,uint8_t(b.size())};stream.insert(stream.end(),b.begin(),b.end());for(auto x:stream)frame.feed(x,0,[&](const uint8_t* a,size_t n){mesh::Packet t;assert(mesh::packet(a,n,t)&&t.rssi==-115);calls++;});assert(calls==1);
 for(auto x:Bytes{0x94,0xc3,3,0})frame.feed(x,0,[&](const uint8_t*,size_t){assert(false);});assert(frame.errors==1);
 frame.feed(0x94,0,[&](const uint8_t*,size_t){});frame.feed(0x94,2000,[&](const uint8_t*,size_t){});assert(frame.errors==2);
 // Malformed nested Data must fail atomically.
 Bytes corrupt; nested(corrupt,4,Bytes{0x12,0xff});assert(!mesh::packet(corrupt.data(),corrupt.size(),p));
 // Unknown fields are skipped, preserving compatibility.
 Bytes extended=b;nested(extended,100,Bytes{1,2,3});assert(mesh::packet(extended.data(),extended.size(),p)&&p.rssi==-115);

 // --- Probe: built ToRadio must round-trip through the independent reader. ---
 {uint8_t out[96];size_t len=mesh::buildProbe(out,sizeof out,0xdeadbeef,0x01020304,0x0a0b,256,0,0);assert(len>0&&out[0]==0x0a&&out[1]==len-2);
  mesh::Reader tr(out,len);mesh::Field t;assert(tr.next(t)&&t.tag==1&&t.wire==2&&!tr.next(t)&&tr.ok());
  mesh::Packet pr;assert(mesh::packet(out+2,len-2,pr));assert(pr.to==0xdeadbeef&&pr.id==0x01020304&&pr.port==256&&pr.payloadSize==4&&pr.payload[0]=='M'&&pr.payload[2]==0x0a&&pr.payload[3]==0x0b);
  Bytes mp(out+2,out+len);mesh::Reader mr(mp.data(),mp.size());bool ack=false;while(mr.next(t))if(t.tag==10&&t.wire==0&&t.value==1)ack=true;assert(ack&&mr.ok());
  assert(mesh::buildProbe(out,sizeof out,0,1,0,256,0,0)==0&&mesh::buildProbe(out,sizeof out,1,0,0,256,0,0)==0&&mesh::buildProbe(out,10,1,1,0,256,0,0)==0);
  size_t l2=mesh::buildProbe(out,sizeof out,0xdeadbeef,7,1,256,2,5);mesh::Reader hr(out+2,l2-2);bool ch=false,hp=false;while(hr.next(t)){if(t.tag==3&&t.value==2)ch=true;if(t.tag==9&&t.value==5)hp=true;}assert(ch&&hp&&hr.ok());}
 // --- ACK from remote node: ROUTING_APP data with request_id and Routing.Error. ---
 {Bytes routingNone;Bytes data;vfield(data,1,5);nested(data,2,routingNone);fixed(data,6,0x01020304);Bytes pk;fixed(pk,1,0xcafe0001);fixed(pk,2,0x11111111);nested(pk,4,data);vfield(pk,12,uint64_t(int64_t(-97)));float s=5.5f;uint32_t sb;memcpy(&sb,&s,4);fixed(pk,8,sb);
  mesh::Packet a;assert(mesh::packet(pk.data(),pk.size(),a)&&a.port==5&&a.requestId==0x01020304&&a.from==0xcafe0001&&a.to==0x11111111&&a.rssi==-97&&a.snr==5.5f);
  unsigned e=99;bool isAck=false;assert(mesh::routing(a.payload,a.payloadSize,e,isAck)&&e==0&&isAck);
  Bytes nak;vfield(nak,3,5);assert(mesh::routing(nak.data(),nak.size(),e,isAck)&&e==5&&isAck);Bytes tr2;nested(tr2,1,Bytes{});assert(mesh::routing(tr2.data(),tr2.size(),e,isAck)&&!isAck);
  uint8_t badr[]={0x18,0x80};assert(!mesh::routing(badr,2,e,isAck));}
 uint32_t rng=123;for(unsigned i=0;i<10000;i++){Bytes fuzz(i%200);for(auto& c:fuzz){rng=rng*1664525+1013904223;c=rng>>24;}mesh::packet(fuzz.data(),fuzz.size(),p);mesh::position(fuzz.data(),fuzz.size(),q);}
 puts("PASS: RSSI/SNR, signed GPS, missing fields, framing, malformed input, probe build/ACK parse, 10,000 fuzz cases");}
