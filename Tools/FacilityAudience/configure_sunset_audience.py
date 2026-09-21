"""One-time audience-only migration. Run inside UE 5.7 after compiling FF_ZoneSystem.

Only the exact SunsetMainStage PDA can be saved. Subsequent runs preserve authored
layouts. Mesh, lights, audio, Queue, maps and other developers' files are untouched.
"""
import csv
import hashlib
import json
import shutil
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved/FacilityAudience"
PDA = "/Game/FestaFiesta/Building/FF_Data_Building/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage"
ASSET = ROOT / "Content" / (PDA.removeprefix("/Game/") + ".uasset")


def xyz(vector):
    return [vector.x, vector.y, vector.z]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    preset = json.loads(Path(__file__).with_name("sunset_audience_preset.json").read_text(encoding="utf-8"))
    pda = unreal.load_asset(PDA)
    assert isinstance(pda, unreal.FFZSFacilityDefinition), "Compile the audience feature before running this tool."
    spec = pda.get_editor_property("spec").copy()
    assert str(spec.get_editor_property("facility_definition_id")) == preset["facility_definition_id"]
    original_export = spec.export_text()
    original_audience = spec.get_editor_property("audience").copy()
    was_enabled = original_audience.get_editor_property("enabled")
    original_hash = hashlib.sha256(ASSET.read_bytes()).hexdigest()
    applied = False
    if not was_enabled:
        defaults = unreal.FFZSAudienceLayout()
        assert original_audience.export_text() == defaults.export_text(), (
            "A disabled but authored audience layout exists. Enable/edit it in the PDA; automatic overwrite was refused."
        )
        audience = unreal.FFZSAudienceLayout()
        audience.set_editor_property("enabled", True)
        for key in ("columns", "rows", "column_spacing_cm", "row_spacing_cm", "center_aisle_width_cm",
                    "perimeter_margin_cm", "agent_radius_cm", "arrival_tolerance_cm"):
            audience.set_editor_property(key, preset[key])
        footprint = spec.get_editor_property("footprint_size_cells")
        width = (preset["columns"] - 1) * preset["column_spacing_cm"] + preset["center_aisle_width_cm"]
        first_y = footprint.y * 0.5 + preset["front_row_offset_from_stage_front_cm"]
        for rect in spec.get_editor_property("queue_reserved_rects"):
            first_y = max(first_y, rect.max.y - footprint.y * 0.5 + preset["perimeter_margin_cm"] + 100.0)
        last_y = first_y + (preset["rows"] - 1) * preset["row_spacing_cm"]
        half_margin = preset["perimeter_margin_cm"] * 0.5
        audience.set_editor_property("first_slot_local_cm", unreal.Vector(-width * 0.5, first_y, 0))
        audience.set_editor_property("entry_local_cm", unreal.Vector(-width * 0.5 - half_margin, last_y + half_margin, 0))
        audience.set_editor_property("exit_local_cm", unreal.Vector(width * 0.5 + half_margin, last_y + half_margin, 0))
        audience.set_editor_property("focus_local_cm", unreal.Vector(0, footprint.y * 0.25, 200))
        spec.set_editor_property("audience", audience)
        pda.set_editor_property("spec", spec)
        validation = pda.is_definition_valid()
        # UE Python consumes the bool success return: output value on success, None on failure.
        assert validation is not None and len(validation) == 0, "PDA validation failed: " + str(validation)
        audit = pda.get_editor_property("spec").copy()
        audit.set_editor_property("audience", original_audience)
        assert audit.export_text() == original_export, "A non-audience Spec property changed."
        backup = OUT / "PDA_SunsetMainStage.before_audience.uasset"
        if not backup.exists():
            shutil.copy2(ASSET, backup)
            (OUT / "before_spec.txt").write_text(original_export, encoding="utf-8")
        assert unreal.EditorAssetLibrary.save_loaded_asset(pda, only_if_is_dirty=False), "Asset save failed"
        applied = True
    validation = pda.is_definition_valid()
    assert validation is not None and len(validation) == 0, "Existing audience layout is invalid; inspect PDA errors."
    spec = pda.get_editor_property("spec")
    audience = spec.get_editor_property("audience")
    cols, rows = audience.columns, audience.rows
    snapshot = unreal.FFZSAudienceSnapshot()
    snapshot.set_editor_property("layout", audience)
    snapshot.set_editor_property("capacity", cols * rows)
    snapshot.set_editor_property("facility_transform", unreal.Transform())
    snapshot.set_editor_property("focus_world_cm", audience.focus_local_cm)
    slots = []
    for index in range(cols * rows):
        transform = unreal.FFZSAudienceLibrary.get_audience_slot_transform(snapshot, index)
        assert transform is not None, "Native slot lookup failed: " + str(index)
        slots.append({"slot_index": index, "position_cm": xyz(transform.translation),
                      "yaw_degrees": transform.rotation.rotator().yaw})
    report = {
        "status": "PASS", "applied_initial_preset": applied, "asset": PDA,
        "before_sha256": original_hash, "after_sha256": hashlib.sha256(ASSET.read_bytes()).hexdigest(),
        "capacity": cols * rows, "columns": cols, "rows": rows,
        "footprint_cm": [spec.footprint_size_cells.x, spec.footprint_size_cells.y],
        "audience_export": audience.export_text(),
        "entry_cm": xyz(audience.entry_local_cm), "exit_cm": xyz(audience.exit_local_cm),
        "focus_cm": xyz(audience.focus_local_cm), "perimeter_margin_cm": audience.perimeter_margin_cm,
        "agent_radius_cm": audience.agent_radius_cm, "slots": slots,
        "validation": "native PDA geometry validation and native slot lookup",
        "not_verified": "Live-map NavMesh, routes from every zone, crowd movement, fixed-time gathering, target visitor capacity"
    }
    (OUT / "sunset_audience_report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    with (OUT / "sunset_audience_slots.csv").open("w", newline="", encoding="utf-8-sig") as file:
        writer = csv.writer(file)
        writer.writerow(["SlotIndex", "LocalX_cm", "LocalY_cm", "LocalZ_cm", "FacingYaw_deg"])
        for slot in slots:
            writer.writerow([slot["slot_index"], *slot["position_cm"], slot["yaw_degrees"]])
    unreal.log("FACILITY_AUDIENCE_PASS capacity=" + str(cols * rows) + " applied=" + str(applied))


if __name__ == "__main__":
    main()
