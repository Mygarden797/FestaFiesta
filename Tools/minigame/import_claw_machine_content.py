# -*- coding: utf-8 -*-
"""Import and wire all ClawMachine production content into Unreal Engine.

Prerequisites:
  1. Build ``FestaFiestaEditor`` after adding the ClawMachine C++ classes.
  2. Run ``Tools/facilities/build_claw_machine_test_asset.py`` in Blender.
  3. Keep the two ImageGen sources at the paths declared below.

Command (editor closed)::

    UnrealEditor-Cmd.exe FestaFiesta.uproject \
        -ExecutePythonScript="Tools/minigame/import_claw_machine_content.py" \
        -nop4 -nosplash -nullrhi -unattended -DDC-ForceMemoryCache

Every package is tagged with ``FF.ClawMachine.Owner``. A rerun may update only
those owned packages and refuses to overwrite unrelated artist content.
"""

from __future__ import annotations

import os

import unreal


OWNER_TAG = "FF.ClawMachine.Owner"
OWNER_VALUE = "FestaFiesta.ClawMachine.v1"

MESH_NAME = "SM_FFZS_Booth_ActivityGame_ClawMachine"
FACILITY_TEXTURE_NAME = "T_UI_BuildCatalog_Booth_ActivityGame_ClawMachine"
BACKDROP_TEXTURE_NAME = "T_MG_ClawMachine_Interior_ImageGen"

MESH_DESTINATION = "/Game/FestaFiesta/Zone/Test_Asset/Asset"
FACILITY_TEXTURE_DESTINATION = "/Game/FestaFiesta/Zone/Test_Asset/image"
MINIGAME_ART_DESTINATION = "/Game/FestaFiesta/UI/Minigame/ClawMachine"
MINIGAME_DATA_DESTINATION = "/Game/FestaFiesta/Minigame/Data"
FACILITY_DEFINITION_DESTINATION = "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth"

MESH_PATH = MESH_DESTINATION + "/" + MESH_NAME
FACILITY_TEXTURE_PATH = FACILITY_TEXTURE_DESTINATION + "/" + FACILITY_TEXTURE_NAME
BACKDROP_TEXTURE_PATH = MINIGAME_ART_DESTINATION + "/" + BACKDROP_TEXTURE_NAME
BACKDROP_MATERIAL_NAME = "M_MG_ClawMachine_Backdrop"
BACKDROP_MATERIAL_PATH = MINIGAME_ART_DESTINATION + "/" + BACKDROP_MATERIAL_NAME
MINIGAME_DEFINITION_PATH = MINIGAME_DATA_DESTINATION + "/DA_Minigame_ClawMachine"
PRIZE_TABLE_PATH = MINIGAME_DATA_DESTINATION + "/DT_ClawMachinePrizes"
FACILITY_DEFINITION_NAME = "PDA_FFZS_Booth_ActivityGame_ClawMachine"
FACILITY_DEFINITION_PATH = FACILITY_DEFINITION_DESTINATION + "/" + FACILITY_DEFINITION_NAME
FACILITY_TEMPLATE_PATH = (
    "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/"
    "PDA_FFZS_Booth_ActivityGame_Surfing"
)


def project_dir() -> str:
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def source_path(relative: str) -> str:
    return os.path.join(project_dir(), relative.replace("/", os.sep))


def owned(asset) -> bool:
    return bool(asset) and str(
        unreal.EditorAssetLibrary.get_metadata_tag(asset, OWNER_TAG) or ""
    ) == OWNER_VALUE


def require_replaceable(asset_path: str):
    asset = (
        unreal.EditorAssetLibrary.load_asset(asset_path)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path)
        else None
    )
    if asset and not owned(asset):
        raise RuntimeError("Refusing to overwrite unowned asset: " + asset_path)
    return asset


def tag_owned(asset, generated_source: str) -> None:
    unreal.EditorAssetLibrary.set_metadata_tag(asset, OWNER_TAG, OWNER_VALUE)
    unreal.EditorAssetLibrary.set_metadata_tag(
        asset, "FF.ClawMachine.GeneratedSource", generated_source.replace("\\", "/")
    )


def try_set(target, property_name: str, value) -> bool:
    try:
        target.set_editor_property(property_name, value)
        return True
    except Exception as error:
        unreal.log_warning(
            "[FF_CLAW_IMPORT] optional property {0} not set: {1}".format(
                property_name, error
            )
        )
        return False


