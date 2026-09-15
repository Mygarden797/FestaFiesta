"""Recreate this kit in a fresh Blender process (or use build_roads via MCP).
Run: blender --background --factory-startup --python <this file>
Only task-generated paths under ROOT are written. Existing deliveries are backed up.
"""
import sys, json, time, shutil
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_roads
root=build_roads.ROOT
stamp=time.strftime('%Y%m%d_%H%M%S')
for name in ['Blends','Exports','Textures']:
    target=root/name
    if target.exists() and any(target.iterdir()):
        backup=root/'Backups'/stamp/name
        shutil.copytree(target,backup)
    target.mkdir(parents=True,exist_ok=True)
(root/'qa').mkdir(parents=True,exist_ok=True)
build_roads.build_all()
result=build_roads.export_all()
print(json.dumps(result,indent=2))
if any(status!='PASS' for levels in result.values() for entry in levels for status in entry[1:]):
    raise RuntimeError('Geometry/reimport validation failed')

