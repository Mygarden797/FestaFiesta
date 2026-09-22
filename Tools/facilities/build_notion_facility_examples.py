# build_notion_facility_examples.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 원본 모델이나 결과물을 생성한다.

"""Build missing FacilityDefinitionId example meshes and catalog thumbnails.

Run from the repository root with Blender 5.1 or newer::

    blender --background --factory-startup \
        --python Tools/facilities/build_notion_facility_examples.py

The authoritative example list is ``notion_facility_examples.json``.  Outputs
are deliberately non-destructive: an existing FBX or PNG is never replaced.
The seven Notion subcategories already represented by the project are
recognized through explicit legacy aliases and skipped.
"""

from __future__ import annotations

import importlib.util
import json
import math
from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parents[2]
BASE_SCRIPT = ROOT / "Tools" / "facilities" / "build_facility_meshes.py"
MANIFEST_PATH = ROOT / "Tools" / "facilities" / "notion_facility_examples.json"
FBX_DIR = ROOT / "SourceAssets" / "Zone" / "FacilityMeshes"
PNG_DIR = ROOT / "Content" / "FestaFiesta" / "Zone" / "Test_Asset" / "image"
REPORT_PATH = ROOT / "Saved" / "NotionFacilityBuildReport.json"

FBX_DIR.mkdir(parents=True, exist_ok=True)
PNG_DIR.mkdir(parents=True, exist_ok=True)


def load_base_module():
    spec = importlib.util.spec_from_file_location("ff_facility_base", BASE_SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError("Could not load base facility builder: " + str(BASE_SCRIPT))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


b = load_base_module()


# The existing seven assets already represent the first functional subcategory
# in each Notion table, although six predate the example's current unique name.
# Treat those category/subcategory rows as complete so this workflow fulfills
# the user's "missing only" constraint instead of creating duplicates.
MANIFEST = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
LEGACY_ASSET_ALIASES = MANIFEST["existing_representatives"]


def add_cone(
    name: str,
    location: tuple[float, float, float],
    radius1: float,
    radius2: float,
    depth: float,
    material: str,
    vertices: int = 12,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
):
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices,
        radius1=radius1,
        radius2=radius2,
        depth=depth,
        location=location,
        rotation=rotation,
    )
    obj = bpy.context.object
    obj.name = name
    return b.assign_material(obj, material)


def add_ring(
    name: str,
    location: tuple[float, float, float],
    major_radius: float,
    minor_radius: float,
    material: str,
    rotation: tuple[float, float, float] = (math.pi / 2, 0.0, 0.0),
):
    bpy.ops.mesh.primitive_torus_add(
        major_radius=major_radius,
        minor_radius=minor_radius,
        major_segments=16,
        minor_segments=6,
        location=location,
        rotation=rotation,
    )
    obj = bpy.context.object
    obj.name = name
    return b.assign_material(obj, material)


def add_wheelchair_icon(name: str, x: float, y: float, z: float):
    return [
        add_ring(f"{name}_Wheel", (x, y, z), 0.27, 0.055, "FF_Cream"),
        b.sphere(f"{name}_Head", (x - 0.05, y, z + 0.47), (0.10, 0.04, 0.10), "FF_Cream"),
        b.box(f"{name}_Body", (x - 0.03, y, z + 0.25), (0.11, 0.05, 0.30), "FF_Cream", 0.012),
        b.box(f"{name}_Seat", (x + 0.13, y, z + 0.13), (0.34, 0.05, 0.10), "FF_Cream", 0.012),
    ]


def add_bench(name: str, x: float, y: float, width: float = 1.55):
    parts = [
        b.box(f"{name}_Seat", (x, y, 0.56), (width, 0.45, 0.14), "FF_WoodWarm", 0.025),
        b.box(f"{name}_Back", (x, y - 0.18, 0.89), (width, 0.10, 0.58), "FF_Teal", 0.025, (math.radians(-8), 0, 0)),
    ]
    for dx in (-width * 0.36, width * 0.36):
        parts.append(b.box(f"{name}_Leg_{dx}", (x + dx, y, 0.29), (0.12, 0.12, 0.48), "FF_WoodDark", 0.015))
    return parts


def add_table(name: str, x: float, y: float, width: float = 1.55, depth: float = 0.72):
    parts = [b.box(f"{name}_Top", (x, y, 0.78), (width, depth, 0.14), "FF_WoodWarm", 0.03)]
    for dx in (-width * 0.34, width * 0.34):
        parts.append(b.box(f"{name}_Leg_{dx}", (x + dx, y, 0.47), (0.13, 0.13, 0.58), "FF_WoodDark", 0.015))
    return parts


