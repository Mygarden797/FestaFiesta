"""Apply approved V10 readability to the existing inspector assets only.

The editor must be freshly built and idle. Every original named widget is retained,
and the migration saves backups before authoring. Gameplay data is never edited.
"""
from datetime import datetime, timezone
from pathlib import Path
import importlib.util
import json
import shutil
import traceback
import unreal as u

ROOT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
OUT = ROOT / 'Saved/BuildCatalogV10'
RUN = OUT / datetime.now(timezone.utc).strftime('apply-%Y%m%d-%H%M%S-%f')
BASE = '/Game/FestaFiesta/Building/FF_WBP_Building/'
TOOLS = BASE + 'Controls/WBP_FF_BuildTools'
INFO = BASE + 'Facility/WBP_FF_FacilityInfoPanel'
LIGHT = BASE + 'Facility/WBP_FF_FacilityLightingControls'
SYNERGY = BASE + 'Facility/WBP_FF_FacilitySynergyEntry'
PACKAGES = (TOOLS, INFO, LIGHT, SYNERGY)
spec = importlib.util.spec_from_file_location('ff_catalog_v9_helpers', ROOT / 'Tools/BuildCatalogV9/apply.py')
v = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)
v.PACKAGES = PACKAGES
REPORT = {'status': 'RUNNING', 'packages': list(PACKAGES), 'saved': [], 'backup': str(RUN / 'Backup')}
VISIBLE, COLLAPSED = v.VISIBLE, v.COLLAPSED
widgets, node, add, bounds, margin, surface, clear_surface, font, label = (
    v.widgets, v.node, v.add, v.bounds, v.margin, v.surface, v.clear_surface, v.font, v.label)


def content(parent, child, padding=(0, 0, 0, 0)):
    child.remove_from_parent()
    parent.set_content(child)
    if child.slot and hasattr(child.slot, 'set_padding'):
        child.slot.set_padding(margin(padding))


def fresh(w, cls, name):
    obj = node(w, cls, name)
    if isinstance(obj, u.PanelWidget):
        obj.clear_children()
    obj.set_visibility(VISIBLE)
    return obj


def column(w, parent, name, expand=False, padding=(0, 0, 0, 0)):
    obj = fresh(w, u.VerticalBox, name)
    add(parent, obj, expand, padding)
    return obj


def row(w, parent, name, expand=False, padding=(0, 0, 0, 0)):
    obj = fresh(w, u.HorizontalBox, name)
    add(parent, obj, expand, padding)
    return obj


def text(w, name, caption, size=16, tint=v.INK, wrap=False):
    result = label(w, name, caption, size, tint)
    result.set_auto_wrap_text(wrap)
    return result


def restyle_button(button, caption=None, size=16, primary=False):
    v.button_style(button, primary)
    style = button.get_editor_property('widget_style')
    style.set_editor_property('normal_padding', margin((5, 2, 5, 2)))
    style.set_editor_property('pressed_padding', margin((5, 2, 5, 2)))
    button.set_editor_property('widget_style', style)
    child = button.get_content()
    if isinstance(child, u.TextBlock):
        if caption is not None:
            child.set_text(caption)
        font(child, size, v.TEAL if primary else v.INK)
        child.set_auto_wrap_text(False)
        child.set_editor_property('justification', u.TextJustify.CENTER)


def button_padding(button, padding=(0, 0, 0, 0)):
    style = button.get_editor_property('widget_style')
    style.set_editor_property('normal_padding', margin(padding))
    style.set_editor_property('pressed_padding', margin(padding))
    button.set_editor_property('widget_style', style)
    child = button.get_content()
    if child and child.slot:
        # UButtonSlot carries separate authored padding; clearing style padding alone
        # does not remove this second inset from glyph/preset buttons.
        child.slot.set_padding(margin())
        child.slot.set_horizontal_alignment(u.HorizontalAlignment.H_ALIGN_CENTER)
        child.slot.set_vertical_alignment(u.VerticalAlignment.V_ALIGN_CENTER)


def gold_brush():
    """Copy the actual bottom-HUD gold brush, including its existing resource."""
    hud = u.load_asset(BASE + 'Shared/WBP_FF_PrepHudBar')
    assert hud and hud.generated_class(), 'Bottom HUD gold source unavailable'
    matches = []
    for widget in u.ObjectIterator(u.Image):
        if not widget.get_outer() or widget.get_outer().get_class().get_name() != 'WidgetTree' or widget.get_outer().get_outer() != hud:
            continue
        brush = widget.get_editor_property('brush')
        resource = brush.get_editor_property('resource_object')
        if resource and 'Gold' in resource.get_path_name():
            matches.append((widget, brush, resource))
    assert matches, 'Bottom HUD has no gold-image brush'
    widget, brush, resource = matches[0]
    REPORT['gold_icon_source'] = {'widget': widget.get_name(), 'resource': resource.get_path_name()}
    return brush


def button_box(w, parent, name, height=30, caption=None, expand=True, padding=(0, 0, 0, 0), size=16):
    button = w[name]
    restyle_button(button, caption, size)
    box = node(w, u.SizeBox, 'V10_Size_' + name)
    bounds(box, None, height)
    content(box, button)
    add(parent, box, expand, padding)
    return box


