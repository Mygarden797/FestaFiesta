"""전 세계 디오라마를 기존 Frontend 지구에 연결하고 기본 높이의 조형물 밑면을 맞춘다."""
from pathlib import Path
import unreal,json,traceback,shutil
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST='/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1'
report={}
try:
    backup=ROOT/'Saved/DioramaWorldBeforeFrontend';backup.mkdir(exist_ok=True)
    for rel in ['Content/FestaFiesta/Maps/Frontend.umap','Content/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas.uasset']:
        path=ROOT/rel;target=backup/path.name
        if not target.exists():shutil.copy2(path,target)
    layers=[unreal.load_asset(DEST+f'/SM_Diorama_{i:02d}_{suffix}') for i in range(8) for suffix in ['Top','Coast']]
    assert len(layers)==16 and all(layers)
    water=unreal.load_asset(DEST+'/M_Diorama_water');assert water
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);level.load_level('/Game/FestaFiesta/Maps/Frontend')
    globe=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.FF_AtlasGlobeActor))
    report['before']={k:str(globe.get_editor_property(k)) for k in ['globe_material','globe_mesh_asset','globe_texture_yaw_offset']}
    globe.set_editor_property('land_layer_meshes',layers)
    globe.set_editor_property('globe_material',water)
    globe.set_editor_property('globe_texture_yaw_offset',0.)
    components=list(globe.get_editor_property('land_layer_components'))
    assert len(components)==16
    for c,m in zip(components,layers):c.set_static_mesh(m)
    globe.get_editor_property('globe_mesh').set_material(0,water)
    data=json.loads((ROOT/'Tools/ui/data/diorama_world_prop_offsets.json').read_text())
    catalog=unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas');entries=list(catalog.get_editor_property('countries'))
    offsets=[]
    for e in entries:
        code=str(e.get_editor_property('country_id'))
        if code not in data:continue
        loc=e.get_editor_property('location');d=data[code]
        assert abs(loc.get_editor_property('latitude')-d['latitude'])<.001 and abs(loc.get_editor_property('longitude')-d['longitude'])<.001
        offsets.append({'country':code,'before':e.get_editor_property('landmark_surface_offset'),'after':d['surface_offset']})
        e.set_editor_property('landmark_surface_offset',d['surface_offset'])
    catalog.set_editor_property('countries',entries)
    assert unreal.EditorAssetLibrary.save_loaded_asset(catalog,False)
    assert level.save_current_level()
    report.update(status='complete',layers=16,offsets=offsets,backup=str(backup))
except Exception:report.update(status='failed',error=traceback.format_exc());unreal.log_error(report['error'])
(ROOT/'Saved/DioramaWorld_frontend.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