def import_texture(
    source_file: str,
    destination: str,
    asset_name: str,
    *,
    ui_texture: bool,
):
    expected_path = destination + "/" + asset_name
    existing = require_replaceable(expected_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_file)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", bool(existing))
    try_set(task, "replace_existing_settings", False)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.TextureFactory())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(expected_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Texture import failed: " + expected_path)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", ui_texture)
    texture.set_editor_property(
        "lod_group",
        unreal.TextureGroup.TEXTUREGROUP_UI
        if ui_texture
        else unreal.TextureGroup.TEXTUREGROUP_WORLD,
    )
    texture.set_editor_property(
        "compression_settings",
        unreal.TextureCompressionSettings.TC_EDITOR_ICON
        if ui_texture
        else unreal.TextureCompressionSettings.TC_DEFAULT,
    )
    texture.set_editor_property(
        "mip_gen_settings",
        unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
        if ui_texture
        else unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP,
    )
    texture.set_editor_property("max_texture_size", 512 if ui_texture else 1024)
    tag_owned(texture, os.path.relpath(source_file, project_dir()))
    return texture


def make_fbx_options():
    options = unreal.FbxImportUI()
    try_set(options, "automated_import_should_detect_type", False)
    try_set(options, "import_mesh", True)
    try_set(options, "import_as_skeletal", False)
    try_set(options, "import_materials", False)
    try_set(options, "import_textures", False)
    try_set(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = options.get_editor_property("static_mesh_import_data")
    try_set(data, "combine_meshes", True)
    try_set(data, "generate_lightmap_u_vs", True)
    try_set(data, "auto_generate_collision", False)
    try_set(data, "convert_scene", True)
    try_set(data, "convert_scene_unit", True)
    try_set(data, "build_nanite", False)
    try_set(data, "remove_degenerates", True)
    return options


def import_mesh(source_file: str):
    existing = require_replaceable(MESH_PATH)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_file)
    task.set_editor_property("destination_path", MESH_DESTINATION)
    task.set_editor_property("destination_name", MESH_NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", bool(existing))
    try_set(task, "replace_existing_settings", False)
    task.set_editor_property("save", False)
    task.set_editor_property("options", make_fbx_options())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("StaticMesh import failed: " + MESH_PATH)
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        if not slot_name.startswith("FF_"):
            raise RuntimeError("Unexpected material slot: " + slot_name)
        material_path = (
            MESH_DESTINATION + "/Materials/M_FFZS_" + slot_name[3:]
        )
        material = unreal.EditorAssetLibrary.load_asset(material_path)
        if not isinstance(material, unreal.MaterialInterface):
            raise RuntimeError("Missing shared facility material: " + material_path)
        mesh.set_material(index, material)

    # UE 5.7 Interchange may ignore an FBX UCX sibling even when the legacy importer
    # recognizes it. Preserve the source hull and add one deterministic box fallback.
    static_mesh_subsystem = unreal.get_editor_subsystem(
        unreal.StaticMeshEditorSubsystem
    )
    if (
        static_mesh_subsystem
        and static_mesh_subsystem.get_simple_collision_count(mesh) <= 0
    ):
        static_mesh_subsystem.add_simple_collisions(
            mesh, unreal.ScriptCollisionShapeType.BOX
        )
    if (
        static_mesh_subsystem
        and static_mesh_subsystem.get_simple_collision_count(mesh) <= 0
    ):
        raise RuntimeError("Facility mesh has no simple collision after fallback")
    try_set(mesh, "light_map_resolution", 64)
    try_set(mesh, "allow_cpu_access", False)
    tag_owned(mesh, os.path.relpath(source_file, project_dir()))
    unreal.EditorAssetLibrary.set_metadata_tag(
        mesh,
        "FFZS.FacilityVisualContract",
        "ReferenceArt;PivotGroundCenter;Front+Y;UCXCollision;DefinitionOwnsBounds",
    )
    return mesh


def create_backdrop_material(backdrop):
    material = require_replaceable(BACKDROP_MATERIAL_PATH)
    if isinstance(material, unreal.Material):
        # UE 5.7 can assert when delete_all_material_expressions is called on an
        # already-saved rooted material during a headless reimport. The parameterized
        # material already follows the owned contract, and the reimported Texture2D
        # keeps the same object path, so no graph mutation is required on reruns.
        tag_owned(material, "Tools/minigame/import_claw_machine_content.py")
        return material
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        BACKDROP_MATERIAL_NAME,
        MINIGAME_ART_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Could not create backdrop material")
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", False)

    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -420, -40
    )
    sample.set_editor_property("parameter_name", "BackdropTexture")
    sample.set_editor_property("texture", backdrop)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -420, 180
    )
    roughness.set_editor_property("r", 0.82)
    emissive_scale = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -420, 290
    )
    emissive_scale.set_editor_property("r", 0.12)
    emissive = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -150, 100
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(sample, "RGB", emissive, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(
        emissive_scale, "", emissive, "B"
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    tag_owned(material, "Tools/minigame/import_claw_machine_content.py")
    return material


def create_minigame_definition(prize_table):
    definition = require_replaceable(MINIGAME_DEFINITION_PATH)
    if definition is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.FF_ClawMachineDefinition)
        definition = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DA_Minigame_ClawMachine",
            MINIGAME_DATA_DESTINATION,
            unreal.FF_ClawMachineDefinition,
            factory,
        )
    if not isinstance(definition, unreal.FF_ClawMachineDefinition):
        raise RuntimeError("Could not create UFF_ClawMachineDefinition")
    definition.set_editor_property("prize_catalog_table", prize_table)
    tag_owned(definition, "Source/FestaFiesta/Private/Minigame/FF_MinigameDefinition.cpp")
    return definition


