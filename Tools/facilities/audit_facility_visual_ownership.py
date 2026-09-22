# -*- coding: utf-8 -*-
# audit_facility_visual_ownership.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 기존 데이터를 읽어 상태·문제를 점검한다.

"""Read-only report for facility Definition visual ownership metadata."""

import json
import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from ffzs_definition_source import read_facility_definitions


def run():
    entries = []
    for record in read_facility_definitions(require_visual_mesh=False):
        mesh = record["visual_mesh_object"]
        entries.append(
            {
                "facility_definition_id": record["facility_definition_id"],
                "definition": record["definition"],
                "visual_mesh": record["visual_mesh"],
                "visual_sizing_mode": record["visual_sizing_mode"],
                "procedurally_managed_visual_mesh": record[
                    "procedurally_managed_visual_mesh"
                ],
                "facility_visual_contract": str(
                    unreal.EditorAssetLibrary.get_metadata_tag(
                        mesh, "FFZS.FacilityVisualContract"
                    )
                    or ""
                )
                if mesh
                else "",
                "generated_source": str(
                    unreal.EditorAssetLibrary.get_metadata_tag(
                        mesh, "FFZS.GeneratedSource"
                    )
                    or ""
                )
                if mesh
                else "",
                "notion_definition_id": str(
                    unreal.EditorAssetLibrary.get_metadata_tag(
                        mesh, "FFZS.NotionFacilityDefinitionId"
                    )
                    or ""
                )
                if mesh
                else "",
            }
        )

    output_path = os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
        "FacilityVisualOwnershipAudit.json",
    )
    with open(output_path, "w", encoding="utf-8") as handle:
        json.dump({"entries": entries}, handle, ensure_ascii=False, indent=2)
    unreal.log(
        "[FF_FACILITY_DEFINITION] Wrote visual ownership audit to " + output_path
    )


run()
