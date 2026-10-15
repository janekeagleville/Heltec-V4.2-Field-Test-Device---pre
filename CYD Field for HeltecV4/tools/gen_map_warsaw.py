# Mapa Warszawy i okolic (do Pultuska i Rozana na polnocy) dla zoomu 6x..12x -> mapwaw.h
# Uklad wspolrzednych: "wirtualna" mapa Mazowsza w skali MAPW_S=6 (6x gestsza niz mapImg); obraz to wycinek [X0,X0+W)x[Y0,Y0+H) tej mapy,
# wiec pozycje wezlow (fx*6, fy*6) trafiaja na piksel 1:1 z ekranem przy zoomie 6x. 16 kolorow, 4 bit/piksel.
import json, re
from shapely.geometry import shape
from PIL import Image, ImageDraw, ImageFont
S=12; SS=2
src=open('mapdata.h').read(); g=lambda n:float(re.search(n+r'=([0-9.]+)',src).group(1))
LON_MIN,LON_MAX,LAT_MIN,LAT_MAX=g('MAP_LON_MIN'),g('MAP_LON_MAX'),g('MAP_LAT_MIN'),g('MAP_LAT_MAX')
MW,MH=160*S,166*S
def V(lon,lat): return ((lon-LON_MIN)/(LON_MAX-LON_MIN)*MW,(LAT_MAX-lat)/(LAT_MAX-LAT_MIN)*MH)
# zakres: Pultusk (52.707N) i Rozan (52.863N, 21.433E) na polnocy, Warszawa w srodku
bx0,by0=V(20.28,52.93); bx1,by1=V(21.74,51.90)
X0,Y0=int(bx0),int(by0); W,H=int(bx1)-X0+1,int(by1)-Y0+1
print('X0,Y0,W,H',X0,Y0,W,H,'km/px',(LAT_MAX-LAT_MIN)*111.2/MH)
def P(lon,lat): x,y=V(lon,lat); return ((x-X0)*SS,(y-Y0)*SS)
vo=json.load(open('wojewodztwa-medium.geojson'))['features']
pw=json.load(open('powiaty.geojson'))['features']
riv=json.load(open('ne_10m_rivers_lake_centerlines.geojson'))['features']
dz=json.load(open('dzielnice.geojson'))['features']
from shapely.geometry import box as _box
BB=_box(20.0,51.7,22.0,53.1)
def load(n): return [f for f in json.load(open(n))['features'] if f['geometry'] and shape(f['geometry']).intersects(BB)]
roads=load('ne_10m_roads.geojson'); rails=load('ne_10m_railroads.geojson'); rivE=load('ne_10m_rivers_europe.geojson'); urban=load('ne_10m_urban_areas.geojson')
maz=[shape(f['geometry']) for f in vo if f['properties']['nazwa']=='mazowieckie'][0]
BG=(0,20,13); MAZ=(10,76,54); URB=(18,96,70); POW=(30,128,96); MAZB=(0,255,224); RIV=(40,130,200); DZ=(70,180,150); CITYB=(120,235,210); ROAD=(235,170,60); RAIL=(150,165,160)
im=Image.new('RGB',(W*SS,H*SS),BG); d=ImageDraw.Draw(im)
def polys(gm): return list(gm.geoms) if gm.geom_type in('MultiPolygon','GeometryCollection') else [gm]
def poly(gm,fill=None,line=None,w=1):
    for p in polys(gm):
        if p.geom_type!='Polygon' or p.is_empty: continue
        pts=[P(x,y) for x,y in p.exterior.coords]
        if fill: d.polygon(pts,fill=fill)
        if line: d.line(pts+[pts[0]],fill=line,width=w,joint='curve')
poly(maz,MAZ)
for f in pw:
    gm=shape(f['geometry'])
    if maz.contains(gm.representative_point()): poly(gm,None,POW,3*SS)
for f in urban: poly(shape(f['geometry']).intersection(maz),URB)
def lines(gm,col,w):
    for ls in (gm.geoms if hasattr(gm,'geoms') else [gm]):
        if ls.geom_type=='LineString': d.line([P(x,y) for x,y in ls.coords],fill=col,width=w*SS,joint='curve')
