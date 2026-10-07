# -*- coding: utf-8 -*-
# import_expansion70.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 제작 결과를 Unreal 에셋으로 가져오고 연결한다.

"""Import the Facility 70 production brief as native Unreal assets.

Run with UnrealEditor-Cmd -run=pythonscript -unattended -script=<this file>.
FF70_NUMBERS=1,59,65 selects a pilot; unset imports the complete build manifest.
Existing unrelated packages are never changed. Reruns preserve owned packages;
FF70_REPLACE_OWNED=1 explicitly refreshes this pipeline's marked assets only.
The saved PDAs, rather than the disposable build manifest, own runtime geometry.
"""
from __future__ import annotations

import hashlib
import glob
import json
import math
import os
from pathlib import Path
import re
import sys
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ffzs_asset_save_transaction import AssetSaveTransaction
from ffzs_definition_source import require_unattended_commandlet

SOURCE = ROOT / "SourceArt/FacilityExpansion70"
OUT = ROOT / "Saved/FacilityExpansion70"
ART = "/Game/FestaFiesta/Art/FacilityExpansion70"
DEFINITIONS = "/Game/FestaFiesta/Building/FF_Data_Building"
OWNER = "FacilityExpansion70.v1"
NO_SERVICE = {"Road", "Tile", "Lighting"}
CATEGORIES = {"Booth", "Toilet", "Info", "Rest", "Performance", "Safety", "PhotoSpot", "Lighting", "Road", "Tile"}
_COST_ROWS = None


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def rollback_changed_packages(transaction):
    """Read-only validation failures need no restore of Windows-locked packages.

    Imported objects stay unsaved until the entire selection is valid. Compare
    every snapshot before skipping restoration; a partially saved batch still
    uses the existing recovery mechanism and preserves its backup on failure.
    """
    unchanged = True
    for file_base, backups in transaction._entries:
        current = set(glob.glob(file_base + ".*"))
        originals = {original for original, backup in backups}
        if current != originals or any(Path(original).read_bytes() != Path(backup).read_bytes()
                                       for original, backup in backups):
            unchanged = False
            break
    if unchanged:
        transaction.commit()
        unreal.log("FF70_ROLLBACK_NOT_NEEDED no package files changed")
    else:
        transaction.rollback()


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False).encode("utf-8")).hexdigest()


def file_digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_path(value):
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def package(asset):
    return asset.get_path_name().split(".", 1)[0]


def metadata(asset, key):
    return str(unreal.EditorAssetLibrary.get_metadata_tag(asset, key) or "")


def mark(asset, identity, source_hash=""):
    for key, value in {"FF70.Owner": OWNER, "FF70.Identity": identity,
                       "FF70.SourceSHA256": source_hash}.items():
        unreal.EditorAssetLibrary.set_metadata_tag(asset, key, value)


def owned_existing(path, identity):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    asset = unreal.load_asset(path)
    check(asset and metadata(asset, "FF70.Owner") == OWNER
          and metadata(asset, "FF70.Identity") == identity,
          "Refusing occupied, unowned destination: " + path)
    return asset


def paths(entry):
    folder = ART + "/" + entry["category"] + "/" + entry["slug"]
    name = entry["id"]
    return dict(folder=folder, mesh=folder + "/SM_FF_" + name,
                texture=folder + "/T_UI_F70_" + name,
                definition=DEFINITIONS + "/" + entry["category"] + "/PDA_FFZS_" + name,
                rig=folder + "/BP_F70_" + name + "_LightingRig")


def authored_dimensions(entry):
    """Author the final native dimensions from the uniformly fitted source mesh.

    Catalog dimensions are design maxima. Rounding up to the 1cm placement grid
    introduces less than 1cm of stretch without changing the source proportions.
    """
    size = [float(value) * 100 for value in entry["bounds_m"]]
    return [max(1, math.ceil(value - .001)) for value in size[:2]], size[2]


