"""Back up and migrate the current construction UI in an idle Entry editor.

Use a freshly compiled editor with -ExecutePythonScript. The C++ allowlist is
the source of truth; unrelated HUD assets must retain their original hashes.
"""
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import shutil
import traceback

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/BuildModeClarity/readability"
RUN = OUT / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
BACKUP = RUN / "before_assets"
COMMANDS = ("FF.ApplyBuildReferenceTools", "FF.ApplyBuildModeTypography", "FF.ApplyBuildReadability")
TYPOGRAPHY_VERSION = "FF.BuildMode.Typography20260929.2"
READABILITY_VERSION = "FF.BuildReadability.20260929.v2"
BASE = "/Game/FestaFiesta/Building/FF_WBP_Building/"
LAYOUT_PACKAGES = tuple(BASE + suffix for suffix in (
    "Catalog/WBP_FF_PrepPanelBuild", "Catalog/WBP_FF_PrepBuildTile",
    "FacilityCodex/WBP_FF_FacilityCodexScreen", "FacilityCodex/WBP_FF_FacilityCodexCard",
    "Facility/WBP_FF_FacilityInfoPanel", "Controls/WBP_FF_BuildTools",
))
REPORT = {"status": "RUNNING", "commands": list(COMMANDS), "executed": [],
          "backup": str(BACKUP.relative_to(ROOT)), "before": {}, "after": {}}


def typography_packages():
    address_source = (ROOT / "Source/FestaFiesta/Public/Building/Presentation/FF_BuildUIAssetPaths.h").read_text(encoding="utf-8-sig")
    addresses = dict(re.findall(r'const TCHAR\*\s+(\w+)\s*=\s*TEXT\("([^\"]+)"\)', address_source))
    migration = (ROOT / "Source/FestaFiestaEditor/Private/Building/Presentation/FF_BuildTypographyBuilder.cpp").read_text(encoding="utf-8-sig")
    block = migration.split("const TCHAR* Paths[] = {", 1)[1].split("};", 1)[0]
    packages = [addresses[name] for name in re.findall(r"FFBuildUIAssetPaths::(\w+)", block)]
    packages.extend(re.findall(r'TEXT\("(/Game/[^\"]+)"\)', block))
    assert packages and len(packages) == len(set(packages)), "Typography allowlist is empty or duplicated"
    return tuple(packages)


def path_for(package):
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ui_hashes():
    return {path.relative_to(ROOT).as_posix(): digest(path)
            for folder in (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
            for path in sorted(folder.rglob("*.uasset"))}


def verify(packages):
    for package in packages:
        blueprint = unreal.load_asset(package)
        assert blueprint, "Missing production WBP: " + package
        assert unreal.EditorAssetLibrary.get_metadata_tag(blueprint, TYPOGRAPHY_VERSION) == "1", "Missing typography migration: " + package
        assert unreal.EditorAssetLibrary.load_blueprint_class(package), "Missing generated class: " + package
    for package in LAYOUT_PACKAGES:
        blueprint = unreal.load_asset(package)
        assert unreal.EditorAssetLibrary.get_metadata_tag(blueprint, READABILITY_VERSION) == "1", "Missing layout migration: " + package
    tools = unreal.load_asset(BASE + "Controls/WBP_FF_BuildTools")
    assert unreal.EditorAssetLibrary.get_metadata_tag(tools, "FF.BuildReferenceTools.RemoveTopWalletV12") == "1", "Missing top gold removal"
    panel_class = unreal.EditorAssetLibrary.load_blueprint_class(LAYOUT_PACKAGES[0])
    assert abs(unreal.get_default_object(panel_class).get_editor_property("catalog_height") - 324.0) < .1, "Catalog height was not saved"


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Open idle /Engine/Maps/Entry first"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE before migration"
    packages = typography_packages()
    assert set(LAYOUT_PACKAGES).issubset(packages), "Layout target is missing from backup allowlist"
    REPORT["packages"] = list(packages)
    targets = {path_for(package).relative_to(ROOT).as_posix() for package in packages}
    before = ui_hashes()
    for package in packages:
        path = path_for(package)
        assert path.is_file(), "Missing WBP: " + package
        REPORT["before"][package] = digest(path)
        backup = BACKUP / path.relative_to(ROOT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, backup)
    for run in (1, 2):
        for command in COMMANDS:
            unreal.log("FF_BUILD_READABILITY_EXECUTE: " + command)
            unreal.SystemLibrary.execute_console_command(world, command)
            REPORT["executed"].append({"run": run, "command": command})
        verify(packages)
        hashes = {package: digest(path_for(package)) for package in packages}
        if run == 1:
            REPORT["after"] = hashes
        else:
            assert hashes == REPORT["after"], "Repeat migration changed a WBP"
            REPORT["idempotent"] = True
    # Readability deliberately adjusts a few text roles. The final font audit
    # must evaluate the saved end result, rather than the intermediate migration.
    unreal.SystemLibrary.execute_console_command(world, "FF.AuditBuildModeTypography")
    audit = json.loads((ROOT / "Saved/Automation/BuildTypography/typography-audit.json").read_text(encoding="utf-8-sig"))
    assert audit["status"] == "PASS", "Final typography audit failed"
    REPORT["typography"] = {key: audit[key] for key in ("status", "assets", "text_widgets", "font_updated")}
    after = ui_hashes()
    unexpected = [path for path in before.keys() | after.keys()
                  if path not in targets and before.get(path) != after.get(path)]
    assert not unexpected, "Unrelated UI changed: " + str(unexpected)
    REPORT["unrelated_ui_unchanged"] = True
    REPORT["changed_packages"] = [package for package in packages if REPORT["before"][package] != REPORT["after"][package]]
    REPORT["status"] = "PASS"


try:
    main()
except Exception:
    REPORT["status"] = "FAIL"
    REPORT["error"] = traceback.format_exc()
    unreal.log_error("FF_BUILD_READABILITY_APPLY_VERIFY_FAIL: " + REPORT["error"])
    raise
finally:
    RUN.mkdir(parents=True, exist_ok=True)
    data = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (RUN / "report.json").write_text(data, encoding="utf-8")
    (OUT / "latest-result.json").write_text(data, encoding="utf-8")

unreal.log("FF_BUILD_READABILITY_APPLY_VERIFY_PASS: " + str(RUN / "report.json"))
