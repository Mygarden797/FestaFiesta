"""등록된 한국 지형/알파를 그대로 사용하는 밝은 UI 재질. 이미지 재생성은 하지 않는다."""
from pathlib import Path
import unreal

DEST = '/Game/FestaFiesta/UI/Atlas/Korea/CleanBright'
TERRAIN = '/Game/FestaFiesta/UI/Atlas/Korea/Map/T_FF_FA_Map_KoreaTerrainEdgeSafe_2400x2160'
mel = unreal.MaterialEditingLibrary


def save(obj):
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False)


def linear(hex_color):
    values = [int(hex_color[i:i + 2], 16) / 255. for i in (0, 2, 4)]
    return unreal.LinearColor(*[(v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4)
                                for v in values], 1.)


def asset(name, cls, factory):
    path = DEST + '/' + name
    existing = unreal.load_asset(path)
    if existing:
        assert isinstance(existing, cls), path
        return existing, False
    result = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, cls, factory())
    assert result, path
    return result, True


def import_data(root, name, filename, height=False):
    existing = unreal.load_asset(DEST + '/' + name)
    if existing:
        return existing
    task = unreal.AssetImportTask()
    for key, value in dict(filename=str(root / 'SourceArt/UI/Atlas/KoreaNaturalParts_v1/DataMasks' / filename),
                           destination_path=DEST, destination_name=name, automated=True,
                           replace_existing=False, save=False).items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    result = task.get_objects()[0]
    assert isinstance(result, unreal.Texture2D)
    for key, value in dict(srgb=False, never_stream=True, address_x=unreal.TextureAddress.TA_CLAMP,
                           address_y=unreal.TextureAddress.TA_CLAMP,
                           filter=unreal.TextureFilter.TF_BILINEAR,
                           # 색상 구분용 높이이므로 R 채널을 보존한다. Displacementmap은 Alpha를 복제한다.
                           compression_settings=unreal.TextureCompressionSettings.TC_GRAYSCALE,
                           lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,
                           mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS).items():
        result.set_editor_property(key, value)
    save(result)
    return result


def material(name, code, vectors, scalars, textures=None, alpha=False):
    mat, created = asset(name, unreal.Material, unreal.MaterialFactoryNew)
    if created:
        mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT if alpha else unreal.BlendMode.BLEND_OPAQUE)

        def node(cls, **props):
            obj = mel.create_material_expression(mat, cls, 0, 0)
            for key, value in props.items():
                obj.set_editor_property(key, value)
            return obj

        def connect(src, target, pin, output=''):
            assert mel.connect_material_expressions(src, output, target, pin)

        names = ['UV'] + list(vectors) + list(scalars) + list(textures or {})
        inputs = []
        for name in names:
            entry = unreal.CustomInput()
            entry.set_editor_property('input_name', name)
            inputs.append(entry)
        shader = node(unreal.MaterialExpressionCustom, inputs=inputs, code=code,
                      output_type=(unreal.CustomMaterialOutputType.CMOT_FLOAT4 if alpha
                                   else unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                      description='원본 지도 등록 유지 · 색상/명암만 단순화')
        connect(node(unreal.MaterialExpressionTextureCoordinate), shader, 'UV')
        for key, color in vectors.items():
            connect(node(unreal.MaterialExpressionVectorParameter, parameter_name=key,
                         default_value=linear(color), group='색상'), shader, key, 'RGB')
        for key, (value, lower, upper, description) in scalars.items():
            connect(node(unreal.MaterialExpressionScalarParameter, parameter_name=key,
                         default_value=value, slider_min=lower, slider_max=upper,
                         group='표현 강도', desc=description), shader, key)
        for key, texture in (textures or {}).items():
            connect(node(unreal.MaterialExpressionTextureObjectParameter, parameter_name=key,
                         texture=texture, group='원본 지리 데이터'), shader, key)
        if alpha:
            color = node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
            opacity = node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
            connect(shader, color, '')
            connect(shader, opacity, '')
            assert mel.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
        else:
            color = shader
        assert mel.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.layout_material_expressions(mat)
        mel.recompile_material(mat)
        save(mat)
    mi_name = mat.get_name().replace('M_', 'MI_', 1)
    mi, created_mi = asset(mi_name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew)
    if created_mi:
        mel.set_material_instance_parent(mi, mat)
        save(mi)
    return mi


MAP_CODE = '''
// FF_CLEAN_KOREA_V1: UV와 알파는 기존 2400x2160 지도와 동일하다.
float a = Texture2DSample(Terrain, TerrainSampler, UV).a;
float2 d = max(SmoothingPixels, 0.) / float2(2400., 2160.);
float h = 0., s = 0.;
[unroll] for (int y=-1; y<=1; ++y) {
    [unroll] for (int x=-1; x<=1; ++x) {
        float2 p = UV + float2(x, y)*d;
        h += Texture2DSample(Height, HeightSampler, p).r;
        s += Texture2DSample(Shade, ShadeSampler, p).r;
    }
}
h = saturate(h / 9. * HeightScale);
s /= 9.;
float t = smoothstep(.025, .48, h);
float3 rgb = lerp(LowlandColor, HighlandColor, t);
rgb *= 1. + clamp(ReliefStrength, 0., .35) * (s-.5);
float2 px = max(CoastWidth, 0.) / float2(2400.,2160.);
float inside = min(min(Texture2DSample(Terrain,TerrainSampler,UV+float2(px.x,0.)).a,
                       Texture2DSample(Terrain,TerrainSampler,UV-float2(px.x,0.)).a),
                   min(Texture2DSample(Terrain,TerrainSampler,UV+float2(0.,px.y)).a,
                       Texture2DSample(Terrain,TerrainSampler,UV-float2(0.,px.y)).a));
rgb = lerp(rgb, CoastColor, saturate(a-inside)*.8);
return float4(rgb, a);
'''

BACKGROUND_CODE = '''
float2 p = (UV-float2(.40,.48))*float2(1.1,1.);
float water = 1.-smoothstep(.14,.78,length(p));
water *= 1.-.25*smoothstep(.6,1.,UV.x);
return lerp(CreamColor, WaterColor, water*saturate(WaterStrength));
'''


def build(root):
    terrain = unreal.load_asset(TERRAIN)
    assert isinstance(terrain, unreal.Texture2D), TERRAIN
    height = import_data(root, 'T_KoreaCleanBrightHeight', 'T_FF_FA_Height_Korea_G16_2400x2160.png', True)
    shade = import_data(root, 'T_KoreaCleanBrightHillshade', 'T_FF_FA_Hillshade_Korea_G8_2400x2160.png')
    map_mi = material('M_KoreaCleanBrightMap', MAP_CODE,
                      dict(LowlandColor='B9D98C', HighlandColor='76AD79', CoastColor='FFF2CB'),
                      dict(SmoothingPixels=(4., 0., 12., '지형 세부를 부드럽게 묶는 원본 픽셀 간격'),
                           HeightScale=(2., .2, 5., '고도 색상 대비'),
                           ReliefStrength=(.16, 0., .35, '지형 명암 강도'),
                           CoastWidth=(2., 0., 6., '해안 안쪽 크림색 선 두께(원본px)')),
                      dict(Terrain=terrain, Height=height, Shade=shade), True)
    backdrop = material('M_KoreaCleanBrightBackdrop', BACKGROUND_CODE,
                        dict(CreamColor='FCFAF1', WaterColor='B4E6ED'),
                        dict(WaterStrength=(.9, 0., 1., '하늘색 배경 강도')))
    return map_mi, backdrop
