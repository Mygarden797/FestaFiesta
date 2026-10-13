"""Read-only crowd cook manifest. Run via UnrealEditor-Cmd -run=pythonscript.

Pass -FFCrowdRequirementsOutput=<absolute JSON path>. No assets are saved,
actors spawned, animation ticks performed, or automation tests executed.
"""
import hashlib
import json
from pathlib import Path
import unreal

RUNTIME_ANIMATION_ROOT = '/Game/Level/Demo_Matereal/Mesh/anim/NPC/Anim'
ANIMATION_TYPES = {'AnimBlueprint', 'AnimSequence', 'BlendSpace', 'BlendSpace1D'}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def object_path(value):
    text = str(value or '').strip().strip('"')
    if text in ('', 'None'):
        return ''
    if "'" in text:
        text = text.split("'", 2)[1]
    require(text.startswith('/') and ':' not in text, 'Invalid asset reference: ' + text)
    return text


def package_path(value):
    return object_path(value).split('.', 1)[0]


def tuple_items(text):
    """Split native FProperty export tuples; nested structs and quotes stay intact."""
    text = str(text).strip()
    if not text:
        return []
    require(text.startswith('(') and text.endswith(')'), 'Invalid property tuple: ' + text)
    text, depth, quote, start, parts = text[1:-1], 0, '', 0, []
    for index, char in enumerate(text):
        if quote:
            if char == quote and (index == 0 or text[index - 1] != '\\'):
                quote = ''
        elif char in ('"', "'"):
            quote = char
        elif char == '(':
            depth += 1
        elif char == ')':
            depth -= 1
            require(depth >= 0, 'Unbalanced property tuple')
        elif char == ',' and depth == 0:
            parts.append(text[start:index].strip())
            start = index + 1
    require(depth == 0 and not quote, 'Unbalanced property tuple')
    if text[start:].strip():
        parts.append(text[start:].strip())
    return parts


def appearance_rows(table):
    # UE 5.7 JSON export crashes for arrays of native structs. These official
    # column APIs use legacy FProperty text export and the same RowMap order.
    names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    columns = {key: list(unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, key))
               for key in ('Mesh', 'AnimClass', 'MeshVariants', 'AnimVariants')}
    require(names and all(len(values) == len(names) for values in columns.values()),
            'Crowd appearance column/row count mismatch')
    rows = []
    for index, name in enumerate(names):
        row = {key: values[index] for key, values in columns.items()}
        row['Name'] = str(name)
        row['MeshVariants'] = tuple_items(row['MeshVariants'])
        variants = []
        for entry in tuple_items(row['AnimVariants']):
            fields = dict(field.split('=', 1) for field in tuple_items(entry))
            require('Mesh' in fields and 'AnimClass' in fields, 'Invalid animation variant: ' + entry)
            variants.append(fields)
        row['AnimVariants'] = variants
        rows.append(row)
    return rows


