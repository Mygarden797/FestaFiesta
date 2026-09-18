"""저장된 왕복 에셋을 다시 읽고 지역 시험 레벨의 시작 방향만 정리한다."""
from pathlib import Path
import json
import traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DEST='/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1'
OUT=ROOT/'Saved/BrightGlobe'
report={'status':'running','reopened':[]}
try:
    p2=json.loads((OUT/'p02_ue_import.json').read_text(encoding='utf-8'))
    for row in p2['meshes']:
        for key in ('reference','roundtrip'):
            asset=unreal.load_asset(row[key])
            assert asset and len(asset.get_editor_property('static_materials'))==1
            verts,tris,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(asset,0,0)
            assert len(verts)>0 and len(verts)==len(uvs) and len(tris)%3==0
            report['reopened'].append({'asset':row[key],'vertices':len(verts),'triangles':len(tris)//3})
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.load_level(DEST+'/L_BrightGlobe_Pilot')
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    globe=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.FF_AtlasGlobeActor))
    globe.set_editor_property('start_facing_first_country',False)
    globe.set_editor_property('start_lat_long',unreal.FF_AtlasLatLong(latitude=42.,longitude=12.))
    meshes=globe.get_editor_property('land_layer_meshes')
    assert len(meshes)==6
    for mesh in meshes:
        assert mesh and '/BrightGlobe_v1/P03/' in mesh.get_path_name()
    col=unreal.load_asset(DEST+'/P03/MPC_BrightGlobe_Controls')
    values={str(p.get_editor_property('parameter_name')):p.get_editor_property('default_value') for p in col.get_editor_property('scalar_parameters')}
    assert values=={'LandThickness':40.,'MountainHeight':650.}
    assert level.save_current_level()
    report.update(status='passed',pilot_level=DEST+'/L_BrightGlobe_Pilot',controls=values,start_lat_lon=[42,12],live_meshes=[m.get_path_name() for m in meshes])
    p2['saved_asset_reopen_verified']=True
    (OUT/'p02_ue_import.json').write_text(json.dumps(p2,indent=2),encoding='utf-8')
except Exception:
    report.update(status='failed',error=traceback.format_exc())
    unreal.log_error('BRIGHT_GLOBE_FAIL '+report['error'])
(OUT/'pilot_reopen.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
