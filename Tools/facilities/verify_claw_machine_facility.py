# verify_claw_machine_facility.py
# 역할: 저장된 일반 인형뽑기 부스의 메시·충돌·목록 이미지·시설 연결을 읽기 전용으로 검사한다.
"""Verify the booth in a fresh Unreal Python process after importing it.

    UnrealEditor-Cmd.exe FestaFiesta.uproject -run=pythonscript \
        -script="Tools/facilities/verify_claw_machine_facility.py" \
        -unattended -nop4 -nosplash -nullrhi

The importer supplies shared paths and ownership metadata without running its
import routine. This script never creates, edits, or saves an asset.
"""

from __future__ import annotations

import os
import sys

import unreal


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from import_claw_machine_facility import (
    ASSET_PATHS,
    EXPECTED_MATERIAL_SLOTS,
    FACILITY_ID,
    material_for_slot,
    owned,
)


def require_asset(path):
    """기존 제작 도구가 관리하는 에셋만 검사 대상으로 읽는다."""
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset: " + path)
    if not owned(asset):
        raise RuntimeError("Unexpected ownership for " + path)
    return asset


def verify_links(facility, mesh, thumbnail) -> None:
    """시설 ID·설치비 규칙·외형 연결이 원래 부스 규칙과 같은지 확인한다."""
    if facility.get_class().get_name() != "FFZSFacilityDefinition":
        raise RuntimeError("Facility definition has the wrong class")
    spec = facility.get_editor_property("spec")
    if str(spec.get_editor_property("facility_definition_id")) != FACILITY_ID:
        raise RuntimeError("Unexpected FacilityDefinitionId")
    install_cost_key = str(spec.get_editor_property("install_cost_key"))
    if install_cost_key not in ("", "None"):
        raise RuntimeError("InstallCostKey must stay empty for convention pricing")
    for property_name in ("revenue_rule_key", "experience_effect_key"):
        if str(spec.get_editor_property(property_name)) != FACILITY_ID:
            raise RuntimeError("Facility rule key mismatch: " + property_name)
    linked_mesh = spec.get_editor_property("visual_mesh")
    linked_thumbnail = facility.get_editor_property("catalog_thumbnail")
    if not linked_mesh or linked_mesh.get_path_name() != mesh.get_path_name():
        raise RuntimeError("Facility VisualMesh link failed")
    if not linked_thumbnail or linked_thumbnail.get_path_name() != thumbnail.get_path_name():
        raise RuntimeError("Facility CatalogThumbnail link failed")


def run() -> None:
    """부스 에셋 3개만 읽고 공용 재질 9개·충돌·썸네일·목록 등록을 검사한다."""
    mesh, thumbnail, facility = [require_asset(path) for path in ASSET_PATHS]
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Facility visual is not a StaticMesh")
    slots = mesh.get_editor_property("static_materials")
    names = [str(slot.get_editor_property("material_slot_name")) for slot in slots]
    if len(names) != len(EXPECTED_MATERIAL_SLOTS) or set(names) != EXPECTED_MATERIAL_SLOTS:
        raise RuntimeError("Facility mesh must keep its 9 shared material slots")
    for slot, slot_name in zip(slots, names):
        material = slot.get_editor_property("material_interface")
        expected = material_for_slot(slot_name)
        if material is None or material.get_path_name() != expected.get_path_name():
            raise RuntimeError("Shared material assignment mismatch: " + slot_name)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("StaticMeshEditorSubsystem is unavailable")
    if subsystem.get_simple_collision_count(mesh) <= 0:
        raise RuntimeError("Facility mesh has no simple collision")
    if not isinstance(thumbnail, unreal.Texture2D) or int(thumbnail.blueprint_get_size_x()) < 512:
        raise RuntimeError("Catalog thumbnail contract failed")
    verify_links(facility, mesh, thumbnail)

    # UE Python에 PrimaryAssetId 조회가 없으므로 원본 클래스와 ID를 위에서 확인한다.
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    if not registry.get_asset_by_object_path(facility.get_path_name()).is_valid():
        raise RuntimeError("Facility is missing from AssetRegistry")
    unreal.log(
        "FF_CLAW_MACHINE_FACILITY_VERIFY_OK primary=FFZSFacilityDefinition:{0} "
        "mesh_materials=9 thumbnail={1}x{2}".format(
            FACILITY_ID,
            int(thumbnail.blueprint_get_size_x()),
            int(thumbnail.blueprint_get_size_y()),
        )
    )


if __name__ == "__main__":
    run()