def category_enum(category):
    # Unreal Python splits CamelCase reflected enum entries into upper snake case.
    return getattr(unreal.FFZSFacilityCategory, "PHOTO_SPOT" if category == "PhotoSpot" else category.upper())


def load_inputs():
    manifest = json.loads((SOURCE / "build_manifest.json").read_text(encoding="utf-8-sig"))
    entries = manifest["facilities"]
    requested = {int(x.strip()) for x in os.environ.get("FF70_NUMBERS", "").split(",") if x.strip()}
    if requested:
        entries = [entry for entry in entries if int(entry["number"]) in requested]
        check({int(e["number"]) for e in entries} == requested, "Pilot numbers missing from build manifest")
    else:
        check(len(entries) == 70, "Complete import requires exactly 70 built facilities")
    palette_file = source_path(manifest.get("palette", str(SOURCE / "palette.json")))
    palette = json.loads(palette_file.read_text(encoding="utf-8-sig"))
    palette = palette.get("materials", palette)
    if isinstance(palette, list):
        palette = {item["name"]: item for item in palette}
    return manifest, entries, palette


def selected_templates(entries):
    available = {}
    for path in unreal.EditorAssetLibrary.list_assets(DEFINITIONS, True, False):
        if "/PDA_FFZS_" not in path:
            continue
        definition = unreal.load_asset(path)
        if not isinstance(definition, unreal.FFZSFacilityDefinition):
            continue
        identity = str(definition.get_editor_property("spec").get_editor_property("facility_definition_id"))
        check(identity not in available, "Duplicate existing FacilityDefinitionId: " + identity)
        available[identity] = definition
    selected = {}
    canonical_ids = {key.casefold(): key for key in available}
    for entry in entries:
        prefix = entry["category"] + "_" + entry["subtype"] + "_"
        candidates = [key for key, value in available.items()
                      if key.startswith(prefix) and metadata(value, "FF70.Owner") != OWNER]
        if not candidates:
            candidates = [key for key, value in available.items()
                          if key.startswith(entry["category"] + "_") and metadata(value, "FF70.Owner") != OWNER]
        requested = entry.get("template_id")
        if not requested and entry.get("template_definition"):
            explicit_template = unreal.load_asset(entry["template_definition"])
            check(isinstance(explicit_template, unreal.FFZSFacilityDefinition),
                  "Missing template package: " + entry["template_definition"])
            requested = str(explicit_template.get_editor_property("spec").get_editor_property("facility_definition_id"))
        if requested:
            requested = canonical_ids.get(requested.casefold(), requested)
        check(not requested or requested in available, "Missing template: " + str(requested))
        check(candidates or requested, "No compatible category template: " + entry["id"])
        template = available[requested or sorted(candidates)[0]]
        template_cost_key(template)
        selected[entry["id"]] = template
        if entry["id"] in available:
            check(package(available[entry["id"]]) == paths(entry)["definition"],
                  "Definition ID already exists elsewhere: " + entry["id"])
    return selected


def template_cost_key(template):
    """Reuse the source's resolved price without inventing or altering table rows."""
    global _COST_ROWS
    if _COST_ROWS is None:
        table = unreal.load_asset("/Game/FestaFiesta/Data/Economy/DT_FacilityCosts")
        check(isinstance(table, unreal.DataTable), "Missing facility cost table")
        rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
        _COST_ROWS = {row["Name"].casefold(): row for row in rows}
    spec = template.get_editor_property("spec")
    source_id = str(spec.get_editor_property("facility_definition_id"))
    tokens = source_id.split("_", 2)
    explicit = str(spec.get_editor_property("install_cost_key"))
    candidates = ([explicit] if explicit not in {"", "None"} else
                  ["Cost." + ".".join(tokens), "Cost." + ".".join(tokens[:2])])
    candidates.append("Cost." + tokens[0] + ".Default")
    for candidate in candidates:
        row = _COST_ROWS.get(candidate.casefold())
        if row is not None and int(row["InstallCost"]) >= 0:
            return row["Name"]
    raise RuntimeError("Template price cannot be resolved: " + source_id + " candidates=" + str(candidates))


