"""Migrate only the two saved staff-filter widths from fixed widths to minimum widths.

Run with Unreal's PythonScript commandlet while the editor is closed. No runtime
or game state changes; repeated runs leave an already migrated asset untouched.
"""
from datetime import datetime, timezone
from pathlib import Path
import json
import shutil
import traceback
import uuid
import unreal


PACKAGE = '/Game/FestaFiesta/UI/Staff/WBP_FF_StaffTraining'
TARGETS = ('Management_Size_AllFilter', 'Management_Size_UnassignedFilter')
ROOT = Path(unreal.Paths.project_dir()).resolve()
SAVED = Path(unreal.Paths.project_saved_dir()).resolve()
OUT = SAVED / 'StaffManagement'
OUT.mkdir(parents=True, exist_ok=True)
REPORT = {'status': 'running', 'asset': PACKAGE, 'changed': False}


def widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


def structure(table):
    return {name: (w.get_class().get_path_name(), w.get_parent().get_name() if w.get_parent() else None)
            for name, w in table.items()}


def dimensions(widget):
    return {prop: widget.get_editor_property(prop) for prop in (
        'width_override', 'override_width_override', 'min_desired_width', 'override_min_desired_width',
        'height_override', 'override_height_override', 'max_desired_width', 'override_max_desired_width')}


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE before migrating the asset.'
    bp = unreal.load_asset(PACKAGE)
    assert bp, 'Staff training WBP is missing.'
    assert PACKAGE not in {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}, \
        'The target asset has unsaved changes; preserve them before migrating.'
    table = widgets(bp)
    before_structure = structure(table)
    before = {}
    for name in TARGETS:
        assert isinstance(table.get(name), unreal.SizeBox), 'Expected filter SizeBox: ' + name
        before[name] = dimensions(table[name])
    REPORT['before'] = before
    needs_change = [name for name in TARGETS if before[name]['override_width_override']]
    if needs_change:
        backup = SAVED / 'UIBackups/StaffFilterCaptions' / (
            datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex)
        backup.mkdir(parents=True)
        source = ROOT / 'Content' / (PACKAGE[6:] + '.uasset')
        shutil.copy2(source, backup / source.name)
        REPORT['backup'] = str(backup / source.name)
        for name in needs_change:
            previous = before[name]
            assert previous['width_override'] > 0, 'Fixed width must be positive: ' + name
            minimum = max(previous['width_override'],
                          previous['min_desired_width'] if previous['override_min_desired_width'] else 0)
            table[name].set_min_desired_width(minimum)
            table[name].clear_width_override()
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        updated = widgets(bp)
        assert structure(updated) == before_structure, 'Widget hierarchy or bindings changed.'
        for name in TARGETS:
            expected = dict(before[name])
            if name in needs_change:
                expected['override_width_override'] = False
                expected['override_min_desired_width'] = True
                expected['min_desired_width'] = max(before[name]['width_override'],
                    before[name]['min_desired_width'] if before[name]['override_min_desired_width'] else 0)
            assert dimensions(updated[name]) == expected, 'Unexpected SizeBox setting change: ' + name
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), 'Failed to save target WBP.'
        REPORT['changed'] = True
    REPORT.update(status='passed', after={name: dimensions(widgets(bp)[name]) for name in TARGETS},
                  structure_unchanged=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('STAFF_FILTER_CAPTIONS_FAIL: ' + REPORT['error'])
    raise
finally:
    (OUT / 'filter-caption-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
