# -*- coding: utf-8 -*-
"""외부 에셋 없이 편집 가능한 저폴리 국가 조형물 8종을 만든다.

실측 복원이 아닌 대표 실루엣의 캐주얼 모형이다. OBJ의 단위는 cm, 위는 +Z.
"""
from pathlib import Path
import math
import json
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'SourceArt/UI/Atlas/Landmarks_v1'
COLORS = {'stone':(0.87,0.78,0.57),'ivory':(0.96,0.92,0.80),'roof':(0.12,0.34,0.36),'red':(0.78,0.20,0.12),'gold':(1.0,0.68,0.18),'jade':(0.22,0.61,0.47),'dark':(0.19,0.25,0.25),'blue':(0.35,0.64,0.71)}

class Mesh:
    def __init__(self): self.v=[]; self.faces=[]
    def poly(self,points,mat):
        start=len(self.v)
        self.v.extend(points)
        for k in range(1,len(points)-1): self.faces.append(([start,start+k,start+k+1],mat))
    def rings(self,rings,mat):
        n=len(rings[0])
        self.poly(list(reversed(rings[0])),mat)
        for low,high in zip(rings,rings[1:]):
            for j in range(n): self.poly([low[j],low[(j+1)%n],high[(j+1)%n],high[j]],mat)
        self.poly(rings[-1],mat)
    def box(self,x,y,z,w,d,h,mat):
        self.rings([[(x-w/2,y-d/2,z),(x+w/2,y-d/2,z),(x+w/2,y+d/2,z),(x-w/2,y+d/2,z)],[(x-w/2,y-d/2,z+h),(x+w/2,y-d/2,z+h),(x+w/2,y+d/2,z+h),(x-w/2,y+d/2,z+h)]],mat)
    def cone(self,x,y,z,r,h,mat,top=0.1,n=12):
        self.rings([[(x+rad*math.cos(i*2*math.pi/n),y+rad*math.sin(i*2*math.pi/n),zz) for i in range(n)] for rad,zz in [(r,z),(top,z+h)]],mat)
    def roof(self,x,y,z,w,d,h,mat):
        # 넓은 처마와 좁은 용마루를 가진 팔작지붕의 단순화.
        self.rings([[(x-ww/2,y-dd/2,zz),(x+ww/2,y-dd/2,zz),(x+ww/2,y+dd/2,zz),(x-ww/2,y+dd/2,zz)] for ww,dd,zz in [(w,d,z),(w*.90,d*.90,z+2),(w*.50,3,z+h)]],mat)
    def beam(self,a,b,r,mat):
        dx,dy,dz=[b[i]-a[i] for i in range(3)]; length=math.sqrt(dx*dx+dy*dy+dz*dz)
        axis=[dx/length,dy/length,dz/length]
        ref=[0,0,1] if abs(axis[2])<.9 else [1,0,0]
        u=[axis[1]*ref[2]-axis[2]*ref[1],axis[2]*ref[0]-axis[0]*ref[2],axis[0]*ref[1]-axis[1]*ref[0]]
        mag=math.sqrt(sum(t*t for t in u));u=[t/mag for t in u]
        v=[axis[1]*u[2]-axis[2]*u[1],axis[2]*u[0]-axis[0]*u[2],axis[0]*u[1]-axis[1]*u[0]]
        self.rings([[tuple(p[k]+r*(u[k]*math.cos(t*math.pi/4)+v[k]*math.sin(t*math.pi/4)) for k in range(3)) for t in range(8)] for p in [a,b]],mat)
    def save(self,name):
        lines=['# FestaFiesta 직접 제작 캐주얼 국가 조형물','mtllib landmarks.mtl','o '+name]
        lines += ['v %.5f %.5f %.5f'%v for v in self.v]
        prev=None
        for face,mat in self.faces:
            if mat!=prev:lines.append('usemtl '+mat);prev=mat
            lines.append('f '+' '.join(str(i+1) for i in face))
        (OUT/(name+'.obj')).write_text('\n'.join(lines),encoding='utf-8')