def add_kiosk_shell(name: str, width: float = 4.8, depth: float = 2.35, roof_material: str = "FF_Teal"):
    parts = [
        b.box(f"{name}_Deck", (0, 0, 0.12), (width + 0.35, depth + 0.20, 0.24), "FF_WoodDark", 0.04),
        b.box(f"{name}_Back", (0, -depth * 0.38, 1.18), (width * 0.92, 0.18, 1.78), "FF_WoodWarm", 0.035),
        b.box(f"{name}_Counter", (0, depth * 0.40, 0.91), (width * 0.78, 0.48, 0.20), "FF_WoodWarm", 0.035),
        b.awning(f"{name}_Canopy", (0, 0.05, 2.33), width + 0.28, depth + 0.18, 0.20, roof_material),
    ]
    for x in (-width * 0.45, width * 0.45):
        parts.append(b.post(f"{name}_Post_{x}", x, depth * 0.42, 0.18, 2.33, 0.13))
    return parts


def add_restroom_shell(name: str, width: float = 4.8, temporary: bool = False):
    body_material = "FF_BlueGrey" if temporary else "FF_WoodWarm"
    parts = [
        b.box(f"{name}_Deck", (0, 0, 0.12), (width + 0.32, 2.56, 0.24), "FF_WoodDark", 0.035),
        b.box(f"{name}_Body", (0, -0.18, 1.13), (width, 2.02, 1.86), body_material, 0.045),
        b.prism_gable(f"{name}_Roof", width + 0.34, 2.55, 2.02, 2.43, "FF_Teal"),
    ]
    return parts


def build_booth(entry):
    sub = entry["subcategory"]
    p = add_kiosk_shell(sub)

    if sub == "FoodStall":
        p += [
            b.box("GrillBody", (0, 0.73, 1.08), (2.10, 0.70, 0.34), "FF_Metal", 0.04),
            b.box("GrillGlow", (0, 1.10, 1.12), (1.82, 0.04, 0.18), "FF_SunsetGlow", 0.01),
            b.cylinder("SeafoodSign", (0, -0.86, 1.76), 0.47, 0.10, "FF_BlueGrey", 18, (math.pi / 2, 0, 0)),
        ]
        for x in (-0.72, -0.24, 0.24, 0.72):
            p.append(b.cylinder(f"Skewer_{x}", (x, 1.12, 1.28), 0.055, 0.50, "FF_Cream", 8, (0, math.pi / 2, 0)))
        for x in (-1.72, 1.72):
            p += b.lantern(f"FoodLantern_{x}", x, 1.23, 1.65, 0.68)

    elif sub == "MarketGoods":
        for x in (-1.45, -0.48, 0.48, 1.45):
            p += [
                b.box(f"Shelf_{x}", (x, -0.48, 1.18), (0.72, 0.35, 1.20), "FF_TealDark", 0.025),
                b.box(f"GoodsTop_{x}", (x, -0.22, 1.50), (0.46, 0.20, 0.38), "FF_Cream", 0.035),
                b.box(f"GoodsLow_{x}", (x, -0.22, 0.91), (0.52, 0.20, 0.33), "FF_AmberGlow", 0.035),
            ]
        p += [
            b.box("BannerBar", (0, -0.94, 1.98), (3.65, 0.12, 0.13), "FF_Metal", 0.02),
            b.box("Banner", (0, -0.86, 1.72), (3.15, 0.055, 0.48), "FF_Teal", 0.025),
        ]

    elif sub == "CraftExperience":
        p += add_table("SandTable", 0, 0.52, 2.25, 0.82)
        p += [
            b.box("SandTray", (0, 0.52, 0.91), (1.92, 0.60, 0.14), "FF_Cream", 0.025),
            add_cone("SandCastle", (0, 0.53, 1.13), 0.32, 0.15, 0.44, "FF_AmberGlow", 8),
        ]
        for index, x in enumerate((-1.45, -0.86, 0.86, 1.45)):
            material = "FF_SunsetGlow" if index % 2 else "FF_BlueGrey"
            p.append(b.cylinder(f"SandJar_{index}", (x, 0.87, 1.10), 0.17, 0.45, material, 12))

    elif sub == "ActivityGame":
        p += [
            b.box("PegBoard", (0, -0.72, 1.32), (3.55, 0.18, 1.35), "FF_TealDark", 0.035),
            b.box("PrizeShelf", (0, 0.72, 0.78), (3.70, 0.42, 0.16), "FF_WoodWarm", 0.025),
        ]
        for index, x in enumerate((-1.20, -0.60, 0.0, 0.60, 1.20)):
            p.append(b.cylinder(f"Peg_{index}", (x, -0.50, 1.16), 0.07, 0.52, "FF_Cream", 10))
            p.append(add_ring(f"Ring_{index}", (x, -0.35, 1.50 + 0.08 * (index % 2)), 0.20, 0.045, "FF_AmberGlow"))
        for x in (-1.45, 0.0, 1.45):
            p.append(b.sphere(f"Prize_{x}", (x, 0.70, 1.02), (0.23, 0.18, 0.23), "FF_Green"))

    elif sub == "Nightlife":
        p += [
            b.box("BarFace", (0, 0.80, 1.02), (3.70, 0.34, 0.74), "FF_TealDark", 0.04),
            b.box("BarGlow", (0, 0.99, 1.07), (3.25, 0.035, 0.38), "FF_SunsetGlow", 0.012),
            b.box("DJConsole", (0, -0.22, 1.28), (1.80, 0.62, 0.28), "FF_Metal", 0.035),
        ]
        for x in (-1.90, 1.90):
            p += [
                b.box(f"Speaker_{x}", (x, -0.38, 1.25), (0.55, 0.48, 1.20), "FF_Metal", 0.04),
                b.cylinder(f"SpeakerCone_{x}", (x, -0.10, 1.28), 0.19, 0.04, "FF_AmberGlow", 16, (math.pi / 2, 0, 0)),
            ]
        for x in (-1.20, 0.0, 1.20):
            p += b.lantern(f"EDMLight_{x}", x, 0.48, 2.08, 0.56)

    return p