def import_yaw(entry):
    # Blender -Y-forward/Z-up FBX maps (x,y,z) to Unreal (x,-y,z).
    # Baking a half-turn gives (-x,+y,z): customer-facing +Y survives the
    # handedness conversion. Ground patterns retain their authored +X direction.
    return 0.0 if entry["category"] in {"Road", "Tile"} else 180.0


def source_point_to_unreal_cm(entry, position):
    x, y, z = [float(value) * 100 for value in position]
    return [-x, y, z] if import_yaw(entry) else [x, -y, z]


def fbx_options(entry):
    options = unreal.FbxImportUI()
    for key, value in dict(automated_import_should_detect_type=False, import_mesh=True,
                           import_as_skeletal=False, import_materials=False, import_textures=False,
                           mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH).items():
        options.set_editor_property(key, value)
    data = options.get_editor_property("static_mesh_import_data")
    for key, value in dict(combine_meshes=True, generate_lightmap_u_vs=True,
                           auto_generate_collision=False, import_mesh_lods=False,
                           convert_scene=True, convert_scene_unit=True, build_nanite=False,
                           remove_degenerates=True, transform_vertex_to_absolute=True,
                           import_rotation=unreal.Rotator(pitch=0, yaw=import_yaw(entry), roll=0),
                           import_uniform_scale=1.0, one_convex_hull_per_ucx=True,
                           normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS).items():
        data.set_editor_property(key, value)
    return options


def import_asset(source, target, mesh=False, replace=False, entry=None):
    folder, name = target.rsplit("/", 1)
    task = unreal.AssetImportTask()
    for key, value in dict(filename=str(source), destination_path=folder, destination_name=name,
                           automated=True, replace_existing=replace,
                           replace_existing_settings=replace, save=False).items():
        task.set_editor_property(key, value)
    if mesh:
        check(entry is not None, "Mesh import requires the facility axis policy")
        if replace:
            # FbxFactory routes existing objects through atomic reimport, whose
            # transforms come from the saved import data rather than task options.
            current = owned_existing(target, entry["id"])
            check(isinstance(current, unreal.StaticMesh), "Missing owned mesh for reimport")
            current.get_editor_property("asset_import_data").set_editor_property(
                "import_rotation", unreal.Rotator(pitch=0, yaw=import_yaw(entry), roll=0))
        task.set_editor_property("options", fbx_options(entry))
        task.set_editor_property("factory", unreal.FbxFactory())
    else:
        task.set_editor_property("factory", unreal.TextureFactory())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = {str(path) for path in task.get_editor_property("imported_object_paths")}
    check(imported == {target + "." + name}, "Unexpected imported packages: " + str(imported))
    asset = unreal.load_asset(target)
    check(isinstance(asset, unreal.StaticMesh if mesh else unreal.Texture2D), "Import failed: " + target)
    return asset


