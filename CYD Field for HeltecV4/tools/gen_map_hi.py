# Mapa Mazowsza w wysokiej rozdzielczosci: 4x wieksza niz mapdata.h (640x664), 16 kolorow (4 bit/piksel) -> maphires.h
# Te same granice geograficzne co mapdata.h (MAP_LON_*/MAP_LAT_*), wiec pozycje wezlow pasuja do obu obrazow.
import json, math, re, sys
from shapely.geometry import shape, box
from PIL import Image, ImageDraw, ImageFont
HS=4; W,H=160*HS,166*HS; SS=2; U=HS*SS      # U = piksele plotna na 1 piksel niskiej rozdzielczosci
vo=json.load(open('wojewodztwa-medium.geojson'))['features']
pw=json.load(open('powiaty.geojson'))['features']
riv=json.load(open('ne_10m_rivers_lake_centerlines.geojson'))['features']
maz=[shape(f['geometry']) for f in vo if f['properties']['nazwa']=='mazowieckie'][0]
src=open('mapdata.h').read()
g=lambda n:float(re.search(n+r'=([0-9.]+)',src).group(1))
LON_MIN,LON_MAX,LAT_MIN,LAT_MAX=g('MAP_LON_MIN'),g('MAP_LON_MAX'),g('MAP_LAT_MIN'),g('MAP_LAT_MAX')
def P(lon,lat): return ((lon-LON_MIN)/(LON_MAX-LON_MIN)*W*SS,(LAT_MAX-lat)/(LAT_MAX-LAT_MIN)*H*SS)
BG=(0,20,13); MAZ=(10,76,54); POW=(22,118,88); MAZB=(0,255,224); RIV=(40,130,200)
im=Image.new('RGB',(W*SS,H*SS),BG); d=ImageDraw.Draw(im)
def polys(gm): return list(gm.geoms) if gm.geom_type in('MultiPolygon','GeometryCollection') else [gm]
def draw_poly(gm,fill=None,line=None,w=1):
    for p in polys(gm):
        if p.geom_type!='Polygon': continue
        pts=[P(x,y) for x,y in p.exterior.coords]
        if fill: d.polygon(pts,fill=fill)
        if line: d.line(pts+[pts[0]],fill=line,width=w,joint='curve')