def build_toilet(entry):
    sub = entry["subcategory"]
    if sub == "Standard":
        return b.build_restroom()

    if sub == "Temporary":
        p = [b.box("TempDeck", (0, 0, 0.10), (5.20, 2.58, 0.20), "FF_WoodDark", 0.035)]
        for index, x in enumerate((-1.55, 0.0, 1.55)):
            p += [
                b.box(f"Cabin_{index}", (x, -0.10, 1.10), (1.30, 1.92, 2.05), "FF_BlueGrey", 0.08),
                b.box(f"Door_{index}", (x, 0.89, 1.08), (0.82, 0.055, 1.60), "FF_TealDark", 0.018),
                b.box(f"Vent_{index}", (x, 0.93, 1.78), (0.46, 0.035, 0.24), "FF_Cream", 0.015),
                add_cone(f"Roof_{index}", (x, -0.10, 2.22), 0.86, 0.62, 0.28, "FF_Teal", 8),
            ]
        p += [
            b.cylinder("WaterTank", (2.25, -0.72, 0.68), 0.38, 1.10, "FF_Metal", 14),
            b.box("CampSign", (-2.25, 0.72, 1.17), (0.64, 0.10, 0.82), "FF_AmberGlow", 0.04),
        ]
        return p

    p = add_restroom_shell(sub, 4.85)
    if sub == "Accessible":
        p += [
            b.box("WideDoor", (0, 0.86, 1.12), (2.15, 0.06, 1.78), "FF_TealDark", 0.018),
            b.box("Ramp", (0, 1.22, 0.22), (2.55, 0.92, 0.18), "FF_BlueGrey", 0.015, (math.radians(-5), 0, 0)),
        ]
        p += add_wheelchair_icon("AccessibleSign", 0, 0.91, 1.17)
        for x in (-1.22, 1.22):
            p.append(b.box(f"Rail_{x}", (x, 1.27, 0.73), (0.07, 0.88, 0.95), "FF_Metal", 0.01))
    elif sub == "Family":
        p += [
            b.box("FamilyDoor", (0, 0.86, 1.10), (2.55, 0.06, 1.82), "FF_TealDark", 0.018),
            b.box("ChangingTable", (1.42, 1.12, 0.83), (1.25, 0.48, 0.16), "FF_Cream", 0.025),
            b.box("StrollerBay", (-1.48, 1.08, 0.53), (1.10, 0.58, 0.84), "FF_BlueGrey", 0.035),
        ]
        p += b.person_icon("ParentIcon", -0.30, 0.91, 1.42, "FF_Cream")
        p += b.person_icon("ChildIcon", 0.38, 0.91, 1.18, "FF_AmberGlow")
    return p


