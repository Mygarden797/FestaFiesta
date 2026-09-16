"""Import Blender surfaces and connect six Roads and six Tiles to the catalog.

Run with compiled Road and Tile categories through Run-RoadEditor.ps1. The only shared
package changed is DT_FacilityCosts; existing prices and unrelated rows survive.
All package writes use the project's recoverable save transaction.
"""
import csv
import hashlib
import json
import sys
import traceback
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'SourceArt/Roads_20260910'
OUT = ROOT / 'Saved/Roads_20260910'
BASE = '/Game/Asset/FacilitiesAsset_HKH/Roads'
DEFINITIONS = '/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Road'
COST_TABLE = '/Game/FestaFiesta/Data/Economy/DT_FacilityCosts'
ROADS = (
    ('Sand', '모래길'), ('Asphalt', '아스팔트길'), ('Wood', '나무 데크길'),
    ('Stone', '석재 타일길'), ('Herringbone', '헤링본 벽돌길'), ('Concrete', '콘크리트길'),
)
CALIBRATION = (
    ('SM_Road_Calibration_1m', [0, 0, 50], [100, 100, 100]),
    ('SM_Road_Calibration_X', [200, 0, 50], [75, 25, 25]),
    ('SM_Road_Calibration_Y', [0, -300, 50], [25, 100, 25]),
    ('SM_Road_Calibration_Z', [0, 0, 400], [25, 25, 125]),
)
sys.path.insert(0, str(ROOT / 'Tools/facilities'))
from ffzs_asset_save_transaction import AssetSaveTransaction


def definition_path(name):
    return DEFINITIONS + '/PDA_FFZS_Road_Surface_' + name


def thumbnail_path(name):
    return DEFINITIONS + '/Thumbnails/T_UI_Road_' + name


def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def fbx_options():
    options = unreal.FbxImportUI()
    for key, value in dict(automated_import_should_detect_type=False, import_mesh=True,
                           import_as_skeletal=False, import_materials=False, import_textures=False,
                           mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH).items():
        options.set_editor_property(key, value)
    data = options.get_editor_property('static_mesh_import_data')
    for key, value in dict(combine_meshes=True, generate_lightmap_u_vs=False,
                           auto_generate_collision=False, import_mesh_lods=False,
                           convert_scene=True, convert_scene_unit=True, build_nanite=False,
                           transform_vertex_to_absolute=True, import_uniform_scale=1.0,
                           normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS).items():
        data.set_editor_property(key, value)
    return options


def import_asset(source, path, expected_class):
    folder, name = path.rsplit('/', 1)
    task = unreal.AssetImportTask()
    for key, value in dict(filename=str(source), destination_path=folder, destination_name=name,
                           automated=True, replace_existing=True, replace_existing_settings=True,
                           save=False).items():
        task.set_editor_property(key, value)
    if expected_class == unreal.StaticMesh:
        task.set_editor_property('options', fbx_options())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = [str(value) for value in task.get_editor_property('imported_object_paths')]
    assert imported == [path + '.' + name], (path, imported)
    asset = unreal.load_asset(path)
    assert isinstance(asset, expected_class), path
    unreal.EditorAssetLibrary.set_metadata_tag(asset, 'FestaFiesta.RoadSource', str(source.relative_to(ROOT)))
    return asset


def create_material(texture):
    path = BASE + '/Materials/M_Road_Palette'
    material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if material:
        assert isinstance(material, unreal.Material)
        return material
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_Road_Palette', BASE + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    assert material
    material.set_editor_property('two_sided', False)
    lib = unreal.MaterialEditingLibrary
    sample = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    sample.set_editor_property('parameter_name', 'RoadPalette')
    sample.set_editor_property('texture', texture)
    assert lib.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    for y, value, prop in ((180, .78, unreal.MaterialProperty.MP_ROUGHNESS),
                           (320, 0.0, unreal.MaterialProperty.MP_METALLIC)):
        constant = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -400, y)
        constant.set_editor_property('r', value)
        assert lib.connect_material_property(constant, '', prop)
    lib.recompile_material(material)
    save(material)
    return material


