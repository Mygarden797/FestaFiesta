# -*- coding: utf-8 -*-
"""Export a disposable Blender/tool manifest from facility Data Assets.

Run with UnrealEditor-Cmd before the offline Blender builder.  Never hand-edit
the result; regenerate it whenever a Definition changes.
"""

import json
import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)
from ffzs_definition_source import (
    DEFINITION_ROOT,
    build_authoring_projection,
    calculate_projection_digest,
    read_facility_definitions,
    select_managed_visual_definitions,
)


REPORT_NAME = "FacilityDefinitionAuthoringManifest.json"


def run():
    entries = []
    definitions = read_facility_definitions(require_visual_mesh=False)
    for record in select_managed_visual_definitions(definitions):
        projection = build_authoring_projection(record)
        projection["projection_digest"] = calculate_projection_digest(projection)
        entries.append(projection)

    manifest_digest = calculate_projection_digest(entries)

    result = {
        "schema_version": 2,
        "generated_from": DEFINITION_ROOT,
        "authoritative": False,
        "projection_digest": manifest_digest,
        "entries": entries,
    }
    output_path = os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
        REPORT_NAME,
    )
    with open(output_path, "w", encoding="utf-8") as handle:
        json.dump(result, handle, ensure_ascii=False, indent=2)
    unreal.log(
        "[FF_FACILITY_DEFINITION] Exported {0} definitions to {1}".format(
            len(entries), output_path
        )
    )


run()
