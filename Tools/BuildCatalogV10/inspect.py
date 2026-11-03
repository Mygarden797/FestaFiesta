"""Read-only preflight for the approved construction catalog migration."""
from pathlib import Path
import json
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/BuildCatalogV10'
BASE = '/Game/FestaFiesta/Building/FF_WBP_Building/'
PACKAGES = [BASE + p for p in ('Catalog/WBP_FF_PrepPanelBuild', 'Catalog/WBP_FF_PrepBuildTile',
                               'Controls/WBP_FF_BuildTools', 'Facility/WBP_FF_FacilityInfoPanel',
                               'Facility/WBP_FF_FacilityLightingControls',
                               'Facility/WBP_FF_FacilitySynergyEntry')]

def read(obj, prop):
    try:
        value = obj.get_editor_property(prop)
        if isinstance(value, (str, bool, int, float)):
            return value
        if isinstance(value, unreal.Object):
            return value.get_path_name()
        if hasattr(value, 'export_text'):
            return value.export_text()
        return str(value)
    except Exception:
        return None

report = {'engine': unreal.SystemLibrary.get_engine_version(), 'packages': {}, 'services': {}}
for cls in ('WidgetService', 'WidgetLayoutService', 'BlueprintService'):
    service = getattr(unreal, cls, None)
    report['services'][cls] = [name for name in dir(service) if not name.startswith('_')] if service else []
for package in PACKAGES:
    bp = unreal.load_asset(package)
    assert bp, package
    cdo = unreal.get_default_object(bp.generated_class())
    ws = [w for w in unreal.ObjectIterator(unreal.Widget)
          if w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree'
          and w.get_outer().get_outer() == bp]
    rows = []
    for widget in ws:
        row = dict(name=widget.get_name(), cls=widget.get_class().get_name(),
                   parent=widget.get_parent().get_name() if widget.get_parent() else None,
                   visibility=str(widget.get_visibility()))
        if isinstance(widget, unreal.TextBlock):
            row['text'] = str(widget.get_text())
        properties = ('width_override', 'height_override', 'min_desired_width', 'min_desired_height',
                      'padding', 'background', 'brush', 'render_opacity', 'hint_text', 'icon_size',
                      'label', 'choice_id', 'horizontal_entry_spacing', 'vertical_entry_spacing',
                      'entry_width', 'entry_height', 'orientation')
        row['props'] = {p: v for p in properties if (v := read(widget, p)) is not None}
        if widget.slot:
            row['slot'] = {'cls': widget.slot.get_class().get_name()}
            for p in ('padding', 'size', 'offsets', 'anchors', 'alignment', 'row', 'column',
                      'horizontal_alignment', 'vertical_alignment'):
                value = read(widget.slot, p)
                if value is not None:
                    row['slot'][p] = value
        rows.append(row)
    properties = ('cards_per_page', 'visible_catalog_rows', 'use_pagination', 'use_designer_catalog_layout',
                  'fit_catalog_to_viewport', 'catalog_height', 'catalog_offset_y', 'catalog_viewport_margin',
                  'category_choice_class', 'build_inspector_width', 'build_inspector_height',
                  'build_inspector_minimum_bottom_inset', 'build_thumbnail_width', 'build_thumbnail_height')
    report['packages'][package] = {'widgets': rows,
        'defaults': {p: v for p in properties if (v := read(cdo, p)) is not None}}
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'preflight.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('FF_CATALOG_V10_PREFLIGHT_PASS ' + str(OUT / 'preflight.json'))
