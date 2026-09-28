# create_festival_materials.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 필요한 에셋 또는 설정을 생성한다.

"""Create the festival field-guide's procedural UI surfaces.

Run before FF.ApplyFacilityCodexFestivalTheme in an idle Entry editor. The
header's pearl interference edge, distant waves and festoon bulbs are native
UI shaders. Card centers remain quiet ivory so collection states stay legible.
Owned assets are versioned; subsequent runs leave their saved bytes unchanged.
"""
import unreal

FOLDER = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials"
BASE_NAME = "M_FF_CodexFestivalSurface"
VERSION_KEY = "FF.FacilityCodex.FestivalSurfaceVersion"
VERSION = "1"
DEFAULTS = {"SurfaceRole": 0.0, "Aspect": 20.625, "MotionStrength": 1.0}
STATES = {
    "Header": {"SurfaceRole": 0.0, "Aspect": 20.625, "MotionStrength": 1.0},
    "Paper": {"SurfaceRole": 1.0, "Aspect": 1.65, "MotionStrength": 0.0},
    "Pass": {"SurfaceRole": 2.0, "Aspect": 1.02, "MotionStrength": 0.0},
    "Selection": {"SurfaceRole": 3.0, "Aspect": 1.02, "MotionStrength": 1.0},
}

SHADER = r"""
float2 uv = saturate(UV);
float aspect = max(Aspect, 0.5);
float2 halfSize = float2(aspect, 1.0) * 0.5;
float radius = SurfaceRole < 0.5 ? 0.14 : (SurfaceRole < 1.5 ? 0.021 : 0.043);
float2 q = abs((uv - 0.5) * float2(aspect, 1.0)) - (halfSize - radius);
float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
float aa = max(fwidth(distance), 0.0008);
float alpha = 1.0 - smoothstep(-aa, aa, distance);
float inward = -distance;
float phase = Clock * 0.12 * saturate(MotionStrength);
float filmA = 0.5 + 0.5 * sin(uv.x * 8.0 + uv.y * 4.0 + phase);
float filmB = 0.5 + 0.5 * sin(uv.x * 5.0 - uv.y * 7.0 + phase + 1.7);
float3 pearl = lerp(float3(0.97, 0.83, 0.52), float3(0.50, 0.88, 0.77), filmA);
pearl = lerp(pearl, float3(0.76, 0.72, 0.91), filmB * 0.27);
float3 color;

if (SurfaceRole < 0.5)
{
    // Ocean/emerald ink: the readable center is never animated.
    color = lerp(float3(0.055, 0.205, 0.205), float3(0.085, 0.295, 0.250), uv.x);
    color += float3(0.01, 0.025, 0.025) * (1.0 - uv.y);
    float rim = 1.0 - smoothstep(0.016, 0.035, inward);
    color = lerp(color, pearl * 0.92, rim * 0.90);
    float waveY = 0.86 + sin(uv.x * 20.0) * 0.045;
    float wave = exp(-pow((uv.y - waveY) * 85.0, 2.0));
    float wave2 = exp(-pow((uv.y - waveY + 0.065) * 80.0, 2.0));
    color += float3(0.32, 0.53, 0.43) * (wave + wave2 * 0.45) * smoothstep(0.48, 0.86, uv.x) * 0.12;
    // A fine cable and widely spaced lights echo the festival's waterfront.
    float cableY = 0.10 + sin(uv.x * 11.0) * 0.025;
    float right = smoothstep(0.72, 0.86, uv.x);
    float cable = exp(-pow((uv.y - cableY) * 105.0, 2.0));
    float lampX = (frac(uv.x * 34.0) - 0.5) * 34.0;
    float lampY = (uv.y - cableY - 0.052) * 28.0;
    float bulb = exp(-(lampX * lampX + lampY * lampY) * 4.0);
    color += float3(0.94, 0.81, 0.49) * right * (cable * 0.045 + bulb * 0.25);
}
else if (SurfaceRole < 1.5)
{
    // Uncoated field-guide paper, with a quiet tidal mark in the margin only.
    color = lerp(float3(0.982, 0.980, 0.936), float3(0.942, 0.954, 0.891), uv.y);
    float edge = 1.0 - smoothstep(0.001, 0.004, inward);
    color = lerp(color, float3(0.59, 0.69, 0.57), edge * 0.72);
    float wave = exp(-pow((uv.y - 0.979 - sin(uv.x * 19.0) * 0.003) * 430.0, 2.0));
    color -= float3(0.10, 0.07, 0.12) * wave * 0.09;
}
else if (SurfaceRole < 2.5)
{
    // A collectible festival pass: clean cream surface and a tiny ticket seam.
    color = lerp(float3(1.0, 0.997, 0.966), float3(0.976, 0.977, 0.940), uv.y);
    float edge = 1.0 - smoothstep(0.002, 0.006, inward);
    color = lerp(color, float3(0.65, 0.73, 0.62), edge * 0.72);
    float seam = exp(-pow((uv.y - 0.756) * 230.0, 2.0));
    float dash = smoothstep(0.24, 0.32, frac(uv.x * 22.0));
    color = lerp(color, float3(0.82, 0.79, 0.65), seam * dash * 0.23);
    float topLip = exp(-pow((uv.y - 0.011) * 260.0, 2.0));
    color = lerp(color, float3(0.72, 0.78, 0.58), topLip * 0.38);
}
else
{
    // The transparent center preserves the pass and its independent state tint.
    float edge = 1.0 - smoothstep(0.008, 0.014, inward);
    color = lerp(float3(0.15, 0.46, 0.35), pearl * 0.74, 0.42);
    alpha *= edge;
}
return float4(pow(saturate(color), 2.2), alpha);
"""


