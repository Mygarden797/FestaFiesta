"""검증된 P05 세계판을 공용 Frontend에 연결하고 타이틀 외곽 면 2개만 숨긴다.

UE 에디터 Python 전용. 세계판 검사와 조형물 오프셋 보고서가 passed일 때만 쓴다.
중간 저장 실패 후에는 자동 재시도하지 않는다. 보고서와 바이트 백업을 먼저 확인한다.
완료본 재실행은 저장 파일 해시와 현재 계약만 읽어 확인하며 다시 쓰지 않는다.
"""
from datetime import datetime, timezone
from pathlib import Path
import copy
import hashlib
import json
import math
import shutil
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/BrightGlobe'
REPORT = OUT / 'p06_frontend_integration.json'
DEST = '/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1'
WORLD = DEST + '/P05'
FRONTEND = '/Game/FestaFiesta/Maps/Frontend'
CATALOG = '/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas'
TITLE = '/Game/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual'
PANELS = ('Border_LeftPanel', 'Border_LeftPanelShadow')
FILES = (
    'Content/FestaFiesta/Maps/Frontend.umap',
    'Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset',
    'Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset',
)
COUNTRY_KEYS = (
    'country_id', 'display_name', 'description', 'preview_image', 'location',
    'landmark_mesh', 'landmark_height_override', 'landmark_max_width',
    'landmark_heading', 'landmark_surface_offset', 'map_image',
    'map_borders_image', 'unlocked', 'lock_reason',
)
GLOBE_KEYS = (
    'globe_radius', 'globe_mesh_asset', 'globe_texture_yaw_offset',
    'start_facing_first_country', 'start_lat_long', 'catalog_override',
    'landmark_height', 'cloud_material', 'cloud_radius_scale',
    'fallback_landmark_mesh', 'use_fallback_landmark',
)
CAMERA_KEYS = (
    'field_of_view', 'aspect_ratio', 'constrain_aspect_ratio',
    'override_aspect_ratio_axis_constraint', 'aspect_ratio_axis_constraint',
    'post_process_blend_weight', 'post_process_settings',
)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def canonical(value):
    """포인터 주소가 든 repr 대신 값·에셋 경로·구조체 내보내기 텍스트를 기록한다."""
    if value is None or isinstance(value, (str, bool, int, float)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, 'export_text'):
        return value.export_text()
    return str(value)


def props(obj, names):
    return {name: canonical(obj.get_editor_property(name)) for name in names}


def write_report(report):
    OUT.mkdir(parents=True, exist_ok=True)
    temporary = REPORT.with_suffix('.writing.json')
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    temporary.replace(REPORT)


def load_required(path, cls):
    obj = unreal.load_asset(path)
    assert isinstance(obj, cls), f'필수 에셋 또는 클래스 불일치: {path}'
    return obj


def gate_reports():
    paths = {key: OUT / name for key, name in (
        ('import', 'p05_ue_import.json'),
        ('geometry', 'p05_validation.json'),
        ('prop_offsets', 'p05_prop_offsets.json'),
    )}
    gates = {}
    for key, path in paths.items():
        data = json.loads(path.read_text(encoding='utf-8-sig'))
        assert data.get('status') == 'passed', f'{path.name}: passed가 아님'
        gates[key] = {'path': str(path), 'sha256': digest(path), 'data': data}
    offsets = gates['prop_offsets']['data']
    assert abs(float(offsets['land_thickness']) - 40.) < 1.e-6
    assert abs(float(offsets['mountain_height']) - 650.) < 1.e-6
    assert offsets.get('countries'), '국가 높이 입력이 비어 있음'
    for code, entry in offsets['countries'].items():
        for key in ('latitude', 'longitude', 'surface_offset'):
            assert math.isfinite(float(entry[key])), f'{code}/{key}: 유한값 필요'
        assert 0. <= float(entry['surface_offset']) < 5000., code
    return gates


def widget_tree(bp):
    # UBaseWidgetBlueprint의 native 멤버는 Python reflected property가 아니다.
    trees = list({obj.get_outer() for obj in unreal.ObjectIterator(unreal.Widget)
                  if obj.get_outer() and obj.get_outer().get_class().get_name() == 'WidgetTree'
                  and obj.get_outer().get_outer() == bp})
    assert len(trees) == 1, f'{bp.get_name()} WidgetTree {len(trees)}개'
    return trees[0]


