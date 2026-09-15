"""원본 해안과 ETOPO1에서 전 세계 디오라마 메시를 8개 경도 구간으로 제작한다."""
from pathlib import Path
from collections import defaultdict
import math,json,hashlib
import numpy as np
from PIL import Image
from build_diorama_pilot import sample

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'SourceArt/UI/Atlas/DioramaWorld_v1'
SRC=ROOT/'SourceArt/UI/Title/CasualClay_Albedo_v1/GeographyLocked'
DEM=ROOT/'SourceArt/ThirdParty/NOAA/ETOPO1/global_10arcmin.csv'
R=5000.;BASE=100.;MOUNTAIN=350.

def smooth(a,passes=2):
    for _ in range(passes):
        p=np.pad(a,((1,1),(0,0)),mode='edge')
        a=(4*a+p[:-2]+p[2:]+np.roll(a,1,axis=1)+np.roll(a,-1,axis=1))/8
    return a

def write(name,verts,uv,faces):
    verts=np.asarray(verts);faces=np.asarray(faces,dtype=int)
    cross=np.cross(verts[faces[:,1]]-verts[faces[:,0]],verts[faces[:,2]]-verts[faces[:,0]])
    faces=faces[np.linalg.norm(cross,axis=1)>1e-6]
    with (OUT/(name+'.obj')).open('w',encoding='utf8') as f:
        f.write('# FestaFiesta 전 세계 디오라마 / 원본 해안 + NOAA ETOPO1\no '+name+'\ns 1\n')
        for p in verts:f.write('v %.6f %.6f %.6f\n'%tuple(p))
        for u,v in uv:f.write('vt %.9f %.9f\n'%(u,1-v))
        for face in faces:f.write('f '+' '.join(f'{i+1}/{i+1}' for i in face)+'\n')
    return len(faces)

