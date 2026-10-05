"""타이틀 인트로 에셋 가져오기 · WBP 생성 · 검증. 일회용 에디터에서 실행한다 (`run.ps1 -Mode Apply [-Rebuild]`).

1. 검은 바탕용 로고 PNG 와 인트로 소리 WAV 를 `/Game/FestaFiesta/UI/Title/Intro` 로 가져온다(같은 이름이면 교체).
   UI 폴더는 DirectoriesToAlwaysCook 이라 패키징에서 빠지지 않는다.
2. `FF.TitleIntroUMG`(없을 때만) 또는 `-Rebuild` 면 `FF.ApplyTitleIntroDesign`(🔴 트리 교체).
3. 배선·위젯·영상·소리를 검사한다.
보고서: Saved/TitleIntro/apply-report.json
"""
from pathlib import Path
import json
import os
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/TitleIntro'
OUT.mkdir(parents=True, exist_ok=True)
FOLDER = '/Game/FestaFiesta/UI/Title/Intro'
ASSET = FOLDER + '/WBP_FF_TitleIntro'
LOGO_SRC = ROOT / 'SourceArt/UI/Title/Intro_v2/T_FF_Title_Logo_OnDark_1024.png'
SOUND_SRC = ROOT / 'SourceArt/UI/Intro/SpaceFireworks_v1/Audio/FF_Intro_SpaceFireworks.wav'
MOVIE = ROOT / 'Content/Movies/FF_Intro_SpaceFireworks.mp4'
REBUILD = os.environ.get('FF_TITLE_INTRO_REBUILD') == '1'
BIND = ('Image_Backdrop', 'Image_Video', 'Panel_Splash', 'Image_FlashCover')
OTHER = ('ScaleBox_Video', 'Image_SplashMark', 'Text_TeamName', 'Text_TeamCourse', 'Text_Disclaimer')
GONE = ('Panel_PressAnyKey', 'Text_PressAnyKey', 'Image_SplashWordmark', 'Image_PromptWordmark')
REPORT = {'status': 'running', 'asset': ASSET, 'rebuild': REBUILD}


def import_file(src, name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(src))
    task.set_editor_property('destination_path', FOLDER)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.get_editor_property('imported_object_paths'))
    assert paths, '가져오기 실패: ' + str(src)
    return unreal.load_asset(paths[0])


def tree_widgets(bp):
    tree = unreal.find_object(bp, 'WidgetTree')
    assert tree, 'WidgetTree 없음'
    return {w.get_name(): w.get_class().get_name()
            for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree}


try:
    # 1) 에셋
    assert LOGO_SRC.exists(), '로고 원본 없음: ' + str(LOGO_SRC)
    assert SOUND_SRC.exists(), '소리 원본 없음: ' + str(SOUND_SRC)
    logo = import_file(LOGO_SRC, 'T_FF_Title_Logo_OnDark')
    logo.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    logo.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    logo.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    logo.set_editor_property('srgb', True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(logo, only_if_is_dirty=False), '로고 저장 실패'
    sound = import_file(SOUND_SRC, 'S_FF_Intro_SpaceFireworks')
    sound.set_editor_property('looping', False)
    assert unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False), '소리 저장 실패'
    REPORT['logo'] = logo.get_path_name()
    REPORT['sound'] = {'path': sound.get_path_name(), 'duration': sound.get_editor_property('duration')}

    # 2) WBP
    REPORT['existed_before'] = unreal.EditorAssetLibrary.does_asset_exist(ASSET)
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyTitleIntroDesign' if REBUILD else 'FF.TitleIntroUMG')
    assert unreal.EditorAssetLibrary.does_asset_exist(ASSET), 'WBP 가 만들어지지 않았다'

    # 3) 검사
    bp = unreal.load_asset(ASSET)
    generated = unreal.load_object(None, ASSET + '.WBP_FF_TitleIntro_C')
    assert generated, '생성 클래스 없음 — 컴파일 실패'
    REPORT['parent_ok'] = unreal.MathLibrary.class_is_child_of(generated, unreal.FF_TitleIntroScreen.static_class())
    assert REPORT['parent_ok'], '부모 클래스가 FF_TitleIntroScreen 이 아니다'
    widgets = tree_widgets(bp)
    REPORT['widgets'] = widgets
    missing = [n for n in BIND + OTHER if n not in widgets]
    assert not missing, '없는 위젯: ' + ', '.join(missing)
    leftover = [n for n in GONE if n in widgets]
    assert not leftover, '빠져야 할 위젯이 남았다(-Rebuild 필요): ' + ', '.join(leftover)

    settings = unreal.get_default_object(unreal.FF_UIFlowSettings)
    wired = settings.get_editor_property('frontend_screens').get(unreal.FF_UIFlowScreen.TITLE_INTRO)
    REPORT['frontend_screens_title_intro'] = str(wired)
    assert wired and 'WBP_FF_TitleIntro' in str(wired), 'FrontendScreens 에 TitleIntro 배선이 없다'

    REPORT['movie'] = {'path': str(MOVIE.relative_to(ROOT)), 'exists': MOVIE.exists(),
                       'bytes': MOVIE.stat().st_size if MOVIE.exists() else 0}
    assert MOVIE.exists(), '인트로 영상 파일 없음 (Content/Movies)'
    REPORT['status'] = 'passed'
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    unreal.log_error('BRIGHT_GLOBE_FAIL TITLE_INTRO_APPLY ' + REPORT['error'])
finally:
    (OUT / 'apply-report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2, default=str), encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
