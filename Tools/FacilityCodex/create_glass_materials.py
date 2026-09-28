# create_glass_materials.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 필요한 에셋 또는 설정을 생성한다.

"""Author the fullscreen festival codex's translucent glass UI surfaces.

Run in an idle Entry editor before FF.ApplyFacilityCodexGlassTheme. This creates
new materials only; the existing festival, reward and catalog-focus materials
are preserved. All measurements in the shader derive from UV derivatives, so
the same rounded glass does not stretch its bevel on a fullscreen panel.

The screen and the nested panels intentionally use real opacity. Paper-white
chrome and a soft sage backdrop frame pearl-white collection cards and details.
Selection has a transparent center with a forest inner edge and pearl outer rim.
"""
import unreal


FOLDER = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials"
BASE_NAME = "M_FF_CodexGlass"
VERSION_KEY = "FF.FacilityCodex.GlassSurfaceVersion"
VERSION = "1"
DEFAULTS = {
    "SurfaceRole": 0.0,
    "SurfaceOpacity": 0.54,
    "CornerRadiusPixels": 20.0,
    "EdgeWidthPixels": 1.5,
    "RimStrength": 1.0,
    "MotionStrength": 1.0,
}
STATES = {
    "Screen": {"SurfaceRole": 0.0, "SurfaceOpacity": 0.96, "CornerRadiusPixels": 20.0, "EdgeWidthPixels": 1.5},
    "Header": {"SurfaceRole": 1.0, "SurfaceOpacity": 0.68, "CornerRadiusPixels": 14.0, "EdgeWidthPixels": 1.0},
    "Card": {"SurfaceRole": 2.0, "SurfaceOpacity": 0.70, "CornerRadiusPixels": 12.0, "EdgeWidthPixels": 1.0,
             "RimStrength": 0.60, "MotionStrength": 0.0},
    "Selection": {"SurfaceRole": 3.0, "SurfaceOpacity": 0.0, "CornerRadiusPixels": 12.0, "EdgeWidthPixels": 2.2,
                  "RimStrength": 1.0},
    "Detail": {"SurfaceRole": 4.0, "SurfaceOpacity": 0.76, "CornerRadiusPixels": 18.0, "EdgeWidthPixels": 1.25},
    "Inset": {"SurfaceRole": 5.0, "SurfaceOpacity": 0.45, "CornerRadiusPixels": 10.0, "EdgeWidthPixels": 1.0,
              "RimStrength": 0.45, "MotionStrength": 0.0},
}

