"""한국 영역 선택용 파생 머티리얼·DataAsset을 생성하고 Frontend에 연결한다.

원래 지구 머티리얼과 카탈로그·나라별 보기 설정은 수정하지 않는다.
일회용 에디터에서 새 바이너리를 로드한 뒤 실행한다.
"""
from pathlib import Path
import hashlib
import json
import traceback
import sys
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(ROOT/'Tools/ui/atlas_regions'))
from region_feedback import upgrade
SRC = ROOT / 'SourceArt/UI/Atlas/CountryRegions/KR'
OUT = ROOT / 'Saved/AtlasRegions'
DEST = '/Game/FestaFiesta/UI/Atlas/CountryRegions'
BASE = '/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1'
TARGET = BASE + '/P10/MI_BrightGlobe_SunnyLand'
MAP = '/Game/FestaFiesta/Maps/Frontend'
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
report = {'status': 'running', 'saved': []}


def hashes():
    paths = list((ROOT/'Content/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P05').glob('*.uasset'))
    paths += list((ROOT/'Content/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P10').glob('*.uasset'))
    paths.append(ROOT/'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset')
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}


def save(obj):
    assert obj.get_path_name().startswith(DEST + '/')
    assert eal.save_loaded_asset(obj, only_if_is_dirty=False)
    report['saved'].append(obj.get_path_name())


def duplicate(source, name):
    path = DEST + '/' + name
    assert not eal.does_asset_exist(path), '기존 결과를 덮어쓰지 않습니다: ' + path
    obj = eal.duplicate_asset(source, path)
    assert obj
    return obj


def texture():
    task = unreal.AssetImportTask()
    for k, v in dict(filename=str(SRC/'T_FF_AtlasRegion_KR_1024.png'),
                     destination_path=DEST, destination_name='T_FF_AtlasRegion_KR',
                     automated=True, replace_existing=False, save=False).items():
        task.set_editor_property(k, v)
    assert not eal.does_asset_exist(DEST+'/T_FF_AtlasRegion_KR')
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = task.get_objects()[0]
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_TRILINEAR)
    save(tex)
    return tex


