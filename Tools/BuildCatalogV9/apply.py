"""Apply the approved search-v9 construction catalog to four existing WBP assets.

Run in an idle, freshly built editor after inspect.py. This does not start PIE,
change gameplay data, import icons, or modify the teammate HUD. Backups and an
independent layout readback are retained under Saved/BuildCatalogV9.
"""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import traceback

import unreal as u


ROOT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
OUT = ROOT / 'Saved/BuildCatalogV9'
RUN = OUT / datetime.now(timezone.utc).strftime('apply-%Y%m%d-%H%M%S-%f')
BASE = '/Game/FestaFiesta/Building/FF_WBP_Building/'
PANEL, CARD, TOOLS, INFO = [BASE + suffix for suffix in (
    'Catalog/WBP_FF_PrepPanelBuild', 'Catalog/WBP_FF_PrepBuildTile',
    'Controls/WBP_FF_BuildTools', 'Facility/WBP_FF_FacilityInfoPanel')]
PACKAGES = (PANEL, CARD, TOOLS, INFO)
FONT = '/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Font'
HEIGHT, INSPECTOR_WIDTH, SEARCH_WIDTH = 300.0, 400.0, 170.0
PEARL, INK, MUTED, LINE, TEAL, SELECTED = 'F9F9FF', '3D3C4F', '626577', 'DBDEED', '286A79', 'E5F6FC'
REPORT = {'status': 'RUNNING', 'packages': list(PACKAGES), 'saved': [],
          'backup': str(RUN / 'Backup'), 'runtime_tests_run': False}
VISIBLE = u.SlateVisibility.SELF_HIT_TEST_INVISIBLE
COLLAPSED = u.SlateVisibility.COLLAPSED


def path_for(package):
    return ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def protected_hashes():
    paths = set((ROOT / 'Content').rglob('WBP*.uasset'))
    for relative in ('Content/FestaFiesta/UI', 'Content/FestaFiesta/Building/FF_WBP_Building',
                     'Content/Asset/Icon_HKH', 'Config'):
        paths.update(p for p in (ROOT / relative).rglob('*') if p.is_file())
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(paths)}


def widgets(package):
    assert package in PACKAGES, 'Out-of-scope asset: ' + package
    bp = u.load_asset(package)
    assert bp and bp.generated_class(), 'Missing WBP/generated class: ' + package
    values = {w.get_name(): w for w in u.ObjectIterator(u.Widget)
              if w.get_outer() and w.get_outer().get_class().get_name() == 'WidgetTree'
              and w.get_outer().get_outer() == bp}
    assert values, 'No authored widget tree: ' + package
    return bp, values


def required(values, names):
    missing = [name for name in names if name not in values]
    assert not missing, 'Missing required named widgets: ' + repr(missing)


