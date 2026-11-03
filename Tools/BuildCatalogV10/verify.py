"""Read saved V10 inspector assets in a fresh process; never mutates an asset."""
from pathlib import Path
import json
import traceback
import unreal as u

ROOT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
OUT = ROOT / 'Saved/BuildCatalogV10'
BASE = '/Game/FestaFiesta/Building/FF_WBP_Building/'
REPORT = {'status': 'RUNNING', 'checks': [], 'assets': {}}


def check(name, result, value=None):
    REPORT['checks'].append({'name': name, 'passed': bool(result), 'value': value})
    assert result, name + ': ' + str(value)


def load(suffix):
    package = BASE + suffix
    bp = u.load_asset(package)
    assert bp and bp.generated_class(), package
    w = {x.get_name(): x for x in u.ObjectIterator(u.Widget) if x.get_outer()
         and x.get_outer().get_class().get_name() == 'WidgetTree' and x.get_outer().get_outer() == bp}
    REPORT['assets'][package] = {'widget_count': len(w)}
    return bp, w, u.get_default_object(bp.generated_class())


def reaches(widget, boundary, honor_visibility=True):
    while widget and widget != boundary:
        if honor_visibility and widget.get_visibility() in (u.SlateVisibility.COLLAPSED, u.SlateVisibility.HIDDEN):
            return False
        widget = widget.get_parent()
    return widget == boundary


def no_scroll_ancestor(widget, boundary):
    while widget and widget != boundary:
        if isinstance(widget, u.ScrollBox):
            return False
        widget = widget.get_parent()
    return widget == boundary


