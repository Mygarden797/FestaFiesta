"""Build the standalone surfing simulator reference asset with Blender.

Run from the repository root:
    blender --background --factory-startup --python Tools/facilities/build_surfing_test_asset.py

This asset deliberately stays outside the authoritative facility-definition
manifest.  It is a reusable Test_Asset reference: metres in Blender, Unreal
centimetres after FBX import, ground-centred pivot, local +Y facing the guest,
shared FF_* material slots, and one explicit UCX collision hull.
"""

from __future__ import annotations

import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from build_facility_meshes import (  # noqa: E402
    FBX_DIR,
    PREVIEW_DIR,
    add_collision,
    awning,
    box,
    configure_scene,
    cylinder,
    export_fbx,
    fit_and_center,
    join_visual_parts,
    lantern,
    material_distribution,
    mesh_from_vertices,
    post,
    render_preview,
    reset_scene,
    sphere,
    stairs,
    validate_bounds,
)


ASSET_NAME = "SM_FFZS_Booth_ActivityGame_Surfing"
CONTRACT_METRES = (5.80, 4.60, 3.35)
REPORT_PATH = PREVIEW_DIR / f"{ASSET_NAME}.json"


def beam_between(
    name: str,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    radius: float,
    material: str,
) -> bpy.types.Object:
    """Create a low-sided cylindrical rail between two arbitrary points."""
    start_vector = Vector(start)
    end_vector = Vector(end)
    direction = end_vector - start_vector
    if direction.length <= 0.0001:
        raise ValueError(f"{name} rail endpoints overlap")
    rail = cylinder(
        name,
        tuple((start_vector + end_vector) * 0.5),
        radius,
        direction.length,
        material,
        vertices=8,
    )
    rail.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    return rail


def extruded_wave_panel(
    name: str,
    y: float,
    thickness: float,
    material: str,
) -> bpy.types.Object:
    """Create a simple readable wave silhouette without fragile modifiers."""
    outline = [
        (-2.08, 0.42),
        (-1.98, 1.22),
        (-1.66, 1.86),
        (-1.15, 2.32),
        (-0.52, 2.57),
        (0.18, 2.62),
        (0.82, 2.44),
        (1.36, 2.10),
        (1.78, 1.63),
        (2.05, 1.10),
        (2.08, 0.42),
    ]
    half = thickness * 0.5
    vertices = [(x, y - half, z) for x, z in outline]
    vertices += [(x, y + half, z) for x, z in outline]
    count = len(outline)
    faces: list[tuple[int, ...]] = [
        tuple(range(count - 1, -1, -1)),
        tuple(range(count, count * 2)),
    ]
    for index in range(count):
        following = (index + 1) % count
        faces.append(
            (index, following, count + following, count + index)
        )
    return mesh_from_vertices(name, vertices, faces, material)


