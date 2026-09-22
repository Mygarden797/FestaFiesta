# inspect_source.py
# 역할: 플레이어 캐릭터 모델·애니메이션 작업에서 기존 에셋 상태를 읽어 기록한다.

import bpy, json, bmesh
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parents[2]/'SourceArt/MainMan_20260911'
report={'version':bpy.app.version_string,'file':bpy.data.filepath,'objects':[],'materials':[],'images':[]}
for o in bpy.data.objects:
    d={'name':o.name,'type':o.type,'location':list(o.location),'rotation':list(o.rotation_euler),'scale':list(o.scale),'dimensions':list(o.dimensions),'hide_render':o.hide_render,'parent':o.parent.name if o.parent else None,'modifiers':[(m.name,m.type) for m in o.modifiers]}
    if o.type=='MESH':
        me=o.data; bm=bmesh.new();bm.from_mesh(me)
        d.update(vertices=len(me.vertices),faces=len(me.polygons),materials=[m.name if m else None for m in me.materials],uv_layers=[u.name for u in me.uv_layers],boundary_edges=sum(e.is_boundary for e in bm.edges),nonmanifold_edges=sum(not e.is_manifold for e in bm.edges),loose_vertices=sum(not v.link_faces for v in bm.verts))
        remaining=set(bm.verts);components=[]
        while remaining:
            seed=remaining.pop();todo=[seed];vs=[seed]
            while todo:
                v=todo.pop()
                for e in v.link_edges:
                    n=e.other_vert(v)
                    if n in remaining:remaining.remove(n);todo.append(n);vs.append(n)
            coords=[o.matrix_world@v.co for v in vs]
            components.append({'vertices':len(vs),'min':[min(c[i] for c in coords) for i in range(3)],'max':[max(c[i] for c in coords) for i in range(3)]})
        d['components']=sorted(components,key=lambda x:-x['vertices']);bm.free()
    report['objects'].append(d)
for m in bpy.data.materials:
    d={'name':m.name,'diffuse':list(m.diffuse_color),'nodes':[]}
    if m.use_nodes:
        for n in m.node_tree.nodes:
            nd={'name':n.name,'type':n.type}
            if n.type=='TEX_IMAGE':nd['image']=n.image.name if n.image else None
            if n.type=='BSDF_PRINCIPLED':nd['base_color']=list(n.inputs['Base Color'].default_value)
            d['nodes'].append(nd)
    report['materials'].append(d)
for im in bpy.data.images:
    report['images'].append({'name':im.name,'size':list(im.size),'filepath':im.filepath,'packed':bool(im.packed_file)})
    if im.type=='IMAGE' and im.size[0]>0:
        im.save_render(str(OUT/'qa'/('original_'+bpy.path.clean_name(im.name)+'.png')))
(OUT/'qa/source_audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report))
