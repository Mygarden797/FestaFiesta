"""Author the codex's alpha-preserving grayscale/silhouette UI shader.

Run before the festival codex WBP migration. The original catalog art is never
rewritten: each visible image supplies its source texture and discovery state.
Existing owned assets are reused without saving, preserving repeat-run hashes.
"""
import unreal

FOLDER = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials"
NAME = "M_FF_Codex_DiscoveryThumbnail"
VERSION_KEY = "FF.FacilityCodex.ThumbnailDiscoveryVersion"
VERSION = "1"

SHADER = r"""
float4 source = Texture2DSample(ThumbnailTexture, ThumbnailTextureSampler, UV);
float3 color = source.rgb;
if (VisualState > 0.5 && VisualState < 1.5)
{
    // Work in linear space. Equal channels are genuine grayscale, not a dim tint.
    float luminance = dot(source.rgb, float3(0.2126, 0.7152, 0.0722));
    color = luminance.xxx;
}
if (VisualState > 1.5)
{
    // A consistent teal silhouette with a one-screen-pixel mint inner contour.
    // Sampling alpha only prevents the hidden artwork's colors leaking through.
    float2 delta = max(TexelSize.xy, fwidth(UV)) * 0.85;
    float neighbours = Texture2DSample(ThumbnailTexture, ThumbnailTextureSampler, UV + float2(delta.x, 0)).a;
    neighbours = min(neighbours, Texture2DSample(ThumbnailTexture, ThumbnailTextureSampler, UV - float2(delta.x, 0)).a);
    neighbours = min(neighbours, Texture2DSample(ThumbnailTexture, ThumbnailTextureSampler, UV + float2(0, delta.y)).a);
    neighbours = min(neighbours, Texture2DSample(ThumbnailTexture, ThumbnailTextureSampler, UV - float2(0, delta.y)).a);
    float innerEdge = saturate(source.a - neighbours);
    float3 body = pow(float3(0.135, 0.315, 0.290), 2.2);
    float3 mint = pow(float3(0.52, 0.75, 0.65), 2.2);
    color = lerp(body, mint, innerEdge * 0.85);
}
// Keep the imported subject mask in every state. Never fill its transparent canvas.
return float4(color, source.a);
"""


def create_materials():
    path = FOLDER + "/" + NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        if unreal.EditorAssetLibrary.get_metadata_tag(material, VERSION_KEY) != VERSION:
            raise RuntimeError("Refusing to replace an unowned discovery material: " + path)
        unreal.log("FF_CODEX_THUMBNAIL_MATERIALS_PASS")
        return material

    unreal.EditorAssetLibrary.make_directory(FOLDER)
    lib = unreal.MaterialEditingLibrary
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    assert material
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -650, -200)
    texture = lib.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -650, -80)
    texture.set_editor_property("parameter_name", "ThumbnailTexture")
    texture.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    state = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -650, 80)
    state.set_editor_property("parameter_name", "VisualState")
    state.set_editor_property("default_value", 0.0)
    texel = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -650, 220)
    texel.set_editor_property("parameter_name", "TexelSize")
    texel.set_editor_property("default_value", unreal.LinearColor(1.0 / 1024, 1.0 / 1024, 0.0, 0.0))
    sources = {"UV": uv, "ThumbnailTexture": texture, "VisualState": state, "TexelSize": texel}

    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -300, -120)
    custom.set_editor_property("description", "Discovery states: color / grayscale / teal silhouette, source alpha retained")
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
    unreal.EditorAssetLibrary.set_metadata_tag(material, VERSION_KEY, VERSION)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log("FF_CODEX_THUMBNAIL_MATERIALS_PASS")
    return material


if __name__ == "__main__":
    create_materials()
