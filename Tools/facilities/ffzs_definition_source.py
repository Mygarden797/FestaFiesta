# -*- coding: utf-8 -*-
"""Read facility authoring inputs from UFFZSFacilityDefinition assets.

This module is intentionally usable only inside Unreal Python.  It contains no
facility IDs, mesh names, or dimensions: the Data Assets are the source and all
tool reports/manifests are disposable projections of that source.
"""

import unreal
import hashlib
import json
import os
import re
import shlex


DEFINITION_ROOT = "/Game/FestaFiesta/Building/FF_Data_Building"
DEFINITION_CLASS_NAME = "FFZSFacilityDefinition"
GENERATED_MESH_ROOT = "/Game/FestaFiesta/Zone/Test_Asset/Asset"


def build_authoring_projection(record):
    """Return the canonical, JSON-safe geometry projection consumed by Blender."""
    return {
        "facility_definition_id": record["facility_definition_id"],
        "definition": record["definition"],
        "visual_mesh": record["visual_mesh"],
        "visual_mesh_name": record["visual_mesh_name"],
        "target_size_cm": record["target_size_cm"],
        "visual_sizing_mode": record["visual_sizing_mode"],
    }


def calculate_projection_digest(projection):
    encoded = json.dumps(
        projection, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def _asset_package_path(asset):
    return asset.get_path_name().split(".", 1)[0]


def _enum_name(value):
    reflected_name = getattr(value, "name", None)
    if reflected_name:
        return str(reflected_name)
    text = str(value)
    text = text.rsplit(".", 1)[-1]
    return text.split(":", 1)[0].strip("<> ")


def read_facility_definitions(require_visual_mesh=True):
    """Return deterministic loaded Definition records and fail on ambiguous data."""
    records = []
    failures = []
    seen_ids = set()

    asset_paths = unreal.EditorAssetLibrary.list_assets(
        DEFINITION_ROOT, recursive=True, include_folder=False
    )
    for asset_path in sorted(asset_paths):
        definition = unreal.EditorAssetLibrary.load_asset(asset_path)
        if definition is None:
            failures.append("Failed to load asset under Definition root: " + asset_path)
            continue
        if definition.get_class().get_name() != DEFINITION_CLASS_NAME:
            continue

        spec = definition.get_editor_property("spec")
        definition_id = str(spec.get_editor_property("facility_definition_id"))
        if not definition_id or definition_id == "None":
            failures.append("Missing FacilityDefinitionId: " + asset_path)
            continue
        if definition_id in seen_ids:
            failures.append("Duplicate FacilityDefinitionId: " + definition_id)
            continue
        seen_ids.add(definition_id)

        visual_mesh = spec.get_editor_property("visual_mesh")
        if visual_mesh is None:
            if require_visual_mesh:
                failures.append("Missing VisualMesh: " + asset_path)

        footprint = spec.get_editor_property("footprint_size_cells")
        height_cm = float(spec.get_editor_property("placement_obstacle_height_cm"))
        records.append(
            {
                "definition_object": definition,
                "spec": spec,
                "definition": _asset_package_path(definition),
                "facility_definition_id": definition_id,
                "visual_mesh_object": visual_mesh,
                "visual_mesh": visual_mesh.get_path_name() if visual_mesh else "",
                "visual_mesh_name": visual_mesh.get_name() if visual_mesh else "",
                "target_size_cm": {
                    "x": int(footprint.x),
                    "y": int(footprint.y),
                    "z": height_cm,
                },
                "visual_sizing_mode": _enum_name(
                    spec.get_editor_property("visual_sizing_mode")
                ),
                "procedurally_managed_visual_mesh": bool(
                    definition.get_editor_property(
                        "procedurally_managed_visual_mesh"
                    )
                ),
            }
        )

    if failures:
        raise RuntimeError("; ".join(failures))
    if not records:
        raise RuntimeError("No UFFZSFacilityDefinition assets found under " + DEFINITION_ROOT)
    return records


def select_managed_visual_definitions(records, generated_root=GENERATED_MESH_ROOT):
    """Select only meshes explicitly owned by the Definition-driven generator.

    Path membership alone is insufficient because reference/artist meshes share the legacy root.
    The Definition's explicit procedurally_managed_visual_mesh flag grants write ownership;
    GeneratedSource and FacilityVisualContract only prove matching provenance. Notion/reference
    meshes carry a separate marker and are never writable by this pipeline. New contracts also
    bind the mesh to the exact Definition; the pre-SSOT contract is accepted only when its exact
    generated FBX provenance is intact.
    """
    root = generated_root.rstrip("/")
    managed = []
    seen_packages = set()
    failures = []
    for record in records:
        mesh_path = record["visual_mesh"]
        if not mesh_path:
            if record["procedurally_managed_visual_mesh"]:
                failures.append(
                    "Procedurally managed Definition has no VisualMesh: "
                    + record["definition"]
                )
            continue
        package_path = mesh_path.split(".", 1)[0]
        is_in_shared_root = package_path == root or package_path.startswith(root + "/")
        mesh = record["visual_mesh_object"]
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
        notion_definition_id = str(
            unreal.EditorAssetLibrary.get_metadata_tag(
                mesh, "FFZS.NotionFacilityDefinitionId"
            )
            or ""
        )
        has_procedural_residue = bool(generated_source) or contract.startswith("Definition=")
        if not is_in_shared_root:
            if record["procedurally_managed_visual_mesh"] or has_procedural_residue:
                failures.append(
                    "Procedurally owned or marked VisualMesh is outside the managed root: "
                    + package_path
                )
            continue
        reference_contract = (
            "ReferenceArtOnly;PivotGroundCenter;Front+Y;NoRuntimePlacementAuthority"
        )
        legacy_reference_contract = bool(
            re.fullmatch(
                r"[0-9]+(?:\.[0-9]+)?x[0-9]+(?:\.[0-9]+)?x[0-9]+(?:\.[0-9]+)?cm;PivotGroundCenter;Front\+Y",
                contract,
            )
        )
        expected_reference_source = "SourceAssets/Zone/FacilityMeshes/{0}.fbx".format(
            record["visual_mesh_name"]
        )
        is_coherent_reference = (
            bool(notion_definition_id)
            and (contract == reference_contract or legacy_reference_contract)
            and generated_source == expected_reference_source
        )
        has_procedural_contract = contract.startswith("Definition=")
        has_any_reference_marker = bool(notion_definition_id) or contract == reference_contract
        if not record["procedurally_managed_visual_mesh"]:
            if is_coherent_reference:
                continue
            if has_procedural_contract and has_any_reference_marker:
                failures.append(
                    "Conflicting reference/generated ownership markers while Definition ownership is disabled: {0}".format(
                        package_path
                    )
                )
            elif has_procedural_contract or generated_source:
                failures.append(
                    "Generated VisualMesh provenance exists but Definition ownership is disabled: {0}".format(
                        package_path
                    )
                )
            elif has_any_reference_marker or legacy_reference_contract:
                failures.append(
                    "Incomplete reference VisualMesh ownership markers: {0}".format(
                        package_path
                    )
                )
            continue
        if notion_definition_id or contract == reference_contract:
            failures.append(
                "Conflicting reference/generated ownership markers: {0}".format(
                    package_path
                )
            )
            continue

        has_owned_residue = bool(generated_source) or has_procedural_contract
        if not has_owned_residue:
            failures.append(
                "Managed Definition points to an unowned VisualMesh: {0}".format(
                    package_path
                )
            )
            continue

        expected_source = "SourceAssets/Zone/FacilityMeshes/{0}.fbx".format(
            record["visual_mesh_name"]
        )
        expected_prefix = "Definition={0};".format(
            record["facility_definition_id"]
        )
        is_definition_contract = contract.startswith("Definition=")
        is_legacy_contract = legacy_reference_contract
        if generated_source != expected_source:
            failures.append(
                "Managed VisualMesh source marker mismatch: {0} (actual={1}, expected={2})".format(
                    package_path, generated_source or "<missing>", expected_source
                )
            )
            continue
        source_file = os.path.join(
            unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
            *expected_source.split("/"),
        )
        if not os.path.isfile(source_file):
            failures.append("Managed VisualMesh source FBX is missing: " + source_file)
            continue
        if is_definition_contract and not contract.startswith(expected_prefix):
            failures.append(
                "Managed VisualMesh Definition owner mismatch: {0} (actual={1}, expected={2}*)".format(
                    package_path, contract, expected_prefix
                )
            )
            continue
        if not is_definition_contract and not is_legacy_contract:
            failures.append(
                "Managed VisualMesh contract mismatch: {0} (actual={1})".format(
                    package_path, contract or "<missing>"
                )
            )
            continue
        if package_path in seen_packages:
            failures.append("Duplicate managed VisualMesh target: " + package_path)
            continue
        seen_packages.add(package_path)
        managed.append(record)
    if failures:
        raise RuntimeError("; ".join(failures))
    if not managed:
        raise RuntimeError("No managed VisualMesh definitions found under " + root)
    return managed


def require_unattended_commandlet():
    """Mutation tools must terminate after rollback; never allow them in an interactive Editor."""
    command_line = str(unreal.SystemLibrary.get_command_line())
    try:
        arguments = {argument.casefold() for argument in shlex.split(command_line, posix=False)}
    except ValueError as error:
        raise RuntimeError("Could not parse Unreal command line safely: " + str(error))
    if "-run=pythonscript" not in arguments or "-unattended" not in arguments:
        raise RuntimeError(
            "This mutating tool must run via UnrealEditor-Cmd -run=pythonscript -unattended"
        )
