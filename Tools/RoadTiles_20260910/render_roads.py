# render_roads.py
# 역할: 도로·타일 제작 원본과 납품 작업에서 모델을 렌더링해 확인용 이미지를 만든다.

"""Render actual road meshes in isolated QA scenes; never edits source scenes.

Execute in Blender, then call render_sheet(BUILT, path), or render_views(builder,
directory). For short interactive calls render_view(builder, 'top', directory).
"""
import json
import math
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


ORDER = ('Sand', 'Asphalt', 'Wood', 'CreamTile', 'MintHerringbone', 'ConcreteDrain')
VIEWS = ('front', 'back', 'left', 'right', 'top', 'bottom', 'game')
DIRECTIONS = {
    'front': (0, 1, 0), 'back': (0, -1, 0),
    'left': (-1, 0, 0), 'right': (1, 0, 0),
    'top': (0, 0, 1), 'bottom': (0, 0, -1), 'game': (5, -8, 13),
}
MINT = (.40, .72, .66, 1)


def _bounds(objects):
    points = [ob.matrix_world @ Vector(corner) for ob in objects for corner in ob.bound_box]
    if not points:
        raise ValueError('No road meshes supplied for QA rendering')
    lo = Vector([min(p[i] for p in points) for i in range(3)])
    hi = Vector([max(p[i] for p in points) for i in range(3)])
    return lo, hi, points


def _mesh_parts(builder):
    return [ob for ob in builder.parts if ob.type == 'MESH']


def _devices(scene):
    """Prefer the same OptiX configuration as the existing lighting workflow."""
    found = []
    scene.cycles.device = 'CPU'
    try:
        prefs = bpy.context.preferences.addons['cycles'].preferences
        for backend in ('OPTIX', 'CUDA', 'HIP', 'ONEAPI', 'METAL'):
            try:
                prefs.compute_device_type = backend
                prefs.get_devices()
                candidates = [dev for dev in prefs.devices if dev.type == backend]
                if not candidates:
                    continue
                for dev in prefs.devices:
                    dev.use = dev.type == backend
                found = [{'name': dev.name, 'type': dev.type, 'use': bool(dev.use)}
                         for dev in prefs.devices]
                scene.cycles.device = 'GPU'
                break
            except Exception:
                continue
        if not found:
            found = [{'name': dev.name, 'type': dev.type, 'use': bool(dev.use)}
                     for dev in prefs.devices]
    except Exception as exc:
        found = [{'error': str(exc), 'fallback': 'CPU'}]
    return found


def _clone(parts, scene, translation=(0, 0, 0), rotation=None):
    """Link the original mesh datablock and retain its modifiers/materials."""
    result = []
    transform = Matrix.Translation(Vector(translation))
    if rotation is not None:
        transform = transform @ rotation
    for source in parts:
        ob = source.copy()
        source_world = source.matrix_world.copy()
        ob.parent = None
        ob.matrix_world = transform @ source_world
        ob.hide_render = False
        ob.hide_viewport = False
        scene.collection.objects.link(ob)
        ob.hide_set(False, view_layer=scene.view_layers[0])
        result.append(ob)
    return result


def _material(name, color):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = color
    bsdf.inputs['Roughness'].default_value = .82
    return mat


