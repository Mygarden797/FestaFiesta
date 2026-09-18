"""실제 UE 구체 버텍스의 UV와 위경도 기준을 읽기 비교한다."""
import json,math
from pathlib import Path
import unreal
mesh=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/CitrusPop_v2/Mesh/SM_FF_Earth_UVSphere_R50cm_256x128')
vertices,triangles,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,0)
rows=[]
for lat,lon in [(0,0),(35,125),(-20,20),(25,-40),(0,90)]:
    la,lo=map(math.radians,(lat,lon));d=(math.cos(la)*math.cos(lo),-math.cos(la)*math.sin(lo),math.sin(la))
    i=max(range(len(vertices)),key=lambda i:sum(a*b for a,b in zip(d,(vertices[i].x,vertices[i].y,vertices[i].z))))
    p,uv=vertices[i],uvs[i];radius=math.sqrt(p.x*p.x+p.y*p.y+p.z*p.z)
    expected=((math.atan2(-p.y,p.x)+math.pi)/(2*math.pi),math.acos(p.z/radius)/math.pi)
    rows.append({'latlon':[lat,lon],'vertex':[p.x,p.y,p.z],'uv':[uv.x,uv.y],'expected':expected,'shifted':[(uv.x+.5)%1,uv.y]})
Path(unreal.Paths.project_dir(),'Saved/BrightGlobe/p05_water_uv.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print(json.dumps(rows))