def run():
    _, _, params = unreal.SystemLibrary.parse_command_line(unreal.SystemLibrary.get_command_line())
    output_arg = next((str(v) for k, v in params.items()
                       if str(k).lstrip('-').lower() == 'ffcrowdrequirementsoutput'), '')
    require(output_arg, 'Required argument: -FFCrowdRequirementsOutput=<absolute JSON path>')
    output = Path(output_arg)
    require(output.is_absolute(), 'Manifest output path must be absolute')
    # Remove stale success output before a failed inspection can be consumed.
    output.unlink(missing_ok=True)
    settings_class = unreal.load_class(None, '/Script/FestaFiesta.FF_SimulationSettings')
    require(settings_class, 'FF_SimulationSettings class could not be loaded')
    settings = unreal.get_default_object(settings_class)
    table = settings.get_editor_property('crowd_appearance_table')
    require(isinstance(table, unreal.DataTable), 'Configured CrowdAppearanceTable could not be loaded')
    source = package_path(table.get_path_name())
    require(source.startswith('/Game/'), 'CrowdAppearanceTable must be project content')
    table_file = Path(unreal.Paths.project_content_dir()) / (source[len('/Game/'):] + '.uasset')
    require(table_file.is_file(), 'CrowdAppearanceTable source file is missing: ' + str(table_file))
    rows = appearance_rows(table)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(synchronous_search=True)
    options = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True, include_hard_package_references=True,
        include_game_package_references=True, include_editor_only_package_references=False,
        include_searchable_names=False, include_soft_management_references=False,
        include_hard_management_references=False)
    required, mesh_paths, animation_classes, inspected_rows = {source}, set(), {}, []
    loaded = {}

    def asset(path, expected=None):
        path = package_path(path)
        if path not in loaded:
            loaded[path] = unreal.load_asset(path)
        value = loaded[path]
        require(value is not None, 'Referenced asset could not be loaded: ' + path)
        require(expected is None or isinstance(value, expected), 'Unexpected asset class: ' + path)
        required.add(path)
        return value

    crowd_anim_base = unreal.load_class(None, '/Script/FestaFiesta.FF_CrowdAnimInstance')
    require(crowd_anim_base, 'FF_CrowdAnimInstance class could not be loaded')
    for row in sorted(rows, key=lambda item: item['Name']):
        row_anim = object_path(row.get('AnimClass'))
        candidates = [(row.get('Mesh'), row_anim, 'Mesh')]
        candidates += [(mesh, row_anim, 'MeshVariants') for mesh in row.get('MeshVariants', [])]
        candidates += [(entry.get('Mesh'), object_path(entry.get('AnimClass')) or row_anim, 'AnimVariants')
                       for entry in row.get('AnimVariants', [])]
        result = {'name': row['Name'], 'candidates': []}
        for mesh_ref, anim_ref, field in candidates:
            if not object_path(mesh_ref):
                continue
            require(anim_ref, 'Mesh candidate has no explicit AnimClass: ' + row['Name'] + '/' + field)
            mesh = asset(mesh_ref, unreal.SkeletalMesh)
            mesh_package, anim_package = package_path(mesh_ref), package_path(anim_ref)
            mesh_paths.add(mesh_package)
            if anim_package not in animation_classes:
                anim_class = unreal.load_class(None, anim_ref, type=crowd_anim_base)
                require(anim_class, 'Crowd AnimClass could not be loaded: ' + anim_ref)
                # UAnimBlueprintGeneratedClass.TargetSkeleton is a UPROPERTY in UE 5.7.
                # Read the source AnimBP if that generated-class field is not exposed.
                try:
                    skeleton = anim_class.get_editor_property('target_skeleton')
                except Exception:
                    skeleton = asset(anim_package, unreal.AnimBlueprint).get_editor_property('target_skeleton')
                require(skeleton, 'AnimClass has no target skeleton: ' + anim_ref)
                animation_classes[anim_package] = (anim_class, skeleton)
            _, skeleton = animation_classes[anim_package]
            require(mesh.get_editor_property('skeleton') == skeleton,
                    'Mesh/AnimClass skeleton mismatch: ' + row['Name'] + ': ' + str(mesh_ref) + ' / ' + anim_ref)
            required.update((anim_package, package_path(skeleton.get_path_name())))
            result['candidates'].append({'field': field, 'mesh': mesh_package,
                                         'animationBlueprint': anim_package,
                                         'skeleton': package_path(skeleton.get_path_name())})
        require(result['candidates'], 'Crowd row has no mesh candidates: ' + row['Name'])
        inspected_rows.append(result)

    # Dedicated generated crowd animation inventory guards against stale on-disk
    # registry edges. Only runtime animation types are roots, never retarget tools.
    registry.scan_paths_synchronous([RUNTIME_ANIMATION_ROOT], force_rescan=True)
    inventory = registry.get_assets_by_path(RUNTIME_ANIMATION_ROOT, recursive=True,
                                             include_only_on_disk_assets=True)
    runtime_inventory = {str(item.package_name) for item in inventory
                         if str(item.asset_class_path.asset_name) in ANIMATION_TYPES}
    require(runtime_inventory, 'Runtime crowd animation inventory is empty: ' + str([
        (str(item.package_name), str(item.asset_class_path)) for item in inventory[:5]]))
    for path in sorted(runtime_inventory):
        asset(path)
    # Engine content follows its normal cooker dependencies. This delivery gate
    # inventories project packages, whose source paths and IoStore paths map 1:1.
    required = {path for path in required if path.startswith('/Game/')}
    pending, visited = list(required), set()
    while pending:
        current = pending.pop()
        if current in visited or not current.startswith('/Game/'):
            continue
        visited.add(current)
        records = registry.get_assets_by_package_name(current, include_only_on_disk_assets=True)
        require(records, 'Runtime dependency has no on-disk asset: ' + current)
        required.add(current)
        # Game-only references omit editor source, preview, retarget and graph data.
        for dependency in registry.get_dependencies(current, options) or []:
            dependency = str(dependency)
            if dependency.startswith('/Game/'):
                pending.append(dependency)
    classes = {}
    for path in sorted(required):
        classes[path] = sorted({str(item.asset_class_path.asset_name) for item in
                               registry.get_assets_by_package_name(path, include_only_on_disk_assets=True)})
    count = lambda types: sum(bool(set(names) & types) for names in classes.values())
    manifest = {'schemaVersion': 1, 'status': 'passed', 'sourceTable': source,
                'sourceTableSha256': hashlib.sha256(table_file.read_bytes()).hexdigest(),
                'runtimeAnimationRoot': RUNTIME_ANIMATION_ROOT, 'dependencyScope': '/Game/',
                'rows': inspected_rows,
                'rowCount': len(inspected_rows), 'meshCount': len(mesh_paths),
                'animationBlueprintCount': count({'AnimBlueprint'}),
                'animationSequenceCount': count({'AnimSequence'}),
                'blendSpaceCount': count({'BlendSpace', 'BlendSpace1D'}),
                'requiredAssets': sorted(required), 'requiredAssetCount': len(required),
                'runtimeAnimationInventory': sorted(runtime_inventory)}
    require(manifest['animationBlueprintCount'] > 0 and manifest['animationSequenceCount'] > 0,
            'No runtime crowd animation blueprints or sequences were collected')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    unreal.log('FF_CROWD_COOK_REQUIREMENTS_PASS: ' + str(output))


if __name__ == '__main__':
    run()
