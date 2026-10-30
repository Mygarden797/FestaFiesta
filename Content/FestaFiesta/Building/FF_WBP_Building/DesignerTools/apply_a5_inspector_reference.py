"""Author the approved compact inspector and circular favorites in the existing WBP assets.
The assets remain editable in Designer; runtime only projects data and interaction states.
"""
import unreal

BASE = '/Game/FestaFiesta/Building/FF_WBP_Building/'

def load(suffix):
    bp = unreal.load_asset(BASE + suffix)
    bp.modify()
    w = {x.get_name(): x for x in unreal.ObjectIterator(unreal.Widget)
         if x.get_outer() and x.get_outer().get_outer() == bp}
    for x in w.values():
        x.modify()
        if x.slot: x.slot.modify()
    return bp, w

def color(hex_value):
    rgb = [int(hex_value[i:i+2], 16) / 255.0 for i in (0, 2, 4)]
    return unreal.LinearColor(*[(v / 12.92 if v <= .04045 else ((v + .055) / 1.055)**2.4) for v in rgb], 1)

def brush(source, fill, edge='DCE3EF', radius=7, width=1):
    source.set_editor_property('draw_as', unreal.SlateBrushDrawType.ROUNDED_BOX)
    source.set_editor_property('tint_color', unreal.SlateColor(specified_color=color(fill)))
    outline = source.get_editor_property('outline_settings')
    outline.set_editor_property('rounding_type', unreal.SlateBrushRoundingType.FIXED_RADIUS)
    outline.set_editor_property('corner_radii', unreal.Vector4(radius, radius, radius, radius))
    outline.set_editor_property('color', unreal.SlateColor(specified_color=color(edge)))
    outline.set_editor_property('width', width)
    source.set_editor_property('outline_settings', outline)
    return source

def surface(x, fill, radius=7, width=1):
    x.set_editor_property('background', brush(x.get_editor_property('background'), fill, radius=radius, width=width))

def font(x, size, tint='354259', face=None):
    f = x.get_editor_property('font')
    f.set_editor_property('size', size)
    if face: f.set_editor_property('typeface_font_name', face)
    x.set_font(f)
    x.set_color_and_opacity(unreal.SlateColor(specified_color=color(tint)))

def save(bp):
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)

bp, w = load('Catalog/WBP_FF_PrepBuildTile')
button = w['Button_Favorite']
style = button.get_editor_property('widget_style')
for state, fill, edge in [('normal','FFFFFF','CCD7E2'), ('hovered','FFF6DC','E1AB35'),
                          ('pressed','FFEAB0','D49C22'), ('disabled','F4F5F8','DCE3EF')]:
    style.set_editor_property(state, brush(style.get_editor_property(state), fill, edge, 12))
style.set_editor_property('normal_padding', unreal.Margin(0,0,0,0))
style.set_editor_property('pressed_padding', unreal.Margin(0,0,0,0))
button.set_editor_property('widget_style', style)
w['Size_Favorite'].set_width_override(24)
w['Size_Favorite'].set_height_override(24)
w['Size_Favorite'].slot.set_padding(unreal.Margin(0,0,4,4))
w['Size_Favorite'].slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_RIGHT)
w['Size_Favorite'].slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
font(w['Text_Favorite'], 16, '91A3B6')
unreal.get_default_object(bp.generated_class()).set_editor_property('favorite_color', color('D9A123'))
save(bp)

bp, w = load('Catalog/WBP_FF_PrepPanelBuild')
w['Size_ClarityCatalogDock'].set_width_override(1450)
w['Size_ClarityCatalogDock'].set_height_override(370)
w['Size_ClarityCatalogDock'].slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('catalog_viewport_margin', 60)
cdo.set_editor_property('catalog_height', 370)
save(bp)

