# enable_unified_build_area.py
# 역할: 건설 가능 영역 작업에서 해당 기능을 기존 에셋 설정에 활성화한다.

"""Run in Unreal Editor's Python console with the target level open.

Creates one unified construction manager, preserving every existing gameplay region and facility.
The editor user saves the level after reviewing Boundary Status. Automation can pass
FFZS_SAVE_LEVEL=True through runpy.run_path(..., init_globals=...).
"""
import json
from pathlib import Path
import unreal


def enable_unified_build_area(save_level=False):
    actor_system = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_system = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = actor_system.get_all_level_actors()
    regions = [actor for actor in actors if isinstance(actor, unreal.FFZSGameplayZone)]
    areas = [actor for actor in actors if isinstance(actor, unreal.FFZSBuildArea)]
    if not regions:
        raise RuntimeError('현재 레벨에 Gameplay Zone이 없습니다. 작업할 레벨을 먼저 여세요.')
    if len(areas) > 1:
        raise RuntimeError('통합 건설 영역이 2개 이상 있습니다. 레벨마다 1개만 사용하세요.')

    def region_state():
        return sorted([
            {'id': str(actor.get_editor_property('zone_id')),
             'name': actor.get_name(),
             'location': list(actor.get_actor_location().to_tuple()),
             'rotation': list(actor.get_actor_rotation().to_tuple()),
             'scale': list(actor.get_actor_scale3d().to_tuple()),
             'polygon': [list(point.to_tuple()) for point in actor.get_boundary_polygon_local_cm()],
             'neighbors': [str(item) for item in actor.get_editor_property('neighbor_zone_ids')],
             'anchors': [str(item) for item in actor.get_editor_property('entry_exit_anchors')],
             'public_area': actor.get_editor_property('usable_public_area_square_meters'),
             'capacity': actor.get_editor_property('soft_capacity_override'),
             'all_categories': actor.get_editor_property('allow_all_facility_categories'),
             'categories': [str(item) for item in actor.get_editor_property('allowed_facility_categories')],
             'height': actor.get_editor_property('vertical_extent_cm')}
            for actor in regions], key=lambda record: record['id'])

    before = region_state()
    with unreal.ScopedEditorTransaction('Enable unified construction area'):
        area = areas[0] if areas else actor_system.spawn_actor_from_class(
            unreal.FFZSBuildArea, unreal.Vector(0, 0, 0))
        if area is None:
            raise RuntimeError('통합 건설 영역 액터를 만들지 못했습니다.')
        area.set_actor_label('BuildArea_Main')
        area.set_folder_path('Construction')
        area.rebuild_from_world_regions()
        if not area.is_build_area_ready():
            raise RuntimeError(str(area.get_editor_property('boundary_status')))
        if before != region_state():
            raise RuntimeError('기존 지역 정보가 달라졌습니다. 저장하지 말고 작업을 확인하세요.')
        actor_system.set_selected_level_actors([area])
        if save_level and not level_system.save_current_level():
            raise RuntimeError('통합 건설 영역은 준비되었지만 레벨 저장에 실패했습니다.')

    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    report_path = project / 'Artifacts' / 'Validation' / 'UnifiedConstruction' / 'MapSetup.json'
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report = {'build_area': area.get_path_name(), 'source_region_count': len(regions),
              'source_regions_unchanged': before == region_state(), 'saved': save_level,
              'buildable_square_meters': area.get_editor_property('boundary_area_square_meters'),
              'status': str(area.get_editor_property('boundary_status')), 'regions': before}
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('FF_UNIFIED_SETUP_OK ' + json.dumps({key: value for key, value in report.items() if key != 'regions'}, ensure_ascii=False))
    return area


enable_unified_build_area(bool(globals().get('FFZS_SAVE_LEVEL', False)))