for f in rivE:
    if f['properties'].get('name') in ('Narew','Wkra','Bzura','Pilica'): lines(shape(f['geometry']).intersection(maz),RIV,4)
for f in rails: lines(shape(f['geometry']),RAIL,3)
for f in roads: lines(shape(f['geometry']),ROAD,5 if f['properties'].get('type')=='Secondary Highway' else 4)
for f in riv:
    if f['properties'].get('name') not in ('Vistula','Bug'): continue
    gm=shape(f['geometry']).intersection(maz)
    if gm.is_empty: continue
    for ls in (gm.geoms if hasattr(gm,'geoms') else [gm]):
        if ls.geom_type=='LineString': d.line([P(x,y) for x,y in ls.coords],fill=RIV,width=7*SS,joint='curve')
for f in dz: poly(shape(f['geometry']),None,DZ,3*SS)
poly(shape(dz[0]['geometry']) if dz[0]['properties']['name']=='Warszawa' else maz,None,CITYB,5*SS)
poly(maz,None,MAZB,6*SS)
F=lambda s:ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',s*SS)
fW,fT,fD=F(36),F(24),F(18); fV=F(17); fR=F(14)
placed=[]
BIGT={'Pułtusk','Różan','Legionowo','Wołomin','Otwock','Piaseczno','Pruszków','Nowy Dwór Maz.','Mińsk Maz.','Grodzisk Maz.','Wyszków','Sochaczew','Ciechanów'}
def label(txt,x,y,font,anchor,col):
    tw=d.textlength(txt,font=font); th=font.size
    if anchor=='c': tx,ty=x-tw/2,y-th/2
    elif anchor=='r': tx,ty=x+10*SS,y-th/2
    elif anchor=='l': tx,ty=x-10*SS-tw,y-th/2
    elif anchor=='a': tx,ty=x-tw/2,y-th-9*SS
    else: tx,ty=x-tw/2,y+8*SS
    tx=min(max(tx,3*SS),W*SS-3*SS-tw); ty=min(max(ty,3*SS),H*SS-3*SS-th)
    r=(tx-2*SS,ty,tx+tw+2*SS,ty+th*1.2)
    if any(not(r[2]<q[0] or r[0]>q[2] or r[3]<q[1] or r[1]>q[3]) for q in placed): return False
    placed.append(r); d.text((tx,ty),txt,font=font,fill=col); return True
x,y=P(21.0122,52.2297); label('Warszawa',x,y,fW,'c',(235,255,250))
towns=[('Pułtusk',52.7068,21.0850),('Różan',52.8634,21.4326),('Legionowo',52.4006,20.9262),('Wołomin',52.3450,21.2417),('Otwock',52.1058,21.2614),('Piaseczno',52.0813,21.0237),
 ('Pruszków',52.1700,20.8115),('Nowy Dwór Maz.',52.4336,20.7233),('Mińsk Maz.',52.1779,21.5724),('Grodzisk Maz.',52.1067,20.6310),('Wyszków',52.5925,21.4572),('Serock',52.5167,21.0833),
 ('Nasielsk',52.5967,20.8000),('Radzymin',52.4167,21.1767),('Tłuszcz',52.4167,21.4333),('Konstancin',52.0903,21.1136),('Góra Kalwaria',51.9747,21.2133),('Marki',52.3333,21.1000),
 ('Ząbki',52.2900,21.1100),('Łomianki',52.3347,20.8878),('Ożarów Maz.',52.2167,20.8000),('Błonie',52.1944,20.6128),('Mszczonów',52.1736,20.5217),('Karczew',52.0764,21.2486),
 ('Józefów',52.1269,21.2342),('Brwinów',52.1333,20.7167),('Zielonka',52.3000,21.1667),('Sulejówek',52.2467,21.2833),('Halinów',52.2300,21.3000),('Kobyłka',52.3400,21.1900),
 ('Maków Maz.',52.8670,21.1010),('Wieliszew',52.3922,20.9617),('Zegrze',52.4478,21.0436),('Nieporęt',52.3731,21.0289),('Jabłonna',52.3897,20.9114),('Stare Babice',52.2806,20.8456),('Kampinos',52.3169,20.4944),('Leszno',52.3039,20.6642),('Czosnów',52.4294,20.7708),('Pomiechówek',52.5008,20.6833),('Zakroczym',52.4358,20.6097),('Tarczyn',51.9772,20.8694),('Nadarzyn',52.0997,20.8036),('Raszyn',52.1558,20.9269),('Lesznowola',52.0692,20.9425),('Piastów',52.1833,20.8500),('Podkowa Leśna',52.1167,20.7167),('Milanówek',52.1167,20.6667),('Teresin',52.2000,20.4333),('Sochaczew',52.2297,20.2370),('Ciechanów',52.8815,20.6199),('Grójec',51.8647,20.8631),('Garwolin',51.8975,21.6140)]
