# finalize_expansion70_blends.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 검증된 결과와 제작 근거를 최종 산출물로 정리한다.

"""Convert tracked Facility70 library writes into ordinary editable .blend files.

Run only in a separate background Blender process after the complete build:
  blender.exe --background --factory-startup --python <this file>

The live artist session is never contacted. Only exact, catalog-matched manifest
paths within the configured Blender directory may be opened or saved.
No renders, FBX re-exports, Unreal operations, or unrelated .blend writes occur.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from expansion70_paths import blend_directory
import re
import sys
import traceback
from datetime import datetime, timezone

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
CATALOG = ROOT / 'Tools/facilities/expansion70_catalog.json'
MANIFEST = ROOT / 'SourceArt/FacilityExpansion70/build_manifest.json'
BLEND_ROOT = blend_directory()
REPORT = ROOT / 'Saved/FacilityExpansion70/blend_finalization_report.json'


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def selected_entries(numbers=None):
    catalog = read_json(CATALOG)['facilities']
    manifest = read_json(MANIFEST)['facilities']
    check(len(catalog) == 70, 'Catalog must contain exactly 70 facilities.')
    check(len(manifest) == 70, 'Finish all 70 source builds before finalizing .blend files.')
    known = {entry['id']: entry for entry in catalog}
    check(len(known) == 70, 'Duplicate catalog IDs.')
    check({int(entry['number']) for entry in catalog} == set(range(1, 71)), 'Catalog numbering mismatch.')
    check(len({entry['id'] for entry in manifest}) == 70, 'Duplicate build-manifest IDs.')
    check({entry['id'] for entry in manifest} == set(known), 'Manifest does not match the catalog.')
    if numbers is not None:
        check(numbers and numbers <= set(range(1, 71)), 'Invalid --numbers selection.')
    result = []
    seen_paths = set()
    for entry in sorted(manifest, key=lambda row: int(row['number'])):
        source = known[entry['id']]
        number = int(entry['number'])
        check(number == int(source['number']), 'Manifest number/ID mismatch: ' + entry['id'])
        expected_name = f'{number:02d}_{source["id"]}.blend'
        path = Path(entry['blend']).resolve(strict=True)
        check(path.is_relative_to(BLEND_ROOT), 'Source escapes the authorized .blend folder: ' + str(path))
        check(path.parent == BLEND_ROOT, 'Nested or redirected .blend source is not supported: ' + str(path))
        check(path.name == expected_name, 'Unexpected source filename: ' + str(path))
        check(path.suffix.lower() == '.blend' and path.name.lower() != 'mainman.blend', 'Protected source rejected.')
        check(path.is_file() and path.stat().st_size > 128, 'Missing or empty source: ' + str(path))
        check(str(path).casefold() not in seen_paths, 'Duplicate source path: ' + str(path))
        seen_paths.add(str(path).casefold())
        if numbers is None or number in numbers:
            result.append((entry, path))
    return result


def owning_scene(entry):
    owners = [scene for scene in bpy.data.scenes if scene.get('FF70.Owner') == entry['id']]
    check(len(owners) == 1, 'Expected exactly one owned scene for ' + entry['id'])
    return owners[0]


def role_collections(scene, entry):
    roots = [collection for collection in scene.collection.children if collection.name.startswith('FAC_' + entry['id'])]
    check(len(roots) == 1, 'Missing or ambiguous facility root collection: ' + entry['id'])
    root = roots[0]
    roles = {}
    for collection in root.children:
        base = re.sub(r'\.\d+$', '', collection.name)
        if base in {'VIS', 'COL', 'GUIDES', 'PREVIEW_ONLY'}:
            check(base not in roles, 'Repeated collection role: ' + base)
            roles[base] = collection
    check({'VIS', 'COL', 'GUIDES'} <= set(roles), 'VIS/COL/GUIDES are incomplete: ' + entry['id'])
    check(len(roles['VIS'].all_objects) > 0, 'No editable visual geometry: ' + entry['id'])
    root.name = 'FAC_' + entry['id']
    root.hide_viewport = False
    root.hide_render = False
    for name, collection in roles.items():
        collection.name = name
        collection.hide_viewport = False
        if name in {'COL', 'GUIDES'}:
            collection.hide_render = True
    return root, roles


def readme_record(scene, entry):
    candidates = []
    linked = scene.get('FF70.Readme')
    if isinstance(linked, bpy.types.Text):
        candidates.append(linked)
    candidates += [text for text in bpy.data.texts if text.name.startswith('FACILITY_README') and text not in candidates]
    for text in candidates:
        try:
            record = json.loads(text.as_string())
        except (ValueError, TypeError):
            continue
        if record.get('id') == entry['id']:
            check(int(record['number']) == int(entry['number']), 'README number mismatch.')
            check(record.get('source_units') == 'metres', 'README unit convention is missing.')
            check(record.get('front') == ('+X' if int(entry['number']) >= 65 else '+Y'), 'README front-axis mismatch.')
            text.name = 'FACILITY_README'
            scene['FF70.Readme'] = text
            return text, record
    raise RuntimeError('No matching production README for ' + entry['id'])


def validate_geometry(scene, roles):
    invalid = []
    negative = []
    for obj in scene.objects:
        values = [float(v) for row in obj.matrix_world for v in row]
        values += [float(v) for v in obj.location] + [float(v) for v in obj.scale]
        if not all(math.isfinite(v) for v in values):
            invalid.append(obj.name)
        if obj.type == 'MESH':
            if any(not math.isfinite(float(c)) for vertex in obj.data.vertices for c in vertex.co):
                invalid.append(obj.name + ': vertices')
        if obj.type in {'MESH', 'FONT', 'CURVE'} and obj.matrix_world.determinant() < 0:
            negative.append(obj.name)
    check(not invalid, 'Nonfinite object transforms or vertices: ' + repr(invalid[:10]))
    visuals = [obj for obj in roles['VIS'].all_objects if obj.type in {'MESH', 'FONT', 'CURVE'}]
    check(visuals, 'No supported editable visual objects.')
    missing = [obj.name for obj in visuals if len(obj.data.materials) == 0 or any(material is None for material in obj.data.materials)]
    check(not missing, 'Missing visual material assignment: ' + repr(missing[:10]))
    materials = {material.name: material for obj in visuals for material in obj.data.materials if material}
    texture_nodes_without_images = []
    for material in materials.values():
        if not material.use_nodes or material.node_tree is None:
            continue
        for node in material.node_tree.nodes:
            if node.type == 'TEX_IMAGE' and node.image is None:
                texture_nodes_without_images.append(material.name + '/' + node.name)
    check(not texture_nodes_without_images, 'Texture image nodes have no images: ' + repr(texture_nodes_without_images))
    images = []
    for image in bpy.data.images:
        if image.users == 0 or image.type in {'RENDER_RESULT', 'COMPOSITING'}:
            continue
        packed = bool(image.packed_file) or bool(getattr(image, 'packed_files', []))
        record = {'name': image.name, 'source': image.source, 'packed': packed, 'filepath': image.filepath}
        if image.source in {'FILE', 'MOVIE', 'TILED'} and not packed:
            raw_path = bpy.path.abspath(image.filepath, library=image.library)
            image_path = Path(raw_path)
            if image.source == 'TILED' and '<UDIM>' in raw_path:
                found = bool(list(image_path.parent.glob(image_path.name.replace('<UDIM>', '[0-9][0-9][0-9][0-9]'))))
            else:
                found = image_path.is_file()
            check(found, 'Missing unpacked image dependency: ' + raw_path)
            record['resolved_filepath'] = str(image_path)
        images.append(record)
    return {'editable_visual_objects': len(visuals), 'collision_objects': len(roles['COL'].all_objects),
            'guide_objects': len(roles['GUIDES'].all_objects), 'material_names': sorted(materials),
            'image_dependencies': images, 'negative_determinant_objects': negative,
            'nonfinite_transform_count': 0, 'missing_material_count': 0, 'missing_image_count': 0}


def visual_bounds(objects):
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    points = [obj.matrix_world @ Vector(corner) for obj in objects
              if obj.type in {'MESH', 'FONT', 'CURVE'}
              for corner in obj.evaluated_get(depsgraph).bound_box]
    check(points, 'Unable to frame an empty facility.')
    lo = Vector([min(p[axis] for p in points) for axis in range(3)])
    hi = Vector([max(p[axis] for p in points) for axis in range(3)])
    check(all(math.isfinite(v) for v in (*lo, *hi)), 'Nonfinite evaluated visual bounds.')
    return lo, hi


def configure_ui(scene, roles):
    windows = list(bpy.context.window_manager.windows)
    check(windows, 'Background process has no window context to serialize a normal startup UI.')
    for window in windows:
        window.scene = scene
    for other in list(bpy.data.scenes):
        if other != scene:
            # Only this isolated process is affected. No other file is saved.
            bpy.data.scenes.remove(other)
    scene.name = 'FAC_' + scene['FF70.Owner']
    check(len(bpy.data.scenes) == 1, 'Source scene selection failed.')
    cameras = [obj for obj in scene.objects if obj.type == 'CAMERA']
    check(cameras, 'Source has no preview camera.')
    if scene.camera not in cameras:
        scene.camera = cameras[0]
    lo, hi = visual_bounds(roles['VIS'].all_objects)
    center = (lo + hi) * .5
    span = max(hi - lo)
    check(span > .001, 'Facility bounds are too small to frame.')
    for obj in scene.objects:
        obj.select_set(False)
    scene.camera.select_set(False)
    for obj in roles['VIS'].all_objects:
        obj.hide_set(False)
    for role in ['COL', 'GUIDES', 'PREVIEW_ONLY']:
        if role in roles:
            for obj in roles[role].all_objects:
                obj.hide_set(True)
    # Keep preview lighting available to rendering, without filling the viewport
    # with light outlines, collision wireframes, and authoring arrows on open.
    scene.render.film_transparent = True
    studio_names = {item.name for item in bpy.context.preferences.studio_lights if item.type == 'STUDIO'}
    viewport_count = 0
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type != 'VIEW_3D':
                continue
            for space in area.spaces:
                if space.type != 'VIEW_3D':
                    continue
                viewport_count += 1
                space.shading.type = 'SOLID'
                space.shading.light = 'STUDIO'
                space.shading.color_type = 'MATERIAL'
                if 'paint.sl' in studio_names:
                    space.shading.studio_light = 'paint.sl'
                space.shading.show_shadows = True
                space.shading.show_cavity = True
                space.shading.cavity_type = 'BOTH'
                space.overlay.show_floor = False
                space.overlay.show_axis_x = False
                space.overlay.show_axis_y = False
                if space.region_3d is not None:
                    space.region_3d.view_location = center
                    space.region_3d.view_rotation = scene.camera.matrix_world.to_quaternion()
                    space.region_3d.view_distance = max(span * 1.8, 1.0)
                    space.region_3d.view_perspective = 'ORTHO'
                space.clip_end = max(1000, span * 100)
    check(viewport_count > 0, 'No View3D screens available for saved source UI.')
    return {'bounds_min_m': list(lo), 'bounds_max_m': list(hi), 'camera': scene.camera.name,
            'saved_viewports': viewport_count, 'scene_count': len(bpy.data.scenes), 'screen_count': len(bpy.data.screens)}


def finalize_one(entry, path):
    original_digest = sha256(path)
    # Retain this isolated process's startup workspaces when the library-only
    # file has no WindowManager or screens of its own.
    result = bpy.ops.wm.open_mainfile(filepath=str(path), load_ui=False)
    check('FINISHED' in result, 'Failed to load ' + str(path))
    scene = owning_scene(entry)
    _, roles = role_collections(scene, entry)
    text, readme = readme_record(scene, entry)
    geometry = validate_geometry(scene, roles)
    ui = configure_ui(scene, roles)
    scene['FF70.FinalizedSource'] = True
    scene['FF70.FinalizedSourceVersion'] = 1
    text.use_fake_user = True
    check(sha256(path) == original_digest, 'Source changed during finalization; refusing to overwrite: ' + str(path))
    # Save-version zero avoids creating/replacing unrelated .blend1 siblings.
    bpy.context.preferences.filepaths.save_version = 0
    result = bpy.ops.wm.save_as_mainfile(filepath=str(path), check_existing=False,
                                         compress=True, copy=False, relative_remap=False)
    check('FINISHED' in result and path.stat().st_size > 128, 'Normal .blend save failed: ' + str(path))
    # This readback loads the saved UI instead of preserving factory workspaces;
    # it detects the original empty-startup-scene problem directly.
    result = bpy.ops.wm.open_mainfile(filepath=str(path), load_ui=True)
    check('FINISHED' in result, 'Readback failed: ' + str(path))
    reread_scene = owning_scene(entry)
    check(len(bpy.data.scenes) == 1, 'Saved .blend contains a stray startup scene.')
    check(all(window.scene == reread_scene for window in bpy.context.window_manager.windows), 'Saved UI opens a different scene.')
    check(reread_scene.camera is not None, 'Saved camera is missing.')
    check(reread_scene.get('FF70.FinalizedSource') is True, 'Finalization metadata did not persist.')
    _, reread_roles = role_collections(reread_scene, entry)
    check(len(reread_roles['VIS'].all_objects) == geometry['editable_visual_objects'], 'Editable part count changed on save.')
    return {'number': int(entry['number']), 'id': entry['id'], 'blend': str(path), 'status': 'passed',
            'original_sha256': original_digest, 'final_sha256': sha256(path), 'bytes': path.stat().st_size,
            **geometry, **ui, 'readback_scene_count': len(bpy.data.scenes),
            'readme': readme, 'render_performed': False}


def write_report(report):
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    temporary = REPORT.with_suffix('.pending.json')
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    os.replace(temporary, REPORT)


def main():
    check(bpy.app.background, 'Finalization must run in a separate --background Blender process.')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--numbers', help='Optional comma-separated subset; the full 70-entry manifest is still required.')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    numbers = {int(value) for value in args.numbers.split(',')} if args.numbers else None
    report = {'schema_version': 1, 'started_utc': datetime.now(timezone.utc).isoformat(),
              'authorized_blend_root': str(BLEND_ROOT), 'manifest': str(MANIFEST),
              'isolated_background_process': True, 'render_performed': False, 'facilities': [], 'status': 'running'}
    try:
        entries = selected_entries(numbers)
        for entry, path in entries:
            try:
                row = finalize_one(entry, path)
            except Exception:
                row = {'number': int(entry['number']), 'id': entry['id'], 'blend': str(path),
                       'status': 'failed', 'error': traceback.format_exc()}
            report['facilities'].append(row)
            write_report(report)
            print('FF70_BLEND_FINALIZE ' + json.dumps({key: row[key] for key in ('number', 'id', 'status')}, ensure_ascii=False), flush=True)
        failures = [row for row in report['facilities'] if row['status'] != 'passed']
        report['status'] = 'failed' if failures else 'passed'
        report['passed_count'] = len(entries) - len(failures)
        report['failed_count'] = len(failures)
        report['finished_utc'] = datetime.now(timezone.utc).isoformat()
        write_report(report)
        check(not failures, 'Some .blend files failed; inspect ' + str(REPORT))
    except Exception:
        report['status'] = 'failed'
        report['fatal_error'] = traceback.format_exc()
        write_report(report)
        raise
    print('FF70_BLEND_FINALIZATION_COMPLETE ' + str(report['passed_count']), flush=True)


if __name__ == '__main__':
    main()