bp, w = load('Facility/WBP_FF_FacilityInfoPanel')
for name, offset in [('Panel_Size',0),('Panel_ShadowSize',4)]:
    x = w[name]
    x.set_width_override(390)
    x.set_height_override(630)
    x.slot.set_anchors(unreal.Anchors(unreal.Vector2D(1,1), unreal.Vector2D(1,1)))
    x.slot.set_alignment(unreal.Vector2D(1,1))
    x.slot.set_offsets(unreal.Margin(-24+offset,-64+offset,390,630))
    x.slot.set_auto_size(False)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('build_inspector_width',390)
cdo.set_editor_property('build_inspector_height',630)
cdo.set_editor_property('build_inspector_minimum_bottom_inset',64)
w['Border_Panel'].set_padding(unreal.Margin(12,8,12,12))
surface(w['Border_Panel'],'F8F8FE',15)
w['Header_Size'].set_height_override(42)
surface(w['Header_Surface'],'F8F8FE',0,0)
w['Text_Context'].set_visibility(unreal.SlateVisibility.COLLAPSED)
font(w['V2_InspectorTitle'],21,face='SemiBold')
font(w['Text_FacilityName'],19,face='SemiBold')
font(w['Text_Category'],12,'8B99AF')
surface(w['Category_Chip'],'EAF2F8',0,0)
w['Category_Chip'].set_padding(unreal.Margin(0,0,0,0))
surface(w['Panel_PersistentIdentity'],'EAF2F8',7)
w['Panel_PersistentIdentity'].set_padding(unreal.Margin(10,10,10,10))
w['Panel_PersistentIdentity'].slot.set_padding(unreal.Margin(0,10,0,8))
w['Thumbnail_Size'].set_visibility(unreal.SlateVisibility.COLLAPSED)
# Keep the existing favorite action available in both build and live inspectors.
favorite=w['V2_FavoriteSize']
favorite.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
favorite.set_width_override(24)
favorite.set_height_override(24)
favorite.remove_from_parent()
header=w['Header_Row']
header.clear_children()
for name in ['Header_Identity','Button_LightingMinimize','V2_FavoriteSize','Close_Size']:
    x=w[name]
    slot=header.add_child_to_horizontal_box(x)
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    slot.set_padding(unreal.Margin(0,0,6,0))
    if name=='Header_Identity':
        size=slot.get_editor_property('size')
        size.set_editor_property('size_rule',unreal.SlateSizeRule.FILL)
        slot.set_editor_property('size',size)
style=w['Button_Favorite'].get_editor_property('widget_style')
for state in ['normal','hovered','pressed','disabled']:
    style.set_editor_property(state,brush(style.get_editor_property(state),'FFF6DC','E1AB35',12))
style.set_editor_property('normal_padding',unreal.Margin(0,0,0,0))
style.set_editor_property('pressed_padding',unreal.Margin(0,0,0,0))
w['Button_Favorite'].set_editor_property('widget_style',style)
font(w['Text_Favorite'],16,'D9A123')
badge = w['Border_A5LightingSupport']
surface(badge,'DDF3F8',12,0)
badge.set_padding(unreal.Margin(7,4,7,4))
badge.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
badge.set_visibility(unreal.SlateVisibility.COLLAPSED)
w['Text_A5LightingSupport'].set_text('조명 지원')
w['Text_A5LightingSupport'].set_font(w['Text_Category'].get_editor_property('font'))
font(w['Text_A5LightingSupport'],11,'33869A')
w['Reference_InspectorTabsSize'].set_height_override(30)
w['Reference_InspectorTabsSize'].slot.set_padding(unreal.Margin(0,0,0,8))
w['Reference_AdditionalFacilityPages'].slot.set_padding(unreal.Margin(0,0,0,8))
for x in w.values():
    if isinstance(x,unreal.Button) and x.get_name().startswith('Button_Tab'):
        style=x.get_editor_property('widget_style')
        for state,fill in [('normal','E4F3F8'),('hovered','D6EDF3'),('pressed','BBDDE6'),('disabled','287789')]:
            style.set_editor_property(state,brush(style.get_editor_property(state),fill,radius=6))
        style.set_editor_property('normal_padding',unreal.Margin(8,6,8,6))
        style.set_editor_property('pressed_padding',unreal.Margin(8,6,8,6))
        x.set_editor_property('widget_style',style)
        x.slot.set_padding(unreal.Margin(2,0,2,0))
        label=x.get_content()
        if isinstance(label,unreal.TextBlock): font(label,13,'33869A')
