"""Create the codex reward button's iridescent UI material and four states.

Run in the editor before applying the reward widget style.  The material draws
its own antialiased silhouette, bevel, and restrained animated interference rim;
the gift/check and caption remain ordinary readable widgets above it.

Asset creation is idempotent. Existing assets without this version's ownership
marker are rejected instead of being silently replaced.
"""
import unreal


FOLDER = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials"
BASE_NAME = "M_FF_CodexRewardIridescent"
VERSION_KEY = "FF.FacilityCodex.RewardIridescenceVersion"
VERSION = "1"

# Aspect is width / height of the complete button brush, including its padding.
# FilmPhase shifts the interference colors without changing status or contrast.
# State: 0 ready, 1 hovered, 2 pressed, 3 claimed. Claimed is deliberately static.
DEFAULTS = {
    "Aspect": 2.25,
    "FilmPhase": 0.0,
    "State": 0.0,
    "MotionStrength": 1.0,
    "RimStrength": 1.0,
}
STATES = {
    "Ready": {"State": 0.0, "MotionStrength": 1.0, "RimStrength": 1.0},
    "Hover": {"State": 1.0, "MotionStrength": 1.0, "RimStrength": 1.15},
    "Pressed": {"State": 2.0, "MotionStrength": 0.0, "RimStrength": 0.78},
    "Claimed": {"State": 3.0, "MotionStrength": 0.0, "RimStrength": 0.3},
}

SHADER = r"""
float2 p = saturate(UV);
float aspect = max(Aspect, 0.5);
float2 halfSize = float2(aspect, 1.0) * 0.5;
float radius = 0.235;
float2 q = abs((p - 0.5) * float2(aspect, 1.0)) - (halfSize - radius);
float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
float aa = max(fwidth(distance), 0.001);
float alpha = 1.0 - smoothstep(-aa, aa, distance);
float inward = -distance;

// All colors below are authored in display RGB, converted once at the output.
// A warm gold action surface stands apart from the surrounding white collection.
// Its stable bright center supports dark bronze foreground widgets in every frame.
float3 goldTop = float3(0.995, 0.905, 0.635);
float3 goldBottom = float3(0.905, 0.695, 0.355);
float3 color = lerp(goldTop, goldBottom, smoothstep(0.05, 0.95, p.y));
float3 champagne = float3(1.0, 0.945, 0.760);
float3 honey = float3(0.985, 0.785, 0.390);
float3 mint = float3(0.790, 0.910, 0.755);

// Changing optical phase is concentrated at the raised edge, not the glyphs.
// Only ready/hover surfaces move. Pressed and claimed remain completely static.
// Optical motion changes color only; alpha never flashes or pulses.
float motion = saturate(MotionStrength) * (State < 1.5 ? 1.0 : 0.0);
float phase = FilmPhase + Clock * 0.45 * motion;
float filmA = 0.5 + 0.5 * sin(p.x * 5.8 + p.y * 3.0 + phase);
float filmB = 0.5 + 0.5 * sin(p.x * 4.2 - p.y * 5.0 + phase + 1.4);
float3 film = lerp(champagne, honey, filmA * 0.55);
film = lerp(film, mint, filmB * 0.14);

float edge = 1.0 - smoothstep(0.030 - aa, 0.048 + aa, inward);
float bevel = 1.0 - smoothstep(0.047, 0.105, inward);
float upper = 1.0 - smoothstep(0.12, 0.85, p.y);
float strength = clamp(RimStrength, 0.0, 1.3);
float breath = 0.94 + sin(Clock * 0.95 + FilmPhase) * 0.06 * motion;
float3 edgeGold = State > 0.5 && State < 1.5 ? float3(0.50, 0.305, 0.075) : float3(0.64, 0.415, 0.135);
color += film * bevel * (0.035 + 0.035 * upper) * strength * breath;
color = lerp(color, edgeGold + film * upper * 0.12 * breath, saturate(edge * strength));

// A fine inner lip and a very soft diagonal light establish a game-like bevel.
float lip = exp(-pow((inward - 0.060) * 100.0, 2.0));
color += champagne * lip * upper * 0.085;
float sweepPosition = frac(Clock * 0.25) * 1.9 - 0.45;
float sweep = exp(-pow((p.x + p.y * 0.22 - sweepPosition) * 9.0, 2.0));
color += champagne * sweep * motion * (0.038 + bevel * 0.035);

// Two tiny, fixed star glints stay near the rim, clear of the icon and caption.
float2 starA = (p - float2(0.19, 0.17)) * float2(aspect, 1.0);
float2 starB = (p - float2(0.82, 0.81)) * float2(aspect, 1.0);
float raysA = exp(-abs(starA.x) * 330.0 - abs(starA.y) * 55.0)
            + exp(-abs(starA.x) * 65.0 - abs(starA.y) * 290.0);
float raysB = exp(-abs(starB.x) * 330.0 - abs(starB.y) * 55.0)
            + exp(-abs(starB.x) * 65.0 - abs(starB.y) * 290.0);
float glints = raysA * (0.085 + 0.035 * sin(phase + 0.4))
             + raysB * (0.070 + 0.030 * sin(phase + 2.0));
color += champagne * glints * motion;

if (State > 0.5 && State < 1.5)
{
    color += float3(0.012, 0.026, 0.028) * (1.0 - edge);
    color = lerp(color, float3(1.0, 0.96, 0.77), lip * upper * 0.18);
}
if (State > 1.5 && State < 2.5)
{
    // Lower relief uses a flatter, slightly deeper gold with no animated sheen.
    color = lerp(float3(0.89, 0.69, 0.36), float3(0.96, 0.80, 0.49), p.y);
    color = lerp(color, float3(0.56, 0.365, 0.115), edge * 0.85);
    color += champagne * lip * p.y * 0.035;
}
if (State > 2.5)
{
    // A pale, flat sage receipt is visibly settled and distinct from the gold action.
    color = lerp(float3(0.920, 0.947, 0.897), float3(0.866, 0.910, 0.847), p.y);
    color = lerp(color, float3(0.54, 0.67, 0.56), edge * 0.65);
    color += float3(0.92, 0.97, 0.88) * lip * upper * 0.025;
}
return float4(pow(saturate(color), 2.2), alpha);
"""


