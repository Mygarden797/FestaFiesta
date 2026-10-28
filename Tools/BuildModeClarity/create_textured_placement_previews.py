"""Generate isolated translucent copies of all catalog mesh materials (UE 5.7).

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> -unattended -nullrhi.
Only owned preview assets and their DefaultGame.ini mapping are saved. Original
materials, meshes and definitions are never modified. No play session is started.
"""
import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
FOLDER = '/Game/FestaFiesta/Building/PreviewMaterials'
MARKER = 'FF.TexturedPlacementPreview.Source'
LIB = unreal.MaterialEditingLibrary
REGISTRY = unreal.AssetRegistryHelpers.get_asset_registry()
REGISTRY.wait_for_completion()

sources = {}
seen_meshes = set()
definitions = REGISTRY.get_assets_by_class(
    unreal.TopLevelAssetPath('/Script/FF_ZoneSystem', 'FFZSFacilityDefinition'), True)
for asset in definitions:
    spec = asset.get_asset().get_editor_property('spec')
    meshes = [spec.get_editor_property('visual_mesh')]
    meshes.extend(tier.get_editor_property('visual_mesh') for tier in spec.get_editor_property('upgrade_tiers'))
    for mesh in meshes:
        if not mesh or mesh.get_path_name() in seen_meshes:
            continue
        seen_meshes.add(mesh.get_path_name())
        for slot in mesh.get_editor_property('static_materials'):
            material = slot.get_editor_property('material_interface')
            if material:
                sources[material.get_path_name()] = material

# All shipped catalog materials currently use individual, opaque material graphs.
# Fail before writing if new content needs an instance/attributes conversion policy.
for source_path, source in sources.items():
    assert isinstance(source, unreal.Material), 'Unsupported source type: ' + source_path
    assert source.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_OPAQUE, source_path
    assert not source.get_editor_property('use_material_attributes'), source_path

mapping = {}
created = 0
for source_path, source in sorted(sources.items()):
    suffix = hashlib.sha256(source_path.encode('utf-8')).hexdigest()[:12]
    name = 'M_Preview_' + source.get_name()[:36] + '_' + suffix
    package = FOLDER + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(package):
        ghost = unreal.load_asset(package)
        assert unreal.EditorAssetLibrary.get_metadata_tag(ghost, MARKER) == source_path, package
    else:
        ghost = unreal.EditorAssetLibrary.duplicate_asset(source_path, package)
        assert ghost, package
        ghost.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        ghost.set_editor_property('translucency_lighting_mode',
                                  unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        ghost.set_editor_property('disable_depth_test', False)
        opacity = LIB.create_material_expression(ghost, unreal.MaterialExpressionScalarParameter, -320, 640)
        opacity.set_editor_property('parameter_name', 'GhostOpacity')
        opacity.set_editor_property('default_value', 0.68)
        assert LIB.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
        LIB.recompile_material(ghost)
        unreal.EditorAssetLibrary.set_metadata_tag(ghost, MARKER, source_path)
        assert unreal.EditorAssetLibrary.save_loaded_asset(ghost), package
        created += 1
    # The drag preview renders many pending copies with one instanced component.
    # Persist this usage so packaged shaders support it without editor auto-fixes.
    if not ghost.get_editor_property('used_with_instanced_static_meshes'):
        ghost.set_editor_property('used_with_instanced_static_meshes', True)
        LIB.recompile_material(ghost)
        assert unreal.EditorAssetLibrary.save_loaded_asset(ghost), package
    assert ghost.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT, package
    assert 'GhostOpacity' in [str(name) for name in LIB.get_scalar_parameter_names(ghost)], package
    # Duplication must retain the same texture objects and all surface input nodes.
    for prop in (unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialProperty.MP_NORMAL,
                 unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialProperty.MP_METALLIC,
                 unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        before = LIB.get_material_property_input_node(source, prop)
        after = LIB.get_material_property_input_node(ghost, prop)
        assert bool(before) == bool(after), (source_path, prop)
        if before:
            assert before.get_class() == after.get_class(), (source_path, prop)
    mapping[source_path] = ghost.get_path_name()

config = ROOT / 'Config/DefaultGame.ini'
with config.open('r', encoding='utf-8-sig', newline='') as stream:
    original = stream.read()
lines = original.splitlines(keepends=True)
section = '[/Script/FF_ZoneSystem.FFZSBuildPresentationSettings]'
start = next(i for i, line in enumerate(lines) if line.rstrip('\r\n') == section) + 1
end = next((i for i in range(start, len(lines)) if lines[i].startswith('[')), len(lines))
section_lines = [line for line in lines[start:end] if not line.startswith('GeneratedPreviewMaterials=')]
# FMapProperty imports a list of (key,value) pairs; paths are quoted soft references.
map_value = '(' + ','.join('("%s","%s")' % item for item in mapping.items()) + ')'
section_lines.insert(0, 'GeneratedPreviewMaterials=' + map_value + '\n')
lines[start:end] = section_lines
cook = '+DirectoriesToAlwaysCook=(Path="' + FOLDER + '")'
if not any(line.rstrip('\r\n') == cook for line in lines):
    packaging = next(i for i, line in enumerate(lines)
                     if line.rstrip('\r\n') == '[/Script/UnrealEd.ProjectPackagingSettings]')
    lines.insert(packaging + 1, cook + '\n')
with config.open('w', encoding='utf-8', newline='') as stream:
    stream.write(''.join(lines))

report = dict(definitions=len(definitions), meshes=len(seen_meshes), materials=len(mapping),
              created=created, mapping=mapping)
output = ROOT / 'Saved/BuildModeClarity/textured_preview_materials.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('FF_TEXTURED_PREVIEW_OK definitions=%d meshes=%d materials=%d created=%d' %
           (len(definitions), len(seen_meshes), len(mapping), created))
