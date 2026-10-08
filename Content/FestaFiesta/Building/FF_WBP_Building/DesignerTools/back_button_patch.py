"""Building back control: keep its existing action, expose only the arrow."""
import unreal
PACKAGE='/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools'
PACKAGES=[PACKAGE]
def apply(context):
    bp,w=context['widgets'](PACKAGE);bp.modify()
    w['Size_Button_BuildBack'].set_width_override(56)
    w['Size_Button_BuildBack'].set_height_override(56)
    w['Text_BuildBack'].set_text('←')
    w['Label_PearlBack'].set_text('')
    w['Label_PearlBack'].set_visibility(unreal.SlateVisibility.COLLAPSED)
    w['Label_BuildMode'].set_visibility(unreal.SlateVisibility.COLLAPSED)
    w['Button_BuildBack'].set_tool_tip_text('이전 화면으로 돌아가기')
    row=w['Row_PearlBack'].get_editor_property('slot')
    row.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    row.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    return PACKAGES,{'backButton':[56,56],'text':'←','existingActionPreserved':True}