def existing_owned(name):
    path = FOLDER + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    asset = unreal.load_asset(path)
    if not asset or unreal.EditorAssetLibrary.get_metadata_tag(asset, VERSION_KEY) != VERSION:
        raise RuntimeError("Existing reward material lacks the expected ownership marker: " + path)
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

    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -680, -160)
    clock = lib.create_material_expression(material, unreal.MaterialExpressionTime, -680, -40)
    sources = {"UV": uv, "Clock": clock}
    for index, (name, default) in enumerate(DEFAULTS.items()):
        parameter = lib.create_material_expression(
            material, unreal.MaterialExpressionScalarParameter, -680, 100 + index * 140)
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", default)
        sources[name] = parameter

    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -300, -100)
    custom.set_editor_property("description", "Codex reward: iridescent bevel, slow sheen, readable teal center")
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
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    material = create_base(asset_tools, lib)
    result = {"Base": material}
    for state_name, overrides in STATES.items():
        name = "MI_FF_CodexReward_" + state_name
        instance = existing_owned(name)
        if not instance:
            instance = asset_tools.create_asset(
                name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            assert instance
            lib.set_material_instance_parent(instance, material)
            for parameter, value in {**DEFAULTS, **overrides}.items():
                lib.set_material_instance_scalar_parameter_value(instance, parameter, float(value))
                actual = lib.get_material_instance_scalar_parameter_value(instance, parameter)
                assert abs(actual - float(value)) < 0.0001, parameter
            save_owned(instance)
        result[state_name] = instance
    unreal.log("FF_CODEX_REWARD_IRIDESCENT_MATERIALS_PASS")
    return result


if __name__ == "__main__":
    create_materials()