def build(country):
    m=Mesh()
    if country=='KR':
        m.box(0,0,0,100,65,6,'stone')
        for x in [-38,-19,0,19,38]:
            for y in [-22,0,22]:m.cone(x,y,6,2.6,22,'ivory',2.6)
        m.box(0,0,28,85,50,5,'red')
        for x in [-35,-17,0,17,35]:
            for y in [-19,19]:m.box(x,y,33,3,3,19,'red')
        m.box(0,0,35,72,30,13,'ivory');m.roof(0,0,51,103,67,19,'roof')
        m.beam((-24,0,71),(24,0,71),1.5,'gold')
    elif country=='BR':
        m.box(0,0,0,29,23,12,'stone');m.cone(0,0,12,12,58,'ivory',6)
        m.cone(0,0,70,7,13,'ivory',5)
        m.beam((-36,0,63),(36,0,63),4,'ivory');m.box(-37,0,60,9,5,5,'ivory');m.box(37,0,60,9,5,5,'ivory')
    elif country=='DE':
        m.box(0,0,0,105,38,6,'stone')
        for x in [-44,-28,-12,12,28,44]:
            for y in [-11,11]:
                m.cone(x,y,6,3.8,37,'ivory',3.3);m.box(x,y,42,9,9,4,'stone')
        m.box(0,0,46,108,35,10,'stone');m.box(0,0,56,94,27,4,'ivory')
        m.box(0,0,60,24,18,8,'jade')
        for x in [-10,-3,4,11]:
            m.box(x,3,68,4,12,6,'jade');m.beam((x,6,71),(x,9,80),2,'jade')
    elif country=='JP':
        for x in [-31,31]:
            m.cone(x,0,0,4,66,'red',3.2);m.cone(x,0,0,5,9,'dark',5)
        m.box(0,0,46,79,7,7,'red');m.box(0,0,63,91,10,6,'red')
        m.box(0,0,69,101,13,4,'dark');m.box(0,0,48,5,5,15,'red')
        m.beam((-50,0,71),(-39,0,68),2,'dark');m.beam((39,0,68),(50,0,71),2,'dark')
    elif country=='US':
        m.box(0,0,0,29,26,8,'stone');m.box(0,0,8,21,20,17,'stone')
        m.cone(0,0,25,11,41,'jade',5);m.cone(0,0,66,6,10,'jade',5)
        m.beam((-4,0,61),(-16,0,86),3,'jade');m.cone(-16,0,86,3,8,'gold',.4)
        m.box(8,-1,45,6,4,18,'jade');m.box(8,-3,49,9,3,11,'roof')
        for a in range(7):
            theta=math.pi*a/6
            m.beam((5*math.cos(theta),0,74+2*math.sin(theta)),(9*math.cos(theta),0,75+7*math.sin(theta)),.7,'jade')
    elif country=='ES':
        m.box(0,0,0,63,48,6,'stone');m.box(0,0,6,48,35,25,'ivory');m.roof(0,0,31,52,38,13,'stone')
        for x,y,h in [(-23,-16,64),(-10,-16,73),(10,-16,73),(23,-16,64),(-18,15,56),(18,15,56),(0,6,91)]:
            m.cone(x,y,5,4.8,h,'stone',2.2);m.cone(x,y,h+5,4,9,'gold',.2)
            for z in range(20,int(h),12):m.box(x,y-4,z,2,1,5,'dark')
    elif country=='TH':
        m.box(0,0,0,78,52,6,'stone');m.box(0,0,6,60,37,29,'ivory')
        for y,z,w,d in [(0,35,80,55),(0,45,65,43),(0,55,47,30)]:m.roof(0,y,z,w,d,15,'gold')
        m.cone(0,0,70,4,20,'gold',.1)
        for x in [-32,32]:
            for y in [-20,20]:m.cone(x,y,6,2,32,'red',2)
    elif country=='AU':
        m.box(0,0,0,111,68,7,'stone')
        for cx,cy,w,d,h in [(-30,-10,29,42,45),(0,-8,35,49,61),(31,-8,29,41,46),(-15,21,24,25,28),(16,21,24,25,32)]:
            # 돛 형태의 곡면 쉘. 여러 폭의 아치 띠를 연결한다.
            sections=[]
            for i in range(9):
                t=i/8;xx=cx-w/2+w*t;arch=math.sin(math.pi*t)**.8
                sections.append([(xx,cy-d/2,7),(xx,cy-d*.12,7+h*arch),(xx,cy+d/2,7)])
            for a,b in zip(sections,sections[1:]):
                for k in range(2):m.poly([a[k],b[k],b[k+1],a[k+1]],'ivory');m.poly([a[k+1],b[k+1],b[k],a[k]],'ivory')
    return m

def preview(meshes):
    im=Image.new('RGB',(1600,800),'#edf4ee');draw=ImageDraw.Draw(im)
    labels={'KR':'KOREA / Gyeonghoeru','BR':'BRAZIL / Christ statue','DE':'GERMANY / Brandenburg Gate','JP':'JAPAN / Torii','US':'USA / Liberty','ES':'SPAIN / Sagrada Familia','TH':'THAILAND / Temple','AU':'AUSTRALIA / Opera House'}
    for idx,(code,m) in enumerate(meshes.items()):
        cx=200+(idx%4)*400; cy=310+(idx//4)*400
        verts=[(cx+(x-y*.45)*2.3,cy-(z+y*.35)*2.3) for x,y,z in m.v]
        for face,mat in sorted(m.faces,key=lambda f:sum(m.v[i][1]-m.v[i][0]*.3+m.v[i][2]*.1 for i in f[0]),reverse=True):
            col=tuple(int(v*255) for v in COLORS[mat]);draw.polygon([verts[i] for i in face],fill=col,outline=tuple(int(c*.82) for c in col))
        draw.text((cx-170,cy+45),labels[code],fill='#24413d')
    im.save(OUT/'Landmarks_ContactSheet.png')

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    mtl=[]
    for name,color in COLORS.items():mtl.extend(['newmtl '+name,'Kd '+' '.join(map(str,color)),'Ks 0 0 0','Ns 1'])
    (OUT/'landmarks.mtl').write_text('\n'.join(mtl))
    meshes={code:build(code) for code in ['KR','BR','DE','JP','US','ES','TH','AU']}
    for code,m in meshes.items():m.save('SM_FF_Landmark_'+code)
    preview(meshes)
    (OUT/'manifest.json').write_text(json.dumps({k:{'triangles':len(m.faces),'vertices':len(m.v)} for k,m in meshes.items()},indent=2))
    print(OUT)

if __name__=='__main__':main()