draw_poly(maz,MAZ,None)
for f in pw:
    gm=shape(f['geometry'])
    if maz.contains(gm.representative_point()): draw_poly(gm,None,POW,2*SS//2*1+1)
for f in riv:
    if f['properties'].get('name') not in ('Vistula','Bug'): continue
    gm=shape(f['geometry']).intersection(maz)
    if gm.is_empty: continue
    for ls in (gm.geoms if hasattr(gm,'geoms') else [gm]):
        if ls.geom_type=='LineString': d.line([P(x,y) for x,y in ls.coords],fill=RIV,width=3*SS,joint='curve')
draw_poly(maz,None,MAZB,3*SS)
fb=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',22*SS)
fs=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',15*SS)
big=[('Warszawa',52.2297,21.0122,'a'),('Radom',51.4027,21.1471,'r'),('Płock',52.5463,19.7065,'r'),('Siedlce',52.1676,22.2902,'b'),('Ostrołęka',53.0842,21.5740,'r'),('Ciechanów',52.8815,20.6199,'r')]
small=[('Żyrardów',52.0488,20.4448),('Mińsk Maz.',52.1779,21.5724),('Sochaczew',52.2297,20.2370),('Mława',53.1130,20.3830),('Garwolin',51.8975,21.6140),('Węgrów',52.4013,22.0190),
 ('Pruszków',52.1700,20.8115),('Piaseczno',52.0813,21.0237),('Otwock',52.1058,21.2614),('Wołomin',52.3450,21.2417),('Legionowo',52.4006,20.9262),('Pułtusk',52.7068,21.0850),('Sierpc',52.8564,19.6742),
 ('Przasnysz',53.0186,20.8800),('Kozienice',51.5817,21.5478),('Szydłowiec',51.2236,20.8586),('Łosice',52.2148,22.7226),('Grodzisk Maz.',52.1067,20.6310),('Ostrów Maz.',52.8003,21.8517),
 ('Gostynin',52.4264,19.4719),('Grójec',51.8647,20.8631),('Zwoleń',51.3578,21.5883),('Maków Maz.',52.8670,21.1010),('Pionki',51.4947,21.4567)]
placed=[]
def label(txt,x,y,font,anchor,col):
    tw=d.textlength(txt,font=font); th=font.size
    if anchor=='r': tx,ty=x+7*SS,y-th/2
    elif anchor=='a': tx,ty=x-tw/2,y-th-7*SS
    else: tx,ty=x-tw/2,y+6*SS
    tx=min(max(tx,3*SS),W*SS-3*SS-tw); ty=min(max(ty,3*SS),H*SS-3*SS-th)
    r=(tx-2*SS,ty,tx+tw+2*SS,ty+th*1.15)
    if any(not(r[2]<q[0] or r[0]>q[2] or r[3]<q[1] or r[1]>q[3]) for q in placed): return False
    placed.append(r); d.text((tx,ty),txt,font=font,fill=col,stroke_width=SS//2+1 if False else 0); return True
for n,la,lo,a in big:
    x,y=P(lo,la); r=6*SS; d.ellipse([x-r,y-r,x+r,y+r],fill=(200,235,225)); label(n,x,y,fb,a,(230,255,248))
for n,la,lo in small:
    x,y=P(lo,la); r=3*SS; d.ellipse([x-r,y-r,x+r,y+r],fill=(120,190,170))
    for a in('r','b','a'):
        if label(n,x,y,fs,a,(170,225,210)): break
kmpx=(LAT_MAX-LAT_MIN)*111.2/(H/HS); L=50/kmpx*HS
x0=8*HS*SS; y0=(H-30)*SS; c=(200,240,230)
d.line([(x0,y0),(x0+L*SS,y0)],fill=c,width=3*SS); d.line([(x0,y0-8*SS),(x0,y0+8*SS)],fill=c,width=2*SS); d.line([(x0+L*SS,y0-8*SS),(x0+L*SS,y0+8*SS)],fill=c,width=2*SS)
d.text((x0+L*SS+8*SS,y0-10*SS),'50 km',font=fs,fill=c)
out=im.resize((W,H),Image.LANCZOS)
q=out.quantize(16,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
pal=q.getpalette()[:48]; pal565=[((pal[i*3]&0xF8)<<8)|((pal[i*3+1]&0xFC)<<3)|(pal[i*3+2]>>3) for i in range(16)]
q.convert('RGB').save('mazowsze_hires.png'); idx=list(q.getdata()); assert max(idx)<16
by=[(idx[i]&15)|((idx[i+1]&15)<<4) for i in range(0,len(idx),2)]
hdr='''#pragma once
#include <Arduino.h>
// Mapa Mazowsza w wysokiej rozdzielczosci: MAPH_S x wiecej pikseli niz mapImg (te same granice MAP_LON_*/MAP_LAT_*), 16 kolorow, 4 bit/piksel
// (piksel parzysty = mlodszy polbajt). Uzywana przy powiekszeniu mapy; kolor = mapPal[indeks] (RGB565, setSwapBytes(true)).
constexpr unsigned MAPH_S=%d, MAPH_W=MAP_W*MAPH_S, MAPH_H=MAP_H*MAPH_S;
const uint16_t mapPal[16] PROGMEM = {%s};
const uint8_t mapHi[MAPH_W*MAPH_H/2] PROGMEM = {
'''%(HS,','.join('0x%04X'%v for v in pal565))+',\n'.join(','.join('0x%02X'%b for b in by[i:i+24]) for i in range(0,len(by),24))+'\n};\n'
open('maphires.h','w').write(hdr); print('bytes',len(by),'pal',[hex(v) for v in pal565])
