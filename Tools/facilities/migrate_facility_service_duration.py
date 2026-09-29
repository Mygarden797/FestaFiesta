"""Audit or migrate production facility services to five simulation seconds.

Use an unattended Unreal Python commandlet. The default is read-only; append
-FFApplyFiveSecondFacilityServices to save authored durations and upgrade duration
multipliers. Run again without that flag in a fresh commandlet to verify disk.
Prices, revenue multipliers, geometry and scheduled program durations are kept.
"""

import datetime
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
from ffzs_definition_source import read_facility_definitions, require_unattended_commandlet

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/FacilityServiceDuration5Seconds"
TARGET_SECONDS = 5.0


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def spec_without_service_times(spec):
    # Compare every other authored property, including all monetary values.
    # Export omits values matching struct defaults. Normalize a detached copy
    # to a non-default sentinel so changing to a default does not look like a
    # removed property, and never rewrite any field on the production object.
    normalized = spec.copy()
    queue = normalized.get_editor_property("queue_rules").copy()
    queue.set_editor_property("service_duration_seconds", 123.0)
    normalized.set_editor_property("queue_rules", queue)
    tiers = [tier.copy() for tier in normalized.get_editor_property("upgrade_tiers")]
    for tier in tiers:
        tier.set_editor_property("service_duration_multiplier", 123.0)
    normalized.set_editor_property("upgrade_tiers", tiers)
    return normalized.export_text()


def timing_record(record):
    spec = record["definition_object"].get_editor_property("spec")
    duration = float(spec.get_editor_property("queue_rules").get_editor_property("service_duration_seconds"))
    tiers = [{"level": int(tier.get_editor_property("target_level")),
              "multiplier": float(tier.get_editor_property("service_duration_multiplier")),
              "effective_seconds": duration * float(tier.get_editor_property("service_duration_multiplier"))}
             for tier in spec.get_editor_property("upgrade_tiers")]
    return {"asset": record["definition"], "id": record["facility_definition_id"],
            "base_seconds": duration, "tiers": tiers,
            "non_duration_sha256": hashlib.sha256(spec_without_service_times(spec).encode("utf-8")).hexdigest(),
            "matches_target": duration == TARGET_SECONDS and all(tier["multiplier"] == 1.0 for tier in tiers)}


def run():
    require_unattended_commandlet()
    apply = bool(re.search(r"(?:^|\s)-FFApplyFiveSecondFacilityServices(?:\s|$)",
                           unreal.SystemLibrary.get_command_line()))
    records = read_facility_definitions(require_visual_mesh=False)
    before = [timing_record(record) for record in records]
    selected = [record for record, timing in zip(records, before) if not timing["matches_target"]]
    report = {"mode": "apply" if apply else "audit", "target_seconds": TARGET_SECONDS,
              "definition_count": len(records), "changed_count": 0, "before": before,
              "backup_directory": None, "backups": []}
    OUT.mkdir(parents=True, exist_ok=True)
    if apply and selected:
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        backup_root = OUT / ("before-" + stamp)
        report["backup_directory"] = str(backup_root)
        # Back up and verify every package before mutating any loaded UObject.
        for record in selected:
            relative = record["definition"].removeprefix("/Game/")
            base = ROOT / "Content" / relative
            package_files = list(base.parent.glob(base.name + ".*"))
            if not package_files:
                raise RuntimeError("Missing package on disk: " + record["definition"])
            for source in package_files:
                # Project paths can already approach Windows MAX_PATH. Keep the
                # backup name short and retain the full original in the manifest.
                backup_name = hashlib.sha256(str(source.relative_to(ROOT)).encode("utf-8")).hexdigest()[:20]
                backup = backup_root / (backup_name + source.suffix)
                backup.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, backup)
                checksum = digest(source)
                if digest(backup) != checksum:
                    raise RuntimeError("Backup mismatch: " + str(source))
                report["backups"].append({"source": str(source), "backup": str(backup), "sha256": checksum})
        (backup_root / "manifest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        try:
            for record in selected:
                definition = record["definition_object"]
                spec = definition.get_editor_property("spec")
                unchanged = spec_without_service_times(spec)
                queue = spec.get_editor_property("queue_rules")
                queue.set_editor_property("service_duration_seconds", TARGET_SECONDS)
                spec.set_editor_property("queue_rules", queue)
                tiers = list(spec.get_editor_property("upgrade_tiers"))
                for tier in tiers:
                    tier.set_editor_property("service_duration_multiplier", 1.0)
                spec.set_editor_property("upgrade_tiers", tiers)
                definition.set_editor_property("spec", spec)
                after_unchanged = spec_without_service_times(definition.get_editor_property("spec"))
                if after_unchanged != unchanged:
                    (OUT / "non-duration-before.txt").write_text(unchanged, encoding="utf-8")
                    (OUT / "non-duration-after.txt").write_text(after_unchanged, encoding="utf-8")
                    raise RuntimeError("A non-duration field changed: " + record["definition"])
                if not timing_record(record)["matches_target"]:
                    raise RuntimeError("The requested timing was not applied: " + record["definition"])
                if not unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False):
                    raise RuntimeError("Cannot save " + record["definition"])
                report["changed_count"] += 1
        except Exception:
            # Loaded object state is discarded when the commandlet exits.
            for item in report["backups"]:
                if digest(Path(item["source"])) == item["sha256"]:
                    continue
                shutil.copy2(item["backup"], item["source"])
                if digest(Path(item["source"])) != item["sha256"]:
                    raise RuntimeError("Rollback hash mismatch: " + item["source"])
            raise
    report["after"] = [timing_record(record) for record in records]
    report["nonmatching_count"] = sum(not timing["matches_target"] for timing in report["after"])
    report["status"] = "PASS" if report["nonmatching_count"] == 0 else "NEEDS_MIGRATION"
    baseline = report["before"]
    previous_migration = OUT / "migration.json"
    if not apply and previous_migration.exists():
        baseline = json.loads(previous_migration.read_text(encoding="utf-8"))["before"]
    expected = {row["asset"]: row["non_duration_sha256"] for row in baseline}
    report["non_duration_fields_preserved"] = all(
        expected.get(row["asset"]) == row["non_duration_sha256"] for row in report["after"])
    report_path = OUT / ("migration.json" if apply else "audit.json")
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FACILITY_SERVICE_DURATION: " + json.dumps({key: report[key] for key in
               ("mode", "status", "definition_count", "changed_count", "nonmatching_count", "backup_directory")}))
    if apply and report["nonmatching_count"]:
        raise RuntimeError("Some production facility durations still differ from five seconds")


run()