def widget_snapshot(bp):
    tree = widget_tree(bp)
    assert tree, '타이틀 원본 WidgetTree 없음'
    widgets = [w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() == tree]
    result = {}
    for widget in widgets:
        name = widget.get_name()
        assert name not in result, f'중복 위젯 이름: {name}'
        parent = widget.get_parent()
        row = {
            'class': widget.get_class().get_path_name(),
            'parent': parent.get_name() if parent else None,
            'properties': props(widget, ('visibility', 'render_opacity', 'is_enabled',
                                        'render_transform', 'render_transform_pivot')),
        }
        slot = widget.get_editor_property('slot')
        if slot:
            row['slot_class'] = slot.get_class().get_path_name()
            if isinstance(slot, unreal.CanvasPanelSlot):
                row['slot'] = props(slot, ('layout_data', 'auto_size', 'z_order'))
            elif isinstance(slot, unreal.VerticalBoxSlot):
                row['slot'] = props(slot, ('size', 'padding', 'horizontal_alignment', 'vertical_alignment'))
            elif isinstance(slot, unreal.OverlaySlot):
                row['slot'] = props(slot, ('padding', 'horizontal_alignment', 'vertical_alignment'))
            elif isinstance(slot, unreal.SafeZoneSlot):
                row['slot'] = props(slot, ('padding', 'h_align', 'v_align'))
        if isinstance(widget, (unreal.Image, unreal.Border)):
            row['brush'] = canonical(widget.get_editor_property('brush' if isinstance(widget, unreal.Image) else 'background'))
        if isinstance(widget, unreal.FF_CommonButtonBase):
            row['button'] = props(widget, ('button_label', 'icon_brush', 'button_size_override',
                'normal_content_color', 'highlighted_content_color', 'disabled_content_color',
                'tint_label', 'disabled_opacity', 'focus_on_hover', 'show_chevron_only_when_highlighted'))
        result[name] = row
    for name in PANELS:
        assert name in result and result[name]['parent'] == 'CanvasPanel_Content'
        panel = unreal.find_object(tree, name, unreal.Border)
        assert panel and panel.get_children_count() == 0, f'{name} 안에 내용이 있음'
    for name in ('Btn_Play', 'Btn_Options', 'Btn_Quit', 'Image_BrandMark', 'Image_Wordmark', 'VerticalBox_Menu'):
        assert name in result, f'타이틀 필수 위젯 없음: {name}'
    return result


def snapshot(globe, cameras, rig, catalog, bp, dome):
    result = {
        'globe': {'transform': canonical(globe.get_actor_transform()), **props(globe, GLOBE_KEYS)},
        'cameras': {},
        'rig': props(rig, ('title_camera', 'globe_camera', 'country_map_camera')),
        'countries': [props(row, COUNTRY_KEYS) for row in catalog.get_editor_property('countries')],
        'festivals': [row.export_text() for row in catalog.get_editor_property('entries')],
        'widgets': widget_snapshot(bp),
        'dome': {'transform': canonical(dome.get_actor_transform()),
                 'mesh': canonical(dome.static_mesh_component.get_editor_property('static_mesh'))},
    }
    for camera in cameras:
        result['cameras'][camera.get_path_name()] = {
            'transform': canonical(camera.get_actor_transform()),
            **props(camera.get_component_by_class(unreal.CameraComponent), CAMERA_KEYS),
        }
    return result


def check_preserved(before, after):
    expected = copy.deepcopy(before)
    for old, current in zip(expected['countries'], after['countries']):
        assert old['country_id'] == current['country_id'], '국가 순서가 바뀜'
        old['landmark_surface_offset'] = current['landmark_surface_offset']
    for name in PANELS:
        expected['widgets'][name]['properties']['visibility'] = after['widgets'][name]['properties']['visibility']
    assert expected == after, '보존 계약 변경: before/after 보고서를 비교할 것'


