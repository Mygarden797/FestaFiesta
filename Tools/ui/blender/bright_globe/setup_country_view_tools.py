"""Frontend에 나라별 에디터 마커와 햇살 접근성 연결을 배치한다. 원래 국가 좌표는 보존한다."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
STAMP = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
OUT = ROOT / 'Saved/BrightGlobe'
OUT.mkdir(parents=True, exist_ok=True)
REPORT = OUT / ('country_view_setup_' + STAMP + '.json')
MAP = '/Game/FestaFiesta/Maps/Frontend'
CATALOG = '/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas'
report = {'status': 'running', 'saved_assets': [], 'markers': []}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def assets():
    return {str(p.relative_to(ROOT)): digest(p) for p in (ROOT / 'Content').rglob('*')
            if p.suffix in ('.umap', '.uasset')}


try:
    assert '-nullrhi' not in unreal.SystemLibrary.get_command_line().lower(), '에디터 마커 배치는 렌더링을 켜고 실행해야 한다'
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    before = assets()
    map_file = ROOT / 'Content/FestaFiesta/Maps/Frontend.umap'
    backup = ROOT / 'Saved/BrightGlobeBefore' / ('CountryView-' + STAMP) / 'Content/FestaFiesta/Maps/Frontend.umap'
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(map_file, backup)
    assert digest(backup) == digest(map_file)
    report['backup_file'] = str(backup)
    assert level.load_level(MAP)
    all_actors = actors.get_all_level_actors()
    globes = [a for a in all_actors if isinstance(a, unreal.FF_AtlasGlobeActor)]
    backdrops = [a for a in all_actors if a.get_actor_label() == 'FF_Clay_Backdrop']
    assert len(globes) == len(backdrops) == 1
    globe = globes[0]
    catalog = unreal.load_asset(CATALOG)
    assert isinstance(catalog, unreal.FF_FestivalAtlasCatalog)
    codes = set()
    for country in catalog.get_editor_property('countries'):
        code = str(country.get_editor_property('country_id'))
        assert code not in codes
        codes.add(code)
        label = 'FF_CountryView_' + code
        existing = [a for a in all_actors if isinstance(a, unreal.FF_AtlasCountryViewMarker)
                    and str(a.get_editor_property('country_id')) == code]
        assert len(existing) <= 1
        if existing:
            marker = existing[0]  # 재실행 때 사용자가 옮긴 위치를 덮어쓰지 않는다.
        else:
            marker = actors.spawn_actor_from_class(unreal.FF_AtlasCountryViewMarker, globe.get_actor_location())
            assert marker
            marker.set_actor_label(label)
            marker.set_folder_path('FF_Editor/CountryViews')
            marker.set_editor_property('globe', globe)
            marker.set_editor_property('catalog', catalog)
            marker.set_editor_property('country_id', country.get_editor_property('country_id'))
            marker.load_country_view()
        assert marker.get_editor_property('globe') == globe and marker.get_editor_property('catalog') == catalog
        location = marker.get_marker_location()
        report['markers'].append({'country': code, 'actor': marker.get_path_name(),
                                  'latitude': location.latitude, 'longitude': location.longitude,
                                  'approach_fraction': marker.get_editor_property('approach_fraction')})
    controllers = [a for a in all_actors if isinstance(a, unreal.FF_TitleSunlightController)]
    assert len(controllers) <= 1
    controller = controllers[0] if controllers else actors.spawn_actor_from_class(unreal.FF_TitleSunlightController, unreal.Vector())
    controller.set_actor_label('FF_TitleSunlightController')
    controller.set_editor_property('backdrop_actor', backdrops[0])
    assert level.save_current_level()
    report['saved_assets'].append(MAP)
    after = assets()
    changed = [p for p in before if before[p] != after.get(p)]
    assert changed == ['Content\\FestaFiesta\\Maps\\Frontend.umap'] or changed == ['Content/FestaFiesta/Maps/Frontend.umap'], changed
    assert before.keys() == after.keys(), '예상하지 않은 패키지 추가/삭제'
    report.update(status='applied', changed_files=changed, map_sha256_after=digest(map_file),
                  country_catalog_unchanged=True, ui_and_geometry_unchanged=True)
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    unreal.log_error('BRIGHT_GLOBE_FAIL ' + report['error'])
REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('COUNTRY_VIEW_SETUP_REPORT ' + str(REPORT))
