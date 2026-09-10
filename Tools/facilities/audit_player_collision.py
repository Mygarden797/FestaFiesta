"""Read-only UE editor audit of existing facility mesh collision; never changes assets."""

import json
from pathlib import Path

import unreal


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.wait_for_completion()
    definitions = registry.get_assets_by_class(
        unreal.TopLevelAssetPath("/Script/FF_ZoneSystem", "FFZSFacilityDefinition"),
        search_sub_classes=True,
    )
    meshes = {}
    missing = []
    errors = []

    def inspect(mesh, source):
        if not mesh:
            missing.append(source)
            return
        path = mesh.get_path_name()
        if path in meshes:
            meshes[path]["used_by"].append(source)
            return
        body = mesh.get_editor_property("body_setup")
        flag = str(body.get_editor_property("collision_trace_flag")) if body else "NO_BODY"
        # StaticMeshEditorSubsystem is absent in Python commandlets. Read the
        # already-imported BodySetup directly; no cooking or asset mutation needed.
        aggregate = body.get_editor_property("agg_geom") if body else None
        primitives = sum(len(aggregate.get_editor_property(name)) for name in
                         ("sphere_elems", "box_elems", "sphyl_elems")) if aggregate else 0
        convex = len(aggregate.get_editor_property("convex_elems")) if aggregate else 0
        meshes[path] = {
            "mesh": path,
            "primitive_shapes": primitives,
            "convex_shapes": convex,
            "collision_complexity": flag,
            "has_simple_collision": primitives + convex > 0,
            "uses_complex_as_simple": "USE_COMPLEX_AS_SIMPLE" in flag.upper(),
            "used_by": [source],
        }

    for asset in definitions:
        source = str(asset.package_name)
        try:
            definition = asset.get_asset()
            spec = definition.get_editor_property("spec")
            inspect(spec.get_editor_property("visual_mesh"), source + " / base")
            if spec.get_editor_property("uses_facility_levels"):
                for index, tier in enumerate(spec.get_editor_property("upgrade_tiers"), start=2):
                    mesh = tier.get_editor_property("visual_mesh")
                    if mesh:
                        inspect(mesh, source + " / level " + str(index))
        except Exception as exc:
            errors.append({"definition": source, "error": str(exc)})

    rows = sorted(meshes.values(), key=lambda row: row["mesh"])
    report = {
        "read_only": True,
        "definitions": len(definitions),
        "unique_meshes": len(rows),
        "meshes_without_simple_collision": [row["mesh"] for row in rows if not row["has_simple_collision"]],
        "missing_base_meshes": missing,
        "errors": errors,
        "meshes": rows,
    }
    output = Path(unreal.Paths.project_saved_dir()) / "FacilityPlayerCollision" / "asset_audit.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FACILITY_PLAYER_COLLISION_AUDIT " + json.dumps({
        "definitions": len(definitions), "meshes": len(rows),
        "without_simple": len(report["meshes_without_simple_collision"]),
        "missing_base": len(missing), "errors": len(errors), "report": str(output),
    }, ensure_ascii=False))
    if not definitions or errors:
        raise RuntimeError("Facility collision audit incomplete; inspect asset_audit.json")


main()
