import json, math
from shapely.geometry import shape, Point, box
from PIL import Image, ImageDraw, ImageFont
W,H=160,166; SS=4
vo=json.load(open('wojewodztwa-medium.geojson'))['features']
pw=json.load(open('powiaty.geojson'))['features']
riv=json.load(open('ne_10m_rivers_lake_centerlines.geojson'))['features']
maz=[shape(f['geometry']) for f in vo if f['properties']['nazwa']=='mazowieckie'][0]
minx,miny,maxx,maxy=maz.bounds
latmid=(miny+maxy)/2; k=math.cos(math.radians(latmid))
# okno: bbox + margines, proporcje W:H przy skali ekwirektangularnej (1 stopien lon = k stopnia lat)
latspan=(maxy-miny)+0.10; lonspan=latspan*k*W/H/k   # lonspan*k/latspan = W/H
lonspan=latspan*W/H/k
cx=(minx+maxx)/2; cy=(miny+maxy)/2
# jesli szerokosc nie miesci sie, powieksz
need=(maxx-minx)+0.10
if lonspan<need: lonspan=need; latspan=lonspan*k*H/W
LON_MIN,LON_MAX=cx-lonspan/2,cx+lonspan/2; LAT_MIN,LAT_MAX=cy-latspan/2,cy+latspan/2
print('bounds',LON_MIN,LON_MAX,LAT_MIN,LAT_MAX,'km/px',(LAT_MAX-LAT_MIN)*111.2/H)
def P(lon,lat): return ((lon-LON_MIN)/(LON_MAX-LON_MIN)*W*SS,(LAT_MAX-lat)/(LAT_MAX-LAT_MIN)*H*SS)
BG=(0,20,13); NEI=(5,40,29); NEIB=(18,86,66); MAZ=(10,76,54); POW=(22,118,88); MAZB=(0,255,224); RIV=(40,130,200); CITY=(255,255,255)
im=Image.new('RGB',(W*SS,H*SS),BG); d=ImageDraw.Draw(im)
def polys(g):
    return list(g.geoms) if g.geom_type in('MultiPolygon','GeometryCollection') else [g]
def draw_poly(g,fill=None,line=None,w=1):
    for p in polys(g):
        if p.geom_type!='Polygon': continue
        pts=[P(x,y) for x,y in p.exterior.coords]
        if fill: d.polygon(pts,fill=fill)
        if line: d.line(pts,fill=line,width=w,joint='curve')
draw_poly(maz,MAZ,None)
for f in pw:
    g=shape(f['geometry'])
    if maz.contains(g.representative_point()): draw_poly(g,None,POW,max(1,SS//4))
# rzeki
win=box(LON_MIN-0.2,LAT_MIN-0.2,LON_MAX+0.2,LAT_MAX+0.2)
for f in riv:
    nm=f['properties'].get('name')
    if nm not in ('Vistula','Bug'): continue
    g=shape(f['geometry']).intersection(maz)
    if g.is_empty: continue
    for ls in (g.geoms if hasattr(g,'geoms') else [g]):
        if ls.geom_type!='LineString': continue
        d.line([P(x,y) for x,y in ls.coords],fill=RIV,width=SS+1,joint='curve')
draw_poly(maz,None,MAZB,SS//2+2)
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',8*SS)
cities=[('Warszawa',52.2297,21.0122,'a'),('Radom',51.4027,21.1471,'r'),('Płock',52.5463,19.7065,'r'),('Siedlce',52.1676,22.2902,'b'),('Ostrołęka',53.0842,21.5740,'r'),
 ('Ciechanów',52.8815,20.6199,'r'),('Żyrardów',52.0488,20.4448,None),('Mińsk Maz.',52.1779,21.5724,None),('Sochaczew',52.2297,20.2370,None),('Mława',53.1130,20.3830,None),
 ('Garwolin',51.8975,21.6140,None),('Węgrów',52.4013,22.0190,None)]
for n,la,lo,pos in cities:
    x,y=P(lo,la); r=(2.2 if pos else 1.5)*SS
    d.ellipse([x-r,y-r,x+r,y+r],fill=(200,235,225) if pos else (120,190,170))
    if pos:
        tw=d.textlength(n,font=font)
        if pos=='r': tx,ty=x+r+2*SS,y-4*SS
        elif pos=='a': tx,ty=x-tw/2,y-r-10*SS
        else: tx,ty=x-tw/2,y+r+1*SS
        tx=min(max(tx,2*SS),W*SS-2*SS-tw)
        d.text((tx,ty),n,font=font,fill=(220,255,245))
# skala 50 km
kmpx=(LAT_MAX-LAT_MIN)*111.2/H; L=50/kmpx
x0=6*SS; y0=(H-8)*SS
d.line([(x0,y0),(x0+L*SS,y0)],fill=(200,240,230),width=SS); d.line([(x0,y0-3*SS),(x0,y0+3*SS)],fill=(200,240,230),width=SS//2+1); d.line([(x0+L*SS,y0-3*SS),(x0+L*SS,y0+3*SS)],fill=(200,240,230),width=SS//2+1)
f2=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',7*SS); d.text((x0+L*SS+3*SS,y0-4*SS),'50 km',font=f2,fill=(200,240,230))
out=im.resize((W,H),Image.LANCZOS); out.save('mazowsze_160x166.png'); out.resize((W*4,H*4),Image.NEAREST).save('mazowsze_preview.png')
mk=Image.new('L',(W*SS,H*SS),0);ImageDraw.Draw(mk).polygon([P(x,y) for x,y in maz.exterior.coords] if maz.geom_type=='Polygon' else [P(x,y) for x,y in max(maz.geoms,key=lambda g:g.area).exterior.coords],fill=255)
mk=mk.resize((W,H),Image.LANCZOS);mv=list(mk.getdata());mbytes=[]
for i in range(0,W*H,8):
    bb=0
    for j in range(8):
        if i+j<W*H and mv[i+j]>=128: bb|=1<<j
    mbytes.append(bb)
inside=sum(1 for v in mv if v>=128)
px=out.getdata(); v=['0x%04X'%(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3)) for r,g,b in px]
hdr=f'''#pragma once
#include <Arduino.h>
// Tlo mapy Mazowsza (dane: granice wojewodztw/powiatow ppatrzyk/polska-geojson, rzeki Natural Earth), RGB565, uzyj setSwapBytes(true).
// Odwzorowanie ekwirektangularne: x=(lon-MAP_LON_MIN)/(MAP_LON_MAX-MAP_LON_MIN)*MAP_W, y=(MAP_LAT_MAX-lat)/(MAP_LAT_MAX-MAP_LAT_MIN)*MAP_H.
constexpr unsigned MAP_W={W}, MAP_H={H};
constexpr double MAP_LON_MIN={LON_MIN:.6f}, MAP_LON_MAX={LON_MAX:.6f}, MAP_LAT_MIN={LAT_MIN:.6f}, MAP_LAT_MAX={LAT_MAX:.6f};
const uint16_t mapImg[MAP_W*MAP_H] PROGMEM = {{
'''+',\n'.join(','.join(v[i:i+16]) for i in range(0,len(v),16))+'\n};\n'
hdr+='// Maska wojewodztwa (1 bit/piksel, wiersz po wierszu, LSB pierwszy): piksel lezy w granicach Mazowsza.\nconst uint8_t mapMask[%d] PROGMEM = {\n'%len(mbytes)+','.join('0x%02X'%b for b in mbytes)+'\n};\n'
open('mapdata.h','w').write(hdr);print('mask inside px',inside,'of',W*H)