def create_facility_definition(mesh, thumbnail):
    definition = require_replaceable(FACILITY_DEFINITION_PATH)
    if definition is None:
        template = unreal.EditorAssetLibrary.load_asset(FACILITY_TEMPLATE_PATH)
        if template is None:
            raise RuntimeError("Missing facility template: " + FACILITY_TEMPLATE_PATH)
        if not unreal.EditorAssetLibrary.duplicate_asset(
            FACILITY_TEMPLATE_PATH, FACILITY_DEFINITION_PATH
        ):
            raise RuntimeError("Could not duplicate facility definition template")
        definition = unreal.EditorAssetLibrary.load_asset(FACILITY_DEFINITION_PATH)
    if definition is None or definition.get_class().get_name() != "FFZSFacilityDefinition":
        raise RuntimeError("Invalid claw-machine facility definition")

    spec = definition.get_editor_property("spec")
    spec.set_editor_property(
        "facility_definition_id", unreal.Name("Booth_ActivityGame_ClawMachine")
    )
    spec.set_editor_property("display_name", unreal.Text("바다 친구 인형뽑기"))
    spec.set_editor_property("visual_mesh", mesh)
    # FacilityDefinitionId is the single source for convention pricing. Keeping
    # this override empty resolves Cost.Booth.ActivityGame.ClawMachine and avoids
    # accidentally looking up the facility ID as a DataTable row name.
    spec.set_editor_property("install_cost_key", unreal.Name("None"))
    spec.set_editor_property(
        "revenue_rule_key", unreal.Name("Booth_ActivityGame_ClawMachine")
    )
    spec.set_editor_property(
        "experience_effect_key", unreal.Name("Booth_ActivityGame_ClawMachine")
    )
    definition.set_editor_property("spec", spec)
    definition.set_editor_property("catalog_thumbnail", thumbnail)
    tag_owned(definition, "Tools/minigame/import_claw_machine_content.py")
    return definition


def save(asset) -> None:
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Failed to save " + asset.get_path_name())


def run() -> None:
    mesh_source = source_path(
        "SourceAssets/Zone/FacilityMeshes/" + MESH_NAME + ".fbx"
    )
    facility_texture_source = source_path(
        "Content/FestaFiesta/Zone/Test_Asset/image/"
        + FACILITY_TEXTURE_NAME
        + ".png"
    )
    backdrop_source = source_path(
        "SourceAssets/UI/Minigame/ClawMachine/"
        + BACKDROP_TEXTURE_NAME
        + ".png"
    )
    for path in (mesh_source, facility_texture_source, backdrop_source):
        if not os.path.isfile(path):
            raise RuntimeError("Missing source file: " + path)

    prize_table = unreal.EditorAssetLibrary.load_asset(PRIZE_TABLE_PATH)
    if not isinstance(prize_table, unreal.DataTable):
        raise RuntimeError(
            "Missing prize table. Run FF.BuildClawMachinePrizeTable first: "
            + PRIZE_TABLE_PATH
        )
    if not owned(prize_table):
        raise RuntimeError("Prize table has unexpected ownership: " + PRIZE_TABLE_PATH)

    mesh = import_mesh(mesh_source)
    thumbnail = import_texture(
        facility_texture_source,
        FACILITY_TEXTURE_DESTINATION,
        FACILITY_TEXTURE_NAME,
        ui_texture=True,
    )
    backdrop = import_texture(
        backdrop_source,
        MINIGAME_ART_DESTINATION,
        BACKDROP_TEXTURE_NAME,
        ui_texture=False,
    )
    material = create_backdrop_material(backdrop)
    minigame_definition = create_minigame_definition(prize_table)
    facility_definition = create_facility_definition(mesh, thumbnail)
    for asset in (
        mesh,
        thumbnail,
        backdrop,
        material,
        prize_table,
        minigame_definition,
        facility_definition,
    ):
        save(asset)
    unreal.log(
        "FF_CLAW_MACHINE_CONTENT_OK mesh={0} facility={1} minigame={2}".format(
            mesh.get_path_name(),
            facility_definition.get_path_name(),
            minigame_definition.get_path_name(),
        )
    )


run()
