import bpy,json
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import intersect_ray_tri
from pathlib import Path
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911/qa'
rows=[]
for o in bpy.data.objects:
    if o.type!='MESH':continue
    m=o.data;m.calc_loop_triangles();verts=[v.co.copy() for v in m.vertices];tris=[tuple(t.vertices) for t in m.loop_triangles];bv=BVHTree.FromPolygons(verts,tris,all_triangles=True)
    crossings=set()
    for a,b in bv.overlap(bv):
        if a>=b or set(tris[a])&set(tris[b]):continue
        found=False
        for aa,bb in [(a,b),(b,a)]:
            target=[verts[i] for i in tris[bb]];source=[verts[i] for i in tris[aa]]
            for i in range(3):
                start=source[i];delta=source[(i+1)%3]-start;length=delta.length
                if length<1e-6:continue
                hit=intersect_ray_tri(*target,delta.normalized(),start,True)
                if hit is not None:
                    t=(hit-start).dot(delta.normalized())
                    if 1e-5<t<length-1e-5:found=True;break
            if found:break
        if found:crossings.add((a,b))
    rows.append({'object':o.name,'nonadjacent_triangle_crossings':len(crossings),'examples':sorted(crossings)[:6]})
report={'method':'Segment/triangle intersections within individual objects; shared-vertex triangles excluded; coplanar overlap and intentional intersections between separate clothing parts excluded.','objects':rows,'total_crossings':sum(r['nonadjacent_triangle_crossings'] for r in rows)}
(OUT/'self_intersections.json').write_text(json.dumps(report,indent=2));print(json.dumps([r for r in rows if r['nonadjacent_triangle_crossings']]));print('TOTAL',report['total_crossings'])
