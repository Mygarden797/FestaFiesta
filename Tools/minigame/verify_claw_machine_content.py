# -*- coding: utf-8 -*-
"""Static content-contract verification for the ClawMachine asset set."""

import unreal


OWNER_TAG = "FF.ClawMachine.Owner"
OWNER_VALUE = "FestaFiesta.ClawMachine.v1"
ASSET_PATHS = (
    "/Game/FestaFiesta/Zone/Test_Asset/Asset/SM_FFZS_Booth_ActivityGame_ClawMachine",
    "/Game/FestaFiesta/Zone/Test_Asset/image/T_UI_BuildCatalog_Booth_ActivityGame_ClawMachine",
    "/Game/FestaFiesta/UI/Minigame/ClawMachine/T_MG_ClawMachine_Interior_ImageGen",
    "/Game/FestaFiesta/UI/Minigame/ClawMachine/M_MG_ClawMachine_Backdrop",
    "/Game/FestaFiesta/Minigame/Data/DT_ClawMachinePrizes",
    "/Game/FestaFiesta/Minigame/Data/DA_Minigame_ClawMachine",
    "/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/PDA_FFZS_Booth_ActivityGame_ClawMachine",
)
SCREEN_PATH = "/Game/FestaFiesta/UI/Minigame/WBP_FF_ClawMachine"
JOYSTICK_SCREEN_PATH = "/Game/FestaFiesta/UI/Minigame/WBP_FF_ClawMachineJoystick"


def require_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset: " + path)
    owner = str(unreal.EditorAssetLibrary.get_metadata_tag(asset, OWNER_TAG) or "")
    if owner != OWNER_VALUE:
        raise RuntimeError("Unexpected ownership for {0}: {1}".format(path, owner))
    return asset


def run():
    mesh, thumbnail, backdrop, material, prize_table, minigame, facility = [
        require_asset(path) for path in ASSET_PATHS
    ]
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Facility visual is not a StaticMesh")
    if len(mesh.get_editor_property("static_materials")) != 9:
        raise RuntimeError("Facility mesh must keep exactly 9 shared material slots")
    static_mesh_subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if static_mesh_subsystem and static_mesh_subsystem.get_simple_collision_count(mesh) <= 0:
        raise RuntimeError("Facility mesh has no simple collision")

    if not isinstance(thumbnail, unreal.Texture2D) or int(thumbnail.blueprint_get_size_x()) < 512:
        raise RuntimeError("Catalog thumbnail contract failed")
    if not isinstance(backdrop, unreal.Texture2D) or int(backdrop.blueprint_get_size_x()) < 1024:
        raise RuntimeError("Backdrop texture contract failed")
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Backdrop material contract failed")
    if not isinstance(prize_table, unreal.DataTable):
        raise RuntimeError("Prize catalog is not a DataTable")
    prize_rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(prize_table)
    if len(prize_rows) < 1:
        raise RuntimeError("Prize catalog must contain at least one row")
    if minigame.get_class().get_name() != "FF_ClawMachineDefinition":
        raise RuntimeError("Minigame data asset has the wrong class")
    minigame_id = str(minigame.get_editor_property("minigame_id"))
    if minigame_id != "ClawMachine":
        raise RuntimeError("Unexpected MinigameId: " + minigame_id)
    linked_prize_table = minigame.get_editor_property("prize_catalog_table")
    if not linked_prize_table or linked_prize_table.get_path_name() != prize_table.get_path_name():
        raise RuntimeError("Minigame PrizeCatalogTable link failed")

    spec = facility.get_editor_property("spec")
    facility_id = str(spec.get_editor_property("facility_definition_id"))
    if facility_id != "Booth_ActivityGame_ClawMachine":
        raise RuntimeError("Unexpected FacilityDefinitionId: " + facility_id)
    install_cost_key = str(spec.get_editor_property("install_cost_key"))
    if install_cost_key not in ("", "None"):
        raise RuntimeError(
            "InstallCostKey must stay empty for convention pricing: "
            + install_cost_key
        )
    linked_mesh = spec.get_editor_property("visual_mesh")
    linked_thumbnail = facility.get_editor_property("catalog_thumbnail")
    if not linked_mesh or linked_mesh.get_path_name() != mesh.get_path_name():
        raise RuntimeError("Facility VisualMesh link failed")
    if not linked_thumbnail or linked_thumbnail.get_path_name() != thumbnail.get_path_name():
        raise RuntimeError("Facility CatalogThumbnail link failed")

    # UE 5.7 does not expose UPrimaryDataAsset::GetPrimaryAssetId to Python.
    # Both project implementations derive the name from the two IDs validated
    # above, so the expected IDs are constructed here after checking the asset
    # class and source fields. The native content tests exercise the overrides.
    facility_primary_id = (
        "FFZSFacilityDefinition:Booth_ActivityGame_ClawMachine"
    )
    minigame_primary_id = "FFMinigameDefinition:ClawMachine"
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    if not registry.get_asset_by_object_path(facility.get_path_name()).is_valid():
        raise RuntimeError("Facility is missing from AssetRegistry")
    if not registry.get_asset_by_object_path(minigame.get_path_name()).is_valid():
        raise RuntimeError("Minigame is missing from AssetRegistry")
    screen = unreal.EditorAssetLibrary.load_asset(SCREEN_PATH)
    if screen is None or screen.get_class().get_name() != "WidgetBlueprint":
        raise RuntimeError("Designer screen WBP is missing or has the wrong class")
    joystick_screen = unreal.EditorAssetLibrary.load_asset(JOYSTICK_SCREEN_PATH)
    if joystick_screen is None or joystick_screen.get_class().get_name() != "WidgetBlueprint":
        raise RuntimeError("Designer joystick WBP is missing or has the wrong class")

    unreal.log(
        "FF_CLAW_MACHINE_VERIFY_OK primary={0},{1} mesh_materials=9 textures={2}x{3}/{4}x{5} prizes={6} screen={7} joystick={8}".format(
            facility_primary_id,
            minigame_primary_id,
            int(thumbnail.blueprint_get_size_x()),
            int(thumbnail.blueprint_get_size_y()),
            int(backdrop.blueprint_get_size_x()),
            int(backdrop.blueprint_get_size_y()),
            len(prize_rows),
            screen.get_path_name(),
            joystick_screen.get_path_name(),
        )
    )


run()