def material(mask, bounds):
    target = unreal.load_asset(TARGET)
    parent = target.get_editor_property('parent')
    while isinstance(parent, unreal.MaterialInstanceConstant):
        parent = parent.get_editor_property('parent')
    mat = duplicate(parent.get_path_name(), 'M_AtlasRegion_Land')
    base = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    base_pin = mel.get_material_property_input_node_output_name(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    emissive = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    emissive_pin = mel.get_material_property_input_node_output_name(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert base and emissive

    def node(cls, **props):
        obj = mel.create_material_expression(mat, cls, 0, 0)
        for k, v in props.items(): obj.set_editor_property(k, v)
        return obj

    def connect(source, target_node, pin, output=''):
        assert mel.connect_material_expressions(source, str(output), target_node, pin)

    custom = node(unreal.MaterialExpressionCustom,
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                  description='국가 영역: 옅은 채움과 화면 크기를 고려한 외곽선')
    names = ['UV', 'Mask', 'Bounds', 'Base', 'FillColor', 'OutlineColor', 'FillAmount', 'Enabled']
    inputs = []
    for name in names:
        entry = unreal.CustomInput(); entry.set_editor_property('input_name', name); inputs.append(entry)
    custom.set_editor_property('inputs', inputs)
    custom.set_editor_property('code', '''
float2 ll = float2(UV.x * 360.0 - 180.0, 90.0 - UV.y * 180.0);
float2 p = float2((ll.x-Bounds.x)/(Bounds.z-Bounds.x), (Bounds.w-ll.y)/(Bounds.w-Bounds.y));
float inside = step(0.0,p.x)*step(p.x,1.0)*step(0.0,p.y)*step(p.y,1.0);
float3 m = Texture2DSample(Mask, MaskSampler, p).rgb;
// 지구를 멀리서 볼 때도 경계가 사라지지 않도록 약 한 픽셀 안쪽까지 검사한다.
float2 dx = ddx(p)*0.8, dy = ddy(p)*0.8;
float3 a = Texture2DSample(Mask, MaskSampler, p+dx).rgb;
float3 b = Texture2DSample(Mask, MaskSampler, p-dx).rgb;
float3 c = Texture2DSample(Mask, MaskSampler, p+dy).rgb;
float3 d = Texture2DSample(Mask, MaskSampler, p-dy).rgb;
float edge = max(m.g, max(max(a.g,b.g),max(c.g,d.g)));
edge = max(edge, max(max(abs(m.r-a.r),abs(m.r-b.r)),max(abs(m.r-c.r),abs(m.r-d.r))));
float enabled = saturate(Enabled)*inside;
float outline = saturate(edge*1.7)*enabled;
float3 filled = lerp(Base, FillColor, saturate(m.r*FillAmount)*enabled);
return float4(lerp(filled, OutlineColor, outline), outline);
''')
    uv = node(unreal.MaterialExpressionTextureCoordinate)
    tex = node(unreal.MaterialExpressionTextureObjectParameter, parameter_name='RegionMask', texture=mask,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    connect(uv, custom, 'UV'); connect(tex, custom, 'Mask'); connect(base, custom, 'Base', base_pin)
    params = {'RegionBounds': bounds, 'RegionFillColor': (.60, .88, .26, 1.),
              'RegionOutlineColor': (1., .94, .53, 1.)}
    for name, value in params.items():
        expr = node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                    default_value=unreal.LinearColor(*value))
        connect(expr, custom, {'RegionBounds':'Bounds','RegionFillColor':'FillColor','RegionOutlineColor':'OutlineColor'}[name],
                'RGBA' if name == 'RegionBounds' else 'RGB')
    for name, value, pin in [('RegionFillAmount', .18, 'FillAmount'), ('RegionEnabled', 0., 'Enabled')]:
        expr = node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)
        connect(expr, custom, pin)
    rgb = node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
    alpha = node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
    connect(custom, rgb, ''); connect(custom, alpha, '')
    assert mel.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
    glow = node(unreal.MaterialExpressionMultiply, const_b=.15)
    connect(alpha, glow, 'A')
    add = node(unreal.MaterialExpressionAdd)
    connect(emissive, add, 'A', emissive_pin); connect(glow, add, 'B')
    assert mel.connect_material_property(add, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    upgrade(mat)
    mel.layout_material_expressions(mat); mel.recompile_material(mat); save(mat)
    mi = duplicate(TARGET, 'MI_AtlasRegion_Land')
    mel.set_material_instance_parent(mi, mat)
    save(mi)
    report['original_parent'] = parent.get_path_name()
    return mi, target


def run():
    before = hashes()
    shape = json.loads((SRC/'KR.region.json').read_text(encoding='utf-8'))
    surface = json.loads((SRC/'surface.json').read_text(encoding='utf-8'))
    validation = json.loads((SRC/'validation.json').read_text(encoding='utf-8'))
    assert shape['country_id'] == 'KR' and len(surface['vertices']) == len(surface['weights'])
    mask = texture()
    mi, target = material(mask, shape['bounds'])
    def ring(points):
        value = unreal.FF_AtlasRegionRing()
        value.set_editor_property('points', [unreal.Vector2D(*p) for p in points])
        return value
    polygons = []
    for polygon in shape['polygons']:
        value = unreal.FF_AtlasRegionPolygon()
        value.set_editor_property('outer', ring(polygon['outer']))
        value.set_editor_property('holes', [ring(h) for h in polygon['holes']])
        polygons.append(value)
    region = unreal.FF_AtlasCountryRegion()
    for k, v in dict(country_id='KR', polygons=polygons, bounds=unreal.LinearColor(*shape['bounds']), mask=mask).items():
        region.set_editor_property(k, v)
    factory = unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class', unreal.FF_AtlasCountryRegions)
    data = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_AtlasCountryRegions', DEST, unreal.FF_AtlasCountryRegions, factory)
    assert data
    props = dict(regions=[region], selection_material=mi, target_material=target,
                 shape_parameters=unreal.load_asset(BASE+'/P05/MPC_BrightGlobe_Controls'),
                 surface_vertices=[unreal.Vector(*p) for p in surface['vertices']],
                 surface_u_vs=[unreal.Vector2D(*p) for p in surface['uvs']],
                 surface_weights=surface['weights'], surface_indices=surface['indices'],
                 source_radius=5000., base_land_thickness=40., base_mountain_height=650.)
    # SurfaceUVs의 Python 이름은 엔진의 약어 변환 규칙으로 확인한다.
    if not hasattr(data, 'surface_u_vs'):
        props['surface_uvs'] = props.pop('surface_u_vs')
    for k, v in props.items(): data.set_editor_property(k, v)
    save(data)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.load_level(MAP)
    globe = next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                 if isinstance(a, unreal.FF_AtlasGlobeActor))
    globe.set_editor_property('region_data', data)
    assert level.save_current_level()
    report['saved'].append(MAP)
    assert hashes() == before, '원본 지구 재질·메시·카탈로그 변경 감지'
    report.update(status='passed', original_assets_unchanged=True, polygons=len(polygons),
                  triangles=len(surface['indices'])//3, bounds=shape['bounds'], region_data=data.get_path_name())


if __name__ == '__main__':
    OUT.mkdir(parents=True, exist_ok=True)
    try: run()
    except Exception:
        report.update(status='failed', error=traceback.format_exc())
        unreal.log_error('ATLAS_REGION_IMPORT_FAIL '+report['error'])
    (OUT/'import_korea.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