def build_info(entry):
    sub = entry["subcategory"]
    if sub == "GeneralInfo":
        return b.build_information()

    p = add_kiosk_shell(sub, 4.95, 2.30, "FF_Teal")
    if sub == "Ticketing":
        for x in (-1.20, 0.0, 1.20):
            p += [
                b.box(f"TicketWindow_{x}", (x, -0.68, 1.42), (0.78, 0.05, 0.58), "FF_Glass", 0.02),
                b.box(f"TicketSlot_{x}", (x, 0.73, 1.02), (0.54, 0.26, 0.09), "FF_Metal", 0.012),
            ]
        for x in (-1.65, 0.0, 1.65):
            p += [b.cylinder(f"QueuePost_{x}", (x, 1.18, 0.55), 0.07, 1.05, "FF_Metal", 10), b.box(f"QueueBand_{x}", (x + 0.55, 1.18, 0.91), (1.10, 0.04, 0.07), "FF_SunsetGlow", 0.01)]
    elif sub == "LostFound":
        p += [
            b.box("OfficeWindow", (0, -0.72, 1.45), (2.65, 0.05, 0.72), "FF_Glass", 0.018),
            b.box("FoundShelf", (-1.45, 0.46, 1.21), (0.95, 0.45, 1.35), "FF_TealDark", 0.03),
            b.box("ClaimDesk", (0.52, 0.52, 0.82), (2.20, 0.72, 0.15), "FF_Cream", 0.025),
            add_ring("SearchSign", (0, -0.82, 2.03), 0.34, 0.07, "FF_AmberGlow"),
            b.box("SearchHandle", (0.27, -0.82, 1.77), (0.10, 0.06, 0.46), "FF_AmberGlow", 0.018, (0, 0, math.radians(-42))),
        ]
        for index, (x, z) in enumerate(((-1.45, 0.78), (-1.45, 1.22), (-1.45, 1.66))):
            p.append(b.box(f"FoundBox_{index}", (x, 0.73, z), (0.64, 0.26, 0.28), "FF_WoodWarm", 0.02))
    elif sub == "Translation":
        p += [
            b.box("LanguageDesk", (0, 0.48, 0.88), (3.15, 0.72, 0.20), "FF_Cream", 0.035),
            add_ring("GlobeRing", (0, -0.78, 1.70), 0.52, 0.065, "FF_AmberGlow"),
            add_ring("GlobeMeridian", (0, -0.78, 1.70), 0.39, 0.045, "FF_Teal", (0, 0, 0)),
            b.sphere("GlobeCore", (0, -0.80, 1.70), (0.30, 0.10, 0.30), "FF_BlueGrey"),
        ]
        for x in (-1.42, 1.42):
            p += [
                b.box(f"Speech_{x}", (x, -0.76, 1.54), (0.90, 0.07, 0.52), "FF_TealDark", 0.08),
                add_cone(f"SpeechTail_{x}", (x + 0.28, -0.76, 1.22), 0.20, 0.0, 0.38, "FF_TealDark", 3, (math.pi / 2, 0, 0)),
            ]
    return p


def build_rest(entry):
    sub = entry["subcategory"]
    if sub == "Seating":
        return b.build_rest_area()

    p = [b.box(f"{sub}Deck", (0, 0, 0.12), (5.38, 2.62, 0.24), "FF_WoodDark", 0.04)]
    if sub == "Shade":
        p += [b.awning("RiverCanopy", (0, -0.10, 2.20), 5.20, 2.45, 0.38, "FF_Teal")]
        for x in (-2.30, 2.30):
            for y in (-0.95, 0.95):
                p.append(b.post(f"ShadePost_{x}_{y}", x, y, 0.18, 2.17, 0.14))
        p += add_bench("RiverBenchL", -1.25, 0.25, 1.75)
        p += add_bench("RiverBenchR", 1.25, 0.25, 1.75)
        for x in (-2.15, 2.15):
            p += b.planter(f"RiverPlant_{x}", x, 1.00, 0.24, 0.65)
    elif sub == "Picnic":
        for x in (-1.35, 1.35):
            p += add_table(f"PicnicTable_{x}", x, 0.0, 1.68, 0.70)
            p += add_bench(f"PicnicBench_{x}", x, 0.83, 1.55)
        p += [
            b.cylinder("UmbrellaPole", (0, -0.35, 1.38), 0.07, 2.35, "FF_Metal", 10),
            add_cone("Umbrella", (0, -0.35, 2.20), 2.20, 0.18, 0.58, "FF_Teal", 16),
            b.box("PicnicBasket", (0, 0.88, 0.50), (0.72, 0.46, 0.58), "FF_AmberGlow", 0.08),
        ]
    elif sub == "Cooling":
        p += [
            b.box("CoolingShell", (0, -0.12, 1.20), (4.80, 2.10, 2.12), "FF_BlueGrey", 0.06),
            b.box("CoolingOpening", (0, 1.00, 1.20), (2.85, 0.06, 1.76), "FF_Glass", 0.02),
            b.prism_gable("CoolingRoof", 5.15, 2.55, 2.10, 2.44, "FF_Teal"),
            b.cylinder("WaterDispenser", (1.75, 0.90, 0.72), 0.24, 1.10, "FF_Cream", 14),
            b.cylinder("WaterGlow", (1.75, 0.90, 1.22), 0.18, 0.42, "FF_BlueGrey", 14),
        ]
        for x in (-1.10, 0.0, 1.10):
            p += [
                b.cylinder(f"FanFrame_{x}", (x, 1.07, 1.58), 0.34, 0.07, "FF_Metal", 18, (math.pi / 2, 0, 0)),
                b.box(f"FanBladeH_{x}", (x, 1.12, 1.58), (0.48, 0.04, 0.10), "FF_AmberGlow", 0.03, (0, 0, math.radians(20))),
                b.box(f"FanBladeV_{x}", (x, 1.12, 1.58), (0.10, 0.04, 0.48), "FF_AmberGlow", 0.03, (0, 0, math.radians(20))),
            ]
    return p


