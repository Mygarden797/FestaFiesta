"""Build the production-reference 3D claw-machine facility with Blender 5.1+.

Run from the repository root::

    blender --background --factory-startup \
        --python Tools/facilities/build_claw_machine_test_asset.py

The FBX follows the FFZS contract: metres in Blender, centimetres in Unreal,
ground-centred pivot, local +Y facing the guest, shared FF_* material slots,
and one explicit UCX collision hull. The generated ImageGen catalog art is
imported separately by ``import_claw_machine_content.py``.
"""

from __future__ import annotations

import hashlib
import json
import math
import sys
from pathlib import Path

import bpy


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
    material_distribution,
    post,
    render_preview,
    reset_scene,
    sphere,
    validate_bounds,
)


ASSET_NAME = "SM_FFZS_Booth_ActivityGame_ClawMachine"
CONTRACT_METRES = (5.80, 4.60, 3.35)
REPORT_PATH = PREVIEW_DIR / f"{ASSET_NAME}.json"


def build_claw_machine_booth() -> list[bpy.types.Object]:
    parts: list[bpy.types.Object] = [
        box("DeckBase", (0.0, 0.0, 0.15), (5.35, 4.05, 0.30), "FF_WoodDark", 0.05),
        box("DeckTop", (0.0, 0.0, 0.35), (5.10, 3.80, 0.12), "FF_WoodWarm", 0.025),
        awning("FestivalCanopy", (0.0, -0.05, 3.16), 5.20, 3.82, 0.18, "FF_Teal"),
        box("MachineBase", (0.0, -0.12, 0.90), (3.25, 2.35, 1.42), "FF_TealDark", 0.14),
        box("MachineControlDeck", (0.0, 1.18, 1.38), (3.25, 0.42, 0.30), "FF_SunsetGlow", 0.08),
        box("MachineTop", (0.0, -0.12, 2.78), (3.25, 2.35, 0.42), "FF_SunsetGlow", 0.15),
        box("ChamberBack", (0.0, -1.21, 2.13), (2.82, 0.10, 1.12), "FF_BlueGrey", 0.02),
        box("ChamberFloor", (0.0, -0.12, 1.50), (2.82, 2.05, 0.12), "FF_Cream", 0.02),
        box("PrizeChute", (0.0, 1.35, 0.72), (1.15, 0.12, 0.70), "FF_Metal", 0.06),
        cylinder("ActionButton", (0.62, 1.43, 1.62), 0.16, 0.12, "FF_AmberGlow", 16),
        cylinder("JoystickStem", (-0.58, 1.42, 1.72), 0.06, 0.40, "FF_Metal", 10),
        sphere("JoystickBall", (-0.58, 1.42, 1.94), (0.15, 0.15, 0.15), "FF_SunsetGlow"),
    ]

    # Four readable frame posts leave the chamber visually open without alpha materials.
    for index, (x, y) in enumerate(
        ((-1.48, -1.18), (1.48, -1.18), (-1.48, 1.03), (1.48, 1.03))
    ):
        parts.append(box(f"MachineFrame_{index}", (x, y, 2.12), (0.14, 0.14, 1.45), "FF_Teal", 0.025))

    # Overhead carriage, cable, hub and three prongs communicate the facility function at distance.
    parts.extend(
        [
            box("ClawRail", (0.0, -0.12, 2.82), (2.35, 0.14, 0.14), "FF_Metal", 0.02),
            box("ClawCarriage", (0.12, -0.12, 2.69), (0.42, 0.40, 0.20), "FF_TealDark", 0.04),
            cylinder("ClawCable", (0.12, -0.12, 2.33), 0.025, 0.62, "FF_Metal", 8),
            cylinder("ClawHub", (0.12, -0.12, 1.98), 0.15, 0.22, "FF_Metal", 12),
        ]
    )
    for tine_index in range(3):
        angle = 2.0 * math.pi * tine_index / 3.0
        tine = cylinder(
            f"ClawTine_{tine_index}",
            (0.12 + math.cos(angle) * 0.18, -0.12 + math.sin(angle) * 0.18, 1.80),
            0.028,
            0.38,
            "FF_Metal",
            8,
        )
        tine.rotation_euler = (math.sin(angle) * 0.38, math.cos(angle) * 0.38, angle)
        parts.append(tine)

    # Low-poly sea-friend plushes; no external character IP or bitmap stand-ins.
    plush_specs = (
        (-0.92, -0.65, 1.68, "FF_Cream", (0.31, 0.24, 0.22)),
        (-0.30, -0.74, 1.68, "FF_SunsetGlow", (0.27, 0.27, 0.26)),
        (0.34, -0.62, 1.68, "FF_BlueGrey", (0.34, 0.23, 0.21)),
        (0.88, -0.72, 1.68, "FF_Cream", (0.29, 0.24, 0.23)),
        (-0.65, 0.02, 1.68, "FF_BlueGrey", (0.34, 0.23, 0.21)),
        (0.05, 0.04, 1.68, "FF_SunsetGlow", (0.27, 0.27, 0.26)),
        (0.72, 0.02, 1.68, "FF_Cream", (0.30, 0.24, 0.22)),
    )
    for index, (x, y, z, material, scale) in enumerate(plush_specs):
        parts.append(sphere(f"PlushBody_{index}", (x, y, z), scale, material))
        parts.append(
            sphere(
                f"PlushHead_{index}",
                (x, y + 0.13, z + 0.20),
                (scale[0] * 0.58, scale[1] * 0.58, scale[2] * 0.58),
                material,
            )
        )

    # Booth posts and warm lanterns tie the machine to the existing seaside facility family.
    for index, (x, y) in enumerate(((-2.35, -1.72), (2.35, -1.72), (-2.35, 1.72), (2.35, 1.72))):
        parts.append(post(f"CanopyPost_{index}", x, y, 0.30, 3.15, 0.12))
    parts.extend(
        [
            sphere("LanternLeft", (-2.34, 1.55, 2.48), (0.22, 0.22, 0.28), "FF_AmberGlow"),
            sphere("LanternRight", (2.34, 1.55, 2.48), (0.22, 0.22, 0.28), "FF_AmberGlow"),
        ]
    )
    return parts


def main() -> None:
    reset_scene()
    configure_scene()
    visual = join_visual_parts(ASSET_NAME, build_claw_machine_booth())
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
        "facility_definition_id": "Booth_ActivityGame_ClawMachine",
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
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("FF_CLAW_MACHINE_FACILITY " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