def backup_files():
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    folder = ROOT / 'Saved/BrightGlobeBefore' / ('P06-' + stamp)
    folder.mkdir(parents=True, exist_ok=False)
    records = []
    for rel in FILES:
        source = ROOT / rel
        assert source.is_file(), f'백업 원본 없음: {rel}'
        candidates = [source] + [source.with_suffix(ext) for ext in ('.uexp', '.ubulk') if source.with_suffix(ext).exists()]
        for candidate in candidates:
            relative = candidate.relative_to(ROOT)
            dest = folder / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(candidate, dest)
            sha = digest(candidate)
            assert sha == digest(dest), f'백업 해시 불일치: {relative}'
            records.append({'file': str(relative).replace('\\', '/'), 'sha256': sha,
                            'bytes': candidate.stat().st_size, 'backup': str(dest)})
    manifest = {'created_at': datetime.now(timezone.utc).isoformat(), 'files': records}
    (folder / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    return str(folder), records


def run():
    report = {'status': 'preflight', 'mutation_started': False, 'saved_assets': [],
              'started_at': datetime.now(timezone.utc).isoformat()}
    previous = json.loads(REPORT.read_text(encoding='utf-8-sig')) if REPORT.exists() else None
    if previous and previous.get('mutation_started') and previous.get('status') != 'passed':
        raise RuntimeError('이전 P06 변경이 미완료다. 보고서/백업을 확인한 뒤 명시적으로 복구할 것')
    try:
        gates = gate_reports()
        report['gates'] = {key: {k: v for k, v in value.items() if k != 'data'} for key, value in gates.items()}
        offsets = gates['prop_offsets']['data']['countries']
        layers = [load_required(WORLD + f'/SM_BrightWorld_{tile:02d}_{part}', unreal.StaticMesh)
                  for tile in range(8) for part in ('Top', 'Coast')]
        water = load_required(WORLD + '/M_BrightGlobe_water', unreal.MaterialInterface)
        land = load_required(WORLD + '/MI_BrightGlobe_Land', unreal.MaterialInstanceConstant)
        coast = load_required(WORLD + '/MI_BrightGlobe_Coast', unreal.MaterialInstanceConstant)
        backdrop = load_required(DEST + '/MI_BrightGlobe_Backdrop', unreal.MaterialInstanceConstant)
        controls = load_required(WORLD + '/MPC_BrightGlobe_Controls', unreal.MaterialParameterCollection)
        values = {str(p.get_editor_property('parameter_name')): float(p.get_editor_property('default_value'))
                  for p in controls.get_editor_property('scalar_parameters')}
        assert values.get('LandThickness') == 40. and values.get('MountainHeight') == 650.
        for i, mesh in enumerate(layers):
            assert mesh.get_material(0) == (land if i % 2 == 0 else coast), mesh.get_path_name()
        level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        assert level.load_level(FRONTEND)
        actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
        globes = [a for a in actors if isinstance(a, unreal.FF_AtlasGlobeActor)]
        rigs = [a for a in actors if isinstance(a, unreal.FF_AtlasCameraRig)]
        domes = [a for a in actors if a.get_actor_label() == 'FF_Clay_Backdrop']
        assert len(globes) == len(rigs) == len(domes) == 1
        globe, rig, dome = globes[0], rigs[0], domes[0]
        cameras = [a for a in actors if isinstance(a, unreal.CameraActor)]
        assert cameras and abs(float(globe.get_editor_property('globe_radius')) - 5000.) < 1.e-6
        catalog = load_required(CATALOG, unreal.FF_FestivalAtlasCatalog)
        bp = load_required(TITLE, unreal.WidgetBlueprint)
        before = snapshot(globe, cameras, rig, catalog, bp, dome)
        report['before'] = before
        countries = list(catalog.get_editor_property('countries'))
        codes = [str(row.get_editor_property('country_id')) for row in countries]
        assert len(codes) == len(set(codes)) and set(codes) == set(offsets), '국가 높이 입력과 카탈로그 불일치'
        for row in countries:
            code = str(row.get_editor_property('country_id'))
            location = row.get_editor_property('location')
            for axis in ('latitude', 'longitude'):
                assert abs(float(location.get_editor_property(axis)) - float(offsets[code][axis])) < 1.e-4, f'{code}/{axis} 불일치'

        def validate_connection():
            assert list(globe.get_editor_property('land_layer_meshes')) == layers
            assert globe.get_editor_property('globe_material') == water
            assert globe.get_editor_property('globe_mesh').get_material(0) == water
            components = list(globe.get_editor_property('land_layer_components'))
            assert len(components) == 16
            root = globe.get_editor_property('globe_root')
            for comp, mesh in zip(components, layers):
                assert comp.get_owner() == globe and comp.get_attach_parent() == root
                assert comp.get_editor_property('static_mesh') == mesh
                assert comp.get_material(0) == mesh.get_material(0), comp.get_name()
            assert dome.static_mesh_component.get_material(0) == backdrop
            for row in catalog.get_editor_property('countries'):
                code = str(row.get_editor_property('country_id'))
                assert abs(float(row.get_editor_property('landmark_surface_offset')) - float(offsets[code]['surface_offset'])) < .001
            tree = widget_tree(bp)
            for name in PANELS:
                assert unreal.find_object(tree, name, unreal.Border).get_visibility() == unreal.SlateVisibility.COLLAPSED

        if previous and previous.get('status') == 'passed':
            for saved in previous['file_hashes_after']:
                assert digest(ROOT / saved['file']) == saved['sha256'], 'P06 완료 뒤 수정된 파일이 있음: 자동 재적용하지 않음'
            validate_connection()
            assert before == previous['after'], '저장된 P06 계약과 현재 값이 다름'
            unreal.log('P06_BRIGHT_FRONTEND_ALREADY_VERIFIED: 저장 없이 완료 상태 확인')
            return previous

        report['backup_directory'], report['backup_files'] = backup_files()
        report['status'] = 'ready'
        write_report(report)
        # 모든 입력·보존값을 기록하고 원본 파일 해시까지 확인한 후 한 번에 변경한다.
        for record in report['backup_files']:
            assert digest(ROOT / record['file']) == record['sha256'], '백업 이후 원본 변경됨'
        report.update(status='applying', mutation_started=True)
        write_report(report)
        globe.modify()
        globe.set_editor_property('land_layer_meshes', layers)
        globe.set_editor_property('globe_material', water)
        # 에디터 속성 이벤트가 구성 스크립트를 즉시 실행하지 않는 경우도 같은 상태로 맞춘다.
        for comp, mesh in zip(globe.get_editor_property('land_layer_components'), layers):
            comp.set_static_mesh(mesh)
        globe.get_editor_property('globe_mesh').set_material(0, water)
        dome.modify()
        dome.static_mesh_component.set_material(0, backdrop)
        catalog.modify()
        for row in countries:
            code = str(row.get_editor_property('country_id'))
            row.set_editor_property('landmark_surface_offset', float(offsets[code]['surface_offset']))
        catalog.set_editor_property('countries', countries)
        bp.modify()
        tree = widget_tree(bp)
        for name in PANELS:
            panel = unreal.find_object(tree, name, unreal.Border)
            panel.modify()
            panel.set_visibility(unreal.SlateVisibility.COLLAPSED)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        validate_connection()
        after = snapshot(globe, cameras, rig, catalog, bp, dome)
        report['after'] = after
        check_preserved(before, after)
        write_report(report)
        for asset in (catalog, bp):
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False), asset.get_path_name()
            report['saved_assets'].append(asset.get_path_name())
            write_report(report)
        assert level.save_current_level()
        report['saved_assets'].append(FRONTEND)
        write_report(report)
        # 레벨을 다시 열어 직렬화된 컴포넌트 연결도 확인한다. 별도 프로세스 PIE는 후속 검사다.
        assert level.load_level('/Engine/Maps/Entry')
        assert level.load_level(FRONTEND)
        actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
        globe = next(a for a in actors if isinstance(a, unreal.FF_AtlasGlobeActor))
        rig = next(a for a in actors if isinstance(a, unreal.FF_AtlasCameraRig))
        dome = next(a for a in actors if a.get_actor_label() == 'FF_Clay_Backdrop')
        cameras = [a for a in actors if isinstance(a, unreal.CameraActor)]
        catalog, bp = unreal.load_asset(CATALOG), unreal.load_asset(TITLE)
        validate_connection()
        report['reopened'] = snapshot(globe, cameras, rig, catalog, bp, dome)
        assert report['reopened'] == after, '레벨 재열기 뒤 보존값 불일치'
        report['file_hashes_after'] = [{'file': rec['file'], 'sha256': digest(ROOT / rec['file'])}
                                      for rec in report['backup_files']]
        report.update(status='passed', finished_at=datetime.now(timezone.utc).isoformat(),
                      separate_process_reopen_verified=False, pie_verified=False,
                      materials_changed_only_on_frontend_components=True)
        write_report(report)
        unreal.log('P06_BRIGHT_FRONTEND_PASSED: 16개 육지·바다·배경 연결, 국가 높이, 타이틀 외곽 2면 숨김')
        return report
    except Exception:
        # 이미 완료된 저널은 잘못된 재실행으로 덮지 않는다.
        if not previous or previous.get('status') != 'passed':
            report.update(status='failed', error=traceback.format_exc())
            write_report(report)
        raise


if __name__ == '__main__':
    run()
