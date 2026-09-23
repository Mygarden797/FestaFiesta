# finalize_delivery.py
# 역할: 도로·타일 제작 원본과 납품 작업에서 검증된 결과와 제작 근거를 최종 산출물로 정리한다.

"""Merge actual evidence after visual review and collect reproducible source code.
The human/agent visual verdict file is separate from automatically derived checks.
"""
from pathlib import Path
import json, hashlib, shutil, datetime, os
TOOLS=Path(__file__).resolve().parent
ROOT=TOOLS.parents[1]/'SourceArt/RoadTiles_20260910'
QA=ROOT/'qa'
def read(name): return json.loads((QA/name).read_text(encoding='utf-8-sig'))
def write(path,value): path.write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')

def main():
    built=read('blender_validation.json')
    ue=read('unreal_import_report.json')
    reload=read('unreal_reload_report.json')
    capture=read('unreal_capture_report.json')
    visual=read('visual_review.json')
    saved=read('saved_blend_verification.json')
    composition=read('composition_manifest.json')
    assert len(built['assets'])==6 and len(ue['entries'])==6
    statuses={
      'reference_reproduction':visual['reference_reproduction']['status'],
      'mesh_validation':'PASS' if all(l['geometry']['status']=='PASS' and l['self_intersections']['status']=='PASS' for a in built['assets'] for l in a['lods']) else 'FAIL',
      'fbx_reimport':'PASS' if all(l['fbx_reimport']['status']=='PASS' for a in built['assets'] for l in a['lods']) else 'FAIL',
      'saved_blend_reopen':saved['status'],
      'six_sides_and_game_view':visual['multiview']['status'],
      'lod_visual':visual['lod_visual']['status'],
      'unreal_import':ue['status'],
      'unreal_reload':reload['status'],
      'unreal_gpu_capture':capture['status'],
      'unreal_surface_visual':visual['unreal_surface']['status'],
      'game_performance':'NOT_RUN',
      'lightmap_uv1':'NOT_APPLICABLE',
    }
    assert all(v in ('PASS','NOT_RUN','NOT_APPLICABLE') for v in statuses.values()),statuses
    assert all(x['complete'] for x in composition['view_sheets']) and composition['lod_sheet']['complete']
    original_value=os.environ.get('FF_ROAD_REFERENCE_IMAGE')
    if not original_value:
        raise RuntimeError('Set FF_ROAD_REFERENCE_IMAGE to the original road reference image for independent hash verification.')
    original=Path(original_value).expanduser().resolve()
    sha=lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    assert sha(original)==sha(ROOT/'References/Road.png')
    calibration=read('calibration.json')
    calibration['unreal_axis_check']=ue['axis_contract']['status']
    calibration['unreal_evidence']='unreal_import_report.json'
    write(QA/'calibration.json',calibration)
    built['calibration']=calibration; write(QA/'blender_validation.json',built)
    report={
      'status':'PASS','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
      'reference_sha256':sha(original),'reference_unmodified':True,
      'blender_version':built['version'],'unreal_version':ue['engine_version'],
      'destination':ue['destination'],'gallery_map':capture['gallery_map'],'stages':statuses,
      'assets':built['assets'],'unreal_measurements':ue['entries'],
      'visual_review':visual,'axis_contract':ue['axis_contract'],
      'game_performance':{'status':'NOT_RUN','frame_time_ms':None,'draw_calls':None,'npc_walking_test':None},
      'limitations':['Dimensions and underside inferred from reference.','Fixed reference patterns; no curves/intersections or all-edge pattern tiling variants.','Per-part self intersection test excludes intentional inter-part construction overlaps; reviewed via rendered views.','LOD2 omits micro grain and fasteners.','Runtime navigation and performance not measured.'],
    }
    write(ROOT/'validation_report.json',report)
    code=ROOT/'Code'; code.mkdir(exist_ok=True)
    for pattern in ('*.py','*.ps1'):
      for p in TOOLS.glob(pattern): shutil.copy2(p,code/p.name)
    manifest={}
    for folder in ('Blends','Exports','Textures','References','Code'):
      for p in sorted((ROOT/folder).rglob('*')):
       if p.is_file(): manifest[str(p.relative_to(ROOT))]={'sha256':sha(p),'bytes':p.stat().st_size}
    write(ROOT/'file_manifest.json',manifest)
    print(json.dumps({'status':'PASS','assets':6,'lod_files':18,'destination':ue['destination'],'manifest_entries':len(manifest)},indent=2))
if __name__=='__main__': main()