def material(name, settings, existing=None):
    result = existing or unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, ART + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    check(isinstance(result, unreal.Material), "Material creation failed: " + name)
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(result)

    def node(cls, x, y):
        return lib.create_material_expression(result, cls, x, y)

    def color(value):
        return unreal.LinearColor(float(value[0]), float(value[1]), float(value[2]), 1.0)

    base = node(unreal.MaterialExpressionConstant3Vector, -600, 0)
    base.set_editor_property("constant", color(settings["base_color"]))
    check(lib.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR), "BaseColor connection")
    for y, key, prop, default in [(150, "roughness", unreal.MaterialProperty.MP_ROUGHNESS, .7),
                                 (260, "metallic", unreal.MaterialProperty.MP_METALLIC, 0.0)]:
        constant = node(unreal.MaterialExpressionConstant, -600, y)
        constant.set_editor_property("r", float(settings.get(key, default)))
        check(lib.connect_material_property(constant, "", prop), "Material scalar connection")
    strength = float(settings.get("emissive_strength", 0))
    if strength > 0:
        original = node(unreal.MaterialExpressionConstant3Vector, -900, 450)
        original.set_editor_property("constant", color(settings.get("emissive_color", settings["base_color"])))
        override = node(unreal.MaterialExpressionVectorParameter, -900, 600)
        override.set_editor_property("parameter_name", "FF_LightingColor")
        override.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
        use_color = node(unreal.MaterialExpressionScalarParameter, -900, 750)
        use_color.set_editor_property("parameter_name", "FF_LightingUseColorOverride")
        use_color.set_editor_property("default_value", 0.0)
        lerp = node(unreal.MaterialExpressionLinearInterpolate, -600, 450)
        for origin, pin in [(original, "A"), (override, "B"), (use_color, "Alpha")]:
            check(lib.connect_material_expressions(origin, "", lerp, pin), "Lighting color connection")
        brightness = node(unreal.MaterialExpressionScalarParameter, -600, 800)
        brightness.set_editor_property("parameter_name", "FF_LightingBrightness")
        brightness.set_editor_property("default_value", 1.0)
        gain = node(unreal.MaterialExpressionMultiply, -300, 700)
        gain.set_editor_property("const_b", strength)
        check(lib.connect_material_expressions(brightness, "", gain, "A"), "Lighting gain connection")
        final = node(unreal.MaterialExpressionMultiply, -100, 450)
        check(lib.connect_material_expressions(lerp, "", final, "A"), "Lighting color output")
        check(lib.connect_material_expressions(gain, "", final, "B"), "Lighting brightness output")
        check(lib.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "Emissive connection")
    result.set_editor_property("two_sided", False)
    result.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mark(result, name, digest(settings))
    lib.recompile_material(result)
    return result


def mesh_metrics(mesh):
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if sub is None:
        # UE excludes this subsystem from Python commandlet initialization.
        # Its three read-only query methods use only the supplied StaticMesh
        # (verified in StaticMeshEditorSubsystem.cpp), so the CDO is sufficient.
        sub = unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
    bounds = mesh.get_bounding_box()
    minimum = [float(bounds.min.x), float(bounds.min.y), float(bounds.min.z)]
    maximum = [float(bounds.max.x), float(bounds.max.y), float(bounds.max.z)]
    return dict(min_cm=minimum, max_cm=maximum,
                size_cm=[b - a for a, b in zip(minimum, maximum)],
                triangles=int(mesh.get_num_triangles(0)), vertices=int(mesh.get_num_vertices(0)),
                uv_channels=int(sub.get_num_uv_channels(mesh, 0)),
                collision_count=int(sub.get_simple_collision_count(mesh)) + int(sub.get_convex_collision_count(mesh)),
                convex_collision_count=int(sub.get_convex_collision_count(mesh)))


def validate_mesh(mesh, entry):
    result = mesh_metrics(mesh)
    expected = [float(value) * 100 for value in entry["bounds_m"]]
    check(all(abs(actual - target) <= max(.5, target * .001) for actual, target in zip(result["size_cm"], expected)),
          "FBX scale mismatch {0}: actual={1} expected={2}".format(entry["id"], result["size_cm"], expected))
    check(abs(result["min_cm"][2]) <= .5 and all(abs(a + b) <= 1.0
          for a, b in zip(result["min_cm"][:2], result["max_cm"][:2])),
          "Ground-center pivot mismatch: " + entry["id"] + " bounds=" + str([result["min_cm"], result["max_cm"]]))
    check(result["triangles"] > 0 and result["uv_channels"] >= 2, "Missing render data/UV0+UV1: " + entry["id"])
    check(result["collision_count"] >= int(entry.get("collision_count", 1)) > 0,
          "Missing authored simple collision: " + entry["id"])
    return result


