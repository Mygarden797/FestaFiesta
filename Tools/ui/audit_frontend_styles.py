"""저장된 프런트엔드 버튼 기본값을 수정 없이 확인한다."""
import unreal
for name in ('WBP_FF_FrontendButton', 'WBP_FF_FrontendPrimaryButton'):
    cls = unreal.load_class(None, '/Game/FestaFiesta/UI/Frontend/' + name + '.' + name + '_C')
    obj = unreal.get_default_object(cls)
    for prop in ('style', 'bTintLabel', 'bFocusOnHover', 'NormalContentColor'):
        try:
            unreal.log('FF_STYLE_AUDIT %s %s=%s' % (name, prop, obj.get_editor_property(prop)))
        except Exception as exc:
            unreal.log('FF_STYLE_AUDIT %s' % exc)
unreal.SystemLibrary.quit_editor()