def metric(w, parent, value_name, caption, value_size=22, height=58, suffix=''):
    card_name = 'V10_Card_' + value_name + suffix
    card = node(w, u.Border, card_name)
    values = fresh(w, u.VerticalBox, card_name + '_Values')
    content(card, values, (9, 5, 9, 5))
    surface(card, .96, padding=(9, 5, 9, 5), fill=v.PEARL)
    add(values, text(w, card_name + '_Label', caption, 14, v.MUTED), False, (0, 0, 0, 3))
    value = w[value_name]
    font(value, value_size, v.TEAL if 'Cost' in value_name or 'Revenue' in value_name else v.INK)
    value.set_auto_wrap_text(False)
    if value_name in ('Text_Cost', 'Text_InstallCost', 'Text_RemoveInstallCost'):
        amount = row(w, values, 'V10_Amount_' + value_name)
        icon = node(w, u.Image, 'V10_Gold_' + value_name)
        icon.set_brush(gold_brush())
        icon.set_color_and_opacity(u.LinearColor(1, 1, 1, 1))
        icon.set_desired_size_override(u.Vector2D(22, 22))
        icon_size = node(w, u.SizeBox, 'V10_Gold_' + value_name + '_Size')
        bounds(icon_size, 22, 22)
        content(icon_size, icon)
        icon_slot = add(amount, icon_size, False, (0, 0, 5, 0))
        icon_slot.set_vertical_alignment(u.VerticalAlignment.V_ALIGN_CENTER)
        add(amount, value)
    else:
        add(values, value)
    size = node(w, u.SizeBox, 'V10_Row_' + value_name + suffix)
    bounds(size, None, height)
    content(size, card)
    slot = add(parent, size, True, (0, 0, 6, 0))
    if value_name in ('Text_Cost', 'Text_InstallCost', 'Text_Footprint'):
        share = slot.get_editor_property('size')
        share.set_editor_property('value', 1.0)
        slot.set_editor_property('size', share)
    return card


def fact(w, parent, value_name, caption, value_size=16):
    item = row(w, parent, 'V10_Row_' + value_name, False, (0, 1, 0, 1))
    add(item, text(w, 'V10_Label_' + value_name, caption, 14, v.MUTED), True, (0, 0, 8, 0))
    value = w[value_name]
    font(value, value_size)
    value.set_auto_wrap_text(False)
    add(item, value)
    return item


def orphan_archive(w, root, host, name):
    archive = node(w, u.VerticalBox, name)
    archive.set_visibility(COLLAPSED)
    for widget in list(w.values()):
        if widget not in (root, archive) and widget.get_parent() is None:
            add(archive, widget)
    if archive.get_parent() is None:
        add(host, archive)
    return archive


def configure_placement():
    bp, w = widgets(TOOLS)
    shell = fresh(w, u.VerticalBox, 'Column_PearlSummaryContent')
    surface(w['Border_PlacementInfo'], .9, (0, 12, 12, 0), (12, 10, 12, 10))
    shell.slot.set_padding(margin((12, 10, 12, 10)))
    bounds(w['Size_PlacementInfo'], 400, 300)
    tabs = fresh(w, u.HorizontalBox, 'Row_V9PlacementTabs')
    for name, caption in (('Button_V9PlacementInfo', '설치 정보'), ('Button_V9PlacementSynergy', '시설 조합')):
        button_box(w, tabs, name, 28, caption, True, (0, 0, 4, 0), 16)
    add(shell, tabs, False, (0, 0, 0, 7))
    pages = fresh(w, u.Overlay, 'Overlay_V9PlacementPages')
    facts = fresh(w, u.VerticalBox, 'Panel_V9PlacementFacts')
    add(pages, facts)
    identity = row(w, facts, 'V10_PlacementIdentity', False, (0, 0, 0, 6))
    bounds(w['Size_PlacementFacilityThumb'], 80, 58)
    surface(w['Border_PlacementFacilityThumb'], 0, padding=(0, 0, 0, 0), width=0)
    add(identity, w['Size_PlacementFacilityThumb'], False, (0, 0, 10, 0))
    identity_text = column(w, identity, 'V10_PlacementIdentityText', True)
    font(w['Text_FacilityName'], 22)
    w['Text_FacilityName'].set_auto_wrap_text(True)
    add(identity_text, w['Text_FacilityName'], False, (0, 3, 0, 3))
    add(identity_text, text(w, 'Text_V10PlacementCategory', '', 16, v.MUTED))
    metrics = row(w, facts, 'V10_PlacementMetrics')
    metric(w, metrics, 'Text_Cost', '설치 비용', 28, 66)
    metric(w, metrics, 'Text_Footprint', '시설 크기', 20, 66)
    status = node(w, u.Border, 'Border_V10PlacementStatus')
    status_text = text(w, 'Text_V10PlacementStatus', '배치 가능', 16, v.TEAL)
    status_text.set_editor_property('justification', u.TextJustify.CENTER)
    content(status, status_text, (12, 4, 12, 4))
    surface(status, .96, padding=(12, 4, 12, 4), fill='DDF5E8', width=0)
    status_slot = add(facts, status, False, (0, 7, 0, 0))
    status_slot.set_horizontal_alignment(u.HorizontalAlignment.H_ALIGN_CENTER)
    synergy = fresh(w, u.VerticalBox, 'Panel_V9PlacementSynergy')
    surface(w['Border_PlacementSynergy'], .96, padding=(6, 5, 6, 5))
    add(synergy, w['Border_PlacementSynergy'])
    add(pages, synergy)
    synergy.set_visibility(COLLAPSED)
    add(shell, pages, True)
    for name in ('Text_PlacementObstruction', 'Text_AssistanceMessage'):
        font(w[name], 14, v.MUTED)
        w[name].set_auto_wrap_text(True)
        add(shell, w[name])
    button_box(w, shell, 'Button_DirectAdjustment', 32, '직접 조정', False, (0, 5, 0, 0), 18)
    for name in ('Text_Legend', 'Text_PlacementAdvice', 'Row_PlacementAssistance'):
        w[name].set_visibility(COLLAPSED)
    orphan_archive(w, w['Canvas_BuildClarity'], shell, 'Panel_V10PlacementRetired')
    return bp