try:
    _, placement, _ = load('Controls/WBP_FF_BuildTools')
    check('placement inspector400x300',
          placement['Size_PlacementInfo'].get_editor_property('width_override') == 400
          and placement['Size_PlacementInfo'].get_editor_property('height_override') == 300)
    for name, minimum in (('Text_FacilityName', 22), ('Text_Cost', 28), ('Text_Footprint', 20),
                          ('Text_V10PlacementCategory', 16), ('Text_V10PlacementStatus', 16)):
        actual = placement[name].get_editor_property('font').get_editor_property('size')
        check('placement readable type ' + name, actual >= minimum, actual)
    check('placement identity preview80x58',
          placement['Size_PlacementFacilityThumb'].get_editor_property('width_override') == 80
          and placement['Size_PlacementFacilityThumb'].get_editor_property('height_override') == 58)
    for name in ('Text_FacilityName', 'Text_Cost', 'Text_Footprint', 'Text_V10PlacementStatus'):
        check('placement facts visible without scrolling ' + name,
              reaches(placement[name], placement['Panel_V9PlacementFacts'])
              and no_scroll_ancestor(placement[name], placement['Panel_V9PlacementFacts']))
    for name in ('Button_DirectAdjustment', 'Button_V9PlacementInfo', 'Button_V9PlacementSynergy'):
        check('placement action retained ' + name, name in placement)
    _, info, cdo = load('Facility/WBP_FF_FacilityInfoPanel')
    check('build inspector400x300', cdo.get_editor_property('build_inspector_width') == 400
          and cdo.get_editor_property('build_inspector_height') == 300)
    check('live authored outer dimensions preserved550x630',
          info['Panel_Size'].get_editor_property('width_override') == 550
          and info['Panel_Size'].get_editor_property('height_override') == 630)
    check('unsupported build tabs are not kept disabled', not cdo.get_editor_property('show_unavailable_build_tabs'))
    check('installed info uses direct non-scroll page surface',
          info['Panel_Content'].get_parent() == info['Body_Surface']
          and isinstance(info['Overlay_V10InfoPages'], u.Overlay))
    for name in ('Panel_BuildSection', 'Panel_LiveSection', 'Panel_RevenueSection', 'Panel_StaffSection',
                 'Panel_FacilityQueueSettings', 'Panel_UpgradeSection', 'Panel_SynergySection',
                 'Panel_ProgramSection', 'Widget_LightingControls', 'Panel_RemoveConfirmation'):
        check('page reachable without scrolling ' + name,
              no_scroll_ancestor(info[name], info['Overlay_V10InfoPages']))
    for name, minimum in (('Text_FacilityName', 22), ('Text_InstallCost', 24), ('Text_Footprint', 18),
                          ('V10_Text_StaffCount', 28), ('V10_Text_StaffHint', 16)):
        actual = info[name].get_editor_property('font').get_editor_property('size')
        check('installed readable type ' + name, actual >= minimum, actual)
    check('identity close and favorite arranged horizontally', isinstance(info['V10_IdentityTools'], u.HorizontalBox))
    for name in ('Button_TabOverview', 'Button_TabOperation', 'Button_TabUpgrade', 'Button_TabSynergy',
                 'Button_TabLighting', 'Button_TabRevenue', 'Button_TabPrograms', 'Button_TabStaff', 'Button_TabQueue'):
        check('facility feature action retained ' + name, name in info)
    _, lighting, light_cdo = load('Facility/WBP_FF_FacilityLightingControls')
    check('full drag color picker retained', isinstance(lighting['ColorPicker_Lighting'], u.FF_FacilityColorPicker))
    check('drag picker remains reachable in basic mode', reaches(lighting['ColorPicker_Lighting'], lighting['Panel_LightingBasic']))
    check('color picker reduced to one third32square', lighting['Aurora_ColorPickerSize'].get_editor_property('width_override') == 32
          and lighting['Aurora_ColorPickerSize'].get_editor_property('height_override') == 32
          and light_cdo.get_editor_property('build_color_wheel_size') == 32)
    for channel in ('H', 'S', 'V', 'R', 'G', 'B'):
        check('precision channel retained and reachable ' + channel,
              reaches(lighting['Input_Lighting' + channel], lighting['Panel_LightingPrecision']))
    for name in ('Button_LightingToggle', 'Slider_LightingBrightness', 'Input_LightingColor',
                 'Button_LightingOriginal', 'Button_LightingTeal', 'Button_LightingSunset', 'Button_LightingViolet',
                 'Button_LightingPrecision', 'Button_LightingObserve', 'Button_LightingReset',
                 'Button_LightingCancel', 'Button_LightingApply'):
        check('lighting input reachable without scrolling ' + name,
              no_scroll_ancestor(lighting[name], lighting['Panel_V10Lighting']))
    _, synergy, _ = load('Facility/WBP_FF_FacilitySynergyEntry')
    for name, minimum in (('Text_Title', 16), ('Text_Partner', 14), ('Text_Effect', 16), ('Text_Condition', 14), ('Text_Status', 14)):
        actual = synergy[name].get_editor_property('font').get_editor_property('size')
        check('synergy row readable ' + name, actual >= minimum, actual)
    check('synergy partner navigation retained', reaches(synergy['Button_OpenPartnerCatalog'], synergy['Vertical_Entry']))
    _, hud, _ = load('Shared/WBP_FF_PrepHudBar')
    hud_gold_resources = {x.get_editor_property('brush').get_editor_property('resource_object').get_path_name()
                          for x in hud.values() if isinstance(x, u.Image)
                          and x.get_editor_property('brush').get_editor_property('resource_object')
                          and 'Gold' in x.get_editor_property('brush').get_editor_property('resource_object').get_path_name()}
    check('actual bottom HUD gold brush available', bool(hud_gold_resources), sorted(hud_gold_resources))
    for tree, name in ((placement, 'Text_Cost'), (info, 'Text_InstallCost'), (info, 'Text_RemoveInstallCost')):
        image = tree['V10_Gold_' + name]
        resource = image.get_editor_property('brush').get_editor_property('resource_object')
        check('installation cost uses exact bottom HUD gold resource ' + name,
              bool(resource) and resource.get_path_name() in hud_gold_resources, resource.get_path_name() if resource else None)
    check('team revenue forecast remains visible in its dedicated revenue page',
          reaches(info['Text_FacilityRevenue'], info['Panel_RevenueSection']))
    check('lighting overview action remains structurally reachable when supported',
          reaches(info['Button_LightingColorHelp'], info['Panel_BuildSection'], False))
    check('lighting first-use guide remains reachable without scrolling',
          no_scroll_ancestor(info['Button_DismissLightingIntroduction'], info['Panel_LightingIntroduction']))
    before = json.loads((OUT / 'preflight.json').read_text(encoding='utf-8-sig'))
    for package, row in before['packages'].items():
        _, current, _ = load(package.removeprefix(BASE))
        missing = [x['name'] for x in row['widgets'] if x['name'] not in current]
        check('all original widget binding names retained ' + package, not missing, missing)
    # The catalog and cards are not migration targets; read their key agreement too.
    _, catalog, catalog_cdo = load('Catalog/WBP_FF_PrepPanelBuild')
    check('twelve catalog columns unchanged', catalog_cdo.get_editor_property('cards_per_page') == 12)
    check('small170 search unchanged', catalog['Size_PearlSearch'].get_editor_property('width_override') == 170)
    _, card, _ = load('Catalog/WBP_FF_PrepBuildTile')
    check('facility thumbnail96x80 unchanged', card['SizeBox_Thumbnail'].get_editor_property('width_override') == 96
          and card['SizeBox_Thumbnail'].get_editor_property('height_override') == 80)
    REPORT['status'] = 'PASS'
except Exception:
    REPORT.update(status='FAIL', error=traceback.format_exc())
    u.log_error(REPORT['error'])
finally:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'verify.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    u.log('FF_CATALOG_V10_VERIFY_' + REPORT['status'])
if REPORT['status'] != 'PASS':
    raise RuntimeError(REPORT['error'])