def existing_owned(name):
    path = FOLDER + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    asset = unreal.load_asset(path)
    if not asset or unreal.EditorAssetLibrary.get_metadata_tag(asset, VERSION_KEY) != VERSION:
        raise RuntimeError("Existing festival surface lacks the expected ownership marker: " + path)
    return asset


def save_owned(asset):
    unreal.EditorAssetLibrary.set_metadata_tag(asset, VERSION_KEY, VERSION)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def create_base(asset_tools, lib):
    material = existing_owned(BASE_NAME)
    if material:
        return material
    material = asset_tools.create_asset(BASE_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    assert material
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    sources = {
        "UV": lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -680, -160),
        "Clock": lib.create_material_expression(material, unreal.MaterialExpressionTime, -680, -40),
    }
    for index, (name, default) in enumerate(DEFAULTS.items()):
        parameter = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -680, 100 + index * 140)
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", default)
        sources[name] = parameter
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -280, -100)
    custom.set_editor_property("description", "Festival field-guide: ocean, pearl edge, quiet ivory passes")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in sources:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        inputs.append(pin)
    custom.set_editor_property("inputs", inputs)
    custom.set_editor_property("code", SHADER)
    for name, expression in sources.items():
        assert lib.connect_material_expressions(expression, "", custom, name), name
    rgb = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 100, -100)
    opacity = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 100, 80)
    for key in ("r", "g", "b", "a"):
        rgb.set_editor_property(key, key != "a")
        opacity.set_editor_property(key, key == "a")
    assert lib.connect_material_expressions(custom, "", rgb, "")
    assert lib.connect_material_expressions(custom, "", opacity, "")
    assert lib.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    save_owned(material)
    return material


def create_materials():
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    material = create_base(tools, lib)
    result = {"Base": material}
    for state, overrides in STATES.items():
        name = "MI_FF_CodexFestival_" + state
        instance = existing_owned(name)
        if not instance:
            instance = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            assert instance
            lib.set_material_instance_parent(instance, material)
            for parameter, value in {**DEFAULTS, **overrides}.items():
                lib.set_material_instance_scalar_parameter_value(instance, parameter, value)
                assert abs(lib.get_material_instance_scalar_parameter_value(instance, parameter) - value) < 0.0001
            save_owned(instance)
        result[state] = instance
    unreal.log("FF_CODEX_FESTIVAL_MATERIALS_PASS")
    return result


if __name__ == "__main__":
    create_materials()
