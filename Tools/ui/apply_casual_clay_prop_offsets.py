# -*- coding: utf-8 -*-
"""Casual Clay 65uu 변위에 맞춘 국가 조형물의 밑면 보정. 위경도는 유지한다."""
from pathlib import Path
import json,unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
data=json.loads((root/'Tools/ui/data/casual_clay_prop_offsets.json').read_text())
catalog=unreal.load_asset('/Game/FestaFiesta/UI/Lv_Test/DA_FestivalAtlas')
entries=list(catalog.get_editor_property('countries'));report=[]
for entry in entries:
 code=str(entry.get_editor_property('country_id'))
 if code not in data:continue
 loc=entry.get_editor_property('location')
 assert abs(loc.get_editor_property('latitude')-data[code]['latitude'])<.001
 assert abs(loc.get_editor_property('longitude')-data[code]['longitude'])<.001
 previous=entry.get_editor_property('landmark_surface_offset')
 entry.set_editor_property('landmark_surface_offset',data[code]['surface_offset'])
 report.append({'country':code,'previous_offset':previous,'new_offset':data[code]['surface_offset']})
catalog.set_editor_property('countries',entries)
assert unreal.EditorAssetLibrary.save_loaded_asset(catalog,False)
(root/'Saved/CasualClay_prop_offsets_applied.json').write_text(json.dumps(report,indent=2))

