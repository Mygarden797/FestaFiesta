"""Read saved Blender library directories without changing the open scene."""
import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'SourceArt/Roads_20260910'

def validate():
    report=json.loads((ROOT/'validation_report.json').read_text(encoding='utf-8'))
    rows=[]
    for asset in report['assets']:
        name=asset['asset'].removeprefix('SM_Road_');render=asset['fbx_render_object']
        with bpy.data.libraries.load(str(ROOT/'blend'/(asset['asset']+'.blend'))) as (source,target):
            scenes=list(source.scenes);objects=list(source.objects)
        checks={'single_owned_scene':scenes==['FF_ROAD_'+name+'_SOURCE'],
                'editable_parts':len([x for x in objects if x.startswith(name+'__')])==asset['source_parts'],
                'three_export_lods':all(x in objects for x in [render,render+'_LOD1',render+'_LOD2']),
                'exact_ucx_name':asset['fbx_collision_object']=='UCX_'+render+'_00' and asset['fbx_collision_object'] in objects,
                'no_unrelated_startup_objects':not any(x in objects for x in ['Cube','Camera','Light'])}
        rows.append({'asset':name,'status':'PASS' if all(checks.values()) else 'FAIL','checks':checks})
    result={'status':'PASS' if all(x['status']=='PASS' for x in rows) else 'FAIL','assets':rows}
    (ROOT/'qa/saved_blends_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result

if __name__=='__main__':print(json.dumps(validate()))
