# import_claw_machine_facility.py
# 역할: 일반 인형뽑기 부스의 메시·목록 이미지·시설 정의 3개를 가져와 연결한다.
"""Import the ordinary claw-machine booth's building assets.

Run the Blender builder first, then with the Editor closed::

    UnrealEditor-Cmd.exe FestaFiesta.uproject -run=pythonscript \
        -script="Tools/facilities/import_claw_machine_facility.py" \
        -unattended -nop4 -nosplash -nullrhi

When creating the facility definition for the first time, the ordinary Surfing
booth template must already exist (Tools/facilities/import_surfing_test_asset.py).
Its FBX is produced by the corresponding Blender builder. Preflight rejects
missing source/template files before any import or save.

The existing ownership tag is preserved for safe reimport. Shared materials and
the facility template are read only. Only the three ASSET_PATHS are saved.
Verify their serialized state with verify_claw_machine_facility.py afterwards.
"""

from __future__ import annotations

import os
import sys

import unreal


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from ffzs_asset_save_transaction import AssetSaveTransaction
from ffzs_definition_source import require_unattended_commandlet


# 기존 에셋의 소유 표시와 경로를 유지해야 재실행 시 같은 부스를 갱신한다.
OWNER_TAG = "FF.ClawMachine.Owner"
OWNER_VALUE = "FestaFiesta.ClawMachine.v1"
FACILITY_ID = "Booth_ActivityGame_ClawMachine"
MESH_NAME = "SM_FFZS_" + FACILITY_ID
FACILITY_TEXTURE_NAME = "T_UI_BuildCatalog_" + FACILITY_ID
FACILITY_DEFINITION_NAME = "PDA_FFZS_" + FACILITY_ID
MESH_DESTINATION = "/Game/FestaFiesta/Zone/Test_Asset/Asset"
FACILITY_TEXTURE_DESTINATION = "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Images"
FACILITY_DEFINITION_DESTINATION = "/Game/FestaFiesta/Building/FF_Data_Building/Booth"
MESH_PATH = MESH_DESTINATION + "/" + MESH_NAME
FACILITY_TEXTURE_PATH = FACILITY_TEXTURE_DESTINATION + "/" + FACILITY_TEXTURE_NAME
FACILITY_DEFINITION_PATH = FACILITY_DEFINITION_DESTINATION + "/" + FACILITY_DEFINITION_NAME
FACILITY_TEMPLATE_PATH = FACILITY_DEFINITION_DESTINATION + "/PDA_FFZS_Booth_ActivityGame_Surfing"
ASSET_PATHS = (MESH_PATH, FACILITY_TEXTURE_PATH, FACILITY_DEFINITION_PATH)
EXPECTED_MATERIAL_SLOTS = frozenset((
    "FF_WoodDark", "FF_WoodWarm", "FF_Teal", "FF_TealDark", "FF_SunsetGlow",
    "FF_BlueGrey", "FF_Cream", "FF_Metal", "FF_AmberGlow",
))
SCRIPT_SOURCE = "Tools/facilities/import_claw_machine_facility.py"


def project_dir() -> str:
    """현재 Unreal 프로젝트의 절대 경로를 반환한다."""
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def source_path(relative: str) -> str:
    """공유 저장소 기준 원본 경로를 현재 컴퓨터 경로로 바꾼다."""
    return os.path.join(project_dir(), relative.replace("/", os.sep))


def owned(asset) -> bool:
    """이 제작 도구가 관리하는 에셋인지 기존 메타데이터로 확인한다."""
    return bool(asset) and str(
        unreal.EditorAssetLibrary.get_metadata_tag(asset, OWNER_TAG) or ""
    ) == OWNER_VALUE


def require_replaceable(asset_path: str):
    """없는 에셋은 만들 수 있지만, 읽기 실패나 타인 에셋은 즉시 중단한다."""
    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return None
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        raise RuntimeError("Could not load existing asset: " + asset_path)
    if not owned(asset):
        raise RuntimeError("Refusing to overwrite unowned asset: " + asset_path)
    return asset


def tag_owned(asset, generated_source: str) -> None:
    """다음 재임포트에서 소유 여부와 원본 파일을 확인할 수 있게 기록한다."""
    unreal.EditorAssetLibrary.set_metadata_tag(asset, OWNER_TAG, OWNER_VALUE)
    unreal.EditorAssetLibrary.set_metadata_tag(
        asset, "FF.ClawMachine.GeneratedSource", generated_source.replace("\\", "/")
    )