surface(w['Body_Surface'],'F8F8FE',0,0)
surface(w['Panel_BuildSection'],'F8F8FE',0,0)
w['Panel_BuildSection'].set_padding(unreal.Margin(0,0,0,0))
for name in ['Reference_BasicCapacitySection','Reference_BasicPlacementSection']:
    surface(w[name],'F0F4FA',7)
    w[name].set_padding(unreal.Margin(10,8,10,8))
for name in ['Reference_BasicCapacityHeading','Reference_BasicPlacementHeading']:
    font(w[name],14,'33869A')
    w[name].slot.set_padding(unreal.Margin(0,0,0,4))
# Restore the reference's four stacked operating facts instead of two large pills.
column=w['Reference_BasicCapacityContent']
ordered=[w['Reference_BasicCapacityHeading']] + [w['Clarity_Row_Text_'+n] for n in
    ['QueueCapacityValue','ServiceCapacityValue','InstanceLimit','ServiceDuration']]
column.clear_children()
for x in ordered:
    x.remove_from_parent()
    column.add_child_to_vertical_box(x)
for x in w.values():
    if x.get_name().startswith('Clarity_Row_Text_') and isinstance(x,unreal.HorizontalBox):
        if isinstance(x.slot,unreal.VerticalBoxSlot): x.slot.set_padding(unreal.Margin(0,5,0,5))
        for i in range(x.get_children_count()):
            child=x.get_child_at(i)
            if isinstance(child,unreal.TextBlock):
                font(child,13,'8B99AF' if i==0 else '354259', 'Regular' if i==0 else 'SemiBold')
                child.set_editor_property('justification',unreal.TextJustify.LEFT if i==0 else unreal.TextJustify.RIGHT)
# Harmonize functional pages without replacing their data, bindings or lighting color wheel.
for name in ['Clarity_RevenueCard','Program_Card','Upgrade_Card','V2_SynergySummaryCard']:
    surface(w[name],'F0F4FA',7)
    w[name].set_padding(unreal.Margin(10,8,10,8))
for name in ['Text_ProgramStatus','Text_ScheduledProgramSummary','Text_StaffSummary','Text_SynergySummary']:
    font(w[name],13)
surface(w['Panel_Actions'],'F8F8FE',0,0)
w['Panel_Actions'].set_padding(unreal.Margin(0,8,0,0))
save(bp)
bp, w = load('Facility/WBP_FF_FacilityLightingControls')
surface(w['Reference_LightingEditorCard'],'F0F4FA',7)
w['Reference_LightingEditorCard'].set_padding(unreal.Margin(10,8,10,8))
surface(w['Reference_LightingPreviewBar'],'EAF2F8',7)
w['Aurora_ColorPickerSize'].set_width_override(156)
w['Aurora_ColorPickerSize'].set_height_override(156)
w['Reference_LightingColorModeSize'].set_height_override(156)
for name in ['Reference_CurrentColorSize','Reference_NewColorSize']:
    w[name].set_width_override(52)
    w[name].set_height_override(48)
for name in ['Reference_LightingApplySize','Reference_LightingCancelSize']:
    w[name].set_height_override(36)
for x in w.values():
    if isinstance(x,unreal.TextBlock): font(x,13)
font(w['Text_LightingTitle'],15,'33869A')
save(bp)
print('A5 reference inspector, lighting and favorites authored; compile these four WBP assets.')