def run():
    OUT.mkdir(parents=True,exist_ok=True)
    data=np.loadtxt(DEM,delimiter=',',skiprows=2)
    assert data.shape==(1081*2161,3)
    dem=data[:,2].reshape(1081,2161)
    assert np.isfinite(dem).all() and dem.max()<10000
    x,y=np.meshgrid(np.arange(2048),np.arange(1024))
    lon=(x+.5)/2048*360-180;lat=90-(y+.5)/1024*180
    elev=np.maximum(0,sample(dem,(lon%360)*6,(lat+90)*6))
    elev=smooth(elev,5)
    cls=np.asarray(Image.open(SRC/'Geography_Classes_2048x1024.png'))
    # 극점에서는 모든 경도가 같은 정점을 공유하므로 고도/색을 통일한다.
    elev[0]=np.mean(elev[0]);elev[-1]=np.mean(elev[-1])
    height=np.uint8(np.rint(np.clip(elev/8000,0,1)*255));h=height/255.
    Image.fromarray(height).save(OUT/'T_Diorama_Height.png')
    old=np.asarray(Image.open(SRC/'T_FF_Earth_CasualClay_Aligned_2048x1024.png').convert('RGB')).astype(float)
    green=np.clip((old[:,:,1]-old[:,:,0])/90,0,1)
    sand=np.clip((old[:,:,0]-old[:,:,2]-80)/80,0,1)
    grass=np.array([190,226,105]);forest=np.array([145,204,84]);desert=np.array([250,231,166])
    color=grass*(1-green[:,:,None]*.65)+forest*green[:,:,None]*.65
    color=color*(1-sand[:,:,None])+desert*sand[:,:,None]
    rock=np.clip((elev-2400)/3000,0,1)[:,:,None]*.65
    color=color*(1-rock)+np.array([230,215,165])*rock
    snow=np.clip((elev-4600)/1600,0,1)[:,:,None]*.8
    color=color*(1-snow)+np.array([250,249,239])*snow
    color[cls==2]=[250,249,241];color[cls==0]=grass
    Image.fromarray(np.uint8(color)).save(OUT/'T_Diorama_Color.png')
    mask=cls>0
    # 극점 행과 좌우 이음선 행을 추가한다. UV는 원본 픽셀 중심을 그대로 사용한다.
    land=np.concatenate([mask[:1],mask,mask[-1:]],axis=0)
    land=np.concatenate([land,land[:,:1]],axis=1)
    gy=np.r_[-.5,np.arange(1024),1023.5]
    reports=[]
    for tile in range(8):
        start=tile*256;end=start+256
        points=[];lookup={};faces=[]
        def vertex(x,y):
            key=(x,y)
            if key not in lookup:lookup[key]=len(points);points.append(key)
            return lookup[key]
        for yy in range(1025):
            for xx in range(start,end):
                for tri in [[(xx,yy),(xx+1,yy),(xx+1,yy+1)],[(xx,yy),(xx+1,yy+1),(xx,yy+1)]]:
                    poly=[]
                    for a,b in zip(tri,tri[1:]+tri[:1]):
                        va=land[a[1],a[0]];vb=land[b[1],b[0]]
                        if va:poly.append(a)
                        if va!=vb:poly.append(((a[0]+b[0])/2,(a[1]+b[1])/2))
                    ids=[vertex(*q) for q in poly]
                    for k in range(1,len(ids)-1):faces.append((ids[0],ids[k],ids[k+1]))
        edges={}
        for f in faces:
            for a,b in zip(f,f[1:]+f[:1]):
                key=tuple(sorted((a,b)))
                edges[key]=None if key in edges else (a,b)
        boundary=[e for e in edges.values() if e is not None]
        pts=np.array(points,dtype=float);original=pts.copy()
        neighbors=defaultdict(list)
        for a,b in boundary:neighbors[a].append(b);neighbors[b].append(a)
        def is_coast(i):
            x,y=original[i];return start+1<x<end-1 and 2<y<1023 and len(neighbors[i])==2
        coast=[i for i in neighbors if is_coast(i)]
        # 미세한 경계만 0.2픽셀 이하로 완화한다. 구간 경계/극점은 고정한다.
        for _ in range(2):
            new=pts.copy()
            for i in coast:new[i]=pts[i]*.75+np.mean(pts[neighbors[i]],axis=0)*.25
            pts=new
        outer=pts.copy()
        centroid=np.zeros_like(pts);cnt=np.zeros(len(pts))
        for face in faces:
            c=pts[list(face)].mean(axis=0)
            for i in face:centroid[i]+=c;cnt[i]+=1
        centroid/=np.maximum(cnt[:,None],1)
        bevel=np.zeros(len(pts))
        for i in coast:
            # 주변 삼각형 중심 쪽으로 제한된 이동: 작은 섬도 뒤집히지 않는다.
            pts[i]=pts[i]*.75+centroid[i]*.25;bevel[i]=6.
        def radial(q):
            px=q[:,0];py=np.interp(q[:,1],np.arange(1026),gy)
            lo=np.deg2rad((px+.5)/2048*360-180);la=np.deg2rad(90-(py+.5)/1024*180)
            return np.stack([np.cos(la)*np.cos(lo),np.cos(la)*np.sin(lo),np.sin(la)],axis=1),px,py
        rt,px,py=radial(pts);ro,ox,oy=radial(outer)
        # x 이음선도 선형 보간되도록 좌우 한 열씩 확장한다.
        hp=np.concatenate([h[:,-1:],h,h[:,:1]],axis=1)
        ph=sample(hp,px+1,py)
        top=rt*(R+BASE+MOUNTAIN*ph)[:,None]
        middle=ro*(R+BASE+MOUNTAIN*ph-bevel)[:,None]
        bottom=ro*(R-2)
        uv=np.stack([(px+.5)/2048,(py+.5)/1024],axis=1)
        fs=np.array(faces)[:,[0,2,1]]
        # 뒤집힌 삼각형은 경계 완화 실패다. 극점 축퇴 면은 출력에서 제거한다.
        cn=np.cross(top[fs[:,1]]-top[fs[:,0]],top[fs[:,2]]-top[fs[:,0]])
        assert np.min(np.sum(cn*top[fs[:,0]],axis=1))>-.01,'해안 완화로 뒤집힌 면'
        prefix=f'SM_Diorama_{tile:02d}'
        nt=write(prefix+'_Top',top,uv,fs)
        nb=write(prefix+'_Bottom',bottom,uv,fs[:,[0,2,1]])
        wv=[];wu=[];wf=[]
        for a,b in boundary:
            for low,high,lweight,hweight in [(bottom,middle,0,1),(middle,top,1,1)]:
                n=len(wv);wv.extend([low[a],low[b],high[b],high[a]])
                wu.extend([(ph[a],lweight),(ph[b],lweight),(ph[b],hweight),(ph[a],hweight)])
                wf.extend([(n,n+2,n+1),(n,n+3,n+2)])
        nw=write(prefix+'_Coast',wv,wu,wf)
        item={'tile':tile,'vertices':len(top),'top_triangles':nt,'wall_triangles':nw,'bottom_triangles':nb,'coast_vertices':len(coast)}
        reports.append(item);print(item,flush=True)
    report={'status':'complete','radius':R,'LandThickness':BASE,'MountainHeight':MOUNTAIN,'tiles':reports,'dem_sha256':hashlib.sha256(DEM.read_bytes()).hexdigest(),'max_elevation_m':float(elev.max()),'bevel_depth_uu':6,'scope':'원본 2K 해안, 전 세계, 극점/경도 이음선 포함'}
    (OUT/'build_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')

if __name__=='__main__':run()
