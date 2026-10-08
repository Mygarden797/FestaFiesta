"""Authored first-use indicators and explanations for Building entry points.

The central runner owns backup, compilation and saving. This module edits only
the existing Building catalog, revenue panel and codex launcher/screen assets.
Native progress events own visibility; no preview values or rewards are granted.
"""

import unreal

BASE = "/Game/FestaFiesta/Building/FF_WBP_Building/"
CATALOG = BASE + "Catalog/WBP_FF_PrepPanelBuild"
RANKING = BASE + "FacilityRevenue/WBP_FF_FacilityRevenueRanking"
CODEX = BASE + "FacilityCodex/WBP_FF_FacilityCodexScreen"
LAUNCHERS = [BASE + "FacilityCodex/WBP_FF_FacilityCodexCatalogLauncher",
             BASE + "FacilityCodex/WBP_FF_FacilityCodexLauncher"]
PACKAGES = [*LAUNCHERS, CODEX, RANKING, CATALOG]

SURFACE = "#F9F9FF"
INK = "#3D3C4F"
MUTED = "#626577"
ACTION = "#286A79"
DANGER = "#D93649"
REWARD = "#986515"
FONT = "/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Font"
FIRST_USE_BADGE_SIZE = 20.0
REWARD_BADGE_SIZE = 18.0