def build_performance(entry):
    sub = entry["subcategory"]
    if sub == "MainStage":
        return b.build_stage()

    if sub == "SubStage":
        p = [
            b.box("SubDeck", (0, 0, 0.22), (5.30, 2.48, 0.44), "FF_WoodDark", 0.04),
            b.box("SubFloor", (0, 0.04, 0.50), (4.90, 2.02, 0.12), "FF_WoodWarm", 0.02),
            b.prism_gable("SubRoof", 5.25, 2.52, 2.08, 2.43, "FF_Teal"),
            b.box("ArtistBackdrop", (0, -1.00, 1.38), (4.20, 0.10, 1.45), "FF_TealDark", 0.02),
        ]
        for x in (-2.25, 2.25):
            p += [b.post(f"SubPost_{x}", x, 0.92, 0.44, 2.18, 0.12), b.box(f"SubSpeaker_{x}", (x, 0.78, 0.93), (0.52, 0.42, 0.90), "FF_Metal", 0.035)]
        p += [b.cylinder("Drum", (0, -0.10, 0.88), 0.46, 0.52, "FF_AmberGlow", 14, (math.pi / 2, 0, 0)), b.box("Keyboard", (-1.15, -0.20, 1.07), (1.38, 0.42, 0.16), "FF_Cream", 0.025)]
        return p

    if sub == "Busking":
        p = [
            b.cylinder("BuskingDeck", (0, 0, 0.18), 2.35, 0.36, "FF_WoodDark", 20),
            b.cylinder("BuskingFloor", (0, 0, 0.40), 2.10, 0.10, "FF_WoodWarm", 20),
            b.box("BackdropRail", (0, -1.72, 1.26), (3.72, 0.12, 1.65), "FF_TealDark", 0.035),
            b.cylinder("MicStand", (0, 0.45, 1.12), 0.045, 1.50, "FF_Metal", 8),
            b.sphere("Mic", (0, 0.45, 1.88), (0.10, 0.08, 0.10), "FF_Cream"),
            b.box("TipCase", (1.15, 0.88, 0.40), (1.10, 0.60, 0.26), "FF_AmberGlow", 0.06),
            b.box("GuitarBody", (-0.92, -0.02, 1.05), (0.45, 0.14, 0.62), "FF_SunsetGlow", 0.12),
            b.box("GuitarNeck", (-0.70, -0.02, 1.58), (0.14, 0.10, 0.72), "FF_WoodWarm", 0.02, (0, 0, math.radians(-18))),
        ]
        for x in (-1.70, 1.70):
            p += b.lantern(f"StreetLight_{x}", x, 0.98, 1.54, 0.70)
        return p

    # ExperienceStage / DanceBattle
    p = [
        b.box("DanceDeck", (0, 0, 0.18), (5.45, 2.66, 0.36), "FF_WoodDark", 0.04),
        b.box("DanceFloor", (0, 0, 0.42), (4.72, 2.18, 0.12), "FF_TealDark", 0.02),
        b.box("DanceBackdrop", (0, -1.18, 1.34), (4.85, 0.12, 1.55), "FF_WoodWarm", 0.025),
        b.box("DanceGlow", (0, -1.09, 1.37), (4.32, 0.04, 1.04), "FF_SunsetGlow", 0.012),
        b.box("TopTruss", (0, 0.95, 2.30), (5.24, 0.14, 0.14), "FF_Metal", 0.02),
    ]
    for x in (-2.35, 2.35):
        p += [b.post(f"DanceTruss_{x}", x, 0.95, 0.34, 2.34, 0.12), b.box(f"DanceSpeaker_{x}", (x, 0.62, 0.96), (0.58, 0.48, 1.05), "FF_Metal", 0.04)]
    for x in (-1.55, -0.52, 0.52, 1.55):
        p += b.lantern(f"DanceLight_{x}", x, 0.96, 2.08, 0.54)
    return p


