"""P3 메시와 P4 시험 재질을 새 폴더/Frontend 사본에만 연결한다."""
from pathlib import Path
import json
import sys
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(ROOT / 'Tools/ui'))
import import_diorama_pilot as helper
DEST = '/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1'
P03 = DEST + '/P03'
MAP = DEST + '/L_BrightGlobe_Pilot'
OUT = ROOT / 'Saved/BrightGlobe'
mel = unreal.MaterialEditingLibrary
report = {'status': 'running', 'frontend_modified': False}


def build_material(name, kind, collection, color, height, mask):
    helper.DEST = P03
    material = helper.asset(name, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(material)
    def node(cls, **values):
        n = mel.create_material_expression(material, cls, 0, 0)
        for k, v in values.items(): n.set_editor_property(k, v)
        return n
    def connect(source, target, pin, output=''):
        assert mel.connect_material_expressions(source, output, target, pin)
    def constant(value): return node(unreal.MaterialExpressionConstant, r=value)
    def binary(cls, a, b):
        n = node(cls); connect(a, n, 'A'); connect(b, n, 'B'); return n
    def mul(a, b): return binary(unreal.MaterialExpressionMultiply, a, b)
    def add(a, b): return binary(unreal.MaterialExpressionAdd, a, b)
    def sub(a, b): return binary(unreal.MaterialExpressionSubtract, a, b)
    def sat(a):
        n = node(unreal.MaterialExpressionSaturate); connect(a, n, ''); return n
    def channel(a, index):
        n = node(unreal.MaterialExpressionComponentMask, r=index==0, g=index==1, b=index==2, a=False)
        connect(a, n, ''); return n
    def vector(label, rgb):
        return node(unreal.MaterialExpressionVectorParameter, parameter_name=label,
                    default_value=unreal.LinearColor(*rgb, 1))
    def mix(a, b, t):
        n = node(unreal.MaterialExpressionLinearInterpolate)
        connect(a,n,'A'); connect(b,n,'B'); connect(t,n,'Alpha'); return n
    def output(n, prop, pin=''):
        assert mel.connect_material_property(n, pin, prop)
    uv = node(unreal.MaterialExpressionTextureCoordinate)
    if kind == 'top':
        tex = node(unreal.MaterialExpressionTextureSample, texture=color, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        r, g, b = [channel(tex, i) for i in range(3)]
        forest = sat(mul(sub(constant(.56), r), constant(4.)))
        sand = sat(mul(add(sub(r, g), constant(.01)), constant(6.)))
        snow = sat(mul(sub(b, constant(.61)), constant(5.)))
        base = mix(vector('GrassColor', (.22,.58,.045)), vector('ForestColor', (.025,.28,.055)), forest)
        base = mix(base, vector('SandColor', (.88,.57,.16)), sand)
        base = mix(base, vector('SnowColor', (.91,.95,1.)), snow)
    elif kind == 'wall':
        base = vector('CoastColor', (.7,.43,.15))
    elif kind == 'water':
        tex = node(unreal.MaterialExpressionTextureSample, texture=mask,
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
            mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL, const_mip_value=2.)
        # 기존 바다 구체는 +X가 U=0. 지리 지도는 -X가 U=0이므로 반 바퀴 UV 보정.
        offset = node(unreal.MaterialExpressionConstant2Vector, r=.5, g=0.)
        connect(add(uv, offset), tex, 'UVs')
        coast = sat(mul(channel(tex, 0), constant(400.)))
        base = mix(vector('OceanColor', (.006,.19,.62)), vector('ShallowColor', (.025,.55,.72)), coast)
    else:
        base = vector('BottomColor', (.2,.19,.09))
    output(base, unreal.MaterialProperty.MP_BASE_COLOR)
    output(constant(.72 if kind != 'water' else .48), unreal.MaterialProperty.MP_ROUGHNESS)
    output(constant(.18), unreal.MaterialProperty.MP_SPECULAR)
    # 적은 환경색 보조만 사용하고 조명에 의한 형태 명암을 남긴다.
    output(mul(base, constant(.04)), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if kind in ('top', 'wall'):
        if kind == 'top':
            texh = node(unreal.MaterialExpressionTextureSample, texture=height,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
                mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL, const_mip_value=0.)
            h = channel(texh, 0); weight = constant(1.)
        else:
            h = channel(uv, 0); weight = channel(uv, 1)
        h = sat(mul(sub(h, constant(.008)), constant(1./.992)))
        shaped = node(unreal.MaterialExpressionPower, const_exponent=1.15); connect(h, shaped, 'Base')
        thickness = node(unreal.MaterialExpressionCollectionParameter, collection=collection, parameter_name='LandThickness')
        mountain = node(unreal.MaterialExpressionCollectionParameter, collection=collection, parameter_name='MountainHeight')
        delta = mul(add(sub(thickness, constant(40.)), mul(shaped, sub(mountain, constant(650.)))), weight)
        world = node(unreal.MaterialExpressionWorldPosition, world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
        origin = node(unreal.MaterialExpressionActorPositionWS)
        radial = node(unreal.MaterialExpressionNormalize); connect(sub(world, origin), radial, 'VectorInput')
        output(mul(radial, delta), unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.layout_material_expressions(material); mel.recompile_material(material); helper.save(material)
    return material


def run():
    assert not unreal.EditorAssetLibrary.does_asset_exist(MAP), '완성된 시험 레벨 덮어쓰기 금지'
    assert json.loads((OUT/'p03_geometry.json').read_text(encoding='utf-8'))['status'] == 'complete'
    helper.DEST = P03
    col = helper.asset('MPC_BrightGlobe_Controls', unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for name, value in [('LandThickness',40.),('MountainHeight',650.)]:
        p = unreal.CollectionScalarParameter(); p.set_editor_property('parameter_name',name); p.set_editor_property('default_value',value); params.append(p)
    col.set_editor_property('scalar_parameters', params); helper.save(col)
    maskpath = P03 + '/T_Bright_LandClass'
    if unreal.EditorAssetLibrary.does_asset_exist(maskpath):
        mask = unreal.load_asset(maskpath)
    else:
        task = unreal.AssetImportTask()
        for k,v in dict(filename=str(ROOT/'SourceArt/UI/Title/CasualClay_Albedo_v1/GeographyLocked/Geography_Classes_2048x1024.png'),
            destination_path=P03, destination_name='T_Bright_LandClass', automated=True, replace_existing=False, save=False).items():task.set_editor_property(k,v)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]); mask = task.get_objects()[0]
        mask.set_editor_property('srgb',False); mask.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_GRAYSCALE)
        mask.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP);helper.save(mask)
    color = unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1/T_Diorama_Color')
    height = unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1/T_Diorama_Height')
    mats = {k:build_material('M_BrightGlobe_'+k,k,col,color,height,mask) for k in ('top','wall','bottom','water')}
    meshes = []
    helper.DEST = P03; helper.SRC = ROOT/'SourceArt/UI/Atlas/BrightGlobe_v1/P03/Exports'
    for path in sorted(helper.SRC.glob('*.obj')):
        kind = {'Top':'top','Coast':'wall','Bottom':'bottom'}[path.stem.rsplit('_',1)[1]]
        meshpath = P03+'/'+path.stem
        mesh = unreal.load_asset(meshpath) if unreal.EditorAssetLibrary.does_asset_exist(meshpath) else helper.mesh(path.stem,mats[kind])
        if kind != 'bottom': meshes.append(mesh)
    assert len(meshes) == 6
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.new_level_from_template(MAP, '/Game/FestaFiesta/Maps/Frontend')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    globe = next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.FF_AtlasGlobeActor))
    globe.set_editor_property('land_layer_meshes', meshes)
    globe.set_editor_property('globe_material', mats['water'])
    for index, comp in enumerate(globe.get_editor_property('land_layer_components')):
        comp.set_static_mesh(meshes[index] if index<len(meshes) else None)
    globe.get_editor_property('globe_mesh').set_material(0,mats['water'])
    assert level.save_current_level()
    report.update(status='complete', level=MAP, materials={k:m.get_path_name() for k,m in mats.items()},
                  meshes=[m.get_path_name() for m in meshes], mpc=col.get_path_name(),
                  coverage='regional pilot only; partial map cuts intentional', rendered_verified=False)


if __name__ == '__main__':
    try: run()
    except Exception:
        report.update(status='failed', error=traceback.format_exc()); unreal.log_error('BRIGHT_GLOBE_FAIL '+report['error'])
    (OUT/'p03_ue_import.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
