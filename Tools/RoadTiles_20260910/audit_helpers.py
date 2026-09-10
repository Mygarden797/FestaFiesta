"""Blender MCP festival-light export and executable geometry QA.

Call export_asset(scene, sources, root, name, mats) in Blender. Geometry is in
metres facing -Y; export copies face +X, with a ground-centred placement pivot.
Copied/adapted from the verified MainStageGrand export pipeline; no import-time
actions and no changes to the original stage code or source geometry.
"""
import json
import math
import struct
import traceback
import zlib
from collections import Counter
from pathlib import Path

import bpy
import bmesh
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

EXPORT_SETTINGS = dict(
    use_selection=True, object_types={'MESH'}, global_scale=1.0,
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
    axis_forward='-Y', axis_up='Z', bake_space_transform=False,
    use_mesh_modifiers=True, mesh_smooth_type='FACE', use_tspace=True,
    add_leaf_bones=False, bake_anim=False, path_mode='RELATIVE',
    embed_textures=False, use_custom_props=True,
)


def _json(path, data):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')


def _linear_to_srgb(value):
    return max(0.0, min(1.0, 12.92 * value if value <= .0031308 else 1.055 * value ** (1 / 2.4) - .055))


def _bounds(obj):
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    if not points:
        raise ValueError('Cannot measure empty mesh ' + obj.name)
    low = [min(v[i] for v in points) for i in range(3)]
    high = [max(v[i] for v in points) for i in range(3)]
    return dict(min_m=low, max_m=high,
                size_m=[high[i] - low[i] for i in range(3)],
                center_m=[(high[i] + low[i]) / 2 for i in range(3)],
                min_cm=[v * 100 for v in low], max_cm=[v * 100 for v in high],
                size_cm=[(high[i] - low[i]) * 100 for i in range(3)],
                center_cm=[(high[i] + low[i]) * 50 for i in range(3)])


def _new_collection(scene, name):
    collection = bpy.data.collections.new(name)
    scene.collection.children.link(collection)
    return collection


