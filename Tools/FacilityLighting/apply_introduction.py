"""Add first-use lighting guidance after backing up the single inspector WBP."""
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/FacilityLighting/Introduction"
RUN = OUT / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
PACKAGE = "/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel"
TARGET = ROOT / "Content" / (PACKAGE.removeprefix("/Game/") + ".uasset")
VERSION = "FF.FacilityLighting.Introduction20261007.1"
REPORT = {"status": "RUNNING", "package": PACKAGE}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ui_hashes():
    return {path.relative_to(ROOT).as_posix(): digest(path)
            for folder in (ROOT / "Content/FestaFiesta/UI",
                           ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
            for path in folder.rglob("*.uasset")}


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Requires idle Entry map"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    assert TARGET.is_file(), "Existing facility inspector WBP is required"
    blueprint = unreal.load_asset(PACKAGE)
    assert blueprint, "Existing facility inspector could not be loaded"
    REPORT["already_applied"] = unreal.EditorAssetLibrary.get_metadata_tag(blueprint, VERSION) == "1"
    RUN.mkdir(parents=True, exist_ok=True)
    shutil.copy2(TARGET, RUN / TARGET.name)
    REPORT["backup"] = str(RUN.relative_to(ROOT))
    before = ui_hashes()
    first_digest = None
    # The native command preflights the authored hierarchy and checks that all
    # original content children keep their exact slots while new cards are added.
    for _ in range(2):
        unreal.SystemLibrary.execute_console_command(world, "FF.ApplyFacilityLightingIntroduction")
        blueprint = unreal.load_asset(PACKAGE)
        assert unreal.EditorAssetLibrary.get_metadata_tag(blueprint, VERSION) == "1", "Migration did not complete"
        assert unreal.EditorAssetLibrary.load_blueprint_class(PACKAGE), "Saved inspector class is unavailable"
        current = digest(TARGET)
        if first_digest is None:
            first_digest = current
        else:
            assert current == first_digest, "Second application changed the saved WBP"
    after = ui_hashes()
    target_key = TARGET.relative_to(ROOT).as_posix()
    unexpected = [key for key in before.keys() | after.keys()
                  if key != target_key and before.get(key) != after.get(key)]
    assert not unexpected, "Unexpected UI asset changes: " + repr(unexpected)
    REPORT.update(status="PASS", idempotent=True, unrelated_ui_unchanged=True,
                  before=before[target_key], after=after[target_key])


try:
    main()
except Exception:
    REPORT.update(status="FAILED", error=traceback.format_exc())
    unreal.log_error(REPORT["error"])
finally:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "latest-result.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_LIGHTING_INTRODUCTION_RESULT: " + REPORT["status"])
if REPORT["status"] != "PASS":
    raise RuntimeError("Facility lighting introduction migration failed; read latest-result.json")
