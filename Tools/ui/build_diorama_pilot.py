"""원본 해안 분류와 NOAA ETOPO1로 동아시아 독립 육지 시험 메시를 만든다.

Python + NumPy + Pillow 전용. AI 이미지 재생성이 아닌 지리 좌표 기반 메시 제작.
"""
from pathlib import Path
import json, hashlib, math
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'SourceArt/UI/Atlas/DioramaPilot_v1'
SRC = ROOT/'SourceArt/UI/Title/CasualClay_Albedo_v1/GeographyLocked'
DEM = ROOT/'SourceArt/ThirdParty/NOAA/ETOPO1/east_asia_10arcmin.csv'
RADIUS, BASE, MOUNTAIN = 5000., 100., 350.

def sample(a, x, y):
    x=np.clip(x,0,a.shape[1]-1); y=np.clip(y,0,a.shape[0]-1)
    ix=np.floor(x).astype(int); iy=np.floor(y).astype(int)
    jx=np.minimum(ix+1,a.shape[1]-1); jy=np.minimum(iy+1,a.shape[0]-1)
    tx=x-ix; ty=y-iy
    return a[iy,ix]*(1-tx)*(1-ty)+a[iy,jx]*tx*(1-ty)+a[jy,ix]*(1-tx)*ty+a[jy,jx]*tx*ty