for n,la,lo in towns:
    x,y=P(lo,la)
    if not(6*SS<x<(W-6)*SS and 6*SS<y<(H-6)*SS): continue
    r=(7 if n in BIGT else 5)*SS; d.ellipse([x-r,y-r,x+r,y+r],fill=(200,235,225))
    for a in('r','l','b','a'):
        if label(n,x,y,fT if n in BIGT else fV,a,(215,250,240)): break
for n,la,lo in [('Lotnisko Chopina',52.1657,20.9671),('Modlin',52.4511,20.6518)]:
    x,y=P(lo,la); r=8*SS; d.polygon([(x,y-r),(x+r,y),(x,y+r),(x-r,y)],fill=(255,255,255),outline=ROAD)
    for a in('b','a','r','l'):
        if label(n,x,y,fV,a,(255,235,170)): break
for f in dz[1:]:
    c=shape(f['geometry']).representative_point(); x,y=P(c.x,c.y); label(f['properties']['name'],x,y,fD,'c',(150,215,195))
# skala 10 km
kmpx=(LAT_MAX-LAT_MIN)*111.2/MH; L=10/kmpx; x0=10*SS; y0=(H-14)*SS; c=(200,240,230)
d.line([(x0,y0),(x0+L*SS,y0)],fill=c,width=2*SS); d.line([(x0,y0-4*SS),(x0,y0+4*SS)],fill=c,width=SS); d.line([(x0+L*SS,y0-4*SS),(x0+L*SS,y0+4*SS)],fill=c,width=SS)
d.text((x0+L*SS+5*SS,y0-6*SS),'10 km',font=fD,fill=c)
out=im.resize((W,H),Image.LANCZOS); q=out.quantize(16,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
pal=q.getpalette()[:48]; pal565=[((pal[i*3]&0xF8)<<8)|((pal[i*3+1]&0xFC)<<3)|(pal[i*3+2]>>3) for i in range(16)]
q.convert('RGB').save('mazowsze_warszawa.png'); idx=list(q.getdata()); idx+=[0]*(len(idx)%2)
by=[(idx[i]&15)|((idx[i+1]&15)<<4) for i in range(0,len(idx),2)]
hdr='''#pragma once
#include <Arduino.h>
// Warszawa i okolice (od Sochaczewa po Minsk Maz., od Grojca po Rozan) - mapa dla maksymalnego zoomu. Wycinek [X0,X0+W)x[Y0,Y0+H) "wirtualnej" mapy Mazowsza
// w skali MAPW_S (pozycja wezla w pikselach = fx*MAPW_S, fy*MAPW_S), 16 kolorow, 4 bit/piksel (piksel parzysty = mlodszy polbajt), kolor = mapWPal[indeks].
constexpr int MAPW_S=%d, MAPW_X0=%d, MAPW_Y0=%d, MAPW_W=%d, MAPW_H=%d;
const uint16_t mapWPal[16] PROGMEM = {%s};
const uint8_t mapW[(MAPW_W*MAPW_H+1)/2] PROGMEM = {
'''%(S,X0,Y0,W,H,','.join('0x%04X'%v for v in pal565))+',\n'.join(','.join('0x%02X'%b for b in by[i:i+24]) for i in range(0,len(by),24))+'\n};\n'
open('mapwaw.h','w').write(hdr); print('bytes',len(by))
