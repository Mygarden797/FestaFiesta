"""기존 영역 재질에 선택 가능/호버/선택/움직임 감소 표현을 추가한다."""
import unreal

mel = unreal.MaterialEditingLibrary
SCALARS = {
    'RegionHovered': (0., 0., 1., '런타임: 마우스가 이 영역 위에 있으면 1.'),
    'RegionSelected': (0., 0., 1., '런타임: 선택한 영역이면 1. 호버와 독립적이다.'),
    'RegionMotionEnabled': (1., 0., 1., '런타임: 움직임 감소 설정에서는 0.'),
    'RegionIdleFillAmount': (.06, 0., .3, '선택 가능한 영역의 기본 채움.'),
    'RegionHoverFillAmount': (.32, 0., .8, '마우스를 올렸을 때 채움.'),
    'RegionSelectedFillAmount': (.52, 0., 1., '선택한 영역의 금빛 채움.'),
    'RegionOutlineWidth': (1.1, .5, 4., '기본 경계의 화면 픽셀 폭. 호버/선택 시 추가로 넓어진다.'),
    'RegionSheenStrength': (.24, 0., .8, '지형 위를 가로지르는 부드러운 반사광. 0이면 끈다.'),
    'RegionSheenSpeed': (.18, 0., 1., '반사광 반복 횟수/초. 0.18은 약 5.6초 주기.'),
    'RegionSheenDirection': (1., -1., 1., '1: 지도 서쪽→동쪽, -1: 동쪽→서쪽.'),
    'RegionSheenWidth': (.16, .03, .4, '반사광 띠의 폭. 영역 너비에 대한 비율.'),
    'RegionGlowStrength': (.55, 0., 2., '경계와 반사광의 약한 자체 발광 강도.'),
}
VECTORS = {
    'RegionHoverColor': ((.84, 1., .67, 1.), '호버 채움: 밝은 연두/아이보리.'),
    'RegionSelectedColor': ((1., .63, .12, 1.), '선택 채움: 따뜻한 금색.'),
}
BASE_INPUTS = ['UV', 'Mask', 'Bounds', 'Base', 'FillColor', 'OutlineColor', 'FillAmount', 'Enabled']
CODE = '''// FF_REGION_FEEDBACK_V2
float2 ll = float2(UV.x * 360.0 - 180.0, 90.0 - UV.y * 180.0);
float2 p = float2((ll.x-Bounds.x)/max(Bounds.z-Bounds.x,.0001), (Bounds.w-ll.y)/max(Bounds.w-Bounds.y,.0001));
float inside = step(0.0,p.x)*step(p.x,1.0)*step(0.0,p.y)*step(p.y,1.0);
float enabled = saturate(Enabled)*inside;
float hovered = saturate(RegionHovered), selected = saturate(RegionSelected);
float3 m = Texture2DSample(Mask, MaskSampler, p).rgb;
float width = clamp(RegionOutlineWidth,.5,4.) + hovered*.8 + selected*.65;
float2 dx = ddx(p)*width, dy = ddy(p)*width;
float3 a = Texture2DSample(Mask, MaskSampler, p+dx).rgb;
float3 b = Texture2DSample(Mask, MaskSampler, p-dx).rgb;
float3 c = Texture2DSample(Mask, MaskSampler, p+dy).rgb;
float3 d = Texture2DSample(Mask, MaskSampler, p-dy).rgb;
float edge = max(m.g, max(max(a.g,b.g),max(c.g,d.g)));
edge = max(edge, max(max(abs(m.r-a.r),abs(m.r-b.r)),max(abs(m.r-c.r),abs(m.r-d.r))));
float outline = saturate(edge*1.8)*enabled;
float fill = lerp(saturate(RegionIdleFillAmount), saturate(RegionSelectedFillAmount), selected);
fill = lerp(fill, max(fill,saturate(RegionHoverFillAmount))+.09*selected, hovered);
// 이전 RegionFillAmount 오버라이드도 전체 채움 배율로 유지한다. .18이 기준이다.
fill = saturate(fill*max(FillAmount,0.)/.18);
float3 fillColor = lerp(FillColor, RegionSelectedColor, selected);
fillColor = lerp(fillColor, lerp(RegionHoverColor, RegionSelectedColor, selected*.75), hovered*.6);
float3 color = lerp(Base, fillColor, saturate(m.r*fill)*enabled);
float3 edgeColor = lerp(OutlineColor, float3(1.,.99,.9), hovered);
color = lerp(color, edgeColor, outline*lerp(.55,1.,max(hovered,selected)));
// 띠가 캔버스 바깥에서 시작/종료하므로 반복 경계에서 번쩍이지 않는다.
float phase = frac(RegionTime*max(RegionSheenSpeed,0.));
float position = lerp(-.5,1.8,phase);
position = lerp(position,1.3-position,step(RegionSheenDirection,-.001));
float stripe = 1.-smoothstep(0.,max(RegionSheenWidth,.01),abs(p.x+.18*p.y-position));
float sheen = stripe*saturate(m.r)*enabled*saturate(RegionMotionEnabled)*clamp(RegionSheenStrength,0.,1.);
color = lerp(color,float3(1.,1.,.94),sheen*(1.+hovered*.4));
float glow = outline*(.18+.5*max(hovered,selected)+.2*hovered*selected) + sheen*.45;
return float4(color,glow);
'''


