"""국가 좌표 방향과 P05 실제 Top 삼각형 교차로 기본 조형물 밑면을 계산한다."""
from pathlib import Path
import json,math,sys
import numpy as np
ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(Path(__file__).parent))
import blender_world as world
OUT=ROOT/'Saved/BrightGlobe'
actual=OUT/'p05_catalog_locations.json'
source=actual if actual.exists() else ROOT/'Tools/ui/data/diorama_world_prop_offsets.json'
locations=json.loads(source.read_text(encoding='utf-8'))
report={'status':'running','land_thickness':40,'mountain_height':650,'location_source':str(source),'clearance_uu':2.,'countries':{}}
for code,row in locations.items():
    lat,lon=map(math.radians,(row['latitude'],row['longitude']))
    direction=np.array([math.cos(lat)*math.cos(lon),math.cos(lat)*math.sin(lon),math.sin(lat)])
    tile=max(0,min(7,int((row['longitude']+180)/45)))
    data=world.load_tile(tile)
    vertices=data['top_vertices'].astype(np.float64);faces=data['top_faces']
    a,b,c=(vertices[faces[:,i]] for i in range(3))
    e1,e2=b-a,c-a;p=np.cross(direction,e2);det=np.einsum('ij,ij->i',e1,p)
    valid=np.abs(det)>1.e-10
    inv=np.divide(1.,det,out=np.zeros_like(det),where=valid)
    u=np.einsum('ij,ij->i',-a,p)*inv;q=np.cross(-a,e1)
    v=q@direction*inv;t=np.einsum('ij,ij->i',e2,q)*inv
    hit=valid&(u>=-1.e-8)&(v>=-1.e-8)&(u+v<=1.+1.e-8)&(t>4900.)
    if np.any(hit):
        radius=float(t[hit].max());method='실제 Top 방향선 교차';nearest=None
    else:
        # 2K 해안에서 작은 섬/항구 기준점이 바다로 분류된 경우: 가까운 지표의 반지름을 사용한다.
        normals=world.unit(vertices)
        distance=np.linalg.norm(normals-direction,axis=1)*5000.
        index=int(distance.argmin());nearest=float(distance[index])
        assert nearest<=40.,(code,'기준점 주변 지표 확인 필요',nearest)
        radius=float(np.linalg.norm(vertices[index]));method='2K 해안의 작은 섬: 가장 가까운 Top 반지름'
    offset=max(0.,radius-5000.)+2.
    report['countries'][code]={'latitude':row['latitude'],'longitude':row['longitude'],'surface_offset':round(offset,6),'surface_radius':radius,'method':method,'nearest_surface_distance_uu':nearest}
    print(code,round(offset,3),method,nearest,flush=True)
report['status']='passed'
(OUT/'p05_prop_offsets.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