def build_surfing_simulator() -> list[bpy.types.Object]:
    parts: list[bpy.types.Object] = [
        box(
            "SurfDeckBase",
            (0.0, 0.0, 0.16),
            (5.30, 3.20, 0.32),
            "FF_WoodDark",
            0.045,
        ),
        box(
            "SurfDeckFloor",
            (0.0, 0.02, 0.37),
            (4.98, 2.92, 0.12),
            "FF_WoodWarm",
            0.020,
        ),
        extruded_wave_panel("SurfWave", -1.08, 0.23, "FF_Teal"),
        cylinder(
            "SimulatorBase",
            (0.0, 0.22, 0.53),
            0.72,
            0.20,
            "FF_TealDark",
            16,
        ),
        cylinder(
            "SimulatorTurntable",
            (0.0, 0.22, 0.67),
            0.49,
            0.12,
            "FF_Metal",
            14,
        ),
        cylinder(
            "SimulatorPivot",
            (0.0, 0.22, 0.84),
            0.14,
            0.34,
            "FF_Metal",
            10,
        ),
        awning(
            "SurfCanopy",
            (0.0, -0.08, 3.15),
            5.02,
            2.84,
            0.18,
            "FF_Teal",
        ),
    ]

    # A low-poly elongated board keeps the silhouette light and readable.
    board = sphere(
        "SimulatorSurfboard",
        (0.0, 0.25, 1.02),
        (1.30, 0.35, 0.095),
        "FF_Cream",
    )
    board.rotation_euler = (0.0, math.radians(-7.0), math.radians(-8.0))
    parts.append(board)
    parts.append(
        box(
            "SurfboardStripe",
            (0.0, 0.25, 1.105),
            (1.62, 0.065, 0.026),
            "FF_Teal",
            0.008,
            (0.0, math.radians(-7.0), math.radians(-8.0)),
        )
    )

    # Layered arcs and foam break up the wave wall while keeping polygon cost low.
    for radius, tube, y, z, material in (
        (1.34, 0.20, -0.93, 0.48, "FF_TealDark"),
        (1.02, 0.15, -0.79, 0.49, "FF_BlueGrey"),
    ):
        segments = 14
        sides = 6
        vertices: list[tuple[float, float, float]] = []
        faces: list[tuple[int, ...]] = []
        for arc_index in range(segments + 1):
            theta = math.pi * arc_index / segments
            centre_x = radius * math.cos(theta)
            centre_z = z + radius * math.sin(theta)
            for side in range(sides):
                phi = 2.0 * math.pi * side / sides
                vertices.append(
                    (
                        centre_x + tube * math.cos(phi),
                        y + tube * math.sin(phi),
                        centre_z,
                    )
                )
        for arc_index in range(segments):
            for side in range(sides):
                a = arc_index * sides + side
                b = arc_index * sides + (side + 1) % sides
                c = (arc_index + 1) * sides + (side + 1) % sides
                d = (arc_index + 1) * sides + side
                faces.append((a, b, c, d))
        parts.append(
            mesh_from_vertices(
                f"WaveBand_{material}_{radius}", vertices, faces, material
            )
        )

    foam_positions = (
        (-1.28, -0.91, 2.23, (0.29, 0.18, 0.18)),
        (-0.82, -0.89, 2.51, (0.30, 0.19, 0.20)),
        (-0.30, -0.88, 2.66, (0.28, 0.18, 0.17)),
        (0.22, -0.87, 2.64, (0.27, 0.17, 0.17)),
        (0.72, -0.86, 2.48, (0.25, 0.16, 0.16)),
        (1.18, -0.85, 2.22, (0.23, 0.15, 0.15)),
    )
    for index, (x, y, z, scale) in enumerate(foam_positions):
        parts.append(sphere(f"WaveFoam_{index}", (x, y, z), scale, "FF_Cream"))

    canopy_posts = (
        (-2.32, -1.24),
        (2.32, -1.24),
        (-2.32, 1.22),
        (2.32, 1.22),
    )
    for index, (x, y) in enumerate(canopy_posts):
        parts.append(post(f"CanopyPost_{index}", x, y, 0.30, 3.14, 0.14))

    rail_posts = (
        (-2.40, -0.12),
        (-2.40, 1.18),
        (2.40, -0.12),
        (2.40, 1.18),
        (-1.25, 1.30),
        (1.25, 1.30),
    )
    for index, (x, y) in enumerate(rail_posts):
        parts.append(post(f"SafetyPost_{index}", x, y, 0.32, 1.35, 0.115))

    rail_segments = (
        ((-2.40, -1.20), (-2.40, 1.18)),
        ((2.40, -1.20), (2.40, 1.18)),
        ((-2.25, -1.30), (2.25, -1.30)),
        ((-2.40, 1.30), (-1.25, 1.30)),
        ((1.25, 1.30), (2.40, 1.30)),
    )
    for segment_index, (start_xy, end_xy) in enumerate(rail_segments):
        for height_index, height in enumerate((0.91, 1.23)):
            parts.append(
                beam_between(
                    f"SafetyRail_{segment_index}_{height_index}",
                    (start_xy[0], start_xy[1], height),
                    (end_xy[0], end_xy[1], height),
                    0.035,
                    "FF_WoodWarm",
                )
            )

    for side, x in (("L", -2.32), ("R", 2.32)):
        parts += lantern(f"SurfLantern_{side}", x, 1.33, 2.30, 0.72)
        parts.append(
            beam_between(
                f"LanternHanger_{side}",
                (x, 1.21, 2.83),
                (x, 1.33, 2.48),
                0.025,
                "FF_Metal",
            )
        )

    parts += stairs("SurfStairs", 1.70, 1.48, 0.34, 3)
    return parts


def main() -> None:
    reset_scene()
    configure_scene()
    parts = build_surfing_simulator()
    visual = join_visual_parts(ASSET_NAME, parts)
    fit_and_center(visual, CONTRACT_METRES)
    validate_bounds(ASSET_NAME, visual, CONTRACT_METRES)
    distribution = material_distribution(visual)
    collision = add_collision(ASSET_NAME, CONTRACT_METRES)
    export_fbx(ASSET_NAME, visual, collision)
    render_preview(ASSET_NAME, CONTRACT_METRES)

    fbx_path = FBX_DIR / f"{ASSET_NAME}.fbx"
    report = {
        "schema_version": 1,
        "asset": ASSET_NAME,
        "reference_only": True,
        "front_axis": "+Y",
        "pivot": "ground_center",
        "contract_metres": CONTRACT_METRES,
        "visual_bounds_metres": [round(value, 4) for value in visual.dimensions],
        "vertices": len(visual.data.vertices),
        "polygons": len(visual.data.polygons),
        "materials": distribution,
        "fbx": str(fbx_path),
        "fbx_sha256": hashlib.sha256(fbx_path.read_bytes()).hexdigest(),
        "preview": str(PREVIEW_DIR / f"{ASSET_NAME}.png"),
    }
    REPORT_PATH.write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print("FF_SURFING_TEST_ASSET " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
