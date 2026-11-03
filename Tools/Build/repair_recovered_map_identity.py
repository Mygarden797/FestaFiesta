"""Repair only the externally renamed /Game/_Recover/TM_Mine map in UE 5.7.4.

Run in a fresh UnrealEditor-Cmd process with -run=pythonscript -script=<file>.
Default: inspect without saving. FF_REPAIR_RECOVER_MAP=1 enables the one-time
repair of the pinned original bytes. Run inspect again in another fresh process
to verify every on-disk AssetData and the saved actor baseline. This never saves
other assets, creates redirects, or changes validation/cook exclusions.
"""
import hashlib
import json
import os
import shutil
import stat
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal

PACKAGE = '/Game/_Recover/TM_Mine'
ORIGINAL_SHA256 = '0b471f2fc09983c8975bfa92655364ae6cf259c58778be328334952fc7737ea0'
SIDECARS = ('.umap', '.uexp', '.ubulk', '.uptnl')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def write_json(path, value):
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    os.replace(temporary, path)


def actor_snapshot():
    rows = {}
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        value = actor.get_actor_transform()
        parent = actor.get_attach_parent_actor()
        guid = actor.get_editor_property('ActorGuid')
        rows[actor.get_path_name()] = {
            'class': actor.get_class().get_path_name(),
            'guid': guid.to_string(),
            'label': actor.get_actor_label(),
            'folder': str(actor.get_folder_path()),
            'parent': parent.get_path_name() if parent else None,
            'location': [round(float(x), 5) for x in (value.translation.x, value.translation.y, value.translation.z)],
            'scale': [round(float(x), 5) for x in (value.scale3d.x, value.scale3d.y, value.scale3d.z)],
            'rotation': [round(float(x), 7) for x in (value.rotation.x, value.rotation.y, value.rotation.z, value.rotation.w)],
        }
    require(rows, 'The recovered map unexpectedly contains no actors.')
    return rows


def disk_assets(registry):
    return list(registry.get_assets_by_package_name(PACKAGE, include_only_on_disk_assets=True))


def describe_assets(records, load=False):
    rows = []
    for data in records:
        class_path = data.asset_class_path
        row = {'path': str(data.package_name) + '.' + str(data.asset_name),
               'class': str(class_path.package_name) + '.' + str(class_path.asset_name)}
        if load:
            obj = data.get_asset()
            row['loaded_path'] = obj.get_path_name() if obj else None
            row['loaded'] = obj is not None
        rows.append(row)
    return rows