def configure_info():
    bp, w = widgets(INFO)
    shell = fresh(w, u.VerticalBox, 'Panel_Shell')
    surface(w['Border_Panel'], .9, (0, 12, 12, 0), (12, 10, 12, 10))
    shell.slot.set_padding(margin((12, 10, 12, 10)))
    # Keep the ordinary/live authored outer size. Build's400x300 is a native CDO override.
    bounds(w['Panel_Size'], 550, 630)
    identity_surface = w['Panel_PersistentIdentity']
    identity = fresh(w, u.HorizontalBox, 'V2_IdentityRow')
    content(identity_surface, identity)
    clear_surface(identity_surface)
    bounds(w['Thumbnail_Size'], 56, 48)
    clear_surface(w['Thumbnail_Background'])
    add(identity, w['Thumbnail_Size'], False, (0, 0, 8, 0))
    meta = fresh(w, u.VerticalBox, 'V2_IdentityMeta')
    font(w['Text_Category'], 14, v.MUTED)
    font(w['Text_FacilityName'], 22)
    w['Text_FacilityName'].set_auto_wrap_text(True)
    add(meta, w['Text_Category'])
    add(meta, w['Text_FacilityName'])
    add(identity, meta, True)
    tools = row(w, identity, 'V10_IdentityTools', False, (5, 0, 0, 0))
    for name in ('Button_Favorite', 'Button_Close'):
        box = button_box(w, tools, name, 28, None, False, size=16)
        bounds(box, 28, 28)
        button_padding(w[name])
    add(shell, identity_surface, False, (0, 0, 0, 5))
    nav = fresh(w, u.VerticalBox, 'Panel_InspectorNavigation')
    grid = w['Grid_InspectorTabs']
    grid.set_min_desired_slot_width(64)
    grid.set_min_desired_slot_height(26)
    grid.set_slot_padding(margin((2, 1, 2, 1)))
    for widget in w.values():
        if isinstance(widget, u.Button) and widget.get_name().startswith('Button_Tab'):
            restyle_button(widget, '기본' if widget.get_name() == 'Button_TabOverview' else None, 14)
            button_padding(widget)
    add(nav, grid)
    add(shell, nav, False, (0, 0, 0, 5))
    body = w['Body_Surface']
    clear_surface(body)
    panel_content = fresh(w, u.VerticalBox, 'Panel_Content')
    content(body, panel_content)
    pages = fresh(w, u.Overlay, 'Overlay_V10InfoPages')
    add(panel_content, pages, True)
    add(shell, body, True)
    for section in ('Panel_BuildSection', 'Panel_LiveSection', 'Panel_RevenueSection', 'Panel_StaffSection',
                    'Panel_FacilityQueueSettings', 'Panel_UpgradeSection', 'Panel_SynergySection',
                    'Panel_ProgramSection', 'Widget_LightingControls', 'Panel_RemoveConfirmation',
                    'Panel_LightingIntroduction'):
        add(pages, w[section])
        w[section].set_visibility(COLLAPSED)
    # Overview: cost and size are the dominant facts; capacity/service facts are conditional.
    overview = fresh(w, u.VerticalBox, 'Clarity_BasicFacts')
    content(w['Panel_BuildSection'], overview)
    clear_surface(w['Panel_BuildSection'])
    overview_metrics = row(w, overview, 'V10_OverviewMetrics')
    metric(w, overview_metrics, 'Text_InstallCost', '설치 비용', 24, 70)
    metric(w, overview_metrics, 'Text_Footprint', '시설 크기', 18, 70)
    fact(w, overview, 'Text_FacilityState', '상태', 14)
    capacities = row(w, overview, 'V10_OverviewService')
    for index, (value_name, caption) in enumerate((('Text_ServiceCapacityValue', '동시 이용'), ('Text_ServiceDuration', '팀당 시간'))):
        part = column(w, capacities, 'V10_OverviewService' + str(index), True, (0, 2, 8, 0))
        fact(w, part, value_name, caption, 14)
    fact(w, overview, 'Text_InstanceLimit', '설치 제한', 14)
    # Preserve the real first-use lighting action and its reminder inside the button.
    help_overlay = fresh(w, u.Overlay, 'Overlay_LightingColorHelp')
    restyle_button(w['Button_LightingColorHelp'], '조명 색상·밝기 변경', 14)
    button_padding(w['Button_LightingColorHelp'], (4, 0, 24, 0))
    add(help_overlay, w['Button_LightingColorHelp'])
    bounds(w['Size_LightingIntroductionHint'], 20, 20)
    w['Text_LightingIntroductionHint'].set_text('!')
    font(w['Text_LightingIntroductionHint'], 14, 'FFFFFF')
    w['Text_LightingIntroductionHint'].set_editor_property('justification', u.TextJustify.CENTER)
    surface(w['Border_LightingIntroductionHint'], 1, padding=(0, 0, 0, 0), fill='DD574E', width=0)
    reminder_slot = add(help_overlay, w['Size_LightingIntroductionHint'], False, (0, 2, 2, 0))
    reminder_slot.set_horizontal_alignment(u.HorizontalAlignment.H_ALIGN_RIGHT)
    reminder_slot.set_vertical_alignment(u.VerticalAlignment.V_ALIGN_TOP)
    content(w['Panel_LightingColorHelp'], help_overlay)
    clear_surface(w['Panel_LightingColorHelp'])
    help_size = node(w, u.SizeBox, 'V10_LightingHelpSize')
    bounds(help_size, None, 26)
    content(help_size, w['Panel_LightingColorHelp'])
    add(overview, help_size, False, (0, 2, 0, 0))
    # Operation details belong to their own short page.
    operation = fresh(w, u.VerticalBox, 'Clarity_OperationFacts')
    content(w['Panel_LiveSection'], operation)
    clear_surface(w['Panel_LiveSection'])
    for value_name, caption in (('Text_Occupancy', '대기·처리 중 팀'), ('Text_RemainingService', '남은 서비스 팀'),
                                ('Text_ExpectedWait', '예상 대기'), ('Text_Throughput', '팀당 처리')):
        fact(w, operation, value_name, caption)
    # Revenue: concise2x2 values and the actual status/shortcut, no repeated paragraphs.
    revenue = fresh(w, u.VerticalBox, 'Panel_RevenueSection')
    quote = w['TeamRevenue_OverviewCard']
    font(w['Text_FacilityRevenue'], 14, v.TEAL)
    w['Text_FacilityRevenue'].set_auto_wrap_text(False)
    content(quote, w['Text_FacilityRevenue'], (3, 0, 3, 0))
    surface(quote, .65, padding=(3, 0, 3, 0), fill=v.SELECTED, width=0)
    add(revenue, quote, False, (0, 0, 0, 2))
    metrics = row(w, revenue, 'V10_RevenueMetrics0', False, (0, 0, 0, 2))
    for value_name, caption in (('Text_FacilityTodayRevenue', '오늘 매출'), ('Text_FacilityTotalRevenue', '축제 누적')):
        metric(w, metrics, value_name, caption, 18, 62)
    secondary = row(w, revenue, 'V10_RevenueSecondaryFacts', False, (0, 0, 0, 2))
    for value_name, caption in (('Text_FacilityRevenueCount', '결제 건수'), ('Text_FacilityRevenueShare', '매출 비중')):
        # Reuse the existing SizeBox/Border names so blank-value rules collapse the complete row.
        compact = fresh(w, u.HorizontalBox, 'V10_Compact_' + value_name)
        short_caption = '결제' if value_name == 'Text_FacilityRevenueCount' else '비중'
        caption_text = text(w, 'V10_Card_' + value_name + '_Label', short_caption, 14, v.MUTED)
        font(caption_text, 14, v.MUTED)
        caption_text.set_visibility(u.SlateVisibility.HIT_TEST_INVISIBLE)
        add(compact, caption_text, False, (0, 0, 4, 0))
        font(w[value_name], 14)
        w[value_name].set_auto_wrap_text(False)
        w[value_name].set_tool_tip_text(caption)
        add(compact, w[value_name])
        card = node(w, u.Border, 'V10_Card_' + value_name)
        content(card, compact)
        clear_surface(card)
        size = node(w, u.SizeBox, 'V10_Row_' + value_name)
        bounds(size, None, 22)
        content(size, card)
        add(secondary, size, True, (0, 0, 4, 0))
    font(w['Text_FacilityRevenueStatus'], 14, v.MUTED)
    w['Text_FacilityRevenueStatus'].set_auto_wrap_text(True)
    add(revenue, w['Text_FacilityRevenueStatus'], False, (0, 2, 0, 2))
    button_box(w, revenue, 'Button_RevenueRanking', 30, '시설별 매출 순위', False, size=14)
    button_padding(w['Button_RevenueRanking'])
    # Staff counts are projected from the real assigned/capacity fields by native code.
    staff = fresh(w, u.VerticalBox, 'Panel_StaffSection')
    add(staff, text(w, 'V10_StaffHeading', '스태프 배정', 16, v.TEAL), False, (0, 0, 0, 4))
    staff_metric = row(w, staff, 'V10_StaffMetric', False, (0, 3, 0, 3))
    add(staff_metric, text(w, 'V10_StaffCaption', '배정 인원', 16, v.MUTED), True)
    add(staff_metric, text(w, 'V10_Text_StaffCount', '', 28, v.TEAL))
    add(staff, text(w, 'V10_Text_StaffHint', '', 16, v.MUTED, True), False, (0, 3, 0, 6))
    staff_actions = fresh(w, u.HorizontalBox, 'Panel_StaffActions')
    button_box(w, staff_actions, 'Button_AssignStaff', 32, '스태프 배치', True, (0, 0, 5, 0))
    button_box(w, staff_actions, 'Button_UnassignStaff', 32, '배정 해제')
    add(staff, staff_actions)
    # Queue: all three inputs share one row and Apply remains visible.
    queue = fresh(w, u.VerticalBox, 'Column_QueueSettings')
    content(w['Panel_FacilityQueueSettings'], queue)
    clear_surface(w['Panel_FacilityQueueSettings'])
    inputs = row(w, queue, 'V10_QueueInputs', False, (0, 0, 0, 5))
    for name, caption in (('Editable_FacilityQueueTeams', '정원 (팀)'), ('Editable_FacilityQueueSpacing', '간격 (cm)'),
                          ('Editable_FacilityQueueWidth', '통로 폭 (cm)')):
        part = column(w, inputs, 'V10_' + name + '_Column', True, (0, 0, 6, 0))
        add(part, text(w, 'V10_' + name + '_Caption', caption, 14, v.MUTED), False, (0, 0, 0, 4))
        input_widget = w[name]
        style = input_widget.get_editor_property('widget_style')
        text_style = style.get_editor_property('text_style')
        f = text_style.get_editor_property('font')
        f.set_editor_property('font_object', u.load_asset(v.FONT))
        f.set_editor_property('size', 18)
        text_style.set_editor_property('font', f)
        style.set_editor_property('text_style', text_style)
        input_widget.set_editor_property('widget_style', style)
        size = node(w, u.SizeBox, 'V10_' + name + '_Size')
        bounds(size, None, 30)
        content(size, input_widget)
        add(part, size)
    font(w['Text_FacilityQueueHint'], 14, v.MUTED)
    w['Text_FacilityQueueHint'].set_auto_wrap_text(True)
    add(queue, w['Text_FacilityQueueHint'], False, (0, 0, 0, 5))
    button_box(w, queue, 'Button_ApplyFacilityQueueSettings', 32, '대기열 설정 적용', False)
    # Upgrade: keep the existing authoritative comparison cells, compact to4rows.
    upgrade = fresh(w, u.VerticalBox, 'Panel_UpgradeSection')
    font(w['Text_FacilityLevel'], 14, v.TEAL)
    add(upgrade, w['Text_FacilityLevel'], False, (0, 0, 0, 1))
    comparison = fresh(w, u.VerticalBox, 'Panel_UpgradeMetricRows')
    headings = row(w, comparison, 'V10_UpgradeHeadings')
    heading_space = node(w, u.SizeBox, 'V10_UpgradeHeadingSpace')
    bounds(heading_space, 80, 1)
    add(headings, heading_space)
    for name, caption in (('V10_UpgradeCurrentHeading', '현재'), ('V10_UpgradeNextHeading', '강화 후')):
        add(headings, text(w, name, caption, 14, v.MUTED), True, (3, 0, 0, 0))
    for key, caption in (('Revenue', '1인 수익'), ('Time', '팀당 시간'), ('Throughput', '처리 속도'), ('Size', '시설 크기')):
        line = fresh(w, u.HorizontalBox, 'V2_UpgradeRow' + key)
        caption_size = node(w, u.SizeBox, 'V10_UpgradeCaption_' + key)
        bounds(caption_size, 80, None)
        content(caption_size, text(w, 'V2_UpgradeLabel' + key, caption, 14, v.MUTED))
        add(line, caption_size)
        for prefix in ('Current', 'Next'):
            value = w['Text_Upgrade' + prefix + key]
            font(value, 14)
            value.set_auto_wrap_text(False)
            cell = w['Text_Upgrade' + prefix + key + '_Cell']
            content(cell, value, (4, 1, 4, 1))
            surface(cell, .7, padding=(4, 1, 4, 1), width=0)
            add(line, cell, True, (3, 0, 0, 0))
        add(comparison, line)
    add(upgrade, comparison)
    quote = row(w, upgrade, 'V10_UpgradeQuote')
    font(w['Text_UpgradePrice'], 18, v.TEAL)
    w['Text_UpgradePrice'].set_auto_wrap_text(False)
    add(quote, text(w, 'V10_UpgradePriceCaption', '강화 비용', 14, v.MUTED), True)
    add(quote, w['Text_UpgradePrice'])
    font(w['Text_UpgradeReason'], 14, v.MUTED)
    w['Text_UpgradeReason'].set_auto_wrap_text(True)
    add(upgrade, w['Text_UpgradeReason'])
    # Synergy keeps the existing paged entry widget and its navigation intact.
    synergy = fresh(w, u.VerticalBox, 'Panel_SynergySection')
    add(synergy, w['VerticalBox_SynergyEntries'], True)
    pager = w['Clarity_SynergyPager']
    for name in ('Button_SynergyPrevious', 'Button_SynergyNext'):
        restyle_button(w[name], size=14)
    font(w['Text_SynergyPage'], 14, v.MUTED)
    add(synergy, pager, False, (0, 3, 0, 0))
    # Programs: current plan and the real purchase action; no duplicated long help.
    programs = fresh(w, u.VerticalBox, 'Panel_ProgramSection')
    for name, size in (('Text_ProgramStatus', 18), ('Text_ScheduledProgramSummary', 16), ('Text_ProgramScheduleReason', 14)):
        font(w[name], size)
        w[name].set_auto_wrap_text(True)
        add(programs, w[name], False, (0, 0, 0, 5))
    button_box(w, programs, 'Button_ScheduleProgram', 32, '프로그램 편성', False)
    # First-use lighting guide remains readable on one page before entering the live editor.
    intro = fresh(w, u.VerticalBox, 'Vertical_LightingIntroduction')
    content(w['Panel_LightingIntroduction'], intro)
    clear_surface(w['Panel_LightingIntroduction'])
    for name, caption, size in (
        ('Text_LightingIntroductionTitle', '조명 색상과 밝기를 바꿔보세요', 16),
        ('Text_LightingIntroductionPick', '1. 색상 휠을 드래그해 색상을 선택하세요.', 14),
        ('Text_LightingIntroductionPreview', '2. 시설에서 변경 결과를 바로 확인하세요.', 14),
        ('Text_LightingIntroductionApply', '3. 적용하면 유지, 취소하면 이전 상태로 복원합니다.', 14),
        ('Text_LightingIntroductionRestore', '기본값은 시설의 원래 조명으로 되돌립니다.', 14)):
        w[name].set_text(caption)
        font(w[name], size, v.TEAL if size == 16 else v.MUTED)
        w[name].set_auto_wrap_text(True)
        add(intro, w[name], False, (0, 0, 0, 3))
    button_box(w, intro, 'Button_DismissLightingIntroduction', 28, '색상 바꾸기', False, size=16)
    # Remove confirmation gives refund the dominant value, with actions outside the body.
    remove = fresh(w, u.VerticalBox, 'Panel_RemoveConfirmation')
    font(w['Text_RemoveConfirmation'], 18)
    w['Text_RemoveConfirmation'].set_auto_wrap_text(True)
    add(remove, w['Text_RemoveConfirmation'], False, (0, 0, 0, 5))
    remove_metrics = row(w, remove, 'V10_RemoveMetrics')
    metric(w, remove_metrics, 'Text_RemoveInstallCost', '설치 비용', 20, 56)
    metric(w, remove_metrics, 'Text_RemoveRecovery', '회수 금액', 22, 56)
    # The actual nested lighting editor is separately compacted below.
    for name in ('Text_Feedback', 'Text_RelocationWarning'):
        font(w[name], 14, v.MUTED)
        w[name].set_auto_wrap_text(True)
        add(panel_content, w[name])
    actions = w['Panel_Actions']
    clear_surface(actions)
    for name in ('Button_Move', 'Button_Remove'):
        restyle_button(w[name], '이동' if name == 'Button_Move' else '철거', 16)
    bounds_actions = node(w, u.SizeBox, 'V10_OverviewActionsSize')
    bounds(bounds_actions, None, 32)
    content(bounds_actions, w['Panel_BuildActions'])
    content(actions, bounds_actions)
    add(shell, actions, False, (0, 4, 0, 0))
    remove_actions = fresh(w, u.HorizontalBox, 'Panel_RemoveActions')
    for button_name, caption in (('Button_RemoveCancel', '취소'), ('Button_RemoveConfirm', '철거')):
        button_box(w, remove_actions, button_name, 32, caption, True, (0, 0, 4, 0))
    add(shell, remove_actions, False, (0, 4, 0, 0))
    upgrade_actions = w['Panel_UpgradeActions']
    clear_surface(upgrade_actions)
    upgrade_size = node(w, u.SizeBox, 'V10_UpgradeActionSize')
    bounds(upgrade_size, None, 32)
    restyle_button(w['Button_Upgrade'], '시설 강화', 16)
    content(upgrade_size, w['Button_Upgrade'])
    content(upgrade_actions, upgrade_size)
    add(shell, upgrade_actions, False, (0, 4, 0, 0))
    # Old header/scroll wrappers and compatibility rows remain serialized, but never consume room.
    for name in ('Header_Size', 'ScrollBox_Details', 'Text_LevelSummary', 'Border_A5LightingSupport'):
        w[name].set_visibility(COLLAPSED)
    orphan_archive(w, w['Root_Canvas'], shell, 'Panel_V10Retired')
    for name in ('Panel_LiveSection', 'Panel_RevenueSection', 'Panel_StaffSection', 'Panel_FacilityQueueSettings',
                 'Panel_UpgradeSection', 'Panel_SynergySection', 'Panel_ProgramSection', 'Widget_LightingControls',
                 'Panel_RemoveConfirmation', 'Panel_RemoveActions', 'Panel_UpgradeActions', 'Panel_LightingIntroduction'):
        w[name].set_visibility(COLLAPSED)
    w['Panel_BuildSection'].set_visibility(VISIBLE)
    return bp