def run():
    OUT.mkdir(parents=True,exist_ok=True)
    rows=np.loadtxt(DEM,delimiter=',',skiprows=2)
    lats=np.unique(rows[:,0]); lons=np.unique(rows[:,1])
    assert len(rows)==len(lats)*len(lons)
    dem=rows[:,2].reshape(len(lats),len(lons))
    assert np.isfinite(dem).all() and dem.max()<10000
    # 원본 2K 픽셀 중심의 위치를 유지한다. 경도 55~155, 위도 -10~65 시험 영역.
    xs=np.arange(math.ceil((55+180)/360*2048-.5),math.floor((155+180)/360*2048-.5)+1)
    ys=np.arange(math.ceil((90-65)/180*1024-.5),math.floor((90+10)/180*1024-.5)+1)
    lon=(xs+.5)/2048*360-180; lat=90-(ys+.5)/1024*180
    xx,yy=np.meshgrid((lon-lons[0])/(lons[1]-lons[0]),(lat-lats[0])/(lats[1]-lats[0]))
    elevation=np.maximum(0,sample(dem,xx,yy))
    # 낮은 강도의 공간 평활화로 원래 산계와 고원을 보존한다.
    p=np.pad(elevation,1,mode='edge')
    elevation=(4*p[1:-1,1:-1]+p[:-2,1:-1]+p[2:,1:-1]+p[1:-1,:-2]+p[1:-1,2:])/8
    h=np.clip(elevation/8000,0,1)
    classes=np.asarray(Image.open(SRC/'Geography_Classes_2048x1024.png'))[np.ix_(ys,xs)]
    land=classes>0
    # 메시와 WPO가 같은 양자화된 높이를 사용한다.
    height8=np.uint8(np.rint(h*255));h=height8/255.
    Image.fromarray(height8).save(OUT/'T_Diorama_Height.png')
    color=np.asarray(Image.open(SRC/'T_FF_Earth_CasualClay_Aligned_2048x1024.png').convert('RGB'))[np.ix_(ys,xs)].astype(float)
    rock=np.clip((elevation-2200)/2800,0,1)[...,None]*.6
    color=color*(1-rock)+np.array([220,202,139])*rock
    snow=np.clip((elevation-5000)/1700,0,1)[...,None]*.9
    color=color*(1-snow)+np.array([249,245,226])*snow
    # 해안의 보간 픽셀에 바다색이 스며들지 않도록 육지용 팔레트를 확장한다.
    color[~land]=[193,224,95]
    Image.fromarray(np.uint8(color)).save(OUT/'T_Diorama_Color.png')
    H,W=land.shape
    points=[];indices={};faces=[]
    def vertex(x,y):
        key=(round(x,4),round(y,4))
        if key not in indices:indices[key]=len(points);points.append((x,y))
        return indices[key]
    # 삼각형별 0.5 등치선으로 해안을 잘라 경계 정점을 공유한다.
    for y in range(H-1):
        for x in range(W-1):
            for tri in [[(x,y),(x+1,y),(x+1,y+1)],[(x,y),(x+1,y+1),(x,y+1)]]:
                poly=[]
                for a,b in zip(tri,tri[1:]+tri[:1]):
                    va=land[a[1],a[0]];vb=land[b[1],b[0]]
                    if va:poly.append(a)
                    if va!=vb:poly.append(((a[0]+b[0])/2,(a[1]+b[1])/2))
                ids=[vertex(*q) for q in poly]
                for k in range(1,len(ids)-1):faces.append((ids[0],ids[k],ids[k+1]))
    pts=np.array(points);ph=sample(h,pts[:,0],pts[:,1])
    plon=np.deg2rad(lon[0]+pts[:,0]*360/2048);plat=np.deg2rad(lat[0]-pts[:,1]*180/1024)
    # UE의 동경 방향은 -Y. OBJ 임포터 Y 반전을 고려해 +Y로 내보낸다.
    radial=np.stack([np.cos(plat)*np.cos(plon),np.cos(plat)*np.sin(plon),np.sin(plat)],axis=1)
    top=radial*(RADIUS+BASE+MOUNTAIN*ph)[:,None]
    bottom=radial*(RADIUS-2)
    edges={}
    for face in faces:
        for a,b in zip(face,face[1:]+face[:1]):
            key=tuple(sorted((a,b)))
            if key in edges:edges[key]=None
            else:edges[key]=(a,b)
    boundary=[v for v in edges.values() if v is not None]
    def write(name,verts,uv,fs):
        # OBJ는 오른손 좌표계로 출력한다.
        lines=['# FestaFiesta NOAA ETOPO1 동아시아 시험 메시','o '+name,'s 1']
        lines += ['v %.6f %.6f %.6f'%tuple(p) for p in verts]
        lines += ['vt %.8f %.8f'%(u,1-v) for u,v in uv]
        lines += ['f '+' '.join(f'{i+1}/{i+1}' for i in f) for f in fs]
        (OUT/(name+'.obj')).write_text('\n'.join(lines),encoding='utf8')
    # lon 증가/lat 감소 삼각형은 바깥 방향이다.
    fs=np.asarray(faces)
    normal=np.cross(top[fs[:,1]]-top[fs[:,0]],top[fs[:,2]]-top[fs[:,0]])
    if np.median(np.sum(normal*top[fs[:,0]],axis=1))<0:fs=fs[:,[0,2,1]]
    uv=np.stack([(pts[:,0]+.5)/W,(pts[:,1]+.5)/H],axis=1)
    write('SM_Diorama_Top',top,uv,fs)
    wallv=[];walluv=[];wallf=[]
    # 옆면 UV.x=고도, UV.y=바닥~윗면 가중치. 공통 두께/산 높이 변경 시 틈 방지.
    for a,b in boundary:
        n=len(wallv);wallv.extend([bottom[a],bottom[b],top[b],top[a]])
        walluv.extend([(ph[a],0),(ph[b],0),(ph[b],1),(ph[a],1)])
        # top 입력 삼각형 방향을 역으로 따라 옆면 바깥을 향한다.
        wallf.extend([(n,n+2,n+1),(n,n+3,n+2)])
    write('SM_Diorama_Coast',wallv,walluv,wallf)
    # 바닥은 바다보다 2uu 아래에서 닫는다.
    write('SM_Diorama_Bottom',bottom,uv,fs[:,[0,2,1]])
    report={'status':'complete','source_dem_sha256':hashlib.sha256(DEM.read_bytes()).hexdigest(),
            'bounds_lon_lat':[float(lon[0]),float(lon[-1]),float(lat[-1]),float(lat[0])],
            'grid':[W,H],'top_vertices':len(top),'top_triangles':len(fs),'boundary_edges':len(boundary),
            'height_range_m':[float(elevation.min()),float(elevation.max())],
            'radius':RADIUS,'land_thickness':BASE,'mountain_height':MOUNTAIN,'height_reference_m':8000,
            'source':'ETOPO1 ice surface, sampled every 10 arc-minutes; original daymap shoreline mask',
            'limits':'동아시아 시험 영역 가장자리는 인공 절단면. 작은 섬은 2K 마스크 해상도 한계.'}
    assert len(top)>10000 and len(boundary)>100
    (OUT/'build_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(report,ensure_ascii=False))

if __name__=='__main__':run()