def inspect_mesh(mesh, material):
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    bounds = mesh.get_bounding_box()
    size = bounds.max - bounds.min
    assert abs(size.x - 400) < .1 and abs(size.y - 400) < .1, (mesh.get_name(), size)
    assert 0 < size.z <= 8.1 and abs(bounds.min.z) < .1, (mesh.get_name(), bounds)
    assert abs(bounds.min.x + bounds.max.x) < .1 and abs(bounds.min.y + bounds.max.y) < .1
    assert sub.get_lod_count(mesh) == 3, mesh.get_name()
    lods = []
    for index in range(3):
        triangles = int(mesh.get_num_triangles(index))
        uvs = int(sub.get_num_uv_channels(mesh, index))
        assert triangles > 0 and uvs >= 2 and mesh.get_num_sections(index) == 1, (mesh.get_name(), index)
        lods.append(dict(lod=index, triangles=triangles, uv_channels=uvs))
    assert lods[0]['triangles'] >= lods[1]['triangles'] >= lods[2]['triangles'], lods
    # UE counts convex UCX hulls separately from box/sphere/capsule primitives.
    assert sub.get_convex_collision_count(mesh) == 1, mesh.get_name()
    assert sub.get_simple_collision_count(mesh) == 0, mesh.get_name()
    assert not sub.get_nanite_settings(mesh).get_editor_property('enabled')
    slots = mesh.get_editor_property('static_materials')
    assert len(slots) == 1 and slots[0].get_editor_property('material_interface') == material
    return dict(mesh=mesh.get_path_name(), size_cm=[size.x, size.y, size.z], lods=lods,
                convex_collision_count=1, primitive_collision_count=0, material=material.get_path_name())


def inspect_calibration(mesh, expected_center, expected_size):
    bounds = mesh.get_bounding_box()
    center = (bounds.min + bounds.max) * .5
    size = bounds.max - bounds.min
    center_values, size_values = [center.x, center.y, center.z], [size.x, size.y, size.z]
    assert all(abs(a - b) < .1 for a, b in zip(center_values, expected_center)), (mesh.get_name(), center_values, expected_center)
    assert all(abs(a - b) < .1 for a, b in zip(size_values, expected_size)), (mesh.get_name(), size_values, expected_size)
    return dict(asset=mesh.get_path_name(), center_cm=center_values, size_cm=size_values, status='PASS')


