"""Build low-poly FestaFiesta facility meshes and preview renders with Blender.

Run from the repository root:
    UnrealEditor-Cmd.exe FestaFiesta.uproject -ExecutePythonScript=Tools/facilities/export_facility_authoring_manifest.py
    blender --background --factory-startup --python Tools/facilities/build_facility_meshes.py

The scene uses metres. FBX unit metadata lets Unreal import each metre as 100 cm.
Every facility faces +Y, has its pivot at ground-centre (0, 0, 0), and includes a
single simple UCX collision box. All decorative parts are joined into one visual
mesh while the small shared material palette remains as named FBX slots.
"""

from __future__ import annotations

import math
import json
import hashlib
import re
from collections import Counter
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
FBX_DIR = ROOT / "SourceAssets" / "Zone" / "FacilityMeshes"
PREVIEW_DIR = ROOT / "Saved" / "FacilityMeshes_Preview"
AUTHORING_MANIFEST = ROOT / "Saved" / "FacilityDefinitionAuthoringManifest.json"
BUILD_MANIFEST = ROOT / "Saved" / "FacilityMeshBuildManifest.json"

FBX_DIR.mkdir(parents=True, exist_ok=True)
PREVIEW_DIR.mkdir(parents=True, exist_ok=True)


PALETTE = {
    "FF_WoodDark": ((0.16, 0.048, 0.014, 1.0), 0.82, 0.0, None),
    "FF_WoodWarm": ((0.46, 0.14, 0.028, 1.0), 0.72, 0.0, None),
    "FF_Teal": ((0.008, 0.43, 0.50, 1.0), 0.66, 0.0, None),
    "FF_TealDark": ((0.006, 0.13, 0.17, 1.0), 0.78, 0.05, None),
    "FF_Cream": ((0.90, 0.66, 0.36, 1.0), 0.78, 0.0, None),
    "FF_Metal": ((0.035, 0.045, 0.048, 1.0), 0.42, 0.52, None),
    "FF_AmberGlow": ((1.0, 0.48, 0.025, 1.0), 0.36, 0.0, (1.0, 0.08, 0.002, 3.0)),
    "FF_SunsetGlow": ((0.80, 0.055, 0.002, 1.0), 0.42, 0.0, (1.0, 0.015, 0.0001, 0.5)),
    "FF_Green": ((0.035, 0.36, 0.105, 1.0), 0.88, 0.0, None),
    "FF_Medical": ((0.90, 0.045, 0.025, 1.0), 0.62, 0.0, None),
    "FF_BlueGrey": ((0.14, 0.34, 0.40, 1.0), 0.72, 0.05, None),
    "FF_Glass": ((0.06, 0.42, 0.52, 1.0), 0.24, 0.08, None),
}


def reset_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.meshes, bpy.data.curves, bpy.data.cameras, bpy.data.lights):
        for datablock in list(datablocks):
            if datablock.users == 0:
                datablocks.remove(datablock)


def configure_scene() -> None:
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 768
    scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.film_transparent = False
    scene.render.image_settings.color_depth = "8"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.view_settings.exposure = 0.55
    world = bpy.data.worlds.new("FF_PreviewWorld") if not bpy.data.worlds else bpy.data.worlds[0]
    scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.0015, 0.006, 0.010, 1.0)
    background.inputs["Strength"].default_value = 0.28


def make_materials() -> dict[str, bpy.types.Material]:
    materials: dict[str, bpy.types.Material] = {}
    for name, (color, roughness, metallic, emission) in PALETTE.items():
        material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        material.use_nodes = True
        material.diffuse_color = color
        bsdf = material.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = color
        bsdf.inputs["Roughness"].default_value = roughness
        bsdf.inputs["Metallic"].default_value = metallic
        if emission:
            emission_color = (*emission[:3], 1.0)
            if "Emission Color" in bsdf.inputs:
                bsdf.inputs["Emission Color"].default_value = emission_color
                bsdf.inputs["Emission Strength"].default_value = emission[3]
            else:
                bsdf.inputs["Emission"].default_value = emission_color
                bsdf.inputs["Emission Strength"].default_value = emission[3]
        else:
            low_emission = (*color[:3], 1.0)
            if "Emission Color" in bsdf.inputs:
                bsdf.inputs["Emission Color"].default_value = low_emission
                bsdf.inputs["Emission Strength"].default_value = 0.10
            else:
                bsdf.inputs["Emission"].default_value = low_emission
                bsdf.inputs["Emission Strength"].default_value = 0.10
        materials[name] = material
    return materials


MATERIALS = make_materials()


def assign_material(obj: bpy.types.Object, material_name: str) -> bpy.types.Object:
    obj.data.materials.append(MATERIALS[material_name])
    obj.active_material_index = 0
    return obj


