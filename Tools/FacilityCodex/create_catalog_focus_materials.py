# create_catalog_focus_materials.py
# 역할: 시설 조합 도감과 보상 표시 작업에서 필요한 에셋 또는 설정을 생성한다.

"""Create a mint/champagne/lilac UI interference rim for the codex destination.

Only the destination card uses these materials. Its center remains transparent,
so thumbnails, availability, prices and the player's normal selection keep their
authored appearance. Run before FF.ApplyCodexCatalogFocus in the idle Entry map.
"""
import unreal

FOLDER = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials"
VERSION_KEY = "FF.FacilityCodex.CatalogFocusMaterialVersion"
VERSION = "1"
DEFAULTS = {"Aspect": 170.0 / 204.0, "Badge": 0.0}
STATES = {"Rim": {"Aspect": 170.0 / 204.0, "Badge": 0.0},
          "Badge": {"Aspect": 5.4, "Badge": 1.0}}
SHADER = r"""
float2 uv = saturate(UV);
float aspect = max(Aspect, 0.5);
float2 halfSize = float2(aspect, 1.0) * 0.5;
float radius = Badge > 0.5 ? 0.21 : 0.044;
float2 q = abs((uv - 0.5) * float2(aspect, 1.0)) - (halfSize - radius);
float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
float aa = max(fwidth(distance), 0.0007);
float silhouette = 1.0 - smoothstep(-aa, aa, distance);
float inward = -distance;
float phase = Clock * 0.24;
float a = 0.5 + 0.5 * sin(uv.x * 6.2 + uv.y * 4.6 + phase);
float b = 0.5 + 0.5 * sin(uv.x * 5.5 - uv.y * 6.8 + phase + 1.9);
float3 mint = float3(0.46, 0.94, 0.78);
float3 gold = float3(1.0, 0.86, 0.48);
float3 lilac = float3(0.78, 0.68, 0.96);
float3 film = lerp(gold, mint, a * 0.85);
film = lerp(film, lilac, b * 0.46);
float3 teal = float3(0.035, 0.23, 0.19);
float width = Badge > 0.5 ? 0.055 : 0.020;
float rim = 1.0 - smoothstep(width - aa, width + aa, inward);
float outer = 1.0 - smoothstep(width * 0.27 - aa, width * 0.27 + aa, inward);
float3 color = lerp(film, teal, outer * 0.82);
float innerLip = exp(-pow((inward - width * 0.63) / max(width * 0.19, 0.001), 2.0));
color += innerLip * float3(0.12, 0.15, 0.11);
float alpha = silhouette * rim;
if (Badge > 0.5)
{
    float3 center = lerp(float3(0.08, 0.32, 0.27), float3(0.035, 0.20, 0.17), uv.y);
    color = lerp(center, color, rim);
    alpha = silhouette;
}
return float4(pow(saturate(color), 2.2), alpha);
"""


def existing_owned(name):
    path = FOLDER + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    asset = unreal.load_asset(path)
    assert asset and unreal.EditorAssetLibrary.get_metadata_tag(asset, VERSION_KEY) == VERSION, path
    return asset


def save_owned(asset):
    unreal.EditorAssetLibrary.set_metadata_tag(asset, VERSION_KEY, VERSION)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def create_materials():
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    name = "M_FF_CodexCatalogFocus"
    material = existing_owned(name)
    if not material:
        material = asset_tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        assert material
        material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        sources = {
            "UV": lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -600, -160),
            "Clock": lib.create_material_expression(material, unreal.MaterialExpressionTime, -600, -40),
        }
        for index, (key, default) in enumerate(DEFAULTS.items()):
            node = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 100 + index * 140)
            node.set_editor_property("parameter_name", key)
            node.set_editor_property("default_value", default)
            sources[key] = node
        custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -250, -100)
        custom.set_editor_property("description", "Codex destination: transparent center and iridescent festival rim")
        custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        inputs = []
        for key in sources:
            pin = unreal.CustomInput()
            pin.set_editor_property("input_name", key)
            inputs.append(pin)
        custom.set_editor_property("inputs", inputs)
        custom.set_editor_property("code", SHADER)
        for key, source in sources.items():
            assert lib.connect_material_expressions(source, "", custom, key)
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
    result = {"Base": material}
    for state, overrides in STATES.items():
        name = "MI_FF_CodexCatalogFocus_" + state
        instance = existing_owned(name)
        if not instance:
            instance = asset_tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            assert instance
            lib.set_material_instance_parent(instance, material)
            for parameter, value in overrides.items():
                lib.set_material_instance_scalar_parameter_value(instance, parameter, float(value))
            save_owned(instance)
        result[state] = instance
    unreal.log("FF_CODEX_CATALOG_FOCUS_MATERIALS_PASS")
    return result


if __name__ == "__main__":
    create_materials()