def configure_mesh(mesh, entry, materials):
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        name = str(slot.get_editor_property("material_slot_name"))
        check(name in materials, "Unknown material slot: " + name)
        mesh.set_material(index, materials[name])
    mesh.set_editor_property("light_map_resolution", 64)
    mesh.set_editor_property("light_map_coordinate_index", 1)
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", False)
    mesh.set_editor_property("nanite_settings", nanite)
    mark(mesh, entry["id"], file_digest(source_path(entry["fbx"])))
    unreal.EditorAssetLibrary.set_metadata_tag(mesh, "FF70.ImportYaw", str(import_yaw(entry)))
    unreal.EditorAssetLibrary.set_metadata_tag(mesh, "FF70.Source", str(source_path(entry["fbx"]).relative_to(ROOT)).replace("\\", "/"))
    return validate_mesh(mesh, entry)


def configure_texture(texture, entry):
    for key, value in dict(compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                           lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,
                           mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                           srgb=True, never_stream=True,
                           address_x=unreal.TextureAddress.TA_CLAMP,
                           address_y=unreal.TextureAddress.TA_CLAMP).items():
        texture.set_editor_property(key, value)
    mark(texture, entry["id"], file_digest(source_path(entry["preview"])))


def create_lighting_rig(entry, existing=None, replace=False):
    if existing and not replace:
        check(existing.generated_class(), "Existing lighting Blueprint has no generated class")
        return existing
    target = paths(entry)["rig"]
    parent_class = unreal.load_class(None, "/Script/FF_ZoneSystem.FFZSFacilityLightingRig")
    bp = existing or unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(target, parent_class)
    check(bp, "Lighting Blueprint creation failed: " + target)
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    fn = unreal.SubobjectDataBlueprintFunctionLibrary
    parent = next(h for h in sub.k2_gather_subobject_data_for_blueprint(bp) if fn.is_root_component(fn.get_data(h)))
    width, depth, height = entry["bounds_m"]
    positions = entry.get("light_positions_m") or [[0, 0, max(.1, height * .85)]]
    previous_lights = []
    if existing:
        previous_lights = sorted((fn.get_object_for_blueprint(fn.get_data(handle), bp)
                                 for handle in sub.k2_gather_subobject_data_for_blueprint(bp)
                                 if isinstance(fn.get_object_for_blueprint(fn.get_data(handle), bp), unreal.PointLightComponent)),
                                key=lambda lamp: lamp.get_name())
        check(len(previous_lights) == min(12, len(positions)),
              "Owned lighting rig light count changed; inspect before rebuilding: " + target)
    # Respect the runtime component budget and use actual authored globe positions when supplied.
    for index, position in enumerate(positions[:12]):
        if existing:
            lamp = previous_lights[index]
        else:
            handle, reason = sub.add_new_subobject(unreal.AddNewSubobjectParams(
                parent_handle=parent, new_class=unreal.PointLightComponent, blueprint_context=bp))
            check(fn.is_handle_valid(handle), "Add light: " + str(reason))
            check(sub.rename_subobject(handle, unreal.Text("FixtureLight_{0:02d}".format(index))), "Rename light")
            lamp = fn.get_object_for_blueprint(fn.get_data(handle), bp)
        lamp.set_editor_property("relative_location", unreal.Vector(*source_point_to_unreal_cm(entry, position)))
        lamp.set_mobility(unreal.ComponentMobility.MOVABLE)
        for key, value in dict(intensity_units=unreal.LightUnits.LUMENS, intensity=650.0 / max(1, len(positions)),
                               attenuation_radius=max(250.0, float(height) * 120),
                               light_color=unreal.Color(255, 218, 166, 255), cast_shadows=False,
                               source_radius=12.0, use_inverse_squared_falloff=True).items():
            lamp.set_editor_property(key, value)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    check(bp.generated_class(), "Lighting Blueprint compile failed")
    mark(bp, entry["id"], digest([source_point_to_unreal_cm(entry, p) for p in positions]))
    return bp


