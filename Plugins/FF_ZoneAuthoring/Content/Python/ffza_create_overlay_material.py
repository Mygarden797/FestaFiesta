# ffza_create_overlay_material.py
# 역할: 건축 모드 바닥 오버레이용 머티리얼(M_FFZA_ZoneOverlay)을 한 번에 만든다.
#
# 사용법(언리얼 에디터):
#   1) 메뉴 Tools > Execute Python Script... 에서 이 파일을 선택하거나,
#   2) Output Log 하단 입력창을 "Python"으로 바꾸고 아래 한 줄을 입력:
#        import ffza_create_overlay_material; ffza_create_overlay_material.run()
#
# 결과: /FF_ZoneAuthoring/Materials/M_FFZA_ZoneOverlay
#   - Blend Mode: Translucent (반투명)
#   - Shading Model: Unlit (조명 영향 없음 → 밤에도 잘 보임)
#   - Two Sided: 꺼짐 (오버레이 메시가 이미 앞/뒷면을 모두 갖고 있어 켜면 두 번 그려져 진해진다)
#   - Vertex Color RGB → Emissive Color, Vertex Color A → Opacity
# 이미 있으면 덮어쓰지 않는다.

import unreal

PACKAGE_PATH = "/FF_ZoneAuthoring/Materials"
ASSET_NAME = "M_FFZA_ZoneOverlay"


def run():
    asset_path = f"{PACKAGE_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"[FFZA] {asset_path} 이(가) 이미 있습니다. 건너뜁니다.")
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(ASSET_NAME, PACKAGE_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        unreal.log_error("[FFZA] 머티리얼 생성에 실패했습니다.")
        return None

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", False)

    mel = unreal.MaterialEditingLibrary
    vertex_color = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, 0)
    mel.connect_material_property(vertex_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(vertex_color, "A", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"[FFZA] {asset_path} 생성 완료. 레벨의 FF Zone Overlay에서 Rebuild Overlay를 누르세요.")
    return material


if __name__ == "__main__":
    run()
