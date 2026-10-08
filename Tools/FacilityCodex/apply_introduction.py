"""Add saved first-use codex UI in an idle Entry editor after compiling the editor module.

Run with Unreal's Python: py "<project>/Tools/FacilityCodex/apply_introduction.py"
The three target assets are backed up under Saved/FacilityCodexIntroduction/<run>/.
The report verifies preserved authored widgets, all unrelated UI packages,
the two optional native UI bindings, and a byte-identical second application.
"""
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/FacilityCodexIntroduction"
RUN = OUT / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
FOLDER = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/"
PACKAGES = {
    "Launcher": FOLDER + "WBP_FF_FacilityCodexLauncher",
    "CatalogLauncher": FOLDER + "WBP_FF_FacilityCodexCatalogLauncher",
    "Screen": FOLDER + "WBP_FF_FacilityCodexScreen",
}
TARGETS = {role: ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")
           for role, package in PACKAGES.items()}
VERSION = "FF.FacilityCodex.Introduction20261007.1"
COMMAND = "FF.ApplyFacilityCodexIntroduction"
REPORT = {"status": "RUNNING", "packages": PACKAGES}


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def ui_hashes():
    # Protect both project UI roots, matching the other narrow codex migrations.
    return {path.relative_to(ROOT).as_posix(): digest(path)
            for folder in (ROOT / "Content/FestaFiesta/UI", ROOT / "Content/FestaFiesta/Building/FF_WBP_Building")
            for path in folder.rglob("*")
            if path.is_file() and path.suffix.lower() in (".uasset", ".umap", ".uexp", ".ubulk")}


def widgets(blueprint):
    result = {widget.get_name(): widget for widget in unreal.ObjectIterator(unreal.Widget)
              if widget.get_outer() and widget.get_outer().get_class().get_name() == "WidgetTree"
              and widget.get_outer().get_outer() == blueprint}
    roots = [widget for widget in result.values() if not widget.get_parent()]
    assert len(roots) == 1, "Detached authored widgets: " + blueprint.get_name()
    return result, roots[0]


def relation(widget):
    parent = widget.get_parent()
    slot = widget.get_editor_property("slot")
    return {
        "parent": parent.get_name() if parent else None,
        "index": parent.get_child_index(widget) if parent else None,
        "slot": slot.get_name() if slot else None,
    }


def authored_snapshot(tree):
    result = {}
    for name, widget in tree.items():
        item = {"class": widget.get_class().get_name(), "relation": relation(widget),
                "visibility": str(widget.get_visibility())}
        if isinstance(widget, unreal.TextBlock):
            item["text"] = str(widget.get_text())
            item["font"] = widget.get_editor_property("font").export_text()
        if isinstance(widget, unreal.Button):
            content = widget.get_content()
            item["content"] = content.get_name() if content else None
            item["style"] = widget.get_editor_property("widget_style").export_text()
        if isinstance(widget, unreal.SizeBox):
            for field in ("width_override", "height_override", "min_desired_width", "min_desired_height",
                          "max_desired_width", "max_desired_height"):
                item[field] = widget.get_editor_property(field)
        result[name] = item
    return result


def verify_authored(before, tree, original_root, role):
    after = authored_snapshot(tree)
    assert before.keys() <= after.keys(), "Existing authored widgets were removed: " + role
    for name, original in before.items():
        current = dict(after[name])
        if role != "Screen" and name == "Button_OpenCodex" and "Overlay_IntroductionLauncher" not in before:
            wrapper = tree["Overlay_IntroductionLauncher"]
            assert relation(wrapper) == original["relation"], "Launcher's original parent slot changed"
            assert tree[name].get_parent() == wrapper
            current["relation"] = original["relation"]
        elif role == "Screen" and name == original_root and "Overlay_IntroductionRoot" not in before:
            # Non-panel roots get one fill overlay wrapper; descendants retain their slots.
            if current["relation"]["parent"] == "Overlay_IntroductionRoot":
                current["relation"] = original["relation"]
        assert current == original, "Unexpected authored-widget edit: " + role + "/" + name


