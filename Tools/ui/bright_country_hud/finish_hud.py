"""첫 렌더 검증에서 확인한 탭 여백만 수정한다. HUD 트리를 재생성하지 않는다."""
import unreal

BASE = '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/'


def apply():
    blueprint = unreal.load_asset(BASE+'WBP_FF_PrepHudTabButton')
    tree = unreal.find_object(blueprint, 'WidgetTree')
    button = unreal.find_object(tree, 'Button_Root', unreal.Button)
    assert button
    style = button.get_editor_property('widget_style')
    style.set_editor_property('normal_padding', unreal.Margin(18., 3., 18., 3.))
    style.set_editor_property('pressed_padding', unreal.Margin(18., 3., 18., 3.))
    button.set_style(style)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    # 인스턴스 템플릿을 소유한 부모도 갱신해서 저장한다.
    parent = unreal.load_asset(BASE+'WBP_FF_PrepHudBar')
    unreal.BlueprintEditorLibrary.compile_blueprint(parent)
    assert unreal.EditorAssetLibrary.save_loaded_asset(parent, only_if_is_dirty=False)
    unreal.log('FF_BRIGHT_HUD_PADDING_UPDATED')


if __name__ == '__main__':
    try:
        apply()
    finally:
        unreal.SystemLibrary.quit_editor()
