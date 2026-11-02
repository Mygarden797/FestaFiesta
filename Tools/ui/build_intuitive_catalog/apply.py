"""Apply the production catalog layout after building FestaFiestaEditor.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> on an idle editor.
Only the catalog panel and tile are saved. Original bytes are backed up in Saved.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
PACKAGES = (
    "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild",
    "/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile",
)
RUN = ROOT / "Saved/CatalogDesign" / datetime.now().strftime("%Y%m%d-%H%M%S")
RUN.mkdir(parents=True, exist_ok=True)
report = {}
for package in PACKAGES:
    source = ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")
    backup = RUN / source.name
    shutil.copy2(source, backup)
    report[package] = {"backup": str(backup), "before": hashlib.sha256(source.read_bytes()).hexdigest()}

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world, "An idle editor world is required"
unreal.SystemLibrary.execute_console_command(world, "FF.ApplyBuildIntuitiveCatalog")
for package in PACKAGES:
    bp = unreal.load_asset(package)
    assert bp, "Catalog asset failed to load: " + package
    source = ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")
    report[package]["after"] = hashlib.sha256(source.read_bytes()).hexdigest()
    if package == PACKAGES[0]:
        names = {w.get_name() for w in unreal.ObjectIterator(unreal.Widget)
                 if w.get_outer() and w.get_outer().get_outer() == bp}
        assert {"Check_FilterRequired", "Check_FilterAffordable", "Check_FilterPrograms",
                "Check_FilterLighting", "ComboBox_CatalogSort"}.issubset(names), "Filter controls were not authored"
(RUN / "apply.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("INTUITIVE_CATALOG_APPLIED " + str(RUN))
