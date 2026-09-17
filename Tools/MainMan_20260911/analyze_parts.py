import bpy,bmesh,json,numpy as np
from pathlib import Path
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911/qa'
o=next(o for o in bpy.data.objects if o.type=='MESH');me=o.data;bm=bmesh.new();bm.from_mesh(me)
bm.verts.ensure_lookup_table();bm.faces.ensure_lookup_table()
remaining=set(bm.verts);parts=[]
while remaining:
    v=remaining.pop();todo=[v];vs={v}
    while todo:
        for e in todo.pop().link_edges:
            n=e.other_vert(v) if False else None
        # visit using explicit current vertex
        for vert in list(vs):
            pass
        break
    todo=list(vs)
    while todo:
        curr=todo.pop()
        for e in curr.link_edges:
            n=e.other_vert(curr)
            if n in remaining:remaining.remove(n);todo.append(n);vs.add(n)
    parts.append(vs)
parts.sort(key=lambda v:-len(v))
data=[]
for i,vs in enumerate(parts):
    if len(vs)<100:continue
    fs={f for v in vs for f in v.link_faces};es={e for v in vs for e in v.link_edges}
    coords=np.array([list(o.matrix_world@v.co) for v in vs]);d={'i':i,'v':len(vs),'f':len(fs),'min':coords.min(axis=0).tolist(),'max':coords.max(axis=0).tolist(),'bad':sum(not e.is_manifold for e in es),'boundary':sum(e.is_boundary for e in es)}
    data.append(d)
    if i in [0,3,4,6]:
        d['slices']=[]
        for z in np.arange(.08,1,.025):
            a=coords[np.abs(coords[:,2]-z)<.008]
            if len(a):d['slices'].append({'z':round(float(z),3),'min':a.min(axis=0).tolist(),'max':a.max(axis=0).tolist()})
(OUT/'parts.json').write_text(json.dumps(data,indent=2))
print(json.dumps([{k:v for k,v in d.items() if k!='slices'} for d in data]))
