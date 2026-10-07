"""Remove queue and clearance reservations from Road, Tile and Lighting assets.

Run in an unattended Unreal Python commandlet. The default audits without saving;
FESTAFIESTA_APPLY_NONSERVICE_FOOTPRINTS=1 enables recoverable package saves. Run
again in a fresh commandlet without the variable to verify persisted results.
Footprints, meshes, prices, lighting and every unrelated field are preserved.
"""

import hashlib
import json
import os
import sys
import traceback
from collections import Counter
from pathlib import Path

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
from ffzs_asset_save_transaction import AssetSaveTransaction
from ffzs_definition_source import read_facility_definitions, require_unattended_commandlet

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved/NonserviceFacilityFootprints"
NON_SERVICE = {unreal.FFZSFacilityCategory.ROAD, unreal.FFZSFacilityCategory.TILE,
               unreal.FFZSFacilityCategory.LIGHTING}
ARRAY_FIELDS = ("queue_reserved_rects", "queue_reserved_cells", "queue_spline_points_cm", "anchors")


def require(value, message):
    if not value:
        raise RuntimeError(message)


def normalized_spec(spec):
    result = spec.copy()
    result.set_editor_property("clearance_cells", 0)
    for field in ARRAY_FIELDS:
        result.set_editor_property(field, [])
    # QueueLayout is intentionally read-only to editor property setters; import
    # its single reflected field on the detached struct when stale data exists.
    if result.get_editor_property("queue_layout") != unreal.FFZSQueueLayout.AUTHORED:
        result.import_text("(QueueLayout=Authored)")
        require(result.get_editor_property("queue_layout") == unreal.FFZSQueueLayout.AUTHORED,
                "Could not reset the queue layout")
    queue = result.get_editor_property("queue_rules").copy()
    queue.set_editor_property("max_waiting_agents", 0)
    queue.set_editor_property("service_capacity", 0)
    result.set_editor_property("queue_rules", queue)
    audience = result.get_editor_property("audience").copy()
    audience.set_editor_property("enabled", False)
    result.set_editor_property("audience", audience)
    return result


def record_layout(record):
    spec = record["definition_object"].get_editor_property("spec")
    category = spec.get_editor_property("category")
    selected = category in NON_SERVICE
    normalized = normalized_spec(spec) if selected else spec
    rules = spec.get_editor_property("queue_rules")
    text = spec.export_text()
    row = {"definition": record["definition"], "id": record["facility_definition_id"],
           "category": str(category), "selected": selected,
           "footprint_cm": [spec.get_editor_property("footprint_size_cells").x,
                            spec.get_editor_property("footprint_size_cells").y],
           "clearance_cm": int(spec.get_editor_property("clearance_cells")),
           "max_waiting_agents": int(rules.get_editor_property("max_waiting_agents")),
           "service_capacity": int(rules.get_editor_property("service_capacity")),
           "audience_enabled": bool(spec.get_editor_property("audience").get_editor_property("enabled")),
           "queue_layout": str(spec.get_editor_property("queue_layout")),
           "spec_sha256": hashlib.sha256(text.encode("utf-8")).hexdigest(),
           "preserved_fields_sha256": hashlib.sha256(normalized.export_text().encode("utf-8")).hexdigest(),
           "matches_target": text == normalized.export_text()}
    row.update({field + "_count": len(spec.get_editor_property(field)) for field in ARRAY_FIELDS})
    return row


def assert_preserved(before, after):
    expected = {row["definition"]: row["preserved_fields_sha256"] for row in before}
    require(len(expected) == len(after), "Definition count changed")
    require(all(expected.get(row["definition"]) == row["preserved_fields_sha256"] for row in after),
            "Unrelated authored fields changed")


def run():
    require_unattended_commandlet()
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT, "Wrong project")
    apply = os.environ.get("FESTAFIESTA_APPLY_NONSERVICE_FOOTPRINTS") == "1"
    records = read_facility_definitions(require_visual_mesh=False)
    before = [record_layout(record) for record in records]
    report = {"mode": "apply" if apply else "audit", "status": "RUNNING", "definition_count": len(records),
              "selected_count": sum(row["selected"] for row in before),
              "category_counts": dict(Counter(row["category"] for row in before)),
              "changed_count": 0, "before": before}
    OUT.mkdir(parents=True, exist_ok=True)
    transaction = None
    try:
        validation_asset = unreal.new_object(unreal.FFZSFacilityDefinition)
        candidates = []
        for record, row in zip(records, before):
            if not row["selected"]:
                continue
            original = record["spec"]
            candidate = normalized_spec(original)
            require(normalized_spec(candidate).export_text() == candidate.export_text(),
                    "Normalization is not idempotent: " + row["definition"])
            validation_asset.set_editor_property("spec", candidate)
            errors = validation_asset.is_definition_valid()
            require(errors is not None and len(errors) == 0,
                    "Native definition validation failed: " + row["definition"] + ": " + str(errors))
            if not row["matches_target"]:
                candidates.append((record, candidate))
        report["needs_change_count"] = len(candidates)
        if apply and candidates:
            # Validate the entire batch before touching production objects or disk.
            transaction = AssetSaveTransaction([record["definition_object"] for record, _ in candidates],
                                               "NonserviceFootprints")
            for record, candidate in candidates:
                record["definition_object"].set_editor_property("spec", candidate)
            after = [record_layout(record) for record in records]
            assert_preserved(before, after)
            require(all(row["matches_target"] for row in after), "Normalization left reserved space")
            for record, _ in candidates:
                require(unreal.EditorAssetLibrary.save_loaded_asset(record["definition_object"], only_if_is_dirty=False),
                        "Asset save failed: " + record["definition"])
                report["changed_count"] += 1
            transaction.commit()
            transaction = None
        report["after"] = [record_layout(record) for record in records]
        assert_preserved(before, report["after"])
        report["unrelated_fields_preserved"] = True
        report["nonmatching_count"] = sum(not row["matches_target"] for row in report["after"])
        report["status"] = "PASS" if report["nonmatching_count"] == 0 else "NEEDS_MIGRATION"
        migration_path = OUT / "migration.json"
        if not apply and migration_path.exists():
            previous = json.loads(migration_path.read_text(encoding="utf-8"))
            assert_preserved(previous["before"], report["after"])
            expected = {row["definition"]: row["spec_sha256"] for row in previous["after"]}
            report["saved_specs_match_migration"] = all(
                expected.get(row["definition"]) == row["spec_sha256"] for row in report["after"])
            require(report["saved_specs_match_migration"], "Persisted specs differ from migration")
    except Exception:
        report["status"] = "FAIL"
        report["error"] = traceback.format_exc()
        if transaction:
            transaction.rollback()
        raise
    finally:
        report_path = OUT / ("migration.json" if apply else "audit.json")
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("NONSERVICE_FOOTPRINTS: " + json.dumps({key: report[key] for key in
               ("mode", "status", "definition_count", "selected_count", "changed_count", "needs_change_count", "nonmatching_count")}))


if __name__ == "__main__":
    run()