def verify(blueprints, baseline):
    for role, blueprint in blueprints.items():
        assert unreal.EditorAssetLibrary.get_metadata_tag(blueprint, VERSION) == "1", "Migration failed: " + role
        assert unreal.EditorAssetLibrary.load_blueprint_class(PACKAGES[role]), "Missing generated class: " + role
        tree, root = widgets(blueprint)
        verify_authored(baseline[role]["widgets"], tree, baseline[role]["root"], role)
        if role != "Screen":
            button = tree["Button_OpenCodex"]
            overlay = tree["Overlay_IntroductionLauncher"]
            badge = tree["Border_IntroductionHint"]
            bounds = tree["Size_IntroductionHint"]
            assert isinstance(badge, unreal.Border)
            assert badge.get_visibility() == unreal.SlateVisibility.COLLAPSED
            assert bounds.get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE
            assert isinstance(button.get_content(), unreal.TextBlock), "Original direct button label was changed"
            assert button.get_parent() == bounds.get_parent() == overlay
            assert badge.get_parent() == bounds
            assert bounds.get_editor_property("width_override") == 18.0
            assert bounds.get_editor_property("height_override") == 18.0
            slot = bounds.get_editor_property("slot")
            assert slot.get_editor_property("horizontal_alignment") == unreal.HorizontalAlignment.H_ALIGN_RIGHT
            assert slot.get_editor_property("vertical_alignment") == unreal.VerticalAlignment.V_ALIGN_TOP
            inset = slot.get_editor_property("padding")
            assert inset.top == 4.0 and inset.right == 4.0, "Badge must remain within launcher bounds"
            assert str(tree["Text_IntroductionHint"].get_text()) == "!"
        else:
            layer = tree["Border_Introduction"]
            assert isinstance(layer, unreal.Border)
            assert layer.get_visibility() == unreal.SlateVisibility.COLLAPSED
            assert isinstance(tree["Button_DismissIntroduction"], unreal.Button)
            assert str(tree["Text_IntroductionTitle"].get_text()) == "시설 조합 도감을 확인해 보세요"
            assert str(tree["Text_DismissIntroduction"].get_text()) == "도감 살펴보기"
            assert layer.get_parent() == root, "Introduction must cover the entire screen root"
            if isinstance(root, unreal.Overlay):
                assert root.get_child_index(layer) == root.get_children_count() - 1, "Introduction must be the top layer"
            else:
                assert isinstance(root, unreal.CanvasPanel)
                slot = layer.get_editor_property("slot")
                layout = slot.get_editor_property("layout_data")
                assert layout.anchors.minimum == unreal.Vector2D(0.0, 0.0)
                assert layout.anchors.maximum == unreal.Vector2D(1.0, 1.0)


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Requires idle Entry map"
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Stop PIE first"
    blueprints = {role: unreal.load_asset(package) for role, package in PACKAGES.items()}
    assert all(blueprints.values()), "All three existing codex assets are required"
    baseline = {}
    for role, blueprint in blueprints.items():
        tree, root = widgets(blueprint)
        baseline[role] = {"widgets": authored_snapshot(tree), "root": root.get_name()}
    RUN.mkdir(parents=True, exist_ok=True)
    for target in TARGETS.values():
        shutil.copy2(target, RUN / target.name)
    REPORT["backup"] = str(RUN.relative_to(ROOT))
    before = ui_hashes()
    after_first = None
    for _ in range(2):
        unreal.SystemLibrary.execute_console_command(world, COMMAND)
        verify(blueprints, baseline)
        current = {target.relative_to(ROOT).as_posix(): digest(target) for target in TARGETS.values()}
        if after_first is None:
            after_first = current
        else:
            assert current == after_first, "Second application changed an asset"
    after = ui_hashes()
    target_keys = {target.relative_to(ROOT).as_posix() for target in TARGETS.values()}
    unexpected = [key for key in before.keys() | after.keys()
                  if key not in target_keys and before.get(key) != after.get(key)]
    assert not unexpected, "Unexpected UI changes: " + repr(unexpected)
    REPORT.update(status="PASS", idempotent=True, existing_authored_widgets_preserved=True,
                  unrelated_ui_unchanged=True, protected_ui_packages=len(before) - len(target_keys),
                  before={key: before[key] for key in sorted(target_keys)},
                  after={key: after[key] for key in sorted(target_keys)})


try:
    main()
except Exception:
    REPORT.update(status="FAILED", error=traceback.format_exc())
    unreal.log_error(REPORT["error"])
finally:
    OUT.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(REPORT, ensure_ascii=False, indent=2)
    (OUT / "latest-result.json").write_text(serialized, encoding="utf-8")
    if RUN.exists():
        (RUN / "result.json").write_text(serialized, encoding="utf-8")
    unreal.log("FF_CODEX_INTRODUCTION_RESULT: " + REPORT["status"])
if REPORT["status"] != "PASS":
    raise RuntimeError("Codex introduction migration failed; read Saved/FacilityCodexIntroduction/latest-result.json")
