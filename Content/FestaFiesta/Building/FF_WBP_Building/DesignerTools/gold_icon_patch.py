"""Use the bottom HUD's existing gold artwork in the native catalog fallback.

The web ribbon embeds its canonical PNG through WebUI/sync_to_umg.py.
Never import, modify, or save the shared HUD texture or any HUD widget here.
"""
import unreal

CARD = '/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile'
GOLD_TEXTURE = '/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics/T_FF_LH_Icon_Gold_White_256'
PACKAGES = [CARD]
ICON_SIZE = 20.0
ICON_GAP = 7.0


def apply(context):
    bp, widgets = context['widgets'](CARD)
    texture = unreal.load_asset(GOLD_TEXTURE)
    assert isinstance(texture, unreal.Texture2D), GOLD_TEXTURE
    bp.modify()
    border, cost = widgets['Border_AuroraCardGold'], widgets['Text_Cost']
    tree = border.get_outer()

    def ensure(cls, name):
        widget = widgets.get(name)
        if widget is None:
            widget = unreal.new_object(cls, outer=tree, name=name)
        assert isinstance(widget, cls), name
        return widget

    row = ensure(unreal.HorizontalBox, 'Row_HudGoldCost')
    image = ensure(unreal.Image, 'Image_CostGold')
    bounds = ensure(unreal.SizeBox, 'Size_CostGold')
    cost.remove_from_parent()
    row.clear_children()
    border.set_content(row)
    bounds.set_width_override(ICON_SIZE)
    bounds.set_height_override(ICON_SIZE)
    bounds.set_content(image)
    image.set_brush_from_texture(texture, match_size=False)
    image.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
    image.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    image.set_tool_tip_text('골드')
    icon_slot = row.add_child_to_horizontal_box(bounds)
    icon_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    icon_slot.set_padding(unreal.Margin(0, 0, ICON_GAP, 0))
    cost_slot = row.add_child_to_horizontal_box(cost)
    cost_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    cost_slot.set_size(unreal.SlateChildSize(value=1.0, size_rule=unreal.SlateSizeRule.FILL))
    cost_slot.set_padding(unreal.Margin())
    row.get_editor_property('slot').set_padding(unreal.Margin())
    return PACKAGES, {'gold_texture': GOLD_TEXTURE, 'icon_size': ICON_SIZE,
                      'icon_gap': ICON_GAP, 'shared_assets_modified': False}