def build_safety(entry):
    sub = entry["subcategory"]
    if sub == "FirstAid":
        return b.build_medical_tent()

    if sub == "Security":
        p = add_kiosk_shell("Security", 5.05, 2.38, "FF_BlueGrey")
        p += [
            b.box("ControlWindow", (0, -0.76, 1.42), (3.55, 0.05, 0.82), "FF_Glass", 0.02),
            b.box("ControlConsole", (0, 0.34, 1.00), (3.10, 0.78, 0.28), "FF_Metal", 0.04),
            b.cylinder("Antenna", (0, -0.30, 2.48), 0.06, 0.76, "FF_Metal", 10),
            b.sphere("Beacon", (0, -0.30, 2.86), (0.16, 0.16, 0.16), "FF_SunsetGlow"),
        ]
        for x in (-1.12, 0.0, 1.12):
            p.append(b.box(f"Monitor_{x}", (x, 0.76, 1.40), (0.72, 0.08, 0.50), "FF_TealDark", 0.025))
        for x in (-1.90, 1.90):
            p.append(b.box(f"Barrier_{x}", (x, 1.18, 0.52), (1.20, 0.12, 0.72), "FF_AmberGlow", 0.025, (0, 0, math.radians(10 * (1 if x > 0 else -1)))))
        return p

    if sub == "FireSafety":
        p = [
            b.box("FireDeck", (0, 0, 0.12), (5.48, 2.64, 0.24), "FF_WoodDark", 0.04),
            b.box("FireHouse", (0, -0.28, 1.18), (5.00, 2.02, 1.95), "FF_Medical", 0.055),
            b.prism_gable("FireRoof", 5.38, 2.58, 2.10, 2.47, "FF_TealDark"),
            b.box("EngineBay", (-0.72, 0.83, 1.10), (2.72, 0.07, 1.72), "FF_Metal", 0.018),
            b.box("EngineFront", (-0.72, 0.94, 0.80), (2.30, 0.40, 1.00), "FF_Medical", 0.06),
            b.box("ServiceDoor", (1.52, 0.84, 1.12), (1.20, 0.06, 1.75), "FF_TealDark", 0.018),
        ]
        for x in (-1.45, -0.02):
            p.append(b.cylinder(f"EngineWheel_{x}", (x, 1.18, 0.43), 0.28, 0.18, "FF_Metal", 12, (math.pi / 2, 0, 0)))
        p += [add_ring("HoseReel", (1.52, 0.90, 1.40), 0.34, 0.07, "FF_Cream"), b.box("FireLight", (-0.72, 1.17, 1.40), (0.86, 0.08, 0.18), "FF_SunsetGlow", 0.02)]
        return p

    # Evacuation / EmergencyPoint
    p = [
        b.box("EmergencyDeck", (0, 0, 0.10), (5.30, 2.60, 0.20), "FF_WoodDark", 0.035),
        b.cylinder("MeetingPole", (0, -0.42, 1.30), 0.10, 2.42, "FF_Metal", 10),
        b.box("MeetingSign", (0, -0.35, 2.02), (2.00, 0.12, 0.80), "FF_Green", 0.06),
        b.box("CenterMarker", (0, 0.42, 0.18), (2.15, 1.25, 0.14), "FF_Teal", 0.04),
    ]
    for x, angle in ((-1.75, -18), (1.75, 18)):
        p += [
            b.box(f"ArrowShaft_{x}", (x, 0.82, 0.88), (0.22, 0.16, 1.35), "FF_Cream", 0.018, (0, 0, math.radians(angle))),
            add_cone(f"ArrowHead_{x}", (x + (0.20 if x > 0 else -0.20), 0.82, 1.60), 0.28, 0.0, 0.55, "FF_Cream", 4, (0, 0, math.radians(-angle))),
            b.cylinder(f"BeaconPost_{x}", (x, -0.70, 0.72), 0.07, 1.28, "FF_Metal", 10),
            b.sphere(f"Beacon_{x}", (x, -0.70, 1.40), (0.16, 0.16, 0.16), "FF_SunsetGlow"),
        ]
    p += b.plus_symbol("EmergencyPlus", (0, -0.43, 2.02), 0.58, "FF_Cream", 0.07)
    return p


