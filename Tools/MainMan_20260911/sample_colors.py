import bpy,numpy as np,json
im=bpy.data.images['tripo_image_29ed07d1_0'];im.scale(512,512);a=np.array(im.pixels[:]).reshape(512,512,4)
o=next(o for o in bpy.data.objects if o.type=='MESH');m=o.data;uv=m.uv_layers.active.data;groups={x:[] for x in ['skin','hair','cloth','sole']}
for p in m.polygons:
    c=o.matrix_world@p.center;x,y,z=c
    k='hair' if z>.91 else 'skin' if (.23<abs(x)<.32 and .64<z<.70) else 'cloth' if (.47<z<.59) else 'sole' if z<.02 else None
    if k:
        t=sum((uv[i].uv for i in p.loop_indices),start=__import__('mathutils').Vector((0,0)))/len(p.loop_indices)
        groups[k].append(a[int(t.y*511)%512,int(t.x*511)%512,:3].tolist())
print(json.dumps({k:np.median(v,axis=0).tolist() for k,v in groups.items()}))