def _studio(name, scale, target, ground_z=None, underside=False):
    scene = bpy.data.scenes.new('QA_ONLY_' + name)
    world = bpy.data.worlds.new('QA_ONLY_' + name + '_World')
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = MINT
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = .35
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    device_list = _devices(scene)
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGB'
    scene.render.film_transparent = False
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = 'Standard'
    try:
        scene.view_settings.look = 'None'
    except TypeError:
        pass
    target = Vector(target)
    scale = max(scale, .01)
    # Underside views have a real light below the mesh and no ground occluder.
    zsign = -1 if underside else 1
    for label, offset, power, size in (
        ('Key', (-2.2, -3.3, 5.4), 520, 3.6),
        ('Fill', (3.6, -1.0, 3.2), 260, 3.0),
        ('Rim', (0.0, 3.2, 4.2), 360, 3.2),
    ):
        data = bpy.data.lights.new('QA_ONLY_' + label, 'AREA')
        data.energy = power * scale * scale * .35
        data.shape = 'DISK'
        data.size = size * scale
        ob = bpy.data.objects.new(data.name, data)
        scene.collection.objects.link(ob)
        ob.location = target + Vector((offset[0], offset[1], offset[2] * zsign)) * scale
        ob.rotation_euler = (target - ob.location).to_track_quat('-Z', 'Y').to_euler()
    if ground_z is not None and not underside:
        mesh = bpy.data.meshes.new('QA_ONLY_Ground')
        extent = scale * 100
        mesh.from_pydata([(-extent, -extent, ground_z), (extent, -extent, ground_z),
                          (extent, extent, ground_z), (-extent, extent, ground_z)], [], [(0, 1, 2, 3)])
        floor = bpy.data.objects.new('QA_ONLY_Ground', mesh)
        scene.collection.objects.link(floor)
        floor.data.materials.append(_material('QA_ONLY_Ground_Mint', MINT))
    camera = bpy.data.cameras.new('QA_ONLY_Camera')
    camera.type = 'ORTHO'
    camera.sensor_fit = 'HORIZONTAL'
    camera.clip_start = scale * .001
    camera.clip_end = scale * 300
    ob = bpy.data.objects.new(camera.name, camera)
    scene.collection.objects.link(ob)
    scene.camera = ob
    scene['qa_source'] = 'actual Blender road geometry; linked original mesh data'
    return scene, device_list


def _camera(scene, objects, direction, width, height, padding=1.15):
    lo, hi, points = _bounds(objects)
    target = (lo + hi) * .5
    scale = max(hi - lo)
    camera = scene.camera
    camera.location = target + Vector(direction).normalized() * scale * 8
    camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
    # Matrix is composed explicitly: caller's active view layer may be another scene.
    camera_matrix = Matrix.Translation(camera.location) @ camera.rotation_euler.to_matrix().to_4x4()
    inv = camera_matrix.inverted()
    projected = [inv @ point for point in points]
    span_x = max(p.x for p in projected) - min(p.x for p in projected)
    span_y = max(p.y for p in projected) - min(p.y for p in projected)
    camera.data.ortho_scale = max(span_x, span_y * width / height) * padding
    scene.render.resolution_x = width
    scene.render.resolution_y = height
    return {
        'location': list(camera.location),
        'rotation_euler_radians': list(camera.rotation_euler),
        'target': list(target), 'orthographic_scale': camera.data.ortho_scale,
        'direction_from_target': list(Vector(direction).normalized()),
        'bounds_min': list(lo), 'bounds_max': list(hi),
    }