def try_set(target, property_name: str, value) -> bool:
    """엔진 버전에 따른 선택 속성의 차이는 경고로 남긴다."""
    try:
        target.set_editor_property(property_name, value)
        return True
    except Exception as error:
        unreal.log_warning(
            "[FF_CLAW_FACILITY_IMPORT] optional property {0} not set: {1}".format(
                property_name, error
            )
        )
        return False


def material_for_slot(slot_name: str):
    """Blender의 공용 재질 이름을 기존 시설 재질 에셋에 대응시킨다."""
    if slot_name not in EXPECTED_MATERIAL_SLOTS:
        raise RuntimeError("Unexpected material slot: " + slot_name)
    path = MESH_DESTINATION + "/Materials/M_FFZS_" + slot_name[3:]
    material = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(material, unreal.MaterialInterface):
        raise RuntimeError("Missing shared facility material: " + path)
    return material


def import_texture(source_file: str):
    """건설 목록용 이미지를 기존 UI 텍스처 설정으로 가져온다."""
    existing = require_replaceable(FACILITY_TEXTURE_PATH)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_file)
    task.set_editor_property("destination_path", FACILITY_TEXTURE_DESTINATION)
    task.set_editor_property("destination_name", FACILITY_TEXTURE_NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", bool(existing))
    try_set(task, "replace_existing_settings", False)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.TextureFactory())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(FACILITY_TEXTURE_PATH)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Texture import failed: " + FACILITY_TEXTURE_PATH)
    for name, value in (
        ("srgb", True), ("never_stream", True),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("max_texture_size", 512),
    ):
        texture.set_editor_property(name, value)
    tag_owned(texture, os.path.relpath(source_file, project_dir()))
    return texture


def make_fbx_options():
    """고정 메시와 원본 충돌 모양만 가져오며 새 재질은 만들지 않는다."""
    options = unreal.FbxImportUI()
    for name, value in (
        ("automated_import_should_detect_type", False), ("import_mesh", True),
        ("import_as_skeletal", False), ("import_materials", False),
        ("import_textures", False),
        ("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH),
    ):
        options.set_editor_property(name, value)
    data = options.get_editor_property("static_mesh_import_data")
    for name, value in (
        ("combine_meshes", True), ("generate_lightmap_u_vs", True),
        ("auto_generate_collision", False), ("convert_scene", True),
        ("convert_scene_unit", True), ("remove_degenerates", True),
    ):
        data.set_editor_property(name, value)
    try_set(data, "build_nanite", False)
    return options


def import_mesh(source_file: str):
    """부스 FBX에 공용 재질과 충돌을 연결하고 원본 정보를 기록한다."""
    existing = require_replaceable(MESH_PATH)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_file)
    task.set_editor_property("destination_path", MESH_DESTINATION)
    task.set_editor_property("destination_name", MESH_NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", bool(existing))
    try_set(task, "replace_existing_settings", False)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.FbxFactory())
    task.set_editor_property("options", make_fbx_options())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("StaticMesh import failed: " + MESH_PATH)
    slots = mesh.get_editor_property("static_materials")
    names = [str(slot.get_editor_property("material_slot_name")) for slot in slots]
    if len(names) != len(EXPECTED_MATERIAL_SLOTS) or set(names) != EXPECTED_MATERIAL_SLOTS:
        raise RuntimeError("Facility mesh must keep its 9 shared material slots")
    for index, slot_name in enumerate(names):
        mesh.set_material(index, material_for_slot(slot_name))

    # 원본 UCX 충돌을 우선 사용하고, 가져오지 못한 경우에만 기존 박스 대체 규칙을 쓴다.
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("StaticMeshEditorSubsystem is unavailable")
    if subsystem.get_simple_collision_count(mesh) <= 0:
        subsystem.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    if subsystem.get_simple_collision_count(mesh) <= 0:
        raise RuntimeError("Facility mesh has no simple collision after fallback")
    try_set(mesh, "light_map_resolution", 64)
    try_set(mesh, "allow_cpu_access", False)
    tag_owned(mesh, os.path.relpath(source_file, project_dir()))
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh, "FFZS.FacilityVisualContract",
        "ReferenceArt;PivotGroundCenter;Front+Y;UCXCollision;DefinitionOwnsBounds",
    )
    return mesh


