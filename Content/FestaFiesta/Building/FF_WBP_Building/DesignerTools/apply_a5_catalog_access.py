"""Targeted A5 correction: bounded catalog scrolling, wider inspector, circular badges.
The six Size_A5*Badge widgets are authored in the WBP before this migration.
"""
import unreal
from pathlib import Path

helper = Path(__file__).with_name('apply_a5_inspector_reference.py').read_text(encoding='utf-8')
exec(helper[:helper.index("bp, w = load('Catalog/WBP_FF_PrepBuildTile')")])

bp, w = load('Catalog/WBP_FF_PrepPanelBuild')
w['Size_ClarityCatalogDock'].set_width_override(1290)  # previous 1450 minus codex button 160
w['Size_ClarityCatalogDock'].set_height_override(416)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('catalog_height',416)
for name in ['Panel_Catalog','VerticalBox_CatalogBody','VerticalBox_Browser']:
    slot=w[name].slot
    sizing=slot.get_editor_property('size')
    sizing.set_editor_property('size_rule',unreal.SlateSizeRule.FILL)
    slot.set_editor_property('size',sizing)
rows=w['Size_A5CatalogRows']; cards=w['Overlay_CatalogCards']; browser=w['VerticalBox_Browser']
cards.remove_from_parent(); rows.remove_from_parent();rows.set_content(cards)
rows.set_height_override(224)  # two rows, matching the authored entry height of 112
browser.clear_children()
for x in [w['Size_SubcategoryHeader'],rows,w['Row_A5CatalogFooter']]:
    x.remove_from_parent(); slot=browser.add_child_to_vertical_box(x)
    if x==w['Row_A5CatalogFooter']:slot.set_padding(unreal.Margin(4,8,4,0))
w['TileView_Facilities'].set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS)
w['Text_A5CatalogDragHint'].set_text('위아래로 드래그 · 휠 / 오른쪽 스크롤바로 더 보기')
font(w['Text_A5CatalogDragHint'],12,'71849B')
save(bp)

bp, w = load('Controls/WBP_FF_BuildTools')
w['Size_PlacementInfo'].set_width_override(550)
w['Size_PlacementInfo'].slot.set_size(unreal.Vector2D(550,630))
save(bp)

bp, w = load('Facility/WBP_FF_FacilityInfoPanel')
unreal.get_default_object(bp.generated_class()).set_editor_property('build_inspector_width',550)
for name in ['Panel_Size','Panel_ShadowSize']:
    w[name].set_width_override(550)
    w[name].slot.set_size(unreal.Vector2D(550,630))
save(bp)

colors={'Required':'D63B49','Lighting':'348C9C','Program':'8A74AC'}
for suffix,legend in [('Catalog/WBP_FF_PrepBuildTile',False),('Catalog/WBP_FF_PrepPanelBuild',True)]:
    bp,w=load(suffix)
    for key in colors:
        border_name=('Badge_A5Legend'+key) if legend else {'Required':'Badge_RequiredIcon','Lighting':'Badge_Lighting','Program':'Border_ProgramBadge'}[key]
        child_name=('Icon_A5Legend'+key) if legend else {'Required':'Text_A5Required','Lighting':'Image_A5Lighting','Program':'Image_A5Program'}[key]
        border=w[border_name]; child=w[child_name]; size=w['Size_A5'+key+'Badge']
        child.remove_from_parent(); size.remove_from_parent()
        border.set_content(size); size.set_content(child)
        diameter=24 if legend else 28
        size.set_width_override(diameter); size.set_height_override(diameter)
        border.set_padding(unreal.Margin(0,0,0,0))
        surface(border,colors[key],diameter/2,0)
        child.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        child.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        if isinstance(child,unreal.TextBlock):
            required_brush=border.get_editor_property('background')
            required_brush.set_editor_property('draw_as',unreal.SlateBrushDrawType.NO_DRAW_TYPE)
            border.set_editor_property('background',required_brush)
            child.set_text('★'); child.set_editor_property('justification',unreal.TextJustify.CENTER)
            font(child,20 if legend else 22,colors[key])
        else:
            b=child.get_editor_property('brush'); dimensions=b.get_editor_property('image_size')
            dimensions.set_editor_property('x',16); dimensions.set_editor_property('y',16)
            b.set_editor_property('image_size',dimensions);child.set_editor_property('brush',b)
        if isinstance(border.slot,unreal.HorizontalBoxSlot):
            border.slot.set_padding(unreal.Margin(0,0,4,0))
            border.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    if not legend:
        # Artwork fits each responsive column, while badges/favorites keep their circular shape.
        w['SizeBox_Thumbnail'].clear_width_override()
        w['SizeBox_Thumbnail'].slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        w['ScaleBox_Thumbnail'].slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        w['ScaleBox_Thumbnail'].slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    save(bp)
print('A5 catalog viewport, inspector widths, and round badges saved.')
