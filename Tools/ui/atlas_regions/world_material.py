"""한국 시범 재질 복제본을 8개 국가 동시 표시용으로 확장한다."""
import unreal
from region_feedback import CODE

mel = unreal.MaterialEditingLibrary
SLOT_COUNT = 8  # UFF_AtlasCountryRegions::MaxRenderingRegions와 같은 계약.
WORLD_CODE = CODE.replace('FF_REGION_FEEDBACK_V2', 'FF_REGION_FEEDBACK_WORLD_V1')
WORLD_CODE = WORLD_CODE.replace('float2 p =', '''// 국가 바운드와 같은 경도 구간으로 이동한다. 미국의 날짜변경선 양쪽 섬을 이어 준다.
ll.x += 360. * floor(((Bounds.x+Bounds.z)*.5-ll.x)/360.+.5);
float2 p =''')
WORLD_CODE = WORLD_CODE.replace('float hovered =', '''float2 gradX = ddx(p), gradY = ddy(p);
// 화면 대부분은 이 국가 바운드 밖이다. 미분을 먼저 구하고 실제 텍스처 샘플은 생략한다.
[branch] if (enabled <= 0.) { return float4(Base,0.); }
float hovered =''')
WORLD_CODE = WORLD_CODE.replace('ddx(p)*width, dy = ddy(p)*width', 'gradX*width, dy = gradY*width')
for suffix in ('', '+dx', '-dx', '+dy', '-dy'):
    WORLD_CODE = WORLD_CODE.replace('Texture2DSample(Mask, MaskSampler, p'+suffix+')',
                                   'Texture2DSampleGrad(Mask, MaskSampler, p'+suffix+', gradX, gradY)')


def build(mat, regions):
    assert len(regions) == SLOT_COUNT and len({str(r.country_id) for r in regions}) == SLOT_COUNT
    rgb = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    assert isinstance(rgb, unreal.MaterialExpressionComponentMask)
    first = list(mel.get_inputs_for_material_expression(mat, rgb))[0]
    assert isinstance(first, unreal.MaterialExpressionCustom) and first.get_editor_property('code').strip() == CODE.strip()
    names = [str(e.get_editor_property('input_name')) for e in first.get_editor_property('inputs')]
    original_inputs = list(mel.get_inputs_for_material_expression(mat, first))
    assert len(names) == len(original_inputs) and all(original_inputs)
    shared = dict(zip(names, original_inputs))
    add = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    glow = list(mel.get_inputs_for_material_expression(mat, add))[1]
    first_alpha = list(mel.get_inputs_for_material_expression(mat, glow))[0]
    assert isinstance(first_alpha, unreal.MaterialExpressionComponentMask)

    def node(cls, **props):
        obj = mel.create_material_expression(mat, cls, 0, 0)
        for k, v in props.items(): obj.set_editor_property(k, v)
        return obj

    def connect(source, target, pin, output=''):
        assert mel.connect_material_expressions(source, output, target, pin)

    current_rgb, sum_alpha = rgb, first_alpha
    custom_nodes = []
    for slot, region in enumerate(regions):
        if slot == 0:
            custom = first
        else:
            entries = []
            for name in names:
                entry = unreal.CustomInput(); entry.set_editor_property('input_name', name); entries.append(entry)
            custom = node(unreal.MaterialExpressionCustom, inputs=entries,
                          output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        # 기존 팔레트·노멀·WPO·재질 파라미터를 같은 입력 노드로 공유한다.
        for name in names:
            if name in ('Mask', 'Bounds', 'Enabled', 'RegionHovered', 'RegionSelected'):
                suffix = dict(Mask='Mask', Bounds='Bounds', Enabled='Enabled',
                              RegionHovered='Hovered', RegionSelected='Selected')[name]
                parameter = f'Region{slot}{suffix}'
                props = dict(parameter_name=parameter, group='국가 슬롯 (런타임)',
                             desc=f'{region.country_id} · 데이터 등록 순서 {slot}. 런타임에서 지정한다.')
                if name == 'Mask':
                    expr = node(unreal.MaterialExpressionTextureObjectParameter,
                                texture=region.mask, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, **props)
                elif name == 'Bounds':
                    expr = node(unreal.MaterialExpressionVectorParameter, default_value=region.bounds, **props)
                else:
                    expr = node(unreal.MaterialExpressionScalarParameter, default_value=0., **props)
                connect(expr, custom, name, 'RGBA' if name == 'Bounds' else '')
            elif name == 'Base' and slot > 0:
                connect(current_rgb, custom, name)
            else:
                output = 'RGB' if name in ('FillColor', 'OutlineColor', 'RegionHoverColor', 'RegionSelectedColor') else ''
                # Base 입력의 원래 출력 핀은 복제본의 연결을 유지한다.
                if slot == 0 and name == 'Base': continue
                connect(shared[name], custom, name, output)
        custom.set_editor_property('code', WORLD_CODE)
        custom.set_editor_property('description', f'국가 {slot} {region.country_id} · 독립 호버/선택 · 경도 이음선')
        custom_nodes.append(custom)
        if slot > 0:
            current_rgb = node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
            alpha = node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
            connect(custom, current_rgb, ''); connect(custom, alpha, '')
            total = node(unreal.MaterialExpressionAdd)
            connect(sum_alpha, total, 'A'); connect(alpha, total, 'B'); sum_alpha = total
    connect(sum_alpha, glow, 'A')
    assert mel.connect_material_property(current_rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
    assert all(all(mel.get_inputs_for_material_expression(mat, custom)) for custom in custom_nodes)
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    return {'schema':'FF_REGION_FEEDBACK_WORLD_V1', 'slots':[str(r.country_id) for r in regions],
            'branch_skips_outside_country_bounds':True, 'original_terrain_connections_preserved':True}
