# verify_assets.py
# 역할: 건설 에셋 폴더 이관 작업에서 저장된 결과가 요구 조건에 맞는지 검증한다.

"""Read-only Unreal verification for the building asset relocation manifest.

Run this script in a fresh, headless editor before and after relocation. It
never saves, renames or fixes an asset. Only JSON reports under Saved are written.
Mode is inferred from the manifest destinations, or can be explicitly selected
with -FFBuildingRelocationMode=before / -FFBuildingRelocationMode=after.
"""
import hashlib
import json
import re
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/BuildingAssetRelocation_20260921"
MANIFEST_FILE = OUT / "manifest.json"
BEFORE_FILE = OUT / "before_snapshot.json"
EXPECTED_ASSETS = 255
EXPECTED_DEFINITIONS = 114
DEFINITION_CLASS = "FFZSFacilityDefinition"
TESTS = [
    "FestaFiesta.Data.FacilityDefinitions.ActiveCatalogIdentityAndPrices",
    "FestaFiesta.Content.FacilityDefinitions.AllDefinitionsAreProjectable",
    "FestaFiesta.Building.Catalog.ProductionLightingAssetsAndEconomy",
    "FestaFiesta.Building.Catalog.ProductionRoadAssetsAndEconomy",
    "FF_ZoneSystem.Editor.FacilityDefinitionCrossAssetValidation",
    "FestaFiesta.FacilityCodex.UI.CatalogHeader",
]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest_file(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_report(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def object_path(value):
    if value is None:
        return ""
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    if hasattr(value, "to_string"):
        return value.to_string()
    return str(value)


def soft_path(value):
    # Preserve the path carried by soft references, including unloaded targets.
    if value is None:
        return ""
    try:
        return unreal.SystemLibrary.conv_soft_object_reference_to_string(value)
    except (TypeError, AttributeError):
        return object_path(value)


def primary_id(value):
    return unreal.SystemLibrary.conv_primary_asset_id_to_string(value)


def optional_property(asset, name):
    try:
        return asset.get_editor_property(name)
    except Exception:
        return None


def imports(asset):
    data = optional_property(asset, "asset_import_data")
    if data is None:
        return []
    result = []
    for filename in data.extract_filenames():
        path = Path(unreal.Paths.convert_relative_path_to_full(str(filename))).resolve()
        exists = path.is_file()
        result.append({"filename": str(filename), "resolved_filename": path.as_posix(),
                       "exists": exists, "sha256": digest_file(path) if exists else None})
    return result


def snapshot_asset(asset, expected_package):
    actual = asset.get_path_name()
    # Unreal package FNames are case-insensitive. The legacy EDMBar package's
    # object already uses EdmBar, and AssetTools retains that canonical spelling.
    require(actual.split(".", 1)[0].casefold() == expected_package.casefold(),
            "Asset loaded through an unexpected redirect: " + expected_package + " -> " + actual)
    record = {"object_path": actual, "asset_name": asset.get_name(),
              "class": asset.get_class().get_path_name(), "class_name": asset.get_class().get_name(),
              "metadata": {str(k): str(v) for k, v in unreal.EditorAssetLibrary.get_metadata_tag_values(asset).items()}}
    if record["class_name"] == DEFINITION_CLASS:
        spec = asset.get_editor_property("spec")
        identity = str(spec.get_editor_property("facility_definition_id"))
        require(identity not in ("", "None"), "Definition has no stable ID: " + actual)
        registered = unreal.SystemLibrary.get_primary_asset_id_from_object(asset)
        record["definition"] = {
            "facility_id": identity,
            "expected_primary_id": "FFZSFacilityDefinition:" + identity,
            # Registration can be absent before the move if config is already updated.
            "registered_primary_id": primary_id(registered),
            "show_in_catalog": bool(asset.get_editor_property("show_in_build_catalog")),
            "spec_export": spec.export_text(),
            "catalog_thumbnail": soft_path(asset.get_editor_property("catalog_thumbnail")),
            "procedurally_managed_visual_mesh": bool(asset.get_editor_property("procedurally_managed_visual_mesh")),
        }
    if record["class_name"] == "Texture2D":
        record["import_sources"] = imports(asset)
    if record["class_name"] == "WidgetBlueprint":
        generated = asset.generated_class()
        require(generated is not None, "WBP has no generated class: " + actual)
        record["generated_class"] = generated.get_path_name()
        # UE5.7 does not expose WidgetBlueprint.ParentClass through Python on
        # every build. Generated class identity is mandatory; native inheritance
        # remains covered by the authored-widget automation tests.
        parent = optional_property(asset, "parent_class")
        record["parent_class"] = object_path(parent)
    if record["class_name"] == "PrimaryAssetLabel":
        explicit = [soft_path(value) for value in asset.get_editor_property("explicit_assets")]
        blueprints = [object_path(value) for value in asset.get_editor_property("explicit_blueprints")]
        require(all(explicit) and all(blueprints), "PAL has an unresolved explicit reference: " + actual)
        record["label"] = {
            "explicit_assets": sorted(explicit), "explicit_blueprints": sorted(blueprints),
            "rules": asset.get_editor_property("rules").export_text(),
            "runtime": bool(asset.get_editor_property("is_runtime_label")),
            "label_directory": bool(asset.get_editor_property("label_assets_in_my_directory")),
        }
        bundle = optional_property(asset, "asset_bundle_data")
        record["label"]["bundle_export"] = bundle.export_text() if bundle is not None and hasattr(bundle, "export_text") else None
    return record


def remap_text(value, manifest):
    pairs = [(entry["old"], entry["new"]) for entry in manifest["assets"]]
    pairs += [("/Game/" + old, "/Game/" + new) for old, new in manifest.get("folder_mappings", {}).items()]
    for old, new in sorted(pairs, key=lambda pair: len(pair[0]), reverse=True):
        value = value.replace(old, new)
    return value


def remap_value(value, manifest):
    if isinstance(value, str):
        return remap_text(value, manifest)
    if isinstance(value, list):
        return [remap_value(item, manifest) for item in value]
    if isinstance(value, dict):
        return {key: remap_value(item, manifest) for key, item in value.items()}
    return value


def expected_source_path(old_path, manifest):
    normalized = Path(old_path).resolve()
    for entry in manifest.get("source_files", []):
        if normalized == (ROOT / entry["old_file"]).resolve():
            return (ROOT / entry["new_file"]).resolve()
    return normalized


def capture(manifest, side):
    records = {}
    identities = set()
    for index, entry in enumerate(manifest["assets"]):
        package = entry[side]
        asset = unreal.load_asset(package)
        require(asset is not None, "Manifest asset failed to load: " + package)
        record = snapshot_asset(asset, package)
        if "definition" in record:
            identity = record["definition"]["facility_id"]
            require(identity not in identities, "Duplicate stable facility ID: " + identity)
            identities.add(identity)
        records[entry["old"]] = record
        if (index + 1) % 50 == 0:
            unreal.log("FF_BUILD_RELOCATION_READ {}/{}".format(index + 1, len(manifest["assets"])))
    require(len(identities) == EXPECTED_DEFINITIONS,
            "Expected {} definitions, found {}".format(EXPECTED_DEFINITIONS, len(identities)))
    return records


def verify_after(manifest, baseline, records):
    before_records = baseline["assets"]
    require(set(records) == set(before_records), "Before/after manifest membership differs")
    imported = []
    labels = []
    for old, record in records.items():
        before = before_records[old]
        expected = remap_value(before, manifest)
        for field in ("object_path", "asset_name", "class", "class_name", "generated_class", "parent_class"):
            actual_value, expected_value = record.get(field), expected.get(field)
            if field in ("object_path", "generated_class") and actual_value and expected_value:
                actual_value, expected_value = actual_value.casefold(), expected_value.casefold()
            require(actual_value == expected_value, "Asset identity/class changed for {} field {}".format(old, field))
        # Metadata is opaque authored content. Relocated package strings may change,
        # but unrelated values and keys must stay exactly the same.
        require(record["metadata"] == expected["metadata"], "Authored metadata changed or has stale paths: " + old)
        if "definition" in before:
            for field, value in expected["definition"].items():
                if field == "registered_primary_id":
                    continue
                require(record["definition"][field] == value, "Definition {} changed field {}".format(old, field))
            require(record["definition"]["registered_primary_id"] == record["definition"]["expected_primary_id"],
                    "Moved definition did not register in AssetManager: " + old)
        if "import_sources" in before:
            previous, current = before["import_sources"], record["import_sources"]
            require(len(previous) == len(current), "Texture import source count changed: " + old)
            for source_before, source_after in zip(previous, current):
                target = expected_source_path(source_before["resolved_filename"], manifest)
                require(Path(source_after["resolved_filename"]).resolve() == target,
                        "Texture import filename was not rebased: {} expected {} found {}".format(old, target, source_after["resolved_filename"]))
                if source_before["exists"]:
                    require(source_after["exists"] and source_after["sha256"] == source_before["sha256"],
                            "Texture import source missing or changed: " + str(target))
            imported.append({"asset": record["object_path"], "sources": current})
        if "label" in before:
            require(record["label"] == expected["label"], "PrimaryAssetLabel references or cook rules changed: " + old)
            labels.append({"asset": record["object_path"], **record["label"]})

    # Audit the entire Primary Asset type in a fresh process, including retired IDs.
    asset_type = unreal.PrimaryAssetType(name="FFZSFacilityDefinition")
    registered_ids = unreal.SystemLibrary.get_primary_asset_id_list(asset_type)
    registered = {primary_id(value): value for value in registered_ids}
    expected_ids = {record["definition"]["expected_primary_id"] for record in records.values() if "definition" in record}
    require(set(registered) == expected_ids,
            "PrimaryAsset ID set changed: missing={} extra={}".format(sorted(expected_ids - set(registered)), sorted(set(registered) - expected_ids)))
    expected_paths = {record["definition"]["expected_primary_id"]: record["object_path"] for record in records.values() if "definition" in record}
    for identity, value in registered.items():
        mapped = soft_path(unreal.SystemLibrary.get_soft_object_reference_from_primary_asset_id(value))
        require(mapped == expected_paths[identity], "PrimaryAsset maps to the wrong package: {} -> {}".format(identity, mapped))

    redirectors = []
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for entry in manifest["assets"]:
        # Registry enumeration treats a missing old package as a normal result;
        # EditorAssetLibrary.find_asset_data logs an error for that success case.
        for data in registry.get_assets_by_package_name(entry["old"]):
            require(str(data.asset_class_path.asset_name) == "ObjectRedirector",
                    "Original package still contains a non-redirector: " + entry["old"])
            redirectors.append(entry["old"])
        require((ROOT / entry["new_file"]).is_file(), "New asset file was not saved: " + entry["new_file"])
    old_packages = {entry["old"] for entry in manifest["assets"]}
    for folder in manifest.get("folder_mappings", {}):
        old_root = "/Game/" + folder
        if old_root in old_packages:
            continue  # This mapping moves one WBP, not its unrelated siblings.
        for data in registry.get_assets_by_path(old_root, recursive=True):
            require(str(data.asset_class_path.asset_name) == "ObjectRedirector",
                    "Original moved folder still contains a non-redirector: " + str(data.package_name))
    for entry in manifest.get("source_files", []):
        target = ROOT / entry["new_file"]
        require(target.is_file() and digest_file(target) == entry["sha256"], "Source file missing or changed: " + str(target))
    return {"registered_primary_ids": sorted(registered), "remaining_redirectors": redirectors,
            "texture_imports": imported, "primary_asset_labels": labels,
            "bundle_note": "Explicit PAL references and exposed bundle exports compared; typed UE5.7 registry bundles need native production tests."}


def run():
    require(MANIFEST_FILE.is_file(), "Missing relocation manifest: " + str(MANIFEST_FILE))
    manifest = json.loads(MANIFEST_FILE.read_text(encoding="utf-8-sig"))
    manifest_hash = digest_file(MANIFEST_FILE)
    require(len(manifest["assets"]) == EXPECTED_ASSETS, "Unexpected manifest asset count")
    require(len({entry["old"] for entry in manifest["assets"]}) == EXPECTED_ASSETS, "Duplicate old package names")
    require(len({entry["new"] for entry in manifest["assets"]}) == EXPECTED_ASSETS, "Duplicate new package names")
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(synchronous_search=True)
    explicit = re.search(r"(?:^|\s)-FFBuildingRelocationMode=(before|after)(?:\s|$)", unreal.SystemLibrary.get_command_line(), re.IGNORECASE)
    if explicit:
        mode = explicit.group(1).lower()
    elif all((ROOT / entry["new_file"]).is_file() for entry in manifest["assets"]):
        mode = "after"
    elif all((ROOT / entry["old_file"]).is_file() for entry in manifest["assets"]):
        mode = "before"
    else:
        raise RuntimeError("Mixed relocation state; cannot infer a complete before/after snapshot")
    records = capture(manifest, "old" if mode == "before" else "new")
    counts = {}
    for record in records.values():
        counts[record["class_name"]] = counts.get(record["class_name"], 0) + 1
    common = {"status": "PASS", "mode": mode, "manifest_sha256": manifest_hash,
              "asset_count": len(records), "classes": counts,
              "recommended_automation_command": "Automation RunTests " + "+".join(TESTS)}
    if mode == "before":
        baseline = {**common, "assets": records}
        if BEFORE_FILE.exists():
            previous = json.loads(BEFORE_FILE.read_text(encoding="utf-8"))
            require(previous["manifest_sha256"] == manifest_hash, "Existing baseline belongs to a different manifest; preserve it before taking a new snapshot")
            require(previous["assets"] == records, "Existing baseline differs; refusing to overwrite original evidence")
        else:
            write_report(BEFORE_FILE.name, baseline)
        write_report("before_verification.json", common)
    else:
        require(BEFORE_FILE.is_file(), "After verification requires before_snapshot.json")
        baseline = json.loads(BEFORE_FILE.read_text(encoding="utf-8"))
        require(baseline["manifest_sha256"] == manifest_hash, "Manifest changed after the baseline snapshot")
        details = verify_after(manifest, baseline, records)
        write_report("after_verification.json", {**common, **details, "assets": records})
    unreal.log("FF_BUILD_RELOCATION_{}_PASS assets={} definitions={}".format(mode.upper(), len(records), EXPECTED_DEFINITIONS))


if __name__ == "__main__":
    try:
        run()
    except Exception as error:
        write_report("verification_failure.json", {"status": "FAIL", "error": str(error), "traceback": traceback.format_exc()})
        unreal.log_error("FF_BUILD_RELOCATION_VERIFY_FAIL: " + str(error))
        raise
