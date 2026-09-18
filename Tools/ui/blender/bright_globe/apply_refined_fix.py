"""확인된 v002 백업 뒤, 원본 inset 0인 절단면만 보정하여 같은 시험본에 저장.

라이브 Blender에서 repair()를 명시적으로 실행한다. 기존 scene/object/Bottom은
유지하며 이전 mesh 데이터도 fake user로 보관한다. MCP/UE 호출은 이 파일에 없다.
"""
import hashlib
import importlib
import json
import os
import shutil
from pathlib import Path

import bpy
import numpy as np
import blender_refine as model
model = importlib.reload(model)

BACKUP = model.ROOT/'Saved/BrightGlobeBefore/v002-before-shorefix'


def _sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def _new_mesh(obj, vertices, faces, corner_uv):
    name = obj.data.name+'_ShoreFix'
    assert name not in bpy.data.meshes, '부분 보정 데이터가 있음: 재개 상태를 먼저 확인'
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    uv = mesh.uv_layers.new(name='UVMap')
    uv.data.foreach_set('uv', np.asarray(corner_uv, dtype=np.float32).ravel())
    for face in mesh.polygons:
        face.use_smooth = True
    for material in obj.data.materials:
        mesh.materials.append(material)
    return mesh


def repair():
    scene = bpy.context.scene
    assert scene.name == model.NAME, '현재 v002 씬에서만 보정 가능'
    assert scene.get('bright_p03_v002_build_complete')
    assert not scene.get('bright_p03_v002_shorefix_complete'), '이미 보정된 씬'
    assert BACKUP.is_dir(), '사전 백업 폴더 필요'
    objects = [bpy.data.objects['GEO-'+model._name(tile, kind)]
               for tile in model.TILES for kind in ('Top', 'Coast')]
    assert all(obj.name in scene.objects for obj in objects)
    assert all(len(obj.users_scene) == 1 and obj.users_scene[0] == scene for obj in objects), '다른 씬 공유 오브젝트 수정 금지'
    destinations = [model.EXPORTS/(obj['bright_p03_v002_export']+'.obj') for obj in objects]
    protected = destinations+[model.BLEND, model.REPORT]
    backup_checks = []
    for path in protected:
        assert path.is_file(), path
        matches = list(BACKUP.rglob(path.name))
        assert len(matches) == 1 and matches[0].is_file(), ('백업 basename 중복/누락', path.name)
        assert _sha(path) == _sha(matches[0]), ('백업과 현재 파일 불일치', path.name)
        backup_checks.append(dict(file=str(path), backup=str(matches[0]), sha256=_sha(path)))
    stage_paths = [path.with_name(path.stem+'_shorefix_staging'+path.suffix) for path in protected]
    assert all(not path.exists() for path in stage_paths), '이전 저장 staging 파일을 먼저 확인'
    tiles, validation = model._prepare()
    staged, old_data = [], [(obj, obj.data, obj.data.use_fake_user) for obj in objects]
    report = json.loads(model.REPORT.read_text(encoding='utf-8'))
    assert not report.get('shorefix'), '이미 보정된 보고서'
    # 데이터 교체 전 모든 형상 계산/UV를 준비한다.
    for item in tiles:
        top = bpy.data.objects['GEO-'+model._name(item['tile'], 'Top')]
        coast = bpy.data.objects['GEO-'+model._name(item['tile'], 'Coast')]
        cv, cf, cuv = model._coast(item)
        staged.append((top, _new_mesh(top, item['vertices'].tolist(), item['faces'].tolist(), item['uv'])))
        staged.append((coast, _new_mesh(coast, cv, cf, cuv)))
        entry = next(t for t in report['tiles'] if t['tile'] == item['tile'])
        widths = [float(np.linalg.norm(item['moved'][i]-outer)*5000.)
                  for a, b, oa, ob in item['edges'] for i, outer in ((a, oa), (b, ob))]
        entry.update(minimum_top_outward_dot=float(item['sign'].min()), min_area_ratio=float(item['area_ratio'].min()),
                     coast_width_uu_percentiles=np.percentile(widths, [0, 50, 90, 100]).tolist())
    selected, active = list(bpy.context.selected_objects), bpy.context.view_layer.objects.active
    old_config = scene.get('bright_globe_config')
    swapped, committed = False, []
    try:
        for obj, old, fake in old_data:
            old.use_fake_user = True
        for obj, mesh in staged:
            obj.data = mesh
        swapped = True
        for obj, temp in zip(objects, stage_paths[:6]):
            hidden = obj.hide_get()
            obj.hide_set(False)
            try:
                for other in scene.objects:
                    other.select_set(False)
                obj.select_set(True)
                bpy.context.view_layer.objects.active = obj
                bpy.ops.wm.obj_export(filepath=str(temp), export_selected_objects=True,
                    forward_axis='Y', up_axis='Z', global_scale=1., apply_modifiers=False,
                    export_uv=True, export_normals=True, export_materials=False,
                    export_triangulated_mesh=False, check_existing=False)
            finally:
                obj.hide_set(hidden)
        # 현재 선택도 저장 직전에 원복한다.
        for obj in scene.objects:
            obj.select_set(obj in selected)
        bpy.context.view_layer.objects.active = active
        scene['bright_globe_config'] = json.dumps(model.CONFIG)
        scene['bright_p03_v002_shorefix_complete'] = True
        report.update(config=model.CONFIG, validation=validation,
                      shorefix=dict(reason='원본 0.01uu 미만 inset의 반올림 잔차 확대 방지',
                                    backup_checks=backup_checks, previous_meshes_preserved=[old.name for _, old, _ in old_data]))
        with stage_paths[-1].open('x', encoding='utf-8') as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
        bpy.ops.wm.save_as_mainfile(filepath=str(stage_paths[-2]), copy=True, check_existing=False)
        assert all(path.is_file() and path.stat().st_size > 0 for path in stage_paths)
        # 모든 임시 출력이 완성된 뒤 백업이 확인된 v002 파일만 교체한다.
        for temp, target in zip(stage_paths, protected):
            os.replace(temp, target)
            committed.append(target)
        print('P03_V002_SHOREFIX_OK', json.dumps(validation))
        return report
    except Exception:
        for target in committed:
            source = next(Path(row['backup']) for row in backup_checks if row['file'] == str(target))
            shutil.copy2(source, target)
        if swapped:
            for obj, old, fake in old_data:
                obj.data = old
                old.use_fake_user = fake
            scene['bright_globe_config'] = old_config
            if 'bright_p03_v002_shorefix_complete' in scene:
                del scene['bright_p03_v002_shorefix_complete']
        raise
    finally:
        for obj in scene.objects:
            obj.select_set(obj in selected)
        bpy.context.view_layer.objects.active = active
