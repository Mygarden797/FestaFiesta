"""Author the approved A5 card/toolbar corrections. No gameplay or tests."""
import unreal

BASE = '/Game/FestaFiesta/Building/FF_WBP_Building/'
def load(path):
    bp = unreal.load_asset(path)
    assert bp
    bp.modify()
    widgets = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
               if w.get_outer() and w.get_outer().get_outer() == bp}
    for w in widgets.values():
        w.modify()
        if w.slot: w.slot.modify()
    return bp, widgets

def save(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)

def flat(button):
    for prop in ['widget_style', 'selection_style']:
        try: style = button.get_editor_property(prop)
        except Exception: continue
        for state in ['normal', 'hovered', 'pressed', 'disabled']:
            brush = style.get_editor_property(state)
            brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.NO_DRAW_TYPE)
            style.set_editor_property(state, brush)
        style.set_editor_property('normal_padding', unreal.Margin(0, 0, 0, 0))
        style.set_editor_property('pressed_padding', unreal.Margin(0, 0, 0, 0))
        button.set_editor_property(prop, style)

bp, w = load(BASE + 'Catalog/WBP_FF_PrepBuildTile')
w['Size_Favorite'].slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
w['Size_Favorite'].slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT)
w['Size_Favorite'].set_width_override(24)
w['Size_Favorite'].set_height_override(24)
w['HorizontalBox_ClarityFacilityBadges'].slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
w['HorizontalBox_ClarityFacilityBadges'].slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
w['SizeBox_Thumbnail'].set_width_override(100)
w['SizeBox_Thumbnail'].set_height_override(96)
save(bp)

bp, w = load(BASE + 'Controls/WBP_FF_BuildTools')
w['Size_BuildTopBar'].set_width_override(521)  # whole-grid addon contributes the remaining 57
w['Size_BuildTopBar'].set_height_override(56)
w['Size_BuildTopBar'].slot.set_position(unreal.Vector2D(0, 0))
w['Border_BuildTopBar'].set_padding(unreal.Margin(14, 4, 14, 4))
row = w['Row_BuildReferenceTopBar']
for cell in row.get_all_children():
    if isinstance(cell, unreal.SizeBox):
        cell.set_width_override(48)
        cell.set_height_override(46)
        cell.slot.set_padding(unreal.Margin(0, 0, 9, 0))
        button = cell.get_child_at(0)
        if isinstance(button, unreal.Button): flat(button)
for name in ['Button_BuildHelpUndo']:
    flat(w[name])
for name, widget in w.items():
    if name.startswith('Badge_'):
        widget.set_render_opacity(0)
w['Border_PlacementHelp'].set_padding(unreal.Margin(0, 0, 0, 0))
brush = w['Border_PlacementHelp'].get_editor_property('background')
brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.NO_DRAW_TYPE)
w['Border_PlacementHelp'].set_editor_property('background', brush)
w['Size_BuildWorldHelp'].set_width_override(305)
w['Size_BuildWorldHelp'].slot.set_position(unreal.Vector2D(24, 385))
save(bp)

bp, w = load(BASE + 'Catalog/WBP_FF_PrepPanelBuild')
w['Size_ClarityCatalogDock'].set_height_override(370)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('catalog_height', 370)
save(bp)

bp, w = load('/FF_MapOverview/UI/WBP_FF_WholeGridButton')
w['Bounds'].set_width_override(48)
w['Bounds'].set_height_override(46)
for widget in w.values():
    if isinstance(widget, unreal.Button): flat(widget)
    if isinstance(widget, unreal.TextBlock) and str(widget.get_text()) == '전체 격자':
        widget.set_visibility(unreal.SlateVisibility.COLLAPSED)
    if isinstance(widget, unreal.SizeBox) and isinstance(widget.slot, unreal.CanvasPanelSlot):
        widget.set_width_override(48)
        widget.set_height_override(46)
cdo = unreal.get_default_object(bp.generated_class())
style = cdo.get_editor_property('selection_style')
for state in ['normal', 'hovered', 'pressed', 'disabled']:
    brush = style.get_editor_property(state)
    brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.NO_DRAW_TYPE)
    style.set_editor_property(state, brush)
cdo.set_editor_property('selection_style', style)
save(bp)
print('A5 docking and icon layout saved')
