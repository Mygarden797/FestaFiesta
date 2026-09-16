"""Blender v002에도 UE와 같은 원본 색상 분류를 연결한다. 메시/UV는 바꾸지 않는다."""
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parents[4]


def apply():
    scene=bpy.context.scene
    assert scene.name=='FF_BrightGlobe_P03_Pilot_v002'
    source=ROOT/'SourceArt/UI/Title/CasualClay_Albedo_v1/GeographyLocked/T_FF_Earth_CasualClay_Aligned_2048x1024.png'
    mat=bpy.data.materials.get('MAT-BrightP03v2-Top') or bpy.data.materials.new('MAT-BrightP03v2-Top')
    mat.use_nodes=True
    tree=mat.node_tree;tree.nodes.clear()
    def n(kind):return tree.nodes.new(kind)
    def mathnode(op,a,b):
        x=n('ShaderNodeMath');x.operation=op
        for index,value in enumerate((a,b)):
            if isinstance(value,(int,float)):x.inputs[index].default_value=value
            else:tree.links.new(value,x.inputs[index])
        return x.outputs[0]
    def sat(a):return mathnode('MINIMUM',mathnode('MAXIMUM',a,0.),1.)
    def mix(a,b,t):
        x=n('ShaderNodeMixRGB')
        for index,value in [(1,a),(2,b)]:
            if isinstance(value,tuple):x.inputs[index].default_value=(*value,1)
            else:tree.links.new(value,x.inputs[index])
        tree.links.new(t,x.inputs[0]);return x.outputs[0]
    tex=n('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(source),check_existing=True)
    split=n('ShaderNodeSeparateColor');tree.links.new(tex.outputs['Color'],split.inputs[0])
    r,g,b=split.outputs[0],split.outputs[1],split.outputs[2]
    forest=sat(mathnode('MULTIPLY',mathnode('SUBTRACT',.56,r),4.))
    sand=sat(mathnode('MULTIPLY',mathnode('ADD',mathnode('SUBTRACT',r,g),.01),6.))
    snow=sat(mathnode('MULTIPLY',mathnode('SUBTRACT',b,.61),5.))
    color=mix((.32,.64,.12),(.10,.42,.11),forest)
    color=mix(color,(.96,.75,.37),sand)
    color=mix(color,(.91,.95,1.),snow)
    bsdf=n('ShaderNodeBsdfPrincipled');bsdf.inputs['Roughness'].default_value=.72
    tree.links.new(color,bsdf.inputs['Base Color'])
    out=n('ShaderNodeOutputMaterial');tree.links.new(bsdf.outputs[0],out.inputs['Surface'])
    side=bpy.data.materials.get('MAT-BrightP03v2-Coast') or bpy.data.materials.new('MAT-BrightP03v2-Coast')
    side.use_nodes=True
    shader=next(n for n in side.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    shader.inputs['Base Color'].default_value=(.73,.52,.24,1)
    shader.inputs['Roughness'].default_value=.72
    for obj in scene.objects:
        if obj.name.endswith('_Top'):
            obj.data.materials.clear();obj.data.materials.append(mat)
        elif obj.name.endswith('_Coast'):
            obj.data.materials.clear();obj.data.materials.append(side)
    for area in bpy.context.screen.areas:
        if area.type=='VIEW_3D':
            shading=area.spaces.active.shading
            assert 'MATERIAL' in [i.identifier for i in shading.bl_rna.properties['type'].enum_items]
            shading.type='MATERIAL'
            area.spaces.active.clip_start=10.
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'SourceArt/UI/Atlas/BrightGlobe_v1/Blender/FF_BrightGlobe_P03_pilot_v002.blend'),check_existing=False)
    print('P03_V002_MATERIAL_PREVIEW_SAVED')