def author_queue(spec, entry):
    width, depth = [int(value) for value in entry["footprint_cm"]]
    rules = spec.get_editor_property("queue_rules").copy()
    for key in ("queue_reserved_rects", "queue_reserved_cells", "queue_spline_points_cm", "anchors"):
        spec.set_editor_property(key, [])
    spec.set_editor_property("audience", unreal.FFZSAudienceLayout())
    rules.set_editor_property("slot_spacing_cm", 115.0)
    rules.set_editor_property("slot_arrival_tolerance_cm", 35.0)
    if entry["category"] in NO_SERVICE:
        rules.set_editor_property("max_waiting_agents", 0)
        rules.set_editor_property("service_capacity", 0)
    else:
        count = int(entry.get("max_waiting_agents", 5))
        check(1 <= count <= 32, "Unsupported authoring queue capacity")
        rules.set_editor_property("max_waiting_agents", count)
        rules.set_editor_property("service_capacity", 1)
        # Front +Y, all movement targets beyond the footprint, all bodies protected.
        # A 115cm pitch exceeds the project's 68cm Crowd diameter and 70cm tolerance separation.
        front = depth * .5
        service = (0.0, front + 75.0, 0.0)
        queue_head = (115.0, front + 190.0, 0.0)
        queue_tail = (115.0, front + 190.0 + max(1, count - 1) * 115.0, 0.0)
        locations = [(unreal.FFZSAnchorType.ENTRY, (230.0, front + 190.0, 0.0)),
                     (unreal.FFZSAnchorType.SERVICE, service),
                     (unreal.FFZSAnchorType.EXIT, (-115.0, front + 75.0, 0.0))]
        anchors = []
        for kind, location in locations:
            anchor = unreal.FFZSAnchorDefinition()
            anchor.set_editor_property("type", kind)
            anchor.set_editor_property("index", 0)
            transform = unreal.Transform()
            transform.set_editor_property("translation", unreal.Vector(*location))
            anchor.set_editor_property("local_transform", transform)
            anchors.append(anchor)
        rect = unreal.FFZSCellRect()
        rect.set_editor_property("min", unreal.IntPoint(math.floor(width * .5 - 160), depth + 35))
        rect.set_editor_property("max", unreal.IntPoint(math.ceil(width * .5 + 275), math.ceil(queue_tail[1] + depth * .5 + 50)))
        spec.set_editor_property("queue_reserved_rects", [rect])
        spec.set_editor_property("queue_spline_points_cm", [unreal.Vector(*queue_head), unreal.Vector(*queue_tail)])
        spec.set_editor_property("anchors", anchors)
    spec.set_editor_property("queue_rules", rules)


