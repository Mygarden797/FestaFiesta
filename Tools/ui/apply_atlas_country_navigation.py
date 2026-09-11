"""지구 국가 목록 배선을 적용한다. 에디터의 정상 Python 실행 경로에서 사용."""
from pathlib import Path
import unreal, shutil, json
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
backup=root/'Saved/AtlasNavigationBefore';backup.mkdir(exist_ok=True)
source=root/'Content/FestaFiesta/UI/Atlas/WBP_FF_AtlasGlobe.uasset'
if not (backup/source.name).exists():shutil.copy2(source,backup/source.name)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world,'FF.ApplyAtlasCountryNavigation')
asset=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasGlobe')
cls=unreal.load_class(None,'/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasGlobe.WBP_FF_AtlasGlobe_C')
row=unreal.get_default_object(cls).get_editor_property('pin_button_class')
assert row and row.get_name()=='WBP_FF_AtlasCountryRow_C'
# CBS는 WidgetBlueprint가 아닌 일반 Blueprint이므로 클래스 경로로 읽는다.
style=unreal.load_class(None,'/Game/FestaFiesta/UI/Styles/CBS_FF_AtlasTransparent.CBS_FF_AtlasTransparent_C')
assert style
row_asset=unreal.load_asset('/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasCountryRow')
row_asset.modify()
defaults=unreal.get_default_object(row)
defaults.modify()
defaults.set_editor_property('style',style)
assert unreal.EditorAssetLibrary.save_loaded_asset(row_asset,False)
(root/'Saved/AtlasNavigation_apply.json').write_text(json.dumps({'status':'complete','row':row.get_path_name()},indent=2),encoding='utf8')