def upgrade(mat):
    """입력·원래 WPO/노멀을 보존한다. 같은 버전 재실행 시 노드를 늘리지 않는다."""
    rgb = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    assert isinstance(rgb, unreal.MaterialExpressionComponentMask)
    custom = list(mel.get_inputs_for_material_expression(mat, rgb))[0]
    assert isinstance(custom, unreal.MaterialExpressionCustom)
    old_code = custom.get_editor_property('code')
    assert 'float3 filled = lerp(Base, FillColor' in old_code or old_code.strip() == CODE.strip(), \
        '예상하지 못한 수동 셰이더 변경: 자동 패치 중단'
    entries = list(custom.get_editor_property('inputs'))
    names = [str(e.get_editor_property('input_name')) for e in entries]
    assert names[:len(BASE_INPUTS)] == BASE_INPUTS
    before = list(mel.get_inputs_for_material_expression(mat, custom))
    required = ['RegionTime'] + [k for k in SCALARS if k != 'RegionGlowStrength'] + list(VECTORS)
    assert set(names[len(BASE_INPUTS):]).issubset(required) and len(names) == len(set(names))
    missing = [name for name in required if name not in names]
    for name in missing:
        entry = unreal.CustomInput(); entry.set_editor_property('input_name', name); entries.append(entry)
    custom.set_editor_property('inputs', entries)
    custom.set_editor_property('code', CODE)
    custom.set_editor_property('description', '국가 선택 가능 · 흐르는 반사광 · 독립 호버 · 금빛 선택')
    for name in missing:
        if name == 'RegionTime':
            expr = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -600, 1100)
            expr.set_editor_property('ignore_pause', True)
        elif name in VECTORS:
            value, desc = VECTORS[name]
            expr = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 1200)
            for k, v in dict(parameter_name=name, default_value=unreal.LinearColor(*value),
                             group='국가 영역 색상', desc=desc).items(): expr.set_editor_property(k, v)
        else:
            expr = scalar(mat, name)
        assert mel.connect_material_expressions(expr, 'RGB' if name in VECTORS else '', custom, name)
    assert list(mel.get_inputs_for_material_expression(mat, custom))[:len(before)] == before
    add = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert isinstance(add, unreal.MaterialExpressionAdd)
    glow = list(mel.get_inputs_for_material_expression(mat, add))[1]
    assert isinstance(glow, unreal.MaterialExpressionMultiply)
    glow_inputs = list(mel.get_inputs_for_material_expression(mat, glow))
    if len(glow_inputs) < 2 or not glow_inputs[1]:
        assert mel.connect_material_expressions(scalar(mat, 'RegionGlowStrength'), '', glow, 'B')
    else:
        assert str(glow_inputs[1].get_editor_property('parameter_name')) == 'RegionGlowStrength'
    return {'schema': 'FF_REGION_FEEDBACK_V2', 'new_inputs': missing,
            'original_input_connections_preserved': True}


def scalar(mat, name):
    value, lower, upper, desc = SCALARS[name]
    expr = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 1400)
    group = '국가 영역 반사광' if 'Sheen' in name else '국가 영역 상태'
    for k, v in dict(parameter_name=name, default_value=value, slider_min=lower, slider_max=upper,
                     group=group, desc=desc, sort_priority=list(SCALARS).index(name)).items():
        expr.set_editor_property(k, v)
    return expr