def _palette_png(path, colors):
    """Actual 256 px RGBA image, 16 cells, Blender UV bottom-left convention."""
    size, cells = 256, 4
    raw = bytearray()
    for y in range(size):
        raw.append(0)
        for x in range(size):
            slot = (cells - 1 - y // 64) * cells + x // 64
            color = colors[slot] if slot < len(colors) else (.12, .14, .19)
            raw.extend(bytes([round(255 * max(0, min(1, c))) for c in color[:3]] + [255]))
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(b'\x89PNG\r\n\x1a\n' +
                          chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) +
                          chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def _materials(source_materials, root):
    ordered = list(source_materials.values()) if hasattr(source_materials, 'values') else list(source_materials)
    if len(ordered) != 8:
        raise ValueError('Lighting set requires 8 ordered palette materials; received %d' % len(ordered))
    colors, report = [], []
    for index, mat in enumerate(ordered):
        pbr = mat.node_tree.nodes.get('Principled BSDF') if mat.use_nodes else None
        base = list(pbr.inputs['Base Color'].default_value if pbr else mat.diffuse_color)
        srgb = list(mat.get('srgb', [_linear_to_srgb(c) for c in base[:3]]))
        colors.append(srgb)
        emission_input = pbr.inputs.get('Emission Color') or pbr.inputs.get('Emission') if pbr else None
        strength_input = pbr.inputs.get('Emission Strength') if pbr else None
        report.append(dict(slot=index, source_material=mat.name,
                           base_color_linear=base, base_color_srgb=srgb,
                           metallic=float(pbr.inputs['Metallic'].default_value) if pbr else 0,
                           roughness=float(pbr.inputs['Roughness'].default_value) if pbr else .5,
                           emission_color_linear=list(emission_input.default_value) if emission_input else [0, 0, 0, 1],
                           emission_strength=float(strength_input.default_value) if strength_input else 0,
                           atlas_cell=[index % 4, index // 4],
                           atlas_uv_center=[(index % 4 + .5) / 4, (index // 4 + .5) / 4]))
    texture = root / 'textures' / 'T_FF_Lighting_Palette.png'
    _palette_png(texture, colors)
    image = bpy.data.images.load(str(texture), check_existing=False)
    image.colorspace_settings.name = 'sRGB'
    image.pack()
    output = []
    for mat, entry in zip(ordered, report):
        # All five assets share the same palette slots and shader parameters.
        # Reuse an owned export material on reruns, avoiding .001 slot drift.
        export_name = mat.name + '_UE'
        exported = bpy.data.materials.get(export_name)
        if exported is None or not exported.get('ff_lighting_export', False):
            exported = mat.copy()
            exported.name = export_name
        exported['ff_lighting_export'] = True
        exported.use_nodes = True
        nodes = exported.node_tree.nodes
        pbr = nodes.get('Principled BSDF')
        if pbr is None:
            raise ValueError('Expected Principled BSDF for explicit UE material ' + mat.name)
        uv = nodes.get('FF_PaletteUV') or nodes.new('ShaderNodeUVMap')
        uv.name = 'FF_PaletteUV'
        uv.uv_map = 'UV0_Palette'
        tex = nodes.get('FF_PaletteTexture') or nodes.new('ShaderNodeTexImage')
        tex.name = 'FF_PaletteTexture'
        tex.image = image
        tex.interpolation = 'Closest'
        tex.extension = 'EXTEND'
        exported.node_tree.links.new(uv.outputs['UV'], tex.inputs['Vector'])
        exported.node_tree.links.new(tex.outputs['Color'], pbr.inputs['Base Color'])
        entry['name'] = exported.name
        entry['texture'] = str(texture)
        exported['palette_slot'] = entry['slot']
        exported['ue_emission_strength'] = entry['emission_strength']
        output.append(exported)
    return output, {m.name: i for i, m in enumerate(ordered)}, report, texture


def _uv0(mesh):
    """Project each triangle inside its material's flat-color swatch.

    Overlap is deliberate: this is a shared color atlas, never a baked lightmap.
    Uniform atlas colors allow a well-conditioned triangle for each triangle.
    This avoids tangent epsilon failures on the thinnest bevel faces in UE.
    """
    uv = mesh.uv_layers.new(name='UV0_Palette')
    for polygon in mesh.polygons:
        slot = polygon.material_index
        if len(polygon.loop_indices) != 3:
            raise ValueError('Palette UV generation requires final triangulated faces')
        vs=[mesh.vertices[v].co for v in polygon.vertices]
        start=max(range(3),key=lambda i:(vs[(i+1)%3]-vs[i]).length_squared)
        ordered_loops=[polygon.loop_indices[(start+i)%3] for i in range(3)]
        for loop_index, local in zip(ordered_loops, ((.12,.12),(.88,.12),(.12,.88))):
            uv.data[loop_index].uv = ((slot % 4 + local[0]) / 4, (slot // 4 + local[1]) / 4)
    mesh.uv_layers.active_index = 0
    uv.active_render = True


def _decimate_part(bm, scene, source, level):
    """Reduce only substantial single closed parts; tiny LED clusters survive.

    A temporary object owns the modifier, leaving every source object untouched.
    Invalid candidates or excessive silhouette bounds changes fall back to the
    original part. Collision-designated structural parts retain their geometry.
    """
    report = dict(applied=False, target_ratio={1: .72, 2: .44}.get(level, 1.0),
                  input_triangles=len(bm.faces), output_triangles=len(bm.faces))
    if level == 0 or len(bm.verts) < 50 or len(bm.faces) < 100 or source.get('collision', False):
        report['reason'] = 'LOD0, small part, or preserved collision structure'
        return report
    # Bead strings comprise many octahedra. Collapsing those would erase lamps.
    first = next(iter(bm.verts))
    seen, queue = {first}, [first]
    while queue:
        current = queue.pop()
        for edge in current.link_edges:
            other = edge.other_vert(current)
            if other not in seen:
                seen.add(other)
                queue.append(other)
    if len(seen) != len(bm.verts):
        report['reason'] = 'Disconnected lamp cluster preserved'
        return report
    temp_mesh = bpy.data.meshes.new('QA_FF_LOD_Simplification')
    bm.to_mesh(temp_mesh)
    temp_obj = bpy.data.objects.new('QA_FF_LOD_Simplification', temp_mesh)
    scene.collection.objects.link(temp_obj)
    reduced = None
    try:
        before = _bounds(temp_obj)
        modifier = temp_obj.modifiers.new('Controlled_PerPart_LOD', 'DECIMATE')
        modifier.decimate_type = 'COLLAPSE'
        modifier.ratio = report['target_ratio']
        modifier.use_collapse_triangulate = True
        bpy.context.view_layer.update()
        depsgraph = bpy.context.evaluated_depsgraph_get()
        reduced = bpy.data.meshes.new_from_object(temp_obj.evaluated_get(depsgraph),
                                                preserve_all_data_layers=True, depsgraph=depsgraph)
        temp_obj.modifiers.clear()
        temp_obj.data = reduced
        candidate = _mesh_audit(temp_obj, include_components=False)
        after = candidate['bounds']
        bounds_ok = all(abs(before[key][i] - after[key][i]) <= max(.0005, before['size_m'][i] * .05)
                        for key in ('min_m', 'max_m') for i in range(3))
        report['bounds_within_5_percent'] = bounds_ok
        if candidate['status'] == 'PASS' and bounds_ok and candidate['triangles'] < len(bm.faces):
            bm.clear()
            bm.from_mesh(reduced)
            bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
            bmesh.ops.triangulate(bm, faces=list(bm.faces))
            report.update(applied=True, output_triangles=len(bm.faces), reason='Validated closed per-part collapse')
        else:
            report['reason'] = 'Candidate rejected: topology, volume, bounds, or no reduction'
            report['candidate_status'] = candidate['status']
    finally:
        bpy.data.objects.remove(temp_obj, do_unlink=True)
        if reduced is not None and reduced.users == 0:
            bpy.data.meshes.remove(reduced)
        if temp_mesh.users == 0:
            bpy.data.meshes.remove(temp_mesh)
    return report


def _build_lod(scene, sources, level, materials, lookup, collection, rotation, asset):
    vertices, faces, slots, parts = [], [], [], []
    modifier_states = [(mod, mod.show_viewport, mod.show_render)
                       for obj in sources for mod in obj.modifiers if mod.type == 'BEVEL']
    try:
        for mod, _, _ in modifier_states:
            mod.show_viewport = level == 0
            mod.show_render = level == 0
        bpy.context.view_layer.update()
        depsgraph = bpy.context.evaluated_depsgraph_get()
        for source in sources:
            if int(source.get('detail', 0)) > 2 - level:
                continue
            evaluated = source.evaluated_get(depsgraph)
            mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
            if mesh is None:
                continue
            bm = bmesh.new()
            try:
                bm.from_mesh(mesh)
                # Weld only within this individual physical part; joints between
                # closed parts are retained, never globally merged or remeshed.
                bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-7)
                bmesh.ops.transform(bm, matrix=rotation @ source.matrix_world, verts=list(bm.verts))
                bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
                bmesh.ops.triangulate(bm, faces=list(bm.faces), quad_method='BEAUTY', ngon_method='BEAUTY')
                simplification = _decimate_part(bm, scene, source, level)
                bm.verts.ensure_lookup_table()
                bm.verts.index_update()
                start, face_start = len(vertices), len(faces)
                vertices.extend(tuple(v.co) for v in bm.verts)
                source_slots = list(mesh.materials)
                for face in bm.faces:
                    if not source_slots:
                        raise ValueError('Source lacks material: ' + source.name)
                    material = source_slots[min(face.material_index, len(source_slots) - 1)]
                    if material is None or material.name not in lookup:
                        raise ValueError('Source material missing from explicit palette: ' + source.name)
                    faces.append(tuple(start + v.index for v in face.verts))
                    slots.append(lookup[material.name])
                parts.append(dict(name=source.get('part_label', source.name), source_object=source.name,
                                  detail=int(source.get('detail', 0)), collision=bool(source.get('collision', False)),
                                  vertex_start=start, vertex_end=len(vertices),
                                  face_start=face_start, face_end=len(faces),
                                  lod_simplification=simplification))
            finally:
                bm.free()
                evaluated.to_mesh_clear()
    finally:
        for mod, viewport, render in modifier_states:
            mod.show_viewport, mod.show_render = viewport, render
        bpy.context.view_layer.update()
    name = asset if level == 0 else asset + '_LOD' + str(level)
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    for mat in materials:
        mesh.materials.append(mat)
    for polygon, slot in zip(mesh.polygons, slots):
        polygon.material_index = slot
        polygon.use_smooth = False
    mesh.update()
    _uv0(mesh)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj['LOD'] = level
    obj['source_parts'] = json.dumps(parts, ensure_ascii=False)
    obj['front_axis'] = '+X'
    obj['pivot'] = 'ground center of evaluated LOD0 bounds'
    return obj, parts


def _mesh_audit(obj, include_components=True):
    mesh = obj.data
    mesh.calc_loop_triangles()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.verts.ensure_lookup_table()
    bm.verts.index_update()
    bm.faces.ensure_lookup_table()
    bm.faces.index_update()
    boundary = sum(e.is_boundary for e in bm.edges)
    nonmanifold = sum(not e.is_manifold for e in bm.edges)
    degenerate = sum(f.calc_area() <= 1e-12 for f in bm.faces)
    duplicates = Counter(tuple(sorted(tuple(round(x, 7) for x in mesh.vertices[i].co)
                                      for i in face.vertices)) for face in mesh.polygons)
    duplicate_faces = sum(count - 1 for count in duplicates.values() if count > 1)
    seen, components = set(), []
    for vertex in bm.verts:
        if vertex.index in seen:
            continue
        queue, face_ids, vertex_ids = [vertex], set(), []
        seen.add(vertex.index)
        while queue:
            current = queue.pop()
            vertex_ids.append(current.index)
            face_ids.update(face.index for face in current.link_faces)
            for edge in current.link_edges:
                neighbor = edge.other_vert(current)
                if neighbor.index not in seen:
                    seen.add(neighbor.index)
                    queue.append(neighbor)
        origin = bm.verts[vertex_ids[0]].co.copy()
        signed = 0.0
        for face_id in face_ids:
            face_vertices = list(bm.faces[face_id].verts)
            for i in range(1, len(face_vertices) - 1):
                signed += (face_vertices[0].co - origin).dot(
                    (face_vertices[i].co - origin).cross(face_vertices[i + 1].co - origin)) / 6
        components.append(dict(vertex_count=len(vertex_ids), triangle_count=len(face_ids), signed_volume_m3=signed))
    bm.free()
    negative = sum(c['signed_volume_m3'] < -1e-12 for c in components)
    zero = sum(abs(c['signed_volume_m3']) <= 1e-12 for c in components)
    uv = mesh.uv_layers.active
    invalid_uv, zero_uv = 0, 0
    if uv is not None:
        for tri in mesh.loop_triangles:
            a, b, c = [uv.data[i].uv for i in tri.loops]
            invalid_uv += sum(not all(math.isfinite(v) and -.00001 <= v <= 1.00001 for v in point)
                              for point in (a, b, c))
            zero_uv += abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) <= 1e-14
    all_triangles = all(len(p.vertices) == 3 for p in mesh.polygons)
    positive_scale = all(abs(v - 1) < 1e-7 for v in obj.scale)
    defects = [boundary, nonmanifold, degenerate, duplicate_faces, negative, zero, invalid_uv, zero_uv]
    report = dict(status='PASS' if not any(defects) and all_triangles and positive_scale else 'FAIL',
                  object=obj.name, vertices=len(mesh.vertices), triangles=len(mesh.loop_triangles),
                  boundary_edges=boundary, nonmanifold_edges=nonmanifold,
                  degenerate_faces=degenerate, duplicate_faces=duplicate_faces,
                  negative_volume_components=negative, zero_volume_components=zero,
                  connected_components=len(components), signed_volume_m3=sum(c['signed_volume_m3'] for c in components),
                  uv_channels=len(mesh.uv_layers), uv0_invalid_coordinates=invalid_uv,
                  uv0_degenerate_triangles=zero_uv, all_faces_triangles=all_triangles,
                  scale=list(obj.scale), positive_unit_scale=positive_scale,
                  bounds=_bounds(obj), material_slots=len(mesh.materials),
                  material_triangle_counts={mesh.materials[i].name: sum(p.material_index == i for p in mesh.polygons)
                                            for i in range(len(mesh.materials))})
    if include_components:
        report['component_volumes'] = components
    return report


def _segment_triangle(a, b, c, d, e):
    ray, u, v = b - a, d - c, e - c
    p = ray.cross(v)
    determinant = u.dot(p)
    if abs(determinant) < 1e-12:
        return False
    inverse, offset = 1 / determinant, a - c
    s = offset.dot(p) * inverse
    if s <= 1e-6 or s >= 1 - 1e-6:
        return False
    q = offset.cross(u)
    t = ray.dot(q) * inverse
    if t <= 1e-6 or s + t >= 1 - 1e-6:
        return False
    depth = v.dot(q) * inverse
    return 1e-6 < depth < 1 - 1e-6


def _self_intersections(obj, parts):
    vertices = [v.co.copy() for v in obj.data.vertices]
    faces = [tuple(p.vertices) for p in obj.data.polygons]
    findings, candidates = [], 0
    for part in parts:
        start, end = part['face_start'], part['face_end']
        base, limit = part['vertex_start'], part['vertex_end']
        local_vertices = vertices[base:limit]
        local_faces = [tuple(i - base for i in f) for f in faces[start:end]]
        if not local_faces:
            continue
        tree = BVHTree.FromPolygons(local_vertices, local_faces, all_triangles=True, epsilon=1e-7)
        for first, second in tree.overlap(tree):
            if first >= second:
                continue
            aa, bb = local_faces[first], local_faces[second]
            if set(aa) & set(bb):
                continue
            candidates += 1
            a, b = [local_vertices[i] for i in aa], [local_vertices[i] for i in bb]
            if (any(_segment_triangle(a[i], a[(i + 1) % 3], *b) for i in range(3)) or
                    any(_segment_triangle(b[i], b[(i + 1) % 3], *a) for i in range(3))):
                findings.append(dict(part=part['name'], triangles=[start + first, start + second]))
    return dict(status='PASS' if not findings else 'FAIL', candidate_pairs=candidates,
                crossing_count=len(findings), crossings=findings[:100],
                method='Per-part BVH and strict nonadjacent triangle crossings; duplicate faces tested separately.',
                limitations='Intentional inter-part joint penetration excluded; coplanar partial overlap and assembly gaps require visual review.')


def _collision(sources, transform, collection, asset):
    hulls, reports = [], []
    for source in sources:
        if not bool(source.get('collision', False)):
            continue
        if source.type != 'MESH':
            raise ValueError('UCX source must be a convex mesh: ' + source.name)
        cage = bmesh.new()
        cage.from_mesh(source.data)
        bmesh.ops.transform(cage, matrix=transform @ source.matrix_world, verts=list(cage.verts))
        bmesh.ops.recalc_face_normals(cage, faces=list(cage.faces))
        source_volume = abs(cage.calc_volume(signed=True))
        bm = bmesh.new()
        for vertex in cage.verts:
            bm.verts.new(vertex.co)
        cage.free()
        bm.verts.ensure_lookup_table()
        result = bmesh.ops.convex_hull(bm, input=list(bm.verts), use_existing_faces=False)
        unused = set(result.get('geom_interior', []) + result.get('geom_unused', []))
        unused_vertices = [item for item in unused if isinstance(item, bmesh.types.BMVert) and item.is_valid]
        if unused_vertices:
            bmesh.ops.delete(bm, geom=unused_vertices, context='VERTS')
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        hull_volume = abs(bm.calc_volume(signed=True))
        volume_error = abs(hull_volume - source_volume) / max(hull_volume, 1e-12)
        if volume_error > 1e-4:
            bm.free()
            raise ValueError('Collision flag on nonconvex source would close an opening: %s (volume difference %.5f)' % (source.name, volume_error))
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        name = 'UCX_' + asset + '_%02d' % len(hulls)
        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        obj = bpy.data.objects.new(name, mesh)
        collection.objects.link(obj)
        obj.display_type = 'WIRE'
        obj.hide_render = True
        obj['source_part'] = source.get('part_label', source.name)
        hulls.append(obj)
        reports.append(dict(name=name, source_object=source.name, bounds=_bounds(obj),
                            source_volume_m3=source_volume, convex_volume_m3=hull_volume,
                            convex_volume_relative_error=volume_error,
                            geometry=_mesh_audit(obj, include_components=False)))
    return hulls, reports


def _export_fbx(path, objects):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    visibility = [(obj, obj.hide_get(), obj.hide_render, obj.hide_viewport) for obj in objects]
    try:
        for obj in objects:
            obj.hide_viewport = False
            obj.hide_set(False)
            obj.hide_render = False
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        bpy.ops.export_scene.fbx(filepath=str(path), **EXPORT_SETTINGS)
    finally:
        for obj, hidden, render, viewport in visibility:
            obj.hide_render = render
            obj.hide_set(hidden)
            obj.hide_viewport = viewport


def _reimport(path, expected, source_scene, expected_collision=0):
    sc = bpy.data.scenes.new('QA_FF_Lighting_FBX_Reimport')
    sc.unit_settings.system = 'METRIC'
    sc.unit_settings.scale_length = 1
    bpy.context.window.scene = sc
    result = dict(status='FAIL', file=str(path))
    try:
        bpy.ops.import_scene.fbx(filepath=str(path), use_custom_normals=True)
        bpy.context.view_layer.update()
        meshes = [o for o in sc.objects if o.type == 'MESH' and not o.name.startswith('UCX_')]
        collision_count = sum(o.name.startswith('UCX_') for o in sc.objects)
        result.update(render_meshes=len(meshes), collision_meshes=collision_count,
                      expected_collision_meshes=expected_collision)
        if len(meshes) != 1:
            return result
        obj = meshes[0]
        evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        mesh = bpy.data.meshes.new_from_object(evaluated)
        mesh.transform(obj.matrix_world)
        obj.data = mesh
        obj.matrix_world = Matrix.Identity(4)
        stats = _mesh_audit(obj, include_components=False)
        expected_stats = _mesh_audit(expected, include_components=False)
        error = max(abs(stats['bounds'][key][i] - expected_stats['bounds'][key][i])
                    for key in ['min_m', 'max_m'] for i in range(3))
        checks = dict(bounds_within_0_1mm=error < .0001,
                      triangle_count=stats['triangles'] == expected_stats['triangles'],
                      closed_geometry=stats['status'] == 'PASS',
                      material_slot_count=stats['material_slots'] == expected_stats['material_slots'],
                      collision_count=collision_count == expected_collision)
        result.update(status='PASS' if all(checks.values()) else 'FAIL', bounds_error_m=error,
                      checks=checks, geometry=stats)
    except Exception:
        result['exception'] = traceback.format_exc()
    finally:
        bpy.context.window.scene = source_scene
        for obj in list(sc.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.scenes.remove(sc)
    return result


def _calibration(scene, root, collection, mat, asset):
    entries = []
    definitions = [(asset + '_Calibration_1m', (0, 0, .5), (1, 1, 1)),
                   (asset + '_Calibration_X', (2, 0, .5), (.75, .25, .25)),
                   (asset + '_Calibration_Y', (0, 3, .5), (.25, 1, .25)),
                   (asset + '_Calibration_Z', (0, 0, 4), (.25, .25, 1.25))]
    for name, center, dimensions in definitions:
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1)
        for vertex in bm.verts:
            vertex.co = Vector([vertex.co[i] * dimensions[i] + center[i] for i in range(3)])
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        mesh.materials.append(mat)
        _uv0(mesh)
        obj = bpy.data.objects.new(name, mesh)
        collection.objects.link(obj)
        path = root / 'qa' / 'calibration' / (name + '.fbx')
        _export_fbx(path, [obj])
        entries.append(dict(name=name, fbx=str(path), bounds_export_m=_bounds(obj),
                            expected_center_cm_identity=[v * 100 for v in center],
                            expected_size_cm=[v * 100 for v in dimensions],
                            fbx_reimport=_reimport(path, obj, scene)))
        obj.hide_render = True
        obj.hide_set(True)
    return dict(status='PASS' if all(e['fbx_reimport']['status'] == 'PASS' for e in entries) else 'FAIL',
                meshes=entries, unreal_axis_check='NOT_RUN',
                description='Independent baked-position meshes share origin. Verify 100cm cube and distinguish +X/+Y/+Z in UE; Y handedness must be measured, not inferred.')


def _lights(lights, transform):
    entries = []
    for obj in lights:
        if obj.type != 'LIGHT':
            continue
        position = transform @ obj.matrix_world.translation
        direction = transform.to_3x3() @ (obj.matrix_world.to_3x3() @ Vector((0, 0, -1)))
        direction.normalize()
        light = obj.data
        entries.append(dict(name=obj.name, type=light.type,
                            position_export_m=list(position), position_export_cm=[v * 100 for v in position],
                            direction_export=list(direction), color_linear=list(light.color),
                            blender_power_w=float(light.energy),
                            ue_lumens=float(obj.get('ue_lumens', light.energy * 12)),
                            ue_lumens_source='explicit object property' if 'ue_lumens' in obj else 'art-directed starting value: Blender watts x12; not a photometric equivalence',
                            attenuation_m=float(obj.get('attenuation_m', 25)),
                            casts_shadow=bool(obj.get('casts_shadow', False)),
                            outer_cone_degrees=math.degrees(light.spot_size / 2) if light.type == 'SPOT' else None,
                            spot_blend=float(light.spot_blend) if light.type == 'SPOT' else None,
                            source_radius_m=float(getattr(light, 'shadow_soft_size', .1)),
                            movable=True))
    return entries


def _scene_light_plan(scene, transform):
    objects = [obj for obj in scene.objects if obj.type == 'LIGHT' and
               ('ue_lumens' in obj or obj.get('asset_light', False))]
    if objects:
        return _lights(objects, transform)
    definitions = scene.get('lights', '[]')
    if isinstance(definitions, str):
        definitions = json.loads(definitions)
    result = []
    for index, entry in enumerate(definitions):
        position = transform @ Vector(entry['location_m'])
        result.append(dict(name=scene.name + '_Practical_%02d' % index, type='POINT',
                           position_source_m=list(entry['location_m']),
                           position_export_m=list(position), position_export_cm=[v * 100 for v in position],
                           color_linear=list(entry.get('color', [1, .74, .36])),
                           ue_lumens=float(entry['intensity_lm']), ue_lumens_source='Explicit source scene light plan',
                           attenuation_m=float(entry['radius_m']), source_radius_m=.12,
                           casts_shadow=False, movable=True))
    return result


def _export_asset(scene, sources, root, name, mats):
    """Create one editable blend, 3 LOD FBXs, UCX, palette, calibration and QA.

    ``name`` is StarTree, CurtainArch, GoldenTree, Festoon or RattanPendant.
    ``sources`` are editable mesh objects in ``scene`` and ``mats`` is the shared
    ordered dictionary of eight Blender materials. ``root`` is an asset-specific
    output folder. detail=0 remains in every LOD; 1 drops in LOD2; 2 only in LOD0.
    Only genuinely convex, individual source objects may set collision=True.
    Scene lights with ue_lumens or asset_light properties enter the UE light plan.
    """
    if name not in {'StarTree', 'CurtainArch', 'GoldenTree', 'Festoon', 'RattanPendant'}:
        raise ValueError('Unexpected lighting asset name: ' + str(name))
    asset = 'SM_FF_' + name
    root = Path(root).resolve()
    root.mkdir(parents=True, exist_ok=True)
    previous_scene = bpy.context.window.scene
    bpy.context.window.scene = scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    sources = list(sources)
    if any(obj.type != 'MESH' or obj.name not in scene.objects for obj in sources):
        raise ValueError('Every physical source must be a mesh in the supplied editable scene')
    if not sources:
        raise ValueError('No physical source geometry supplied')
    export_scene = bpy.data.scenes.new('EXPORT_FF_' + name)
    export_scene['ff_lighting_export'] = True
    export_scene.unit_settings.system = 'METRIC'
    export_scene.unit_settings.scale_length = 1
    export_collection = _new_collection(export_scene, 'EXPORT_' + asset)
    collision_collection = _new_collection(export_scene, 'COLLISION_' + asset)
    calibration_collection = _new_collection(export_scene, 'QA_CALIBRATION_' + asset)
    output_materials, lookup, material_report, texture = _materials(mats, root)
    rotation = Matrix.Rotation(math.pi / 2, 4, 'Z')
    lods, all_parts = [], []
    for level in range(3):
        obj, parts = _build_lod(scene, sources, level, output_materials, lookup, export_collection, rotation, asset)
        lods.append(obj)
        all_parts.append(parts)
    bounds = _bounds(lods[0])
    shift = Vector((-bounds['center_m'][0], -bounds['center_m'][1], -bounds['min_m'][2]))
    transform = Matrix.Translation(shift) @ rotation
    for obj in lods:
        obj.data.transform(Matrix.Translation(shift))
        obj.data.update()
    bpy.context.view_layer.update()
    hulls, collision_report = _collision(sources, transform, collision_collection, asset)
    bpy.context.window.scene = export_scene
    bpy.context.view_layer.update()
    lod_reports, reimport_reports = [], []
    for level, obj in enumerate(lods):
        stats = _mesh_audit(obj)
        intersections = _self_intersections(obj, all_parts[level])
        path = root / 'fbx' / (asset + ('' if level == 0 else '_LOD' + str(level)) + '.fbx')
        _export_fbx(path, [obj] + (hulls if level == 0 else []))
        reimport = _reimport(path, obj, export_scene, len(hulls) if level == 0 else 0)
        reimport_reports.append(reimport)
        stats.update(level=level, fbx=str(path), self_intersections=intersections,
                     part_count=len(all_parts[level]), bevels_evaluated=level == 0,
                     source_detail_max=2 - level, fbx_reimport=reimport)
        lod_reports.append(stats)
        _json(root / 'qa' / ('geometry_LOD%d.json' % level), stats)
        print('LIGHTING_LOD_EXPORT', asset, level, stats['triangles'], stats['status'], reimport['status'], flush=True)
    calibration = _calibration(export_scene, root, calibration_collection, output_materials[0], asset)
    for obj in lods + hulls:
        obj.hide_render = True
        obj.hide_set(True)
    export_collection.hide_render = True
    collision_collection.hide_render = True
    calibration_collection.hide_render = True
    lod_triangles = [item['triangles'] for item in lod_reports]
    report = dict(
        schema_version=1, asset=asset, mesh_name=asset, name=name,
        blender_version=bpy.app.version_string, source_parts=len(sources),
        bounds=_bounds(lods[0]), pivot_m=[0, 0, 0], front_axis='+X', up_axis='+Z',
        source_front_axis='-Y', source_to_export_matrix=[list(row) for row in transform],
        source_to_export_shift_m=list(shift), source_transform_preserved=True,
        materials=material_report, material_slots=len(output_materials), texture=str(texture),
        texture_resolution=[256, 256], lods=lod_reports,
        lod_reduction=dict(triangles=lod_triangles,
                           ratios_to_LOD0=[v / lod_triangles[0] for v in lod_triangles],
                           strictly_decreasing=lod_triangles[0] > lod_triangles[1] > lod_triangles[2]),
        collision_hulls=len(hulls), collision=collision_report, calibration=calibration,
        lights=_scene_light_plan(scene, transform),
        export_settings={k: sorted(v) if isinstance(v, set) else v for k, v in EXPORT_SETTINGS.items()},
        uv0=dict(status='PASS' if all(x['uv0_invalid_coordinates'] == 0 and x['uv0_degenerate_triangles'] == 0 for x in lod_reports) else 'FAIL',
                 method='Shared 4x4 flat-color palette atlas. Well-conditioned fixed UV triangle per mesh triangle; 7.68px inset within each 64px swatch. Constant swatch color means no appearance change.',
                 overlaps='Intentional repeated constant-color swatches; not a lightmap.'),
        lightmap_uv1=dict(status='NOT_APPLICABLE', channels=1,
                          reason='Project Config/DefaultEngine.ini sets r.AllowStaticLighting=False. Production and UE lighting actors are movable; UV1 is not generated.'),
        part_manifest=all_parts[0],
        assembly_gaps=dict(status='NOT_RUN', reason='Closed component topology does not certify joint placement. Inspect front/rear/sides/top/underside render set.'),
        stages=dict(mesh_validation='PASS' if all(x['status'] == 'PASS' and x['self_intersections']['status'] == 'PASS' for x in lod_reports) else 'FAIL',
                    convex_collision='PASS' if all(x['geometry']['status'] == 'PASS' for x in collision_report) else 'FAIL',
                    fbx_reimport='PASS' if all(x['status'] == 'PASS' for x in reimport_reports) else 'FAIL',
                    reference_fidelity='NOT_RUN', unreal_import='NOT_RUN', game_performance='NOT_RUN'),
        nanite=dict(enabled=False, reason='Conventional three-level LODs; project opt-in required.'),
        game_performance=dict(status='NOT_RUN', frame_time_ms=None, draw_calls=None),
    )
    # libraries.write follows dependencies of exactly this owned SOURCE scene.
    # Export clones live in another scene and cannot leak into source renders.
    bpy.context.window.scene = scene
    editable = root / 'blend' / ('FF_' + name + '_Editable.blend')
    editable.parent.mkdir(parents=True, exist_ok=True)
    bpy.data.libraries.write(str(editable), {scene}, path_remap='RELATIVE_ALL', fake_user=True, compress=True)
    report['source_blend'] = str(editable)
    report['light_plan'] = report['lights']
    report['source_scene'] = scene.name
    report['source_objects'] = [dict(name=obj.name, detail=int(obj.get('detail', 0)),
                                     collision=bool(obj.get('collision', False))) for obj in sources]
    report['editable_source'] = dict(status='PASS', object_count=len(sources),
                                     separate_objects=True, export_scene_included=False)
    _json(root / 'asset_manifest.json', report)
    _json(root / 'geometry_manifest.json', report)
    _json(root / 'export_settings.json', report['export_settings'])
    _json(root / 'validation_report.json', dict(
        asset=asset, stages=report['stages'], lods=lod_reports,
        collision=collision_report, calibration=calibration,
        uv0=report['uv0'], lightmap_uv1=report['lightmap_uv1'],
        assembly_gaps=report['assembly_gaps'], game_performance=report['game_performance']))
    # Remove only this call's scratch scene/objects; successful reruns retain
    # deterministic FBX object names and never accumulate exported duplicates.
    for obj in list(export_scene.objects):
        mesh = obj.data if obj.type == 'MESH' else None
        bpy.data.objects.remove(obj, do_unlink=True)
        if mesh is not None and mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    bpy.data.scenes.remove(export_scene)
    bpy.context.window.scene = previous_scene
    return report


def export_asset(scene, sources, root, name, mats):
    """Public, exception-safe wrapper; never modifies or deletes source objects."""
    previous_scene = bpy.context.window.scene
    existing_scenes = {item.as_pointer() for item in bpy.data.scenes}
    try:
        return _export_asset(scene, sources, root, name, mats)
    finally:
        bpy.context.window.scene = previous_scene
        for scratch in list(bpy.data.scenes):
            if scratch.as_pointer() in existing_scenes or not scratch.get('ff_lighting_export', False):
                continue
            for obj in list(scratch.objects):
                mesh = obj.data if obj.type == 'MESH' else None
                bpy.data.objects.remove(obj, do_unlink=True)
                if mesh is not None and mesh.users == 0:
                    bpy.data.meshes.remove(mesh)
            bpy.data.scenes.remove(scratch)