SHADER = r"""
float2 uv = saturate(UV);

// Derivative-derived physical pixels keep corners and the optical rim uniform
// at every authored aspect ratio and at every viewport scale. No Aspect input.
float pixelU = max(length(float2(ddx(UV.x), ddy(UV.x))), 0.00001);
float pixelV = max(length(float2(ddx(UV.y), ddy(UV.y))), 0.00001);
float2 sizePixels = clamp(1.0 / float2(pixelU, pixelV), 1.0, 16384.0);
float2 halfSize = sizePixels * 0.5;
float radius = clamp(CornerRadiusPixels, 0.0, min(halfSize.x, halfSize.y));
float2 q = abs((uv - 0.5) * sizePixels) - (halfSize - radius);
float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
float aa = max(fwidth(distance), 0.75);
float shape = 1.0 - smoothstep(-aa, aa, distance);
float inward = -distance;
float edgeWidth = max(EdgeWidthPixels, 0.75);
float rim = 1.0 - smoothstep(edgeWidth - aa * 0.45, edgeWidth + aa * 0.65, inward);
float bevel = 1.0 - smoothstep(edgeWidth, edgeWidth + 8.0, inward);
float lip = exp(-pow((inward - edgeWidth - 2.0) * 0.8, 2.0));
float upper = 1.0 - smoothstep(0.12, 0.82, uv.y);

float role = SurfaceRole;
float motion = saturate(MotionStrength);
float clockPhase = Clock * 0.10 * motion;
// An analytic optical phase blends champagne, mint and pearl-lilac. The
// 63-second phase cycle is smooth and never pulses opacity or text contrast.
float viewPhase = uv.x * 5.2 + uv.y * 3.7 + clockPhase;
float filmA = 0.5 + 0.5 * sin(viewPhase);
float filmB = 0.5 + 0.5 * sin(viewPhase * 0.72 - uv.y * 5.3 + 1.2);
float3 pearl = lerp(float3(0.96, 0.84, 0.56), float3(0.57, 0.92, 0.81), filmA);
pearl = lerp(pearl, float3(0.79, 0.76, 0.96), filmB * 0.34);
float strength = saturate(RimStrength);

// These RGB values are display-space art direction; convert once at output.
// Paper is the project's #F7F8F6. The entire collection shares its light canvas.
float3 paper = float3(0.968627, 0.972549, 0.964706);
float3 forest = float3(0.075, 0.235, 0.185);
float3 color = paper;
float opacity = saturate(SurfaceOpacity);

if (role < 0.5)
{
    // Near-white glass remains white even over the nighttime festival scene.
    color = lerp(paper, float3(0.950, 0.955, 0.943), uv.y);
}
else if (role < 1.5)
{
    // Waterline and festival string lights live in the header margins only.
    color = lerp(float3(0.989, 0.988, 0.974), paper, uv.y);
    float waveY = 0.83 + sin(uv.x * 18.0) * 0.05;
    float waveDistance = abs(uv.y - waveY) * sizePixels.y;
    float wave2Distance = abs(uv.y - waveY + 0.055) * sizePixels.y;
    float waves = exp(-waveDistance * waveDistance * 0.35) + exp(-wave2Distance * wave2Distance * 0.4) * 0.4;
    color = lerp(color, float3(0.51, 0.61, 0.54), waves * smoothstep(0.53, 0.9, uv.x) * 0.18);

    float cableY = 0.11 + sin(uv.x * 10.0) * 0.032;
    float right = smoothstep(0.73, 0.91, uv.x);
    float cableDistance = abs(uv.y - cableY) * sizePixels.y;
    float cable = exp(-cableDistance * cableDistance * 0.7);
    float lampX = (frac(uv.x * 25.0) - 0.5) * sizePixels.x / 25.0;
    float lampY = (uv.y - cableY) * sizePixels.y - 4.0;
    float bulb = exp(-(lampX * lampX + lampY * lampY) * 0.32);
    float halo = exp(-(lampX * lampX + lampY * lampY) * 0.027);
    color = lerp(color, float3(0.68, 0.56, 0.37), right * (cable * 0.18 + bulb * 0.28 + halo * 0.015));
}
else if (role < 2.5)
{
    // Quiet paper-white cards support dark ink and colorful facility thumbnails.
    color = lerp(paper, float3(0.945, 0.957, 0.938), uv.y);
}
else if (role > 3.5 && role < 4.5)
{
    color = lerp(float3(0.980, 0.981, 0.966), paper, uv.y);
}
else if (role > 4.5)
{
    color = lerp(float3(0.930, 0.945, 0.909), float3(0.883, 0.911, 0.872), uv.y);
}

// A broad reflection reads as glass without sweeping a bright stripe over text.
// Cards/insets have zero motion; only large surfaces and the selected frame move.
float reflectionPosition = 0.36 + sin(clockPhase * 0.53) * 0.26;
float broadReflection = exp(-pow((uv.x + uv.y * 0.27 - reflectionPosition) * 2.9, 2.0));
float largeSurface = (role < 1.5 || (role > 3.5 && role < 4.5)) ? 1.0 : 0.0;
float lightSurface = role < 2.5 || role > 3.5 ? 1.0 : 0.0;
float reflectionStrength = lerp(1.0, 0.35, lightSurface);
color += pearl * broadReflection * largeSurface * 0.026 * reflectionStrength;
color += pearl * bevel * upper * 0.105 * strength * reflectionStrength;
color += float3(0.74, 0.96, 0.85) * lip * upper * 0.055 * strength * reflectionStrength;
color = lerp(color, pearl * (0.63 + upper * 0.23), rim * strength * 0.78);
opacity = lerp(opacity, max(opacity, 0.86), rim * strength * 0.75);

if (role > 2.5 && role < 3.5)
{
    // No center fog: this frame must never cover the thumbnail or reward action.
    // Forest on the inside stays legible over white; only the outer edge shimmers.
    // Color changes inside the existing rim, preserving its pixel width and alpha.
    float innerBoundary = smoothstep(max(0.85, edgeWidth * 0.40), max(1.4, edgeWidth * 0.74), inward);
    color = lerp(pearl * (0.82 + upper * 0.15), forest, innerBoundary);
    opacity = rim * 0.97;
}
return float4(pow(saturate(color), 2.2), shape * saturate(opacity));
"""


def existing_owned(name):
    path = FOLDER + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    asset = unreal.load_asset(path)
    if not asset or unreal.EditorAssetLibrary.get_metadata_tag(asset, VERSION_KEY) != VERSION:
        raise RuntimeError("Existing glass material lacks the expected ownership marker: " + path)
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
        "UV": lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -760, -240),
        "Clock": lib.create_material_expression(material, unreal.MaterialExpressionTime, -760, -110),
    }
    for index, (name, default) in enumerate(DEFAULTS.items()):
        expression = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -760, 70 + index * 130)
        expression.set_editor_property("parameter_name", name)
        expression.set_editor_property("default_value", default)
        sources[name] = expression
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -300, -170)
    custom.set_editor_property("description", "Festival glass: real translucency, derivative-sized pearl rim, quiet ocean center")
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
    rgb = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 140, -170)
    opacity = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 140, 10)
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
    # Validate every existing destination before creating anything new.
    for name in [BASE_NAME] + ["MI_FF_CodexGlass_" + state for state in STATES]:
        existing_owned(name)
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    material = create_base(asset_tools, lib)
    result = {"Base": material}
    for state, overrides in STATES.items():
        name = "MI_FF_CodexGlass_" + state
        instance = existing_owned(name)
        if not instance:
            instance = asset_tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            assert instance
            lib.set_material_instance_parent(instance, material)
            for parameter, value in {**DEFAULTS, **overrides}.items():
                lib.set_material_instance_scalar_parameter_value(instance, parameter, float(value))
                actual = lib.get_material_instance_scalar_parameter_value(instance, parameter)
                assert abs(actual - float(value)) < 0.0001, parameter
            save_owned(instance)
        result[state] = instance
    unreal.log("FF_CODEX_GLASS_MATERIALS_PASS")
    return result


if __name__ == "__main__":
    create_materials()