def configure_lighting():
    bp, w = widgets(LIGHT)
    root = fresh(w, u.VerticalBox, 'Aurora_LightingV7')
    panel = column(w, root, 'Panel_V10Lighting')
    toolbar = row(w, panel, 'V10_LightingToolbar', False, (0, 0, 0, 3))
    for name in ('Button_LightingToggle', 'Button_LightingPrecision', 'Button_LightingObserve'):
        button_box(w, toolbar, name, 28, None, True, (0, 0, 4, 0), 14)
        button_padding(w[name])
    modes = fresh(w, u.Overlay, 'ClarityV2_ColorModes')
    basic = fresh(w, u.HorizontalBox, 'Panel_LightingBasic')
    bounds(w['Aurora_ColorPickerSize'], 32, 32)
    wheel_slot = add(basic, w['Aurora_ColorPickerSize'], False, (0, 0, 8, 0))
    wheel_slot.set_vertical_alignment(u.VerticalAlignment.V_ALIGN_CENTER)
    controls = column(w, basic, 'V10_LightingControls', True)
    brightness = row(w, controls, 'V10_BrightnessHeading')
    add(brightness, text(w, 'V10_BrightnessLabel', '밝기', 14, v.MUTED), True)
    font(w['Text_LightingBrightness'], 18, v.TEAL)
    add(brightness, w['Text_LightingBrightness'])
    slider_size = node(w, u.SizeBox, 'V10_BrightnessSliderSize')
    bounds(slider_size, None, 20)
    content(slider_size, w['Slider_LightingBrightness'])
    add(controls, slider_size)
    presets = row(w, controls, 'V10_LightingPresets', False, (0, 2, 0, 2))
    for name in ('Button_LightingOriginal', 'Button_LightingTeal', 'Button_LightingSunset', 'Button_LightingViolet'):
        button_box(w, presets, name, 28, None, True, (0, 0, 3, 0), 14)
        button_padding(w[name], (1, 0, 1, 0))
    previews = row(w, controls, 'V10_LightingColorFields')
    for name in ('Border_LightingCurrentColor', 'Border_LightingNewColor'):
        size = node(w, u.SizeBox, 'V10_' + name + '_Size')
        bounds(size, 24, 24)
        content(size, w[name])
        add(previews, size, False, (0, 0, 4, 0))
    input_size = node(w, u.SizeBox, 'V10_LightingHexSize')
    bounds(input_size, None, 26)
    content(input_size, w['Input_LightingColor'])
    add(previews, input_size, True)
    precision = fresh(w, u.HorizontalBox, 'Panel_LightingPrecision')
    for index, (title, channels) in enumerate((('HSV', ('H', 'S', 'V')), ('RGB', ('R', 'G', 'B')))):
        channel_column = column(w, precision, 'V10_Channels' + title, True, (0, 0, 8, 0))
        add(channel_column, text(w, 'V10_Channels' + title + '_Title', title, 14, v.TEAL))
        for channel in channels:
            channel_row = row(w, channel_column, 'V10_Channel_' + channel, False, (0, 1, 0, 1))
            add(channel_row, text(w, 'V10_Channel_' + channel + '_Label', channel, 14, v.MUTED), True)
            input_box = node(w, u.SizeBox, 'V10_Channel_' + channel + '_Size')
            bounds(input_box, 75, 22)
            content(input_box, w['Input_Lighting' + channel])
            add(channel_row, input_box)
    add(modes, basic)
    add(modes, precision)
    precision.set_visibility(COLLAPSED)
    mode_size = node(w, u.SizeBox, 'V10_LightingModesSize')
    bounds(mode_size, None, 102)
    content(mode_size, modes)
    add(panel, mode_size)
    actions = row(w, panel, 'V10_LightingActions', False, (0, 3, 0, 0))
    for name in ('Button_LightingReset', 'Button_LightingCancel', 'Button_LightingApply'):
        button_box(w, actions, name, 28, None, True, (0, 0, 4, 0), 14)
        button_padding(w[name])
    # Keep drag picker, all six HSV/RGB fields, HEX and transaction buttons alive.
    for name in ('Input_LightingColor', 'Input_LightingH', 'Input_LightingS', 'Input_LightingV',
                 'Input_LightingR', 'Input_LightingG', 'Input_LightingB'):
        editable = w[name]
        style = editable.get_editor_property('widget_style')
        text_style = style.get_editor_property('text_style')
        f = text_style.get_editor_property('font')
        f.set_editor_property('font_object', u.load_asset(v.FONT))
        f.set_editor_property('size', 14)
        text_style.set_editor_property('font', f)
        style.set_editor_property('text_style', text_style)
        editable.set_editor_property('widget_style', style)
    orphan_archive(w, root, panel, 'Panel_V10LightingRetired')
    return bp