def color(value, alpha=1.0):
    value = value.lstrip('#')
    rgb = [int(value[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return u.LinearColor(*[v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in rgb], alpha)


def slate(value, alpha=1.0):
    return u.SlateColor(specified_color=color(value, alpha))


def margin(values=(0, 0, 0, 0)):
    return u.Margin(*[float(v) for v in values])


def rounded(fill=PEARL, alpha=1.0, radii=(8, 8, 8, 8), stroke=LINE, width=1.0):
    outline = u.SlateBrushOutlineSettings(corner_radii=u.Vector4(*radii),
        color=slate(stroke, min(alpha, .65)), width=width,
        rounding_type=u.SlateBrushRoundingType.FIXED_RADIUS)
    return u.SlateBrush(draw_as=u.SlateBrushDrawType.ROUNDED_BOX,
                        tint_color=slate(fill, alpha), outline_settings=outline)


def surface(widget, alpha=.9, radii=(8, 8, 8, 8), padding=(0, 0, 0, 0), fill=PEARL, width=1.0):
    widget.set_editor_property('background', rounded(fill, alpha, radii, width=width))
    widget.set_brush_color(u.LinearColor(1, 1, 1, 1))
    widget.set_padding(margin(padding))


def clear_surface(widget):
    brush = widget.get_editor_property('background')
    brush.set_editor_property('draw_as', u.SlateBrushDrawType.NO_DRAW_TYPE)
    widget.set_editor_property('background', brush)
    widget.set_padding(margin())


def font(widget, size=13, tint=INK):
    if isinstance(widget, u.CommonTextBlock):
        widget.set_style(None)
    value = widget.get_editor_property('font')
    value.set_editor_property('font_object', u.load_asset(FONT))
    value.set_editor_property('typeface_font_name', 'Default')
    value.set_editor_property('size', int(size))
    widget.set_font(value)
    widget.set_color_and_opacity(slate(tint))


def node(values, cls, name):
    result = values.get(name)
    if result is None:
        tree = next(iter(values.values())).get_outer()
        result = u.new_object(cls, outer=tree, name=name)
        values[name] = result
    assert isinstance(result, cls), 'Widget name has another type: ' + name
    result.modify()
    return result


def label(values, name, caption, size=13, tint=INK):
    result = node(values, u.TextBlock, name)
    result.set_text(caption)
    result.set_auto_wrap_text(False)
    font(result, size, tint)
    result.set_visibility(u.SlateVisibility.HIT_TEST_INVISIBLE)
    return result


def bounds(widget, width=None, height=None):
    widget.clear_min_desired_width()
    widget.clear_max_desired_width()
    widget.clear_min_desired_height()
    widget.clear_max_desired_height()
    widget.clear_width_override() if width is None else widget.set_width_override(float(width))
    widget.clear_height_override() if height is None else widget.set_height_override(float(height))


def fill(slot, yes=False):
    if isinstance(slot, (u.HorizontalBoxSlot, u.VerticalBoxSlot)):
        size = slot.get_editor_property('size')
        size.set_editor_property('size_rule', u.SlateSizeRule.FILL if yes else u.SlateSizeRule.AUTOMATIC)
        size.set_editor_property('value', 1.0)
        slot.set_editor_property('size', size)


def add(parent, child, expand=False, padding=(0, 0, 0, 0)):
    child.remove_from_parent()
    slot = parent.add_child(child)
    fill(slot, expand)
    if hasattr(slot, 'set_padding'):
        slot.set_padding(margin(padding))
    if hasattr(slot, 'set_horizontal_alignment'):
        slot.set_horizontal_alignment(u.HorizontalAlignment.H_ALIGN_FILL)
    if hasattr(slot, 'set_vertical_alignment'):
        slot.set_vertical_alignment(u.VerticalAlignment.V_ALIGN_FILL)
    return slot


def archive(values, host, candidates, name):
    retired = node(values, u.VerticalBox, name)
    retired.set_visibility(COLLAPSED)
    # Collapsed is authoritative at runtime; this editor-only hint also hides the archive in Designer.
    try:
        retired.set_editor_property('hidden_in_designer', True)
    except Exception:
        REPORT.setdefault('warnings', []).append('Designer hidden flag unavailable: ' + name)
    for child in candidates:
        if child != retired and child.get_parent() is None:
            add(retired, child)
    add(host, retired)
    return retired


def button_style(widget, primary=False):
    style = widget.get_editor_property('widget_style')
    for state, fill_color in [('normal', PEARL), ('hovered', SELECTED), ('pressed', SELECTED), ('disabled', SELECTED)]:
        style.set_editor_property(state, rounded(fill_color, .96, (6, 6, 6, 6), TEAL if state != 'normal' else LINE))
    style.set_editor_property('normal_padding', margin((8, 4, 8, 4)))
    style.set_editor_property('pressed_padding', margin((8, 4, 8, 4)))
    widget.set_editor_property('widget_style', style)
    child = widget.get_content()
    if isinstance(child, u.TextBlock):
        font(child, 13, TEAL if primary else INK)


def text_button(values, name, caption, tooltip, width=32):
    widget = node(values, u.Button, name)
    content = label(values, name + '_Label', caption, 14, TEAL)
    content.set_editor_property('justification', u.TextJustify.CENTER)
    widget.set_content(content)
    widget.set_tool_tip_text(tooltip)
    button_style(widget)
    size = node(values, u.SizeBox, name + '_Size')
    bounds(size, width, 32)
    size.set_content(widget)
    return size


def configure_catalog():
    bp, w = widgets(PANEL)
    required(w, ('Panel_Catalog', 'VerticalBox_CatalogBody', 'Size_ClarityCatalogDock',
                 'HorizontalBox_Categories', 'Size_PearlSearch', 'TileView_Facilities',
                 'HorizontalBox_Subcategories', 'ScrollBox_Subcategories',
                 'Check_FilterRequired', 'Check_FilterAffordable', 'Check_FilterPrograms', 'Check_FilterLighting'))
    old_children = list(w['Panel_Catalog'].get_all_children())
    old_body = list(w['VerticalBox_CatalogBody'].get_all_children())
    panel = w['Panel_Catalog']
    panel.clear_children()
    body = w['VerticalBox_CatalogBody']
    body.clear_children()
    # The previous intuitive layout retired this wrapper; it is the active center column again.
    body.set_visibility(VISIBLE)
    for name in ('Panel_Catalog', 'Size_A5CatalogRows', 'Overlay_CatalogCards',
                 'Size_PearlSearch', 'Size_FavoritesOnly', 'HorizontalBox_Categories'):
        w[name].set_visibility(VISIBLE)
    w['TileView_Facilities'].set_visibility(u.SlateVisibility.VISIBLE)
    w['Search_Facilities'].set_visibility(u.SlateVisibility.VISIBLE)
    w['Button_FavoritesOnly'].set_visibility(u.SlateVisibility.VISIBLE)
    overlay = node(w, u.Overlay, 'Overlay_V9Catalog')
    overlay.clear_children()
    add(panel, overlay, True)
    row = node(w, u.HorizontalBox, 'Row_V9Catalog')
    row.clear_children()
    add(overlay, row)
    rail = node(w, u.SizeBox, 'Size_V9CategoryRail')
    bounds(rail, 80, None)
    categories = w['HorizontalBox_Categories']
    choices = [w['Choice_Category' + suffix] for suffix in ('All', '0', '9', '1', '2', '3', '4', '5', '6', '7', '8')]
    for choice in choices:
        choice.remove_from_parent()
    categories.clear_children()
    grid = node(w, u.UniformGridPanel, 'Grid_V9Categories')
    grid.clear_children()
    grid.set_min_desired_slot_width(34)
    grid.set_min_desired_slot_height(34)
    grid.set_slot_padding(margin((3, 3, 3, 3)))
    for i, choice in enumerate(choices):
        choice.set_editor_property('icon_only', True)
        choice.set_editor_property('icon_only_size', 22.0)
        grid.add_child_to_uniform_grid(choice, i // 2, i % 2)
    add(categories, grid, True)
    rail.set_content(categories)
    add(row, rail, False, (0, 0, 12, 0))
    add(row, body, True)
    header = node(w, u.HorizontalBox, 'Row_V9CatalogHeader')
    header.clear_children()
    bounds(w['Size_PearlSearch'], SEARCH_WIDTH, 32)
    search = w['Search_Facilities']
    search.set_hint_text('시설 검색')
    style = search.get_editor_property('widget_style')
    for state in ('background_image_normal', 'background_image_hovered', 'background_image_focused'):
        style.set_editor_property(state, rounded(PEARL, .96, (6, 6, 6, 6)))
    text_style = style.get_editor_property('text_style')
    text_font = text_style.get_editor_property('font')
    text_font.set_editor_property('font_object', u.load_asset(FONT))
    text_font.set_editor_property('typeface_font_name', 'Default')
    text_font.set_editor_property('size', 14)
    text_style.set_editor_property('font', text_font)
    style.set_editor_property('text_style', text_style)
    search.set_editor_property('widget_style', style)
    add(header, w['Size_PearlSearch'])
    spacer = node(w, u.SizeBox, 'Size_V9HeaderSpace')
    bounds(spacer, None, 1)
    add(header, spacer, True)
    favorite = w['Button_FavoritesOnly']
    original_favorite = favorite.get_content()
    favorite.set_content(label(w, 'Text_V9FavoriteGlyph', '☆', 18, TEAL))
    favorite.set_tool_tip_text('즐겨찾기만 보기')
    button_style(favorite)
    bounds(w['Size_FavoritesOnly'], 32, 32)
    add(header, w['Size_FavoritesOnly'], False, (0, 0, 8, 0))
    add(header, text_button(w, 'Button_V9Filters', '필터', '시설 조건 필터', 68), False, (0, 0, 8, 0))
    add(header, text_button(w, 'Button_V9More', '⋯', '정렬·매출 순위·조합 도감'), False, (0, 0, 8, 0))
    add(header, text_button(w, 'Button_V9Close', '×', '시설 목록 닫기'))
    add(body, header, False, (0, 0, 0, 14))
    bounds(w['Size_A5CatalogRows'], None, 200)
    add(body, w['Size_A5CatalogRows'], True)
    view = w['TileView_Facilities']
    view.set_editor_property('horizontal_entry_spacing', 8.0)
    view.set_editor_property('vertical_entry_spacing', 0.0)
    view.set_editor_property('entry_height', 100.0)
    view.set_clipping(u.WidgetClipping.CLIP_TO_BOUNDS)
    view.set_scrollbar_visibility(u.SlateVisibility.VISIBLE)
    # Popup wrappers are real runtime-toggle targets; existing filter/sort/gameplay widgets stay bound.
    filters = node(w, u.Border, 'Popup_V9Filters')
    filter_column = node(w, u.VerticalBox, 'Column_V9Filters')
    filter_column.clear_children()
    filter_scroll = node(w, u.ScrollBox, 'Scroll_V9Filters')
    filter_scroll.clear_children()
    filter_scroll.set_orientation(u.Orientation.ORIENT_VERTICAL)
    filter_scroll.set_clipping(u.WidgetClipping.CLIP_TO_BOUNDS)
    add(filter_scroll, filter_column)
    filter_limits = node(w, u.SizeBox, 'Size_V9FilterPopup')
    bounds(filter_limits, 244, None)
    filter_limits.set_max_desired_height(228.0)
    filter_limits.set_content(filter_scroll)
    filters.set_content(filter_limits)
    surface(filters, .98, padding=(12, 10, 12, 10))
    # Keep both native bindings: category changes reset this scroller to its first chip.
    minor_host = w['HorizontalBox_Subcategories']
    minor_host.set_visibility(VISIBLE)
    minor_scroll = w['ScrollBox_Subcategories']
    minor_scroll.set_visibility(u.SlateVisibility.VISIBLE)
    minor_scroll.set_orientation(u.Orientation.ORIENT_HORIZONTAL)
    minor_scroll.set_clipping(u.WidgetClipping.CLIP_TO_BOUNDS)
    minor_scroll.clear_children()
    add(minor_scroll, minor_host)
    minor_bounds = node(w, u.SizeBox, 'Size_V9SubcategoryViewport')
    bounds(minor_bounds, 244, 36)
    minor_bounds.set_content(minor_scroll)
    minor_bounds.set_tool_tip_text('세부 분류 · 좌우로 스크롤')
    add(filter_column, minor_bounds, False, (0, 0, 0, 6))
    for name in ('Check_FilterRequired', 'Check_FilterAffordable', 'Check_FilterPrograms', 'Check_FilterLighting'):
        add(filter_column, w[name], False, (0, 4, 0, 4))
    add(filter_column, w['Button_ResetFilters'], False, (0, 8, 0, 0))
    filters.set_visibility(COLLAPSED)
    filter_slot = add(overlay, filters, False, (0, 40, 44, 0))
    filter_slot.set_horizontal_alignment(u.HorizontalAlignment.H_ALIGN_RIGHT)
    filter_slot.set_vertical_alignment(u.VerticalAlignment.V_ALIGN_TOP)
    more = node(w, u.Border, 'Popup_V9More')
    more_column = node(w, u.VerticalBox, 'Column_V9More')
    more_column.clear_children()
    more.set_content(more_column)
    surface(more, .98, padding=(12, 10, 12, 10))
    for name in ('ComboBox_CatalogSort', 'Size_PearlRevenueRanking', 'Widget_FacilityCodexCatalogLauncher'):
        add(more_column, w[name], False, (0, 4, 0, 4))
    more.set_visibility(COLLAPSED)
    more_slot = add(overlay, more, False, (0, 40, 8, 0))
    more_slot.set_horizontal_alignment(u.HorizontalAlignment.H_ALIGN_RIGHT)
    more_slot.set_vertical_alignment(u.VerticalAlignment.V_ALIGN_TOP)
    if original_favorite:
        old_children.append(original_favorite)
    archive(w, panel, old_children + old_body, 'Panel_V9Retired')
    for name in ('Text_A5CatalogHeading', 'Text_ItemCount', 'Text_ActiveFilterCount', 'Row_A5CatalogFooter'):
        if name in w:
            w[name].set_visibility(COLLAPSED)
    bounds(w['Size_ClarityCatalogDock'], 1484, HEIGHT)
    surface(w['Border_ApprovedCatalogV9'], .9, (12, 0, 0, 12), (0, 0, 0, 0))
    clear_surface(w['Border_PanelRoot'])
    w['Border_PanelRoot'].set_padding(margin((12, 12, 12, 12)))
    return bp


def configure_card():
    bp, w = widgets(CARD)
    required(w, ('SizeBox_Card', 'SizeBox_Thumbnail', 'Text_Name', 'Text_Cost', 'Border_Card'))
    bounds(w['SizeBox_Card'], 100, 100)
    # Source thumbnails include transparent margins. Match the visible facility
    # silhouette in search-v9, rather than shrinking its entire texture to 48px.
    bounds(w['SizeBox_Thumbnail'], 96, 80)
    w['Border_Card'].set_padding(margin((4, 8, 4, 8)))
    surface(w['Border_Card'], .94, (7, 7, 7, 7), (4, 8, 4, 8), width=.35)
    w['VerticalBox_Content'].slot.set_padding(margin((4, 8, 4, 8)))
    w['VerticalBox_Content'].slot.set_vertical_alignment(u.VerticalAlignment.V_ALIGN_CENTER)
    surface(w['Border_ThumbnailBackdrop'], 0, width=0)
    for name in ('Size_PearlFacilityName', 'HorizontalBox_Meta', 'Text_State', 'Border_ProgramBadge', 'Badge_Lighting'):
        if name in w:
            w[name].set_visibility(COLLAPSED)
    # Favorite retains its live hover/focus behavior; never place it under the hidden caption wrappers.
    bounds(w['Size_Favorite'], 22, 22)
    return bp


def configure_placement():
    bp, w = widgets(TOOLS)
    required(w, ('Size_PlacementInfo', 'Border_PlacementInfo', 'Column_PearlSummaryContent',
                 'Text_FacilityName', 'Text_Cost', 'Text_Footprint', 'Border_PlacementSynergy',
                 'Size_PlacementFacilityThumb', 'Button_DirectAdjustment'))
    bounds(w['Size_PlacementInfo'], INSPECTOR_WIDTH, HEIGHT)
    s = w['Size_PlacementInfo'].slot
    s.set_anchors(u.Anchors(u.Vector2D(1, 1), u.Vector2D(1, 1)))
    s.set_alignment(u.Vector2D(1, 1))
    s.set_offsets(u.Margin(-24, -70, INSPECTOR_WIDTH, HEIGHT))
    s.set_auto_size(False)
    surface(w['Border_PlacementInfo'], .9, (0, 12, 12, 0), (14, 12, 14, 12))
    shell = w['Column_PearlSummaryContent']
    retired = list(shell.get_all_children())
    shell.clear_children()
    tabs = node(w, u.HorizontalBox, 'Row_V9PlacementTabs')
    tabs.clear_children()
    for name, caption in (('Button_V9PlacementInfo', '설치 정보'), ('Button_V9PlacementSynergy', '시설 조합')):
        size = text_button(w, name, caption, caption, 180)
        bounds(size, None, 28)
        add(tabs, size, True, (0, 0, 4, 0))
    add(shell, tabs, False, (0, 0, 0, 8))
    summary = w['Scroll_PearlPlacementSummary']
    column = w['Column_ClaritySummary']
    old_summary = list(column.get_all_children())
    column.clear_children()
    overlay = node(w, u.Overlay, 'Overlay_V9PlacementPages')
    overlay.clear_children()
    add(column, overlay)
    facts = node(w, u.VerticalBox, 'Panel_V9PlacementFacts')
    facts.clear_children()
    add(overlay, facts)
    font(w['Text_FacilityName'], 18)
    add(facts, w['Text_FacilityName'], False, (0, 0, 0, 10))
    hero = node(w, u.HorizontalBox, 'Row_V9PlacementHero')
    hero.clear_children()
    bounds(w['Size_PlacementFacilityThumb'], 124, 100)
    w['Size_PlacementFacilityThumb'].set_visibility(VISIBLE)
    surface(w['Border_PlacementFacilityThumb'], .35, padding=(6, 6, 6, 6), width=0)
    add(hero, w['Size_PlacementFacilityThumb'], False, (0, 0, 14, 0))
    values = node(w, u.VerticalBox, 'Column_V9PlacementValues')
    values.clear_children()
    add(values, label(w, 'Text_V9CostCaption', '설치 비용', 12, TEAL), False, (0, 0, 0, 3))
    font(w['Text_Cost'], 20, TEAL)
    add(values, w['Text_Cost'], False, (0, 0, 0, 12))
    add(values, label(w, 'Text_V9FootprintCaption', '시설 크기', 12, MUTED), False, (0, 0, 0, 3))
    font(w['Text_Footprint'], 13, MUTED)
    add(values, w['Text_Footprint'])
    add(hero, values, True)
    add(facts, hero)
    font(w['Text_Legend'], 12, TEAL)
    add(facts, w['Text_Legend'], False, (0, 8, 0, 4))
    synergy = node(w, u.VerticalBox, 'Panel_V9PlacementSynergy')
    synergy.clear_children()
    add(synergy, w['Border_PlacementSynergy'])
    surface(w['Border_PlacementSynergy'], .96, padding=(8, 8, 8, 8))
    add(overlay, synergy)
    synergy.set_visibility(COLLAPSED)
    add(shell, summary, True)
    for name in ('Text_PlacementAdvice', 'Text_PlacementObstruction', 'Text_AssistanceMessage'):
        if name in w:
            font(w[name], 11, MUTED)
            add(shell, w[name])
    button_style(w['Button_DirectAdjustment'], True)
    add(shell, w['Button_DirectAdjustment'], False, (0, 6, 0, 0))
    add(shell, w['Row_PlacementAssistance'], False, (0, 4, 0, 0))
    archive(w, shell, retired + old_summary, 'Panel_V9PlacementRetired')
    return bp


def configure_installed_info():
    bp, w = widgets(INFO)
    # Keep ordinary/live authored 550x630 dimensions: only the Build CDO dimensions change below.
    surface(w['Border_Panel'], .9, (0, 12, 12, 0), (12, 10, 12, 10))
    for name in ('Header_Surface', 'Body_Surface', 'Panel_Actions', 'Panel_BuildSection'):
        clear_surface(w[name])
    bounds(w['Header_Size'], None, 26)
    bounds(w['Thumbnail_Size'], 48, 36)
    surface(w['Panel_PersistentIdentity'], 0, padding=(0, 2, 0, 2), width=0)
    w['Panel_PersistentIdentity'].slot.set_padding(margin((0, 2, 0, 4)))
    font(w['Text_FacilityName'], 16)
    font(w['V2_InspectorTitle'], 14, TEAL)
    for name in ('Text_Category', 'Text_LevelSummary'):
        font(w[name], 11, MUTED)
    grid = w['Grid_InspectorTabs']
    grid.set_min_desired_slot_width(64)
    grid.set_min_desired_slot_height(26)
    grid.set_slot_padding(margin((2, 2, 2, 2)))
    grid.slot.set_padding(margin((0, 0, 0, 4)))
    for name, widget in w.items():
        if name.startswith('Button_Tab'):
            button_style(widget)
            content = widget.get_content()
            if isinstance(content, u.TextBlock):
                font(content, 11)
        if name.startswith('Clarity_Row_Text_') and isinstance(widget, u.HorizontalBox):
            widget.slot.set_padding(margin((0, 2, 0, 2)))
            for child in widget.get_all_children():
                if isinstance(child, u.TextBlock):
                    font(child, 11)
    for name in ('Button_Move', 'Button_Remove'):
        if name in w:
            button_style(w[name])
    return bp


def apply_defaults():
    values = {
        PANEL: {'cards_per_page': 12, 'visible_catalog_rows': 2, 'use_pagination': False,
                'use_designer_catalog_layout': True, 'fit_catalog_to_viewport': True,
                'catalog_height': HEIGHT, 'catalog_viewport_margin': 36.0,
                'catalog_inspector_width': INSPECTOR_WIDTH, 'catalog_inspector_gap': 0.0,
                'integrate_catalog_inspector': True},
        CARD: {'thumbnail_only': True},
        TOOLS: {'installation_cost_format': '{Cost} Gold', 'footprint_format': '{Width} × {Depth} m'},
        INFO: {'build_inspector_width': INSPECTOR_WIDTH, 'build_inspector_height': HEIGHT,
               'build_inspector_minimum_bottom_inset': 70.0, 'build_inspector_tab_columns': 5},
    }
    for package, properties in values.items():
        bp, _ = widgets(package)
        cdo = u.get_default_object(bp.generated_class())
        for name, value in properties.items():
            try:
                cdo.set_editor_property(name, value)
            except Exception as error:
                raise RuntimeError('Build the native module before applying; property unavailable: ' + name) from error
    return values


def register_widget_guids(bp):
    # UE 5.7 does not expose the GUID map to Python. Use the canonical
    # editor registration API so existing GUIDs remain unchanged.
    count = u.FF_CatalogEditorLibrary.register_widget_guids(bp)
    REPORT.setdefault('registered_widget_guids', {})[bp.get_path_name()] = int(count)


def readback():
    _, panel = widgets(PANEL)
    _, card = widgets(CARD)
    _, tools = widgets(TOOLS)
    _, info = widgets(INFO)
    panel_defaults = u.get_default_object(u.load_asset(PANEL).generated_class())
    card_defaults = u.get_default_object(u.load_asset(CARD).generated_class())
    minor_path = []
    minor_parent = panel['HorizontalBox_Subcategories']
    while minor_parent and minor_parent != panel['Popup_V9Filters']:
        assert minor_parent.get_visibility() not in (COLLAPSED, u.SlateVisibility.HIDDEN), 'Hidden subcategory ancestor: ' + minor_parent.get_name()
        minor_path.append(minor_parent.get_name())
        minor_parent = minor_parent.get_parent()
    state = {
        'category_count': panel['Grid_V9Categories'].get_children_count(),
        'category_grid_parent': panel['Grid_V9Categories'].get_parent().get_name(),
        'search_width': panel['Size_PearlSearch'].get_editor_property('width_override'),
        'catalog_height': panel['Size_ClarityCatalogDock'].get_editor_property('height_override'),
        'rows_height': panel['Size_A5CatalogRows'].get_editor_property('height_override'),
        'vertical_spacing': panel['TileView_Facilities'].get_editor_property('vertical_entry_spacing'),
        'cards_per_page': panel_defaults.get_editor_property('cards_per_page'),
        'thumbnail_only': card_defaults.get_editor_property('thumbnail_only'),
        'placement_size': [tools['Size_PlacementInfo'].get_editor_property('width_override'), tools['Size_PlacementInfo'].get_editor_property('height_override')],
        'live_info_size': [info['Panel_Size'].get_editor_property('width_override'), info['Panel_Size'].get_editor_property('height_override')],
        'filters_parent': panel['Check_FilterRequired'].get_parent().get_name(),
        'reset_parent': panel['Button_ResetFilters'].get_parent().get_name(),
        'subcategory_filter_path': minor_path,
        'subcategory_reachable_when_filter_opens': minor_parent == panel['Popup_V9Filters'],
        'popup_filter_closed': panel['Popup_V9Filters'].get_visibility() == COLLAPSED,
        'popup_more_closed': panel['Popup_V9More'].get_visibility() == COLLAPSED,
        'placement_name_parent': tools['Text_FacilityName'].get_parent().get_name(),
        'placement_synergy_parent': tools['Border_PlacementSynergy'].get_parent().get_name(),
        'caption_wrapper_hidden': card['Size_PearlFacilityName'].get_visibility() == COLLAPSED,
        'price_wrapper_hidden': card['HorizontalBox_Meta'].get_visibility() == COLLAPSED,
        'retired_header_hidden': panel['Panel_V9Retired'].get_visibility() == COLLAPSED,
        'center_column_visible': panel['VerticalBox_CatalogBody'].get_visibility() == VISIBLE,
    }
    assert state['category_count'] == 11 and state['category_grid_parent'] == 'HorizontalBox_Categories'
    assert state['search_width'] == SEARCH_WIDTH and state['catalog_height'] == HEIGHT
    assert state['cards_per_page'] == 12 and state['thumbnail_only'] is True
    assert state['caption_wrapper_hidden'] and state['price_wrapper_hidden'] and state['retired_header_hidden']
    assert state['center_column_visible'], 'The active center column must not retain its retired Collapsed flag'
    assert state['placement_size'] == [INSPECTOR_WIDTH, HEIGHT]
    assert state['live_info_size'] == [550.0, 630.0], 'Live inspector authored dimensions must remain unchanged'
    assert state['vertical_spacing'] == 0.0
    assert state['filters_parent'] == 'Column_V9Filters' and state['reset_parent'] == 'Column_V9Filters'
    assert state['subcategory_reachable_when_filter_opens']
    assert panel['HorizontalBox_Subcategories'].get_parent() == panel['ScrollBox_Subcategories']
    assert state['popup_filter_closed'] and state['popup_more_closed']
    assert state['placement_name_parent'] == 'Panel_V9PlacementFacts'
    assert state['placement_synergy_parent'] == 'Panel_V9PlacementSynergy'
    return state


def main():
    assert (OUT / 'preflight.json').is_file(), 'Run the read-only inspect.py first'
    # Python commandlets cannot have PIE and do not provide GEditor. Calling the
    # level-editor PIE query there dereferences a null editor in UE 5.7.
    if '-run=pythonscript' not in u.SystemLibrary.get_command_line().lower():
        assert not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor(), 'Stop PIE before applying'
    preflight = json.loads((OUT / 'preflight.json').read_text(encoding='utf-8-sig'))
    assert all(package in preflight['packages'] for package in PACKAGES)
    RUN.mkdir(parents=True, exist_ok=False)
    before_hashes = protected_hashes()
    before_names = {}
    for package in PACKAGES:
        bp, ws = widgets(package)
        before_names[package] = {name: widget.get_class().get_name() for name, widget in ws.items()}
        source = path_for(package)
        target = RUN / 'Backup' / source.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        assert digest(source) == digest(target), 'Backup mismatch: ' + package
        bp.modify()
        for widget in ws.values():
            widget.modify()
            if widget.slot:
                widget.slot.modify()
    REPORT.update(before_hashes=before_hashes, widgets_before=before_names)
    # Compile the tile before its host so nested/default classes are reinstanced in dependency order.
    edited = [configure_card(), configure_catalog(), configure_placement(), configure_installed_info()]
    for bp in edited:
        register_widget_guids(bp)
        u.BlueprintEditorLibrary.compile_blueprint(bp)
        assert bp.generated_class(), 'Compile failed: ' + bp.get_name()
    REPORT['defaults'] = apply_defaults()
    for package in PACKAGES:
        bp, ws = widgets(package)
        missing = [name for name, cls in before_names[package].items()
                   if name not in ws or ws[name].get_class().get_name() != cls]
        assert not missing, 'An existing bound widget was removed/retyped: ' + repr(missing)
        u.EditorAssetLibrary.set_metadata_tag(bp, 'FF.BuildCatalog.SearchV9', '1')
        assert u.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), 'Save failed: ' + package
        REPORT['saved'].append(package)
    REPORT['readback'] = readback()
    after_hashes = protected_hashes()
    allowed = {path_for(package).relative_to(ROOT).as_posix() for package in PACKAGES}
    unexpected = [path for path in before_hashes.keys() | after_hashes.keys()
                  if path not in allowed and before_hashes.get(path) != after_hashes.get(path)]
    REPORT['unexpected_changes'] = unexpected
    assert not unexpected, 'Non-target UI/config/icon files changed: ' + repr(unexpected)
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
        OUT.mkdir(parents=True, exist_ok=True)
        output = json.dumps(REPORT, ensure_ascii=False, indent=2)
        (RUN / 'report.json').write_text(output, encoding='utf-8')
        (OUT / 'apply-result.json').write_text(output, encoding='utf-8')
        u.log('FF_BUILD_CATALOG_V9_' + REPORT['status'] + ' ' + str(RUN))
    if REPORT['status'] != 'PASS':
        raise RuntimeError('Catalog migration failed; backups/report retained in ' + str(RUN))
