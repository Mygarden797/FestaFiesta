"""경계 반올림 수정 후 v002 Top/Coast 6개만 재임포트한다."""
from pathlib import Path
import sys
import json
import traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0,str(ROOT/'Tools/ui'))
import import_diorama_pilot as helper
helper.DEST='/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P03v2'
helper.SRC=ROOT/'SourceArt/UI/Atlas/BrightGlobe_v1/P03v2/Exports'
report={'status':'running','meshes':[]}
try:
    assert (ROOT/'Saved/BrightGlobeBefore/v002-before-shorefix/UEAssets').is_dir()
    validation=json.loads((ROOT/'Saved/BrightGlobe/p03_v002_validation.json').read_text(encoding='utf-8'))
    assert validation['status']=='passed'
    for path in sorted(helper.SRC.glob('*.obj')):
        kind=path.stem.rsplit('_',1)[1]
        if kind=='Bottom':continue
        material=unreal.load_asset(helper.DEST+'/M_BrightGlobe_'+('top' if kind=='Top' else 'wall')+'_v002')
        mesh=helper.mesh(path.stem,material)
        verts,tris,normals,uvs,tangents=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,0)
        assert len(verts)==len(uvs) and len(tris)>0
        report['meshes'].append({'asset':mesh.get_path_name(),'triangles':len(tris)//3})
    report['status']='passed'
except Exception:
    report.update(status='failed',error=traceback.format_exc());unreal.log_error('BRIGHT_GLOBE_FAIL '+report['error'])
(ROOT/'Saved/BrightGlobe/p03_v002_coast_reimport.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
