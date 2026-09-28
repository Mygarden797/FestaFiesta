"""현재 임포트된 육지 LOD0에서 한국 주변 클릭 표면을 읽는다. 에셋은 저장하지 않는다."""
from pathlib import Path
import json
import math
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / 'Saved/AtlasRegions/Surface'
OUT.mkdir(parents=True, exist_ok=True)
BASE = '/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1'


def run():
    vertices, uvs, indices, meshes = [], [], [], []
    # 경계 밖 이웃 육지도 포함한다. 가까운 일본/북한 지표를 통과해 한국을 고르지 않는다.
    west, south, east, north = 122., 31., 134., 41.
    u0, u1 = (west + 180.) / 360., (east + 180.) / 360.
    v0, v1 = (90. - north) / 180., (90. - south) / 180.
    for tile in range(8):
        mesh = unreal.load_asset(BASE + f'/P05/SM_BrightWorld_{tile:02d}_Top')
        assert mesh
        pos, triangles, normals, texcoords, tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, 0)
        mapping, selected = {}, []
        for start in range(0, len(triangles), 3):
            face = [int(x) for x in triangles[start:start+3]]
            fu = [float(texcoords[i].x) for i in face]
            fv = [float(texcoords[i].y) for i in face]
            if max(fu) < u0 or min(fu) > u1 or max(fv) < v0 or min(fv) > v1:
                continue
            selected.append(face)
        for face in selected:
            for index in face:
                if index not in mapping:
                    mapping[index] = len(vertices)
                    p, uv = pos[index], texcoords[index]
                    vertices.append([float(p.x), float(p.y), float(p.z)])
                    uvs.append([float(uv.x), float(uv.y)])
                indices.append(mapping[index])
        meshes.append(dict(path=mesh.get_path_name(), vertices=len(pos), triangles=len(triangles)//3,
                           selected=len(selected)))
        unreal.log('ATLAS_REGION_SURFACE ' + json.dumps(meshes[-1]))
    assert len(indices) > 30
    # UE UV는 북쪽이 0이다. 위치에서 얻은 위경도와 대조해 투영 방향을 검증한다.
    errors = []
    for p, uv in zip(vertices, uvs):
        radius = math.sqrt(sum(x*x for x in p))
        lat = math.degrees(math.asin(p[2] / radius))
        lon = math.degrees(math.atan2(-p[1], p[0]))
        errors.append(max(abs(lon-(uv[0]*360.-180.)), abs(lat-(90.-uv[1]*180.))))
    assert max(errors) < 1., ('육지 좌표/UV 계약 불일치', max(errors))
    height = unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Earth/DioramaWorld_v1/T_Diorama_Height')
    task = unreal.AssetExportTask()
    task.object = height
    task.exporter = unreal.TextureExporterPNG()
    task.filename = str(OUT / 'height.png')
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    assert unreal.Exporter.run_asset_export_task(task)
    result = dict(status='passed', vertices=vertices, uvs=uvs, indices=indices, meshes=meshes,
                  geographic_bounds=[west,south,east,north], max_vertex_uv_deviation_degrees=max(errors),
                  height_texture=height.get_path_name(), source_radius=5000.)
    (OUT/'surface_raw.json').write_text(json.dumps(result), encoding='utf-8')
    unreal.log('ATLAS_REGION_SURFACE_PASSED ' + str(len(indices)//3))


try:
    run()
except Exception:
    (OUT/'failure.json').write_text(json.dumps(dict(error=traceback.format_exc())), encoding='utf-8')
    unreal.log_error(traceback.format_exc())
