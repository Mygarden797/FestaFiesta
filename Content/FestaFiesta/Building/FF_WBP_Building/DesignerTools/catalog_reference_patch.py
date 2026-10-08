"""건축 카탈로그 버튼/필수 시설 표시를 편집하는 중앙 적용용 모듈.

apply(context)는 위젯 속성만 편집하고 패키지 목록/요약을 반환한다.
호출자가 백업·저장과 적용 순서를 관리한다. 테스트/PIE/콘솔 실행은 없다.
Designer: Size_PearlRevenueRanking, Size_Launcher는 모두 160×44 논리 단위.
필수 배지는 기존 Border_RequiredMissing/Text_RequiredMissing 바인딩을 유지한다.
"""

import unreal


BASE = "/Game/FestaFiesta/Building/FF_WBP_Building/"
PANEL = BASE + "Catalog/WBP_FF_PrepPanelBuild"
CARD = BASE + "Catalog/WBP_FF_PrepBuildTile"
LAUNCHER = BASE + "FacilityCodex/WBP_FF_FacilityCodexCatalogLauncher"
PACKAGES = [LAUNCHER, CARD, PANEL]

SURFACE = "#F9F9FF"
INK = "#3D3C4F"
LINE = "#DBDEED"
ACTION = "#286A79"
SELECTED = "#E5F6FC"
DANGER = "#AD3F48"
REQUIRED_SURFACE = "#FFF2F4"
BUTTON_WIDTH = 160.0
BUTTON_HEIGHT = 44.0
HEADER_FONT_ASSET = "/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Font"
HEADER_FONT_SIZE = 16