def configure_synergy():
    bp, w = widgets(SYNERGY)
    clear_surface(w['Border_EntrySpacing'])
    surface(w['Border_Entry'], .8, padding=(7, 5, 7, 5), width=.5)
    details = fresh(w, u.VerticalBox, 'Vertical_Entry')
    content(w['Border_Entry'], details, (7, 5, 7, 5))
    font(w['Text_Title'], 16, v.TEAL)
    w['Text_Title'].set_auto_wrap_text(True)
    add(details, w['Text_Title'], False, (0, 0, 0, 3))
    restyle_button(w['Button_OpenPartnerCatalog'], size=14)
    button_box(w, details, 'Button_OpenPartnerCatalog', 28, None, False, (0, 0, 0, 3), 14)
    button_padding(w['Button_OpenPartnerCatalog'], (1, 0, 1, 0))
    for name, size in (('Text_Effect', 16), ('Text_Condition', 14), ('Text_Status', 14)):
        font(w[name], size, v.INK if name == 'Text_Effect' else v.MUTED)
        w[name].set_auto_wrap_text(True)
        add(details, w[name], False, (0, 0, 0, 3))
    orphan_archive(w, w['Border_EntrySpacing'], details, 'Panel_V10SynergyRetired')
    return bp


def defaults():
    values = {
        TOOLS: {'installation_cost_format': '{Cost}', 'footprint_format': '{Width} × {Depth} m'},
        INFO: {'build_inspector_width': 400.0, 'build_inspector_height': 300.0,
               'build_inspector_tab_columns': 5, 'show_unavailable_build_tabs': False,
               'build_thumbnail_width': 56.0, 'build_thumbnail_height': 48.0},
        LIGHT: {'use_compact_build_layout': True, 'build_color_wheel_size': 32.0,
                'build_color_swatch_height': 24.0, 'build_maximum_vertical_padding': 0.0},
    }
    for package, properties in values.items():
        bp, _ = widgets(package)
        cdo = u.get_default_object(bp.generated_class())
        for name, value in properties.items():
            cdo.set_editor_property(name, value)
    info_bp, _ = widgets(INFO)
    info_cdo = u.get_default_object(info_bp.generated_class())
    info_cdo.set_editor_property('selected_tab_text_color', v.color(v.TEAL))
    info_cdo.set_editor_property('inactive_tab_text_color', v.color(v.INK))
    values[INFO]['selected_tab_text_color'] = v.TEAL
    values[INFO]['inactive_tab_text_color'] = v.INK
    return values


