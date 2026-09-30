"""Apply the editable codex layout after backing up the four explicitly scoped WBPs."""
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/BuildModeClarity/codex-navigation"
RUN = OUT / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
FOLDER = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/"
PACKAGES = [FOLDER + name for name in (
    "WBP_FF_FacilityCodexScreen", "WBP_FF_FacilityCodexCard",
    "Controls/WBP_FF_CodexCollectionReward", "WBP_FF_FacilityCodexCatalogLauncher")]
TARGETS = [ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset") for package in PACKAGES]
VERSION = "FF.Codex.NavigationRewards.20260929.v4"
REPORT = {"status": "RUNNING", "packages": PACKAGES}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ui_hashes():
    return {p.relative_to(ROOT).as_posix(): digest(p)
            for folder in (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
            for p in folder.rglob("*.uasset")}


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Requires idle Entry map"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    RUN.mkdir(parents=True, exist_ok=True)
    for target in TARGETS:
        shutil.copy2(target, RUN / target.name)
    REPORT["backup"] = str(RUN.relative_to(ROOT))
    before = ui_hashes()
    after_first = None
    for _ in range(2):
        unreal.SystemLibrary.execute_console_command(world, "FF.ApplyCodexNavigation")
        for package in PACKAGES:
            blueprint = unreal.load_asset(package)
            assert unreal.EditorAssetLibrary.get_metadata_tag(blueprint, VERSION) == "1", "Migration did not complete: " + package
            assert unreal.EditorAssetLibrary.load_blueprint_class(package), "Blueprint class is unavailable: " + package
        screen_class = unreal.EditorAssetLibrary.load_blueprint_class(PACKAGES[0])
        assert unreal.get_default_object(screen_class).get_editor_property("use_authored_collection_layout"), "Saved Canvas layout is not enabled"
        current = {target.relative_to(ROOT).as_posix(): digest(target) for target in TARGETS}
        if after_first is None:
            after_first = current
        else:
            assert current == after_first, "Second application changed the asset"
    after = ui_hashes()
    target_keys = {target.relative_to(ROOT).as_posix() for target in TARGETS}
    unexpected = [key for key in before.keys() | after.keys()
                  if key not in target_keys and before.get(key) != after.get(key)]
    assert not unexpected, "Unexpected changes: " + repr(unexpected)
    REPORT.update(status="PASS", idempotent=True, unrelated_ui_unchanged=True,
                  before={key: before[key] for key in target_keys}, after={key: after[key] for key in target_keys})


try:
    main()
except Exception:
    REPORT.update(status="FAILED", error=traceback.format_exc())
    unreal.log_error(REPORT["error"])
finally:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "latest-result.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("FF_CODEX_NAVIGATION_RESULT: " + REPORT["status"])
if REPORT["status"] != "PASS":
    raise RuntimeError("Codex navigation migration failed; read latest-result.json")