def create_definition(name, label, mesh, thumbnail):
    path = definition_path(name)
    definition = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not definition:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.load_class(None, '/Script/FF_ZoneSystem.FFZSFacilityDefinition'))
        definition = unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/', 1)[1], DEFINITIONS, None, factory)
        assert definition
        spec = definition.get_editor_property('spec')
        for key, value in dict(facility_definition_id='Road_Surface_' + name, display_name=unreal.Text(label),
                               category=unreal.FFZSFacilityCategory.ROAD, footprint_size_cells=unreal.IntPoint(400, 400),
                               bAllowFacilityPlacementOnTop=False,
                               clearance_cells=0, placement_obstacle_height_cm=8.0,
                               visual_mesh=mesh, visual_sizing_mode=unreal.FFZSVisualSizingMode.MATCH_PLACEMENT_BOUNDS,
                               visual_rotation=unreal.Rotator(pitch=0, yaw=0, roll=0), uses_facility_levels=False,
                               upgrade_tiers=[], queue_reserved_rects=[], queue_reserved_cells=[],
                               queue_spline_points_cm=[], anchors=[]).items():
            spec.set_editor_property(key, value)
        rules = spec.get_editor_property('queue_rules')
        rules.set_editor_property('max_waiting_agents', 0)
        rules.set_editor_property('service_capacity', 0)
        spec.set_editor_property('queue_rules', rules)
        definition.set_editor_property('spec', spec)
        definition.set_editor_property('show_in_build_catalog', True)
        # Input policy belongs to the Definition. Reimport must retain later author choices.
        definition.set_editor_property('enable_drag_placement', True)
    # The category now owns this policy. Never reintroduce permissive legacy Roads.
    spec = definition.get_editor_property('spec')
    spec.set_editor_property('bAllowFacilityPlacementOnTop', False)
    definition.set_editor_property('spec', spec)
    definition.set_editor_property('catalog_thumbnail', thumbnail)
    errors = definition.is_definition_valid()
    assert errors is not None and len(errors) == 0, (name, errors)
    save(definition)
    return definition


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    assert hasattr(unreal.FFZSFacilityCategory, 'TILE'), 'Compile the Road/Tile categories before importing'
    from split_road_tiles import run as split_surfaces, tile_definition_path
    from enable_drag_placement import require_isolated_editor
    require_isolated_editor()
    sources = [SOURCE / 'textures/T_Road_Palette.png']
    sources += [SOURCE / 'export' / ('SM_Road_' + name + suffix + '.fbx') for name, _ in ROADS for suffix in ('', '_LOD1', '_LOD2')]
    sources += [SOURCE / 'qa' / ('SM_Road_' + name + '_Hero.png') for name, _ in ROADS]
    sources += [SOURCE / 'qa/calibration' / (name + '.fbx') for name, _, _ in CALIBRATION]
    assert all(path.is_file() for path in sources), [str(path) for path in sources if not path.is_file()]
    packages = [BASE + '/Materials/T_Road_Palette', BASE + '/Materials/M_Road_Palette', COST_TABLE]
    packages += [BASE + '/SM_Road_' + name for name, _ in ROADS]
    packages += [definition_path(name) for name, _ in ROADS] + [thumbnail_path(name) for name, _ in ROADS]
    packages += [tile_definition_path(name) for name, _ in ROADS]
    packages += [BASE + '/QA/Calibration/' + name for name, _, _ in CALIBRATION]
    transaction = AssetSaveTransaction(packages, 'RoadFacilitiesImport')
    try:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX.ToLevel 0')
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        texture = import_asset(sources[0], BASE + '/Materials/T_Road_Palette', unreal.Texture2D)
        for key, value in dict(srgb=True, filter=unreal.TextureFilter.TF_NEAREST,
                               mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                               compression_settings=unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP,
                               address_x=unreal.TextureAddress.TA_CLAMP, address_y=unreal.TextureAddress.TA_CLAMP).items():
            texture.set_editor_property(key, value)
        save(texture)
        material = create_material(texture)
        calibration = []
        for name, expected_center, expected_size in CALIBRATION:
            witness = import_asset(SOURCE / 'qa/calibration' / (name + '.fbx'), BASE + '/QA/Calibration/' + name, unreal.StaticMesh)
            witness.set_material(0, material)
            calibration.append(inspect_calibration(witness, expected_center, expected_size))
            save(witness)
        records = []
        for name, label in ROADS:
            mesh = import_asset(SOURCE / 'export' / ('SM_Road_' + name + '.fbx'), BASE + '/SM_Road_' + name, unreal.StaticMesh)
            for index in (1, 2):
                assert sub.import_lod(mesh, index, str(SOURCE / 'export' / ('SM_Road_' + name + '_LOD' + str(index) + '.fbx'))) == index
            mesh.set_material(0, material)
            mesh.set_editor_property('light_map_coordinate_index', 1)
            mesh.set_editor_property('light_map_resolution', 64)
            for index in range(3):
                settings = sub.get_lod_build_settings(mesh, index)
                settings.set_editor_property('generate_lightmap_u_vs', False)
                settings.set_editor_property('use_full_precision_u_vs', True)
                sub.set_lod_build_settings(mesh, index, settings)
            # UE 5.7 exposes the subsystem setter, not bAutoComputeLODScreenSize.
            # Apply after build settings so mesh rebuilds retain the explicit sizes.
            assert sub.set_lod_screen_sizes(mesh, [1.0, .35, .12])
            mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
            unreal.EditorAssetLibrary.set_metadata_tag(mesh, 'FestaFiesta.RoadContract', '400x400x8cm;PivotGroundCenter;Front+X;UCX;LOD0,1,2;PaletteUV0;LightmapUV1')
            record = inspect_mesh(mesh, material)
            save(mesh)
            thumbnail = import_asset(SOURCE / 'qa' / ('SM_Road_' + name + '_Hero.png'), thumbnail_path(name), unreal.Texture2D)
            for key, value in dict(srgb=True, compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,
                                   mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                                   lod_group=unreal.TextureGroup.TEXTUREGROUP_UI, never_stream=True).items():
                thumbnail.set_editor_property(key, value)
            save(thumbnail)
            definition = create_definition(name, label, mesh, thumbnail)
            records.append(dict(name=name, label=label, definition=definition.get_path_name(), thumbnail=thumbnail.get_path_name(), **record))
        table = unreal.load_asset(COST_TABLE)
        assert table
        costs = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
        existing = {row['Name'] for row in costs}
        source_costs = csv.DictReader((ROOT / 'data/unreal/facility_costs_v0.csv').read_text(encoding='utf-8-sig').splitlines())
        additions = [dict(Name=row['Name'], InstallCost=int(row['InstallCost'])) for row in source_costs
                     if row['Name'].startswith('Cost.Road.Surface.') and row['Name'] not in existing]
        if additions:
            assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(costs + additions))
            save(table)
        # The outer import transaction also snapshots Tile packages. Standalone
        # migration uses its own transaction and can safely run without FBX import.
        split_report = split_surfaces()
        transaction.commit()
        report = dict(status='PASS', roads=records, tiles=split_report['surfaces'], added_cost_rows=additions, calibration=calibration,
                      axis_contract=dict(forward='+X', up='+Z', source_y_to_unreal_y=-1, unit_scale='1m=100cm'),
                      source_sha256={str(path.relative_to(SOURCE)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources})
        (OUT / 'import_report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        unreal.log('ROAD_IMPORT_PASS')
    except Exception:
        transaction.rollback()
        (OUT / 'import_error.txt').write_text(traceback.format_exc(), encoding='utf-8')
        raise


if __name__ == '__main__':
    main()
