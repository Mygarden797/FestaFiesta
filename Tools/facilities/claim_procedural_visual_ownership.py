# -*- coding: utf-8 -*-
# claim_procedural_visual_ownership.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 자동 생성 외형의 수정 권한을 해당 시설 정의에 연결한다.

"""One-time claim of the seven existing generated visuals by their Definitions.

This never infers facility dimensions. It only establishes destructive-tool
ownership after proving the existing mesh source/contract/reference markers.
"""

import os
import re
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from ffzs_asset_save_transaction import AssetSaveTransaction
from ffzs_definition_source import (
    GENERATED_MESH_ROOT,
    read_facility_definitions,
    require_unattended_commandlet,
)


def run():
    require_unattended_commandlet()
    candidates = []
    failures = []
    already_claimed = []
    seen_mesh_packages = set()
    root = GENERATED_MESH_ROOT.rstrip("/")
    definitions = read_facility_definitions(require_visual_mesh=False)
    for record in definitions:
        mesh = record["visual_mesh_object"]
        if not mesh:
            failures.append("Production Definition has no VisualMesh: " + record["definition"])
            continue
        package_path = record["visual_mesh"].split(".", 1)[0]
        if package_path in seen_mesh_packages:
            failures.append("Duplicate production VisualMesh target: " + package_path)
            continue
        seen_mesh_packages.add(package_path)
        if package_path != root and not package_path.startswith(root + "/"):
            failures.append(
                "Production visual is outside the migration's managed root: "
                + package_path
            )
            continue
        contract = str(
            unreal.EditorAssetLibrary.get_metadata_tag(
                mesh, "FFZS.FacilityVisualContract"
            )
            or ""
        )
        generated_source = str(
            unreal.EditorAssetLibrary.get_metadata_tag(mesh, "FFZS.GeneratedSource")
            or ""
        ).replace("\\", "/")
        notion_id = str(
            unreal.EditorAssetLibrary.get_metadata_tag(
                mesh, "FFZS.NotionFacilityDefinitionId"
            )
            or ""
        )
        if notion_id:
            failures.append(
                "Production visual has a conflicting Notion reference marker: "
                + package_path
            )
            continue
        expected_source = "SourceAssets/Zone/FacilityMeshes/{0}.fbx".format(
            record["visual_mesh_name"]
        )
        source_file = os.path.join(
            unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
            *expected_source.split("/"),
        )
        if not os.path.isfile(source_file):
            failures.append(
                "Cannot prove procedural ownership; source FBX is missing: "
                + source_file
            )
            continue
        is_legacy_contract = bool(
            re.fullmatch(
                r"[0-9]+(?:\.[0-9]+)?x[0-9]+(?:\.[0-9]+)?x[0-9]+(?:\.[0-9]+)?cm;PivotGroundCenter;Front\+Y",
                contract,
            )
        )
        expected_prefix = "Definition={0};".format(
            record["facility_definition_id"]
        )
        if generated_source != expected_source or not (
            is_legacy_contract or contract.startswith(expected_prefix)
        ):
            failures.append(
                "Cannot prove procedural ownership for {0}".format(package_path)
            )
            continue
        if record["procedurally_managed_visual_mesh"]:
            already_claimed.append(record)
        else:
            candidates.append(record)

    if failures:
        raise RuntimeError(" | ".join(failures))
    if not candidates:
        if len(already_claimed) != len(definitions):
            raise RuntimeError("Visual ownership migration postcondition was not proven")
        unreal.log("[FF_FACILITY_DEFINITION] No visual ownership claims required")
        return

    transaction = AssetSaveTransaction(
        [record["definition"] for record in candidates],
        "FacilityVisualOwnershipClaim",
    )
    try:
        for record in candidates:
            definition = record["definition_object"]
            definition.set_editor_property(
                "procedurally_managed_visual_mesh", True
            )
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                definition, only_if_is_dirty=False
            ):
                raise RuntimeError("Could not save " + record["definition"])
        transaction.commit()
    except Exception:
        transaction.rollback()
        raise

    unreal.log(
        "[FF_FACILITY_DEFINITION] Claimed {0} procedural VisualMeshes".format(
            len(candidates)
        )
    )


run()