def run():
    root = Path(unreal.Paths.project_dir()).resolve()
    require((root / 'FestaFiesta.uproject').is_file(), 'Wrong project.')
    source = root / 'Content/_Recover/TM_Mine.umap'
    require(source.is_file(), 'The exact recovered map is missing.')
    # Refuse junctions/symlinks before any asset or backup mutation.
    for path in (source, source.parent, source.parent.parent):
        attributes = getattr(path.stat(), 'st_file_attributes', 0)
        require(not (attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT), 'The map path must not use reparse points.')
    files = {source.with_suffix(extension) for extension in SIDECARS}
    require({path for path in files if path.exists()} == {source}, 'Unexpected map sidecars require a separate review.')
    for relative in ('__ExternalActors__/_Recover/TM_Mine', '__ExternalObjects__/_Recover/TM_Mine'):
        external = root / 'Content' / relative
        require(not external.exists(), 'External map packages require a separate review.')
    apply = os.environ.get('FF_REPAIR_RECOVER_MAP') == '1'
    engine = unreal.SystemLibrary.get_engine_version()
    require(engine.startswith('5.7.4-'), 'This repair was reviewed for UE 5.7.4 only.')
    artifacts = root / 'Artifacts/RecoveredMapRepair'
    directory = artifacts / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
    directory.mkdir(parents=True, exist_ok=False)
    report_path = directory / 'report.json'
    latest_path = artifacts / 'latest-applied.json'
    before_hash = digest(source)
    report = {'mode': 'apply' if apply else 'inspect', 'package': PACKAGE, 'engine': engine,
              'source': str(source), 'original_sha256': ORIGINAL_SHA256,
              'before_sha256': before_hash, 'saved': False, 'fresh_process_verification_required': True}
    backup = None
    save_started = False
    try:
        if apply:
            require(before_hash == ORIGINAL_SHA256, 'Apply requires the known original LFS bytes; inspect any newer map instead.')
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        registry.search_all_assets(synchronous_search=True)
        registry.scan_files_synchronous([str(source)], force_rescan=True)
        records = disk_assets(registry)
        require(records, 'The map has no on-disk AssetData.')
        report['assets_before'] = describe_assets(records)
        options = unreal.AssetRegistryDependencyOptions(
            include_soft_package_references=True, include_hard_package_references=True,
            include_game_package_references=True, include_editor_only_package_references=True,
            include_searchable_names=True, include_soft_management_references=True,
            include_hard_management_references=True)
        referencers = sorted(str(x) for x in registry.get_referencers(PACKAGE, options) if str(x) != PACKAGE)
        report['external_referencers'] = referencers
        if apply:
            require(not referencers, 'Referenced recovery content requires a separate object-reference review.')
        # SaveMap also saves an externally referenced MapBuildData package.
        # Inspect serialized package dependencies through the public registry API;
        # ULevel.MapBuildData is intentionally not exposed to Python.
        dependencies = sorted(str(x) for x in registry.get_dependencies(PACKAGE, options))
        external_build_data = []
        for dependency in dependencies:
            if dependency != PACKAGE:
                for row in describe_assets(registry.get_assets_by_package_name(dependency, include_only_on_disk_assets=True)):
                    if row['class'] == '/Script/Engine.MapBuildDataRegistry':
                        external_build_data.append(row['path'])
        report['external_map_build_data_dependencies'] = external_build_data
        require(not external_build_data, 'SaveMap would also save an external MapBuildData package.')
        world = unreal.EditorLoadingAndSavingUtils.load_map(PACKAGE)
        require(world is not None and world.get_path_name() == PACKAGE + '.TM_Mine', 'The exact recovered world did not load with its canonical name.')
        require(unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world), 'World compilation/physics preparation did not complete.')
        # UWorld.PersistentLevel is protected in Python. Resolve the existing
        # child through the supported object lookup, without changing access.
        level = unreal.find_object(world, 'PersistentLevel', unreal.Level)
        require(level is not None and level.get_outer() == world, 'The loaded world has no persistent level.')
        require(not level.get_editor_property('bUseExternalActors'), 'External actor saves are outside this repair.')
        before = actor_snapshot()
        report.update(world=world.get_path_name(), actor_count_before=len(before), actors_before=before)
        if apply:
            backup = directory / 'Backups/Content/_Recover/TM_Mine.umap'
            backup.parent.mkdir(parents=True, exist_ok=False)
            shutil.copy2(source, backup)
            require(digest(backup) == before_hash, 'The original-map backup hash did not match.')
            report['backup'] = str(backup)
            # UWorld::PostLoad already reconciled the externally renamed world's
            # in-memory name. Saving this exact map updates its serialized exports
            # and AssetRegistry entry; no actor or level setting is changed here.
            save_started = True
            require(unreal.EditorLoadingAndSavingUtils.save_map(world, PACKAGE), 'The exact recovered map could not be saved.')
            require(actor_snapshot() == before, 'SaveMap changed actor identity, labels, attachments, folders or transforms.')
            require({path for path in files if path.exists()} == {source}, 'SaveMap created unexpected sidecars.')
            report['saved'] = True
            registry.scan_files_synchronous([str(source)], force_rescan=True)
            records = disk_assets(registry)
        report['assets_after'] = describe_assets(records, load=True)
        report['all_assets_load'] = bool(records) and all(row['loaded'] for row in report['assets_after'])
        report['canonical_world_in_registry'] = any(row['path'] == PACKAGE + '.TM_Mine' and
            row['class'] == '/Script/Engine.World' for row in report['assets_after'])
        report['actor_count_after'] = len(actor_snapshot())
        report['actors_unchanged'] = actor_snapshot() == before
        report['after_sha256'] = digest(source)
        if apply:
            require(report['all_assets_load'] and report['canonical_world_in_registry'], 'Saved on-disk AssetData still has an invalid object path.')
            require(report['after_sha256'] != before_hash, 'SaveMap did not update the original package metadata.')
        else:
            require(report['after_sha256'] == before_hash, 'Inspection changed the map bytes.')
            if latest_path.is_file():
                latest = json.loads(latest_path.read_text(encoding='utf-8'))
                baseline_path = Path(latest['report']).resolve()
                require(artifacts.resolve() in baseline_path.parents, 'The baseline report is outside the repair artifacts.')
                baseline = json.loads(baseline_path.read_text(encoding='utf-8'))
                require(baseline['package'] == PACKAGE and baseline['after_sha256'] == before_hash, 'The repaired map does not match its saved receipt.')
                require(before == baseline['actors_before'], 'Fresh loading changed the repaired map actor baseline.')
                require(report['all_assets_load'] and report['canonical_world_in_registry'], 'Fresh process could not resolve all repaired AssetData.')
                report['fresh_process_verification_required'] = False
                report['saved_baseline_verified'] = True
        report['status'] = 'applied' if apply else 'inspection-complete'
        write_json(report_path, report)
        if apply:
            write_json(latest_path, {'package': PACKAGE, 'report': str(report_path), 'after_sha256': report['after_sha256']})
        unreal.log('FF_RECOVER_MAP_' + report['status'].upper() + ': ' + str(report_path))
    except Exception:
        report.update(status='failed', error=traceback.format_exc())
        if save_started and backup is not None:
            try:
                # Restore only the pinned map and remove only its four explicitly
                # named sidecars. The fresh commandlet must exit after rollback.
                shutil.copy2(backup, source)
                for path in files - {source}:
                    if path.exists():
                        require(not path.is_symlink(), 'Refusing to remove a linked sidecar during rollback.')
                        path.unlink()
                require(digest(source) == before_hash, 'Rollback did not restore the original map hash.')
                report['rollback_verified'] = True
                report['saved'] = False
            except Exception:
                report['rollback_error'] = traceback.format_exc()
        report['after_sha256'] = digest(source)
        write_json(report_path, report)
        unreal.log_error('FF_RECOVER_MAP_FAILED: ' + str(report_path))
        raise


run()
