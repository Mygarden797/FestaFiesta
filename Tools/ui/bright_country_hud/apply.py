"""승인된 한국 지도/하단 HUD만 적용하고 기존 에셋을 Saved에 백업한다.

명시적인 최초 적용용이다. 이후 에디터에서 마감한 WBP를 덮지 않도록 재실행을 막는다.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import sys
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(ROOT / 'Tools/ui/bright_country_hud'))
import materials

OUT = ROOT / 'Saved/BrightCountryHud'
REPORT_PATH = OUT / 'apply-report.json'
REPORT = {'status': 'running', 'saved': [], 'backup': ''}
TARGETS = [
    '/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasCountry_Natural',
    '/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasPin_Natural',
    '/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasPrimaryButton',
    '/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasGhostButton',
    '/Game/FestaFiesta/UI/Styles/CBS_FF_AtlasBrightPrimary',
    '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar',
    '/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudTabButton',
]


def file_for(package):
    assert package.startswith('/Game/')
    return ROOT / 'Content' / (package[6:] + '.uasset')


def protected():
    packages = ['/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas', materials.TERRAIN]
    paths = [file_for(p) for p in packages]
    paths += list((ROOT / 'Content/FestaFiesta/Maps').glob('*.umap'))
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in paths if p.is_file()}


try:
    OUT.mkdir(parents=True, exist_ok=True)
    if REPORT_PATH.exists():
        old = json.loads(REPORT_PATH.read_text(encoding='utf-8'))
        assert old.get('status') != 'passed', '이미 적용했다. 디자이너 마감 보존을 위해 재실행하지 않는다.'
    before = protected()
    backup = OUT / ('backup-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    for package in TARGETS:
        path = file_for(package)
        if path.is_file():
            target = backup / path.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    REPORT['backup'] = str(backup)
    map_mi, backdrop = materials.build(ROOT)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for command in ('FF.ApplyAtlasCountryBrightDesign', 'FF.ApplyPrepHudBrightBottomDesign'):
        unreal.SystemLibrary.execute_console_command(world, command)
    country_path = TARGETS[0]
    country = unreal.load_asset(country_path)
    country_class = unreal.EditorAssetLibrary.load_blueprint_class(country_path)
    defaults = unreal.get_default_object(country_class)
    overrides = defaults.get_editor_property('country_map_display_materials')
    overrides[unreal.Name('KR')] = map_mi
    defaults.set_editor_property('country_map_display_materials', overrides)
    tree = unreal.find_object(country, 'WidgetTree')
    assert tree
    image = unreal.find_object(tree, 'Image_Backdrop', unreal.Image)
    map_image = unreal.find_object(tree, 'Image_Map', unreal.Image)
    assert image and map_image
    image.set_brush_from_material(backdrop)
    brush = image.get_editor_property('brush')
    brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property('tint_color', unreal.SlateColor(specified_color=unreal.LinearColor(1., 1., 1., 1.)))
    image.set_brush(brush)
    image.set_color_and_opacity(unreal.LinearColor(1., 1., 1., 1.))
    # Designer에서도 같은 지도를 보이되 런타임은 국가별 override를 사용한다.
    map_image.set_brush_from_material(map_mi)
    unreal.BlueprintEditorLibrary.compile_blueprint(country)
    # 컴파일이 CDO를 재생성할 수 있으므로 최종 CDO에도 동일한 soft reference를 저장한다.
    defaults = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(country_path))
    overrides = defaults.get_editor_property('country_map_display_materials')
    overrides[unreal.Name('KR')] = map_mi
    defaults.set_editor_property('country_map_display_materials', overrides)
    materials.save(country)
    REPORT['map_material'] = map_mi.get_path_name()
    REPORT['backdrop_material'] = backdrop.get_path_name()
    for package in TARGETS:
        obj = unreal.load_asset(package)
        assert obj, '적용 대상 누락: ' + package
        if isinstance(obj, unreal.WidgetBlueprint):
            assert unreal.EditorAssetLibrary.load_blueprint_class(package), package
        REPORT['saved'].append(obj.get_path_name())
    assert protected() == before, '지도 원본/카탈로그/레벨이 변경되었다'
    REPORT['protected_assets_unchanged'] = True
    REPORT['status'] = 'passed'
except Exception:
    REPORT['status'] = 'failed'
    REPORT['error'] = traceback.format_exc()
    unreal.log_error(REPORT['error'])
finally:
    REPORT_PATH.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('FF_BRIGHT_COUNTRY_HUD_APPLY ' + REPORT['status'])
    unreal.SystemLibrary.quit_editor()