def apply(context):
    color, slate, make_brush = context["color"], context["slate"], context["brush"]
    font_asset = unreal.load_asset(FONT)
    if not font_asset:
        raise RuntimeError("Missing existing project font: " + FONT)
    summary = {
        "packages": PACKAGES,
        "first_use_badge": {"size": FIRST_USE_BADGE_SIZE, "label": "!", "color": DANGER},
        "reward_badge": {"size": REWARD_BADGE_SIZE, "label": "G", "color": REWARD,
                         "source": "Codex.HasUnclaimedRewards; independent of first-use state"},
        "state": "Profile first-use state is consumed only after the authored explanation is shown.",
        "native_rewards": "No reward grant, claim, discovery or ledger mutations.",
        "button_dimensions": "Owned by catalog_reference_patch; sibling overlays do not add desired width.",
    }

    def margin(l=0, t=0, r=0, b=0):
        return unreal.Margin(float(l), float(t), float(r), float(b))

    def need(widgets, names, package):
        missing = [name for name in names if name not in widgets]
        if missing:
            raise RuntimeError("Missing authored widgets in %s: %s" % (package, missing))

    def ensure(widgets, tree, cls, name):
        widget = widgets.get(name)
        if widget is None:
            widget = unreal.new_object(cls, outer=tree, name=name)
            widgets[name] = widget
        elif not isinstance(widget, cls):
            raise RuntimeError("Authored name occupied by another class: " + name)
        return widget

    def text(widget, value, size=14, tint=INK, wrap=False):
        widget.set_text(value)
        info = widget.get_editor_property("font")
        info.set_editor_property("font_object", font_asset)
        info.set_editor_property("size", size)
        widget.set_font(info)
        widget.set_color_and_opacity(slate(tint))
        widget.set_auto_wrap_text(wrap)
        widget.set_editor_property('justification', unreal.TextJustify.LEFT)
        widget.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)

    def fill(slot):
        slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
        slot.set_padding(margin())

    def overlay_add(overlay, child):
        if child.get_parent() != overlay:
            child.remove_from_parent()
            return overlay.add_child_to_overlay(child)
        return child.get_editor_property("slot")

    def badge(widgets, tree, overlay, size_name, border_name, label_name, label, tint, dimension,
              bottom=False):
        bounds = ensure(widgets, tree, unreal.SizeBox, size_name)
        bounds.set_width_override(dimension)
        bounds.set_height_override(dimension)
        bounds.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
        slot = overlay_add(overlay, bounds)
        slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT)
        slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM if bottom
                                    else unreal.VerticalAlignment.V_ALIGN_TOP)
        slot.set_padding(margin(0, 2 if not bottom else 0, 2, 2 if bottom else 0))
        border = ensure(widgets, tree, unreal.Border, border_name)
        bounds.set_content(border)
        border.set_brush(make_brush(tint, dimension / 2.0))
        border.set_brush_color(color("#FFFFFF"))
        border.set_padding(margin())
        border.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        border.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        border.set_render_scale(unreal.Vector2D(1.0, 1.0))
        border.set_visibility(unreal.SlateVisibility.COLLAPSED)
        label_widget = ensure(widgets, tree, unreal.TextBlock, label_name)
        border.set_content(label_widget)
        text(label_widget, label, 12 if label == "!" else 9, "#FFFFFF")
        label_widget.set_editor_property('justification', unreal.TextJustify.CENTER)
        return border

    for package in LAUNCHERS:
        bp, widgets = context["widgets"](package)
        need(widgets, ["Overlay_IntroductionLauncher", "Button_OpenCodex"], package)
        bp.modify()
        overlay = widgets["Overlay_IntroductionLauncher"]
        tree = overlay.get_outer()
        badge(widgets, tree, overlay, "Size_IntroductionHint", "Border_IntroductionHint",
              "Text_IntroductionHint", "!", DANGER, FIRST_USE_BADGE_SIZE)
        badge(widgets, tree, overlay, "Size_RewardHint", "Border_RewardHint", "Text_RewardHint",
              "G", REWARD, REWARD_BADGE_SIZE, bottom=True)

    bp, widgets = context["widgets"](CODEX)
    need(widgets, ["Border_Introduction", "Text_IntroductionTitle", "Text_IntroductionCard",
                   "Text_IntroductionDistance", "Text_IntroductionRewards", "Text_DismissIntroduction"], CODEX)
    bp.modify()
    text(widgets["Text_IntroductionTitle"], "시설 조합으로 보너스를 찾아보세요", 20, INK, True)
    text(widgets["Text_IntroductionCard"], "조합 카드를 누르면 필요한 짝 시설, 배치 거리와 효과를 확인할 수 있어요.", 14, MUTED, True)
    text(widgets["Text_IntroductionDistance"], "조건에 맞게 두 시설을 배치하면 조합이 발견되고, 현재 적용 중인 효과를 볼 수 있어요.", 14, MUTED, True)
    text(widgets["Text_IntroductionRewards"], "받을 수 있는 보상은 ‘보상 받기’로 직접 수령하세요. 버튼의 금색 G 표시는 아직 받을 보상이 있다는 뜻이에요.", 14, MUTED, True)
    text(widgets["Text_DismissIntroduction"], "도감 살펴보기", 14, "#FFFFFF")
    widgets["Text_DismissIntroduction"].set_editor_property('justification', unreal.TextJustify.CENTER)
    widgets["Border_Introduction"].set_visibility(unreal.SlateVisibility.COLLAPSED)

    bp, widgets = context["widgets"](RANKING)
    need(widgets, ["Border_Panel", "Button_Close", "List_Ranking"], RANKING)
    bp.modify()
    panel = widgets["Border_Panel"]
    tree = panel.get_outer()
    overlay = widgets.get("Overlay_RevenueIntroduction")
    if overlay is None:
        original = panel.get_content()
        if original is None:
            raise RuntimeError("Revenue panel has no authored content")
        original_slot = original.get_editor_property("slot")
        saved = {name: original_slot.get_editor_property(name)
                 for name in ("padding", "horizontal_alignment", "vertical_alignment")}
        original.remove_from_parent()
        overlay = ensure(widgets, tree, unreal.Overlay, "Overlay_RevenueIntroduction")
        panel.set_content(overlay)
        for name, value in saved.items():
            overlay.get_editor_property("slot").set_editor_property(name, value)
        fill(overlay.add_child_to_overlay(original))
    elif overlay.get_parent() != panel:
        raise RuntimeError("Revenue introduction overlay was reparented")
    overlay.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    guide = ensure(widgets, tree, unreal.Border, "Border_Introduction")
    fill(overlay_add(overlay, guide))
    guide.set_brush(make_brush(SURFACE, 12))
    guide.set_brush_color(color("#FFFFFF"))
    guide.set_padding(margin(18, 12, 18, 12))
    guide.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    guide.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    guide.set_visibility(unreal.SlateVisibility.COLLAPSED)
    column = ensure(widgets, tree, unreal.VerticalBox, "Vertical_IntroductionGuide")
    guide.set_content(column)
    rows = [
        ("Text_IntroductionTitle", "어떤 시설이 수입을 만드는지 확인하세요", 20, INK, 14),
        ("Text_IntroductionPeriod", "오늘·축제 누적으로 기간을 바꿔, 설치된 시설의 실제 수입을 비교하세요.", 14, MUTED, 10),
        ("Text_IntroductionInspect", "시설 행을 선택하고 ‘시설 보기’를 누르면 해당 시설을 확인할 수 있어요.", 14, MUTED, 10),
        ("Text_IntroductionEmpty", "아직 수입이 없으면 0 Gold로 표시돼요. 수입이 발생하면 순위가 자동으로 갱신됩니다.", 14, MUTED, 18),
    ]
    for name, label, size, tint, space in rows:
        widget = ensure(widgets, tree, unreal.TextBlock, name)
        if widget.get_parent() != column:
            widget.remove_from_parent()
            column.add_child_to_vertical_box(widget)
        text(widget, label, size, tint, True)
        slot = widget.get_editor_property("slot")
        slot.set_padding(margin(0, 0, 0, space))
        slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        slot.set_size(unreal.SlateChildSize(value=1.0, size_rule=unreal.SlateSizeRule.AUTOMATIC))
    dismiss_size = ensure(widgets, tree, unreal.SizeBox, "Size_DismissIntroduction")
    dismiss_size.set_width_override(240)
    dismiss_size.set_height_override(44)
    if dismiss_size.get_parent() != column:
        dismiss_size.remove_from_parent()
        column.add_child_to_vertical_box(dismiss_size)
    dismiss_size.get_editor_property("slot").set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    button = ensure(widgets, tree, unreal.Button, "Button_DismissIntroduction")
    dismiss_size.set_content(button)
    style = button.get_editor_property("widget_style")
    for state, tint in (("normal", ACTION), ("hovered", "#347F8F"), ("pressed", "#1D5260"), ("disabled", "#CAD2E0")):
        style.set_editor_property(state, make_brush(tint, 8))
    style.set_editor_property("normal_padding", margin(12, 6, 12, 6))
    style.set_editor_property("pressed_padding", margin(12, 6, 12, 6))
    button.set_style(style)
    dismiss_label = ensure(widgets, tree, unreal.TextBlock, "Text_DismissIntroduction")
    button.set_content(dismiss_label)
    text(dismiss_label, "수익 순위 살펴보기", 14, "#FFFFFF")
    dismiss_label.set_editor_property('justification', unreal.TextJustify.CENTER)

    bp, widgets = context["widgets"](CATALOG)
    need(widgets, ["Button_RevenueRanking", "Size_PearlRevenueRanking"], CATALOG)
    bp.modify()
    button = widgets["Button_RevenueRanking"]
    bounds = widgets["Size_PearlRevenueRanking"]
    tree = button.get_outer()
    overlay = widgets.get("Overlay_RevenueIntroductionLauncher")
    if overlay is None:
        if button.get_parent() != bounds:
            raise RuntimeError("Ranking launcher hierarchy changed; expected the authored size box")
        old_slot = button.get_editor_property("slot")
        saved = {name: old_slot.get_editor_property(name)
                 for name in ("padding", "horizontal_alignment", "vertical_alignment")}
        button.remove_from_parent()
        overlay = ensure(widgets, tree, unreal.Overlay, "Overlay_RevenueIntroductionLauncher")
        bounds.set_content(overlay)
        for name, value in saved.items():
            overlay.get_editor_property("slot").set_editor_property(name, value)
        fill(overlay.add_child_to_overlay(button))
    elif button.get_parent() != overlay or overlay.get_parent() != bounds:
        raise RuntimeError("Ranking introduction launcher was reparented")
    overlay.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    badge(widgets, tree, overlay, "Size_RevenueIntroductionHint", "Border_RevenueIntroductionHint",
          "Text_RevenueIntroductionHint", "!", DANGER, FIRST_USE_BADGE_SIZE)
    button.set_tool_tip_text("시설별 오늘 수입과 축제 누적 수입을 비교합니다.")
    return PACKAGES, summary
