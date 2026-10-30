"""Style the authored inspector grid and queue page. Run only for this migration."""
import unreal
from pathlib import Path

helper=Path(__file__).with_name('apply_a5_inspector_reference.py').read_text(encoding='utf-8')
exec(helper[:helper.index("bp, w = load('Catalog/WBP_FF_PrepBuildTile')")])
bp,w=load('Facility/WBP_FF_FacilityInfoPanel')
grid=w['Grid_InspectorTabs']
grid.set_min_desired_slot_width(160)
grid.set_min_desired_slot_height(42)
grid.set_slot_padding(unreal.Margin(3,3,3,3))
grid.slot.set_padding(unreal.Margin(-3,0,-3,10))
tabs=[('Overview','기본'),('Synergy','조합'),('Operation','운영'),('Queue','대기열'),
      ('Revenue','매출'),('Lighting','조명'),('Programs','프로그램'),('Upgrade','강화'),('Staff','스태프')]
shared_font=w['Text_TabOverview'].get_editor_property('font')
shared_font.set_editor_property('size',14)
for i,(key,caption) in enumerate(tabs):
    button=w['Button_Tab'+key];button.remove_from_parent()
    slot=grid.add_child_to_uniform_grid(button,i//3,i%3)
    slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    style=button.get_editor_property('widget_style')
    for state,fill in [('normal','E4F3F8'),('hovered','D6EDF3'),('pressed','BBDDE6'),('disabled','287789')]:
        style.set_editor_property(state,brush(style.get_editor_property(state),fill,radius=6))
    for p in ['normal_padding','pressed_padding']:style.set_editor_property(p,unreal.Margin(8,4,8,4))
    button.set_editor_property('widget_style',style)
    label=button.get_content();label.set_text(caption);label.set_font(shared_font)
    label.set_color_and_opacity(unreal.SlateColor(specified_color=color('33869A')))
    label.set_editor_property('justification',unreal.TextJustify.CENTER)
    label.set_auto_wrap_text(False)
    label.slot.set_padding(unreal.Margin(0,0,0,0))
    label.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    label.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    button.set_visibility(unreal.SlateVisibility.VISIBLE)
# Keep the migration marker outside the two retired navigation rows.
marker=w['A5_InspectorDesignerLayout'];marker.remove_from_parent()
w['Panel_RequiredBindings'].add_child_to_vertical_box(marker)
marker.set_visibility(unreal.SlateVisibility.COLLAPSED)
for name in ['Reference_InspectorTabsSize','Reference_AdditionalFacilityPages']:
    if name in w:w[name].set_visibility(unreal.SlateVisibility.COLLAPSED)

def label(x,caption,size=14,tint='354259'):
    x.set_text(caption);x.set_font(shared_font);font(x,size,tint)

queue=w['Panel_FacilityQueueSettings']
surface(queue,'F0F4FA',8);queue.set_padding(unreal.Margin(14,12,14,14))
queue.slot.set_padding(unreal.Margin(0,0,0,8))
queue.set_visibility(unreal.SlateVisibility.COLLAPSED)
label(w['Text_FacilityQueueTitle'],'대기열 설정',16,'287789')
w['Text_FacilityQueueTitle'].slot.set_padding(unreal.Margin(0,0,0,6))
label(w['Text_QueueSettingsDescription'],'수용할 팀 수와 대기 공간의 간격을 설정합니다.',13,'71849B')
w['Text_QueueSettingsDescription'].set_auto_wrap_text(True)
w['Text_QueueSettingsDescription'].slot.set_padding(unreal.Margin(0,0,0,14))
w['Text_QueueSettingsDescription'].set_visibility(unreal.SlateVisibility.COLLAPSED)
for key,caption,hint in [('Teams','대기 정원 (팀)','1~512'),('Spacing','자리 간격 (cm)','90~1000'),('Width','통로 폭 (cm)','100~1000')]:
    row=w['Row_Queue'+key];row.slot.set_padding(unreal.Margin(0,0,0,10))
    title=w['Label_Queue'+key];label(title,caption)
    s=title.slot.get_editor_property('size');s.set_editor_property('size_rule',unreal.SlateSizeRule.FILL)
    title.slot.set_editor_property('size',s);title.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    bounds=w['Size_Queue'+key];bounds.set_width_override(140);bounds.set_height_override(36)
    entry=w['Editable_FacilityQueue'+key]
    style=entry.get_editor_property('widget_style')
    text_style=style.get_editor_property('text_style');text_style.set_editor_property('font',shared_font)
    style.set_editor_property('text_style',text_style)
    for prop,fill,edge in [('background_image_normal','FFFFFF','DCE3EF'),('background_image_hovered','FFFFFF','80B9C2'),
                           ('background_image_focused','FFFFFF','287789'),('background_image_read_only','EAF0F6','DCE3EF')]:
        style.set_editor_property(prop,brush(style.get_editor_property(prop),fill,edge,5))
    entry.set_editor_property('widget_style',style)
    entry.set_foreground_color(color('354259'))
    entry.set_hint_text(hint);entry.set_tool_tip_text(hint)
    entry.set_editor_property('select_all_text_when_focused',True)
    entry.set_editor_property('clear_keyboard_focus_on_commit',False)
    entry.set_editor_property('revert_text_on_escape',False)
label(w['Text_FacilityQueueHint'],'',13,'71849B')
w['Text_FacilityQueueHint'].set_auto_wrap_text(True)
w['Text_FacilityQueueHint'].slot.set_padding(unreal.Margin(0,0,0,8))
w['Size_QueueApply'].set_height_override(36)
button=w['Button_ApplyFacilityQueueSettings']
button.set_editor_property('widget_style',w['Button_TabOverview'].get_editor_property('widget_style'))
label(w['Text_ApplyFacilityQueueSettings'],'대기열 설정 적용',14,'287789')
w['Text_ApplyFacilityQueueSettings'].set_editor_property('justification',unreal.TextJustify.CENTER)
w['Text_ApplyFacilityQueueSettings'].slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
w['Text_ApplyFacilityQueueSettings'].slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

# Separate headings, labels and values; keep all authoritative values and bindings.
font(w['Text_FacilityName'],21,'354259')
font(w['Text_Category'],13,'71849B')
for name in ['Reference_BasicCapacitySection','Reference_BasicPlacementSection','Clarity_RevenueCard','Program_Card','Upgrade_Card','V2_SynergySummaryCard','Panel_LiveSection']:
    surface(w[name],'F0F4FA',8);w[name].set_padding(unreal.Margin(14,10,14,10))
for name in ['Reference_BasicCapacityHeading','Reference_BasicPlacementHeading']:
    font(w[name],15,'287789');w[name].slot.set_padding(unreal.Margin(0,0,0,8))
for x in w.values():
    if x.get_name().startswith('Clarity_Row_Text_') and isinstance(x,unreal.HorizontalBox):
        if isinstance(x.slot,unreal.VerticalBoxSlot):x.slot.set_padding(unreal.Margin(0,4,0,4))
        for i in range(x.get_children_count()):
            child=x.get_child_at(i)
            if isinstance(child,unreal.TextBlock):
                font(child,14 if i==0 else 15,'71849B' if i==0 else '354259')
                child.set_editor_property('justification',unreal.TextJustify.LEFT if i==0 else unreal.TextJustify.RIGHT)
save(bp)
print('Equal-sized 3-column tabs and authored queue settings page saved.')