def apply(context):
    """기존 건축 전용 WBP 3개만 변경한다. Shared/Legacy/외부 UI는 불러오지 않는다."""
    color, slate, make_brush = context["color"], context["slate"], context["brush"]
    header_font_asset = unreal.load_asset(HEADER_FONT_ASSET)
    if not header_font_asset:
        raise RuntimeError("Missing existing project header font: " + HEADER_FONT_ASSET)
    # One complete font descriptor prevents old Medium/Bold faces or size overrides surviving.
    header_font = unreal.SlateFontInfo(font_object=header_font_asset,
                                      typeface_font_name="Default", size=HEADER_FONT_SIZE,
                                      letter_spacing=0)
    summary = {
        "packages": PACKAGES,
        "button_size": [BUTTON_WIDTH, BUTTON_HEIGHT],
        "button_font_umg_points": HEADER_FONT_SIZE,
        "header_typography": {"font": HEADER_FONT_ASSET, "face": "Default",
                              "size_umg_points": HEADER_FONT_SIZE, "letter_spacing": 0,
                              "includes": ["Text_FavoritesOnly", "Text_FavoriteCount",
                                           "Search_Facilities input and hint", "Text_RevenueRanking",
                                           "Button_OpenCodex_Label"]},
        "card_allocation": [184, 210],
        "thumbnail": [162, 104],
        "name_allocation": [162, 48],
        "required_missing": {
            "card_fill": REQUIRED_SURFACE, "outline": DANGER, "outline_width": 2,
            "badge_fill": DANGER, "badge_text": "#FFFFFF",
            "label": "! 필수 현재/최소", "counts": "기존 시설 데이터의 실제 값",
        },
        "required_satisfied": {"badge_fill": SELECTED, "badge_text": ACTION, "label": "✓ 필수 현재/최소"},
        "scope": "건축 전용 Catalog panel/card 및 CatalogLauncher만. 다른 팀원 UI/Shared/Legacy 변경 없음.",
        "runtime_execution": False,
    }

    def require(widgets, names, package):
        missing = [name for name in names if name not in widgets]
        if missing:
            raise RuntimeError("Catalog reference patch missing widgets in %s: %s" % (package, missing))

    def margin(left=0, top=0, right=0, bottom=0):
        return unreal.Margin(float(left), float(top), float(right), float(bottom))

    def rounded(fill, radius=8, stroke_color=LINE, stroke_width=1):
        brush = make_brush(fill, radius)
        outline = brush.get_editor_property("outline_settings")
        outline.set_editor_property("color", slate(stroke_color))
        outline.set_editor_property("width", float(stroke_width))
        brush.set_editor_property("outline_settings", outline)
        return brush

    def exact_size(widget, width, height):
        widget.clear_min_desired_width()
        widget.clear_max_desired_width()
        widget.clear_min_desired_height()
        widget.clear_max_desired_height()
        widget.set_width_override(float(width))
        widget.set_height_override(float(height))

    def text_style(widget, size, tint, centered=False):
        if widget.get_class().get_name() == "CommonTextBlock":
            widget.set_style(None)
        font = widget.get_editor_property("font")
        font.set_editor_property("size", int(size))
        widget.set_font(font)
        widget.set_color_and_opacity(slate(tint))
        widget.set_editor_property("justification", unreal.TextJustify.CENTER if centered else unreal.TextJustify.LEFT)

    def content_texts(widget):
        if isinstance(widget, unreal.TextBlock):
            yield widget
        elif isinstance(widget, unreal.PanelWidget):
            # Old content-only wrappers must not force 180px content inside a 160px button.
            # Preserve small icon boxes and the separate introduction/reward badge siblings.
            if isinstance(widget, unreal.SizeBox):
                if widget.get_editor_property("width_override") > BUTTON_WIDTH - 24:
                    widget.clear_width_override()
                    widget.clear_min_desired_width()
                    widget.clear_max_desired_width()
                if widget.get_editor_property("height_override") > BUTTON_HEIGHT - 12:
                    widget.clear_height_override()
                    widget.clear_min_desired_height()
                    widget.clear_max_desired_height()
            for child in widget.get_all_children():
                yield from content_texts(child)

    def normalize_button_ancestors(button, outer_size):
        current = button
        while current and current != outer_size:
            slot = current.get_editor_property("slot")
            if slot:
                if hasattr(slot, "set_padding"):
                    slot.set_padding(margin())
                if hasattr(slot, "set_horizontal_alignment"):
                    slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
                if hasattr(slot, "set_vertical_alignment"):
                    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
            current = current.get_parent()
            if isinstance(current, unreal.SizeBox):
                exact_size(current, BUTTON_WIDTH, BUTTON_HEIGHT)
            elif isinstance(current, unreal.Border):
                current.set_padding(margin())
        if current != outer_size:
            raise RuntimeError("Catalog button is not inside its expected size wrapper")

    def button_style(widget, primary=False):
        style = widget.get_editor_property("widget_style")
        fills = {
            "normal": ACTION if primary else SURFACE,
            "hovered": "#225B68" if primary else SELECTED,
            "pressed": "#1D515D" if primary else SELECTED,
            "disabled": "#ECEEF5",
        }
        for key, fill in fills.items():
            style.set_editor_property(key, rounded(fill, 8, ACTION if primary or key in ("hovered", "pressed") else LINE))
        style.set_editor_property("normal_padding", margin(12, 6, 12, 6))
        style.set_editor_property("pressed_padding", margin(12, 6, 12, 6))
        widget.set_style(style)
        child = widget.get_content()
        if child:
            slot = child.get_editor_property("slot")
            if slot and hasattr(slot, "set_padding"):
                slot.set_padding(margin())
            for label in content_texts(child):
                text_style(label, HEADER_FONT_SIZE, "#FFFFFF" if primary else INK, True)
                label.set_font(header_font)
        slot = widget.get_editor_property("slot")
        if isinstance(slot, unreal.SizeBoxSlot):
            slot.set_padding(margin())
            slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
            slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)

    def badge_style(border, label, fill, tint, authored_label):
        border.set_brush(rounded(fill, 8, fill, 0))
        border.set_brush_color(color("#FFFFFF"))
        border.set_padding(margin(6, 2, 6, 2))
        text_style(label, 12, tint)
        label.set_text(authored_label)
        label.set_editor_property("auto_wrap_text", False)
        label.set_text_overflow_policy(unreal.TextOverflowPolicy.ELLIPSIS)
        label.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
        slot = label.get_editor_property("slot")
        if isinstance(slot, unreal.BorderSlot):
            slot.set_padding(margin(6, 2, 6, 2))

    # Compile the nested launcher before loading its host; Unreal reinstantiates embedded instances.
    launcher_bp, launcher = context["widgets"](LAUNCHER)
    require(launcher, ["Size_Launcher", "Button_OpenCodex"], LAUNCHER)
    exact_size(launcher["Size_Launcher"], BUTTON_WIDTH, BUTTON_HEIGHT)
    normalize_button_ancestors(launcher["Button_OpenCodex"], launcher["Size_Launcher"])
    button_style(launcher["Button_OpenCodex"])
    unreal.BlueprintEditorLibrary.compile_blueprint(launcher_bp)

    card_bp, card = context["widgets"](CARD)
    require(card, ["SizeBox_Card", "SizeBox_Thumbnail", "Size_PearlFacilityName", "Text_Name",
                   "Border_RequiredMissing", "Text_RequiredMissing", "Border_RequiredSatisfied", "Text_RequiredSatisfied"], CARD)
    exact_size(card["SizeBox_Card"], 184, 210)
    exact_size(card["SizeBox_Thumbnail"], 162, 104)
    exact_size(card["Size_PearlFacilityName"], 162, 48)
    text_style(card["Text_Name"], 16, INK, True)
    card["Text_Name"].set_editor_property("auto_wrap_text", True)
    card["Text_Name"].set_editor_property("wrap_text_at", 160.0)
    badge_style(card["Border_RequiredMissing"], card["Text_RequiredMissing"], DANGER, "#FFFFFF", "! 필수")
    badge_style(card["Border_RequiredSatisfied"], card["Text_RequiredSatisfied"], SELECTED, ACTION, "✓ 필수")
    unreal.BlueprintEditorLibrary.compile_blueprint(card_bp)
    generated = unreal.EditorAssetLibrary.load_blueprint_class(CARD)
    if not generated:
        raise RuntimeError("Could not load catalog card generated class")
    card_cdo = unreal.get_default_object(generated)
    card_cdo.set_editor_property("required_card_tint", color(REQUIRED_SURFACE))
    card_cdo.set_editor_property("required_outline_color", color(DANGER))
    card_cdo.set_editor_property("required_thumbnail_brush", rounded(REQUIRED_SURFACE, 8, REQUIRED_SURFACE, 0))

    panel_bp, panel = context["widgets"](PANEL)
    require(panel, ["Size_PearlRevenueRanking", "Button_RevenueRanking",
                    "Text_FavoritesOnly", "Text_FavoriteCount", "Search_Facilities"], PANEL)
    exact_size(panel["Size_PearlRevenueRanking"], BUTTON_WIDTH, BUTTON_HEIGHT)
    normalize_button_ancestors(panel["Button_RevenueRanking"], panel["Size_PearlRevenueRanking"])
    button_style(panel["Button_RevenueRanking"])
    for name in ("Text_FavoritesOnly", "Text_FavoriteCount"):
        label = panel[name]
        if label.get_class().get_name() == "CommonTextBlock":
            label.set_style(None)
        label.set_font(header_font)
    search = panel["Search_Facilities"]
    search_style = search.get_editor_property("widget_style")
    search_text_style = search_style.get_editor_property("text_style")
    search_text_style.set_editor_property("font", header_font)
    search_style.set_editor_property("text_style", search_text_style)
    search.set_editor_property("widget_style", search_style)
    # EditableTextBox uses the same TextStyle font for editable text and HintText.
    # Also update the direct setter on engine versions that expose a font override.
    if hasattr(search, "set_font"):
        search.set_font(header_font)
    launcher_instance = panel.get("Widget_FacilityCodexCatalogLauncher")
    if launcher_instance:
        launcher_instance.set_padding(margin())
    unreal.BlueprintEditorLibrary.compile_blueprint(panel_bp)

    summary["designer_nodes"] = {
        PANEL: ["Size_PearlRevenueRanking", "Button_RevenueRanking", "Text_RevenueRanking",
                "Text_FavoritesOnly", "Text_FavoriteCount", "Search_Facilities"],
        LAUNCHER: ["Size_Launcher", "Button_OpenCodex", "Button_OpenCodex_Label"],
        CARD: ["Border_RequiredMissing", "Text_RequiredMissing", "Border_RequiredSatisfied", "Text_RequiredSatisfied"],
    }
    summary["runtime_dependencies"] = [
        "FF_PrepBuildTileWidget::ApplyRequiredFacilityStatus uses live count labels.",
        "FF_PrepBuildTileWidget::RefreshVisualState retains the required 2px outline.",
        "FF_BuildCatalogWebView snapshot includes requiredCurrent and requiredMinimum.",
        "Introduction/reward indicator overlays are managed by the separate discovery module.",
    ]
    return PACKAGES[:], summary


def apply_defaults(context):
    """중앙 실행기의 마지막 Blueprint 컴파일 후 카드 CDO 스타일을 다시 적용한다."""
    generated = unreal.EditorAssetLibrary.load_blueprint_class(CARD)
    if not generated:
        raise RuntimeError("Could not load catalog card generated class after compilation")
    card_cdo = unreal.get_default_object(generated)
    thumbnail = context["brush"](REQUIRED_SURFACE, 8)
    outline = thumbnail.get_editor_property("outline_settings")
    outline.set_editor_property("width", 0.0)
    outline.set_editor_property("color", context["slate"](REQUIRED_SURFACE))
    thumbnail.set_editor_property("outline_settings", outline)
    card_cdo.set_editor_property("required_card_tint", context["color"](REQUIRED_SURFACE))
    card_cdo.set_editor_property("required_outline_color", context["color"](DANGER))
    card_cdo.set_editor_property("required_thumbnail_brush", thumbnail)
    return {"package": CARD, "required_card_tint": REQUIRED_SURFACE,
            "required_outline_color": DANGER, "required_thumbnail_fill": REQUIRED_SURFACE,
            "applied_after_final_compile": True}
