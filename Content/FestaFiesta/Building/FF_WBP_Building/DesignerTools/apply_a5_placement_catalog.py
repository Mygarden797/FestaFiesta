"""Style the existing placement inspector, catalog scroll affordances and facility badges."""
import unreal
from pathlib import Path
# Share only the style helper definitions, without replaying earlier migrations.
helper = Path(__file__).with_name('apply_a5_inspector_reference.py').read_text(encoding='utf-8')
exec(helper[:helper.index("bp, w = load('Catalog/WBP_FF_PrepBuildTile')")])

def fill_slot(slot, fill=True):
    size=slot.get_editor_property('size')
    size.set_editor_property('size_rule',unreal.SlateSizeRule.FILL if fill else unreal.SlateSizeRule.AUTOMATIC)
    slot.set_editor_property('size',size)

def text_style(x,text,size=13,tint='354259'):
    x.set_text(text)
    f=x.get_editor_property('font')
    f.set_editor_property('font_object',unreal.load_asset('/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard'))
    f.set_editor_property('typeface_font_name','Regular')
    x.set_font(f)
    font(x,size,tint)

bp,w=load('Controls/WBP_FF_BuildTools')
size=w['Size_PlacementInfo']
size.set_width_override(390); size.set_height_override(630)
size.slot.set_anchors(unreal.Anchors(unreal.Vector2D(1,1),unreal.Vector2D(1,1)))
size.slot.set_alignment(unreal.Vector2D(1,1))
size.slot.set_offsets(unreal.Margin(-24,-64,390,630));size.slot.set_auto_size(False)
surface(w['Border_PlacementInfo'],'F8F8FE',15)
w['Border_PlacementInfo'].set_padding(unreal.Margin(12,12,12,12))
shell=w['Column_PearlSummaryContent']
children=[w['Text_A5PlacementTitle'],w['Scroll_PearlPlacementSummary'],w['Button_DirectAdjustment'],w['Row_PlacementAssistance']]
shell.clear_children()
for child in children:
    child.remove_from_parent();slot=shell.add_child_to_vertical_box(child)
    slot.set_padding(unreal.Margin(0,0,0,8))
    fill_slot(slot,child==w['Scroll_PearlPlacementSummary'])
text_style(w['Text_A5PlacementTitle'],'시설 정보',21)
surface(w['Border_BuildPlacementHero'],'EAF2F8',7)
w['Border_BuildPlacementHero'].set_padding(unreal.Margin(10,10,10,10))
w['Border_BuildPlacementHero'].slot.set_padding(unreal.Margin(0,4,0,8))
font(w['Text_FacilityName'],19)
font(w['Text_Mode'],12,'33869A')
w['Size_PlacementFacilityThumb'].set_visibility(unreal.SlateVisibility.COLLAPSED)
surface(w['Border_A5InstallCard'],'F0F4FA',7)
w['Border_A5InstallCard'].set_padding(unreal.Margin(10,8,10,8))
text_style(w['Text_A5InstallHeading'],'설치 정보',14,'33869A')
for name in ['Text_Cost','Text_Footprint']:
    x=w[name];x.remove_from_parent()
    slot=w['Column_A5InstallCard'].add_child_to_vertical_box(x)
    slot.set_padding(unreal.Margin(0,6,0,6));font(x,13)
surface(w['Border_PlacementSpace'],'F0F4FA',7)
w['Border_PlacementSpace'].set_padding(unreal.Margin(10,8,10,8))
text_style(w['Text_PlacementSpaceTitle'],'운영 공간',14,'33869A')
for name in ['Text_Queue','Text_Audience','Text_RangeLegend','Text_Legend']:
    font(w[name],13)
column=w['Column_ClaritySummary']
ordered=['Border_BuildPlacementHero','Border_A5InstallCard','Border_PlacementSpace','Text_Legend',
         'Panel_PlacementFacts','Text_PlacementAdvice','Text_PlacementObstruction','Text_AssistanceMessage']
column.clear_children()
for name in ordered:
    x=w[name];x.remove_from_parent();slot=column.add_child_to_vertical_box(x)
    slot.set_padding(unreal.Margin(0,0,0,8))
save(bp)

bp,w=load('Catalog/WBP_FF_PrepPanelBuild')
footer=w['Row_A5CatalogFooter']
footer.slot.set_padding(unreal.Margin(4,8,4,0))
fill_slot(footer.slot,False)
fill_slot(w['Text_A5CatalogDragHint'].slot)
text_style(w['Text_A5CatalogDragHint'],'위아래로 드래그하거나 휠로 더 보기',12,'8B99AF')
view=w['TileView_Facilities']
style=view.get_editor_property('scroll_bar_style')
for prop,col in [('vertical_background_image','E4EDF2'),('normal_thumb_image','80B9C2'),
                 ('hovered_thumb_image','388B9B'),('dragged_thumb_image','287789')]:
    b=brush(style.get_editor_property(prop),col,col,4,0)
    style.set_editor_property(prop,b)
style.set_editor_property('thickness',8)
view.set_editor_property('scroll_bar_style',style)
view.set_scrollbar_visibility(unreal.SlateVisibility.VISIBLE)
save(bp)

textures={'Lighting':'/Game/FestaFiesta/UI/PrepHUD/Icons/T_FF_PrepRecommended_Lighting',
          'Program':'/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_Calendar_White_256'}
colors={'Required':'D63B49','Lighting':'348C9C','Program':'8A74AC'}
labels={'Required':'필수','Lighting':'조명','Program':'프로그램'}
def icon(border,child,key):
    surface(border,colors[key],10,0)
    border.set_padding(unreal.Margin(3,2,3,2))
    if isinstance(border.slot,unreal.HorizontalBoxSlot):
        border.slot.set_padding(unreal.Margin(0,0,4,0))
        border.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
    if key=='Required':text_style(child,'!',12,'FFFFFF')
    else:
        child.set_brush_from_texture(unreal.load_asset(textures[key]),False)
        b=child.get_editor_property('brush')
        dimensions=b.get_editor_property('image_size')
        dimensions.set_editor_property('x',14);dimensions.set_editor_property('y',16)
        b.set_editor_property('image_size',dimensions)
        child.set_editor_property('brush',b)
    border.set_tool_tip_text(labels[key])

for key in labels:
    icon(w['Badge_A5Legend'+key],w['Icon_A5Legend'+key],key)
    text_style(w['Text_A5Legend'+key],labels[key],12,'8B99AF')
    w['Text_A5Legend'+key].slot.set_padding(unreal.Margin(0,0,10,0))
save(bp)

bp,w=load('Catalog/WBP_FF_PrepBuildTile')
for name in ['Badge_RequiredIcon_Legacy','Badge_Lighting_Legacy']:
    w[name].set_visibility(unreal.SlateVisibility.COLLAPSED)
for key,border,child in [('Required','Badge_RequiredIcon','Text_A5Required'),
                         ('Lighting','Badge_Lighting','Image_A5Lighting'),
                         ('Program','Border_ProgramBadge','Image_A5Program')]:
    icon(w[border],w[child],key)
row=w['HorizontalBox_ClarityFacilityBadges']
row.clear_children()
for name in ['Badge_RequiredIcon','Badge_Lighting','Border_ProgramBadge','Border_RequiredMissing',
             'Border_RequiredSatisfied','Badge_RequiredIcon_Legacy','Badge_Lighting_Legacy']:
    x=w[name];x.remove_from_parent();slot=row.add_child_to_horizontal_box(x)
    slot.set_padding(unreal.Margin(0,0,4,0));slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
save(bp)
print('Placement inspector, scroll indicator, and data-driven badges saved.')