def build_photospot(entry):
    sub = entry["subcategory"]
    if sub == "Landmark":
        p = [
            b.box("GateDeck", (0, 0, 0.12), (5.38, 2.50, 0.24), "FF_WoodDark", 0.04),
            b.half_arch("FestivalArch", 2.15, 0.22, -0.20, 0.28, "FF_WoodWarm"),
            b.post("GatePostL", -2.15, -0.20, 0.20, 1.05, 0.28),
            b.post("GatePostR", 2.15, -0.20, 0.20, 1.05, 0.28),
            b.box("GateBanner", (0, -0.14, 1.64), (2.60, 0.10, 0.62), "FF_Teal", 0.06),
            b.cylinder("GateSun", (0, -0.20, 2.28), 0.42, 0.12, "FF_SunsetGlow", 20, (math.pi / 2, 0, 0)),
        ]
        for x in (-1.70, -0.85, 0.0, 0.85, 1.70):
            p += b.lantern(f"GateLantern_{x}", x, 0.05, 2.18 - 0.20 * abs(x), 0.48)
        p += b.stairs("GateStairs", 2.40, 1.10, 0.24, 2)
        return p

    if sub == "Theme":
        p = [
            b.box("GardenDeck", (0, 0, 0.10), (5.25, 2.55, 0.20), "FF_WoodDark", 0.035),
            b.half_arch("FlowerArch", 1.95, 0.15, -0.30, 0.28, "FF_Green"),
            b.post("FlowerPostL", -1.95, -0.30, 0.18, 0.92, 0.20),
            b.post("FlowerPostR", 1.95, -0.30, 0.18, 0.92, 0.20),
            b.box("PhotoBench", (0, 0.18, 0.56), (2.35, 0.48, 0.15), "FF_WoodWarm", 0.035),
        ]
        for index, x in enumerate((-2.10, -1.50, -0.90, -0.30, 0.30, 0.90, 1.50, 2.10)):
            material = "FF_SunsetGlow" if index % 2 else "FF_AmberGlow"
            p += [
                b.sphere(f"Flower_{index}", (x, 0.76 if index % 2 else -0.62, 0.62 + 0.08 * (index % 3)), (0.22, 0.18, 0.22), material),
                b.cylinder(f"FlowerStem_{index}", (x, 0.76 if index % 2 else -0.62, 0.38), 0.045, 0.48, "FF_Green", 8),
            ]
        for x in (-1.42, -0.72, 0.0, 0.72, 1.42):
            p.append(b.sphere(f"ArchBloom_{x}", (x, -0.22, 2.00 - 0.18 * abs(x)), (0.18, 0.14, 0.18), "FF_Cream"))
        return p

    if sub == "Scenic":
        p = [
            b.box("ViewDeck", (0, 0, 0.18), (5.38, 2.50, 0.36), "FF_WoodDark", 0.04),
            b.box("ViewFloor", (0, 0, 0.41), (5.02, 2.18, 0.10), "FF_WoodWarm", 0.02),
            b.box("RiverRail", (0, -1.02, 1.02), (4.88, 0.10, 1.10), "FF_Teal", 0.02),
            b.cylinder("TelescopeStand", (1.12, 0.18, 1.10), 0.09, 1.35, "FF_Metal", 10),
            b.cylinder("Telescope", (1.12, 0.12, 1.72), 0.16, 0.92, "FF_BlueGrey", 12, (0, math.radians(68), 0)),
            b.cylinder("TelescopeLens", (1.52, 0.12, 1.90), 0.18, 0.08, "FF_Glass", 14, (0, math.radians(68), 0)),
        ]
        for x in (-2.30, -1.15, 0.0, 1.15, 2.30):
            p.append(b.post(f"RailPost_{x}", x, -1.02, 0.34, 1.55, 0.11))
        p += add_bench("ViewBench", -0.95, 0.55, 2.10)
        p += b.lantern("ViewLanternL", -2.25, 0.92, 1.34, 0.70)
        p += b.lantern("ViewLanternR", 2.25, 0.92, 1.34, 0.70)
        return p

    # Interactive / ARFrame
    p = [
        b.box("ARDeck", (0, 0, 0.12), (5.25, 2.50, 0.24), "FF_WoodDark", 0.04),
        b.box("ARFrameTop", (0, -0.25, 2.18), (3.85, 0.28, 0.28), "FF_AmberGlow", 0.04),
        b.box("ARFrameLeft", (-1.79, -0.25, 1.25), (0.28, 0.28, 1.86), "FF_AmberGlow", 0.04),
        b.box("ARFrameRight", (1.79, -0.25, 1.25), (0.28, 0.28, 1.86), "FF_AmberGlow", 0.04),
        b.box("ARInnerTop", (0, -0.12, 2.10), (3.25, 0.08, 0.08), "FF_Teal", 0.01),
        b.box("Console", (2.15, 0.45, 0.85), (0.70, 0.65, 1.35), "FF_TealDark", 0.06, (0, 0, math.radians(-7))),
        b.box("ConsoleScreen", (2.10, 0.80, 1.12), (0.48, 0.05, 0.55), "FF_Glass", 0.02, (0, 0, math.radians(-7))),
        b.cylinder("Camera", (0, -0.15, 2.43), 0.16, 0.26, "FF_Metal", 12, (math.pi / 2, 0, 0)),
    ]
    for x in (-1.20, -0.40, 0.40, 1.20):
        p += b.lantern(f"ARLight_{x}", x, 0.82, 0.54, 0.50)
    return p