def create_facility_definition(mesh, thumbnail):
    """기존 부스 정의를 갱신하거나 일반 시설 템플릿에서 만들고 외형을 연결한다."""
    definition = require_replaceable(FACILITY_DEFINITION_PATH)
    if definition is None:
        if not unreal.EditorAssetLibrary.duplicate_asset(
            FACILITY_TEMPLATE_PATH, FACILITY_DEFINITION_PATH
        ):
            raise RuntimeError("Could not duplicate facility definition template")
        definition = unreal.EditorAssetLibrary.load_asset(FACILITY_DEFINITION_PATH)
    if definition is None or definition.get_class().get_name() != "FFZSFacilityDefinition":
        raise RuntimeError("Invalid claw-machine facility definition")
    spec = definition.get_editor_property("spec")
    spec.set_editor_property("facility_definition_id", unreal.Name(FACILITY_ID))
    spec.set_editor_property("display_name", unreal.Text("바다 친구 인형뽑기"))
    spec.set_editor_property("visual_mesh", mesh)
    # 빈 설치비 키는 기존 Cost.Booth.ActivityGame.ClawMachine 규칙을 사용한다.
    spec.set_editor_property("install_cost_key", unreal.Name("None"))
    spec.set_editor_property("revenue_rule_key", unreal.Name(FACILITY_ID))
    spec.set_editor_property("experience_effect_key", unreal.Name(FACILITY_ID))
    definition.set_editor_property("spec", spec)
    definition.set_editor_property("catalog_thumbnail", thumbnail)
    tag_owned(definition, SCRIPT_SOURCE)
    return definition


def preflight(mesh_source: str, texture_source: str) -> None:
    """원본·대상 소유권·공용 재질을 모두 확인한 뒤에만 변경을 허용한다."""
    for path in (mesh_source, texture_source):
        if not os.path.isfile(path):
            raise RuntimeError("Missing source file: " + path)
    existing = [require_replaceable(path) for path in ASSET_PATHS]
    if existing[0] is not None and not isinstance(existing[0], unreal.StaticMesh):
        raise RuntimeError("Existing facility mesh has the wrong class")
    if existing[1] is not None and not isinstance(existing[1], unreal.Texture2D):
        raise RuntimeError("Existing catalog thumbnail has the wrong class")
    definition = existing[2]
    if definition is None:
        definition = unreal.EditorAssetLibrary.load_asset(FACILITY_TEMPLATE_PATH)
    if definition is None or definition.get_class().get_name() != "FFZSFacilityDefinition":
        raise RuntimeError("Missing or invalid facility definition/template: " + FACILITY_TEMPLATE_PATH)
    for slot_name in sorted(EXPECTED_MATERIAL_SLOTS):
        material_for_slot(slot_name)


def run() -> None:
    """에디터가 닫힌 명령 실행에서 부스 에셋 3개만 복구 가능한 방식으로 저장한다."""
    require_unattended_commandlet()
    mesh_source = source_path("SourceAssets/Zone/FacilityMeshes/" + MESH_NAME + ".fbx")
    texture_source = source_path(
        "Content/FestaFiesta/Building/FF_WBP_Building/Catalog/Images/"
        + FACILITY_TEXTURE_NAME + ".png"
    )
    preflight(mesh_source, texture_source)
    # 무인 실행에서 Slate UI를 요구하지 않는 기존 FBX 가져오기를 사용한다.
    for command in ("Interchange.FeatureFlags.Import.FBX 0", "Interchange.FeatureFlags.Import.FBX.ToLevel 0"):
        unreal.SystemLibrary.execute_console_command(None, command)
    transaction = AssetSaveTransaction(ASSET_PATHS, "ClawMachineFacilityImport")
    try:
        mesh = import_mesh(mesh_source)
        thumbnail = import_texture(texture_source)
        facility = create_facility_definition(mesh, thumbnail)
        for asset in (mesh, thumbnail, facility):
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError("Failed to save " + asset.get_path_name())
        transaction.commit()
    except Exception:
        transaction.rollback()
        raise
    unreal.log("FF_CLAW_MACHINE_FACILITY_IMPORT_OK facility=" + facility.get_path_name())


if __name__ == "__main__":
    run()