def box(
    name: str,
    location: tuple[float, float, float],
    dimensions: tuple[float, float, float],
    material: str,
    bevel: float = 0.025,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel > 0.0:
        modifier = obj.modifiers.new("FF_EdgeSoftness", "BEVEL")
        modifier.width = min(bevel, min(dimensions) * 0.22)
        modifier.segments = 1
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return assign_material(obj, material)


def cylinder(
    name: str,
    location: tuple[float, float, float],
    radius: float,
    depth: float,
    material: str,
    vertices: int = 12,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices,
        radius=radius,
        depth=depth,
        end_fill_type="NGON",
        location=location,
        rotation=rotation,
    )
    obj = bpy.context.object
    obj.name = name
    return assign_material(obj, material)


def sphere(
    name: str,
    location: tuple[float, float, float],
    scale: tuple[float, float, float],
    material: str,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return assign_material(obj, material)


def mesh_from_vertices(
    name: str,
    vertices: list[tuple[float, float, float]],
    faces: list[tuple[int, ...]],
    material: str,
) -> bpy.types.Object:
    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return assign_material(obj, material)


def prism_gable(
    name: str,
    width: float,
    depth: float,
    bottom: float,
    ridge: float,
    material: str,
) -> bpy.types.Object:
    x = width * 0.5
    y = depth * 0.5
    verts = [
        (-x, -y, bottom), (x, -y, bottom), (0, -y, ridge),
        (-x, y, bottom), (x, y, bottom), (0, y, ridge),
    ]
    faces = [(0, 1, 2), (5, 4, 3), (0, 3, 4, 1), (1, 4, 5, 2), (2, 5, 3, 0)]
    return mesh_from_vertices(name, verts, faces, material)


def awning(
    name: str,
    centre: tuple[float, float, float],
    width: float,
    depth: float,
    rise: float,
    material: str = "FF_Teal",
) -> bpy.types.Object:
    cx, cy, cz = centre
    x = width * 0.5
    y = depth * 0.5
    verts = [
        (cx - x, cy - y, cz), (cx + x, cy - y, cz),
        (cx + x, cy + y, cz - rise), (cx - x, cy + y, cz - rise),
        (cx - x, cy - y, cz - 0.055), (cx + x, cy - y, cz - 0.055),
        (cx + x, cy + y, cz - rise - 0.055), (cx - x, cy + y, cz - rise - 0.055),
    ]
    faces = [(0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]
    return mesh_from_vertices(name, verts, faces, material)


def lantern(name: str, x: float, y: float, z: float, scale: float = 1.0) -> list[bpy.types.Object]:
    parts = [
        cylinder(f"{name}_CapTop", (x, y, z + 0.12 * scale), 0.075 * scale, 0.055 * scale, "FF_Metal", 8),
        cylinder(f"{name}_Glow", (x, y, z), 0.092 * scale, 0.22 * scale, "FF_AmberGlow", 10),
        cylinder(f"{name}_CapBottom", (x, y, z - 0.12 * scale), 0.075 * scale, 0.055 * scale, "FF_Metal", 8),
    ]
    return parts


def post(name: str, x: float, y: float, z0: float, z1: float, thick: float = 0.13) -> bpy.types.Object:
    return box(name, (x, y, (z0 + z1) * 0.5), (thick, thick, z1 - z0), "FF_WoodDark", thick * 0.10)


def stairs(name: str, width: float, y_front: float, z_top: float, count: int = 2) -> list[bpy.types.Object]:
    parts = []
    step_depth = 0.24
    step_height = z_top / count
    for index in range(count):
        height = step_height * (index + 1)
        y = y_front + step_depth * (count - index - 0.5)
        parts.append(box(f"{name}_{index}", (0, y, height * 0.5), (width, step_depth, height), "FF_WoodWarm", 0.018))
    return parts


def planter(name: str, x: float, y: float, z: float = 0.22, width: float = 0.62) -> list[bpy.types.Object]:
    parts = [box(f"{name}_Box", (x, y, z), (width, 0.34, 0.32), "FF_WoodWarm", 0.025)]
    for offset in (-0.20, 0.0, 0.20):
        parts.append(sphere(f"{name}_Leaf_{offset}", (x + offset, y, z + 0.28), (0.15, 0.13, 0.22), "FF_Green"))
    return parts


def plus_symbol(name: str, location: tuple[float, float, float], size: float, material: str, depth: float = 0.055) -> list[bpy.types.Object]:
    x, y, z = location
    return [
        box(f"{name}_H", (x, y, z), (size, depth, size * 0.28), material, 0.012),
        box(f"{name}_V", (x, y, z), (size * 0.28, depth, size), material, 0.012),
    ]


def normalize_material_slots(obj: bpy.types.Object) -> None:
    old_materials = list(obj.data.materials)
    unique: list[bpy.types.Material] = []
    index_map: dict[int, int] = {}
    for old_index, material in enumerate(old_materials):
        if material in unique:
            index_map[old_index] = unique.index(material)
        else:
            index_map[old_index] = len(unique)
            unique.append(material)
    remapped_indices = [index_map.get(polygon.material_index, 0) for polygon in obj.data.polygons]
    obj.data.materials.clear()
    for material in unique:
        obj.data.materials.append(material)
    for polygon, material_index in zip(obj.data.polygons, remapped_indices):
        polygon.material_index = material_index
    obj.data.update()


def join_visual_parts(asset_name: str, parts: list[bpy.types.Object]) -> bpy.types.Object:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    visual = bpy.context.object
    visual.name = asset_name
    visual.data.name = f"{asset_name}_Mesh"
    normalize_material_slots(visual)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return visual


def fit_and_center(obj: bpy.types.Object, contract: tuple[float, float, float]) -> None:
    """Centre the footprint, ground the mesh, and fit it inside its art contract."""
    coordinates = [vertex.co.copy() for vertex in obj.data.vertices]
    minimum = Vector((min(v.x for v in coordinates), min(v.y for v in coordinates), min(v.z for v in coordinates)))
    maximum = Vector((max(v.x for v in coordinates), max(v.y for v in coordinates), max(v.z for v in coordinates)))
    centre_xy = Vector(((minimum.x + maximum.x) * 0.5, (minimum.y + maximum.y) * 0.5, minimum.z))
    for vertex in obj.data.vertices:
        vertex.co -= centre_xy
    obj.data.update()
    bpy.context.view_layer.update()

    dimensions = obj.dimensions.copy()
    scale = Vector(
        tuple(min(1.0, (limit * 0.995) / actual) if actual > 0 else 1.0 for actual, limit in zip(dimensions, contract))
    )
    for vertex in obj.data.vertices:
        vertex.co.x *= scale.x
        vertex.co.y *= scale.y
        vertex.co.z *= scale.z
    obj.data.update()
    obj.location = (0.0, 0.0, 0.0)
    bpy.context.view_layer.update()


def build_dada_pocha_food_truck() -> list[bpy.types.Object]:
    p: list[bpy.types.Object] = []
    p += [
        box("TruckChassis", (0, 0, 0.44), (5.18, 2.22, 0.34), "FF_Metal", 0.045),
        box("TruckBody", (0.60, -0.04, 1.25), (3.84, 2.16, 1.45), "FF_WoodDark", 0.055),
        box("TruckCab", (-2.02, -0.04, 1.02), (1.16, 2.06, 1.22), "FF_BlueGrey", 0.12),
        box("CabWindshield", (-2.24, 1.005, 1.27), (0.68, 0.035, 0.45), "FF_Glass", 0.01),
        box("ServiceGlow", (0.72, 1.055, 1.39), (2.72, 0.045, 0.76), "FF_AmberGlow", 0.012),
        box("ServiceInner", (0.72, 1.082, 1.38), (2.55, 0.04, 0.60), "FF_WoodDark", 0.008),
        box("Counter", (0.72, 1.25, 1.00), (3.16, 0.36, 0.16), "FF_WoodWarm", 0.03),
        awning("TruckAwning", (0.72, 1.16, 2.12), 3.35, 0.68, 0.16),
        box("RoofBase", (0.35, -0.02, 2.16), (4.56, 2.36, 0.14), "FF_TealDark", 0.04),
        prism_gable("TruckRoof", 4.75, 2.42, 2.14, 2.45, "FF_Teal"),
    ]
    for x in (-1.82, 1.77):
        for y in (-1.07, 1.07):
            p += [
                cylinder(f"Wheel_{x}_{y}", (x, y, 0.43), 0.37, 0.20, "FF_Metal", 12, (math.pi / 2, 0, 0)),
                cylinder(f"Hub_{x}_{y}", (x, y + (0.11 if y > 0 else -0.11), 0.43), 0.15, 0.035, "FF_Cream", 10, (math.pi / 2, 0, 0)),
            ]
    for x in (-0.54, 0.12, 0.78, 1.44):
        p.append(box(f"FoodTray_{x}", (x, 1.31, 1.16), (0.40, 0.24, 0.08), "FF_Cream", 0.018))
    for x in (-0.88, 2.32):
        p += lantern(f"TruckLantern_{x}", x, 1.30, 1.70, 0.82)
    for x in (-0.6, 0.4, 1.4):
        p += [
            box(f"StoolSeat_{x}", (x, 1.62, 0.56), (0.46, 0.38, 0.12), "FF_WoodWarm", 0.025),
            box(f"StoolLeg_{x}", (x, 1.62, 0.30), (0.12, 0.12, 0.48), "FF_WoodDark", 0.015),
        ]
    return p


def build_main_information() -> list[bpy.types.Object]:
    p: list[bpy.types.Object] = [
        box("InfoDeck", (0, 0, 0.13), (4.95, 2.38, 0.26), "FF_WoodDark", 0.04),
        box("InfoBody", (0, -0.25, 1.08), (4.35, 1.63, 1.72), "FF_WoodWarm", 0.055),
        box("InfoOpeningGlow", (0, 0.59, 1.35), (2.55, 0.035, 0.76), "FF_AmberGlow", 0.01),
        box("InfoOpening", (0, 0.615, 1.35), (2.32, 0.04, 0.60), "FF_WoodDark", 0.01),
        box("InfoCounter", (0, 0.86, 0.98), (3.05, 0.48, 0.17), "FF_WoodWarm", 0.035),
        prism_gable("InfoRoof", 5.02, 2.48, 2.06, 2.38, "FF_Teal"),
        box("InfoRoofBeam", (0, 0, 2.10), (5.15, 0.16, 0.16), "FF_WoodDark", 0.025),
    ]
    for x in (-2.10, 2.10):
        p += [post(f"InfoPost_{x}", x, 0.90, 0.20, 2.16), *lantern(f"InfoLantern_{x}", x, 1.01, 1.46, 0.72)]
    p += [
        cylinder("InfoSignDisc", (0, 1.24, 2.18), 0.36, 0.10, "FF_WoodDark", 18, (math.pi / 2, 0, 0)),
        cylinder("InfoSignInner", (0, 1.30, 2.18), 0.27, 0.035, "FF_AmberGlow", 18, (math.pi / 2, 0, 0)),
        cylinder("InfoDot", (0, 1.335, 2.30), 0.055, 0.035, "FF_Cream", 10, (math.pi / 2, 0, 0)),
        box("InfoStem", (0, 1.34, 2.10), (0.10, 0.035, 0.27), "FF_Cream", 0.012),
        box("MapBoard", (1.73, 0.98, 0.80), (0.72, 0.15, 1.08), "FF_TealDark", 0.03, (math.radians(-8), 0, 0)),
        box("MapPaper", (1.73, 1.065, 0.84), (0.54, 0.025, 0.66), "FF_Cream", 0.006, (math.radians(-8), 0, 0)),
    ]
    p += planter("InfoPlanterL", -1.86, 1.10)
    p += stairs("InfoStep", 2.20, 1.18, 0.26, 2)
    return p


def build_sunset_main_stage() -> list[bpy.types.Object]:
    p: list[bpy.types.Object] = [
        box("StageDeck", (0, -0.06, 0.26), (5.62, 2.60, 0.52), "FF_WoodDark", 0.045),
        box("StageFloor", (0, 0.06, 0.55), (5.20, 2.18, 0.13), "FF_WoodWarm", 0.018),
        box("StageBackdrop", (0, -1.10, 1.43), (4.72, 0.12, 1.62), "FF_WoodWarm", 0.025),
        box("StageCurtain", (0, -1.02, 1.45), (4.22, 0.045, 1.40), "FF_TealDark", 0.012),
        awning("StageCanopy", (0, -0.05, 2.48), 5.70, 2.65, 0.22, "FF_Teal"),
    ]
    for x in (-2.62, 2.62):
        for y in (-1.17, 1.15):
            p.append(post(f"StageTruss_{x}_{y}", x, y, 0.50, 2.47, 0.11))
        p.append(box(f"StageSpeaker_{x}", (x * 0.83, 0.83, 1.05), (0.55, 0.42, 1.12), "FF_Metal", 0.04))
        p.append(box(f"StageSpeakerInset_{x}", (x * 0.83, 1.055, 1.12), (0.35, 0.025, 0.75), "FF_TealDark", 0.01))
    p += [
        box("StageTopTruss", (0, 0.95, 2.30), (5.34, 0.13, 0.13), "FF_Metal", 0.02),
        box("StageFrontSkirt", (0, 1.26, 0.31), (5.30, 0.05, 0.43), "FF_TealDark", 0.012),
    ]
    for x in (-1.80, -0.90, 0.0, 0.90, 1.80):
        p += lantern(f"StageLight_{x}", x, 1.00, 2.08, 0.56)
    for x in (-1.55, 0.0, 1.55):
        p.append(box(f"FootLight_{x}", (x, 1.02, 0.71), (0.34, 0.26, 0.18), "FF_AmberGlow", 0.04, (math.radians(15), 0, 0)))
    p += stairs("StageStairs", 1.70, 1.32, 0.52, 3)
    return p


def half_arch(name: str, radius: float, tube: float, y: float, z: float, material: str) -> bpy.types.Object:
    segments = 16
    sides = 6
    verts: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []
    for i in range(segments + 1):
        theta = math.pi * i / segments
        cx = radius * math.cos(theta)
        cz = z + radius * math.sin(theta)
        for side in range(sides):
            phi = 2 * math.pi * side / sides
            verts.append((cx + tube * math.cos(phi), y + tube * math.sin(phi), cz))
    for i in range(segments):
        for side in range(sides):
            a = i * sides + side
            b = i * sides + (side + 1) % sides
            c = (i + 1) * sides + (side + 1) % sides
            d = (i + 1) * sides + side
            faces.append((a, b, c, d))
    return mesh_from_vertices(name, verts, faces, material)


def build_sunset_landmark() -> list[bpy.types.Object]:
    p: list[bpy.types.Object] = [
        box("LandmarkDeck", (0, 0, 0.18), (5.0, 2.38, 0.36), "FF_WoodDark", 0.045),
        box("LandmarkFloor", (0, 0.18, 0.40), (4.45, 1.75, 0.12), "FF_WoodWarm", 0.02),
        cylinder("SunDisc", (0, -0.72, 1.33), 0.88, 0.13, "FF_SunsetGlow", 24, (math.pi / 2, 0, 0)),
        cylinder("SunRim", (0, -0.79, 1.33), 1.02, 0.10, "FF_WoodDark", 24, (math.pi / 2, 0, 0)),
        cylinder("SunDiscFront", (0, -0.70, 1.33), 0.84, 0.16, "FF_SunsetGlow", 24, (math.pi / 2, 0, 0)),
        half_arch("LandmarkArch", 2.10, 0.19, -0.30, 0.34, "FF_WoodWarm"),
        post("ArchPostL", -2.10, -0.30, 0.30, 1.04, 0.24),
        post("ArchPostR", 2.10, -0.30, 0.30, 1.04, 0.24),
    ]
    for side in (-1, 1):
        x0 = 1.06 * side
        p += [
            cylinder(f"WaveA_{side}", (x0, 0.02, 0.73), 0.48, 0.14, "FF_Teal", 16, (math.pi / 2, 0, 0)),
            cylinder(f"WaveCut_{side}", (x0 - 0.18 * side, 0.12, 0.82), 0.33, 0.16, "FF_TealDark", 16, (math.pi / 2, 0, 0)),
            cylinder(f"WaveB_{side}", (1.73 * side, 0.05, 0.55), 0.34, 0.14, "FF_Teal", 14, (math.pi / 2, 0, 0)),
        ]
        p += lantern(f"LandmarkLantern_{side}", 2.20 * side, 0.70, 0.95, 0.75)
    for x in (-1.42, -0.71, 0.0, 0.71, 1.42):
        p += lantern(f"ArchLight_{x}", x, -0.02, 2.16 - 0.23 * abs(x), 0.48)
    p += stairs("LandmarkStairs", 2.25, 1.20, 0.36, 2)
    return p


def build_main_rest_area() -> list[bpy.types.Object]:
    p: list[bpy.types.Object] = [
        box("RestDeck", (0, 0, 0.12), (5.38, 2.58, 0.24), "FF_WoodDark", 0.04),
        box("RestFloor", (0, 0, 0.27), (5.02, 2.30, 0.10), "FF_WoodWarm", 0.018),
        prism_gable("RestCanopy", 5.45, 2.62, 1.95, 2.28, "FF_Teal"),
    ]
    for x in (-2.42, 2.42):
        for y in (-1.02, 1.02):
            p.append(post(f"RestPost_{x}_{y}", x, y, 0.20, 2.03, 0.15))
        p += lantern(f"RestLantern_{x}", x, 1.11, 1.43, 0.76)
    for y in (-0.46, 0.47):
        p += [
            box(f"RestTable_{y}", (0, y, 0.73), (2.00, 0.62, 0.16), "FF_WoodWarm", 0.035),
            box(f"RestTableLegL_{y}", (-0.66, y, 0.48), (0.13, 0.13, 0.48), "FF_WoodDark", 0.015),
            box(f"RestTableLegR_{y}", (0.66, y, 0.48), (0.13, 0.13, 0.48), "FF_WoodDark", 0.015),
            box(f"RestBenchL_{y}", (-1.48, y, 0.48), (0.64, 0.46, 0.14), "FF_WoodWarm", 0.025),
            box(f"RestBenchR_{y}", (1.48, y, 0.48), (0.64, 0.46, 0.14), "FF_WoodWarm", 0.025),
        ]
    p += [
        cylinder("WaterBarrel", (2.06, -0.62, 0.60), 0.32, 0.82, "FF_BlueGrey", 14),
        cylinder("BarrelBand1", (2.06, -0.62, 0.40), 0.34, 0.07, "FF_Metal", 14),
        cylinder("BarrelBand2", (2.06, -0.62, 0.80), 0.34, 0.07, "FF_Metal", 14),
    ]
    p += planter("RestPlanterL", -2.03, 1.02, 0.25, 0.65)
    p += planter("RestPlanterR", 2.03, 1.02, 0.25, 0.65)
    return p


def build_medical_tent() -> list[bpy.types.Object]:
    p: list[bpy.types.Object] = [
        box("MedicalFloor", (0, 0, 0.10), (4.78, 2.62, 0.20), "FF_WoodDark", 0.035),
        prism_gable("MedicalTent", 4.86, 2.70, 1.52, 2.48, "FF_Cream"),
        prism_gable("MedicalTealCap", 4.72, 2.74, 1.58, 2.45, "FF_Teal"),
        box("MedicalFrontOpening", (0, 1.48, 1.02), (2.05, 0.06, 1.72), "FF_TealDark", 0.015),
        box("MedicalFlapL", (-1.23, 1.505, 1.05), (0.42, 0.055, 1.70), "FF_Cream", 0.012, (0, 0, math.radians(-7))),
        box("MedicalFlapR", (1.23, 1.505, 1.05), (0.42, 0.055, 1.70), "FF_Cream", 0.012, (0, 0, math.radians(7))),
        box("MedicalCrossPanel", (0, -1.385, 1.34), (1.05, 0.055, 1.05), "FF_Teal", 0.025),
    ]
    p += plus_symbol("MedicalCross", (0, -1.42, 1.34), 0.72, "FF_Cream", 0.06)
    p += plus_symbol("MedicalFrontCross", (0, 1.43, 2.02), 0.52, "FF_Cream", 0.06)
    for x in (-2.18, 2.18):
        p += [post(f"MedicalPole_{x}", x, 1.19, 0.18, 1.82, 0.11), *lantern(f"MedicalLantern_{x}", x, 1.28, 1.42, 0.63)]
    p += [
        box("StretcherBed", (1.22, 0.48, 0.64), (1.72, 0.62, 0.15), "FF_Cream", 0.035),
        box("StretcherFrame", (1.22, 0.48, 0.48), (1.88, 0.12, 0.12), "FF_Metal", 0.02),
        box("MedicalCase", (-1.54, 0.55, 0.48), (0.72, 0.45, 0.56), "FF_Medical", 0.045),
    ]
    p += plus_symbol("CaseCross", (-1.54, 0.785, 0.48), 0.32, "FF_Cream", 0.035)
    return p


def person_icon(name: str, x: float, y: float, z: float, material: str) -> list[bpy.types.Object]:
    return [
        sphere(f"{name}_Head", (x, y, z + 0.28), (0.12, 0.04, 0.12), material),
        box(f"{name}_Body", (x, y, z - 0.02), (0.22, 0.05, 0.42), material, 0.02),
        box(f"{name}_LegL", (x - 0.07, y, z - 0.31), (0.07, 0.05, 0.28), material, 0.01),
        box(f"{name}_LegR", (x + 0.07, y, z - 0.31), (0.07, 0.05, 0.28), material, 0.01),
    ]


def build_main_restroom() -> list[bpy.types.Object]:
    p: list[bpy.types.Object] = [
        box("RestroomDeck", (0, 0, 0.13), (5.38, 2.62, 0.26), "FF_WoodDark", 0.04),
        box("RestroomBody", (0, -0.12, 1.13), (4.86, 2.13, 1.82), "FF_WoodWarm", 0.05),
        prism_gable("RestroomRoof", 5.46, 2.74, 2.03, 2.45, "FF_Teal"),
        box("Divider", (0, 0.86, 1.12), (0.14, 0.10, 1.65), "FF_WoodDark", 0.02),
        box("DoorL", (-1.22, 0.985, 1.15), (1.52, 0.055, 1.70), "FF_TealDark", 0.015),
        box("DoorR", (1.22, 0.985, 1.15), (1.52, 0.055, 1.70), "FF_TealDark", 0.015),
        box("SinkBase", (0, 1.25, 0.62), (1.25, 0.34, 0.72), "FF_WoodDark", 0.035),
        box("SinkTop", (0, 1.30, 1.02), (1.42, 0.44, 0.14), "FF_Cream", 0.035),
        cylinder("SinkBowl", (0, 1.31, 1.10), 0.28, 0.08, "FF_BlueGrey", 14),
        cylinder("SinkTap", (0, 1.28, 1.29), 0.045, 0.36, "FF_Metal", 8),
    ]
    p += person_icon("RestroomIconL", -1.22, 1.025, 1.31, "FF_Cream")
    p += person_icon("RestroomIconR", 1.22, 1.025, 1.31, "FF_Cream")
    for x in (-2.30, 2.30):
        p += [post(f"RestroomPost_{x}", x, 1.05, 0.20, 2.05, 0.13), *lantern(f"RestroomLantern_{x}", x, 1.16, 1.47, 0.64)]
    p += planter("RestroomPlanterL", -2.18, 1.16, 0.25, 0.62)
    p += planter("RestroomPlanterR", 2.18, 1.16, 0.25, 0.62)
    return p


def resolve_builder(mesh_name: str):
    """Resolve art code by naming convention without maintaining a facility list."""
    prefix = "SM_FFZS_"
    if not mesh_name.startswith(prefix):
        return None
    suffix = mesh_name[len(prefix):]
    snake_name = re.sub(r"(?<!^)(?=[A-Z])", "_", suffix).lower()
    return globals().get("build_" + snake_name)


def canonical_digest(value: object) -> str:
    payload = json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def load_authoring_assets() -> tuple[str, list[tuple[str, object, tuple[float, float, float], str]]]:
    if not AUTHORING_MANIFEST.is_file():
        raise RuntimeError(
            "Missing generated manifest: "
            f"{AUTHORING_MANIFEST}. Run export_facility_authoring_manifest.py in Unreal first."
        )
    with AUTHORING_MANIFEST.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if manifest.get("schema_version") != 2 or manifest.get("authoritative") is not False:
        raise RuntimeError("Unsupported or incorrectly marked facility authoring manifest")
    entries = manifest.get("entries", [])
    manifest_digest = canonical_digest(entries)
    if manifest.get("projection_digest") != manifest_digest:
        raise RuntimeError("Facility authoring manifest projection digest mismatch")

    assets = []
    seen_mesh_names = set()
    for entry in entries:
        projection = {key: value for key, value in entry.items() if key != "projection_digest"}
        entry_digest = canonical_digest(projection)
        if entry.get("projection_digest") != entry_digest:
            raise RuntimeError(
                f"Facility authoring entry projection digest mismatch: {entry.get('definition', '<unknown>')}"
            )
        mesh_name = entry.get("visual_mesh_name", "")
        if mesh_name in seen_mesh_names:
            raise RuntimeError(f"Duplicate VisualMesh in authoring manifest: {mesh_name}")
        seen_mesh_names.add(mesh_name)
        builder = resolve_builder(mesh_name)
        if builder is None:
            raise RuntimeError(
                f"No procedural art builder registered for Definition VisualMesh {mesh_name}"
            )
        target = entry.get("target_size_cm", {})
        bounds = tuple(float(target.get(axis, 0.0)) / 100.0 for axis in "xyz")
        if any(not math.isfinite(value) or value <= 0.0 for value in bounds):
            raise RuntimeError(f"Invalid Definition target bounds for {mesh_name}: {target}")
        assets.append((mesh_name, builder, bounds, entry_digest))
    if not assets:
        raise RuntimeError("Generated facility authoring manifest contains no entries")
    return manifest_digest, assets


def add_collision(asset_name: str, bounds: tuple[float, float, float]) -> bpy.types.Object:
    width, depth, height = bounds
    collision = box(
        f"UCX_{asset_name}_00",
        (0.0, 0.0, height * 0.5),
        (width * 0.96, depth * 0.92, height),
        "FF_Metal",
        bevel=0.0,
    )
    collision.display_type = "WIRE"
    collision.hide_render = True
    return collision


def look_at(obj: bpy.types.Object, target: tuple[float, float, float]) -> None:
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def add_preview_environment(asset_name: str, bounds: tuple[float, float, float]) -> None:
    width, depth, height = bounds
    ground = box("PreviewGround", (0, 0, -0.07), (9.0, 7.0, 0.10), "FF_TealDark", 0.02)
    ground.hide_render = False
    for index in range(-8, 9):
        x = index * 0.5
        line = box(f"GridX{index}", (x, 0, -0.012), (0.008, 7.0, 0.012), "FF_Teal", 0)
        line.hide_render = False
    for index in range(-7, 8):
        y = index * 0.5
        line = box(f"GridY{index}", (0, y, -0.011), (9.0, 0.008, 0.012), "FF_Teal", 0)
        line.hide_render = False

    bpy.ops.object.light_add(type="AREA", location=(4.8, 5.8, 7.2))
    key = bpy.context.object
    key.name = "WarmKey"
    key.data.energy = 1450
    key.data.color = (1.0, 0.78, 0.58)
    key.data.shape = "DISK"
    key.data.size = 5.0
    look_at(key, (0, 0, 0.8))

    bpy.ops.object.light_add(type="AREA", location=(-5.2, 3.0, 4.2))
    fill = bpy.context.object
    fill.name = "TealFill"
    fill.data.energy = 850
    fill.data.color = (0.16, 0.52, 0.68)
    fill.data.size = 4.0
    look_at(fill, (0, 0, 0.9))

    bpy.ops.object.light_add(type="AREA", location=(0, -4.6, 5.0))
    rim = bpy.context.object
    rim.name = "RimLight"
    rim.data.energy = 650
    rim.data.color = (0.18, 0.44, 0.54)
    rim.data.size = 3.0
    look_at(rim, (0, 0, 1.0))

    bpy.ops.object.camera_add(location=(width * 1.16, depth * 2.25, height * 1.78))
    camera = bpy.context.object
    camera.name = f"Camera_{asset_name}"
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = max(width * 1.35, depth * 2.25)
    camera.data.lens = 52
    look_at(camera, (0, 0, height * 0.72))
    bpy.context.scene.camera = camera


def export_fbx(asset_name: str, visual: bpy.types.Object, collision: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    visual.select_set(True)
    collision.select_set(True)
    bpy.context.view_layer.objects.active = visual
    path = FBX_DIR / f"{asset_name}.fbx"
    bpy.ops.export_scene.fbx(
        filepath=str(path),
        use_selection=True,
        object_types={"MESH"},
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS",
        axis_forward="-Y",
        axis_up="Z",
        bake_anim=False,
        add_leaf_bones=False,
        path_mode="AUTO",
    )


def render_preview(asset_name: str, bounds: tuple[float, float, float]) -> None:
    add_preview_environment(asset_name, bounds)
    scene = bpy.context.scene
    scene.render.filepath = str(PREVIEW_DIR / f"{asset_name}.png")
    bpy.ops.render.render(write_still=True)


def validate_bounds(asset_name: str, obj: bpy.types.Object, contract: tuple[float, float, float]) -> None:
    actual = obj.dimensions
    tolerance = 0.025
    for actual_axis, contract_axis, axis in zip(actual, contract, "XYZ"):
        if actual_axis > contract_axis + tolerance:
            raise RuntimeError(
                f"{asset_name} exceeds {axis} contract: {actual_axis:.3f}m > {contract_axis:.3f}m"
            )
    min_z = min((obj.matrix_world @ Vector(corner)).z for corner in obj.bound_box)
    if abs(min_z) > 0.03:
        raise RuntimeError(f"{asset_name} pivot/ground contract failed: min Z is {min_z:.3f}m")


def material_distribution(obj: bpy.types.Object) -> dict[str, int]:
    counts = Counter(polygon.material_index for polygon in obj.data.polygons)
    distribution = {
        obj.data.materials[index].name: count
        for index, count in sorted(counts.items())
        if index < len(obj.data.materials)
    }
    if len(distribution) < 4:
        raise RuntimeError(f"{obj.name} material assignment collapsed: {distribution}")
    return distribution


def main() -> None:
    source_manifest_digest, assets = load_authoring_assets()
    built_entries = []
    for asset_name, builder, bounds, projection_digest in assets:
        reset_scene()
        configure_scene()
        parts = builder()
        visual = join_visual_parts(asset_name, parts)
        fit_and_center(visual, bounds)
        validate_bounds(asset_name, visual, bounds)
        distribution = material_distribution(visual)
        collision = add_collision(asset_name, bounds)
        export_fbx(asset_name, visual, collision)
        render_preview(asset_name, bounds)
        fbx_path = FBX_DIR / f"{asset_name}.fbx"
        fbx_digest = hashlib.sha256(fbx_path.read_bytes()).hexdigest()
        built_entries.append(
            {
                "visual_mesh_name": asset_name,
                "projection_digest": projection_digest,
                "fbx_sha256": fbx_digest,
            }
        )
        print(
            f"FF_FACILITY_BUILT {asset_name} "
            f"bounds={tuple(round(v, 3) for v in visual.dimensions)} "
            f"vertices={len(visual.data.vertices)} polygons={len(visual.data.polygons)} "
            f"material_polygons={distribution}"
        )

    build_manifest = {
        "schema_version": 1,
        "source_manifest_digest": source_manifest_digest,
        "entries": built_entries,
    }
    pending_path = BUILD_MANIFEST.with_suffix(BUILD_MANIFEST.suffix + ".pending")
    pending_path.write_text(
        json.dumps(build_manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    pending_path.replace(BUILD_MANIFEST)


if __name__ == "__main__":
    main()