def configure_definition(entry, template, mesh, texture, rig=None, existing=None):
    target = paths(entry)["definition"]
    if existing:
        definition = existing
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.load_class(None, "/Script/FF_ZoneSystem.FFZSFacilityDefinition"))
        folder, name = target.rsplit("/", 1)
        definition = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, None, factory)
    check(isinstance(definition, unreal.FFZSFacilityDefinition), "Native DataAsset creation failed: " + target)
    spec = template.get_editor_property("spec").copy()
    final_footprint, final_height = authored_dimensions(entry)
    values = dict(facility_definition_id=entry["id"], display_name=unreal.Text(entry["name_ko"]),
                  install_cost_key=template_cost_key(template),
                  category=category_enum(entry["category"]),
                  footprint_size_cells=unreal.IntPoint(*final_footprint),
                  placement_obstacle_height_cm=final_height,
                  clearance_cells=0 if entry["category"] in NO_SERVICE else 100,
                  visual_mesh=mesh, visual_sizing_mode=unreal.FFZSVisualSizingMode.MATCH_PLACEMENT_BOUNDS,
                  visual_rotation=unreal.Rotator(), visual_offset_cm=unreal.Vector(),
                  max_instances_per_world=0, allowed_zone_ids=[], uses_facility_levels=False, upgrade_tiers=[])
    for key, value in values.items():
        spec.set_editor_property(key, value)
    # New definitions retain same-subtype economic/audio settings, never old geometry or upgrade meshes.
    author_queue(spec, dict(entry, footprint_cm=final_footprint, height_cm=final_height))
    lights = unreal.FFZSFacilityLightingSettings()
    if rig:
        lights.set_editor_property("lighting_rig_class", rig.generated_class())
        lights.set_editor_property("enabled", True)
        lights.set_editor_property("brightness_multiplier", 1.0)
    spec.set_editor_property("lighting_settings", lights)
    spec.set_editor_property("bAllowFacilityPlacementOnTop", entry["category"] == "Tile")
    definition.set_editor_property("spec", spec)
    definition.set_editor_property("catalog_thumbnail", texture)
    definition.set_editor_property("show_in_build_catalog", True)
    definition.set_editor_property("enable_drag_placement", entry["category"] in {"Road", "Tile"})
    definition.set_editor_property("procedurally_managed_visual_mesh", False)
    mark(definition, entry["id"], digest({key: entry[key] for key in ["id", "name_ko", "footprint_cm", "height_cm"]}))
    unreal.EditorAssetLibrary.set_metadata_tag(definition, "FF70.DescriptionKO", entry.get("description_ko", ""))
    unreal.EditorAssetLibrary.set_metadata_tag(definition, "FF70.Template", package(template))
    unreal.EditorAssetLibrary.set_metadata_tag(definition, "FF70.RequestedDimensionsCM",
                                               json.dumps([*entry["footprint_cm"], entry["height_cm"]]))
    errors = definition.is_definition_valid()
    check(errors is not None and len(errors) == 0, "Native facility validation failed: " + entry["id"] + ": " + str(errors))
    return definition


