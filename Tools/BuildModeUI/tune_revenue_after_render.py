"""One-time follow-up for the just-created revenue WBP, after 720p render QA."""
import unreal

BASE = "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/"
VERSION = "FF.Revenue.Compact20260919"


def edit(name, fonts, paddings, sizes):
    asset = unreal.EditorAssetLibrary.load_asset(BASE + name)
    assert asset
    if unreal.EditorAssetLibrary.get_metadata_tag(asset, VERSION) == "1":
        return
    widgets = {}
    for widget in unreal.ObjectIterator(unreal.Widget):
        tree = widget.get_outer()
        if tree and tree.get_class().get_name() == "WidgetTree" and tree.get_outer() == asset:
            widgets[widget.get_name()] = widget
    for key, size in fonts.items():
        font = widgets[key].get_editor_property("font")
        font.size = size
        widgets[key].set_font(font)
    for key, (horizontal, vertical) in paddings.items():
        widgets[key].set_padding(unreal.Margin(horizontal, vertical, horizontal, vertical))
    for key, (width, height) in sizes.items():
        if width:
            widgets[key].set_width_override(width)
        if height:
            widgets[key].set_height_override(height)
    if name == "WBP_FF_FacilityRevenueRanking":
        row=widgets["Row_SelectedMetrics"]
        children=[widgets[key] for key in ("Text_SelectedName", "Text_SelectedRank", "Text_ShareLabel", "Text_SelectedShare")]
        row.remove_from_parent()
        row.clear_children()
        widgets["Border_Selected"].set_content(row)
        for index, child in enumerate(children):
            child.remove_from_parent()
            slot=row.add_child_to_horizontal_box(child)
            slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            slot.set_padding(unreal.Margin(0,0,8 if index != 1 else 12,0))
            if index == 0:
                slot.set_size(unreal.SlateChildSize(1,unreal.SlateSizeRule.FILL))
                child.set_auto_wrap_text(False)
                child.set_text_overflow_policy(unreal.TextOverflowPolicy.ELLIPSIS)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, VERSION, "1")
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


edit("WBP_FF_FacilityRevenueRankRow", {"Text_Name": 16, "Text_Location": 11, "Text_Amount": 18},
     {"Border_Row": (10, 8)}, {"Size_Row": (0, 88), "Size_Thumbnail": (56, 56)})
edit("WBP_FF_FacilityRevenueRanking",
     {"Text_TotalLabel": 13, "Text_Total": 26, "Text_Count": 12,
      "Text_SelectedName": 16, "Text_SelectedRank": 18, "Text_SelectedShare": 18},
     {"Border_Total": (8, 8), "Border_Selected": (8, 8)}, {})
unreal.log("FF_REVENUE_COMPACT_QA_APPLIED")
