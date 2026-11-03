"""Independent saved-asset readback for the approved catalog; never mutates assets."""
from pathlib import Path
import json
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/BuildCatalogV9'
BASE = '/Game/FestaFiesta/Building/FF_WBP_Building/'
REPORT = {'status': 'RUNNING', 'checks': [], 'assets': {}}

def check(name, condition, value=None):
    REPORT['checks'].append({'name': name, 'passed': bool(condition), 'value': value})
    assert condition, name + ': ' + str(value)

def load(suffix):
    package = BASE + suffix
    bp = unreal.load_asset(package)
    assert bp and bp.generated_class(), package
    widgets = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
               if w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree'
               and w.get_outer().get_outer() == bp}
    REPORT['assets'][package] = {'widget_count': len(widgets)}
    return bp, widgets, unreal.get_default_object(bp.generated_class())

def visible(widget):
    while widget:
        if widget.get_visibility() in (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN):
            return False
        widget = widget.get_parent()
    return True

def reachable_when_open(widget, popup):
    """The popup may be closed; all descendants and its own ancestry must be active."""
    while widget and widget != popup:
        if widget.get_visibility() in (unreal.SlateVisibility.COLLAPSED, unreal.SlateVisibility.HIDDEN):
            return False
        widget = widget.get_parent()
    return widget == popup and visible(popup.get_parent())

try:
    bp, w, cdo = load('Catalog/WBP_FF_PrepPanelBuild')
    for prop, expected in (('cards_per_page', 12), ('visible_catalog_rows', 2),
                           ('use_pagination', False), ('use_designer_catalog_layout', True),
                           ('integrate_catalog_inspector', True)):
        value = cdo.get_editor_property(prop)
        check(prop, value == expected, value)
    for prop, expected in (('catalog_inspector_width', 400), ('catalog_inspector_gap', 0)):
        value = float(cdo.get_editor_property(prop))
        check(prop, abs(value - expected) < .1, value)
    width = float(w['Size_PearlSearch'].get_editor_property('width_override'))
    check('search width is one-third mockup width in logical units', abs(width - 170) < .1, width)
    choices = [w[name] for name in ['Choice_CategoryAll'] + ['Choice_Category' + str(i) for i in range(10)]]
    check('all eleven category buttons retained', len(choices) == 11)
    check('all major categories icon-only', all(x.get_editor_property('icon_only') for x in choices))
    check('categories in two-column grid', all(isinstance(x.slot, unreal.UniformGridSlot) for x in choices))
    positions = [[x.slot.get_editor_property('row'), x.slot.get_editor_property('column')] for x in choices]
    check('unique two-column positions', len({tuple(p) for p in positions}) == 11
          and max(p[1] for p in positions) == 1 and max(p[0] for p in positions) == 5, positions)
    if 'Text_A5CatalogHeading' in w:
        check('catalog title hidden', not visible(w['Text_A5CatalogHeading']))
    visible_titles = [str(x.get_text()) for x in w.values() if isinstance(x, unreal.TextBlock)
                      and visible(x) and ('시설 카탈로그' in str(x.get_text()) or '114종' in str(x.get_text()))]
    check('no visible title or facility-count caption', not visible_titles, visible_titles)
    check('filter popup initially closed', not visible(w['Popup_V9Filters']))
    check('more popup initially closed', not visible(w['Popup_V9More']))
    check('minor choices retain the native horizontal scroll host',
          w['HorizontalBox_Subcategories'].get_parent() == w['ScrollBox_Subcategories'])
    check('minor choices are reachable when the filter popup opens',
          reachable_when_open(w['HorizontalBox_Subcategories'], w['Popup_V9Filters']))
    check('minor categories scroll horizontally',
          w['ScrollBox_Subcategories'].get_editor_property('orientation') == unreal.Orientation.ORIENT_HORIZONTAL)
    minor_size = w['Size_V9SubcategoryViewport']
    check('minor category viewport stays compact',
          abs(minor_size.get_editor_property('width_override') - 244) < .1
          and abs(minor_size.get_editor_property('height_override') - 36) < .1)
    check('filter content is vertically scrollable within the dock',
          isinstance(w['Scroll_V9Filters'], unreal.ScrollBox)
          and w['Scroll_V9Filters'].get_editor_property('orientation') == unreal.Orientation.ORIENT_VERTICAL
          and abs(w['Size_V9FilterPopup'].get_editor_property('max_desired_height') - 228) < .1)
    for name in ('Check_FilterRequired', 'Check_FilterAffordable', 'Check_FilterPrograms',
                 'Check_FilterLighting', 'ComboBox_CatalogSort', 'Button_V9Filters', 'Button_V9More'):
        check('retained action ' + name, name in w)
    check('catalog content remains fully opaque', abs(float(w['Size_ClarityCatalogDock'].get_editor_property('render_opacity')) - 1) < .001)
    surface = w['Border_ApprovedCatalogV9'].get_editor_property('background')
    tint = surface.get_editor_property('tint_color').get_editor_property('specified_color')
    check('only background is slightly translucent', .85 <= tint.a <= .95, tint.a)
    _, card, card_cdo = load('Catalog/WBP_FF_PrepBuildTile')
    check('thumbnail-only card mode', bool(card_cdo.get_editor_property('thumbnail_only')))
    for name in ('Text_Name', 'Text_Cost'):
        check('hidden card caption ' + name, not visible(card[name]))
    check('thumbnail is available', visible(card['Image_Thumbnail']))
    check('thumbnail frame matches mockup silhouette scale',
          abs(card['SizeBox_Thumbnail'].get_editor_property('width_override') - 96) < .1
          and abs(card['SizeBox_Thumbnail'].get_editor_property('height_override') - 80) < .1)
    check('facility images are vertically centered in their cards',
          card['VerticalBox_Content'].slot.get_editor_property('vertical_alignment') == unreal.VerticalAlignment.V_ALIGN_CENTER)
    check('favorite action retained for hover/focus', 'Button_Favorite' in card)
    _, placement, _ = load('Controls/WBP_FF_BuildTools')
    check('placement inspector width', abs(placement['Size_PlacementInfo'].get_editor_property('width_override') - 400) < .1)
    for name in ('Text_FacilityName', 'Text_Cost', 'Text_Footprint', 'Button_DirectAdjustment'):
        check('placement binding retained ' + name, name in placement)
    _, inspector, inspector_cdo = load('Facility/WBP_FF_FacilityInfoPanel')
    check('build inspector width', abs(inspector_cdo.get_editor_property('build_inspector_width') - 400) < .1)
    for name in ('Button_TabOverview', 'Button_TabOperation', 'Button_TabUpgrade',
                 'Button_TabSynergy', 'Button_TabLighting', 'Button_TabRevenue',
                 'Button_TabPrograms', 'Button_TabStaff', 'Button_TabQueue'):
        check('placed-facility page retained ' + name, name in inspector)
    before = json.loads((OUT / 'preflight.json').read_text(encoding='utf-8'))
    for package, row in before['packages'].items():
        _, current, _ = load(package.removeprefix(BASE))
        missing = [x['name'] for x in row['widgets'] if x['name'] not in current]
        check('all original widget binding names retained ' + package, not missing, missing)
    REPORT['status'] = 'PASS'
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    unreal.log_error(REPORT['error'])
finally:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'verify.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('FF_CATALOG_V9_VERIFY_' + REPORT['status'])
if REPORT['status'] != 'PASS':
    raise RuntimeError(REPORT['error'])