def preflight(entries, palette, replace):
    check(len({entry["id"].casefold() for entry in entries}) == len(entries), "Duplicate build IDs")
    targets, existing, material_names = [], {}, set()
    for entry in entries:
        check(entry["category"] in CATEGORIES, "Invalid category")
        category_enum(entry["category"])
        check(entry["id"] == "_".join([entry["category"], entry["subtype"], entry["slug"]]), "Malformed stable ID")
        check(re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", entry["id"]), "Noncanonical ID")
        check(len(entry["bounds_m"]) == 3 and all(float(v) > 0 for v in entry["bounds_m"]), "Invalid bounds")
        for key in ["fbx", "blend", "preview"]:
            check(source_path(entry[key]).is_file(), "Missing source: " + str(entry[key]))
        p = paths(entry)
        for kind in ["mesh", "texture", "definition"] + (["rig"] if entry["category"] == "Lighting" else []):
            asset = owned_existing(p[kind], entry["id"])
            existing[p[kind]] = asset
            if not asset or replace:
                targets.append(p[kind])
            if asset and not replace and kind in {"mesh", "texture"}:
                source = source_path(entry["fbx" if kind == "mesh" else "preview"])
                check(metadata(asset, "FF70.SourceSHA256") == file_digest(source),
                      "Owned source changed; use FF70_REPLACE_OWNED=1 to refresh: " + p[kind])
                if kind == "mesh":
                    check(metadata(asset, "FF70.ImportYaw") == str(import_yaw(entry)),
                          "Owned mesh axis policy changed; use FF70_REPLACE_OWNED=1: " + p[kind])
        material_names.update(entry["material_names"])
    for name in sorted(material_names):
        check(name.startswith("M_F70_") and name in palette, "Unknown palette material: " + name)
        p = ART + "/Materials/" + name
        asset = owned_existing(p, name)
        existing[p] = asset
        if not asset or replace:
            targets.append(p)
        if asset and not replace:
            check(metadata(asset, "FF70.SourceSHA256") == digest(palette[name]), "Existing palette differs: " + name)
    return targets, existing, material_names


def run():
    require_unattended_commandlet()
    OUT.mkdir(parents=True, exist_ok=True)
    report = dict(status="RUNNING", entries=[], failures=[])
    transaction = None
    try:
        manifest, entries, palette = load_inputs()
        replace = os.environ.get("FF70_REPLACE_OWNED") == "1"
        templates = selected_templates(entries)
        targets, existing, names = preflight(entries, palette, replace)
        if targets:
            transaction = AssetSaveTransaction(targets, "FacilityExpansion70")
        for command in ["Interchange.FeatureFlags.Import.FBX 0", "Interchange.FeatureFlags.Import.FBX.ToLevel 0"]:
            unreal.SystemLibrary.execute_console_command(None, command)
        saves, materials = [], {}
        for name in sorted(names):
            current = existing[ART + "/Materials/" + name]
            materials[name] = material(name, palette[name], current) if not current or replace else current
            if not current or replace:
                saves.append(materials[name])
        for entry in entries:
            p = paths(entry)
            mesh = existing[p["mesh"]]
            if not mesh or replace:
                mesh = import_asset(source_path(entry["fbx"]), p["mesh"], mesh=True, replace=bool(mesh), entry=entry)
                configure_mesh(mesh, entry, materials)
                saves.append(mesh)
            metrics = validate_mesh(mesh, entry)
            texture = existing[p["texture"]]
            if not texture or replace:
                texture = import_asset(source_path(entry["preview"]), p["texture"], replace=bool(texture))
                configure_texture(texture, entry)
                saves.append(texture)
            rig = None
            if entry["category"] == "Lighting":
                rig = create_lighting_rig(entry, existing[p["rig"]], replace=replace)
                if not existing[p["rig"]] or replace:
                    saves.append(rig)
            definition = existing[p["definition"]]
            if not definition or replace:
                definition = configure_definition(entry, templates[entry["id"]], mesh, texture, rig, definition)
                saves.append(definition)
            errors = definition.is_definition_valid()
            check(errors is not None and len(errors) == 0,
                  "Existing Definition invalid: " + entry["id"] + ": " + str(errors))
            report["entries"].append(dict(number=entry["number"], id=entry["id"], mesh=package(mesh),
                                           definition=package(definition), thumbnail=package(texture),
                                           template=package(templates[entry["id"]]),
                                           requested_footprint_cm=entry["footprint_cm"], requested_height_cm=entry["height_cm"],
                                           authored_footprint_cm=authored_dimensions(entry)[0], authored_height_cm=authored_dimensions(entry)[1],
                                           import_yaw_degrees=import_yaw(entry), customer_front="+X" if entry["category"] in {"Road", "Tile"} else "+Y",
                                           rig=package(rig) if rig else None, **metrics))
            unreal.log("FF70_IMPORT_READY " + entry["id"])
        for asset in saves:
            check(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), "Save failed: " + package(asset))
        if transaction:
            transaction.commit()
        report.update(status="PASS", selected_count=len(entries), saved_packages=len(saves),
                      selection=os.environ.get("FF70_NUMBERS") or "all", engine_version=str(unreal.SystemLibrary.get_engine_version()))
        unreal.log("FF70_IMPORT_PASS count=" + str(len(entries)))
    except Exception:
        report.update(status="FAIL", failures=[traceback.format_exc()])
        if transaction:
            rollback_changed_packages(transaction)
        raise
    finally:
        label = (os.environ.get("FF70_NUMBERS") or "all").replace(",", "_")
        (OUT / ("import_" + label + ".json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    run()