BUILDERS = {
    "Booth": build_booth,
    "Toilet": build_toilet,
    "Info": build_info,
    "Rest": build_rest,
    "Performance": build_performance,
    "Safety": build_safety,
    "PhotoSpot": build_photospot,
}

BOUNDS = {
    "Booth": (5.60, 2.80, 2.50),
    "Toilet": (5.60, 2.80, 2.50),
    "Info": (5.35, 2.75, 2.50),
    "Rest": (5.60, 2.80, 2.50),
    "Performance": (5.80, 2.80, 2.50),
    "Safety": (5.60, 2.80, 2.50),
    "PhotoSpot": (5.60, 2.80, 2.50),
}


def canonical_names(entry):
    definition_id = entry["definition_id"]
    return (
        "SM_FFZS_" + definition_id,
        "T_UI_BuildCatalog_" + definition_id,
    )


def configure_thumbnail_render():
    scene = bpy.context.scene
    scene.render.resolution_x = 324
    scene.render.resolution_y = 260
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.image_settings.color_depth = "8"


def build_one(entry):
    definition_id = entry["definition_id"]
    mesh_name, texture_name = canonical_names(entry)
    fbx_path = FBX_DIR / f"{mesh_name}.fbx"
    png_path = PNG_DIR / f"{texture_name}.png"
    need_fbx = not fbx_path.exists()
    need_png = not png_path.exists()
    if not need_fbx and not need_png:
        return {"definition_id": definition_id, "status": "source_outputs_exist", "fbx": str(fbx_path), "png": str(png_path)}

    b.reset_scene()
    b.configure_scene()
    configure_thumbnail_render()
    builder = BUILDERS[entry["category"]]
    bounds = BOUNDS[entry["category"]]
    parts = builder(entry)
    visual = b.join_visual_parts(mesh_name, parts)
    b.fit_and_center(visual, bounds)
    b.validate_bounds(mesh_name, visual, bounds)
    distribution = b.material_distribution(visual)
    collision = b.add_collision(mesh_name, bounds)

    if need_fbx:
        b.export_fbx(mesh_name, visual, collision)
    if need_png:
        b.add_preview_environment(mesh_name, bounds)
        bpy.context.scene.render.filepath = str(png_path)
        bpy.ops.render.render(write_still=True)

    return {
        "definition_id": definition_id,
        "status": "built",
        "mesh": mesh_name,
        "texture": texture_name,
        "fbx": str(fbx_path),
        "png": str(png_path),
        "wrote_fbx": need_fbx,
        "wrote_png": need_png,
        "bounds_m": [round(float(value), 3) for value in visual.dimensions],
        "vertices": len(visual.data.vertices),
        "polygons": len(visual.data.polygons),
        "materials": distribution,
    }


def main():
    manifest = MANIFEST
    results = []
    failures = []
    for entry in manifest["examples"]:
        definition_id = entry["definition_id"]
        if definition_id in LEGACY_ASSET_ALIASES:
            results.append({"definition_id": definition_id, "status": "legacy_alias_deferred_to_import_verifier", **LEGACY_ASSET_ALIASES[definition_id]})
            print("FF_NOTION_FACILITY_SKIP", definition_id, "legacy-alias")
            continue
        try:
            result = build_one(entry)
            results.append(result)
            print("FF_NOTION_FACILITY", definition_id, result["status"])
        except Exception as error:
            failures.append({"definition_id": definition_id, "error": str(error)})
            print("FF_NOTION_FACILITY_ERROR", definition_id, error)

    report = {
        "source": manifest["source"],
        "expected_examples": len(manifest["examples"]),
        "results": results,
        "failures": failures,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    if failures:
        raise RuntimeError(f"Notion facility build failed for {len(failures)} assets")
    print(f"FF_NOTION_FACILITY_COMPLETE examples={len(results)} failures=0")


if __name__ == "__main__":
    main()