def main():
    assert (OUT / 'preflight.json').is_file(), 'Run V10 inspect first'
    if '-run=pythonscript' not in u.SystemLibrary.get_command_line().lower():
        assert not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor()
    RUN.mkdir(parents=True, exist_ok=False)
    before_hashes = v.protected_hashes()
    before_names = {}
    for package in PACKAGES:
        bp, w = widgets(package)
        before_names[package] = {name: x.get_class().get_name() for name, x in w.items()}
        source = v.path_for(package)
        target = RUN / 'Backup' / source.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        assert v.digest(source) == v.digest(target)
        bp.modify()
        for widget in w.values():
            widget.modify()
            if widget.slot:
                widget.slot.modify()
    REPORT.update(before_hashes=before_hashes, widgets_before=before_names)
    edited = (configure_synergy(), configure_lighting(), configure_info(), configure_placement())
    for bp in edited:
        REPORT.setdefault('registered_widget_guids', {})[bp.get_path_name()] = u.FF_CatalogEditorLibrary.register_widget_guids(bp)
    # A nested WBP can immediately compile its dependent inspector. Register
    # every edited tree before any dependency is compiled.
    for bp in edited:
        u.BlueprintEditorLibrary.compile_blueprint(bp)
        assert bp.generated_class(), bp.get_name()
    REPORT['defaults'] = defaults()
    for package in PACKAGES:
        bp, w = widgets(package)
        missing = [name for name, cls in before_names[package].items()
                   if name not in w or w[name].get_class().get_name() != cls]
        assert not missing, 'Bound widgets removed/retyped: ' + repr(missing)
        u.EditorAssetLibrary.set_metadata_tag(bp, 'FF.BuildCatalog.ReadabilityV10', '1')
        assert u.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
        REPORT['saved'].append(package)
    after_hashes = v.protected_hashes()
    allowed = {v.path_for(package).relative_to(ROOT).as_posix() for package in PACKAGES}
    unexpected = [path for path in before_hashes.keys() | after_hashes.keys()
                  if path not in allowed and before_hashes.get(path) != after_hashes.get(path)]
    REPORT['unexpected_changes'] = unexpected
    assert not unexpected, repr(unexpected)
    REPORT['changed_assets'] = [path for path in allowed if before_hashes.get(path) != after_hashes.get(path)]
    REPORT['status'] = 'PASS'


if __name__ == '__main__':
    try:
        main()
    except Exception:
        REPORT.update(status='FAIL', error=traceback.format_exc())
        u.log_error(REPORT['error'])
    finally:
        RUN.mkdir(parents=True, exist_ok=True)
        output = json.dumps(REPORT, ensure_ascii=False, indent=2)
        (RUN / 'report.json').write_text(output, encoding='utf-8')
        (OUT / 'apply-result.json').write_text(output, encoding='utf-8')
        u.log('FF_BUILD_CATALOG_V10_' + REPORT['status'])
    if REPORT['status'] != 'PASS':
        raise RuntimeError('V10 migration failed; backups/report: ' + str(RUN))