def _save(scene, path, record, devices):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(path)
    previous = bpy.context.window.scene if bpy.context.window else None
    try:
        if bpy.context.window:
            bpy.context.window.scene = scene
        bpy.ops.render.render(write_still=True, scene=scene.name)
    finally:
        if previous is not None and bpy.context.window:
            bpy.context.window.scene = previous
    record.update({
        'path': str(path), 'source': 'actual Blender mesh',
        'resolution': [scene.render.resolution_x, scene.render.resolution_y],
        'engine': scene.render.engine, 'samples': scene.cycles.samples,
        'render_device': scene.cycles.device, 'devices': devices,
        'view_transform': scene.view_settings.view_transform,
        'presentation_scene': scene.name,
    })
    path.with_suffix('.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
    manifest_path = path.parent / 'render_manifest.json'
    records = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else []
    records = [item for item in records if item.get('path') != str(path)]
    records.append(record)
    manifest_path.write_text(json.dumps(records, indent=2), encoding='utf-8')
    return record


def _cleanup(scene):
    """Dispose only objects and datablocks belonging to our temporary scene."""
    world = scene.world
    objects = list(scene.objects)
    meshes, materials, lights, cameras = [], [], [], []
    for ob in objects:
        if ob.name.startswith('QA_ONLY_Ground'):
            meshes.append(ob.data)
            materials.extend(list(ob.data.materials))
        elif ob.type == 'LIGHT':
            lights.append(ob.data)
        elif ob.type == 'CAMERA':
            cameras.append(ob.data)
        bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.scenes.remove(scene)
    for data, collection in [(item, bpy.data.meshes) for item in meshes] + \
                            [(item, bpy.data.materials) for item in materials] + \
                            [(item, bpy.data.lights) for item in lights] + \
                            [(item, bpy.data.cameras) for item in cameras]:
        if data.users == 0:
            collection.remove(data)
    if world and world.users == 0:
        bpy.data.worlds.remove(world)


def _ordered(builders):
    if isinstance(builders, dict):
        return [(name, builders[name]) for name in ORDER if name in builders]
    values = list(builders)
    return [(getattr(item, 'name', str(i)), item) for i, item in enumerate(values)]


def render_sheet(builders, outpath):
    """Reference order: sand/asphalt/wood, cream/mint/concrete (3 by 2)."""
    items = _ordered(builders)
    if not items:
        raise ValueError('No road builders')
    spans = [_bounds(_mesh_parts(b)) for _, b in items]
    tile_span = max(max(hi.x - lo.x, hi.y - lo.y) for lo, hi, _ in spans)
    spacing = tile_span * 1.12
    scene, devices = _studio('Road_Six_Tile_Sheet', tile_span * 2.1, (0, 0, 0), ground_z=-tile_span * .006)
    parts, placement = [], []
    try:
        for index, ((label, b), (lo, hi, _)) in enumerate(zip(items, spans)):
            center = (lo + hi) * .5
            offset = Vector(((index % 3 - 1) * spacing - center.x,
                             (.5 - index // 3) * spacing - center.y, -lo.z))
            parts.extend(_clone(_mesh_parts(b), scene, offset))
            placement.append({'asset': b.name, 'reference_label': label,
                              'row': index // 3, 'column': index % 3, 'translation': list(offset)})
        camera = _camera(scene, parts, (0, -.25, 1), 1500, 1050, padding=1.08)
        return _save(scene, outpath, {'view': 'reference_sheet', 'camera': camera,
                                     'layout': placement, 'ground_color_linear_rgba': MINT}, devices)
    finally:
        _cleanup(scene)


def _lod_objects(builder):
    lods = getattr(builder, 'lods', None)
    if lods is None:
        return []
    pairs = list(lods.items()) if isinstance(lods, dict) else list(enumerate(lods))
    result = []
    for index, value in pairs:
        label = str(index).upper()
        if 'LOD' not in label:
            label = 'LOD' + label
        if hasattr(value, 'type'):
            objects = [value]
        elif isinstance(value, dict):
            objects = [item for item in value.values() if getattr(item, 'type', None) == 'MESH']
        else:
            objects = [item for item in value if getattr(item, 'type', None) == 'MESH']
        if objects:
            result.append((label, objects))
    return result


def render_view(builder, view, outdir, lod=None):
    """Render one source view, or one LOD game view with export +X restored to +Y."""
    view = view.lower()
    if view not in DIRECTIONS:
        raise ValueError('Expected one of: ' + ', '.join(VIEWS))
    rotation = None
    label = view
    source = _mesh_parts(builder)
    if lod is not None:
        lod_label = str(lod).upper()
        if not lod_label.startswith('LOD'):
            lod_label = 'LOD' + lod_label
        matches = [(key, objects) for key, objects in _lod_objects(builder) if key == lod_label]
        if not matches:
            raise ValueError('No ' + lod_label + ' export object on ' + builder.name)
        source = matches[0][1]
        rotation = Matrix.Rotation(math.pi / 2, 4, 'Z')
        label = lod_label + '_' + view
    lo, hi, _ = _bounds(source)
    scale = max(hi - lo)
    scene, devices = _studio(builder.name + '_' + label, scale, (lo + hi) * .5,
                             ground_z=lo.z - scale * .006, underside=view == 'bottom')
    try:
        parts = _clone(source, scene, rotation=rotation)
        camera = _camera(scene, parts, DIRECTIONS[view], 640, 640)
        record = {'asset': builder.name, 'view': label, 'camera': camera,
                  'source_forward_axis': '+X' if lod is not None else '+Y',
                  'display_forward_axis': '+Y',
                  'source_objects': [ob.name for ob in source],
                  'underside_light': view == 'bottom',
                  'ground_color_linear_rgba': MINT}
        return _save(scene, Path(outdir) / ('Blender_' + label + '.png'), record, devices)
    finally:
        _cleanup(scene)


def render_views(builder, outdir, views=None, include_lods=True):
    records = [render_view(builder, view, outdir) for view in (views or VIEWS)]
    if include_lods:
        records.extend(render_view(builder, 'game', outdir, lod=label)
                       for label, _ in _lod_objects(builder))
    return records
