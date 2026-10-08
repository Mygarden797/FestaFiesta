"""Read-only UI authoring diagnostics for packaging QA. Run in the idle Entry editor."""
import json
from pathlib import Path
import unreal

packages = {
    'tile': '/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile',
    'catalog': '/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild',
    'tools': '/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools',
    'staff': '/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff',
}
result = {}
for key, package in packages.items():
    bp = unreal.load_asset(package)
    assert bp, package
    rows = []
    for w in unreal.ObjectIterator(unreal.Widget):
        if not (w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree' and w.get_outer().get_outer() == bp):
            continue
        row = {'name': w.get_name(), 'type': w.get_class().get_name(), 'parent': w.get_parent().get_name() if w.get_parent() else None}
        for prop in ('visibility', 'width_override', 'height_override', 'override_height_override', 'min_desired_height', 'auto_wrap_text', 'wrap_text_at', 'text_overflow_policy', 'line_height_percentage', 'background', 'padding'):
            try:
                row[prop] = str(w.get_editor_property(prop))
            except Exception:
                pass
        slot = w.get_editor_property('slot')
        if slot:
            row['slot_type'] = slot.get_class().get_name()
            for prop in ('padding', 'size', 'vertical_alignment', 'horizontal_alignment', 'layout_data', 'auto_size'):
                try:
                    row['slot_' + prop] = str(slot.get_editor_property(prop))
                except Exception:
                    pass
        rows.append(row)
    result[key] = sorted(rows, key=lambda row: row['name'])
out = Path(unreal.Paths.project_saved_dir()) / 'PackagingUI' / 'inspection.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
unreal.log('FF_PACKAGING_UI_INSPECT_PASS: ' + str(out))
