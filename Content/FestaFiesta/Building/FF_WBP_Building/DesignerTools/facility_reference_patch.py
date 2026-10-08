"""Reference facility inspector and lighting layout. Only the two local WBP templates.

The caller owns transaction, compilation and saving. Existing named controls,
bindings and gameplay handlers are reused; this module creates presentation containers.
No runtime tests are performed. All dimensions are 1920x1080 design-space units.
"""
INFO = '/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel'
LIGHT = '/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityLightingControls'
PACKAGES = (INFO, LIGHT)


def apply(context):
    import unreal as u
    report = {'module':'facility_reference_patch', 'errors':[], 'created':[],
              'dimensions':{'panel':[480,800], 'right':24, 'top':24,
                            'wheel':[192,192], 'swatches':[72,64],
                            'presets_height':40, 'footer_height':54},
              'runtime_tests':'not run, as requested'}
    color, slate, brush = context['color'], context['slate'], context['brush']
    tokens = context.get('tokens', {})
    pearl=tokens.get('surface','#F9F9FF'); ink=tokens.get('ink','#3D3C4F')
    muted=tokens.get('muted','#626577'); action=tokens.get('action','#146A78')
    line=tokens.get('line','#D8E1F2'); selected=tokens.get('selected','#E5F6FC')
    changed=[]

    def margin(l=0,t=0,r=0,b=0): return u.Margin(left=l,top=t,right=r,bottom=b)
    def setp(o,k,v): o.set_editor_property(k,v)
    def visibility(w,show=True): w.set_visibility(u.SlateVisibility.SELF_HIT_TEST_INVISIBLE if show else u.SlateVisibility.COLLAPSED)
    def font(w,size,tint=ink):
        f=w.get_editor_property('font');f.set_editor_property('size',size)
        w.set_editor_property('font',f);w.set_editor_property('color_and_opacity',slate(tint))
    def text(w,value,size=16,tint=ink):
        w.set_text(value);font(w,size,tint);w.set_editor_property('auto_wrap_text',True)
    def single_line(w):
        # Automatic-width slots must measure the whole label, not a wrapped glyph.
        w.set_editor_property('auto_wrap_text',False);w.set_editor_property('wrap_text_at',0.0)
        w.set_editor_property('min_desired_width',0.0)
    def size(w,width=None,height=None):
        if width is not None:w.set_width_override(width)
        if height is not None:w.set_height_override(height)
    def pad(w,l=0,t=0,r=0,b=0): w.set_editor_property('padding',margin(l,t,r,b))
    def slotfill(slot,fill=False):
        if isinstance(slot,(u.VerticalBoxSlot,u.HorizontalBoxSlot)):
            s=slot.get_editor_property('size');s.set_editor_property('size_rule',u.SlateSizeRule.FILL if fill else u.SlateSizeRule.AUTOMATIC);s.set_editor_property('value',1.0);slot.set_editor_property('size',s)
    def add(parent,child,fill=False,padding=None):
        child.remove_from_parent();s=parent.add_child(child)
        slotfill(s,fill)
        if padding is not None: s.set_editor_property('padding',margin(*padding))
        elif isinstance(s,u.BorderSlot):s.set_editor_property('padding',parent.get_editor_property('padding'))
        if isinstance(s,(u.VerticalBoxSlot,u.HorizontalBoxSlot,u.OverlaySlot,u.SizeBoxSlot)):
            s.set_editor_property('horizontal_alignment',u.HorizontalAlignment.H_ALIGN_FILL)
            s.set_editor_property('vertical_alignment',u.VerticalAlignment.V_ALIGN_FILL)
        return s
    def surface(w,fill=pearl,radius=12,padding=0):
        w.set_editor_property('background',brush(fill,radius=radius));w.set_editor_property('brush_color',color('#FFFFFF'));pad(w,padding,padding,padding,padding)
        if w.get_content():pad(w.get_content().get_editor_property('slot'),padding,padding,padding,padding)
    def button(w,fill=pearl,foreground=ink,tab=False):
        style=w.get_editor_property('widget_style')
        hover=action if foreground=='#FFFFFF' else selected
        for state,c in [('normal',fill),('hovered',hover),('pressed',hover),('disabled',action if tab else '#E0E5EF')]:
            style.set_editor_property(state,brush(c,radius=18 if tab else 9))
            style.set_editor_property(state+'_foreground',slate(foreground))
        style.set_editor_property('normal_padding',margin(8,4,8,4));style.set_editor_property('pressed_padding',margin(8,4,8,4))
        w.set_editor_property('widget_style',style)
        w.set_editor_property('background_color',color('#FFFFFF'));w.set_editor_property('color_and_opacity',color('#FFFFFF'))
        child=w.get_content()
        if child and child.get_editor_property('slot'):pad(child.get_editor_property('slot'))
    def input_style(w):
        st=w.get_editor_property('widget_style')
        for p in ['background_image_normal','background_image_hovered','background_image_focused','background_image_read_only']:st.set_editor_property(p,brush(pearl,radius=8))
        for p in ['foreground_color','focused_foreground_color','read_only_foreground_color']:st.set_editor_property(p,slate(ink))
        st.set_editor_property('background_color',slate('#FFFFFF'))
        ts=st.get_editor_property('text_style');f=ts.get_editor_property('font');f.set_editor_property('size',14);ts.set_editor_property('font',f);st.set_editor_property('text_style',ts);w.set_editor_property('widget_style',st)

    bp,w=context['widgets'](LIGHT);bp.modify();tree=w['Aurora_LightingV7'].get_outer()
    def node(cls,name):
        found=w.get(name)
        if found is None:
            found=u.new_object(cls,outer=tree,name=name);w[name]=found;report['created'].append(LIGHT+':'+name)
        found.modify();return found
    def label(name,value,pt=14,tint=ink):
        n=node(u.TextBlock,name)
        f=w['Text_LightingTitle'].get_editor_property('font');n.set_editor_property('font',f);text(n,value,pt,tint);return n
    def box(name,child,width=None,height=None):
        n=node(u.SizeBox,name);n.clear_children();size(n,width,height);add(n,child);return n
    def section(name,fill=pearl,radius=12,padding=14):
        n=node(u.Border,name);n.clear_children();surface(n,fill,radius,padding);return n
    root=w['Aurora_LightingV7'];root.clear_children()
    card=section('Reference_LightingEditorCard',pearl,14,8)
    body=node(u.VerticalBox,'Reference_LightingEditorBody');body.clear_children();add(card,body);add(root,card,padding=(0,0,0,8))
    title=node(u.HorizontalBox,'Reference_LightingTitleRow');title.clear_children()
    text(w['Text_LightingTitle'],'조명 색상 변경',18);add(title,w['Text_LightingTitle'],True)
    button(w['Button_LightingReset']);text(w['Text_LightingReset'],'기본값',12,muted);add(title,box('Reference_LightingResetSize',w['Button_LightingReset'],76,32))
    add(body,title,padding=(0,0,0,4));description=label('Reference_LightingDescription','색상·밝기 변경을 시설에서 미리 확인하세요.\n적용: 변경 유지 · 취소: 이전 상태로 복원',12,muted);single_line(description)
    add(body,description,padding=(0,0,0,4))
    modes=w['ClarityV2_ColorModes'];modes.clear_children()
    basic=w['Panel_LightingBasic'];basic.clear_children();add(modes,basic)
    picker=w['Aurora_ColorPickerSize'];size(picker,192,192);add(basic,picker,padding=(0,0,12,0))
    colors=node(u.VerticalBox,'Reference_LightingColorSamples');colors.clear_children();add(basic,colors,True)
    swatchrow=node(u.HorizontalBox,'Reference_LightingSwatches');swatchrow.clear_children()
    for key,title_text in [('Current','현재 색상'),('New','변경할 색상')]:
        column=node(u.VerticalBox,'Reference_'+key+'ColorColumn');column.clear_children()
        add(column,label('Reference_'+key+'ColorLabel',title_text,12,muted),padding=(0,0,0,8))
        swatch=section('Border_Lighting'+key+'Color','#FFFFFF',8,0);swatch.set_editor_property('brush_color',color('#56DCE7'))
        add(column,box('Reference_'+key+'ColorSize',swatch,72,64))
        add(swatchrow,column,True,padding=(0,0,8 if key=='Current' else 0,0))
    add(colors,swatchrow,padding=(0,10,0,12))
    input_style(w['Input_LightingColor']);add(colors,box('Reference_LightingHexSize',w['Input_LightingColor'],None,36))
    precision=w['Panel_LightingPrecision'];add(modes,precision);visibility(precision,False)
    for key in ['Input_LightingH','Input_LightingS','Input_LightingV','Input_LightingR','Input_LightingG','Input_LightingB']:input_style(w[key])
    add(body,box('Reference_LightingColorModeSize',modes,None,192),padding=(0,0,0,6))
    presets=node(u.HorizontalBox,'Reference_LightingPresetRow');presets.clear_children()
    for i,(key,caption,fill,fg) in enumerate([('Original','원래 색',pearl,ink),('Teal','청록','#1699AB','#FFFFFF'),('Sunset','노을','#FFC0A0',ink),('Violet','보라','#D4B4F4',ink)]):
        b=w['Button_Lighting'+key];button(b,fill,fg);oldlabel=w['Text_LightingPreset'+str(i)];oldlabel.remove_from_parent();b.clear_children();b.add_child(oldlabel);text(oldlabel,caption,13,fg)
        add(presets,box('Reference_Preset'+key+'Size',b,None,40),True,padding=(0,0,6 if i<3 else 0,0))
    add(body,presets,padding=(0,0,0,6))
    brightness=node(u.HorizontalBox,'Reference_LightingBrightnessHeading');brightness.clear_children()
    text(w['Text_LightingBrightnessLabel'],'밝기',17);font(w['Text_LightingBrightness'],17)
    add(brightness,box('Reference_LightingBrightnessLabelSize',w['Text_LightingBrightnessLabel'],56,None),padding=(0,0,6,0));add(brightness,w['Text_LightingBrightness'],True)
    add(body,box('Reference_LightingBrightnessHeadingSize',brightness,None,26),padding=(0,0,0,2));slider=w['Slider_LightingBrightness'];slider.set_editor_property('slider_bar_color',color(line));slider.set_editor_property('slider_handle_color',color('#16B4D5'))
    add(body,box('Reference_LightingBrightnessSliderSize',slider,None,20),padding=(0,0,0,4))
    levels=node(u.HorizontalBox,'Reference_LightingBrightnessLevels');levels.clear_children()
    for i,key in enumerate(['Low','Normal','High']):
        b=w['Button_LightingBrightness'+key];button(b,pearl);font(w['Button_LightingBrightness'+key+'_Label'],13)
        add(levels,box('Reference_Brightness'+key+'Size',b,None,36),True,padding=(0,0,6,0))
    button(w['Button_LightingPrecision']);font(w['Text_LightingPrecision'],13);add(levels,box('Reference_LightingPrecisionSize',w['Button_LightingPrecision'],None,36),True);add(body,levels)
    preview=section('Reference_LightingPreviewBar',selected,12,6);previewrow=node(u.HorizontalBox,'Reference_LightingPreviewRow');previewrow.clear_children();add(preview,previewrow)
    add(previewrow,label('Reference_LightingPreviewLabel','▶  미리보기 중',14,action),True)
    button(w['Button_LightingToggle']);font(w['Text_LightingToggle'],11);add(previewrow,box('Reference_LightingToggleSize',w['Button_LightingToggle'],116,28),padding=(4,0,4,0))
    button(w['Button_LightingObserve']);text(w['Text_LightingObserve'],'장면 확인',13);add(previewrow,box('Reference_LightingObserveSize',w['Button_LightingObserve'],92,28))
    add(root,box('Reference_LightingPreviewSize',preview,None,40),padding=(0,0,0,6))
    font(w['Text_LightingColorStatus'],11,muted);single_line(w['Text_LightingColorStatus']);add(root,box('Reference_LightingStatusSize',w['Text_LightingColorStatus'],None,18),padding=(4,0,4,2))
    footer=w['ClarityV2_LightingActions'];footer.clear_children()
    for i,key in enumerate(['Cancel','Apply']):
        b=w['Button_Lighting'+key];button(b,action if key=='Apply' else pearl,'#FFFFFF' if key=='Apply' else ink);font(w['Text_Lighting'+key],18,'#FFFFFF' if key=='Apply' else ink)
        add(footer,box('Reference_Lighting'+key+'Size',b,None,54),True,padding=(0,0,10 if i==0 else 0,0))
    add(root,footer)
    # Descriptions may wrap; all compact controls remain one horizontal line.
    for name in ['Text_LightingTitle','Text_LightingReset','Text_LightingBrightnessLabel',
                 'Text_LightingBrightness','Text_LightingPrecision','Text_LightingToggle',
                 'Text_LightingObserve','Text_LightingCancel','Text_LightingApply',
                 'Reference_LightingPreviewLabel','Reference_CurrentColorLabel','Reference_NewColorLabel']:
        single_line(w[name])
    for i in range(4):single_line(w['Text_LightingPreset'+str(i)])
    for key in ['Low','Normal','High']:single_line(w['Button_LightingBrightness'+key+'_Label'])
    report['horizontal_labels']='No automatic wrapping on compact lighting labels; reset 76, brightness label 56, toggle 116 and observe 92 design units wide.'
    report['persistent_guidance']='Two-line preview and Apply/Cancel explanation stays visible in the lighting editor.'
    # Retain old, now-empty presentation groups and labels as an invisible archive.
    # This preserves their names and ownership without displaying a second layout.
    archive=node(u.VerticalBox,'Reference_LightingRetiredLayout');archive.clear_children()
    for name,old in list(w.items()):
        if old != root and old != archive and old.get_parent() is None:
            add(archive,old)
    visibility(archive,False);archive.set_render_opacity(0.0)
    try: archive.set_editor_property('hidden_in_designer',True)
    except Exception: pass
    add(root,archive);changed.append(LIGHT)

    bp,w=context['widgets'](INFO);bp.modify();tree=w['Panel_Size'].get_outer()
    def inode(cls,name):
        n=w.get(name)
        if n is None:n=u.new_object(cls,outer=tree,name=name);w[name]=n;report['created'].append(INFO+':'+name)
        n.modify();return n
    for name in ['Panel_Size','Panel_ShadowSize']:
        n=w.get(name)
        if not n:continue
        size(n,480,800);s=n.get_editor_property('slot')
        s.set_anchors(u.Anchors(minimum=u.Vector2D(1,0),maximum=u.Vector2D(1,0)));s.set_alignment(u.Vector2D(1,0));s.set_position(u.Vector2D(-24,24));s.set_size(u.Vector2D(480,800));s.set_auto_size(False)
    visibility(w['Panel_ShadowSize'],False)
    surface(w['Border_Panel'],pearl,20,14);surface(w['Header_Surface'],pearl,0,0);size(w['Header_Size'],None,40)
    text(w['V2_InspectorTitle'],'시설 정보',20);visibility(w['Text_Context'],False)
    surface(w['Panel_PersistentIdentity'],'#EEF3FD',14,8);pad(w['Panel_PersistentIdentity'].get_editor_property('slot'),0,0,0,6)
    font(w['Text_FacilityName'],20);font(w['Text_Category'],14,muted);font(w['Text_LevelSummary'],12,muted)
    size(w['Thumbnail_Size'],112,80);surface(w['Thumbnail_Background'],'#EEF3FD',8,0);size(w['V2_FavoriteSize'],32,32)
    tabs=w['Row_ReadableInspectorTabs'];tabs.clear_children()
    for i,key in enumerate(['Overview','Synergy','Operation','Upgrade','Lighting']):
        b=w['Button_Tab'+key];button(b,pearl,ink,True);font(w['Text_Tab'+key],13)
        add(tabs,b,True,padding=(0,2,4 if i<4 else 0,2))
    text(w['Text_TabLighting'],'조명 ◉',13)
    tabsize=inode(u.SizeBox,'Reference_InspectorTabsSize');tabsize.clear_children();size(tabsize,None,44)
    add(tabsize,tabs);add(w['Panel_InspectorNavigation'],tabsize,padding=(0,0,0,6))
    surface(w['Body_Surface'],pearl,0,0)
    pad(w['Widget_LightingControls'].get_editor_property('slot'))
    report['lighting_height_budget']={'host':800,'outer_padding':28,'header':40,'identity':102,'tabs':50,'available_body':580,'estimated_lighting_stack':554,'wheel':192,'footer':54}
    # Existing revenue/program pages stay accessible from the basic page without crowding the five primary pills.
    links=inode(u.HorizontalBox,'Reference_AdditionalFacilityPages');links.clear_children()
    for key in ['Revenue','Programs']:
        b=w['Button_Tab'+key];button(b,selected);add(links,b,True,padding=(0,6,6,6))
    facts=w['Clarity_BasicFacts'];facts.clear_children();add(facts,w['TeamRevenue_OverviewCard'],padding=(0,0,0,8))
    for group,caption,names in [
        ('Capacity','운영 규모',['Reference_BasicStatPair','Reference_LimitPill','Clarity_Row_Text_ServiceDuration']),
        ('Placement','설치 정보',['Clarity_Row_Text_Footprint','Clarity_Row_Text_Clearance','Clarity_Row_Text_InstallCost','Clarity_Row_Text_RecoveryCost'])]:
        # Keep the complete overview on its first page after the header, quote
        # and fixed actions receive their authored allocation at 1080p.
        groupbox=inode(u.Border,'Reference_Basic'+group+'Section');groupbox.clear_children();surface(groupbox,'#EEF3FD',10,8)
        content=inode(u.VerticalBox,'Reference_Basic'+group+'Content');content.clear_children();add(groupbox,content)
        heading=inode(u.TextBlock,'Reference_Basic'+group+'Heading');heading.set_editor_property('font',w['Text_FacilityName'].get_editor_property('font'));text(heading,caption,15,action);add(content,heading,padding=(0,0,0,8))
        for name in names:add(content,w[name],padding=(0,0,0,4))
        add(facts,groupbox,padding=(0,0,0,8))
    add(facts,links)
    surface(w['Panel_BuildSection'],pearl,12,8)
    for n in ['Reference_QueuePill','Reference_ServicePill','Reference_LimitPill','TeamRevenue_OverviewCard']:
        surface(w[n],'#EEF3FD',10,10)
    for n,p in w.items():
        if isinstance(p,(u.TextBlock,u.CommonTextBlock)) and (n.startswith('Clarity_Label_') or n.startswith('Clarity_Row_')):font(p,13,muted)
        if isinstance(p,u.HorizontalBox) and n.startswith('Clarity_Row_'):
            if p.get_editor_property('slot'):pad(p.get_editor_property('slot'),0,5,0,5)
    surface(w['Panel_LightingColorHelp'],selected,12,8);button(w['Button_LightingColorHelp'],selected,action)
    text(w['Text_LightingColorHelp'],'◉  조명 색상·밝기 변경 가능',14,action)
    text(w['Text_LightingIntroductionTitle'],'조명 색상과 밝기를 바꿀 수 있어요',20)
    text(w['Text_LightingIntroductionPick'],'1. 색상 휠이나 네 가지 색상을 선택하세요.',14,muted)
    text(w['Text_LightingIntroductionPreview'],'2. 선택한 시설에서 변경한 조명을 바로 확인하세요.',14,muted)
    text(w['Text_LightingIntroductionApply'],'3. 적용하면 유지되고, 취소하면 편집 전 상태로 돌아갑니다.',14,muted)
    # Class defaults are written after compiling by the caller, to avoid CDO regeneration.
    defaults={'lighting_panel_width':480.0,'selected_tab_text_color':'#FFFFFF','inactive_tab_text_color':ink}
    report['default_updates']={INFO:defaults}
    report['preserved']='Existing controls, reflection names, gameplay delegates, availability gating, precision fields, reset/toggle/observe and apply/cancel remain connected.'
    changed.append(INFO)
    return changed,report


def apply_defaults(context):
    """Call after all WBP compilation, before saving; values remain local to this WBP."""
    import unreal as u
    color=context['color'];tokens=context.get('tokens',{})
    cls=u.EditorAssetLibrary.load_blueprint_class(INFO)
    cdo=u.get_default_object(cls);cdo.modify()
    values={'lighting_panel_width':480.0, 'selected_tab_text_color':'#FFFFFF',
            'inactive_tab_text_color':tokens.get('ink','#3D3C4F')}
    for name,value in values.items():
        cdo.set_editor_property(name,color(value) if isinstance(value,str) else value)
    return {'package':INFO,'class_defaults':values,'applied':True}
