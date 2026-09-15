"""Render the six actual lighting LOD0 models like existing facility thumbnails.

Run in a fresh background Blender 5.2 session::

    blender --background --factory-startup --python-exit-code 1 \
        --python Tools/FacilityLighting/render_catalog_thumbnails.py

Append ``-- --fixtures GoldenTree FestivalStreetlamp`` for a preview subset.
The facility camera, studio lights and transparent 1024px format come from
Saved/BlenderProduction_20260909_v2/code/{pipeline,render_uv_evidence}.py.
All model loads and material repairs happen in temporary in-memory scenes;
this script never saves a blend file, FBX, mesh or source material asset.
"""

import argparse
from array import array
import hashlib
import json
from pathlib import Path
import sys
import traceback

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
LIGHTING_SOURCE = ROOT / "SourceArt/FestivalLighting_20260910"
STREETLAMP_SOURCE = (
    ROOT / "Saved/StreetlightProduction_20260909/Lighting/FestivalStreetlamp"
)
OUTPUT = ROOT / "Content/FestaFiesta/Zone/Data/FacilityDefinitions/Lighting/Thumbnails"
REPORT_DIR = ROOT / "Saved/FacilityLighting_20260911"
FIXTURES = (
    "GoldenTree", "StarTree", "CurtainArch", "Festoon",
    "RattanPendant", "FestivalStreetlamp",
)
SIZE_PX = 1024


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def look_at(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def load_lod0(scene, fixture):
    """Read the exact exported LOD0, retaining the facility +X front axis."""
    if fixture == "FestivalStreetlamp":
        source = STREETLAMP_SOURCE / "FestivalStreetlamp.blend"
        manifest = json.loads((STREETLAMP_SOURCE / "validation_report.json").read_text(encoding="utf-8"))
        mesh_name = "SM_FestivalStreetlamp"
        before = sha256(source)
        with bpy.data.libraries.load(str(source), link=False) as (available, loaded):
            if mesh_name not in available.objects:
                raise RuntimeError("Missing actual streetlamp LOD0: " + mesh_name)
            loaded.objects = [mesh_name]
        mesh = loaded.objects[0]
        scene.collection.objects.link(mesh)
        expected_size = manifest["size_m"]
        materials_restored = []
    else:
        folder = LIGHTING_SOURCE / fixture
        manifest = json.loads((folder / "asset_manifest.json").read_text(encoding="utf-8"))
        mesh_name = "SM_FF_" + fixture
        source = folder / "fbx" / (mesh_name + ".fbx")
        before = sha256(source)
        # Editable blends contain source parts only. The delivered FBX is the
        # authoritative combined LOD0 used by Unreal, including authored UVs.
        bpy.ops.import_scene.fbx(filepath=str(source), use_anim=False)
        meshes = [obj for obj in scene.objects if obj.type == "MESH" and not obj.name.startswith("UCX_")]
        if len(meshes) != 1 or not meshes[0].name.startswith(mesh_name):
            raise RuntimeError("Unexpected LOD0 objects: " + repr([obj.name for obj in meshes]))
        mesh = meshes[0]
        for obj in list(scene.objects):
            if obj != mesh:
                bpy.data.objects.remove(obj, do_unlink=True)
        expected_size = manifest["bounds"]["size_m"]
        materials_restored = []
        # FBX does not carry the emission strength input. Restore the values
        # recorded by the exporter, as its existing render_lods.py does.
        for material in mesh.data.materials:
            matches = [entry for entry in manifest["materials"] if material.name.startswith(entry["name"])]
            if len(matches) != 1:
                raise RuntimeError("Unmatched LOD0 material: " + material.name)
            entry = matches[0]
            shader = material.node_tree.nodes.get("Principled BSDF")
            if not shader:
                raise RuntimeError("Missing Principled BSDF: " + material.name)
            shader.inputs["Metallic"].default_value = entry["metallic"]
            shader.inputs["Roughness"].default_value = entry["roughness"]
            shader.inputs["Emission Color"].default_value = entry["emission_color_linear"]
            shader.inputs["Emission Strength"].default_value = entry["emission_strength"]
            materials_restored.append(material.name)

    mesh.hide_render = False
    mesh.hide_viewport = False
    mesh.hide_set(False)
    bpy.context.view_layer.update()
    mesh.data.calc_loop_triangles()
    actual_triangles = len(mesh.data.loop_triangles)
    expected_triangles = manifest["lods"][0]["triangles"]
    if actual_triangles != expected_triangles:
        raise RuntimeError(f"{fixture}: LOD0 triangle mismatch {actual_triangles} != {expected_triangles}")
    points = [mesh.matrix_world @ vertex.co for vertex in mesh.data.vertices]
    minimum = Vector([min(point[i] for point in points) for i in range(3)])
    maximum = Vector([max(point[i] for point in points) for i in range(3)])
    dimensions = maximum - minimum
    if any(abs(float(actual) - expected) > 0.002 for actual, expected in zip(dimensions, expected_size)):
        raise RuntimeError(f"{fixture}: LOD0 bounds mismatch {list(dimensions)} != {expected_size}")
    for material in mesh.data.materials:
        if material and material.use_nodes:
            for node in material.node_tree.nodes:
                if node.type == "TEX_IMAGE" and node.image:
                    image = node.image
                    if image.source == "FILE" and not image.packed_file:
                        path = Path(bpy.path.abspath(image.filepath, library=image.library))
                        if not path.is_file():
                            raise RuntimeError("Missing source texture: " + str(path))
    return mesh, points, dimensions, {
        "source": str(source), "source_sha256": before,
        "mesh": mesh_name, "lod": 0, "triangles": actual_triangles,
        "bounds_m": list(dimensions), "expected_bounds_m": expected_size,
        "materials_restored_from_manifest": materials_restored,
        "front_axis": "+X", "source_model_modified": False,
    }


def configure_presentation(scene, dimensions):
    """Match the original facility render recipe, including its framing."""
    size = max(dimensions)
    target = Vector((0, 0, dimensions.z * 0.5))
    scene.render.engine = "BLENDER_EEVEE"
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = -0.3
    if hasattr(scene, "eevee") and hasattr(scene.eevee, "taa_render_samples"):
        scene.eevee.taa_render_samples = 24
    world = bpy.data.worlds.new(scene.name + "_World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.67, 0.75, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.35
    scene.world = world
    light_target = Vector((0, 0, dimensions.z * 0.46))
    for name, location, power, light_size in (
        ("Key", (size * 0.7, -size, size * 1.6), 50 * size * size, size),
        ("Fill", (-size, size * 0.6, size * 0.9), 20 * size * size, size * 0.9),
    ):
        data = bpy.data.lights.new(scene.name + "_" + name, "AREA")
        data.energy = power
        data.shape = "DISK"
        data.size = light_size
        obj = bpy.data.objects.new(data.name, data)
        scene.collection.objects.link(obj)
        obj.location = location
        look_at(obj, light_target)
    data = bpy.data.cameras.new(scene.name + "_Camera")
    camera = bpy.data.objects.new(data.name, data)
    scene.collection.objects.link(camera)
    camera.location = target + Vector((2.5, -3, 1.8)) * size
    look_at(camera, target)
    data.type = "ORTHO"
    data.ortho_scale = size * 1.5
    data.clip_end = 1000
    scene.camera = camera
    scene.render.resolution_x = SIZE_PX
    scene.render.resolution_y = SIZE_PX
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.film_transparent = True
    bpy.context.view_layer.update()
    return {
        "type": "ORTHO", "location_m": list(camera.location),
        "target_m": list(target), "ortho_scale_m": data.ortho_scale,
        "reference_recipe": "Saved/BlenderProduction_20260909_v2/code/render_uv_evidence.py",
    }


def validate_alpha(path):
    image = bpy.data.images.load(str(path), check_existing=False)
    try:
        if list(image.size) != [SIZE_PX, SIZE_PX] or image.channels != 4:
            raise RuntimeError("Expected 1024x1024 RGBA: " + str(path))
        pixels = array("f", [0.0]) * (SIZE_PX * SIZE_PX * 4)
        image.pixels.foreach_get(pixels)
        alpha = pixels[3::4]
        visible = [index for index, value in enumerate(alpha) if value > 0.01]
        transparent = sum(value == 0 for value in alpha)
        opaque = sum(value > 0.99 for value in alpha)
        if not visible or not transparent or not opaque:
            raise RuntimeError("Missing transparent background or opaque subject: " + str(path))
        xs = [index % SIZE_PX for index in visible]
        ys = [index // SIZE_PX for index in visible]
        # Return image coordinates with the conventional top-left origin.
        bbox = [min(xs), SIZE_PX - 1 - max(ys), max(xs) + 1, SIZE_PX - min(ys)]
        if bbox[0] < 2 or bbox[1] < 2 or bbox[2] > SIZE_PX - 2 or bbox[3] > SIZE_PX - 2:
            raise RuntimeError("Subject touches the thumbnail edge: " + str(bbox))
        return {
            "size_px": list(image.size), "format": "RGBA8 PNG",
            "transparent_pixels": transparent, "opaque_pixels": opaque,
            "visible_bbox_px": bbox, "background": "transparent",
            "occupancy_ratio": len(visible) / len(alpha),
            "alpha_min": min(alpha), "alpha_max": max(alpha),
        }
    finally:
        bpy.data.images.remove(image)


def render_fixture(fixture):
    previous_scene = bpy.context.window.scene
    previous_objects = set(bpy.data.objects)
    scene = bpy.data.scenes.new("LightingCatalog_" + fixture)
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1
    bpy.context.window.scene = scene
    record = None
    try:
        mesh, points, dimensions, record = load_lod0(scene, fixture)
        record.update(fixture=fixture, status="FAIL")
        record["camera"] = configure_presentation(scene, dimensions)
        projected = [world_to_camera_view(scene, scene.camera, point) for point in points]
        framing = {
            "x": [min(point.x for point in projected), max(point.x for point in projected)],
            "y": [min(point.y for point in projected), max(point.y for point in projected)],
        }
        if not all(0 < value < 1 for axis in framing.values() for value in axis):
            raise RuntimeError("Fixture falls outside the original facility frame: " + str(framing))
        record["projected_vertex_bounds"] = framing
        path = OUTPUT / ("T_UI_Lighting_" + fixture + ".png")
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        record.update(validate_alpha(path))
        if sha256(Path(record["source"])) != record["source_sha256"]:
            raise RuntimeError("Source changed during render: " + record["source"])
        record.update(status="PASS", output=str(path), output_sha256=sha256(path), source_file_unchanged=True)
        return record
    finally:
        bpy.context.window.scene = previous_scene
        for obj in list(bpy.data.objects):
            if obj not in previous_objects:
                bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.scenes.remove(scene)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", nargs="+", choices=FIXTURES, default=list(FIXTURES))
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    args = parser.parse_args(argv)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    report = {
        "status": "PASS", "blender_version": bpy.app.version_string,
        "requested_fixtures": args.fixtures, "assets": [],
        "presentation": "Existing facility camera and lighting; transparent 1024x1024 RGBA8",
    }
    report_path = REPORT_DIR / "catalog_thumbnail_render_report.json"
    for fixture in args.fixtures:
        try:
            result = render_fixture(fixture)
        except Exception:
            result = {"fixture": fixture, "status": "FAIL", "error": traceback.format_exc()}
            report["status"] = "FAIL"
        report["assets"].append(result)
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print("LIGHTING_CATALOG_RENDER " + json.dumps(result), flush=True)
    if report["status"] != "PASS":
        raise RuntimeError("Lighting thumbnail render failed; see " + str(report_path))


if __name__ == "__main__":
    main()
