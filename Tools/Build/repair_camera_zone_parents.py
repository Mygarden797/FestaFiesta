"""Preserve Test_Main's authored zone coordinates when editor-only parents cook out.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>. Default: inspect.
Set FF_REPAIR_CAMERA_ZONE_PARENTS=1 to back up and save Test_Main only. This is
an asset migration, not a gameplay test. No pawn, camera, geometry or zone shape
is moved. Runtime zones use Outliner folders instead of an editor-only parent.
"""
import hashlib
import json
import os
import shutil
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal

MAP = '/Game/FestaFiesta/Maps/Test_Main'
EXPECTED_IDS = {
    'GZ_Beach01', 'GZ_Beach02', 'GZ_Beach03', 'GZ_Beach04', 'GZ_Beach05',
    'GZ_DadaYajang', 'GZ_DadaepoForest', 'GZ_FountainPirates', 'GZ_FleaMarket',
    'GZ_PublicParkingWest', 'GZ_TemporaryParkingEast', 'GZ_Operations', 'GZ_Tutorial',
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def vec(value):
    return [round(float(value.x), 5), round(float(value.y), 5), round(float(value.z), 5)]


def transform(value):
    q = value.rotation
    return {'location': vec(value.translation), 'scale': vec(value.scale3d),
            'rotation': [round(float(v), 7) for v in (q.x, q.y, q.z, q.w)]}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run():
    root = Path(unreal.Paths.project_dir()).resolve()
    require((root / 'FestaFiesta.uproject').is_file(), 'Wrong project')
    sys.path.insert(0, str(root / 'Tools/facilities'))
    from ffzs_asset_save_transaction import AssetSaveTransaction

    apply = os.environ.get('FF_REPAIR_CAMERA_ZONE_PARENTS') == '1'
    folder = root / 'Artifacts/CameraZoneParentRepair' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
    folder.mkdir(parents=True, exist_ok=False)
    report = {'mode': 'apply' if apply else 'inspect', 'saved': False, 'map': MAP}
    source = root / 'Content/FestaFiesta/Maps/Test_Main.umap'
    before_hash = digest(source)
    transaction = None
    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
        require(world is not None, 'Test_Main did not load')
        # Finish landscape/water preparation before saving a commandlet-loaded map.
        require(unreal.FFZSNavigationEditorLibrary.prepare_world_for_navigation(world),
                'World geometry preparation failed')
        actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
        zones = sorted((a for a in actors if isinstance(a, unreal.FFZSGameplayZone)), key=lambda a: a.get_name())
        require({str(a.get_editor_property('zone_id')) for a in zones} == EXPECTED_IDS and len(zones) == 13,
                'Unexpected production zone set; review the migration before applying')

        def authored():
            rows = {}
            for a in actors:
                row = {'transform': transform(a.get_actor_transform())}
                if a in zones:
                    row.update(zone_id=str(a.get_editor_property('zone_id')),
                               folder=str(a.get_folder_path()),
                               polygon_world=[vec(unreal.MathLibrary.transform_location(
                                   a.get_actor_transform(), unreal.Vector(p.x, p.y, 0)))
                                   for p in a.get_boundary_polygon_local_cm()],
                               anchors=[transform(t) for t in a.get_editor_property('entry_exit_anchors')])
                rows[a.get_path_name()] = row
            return rows

        before = authored()
        targets = []
        for zone in zones:
            parent = zone.get_attach_parent_actor()
            if parent:
                require(isinstance(parent, unreal.FFZAZoneLayoutManager),
                        'Unexpected zone parent: ' + parent.get_path_name())
                targets.append(zone)
        report.update(before_sha256=before_hash, before=before, attachments=[{
            'zone': a.get_path_name(), 'parent': a.get_attach_parent_actor().get_path_name(),
            'parent_transform': transform(a.get_attach_parent_actor().get_actor_transform())} for a in targets])
        require(not targets or len(targets) == 13, 'Partial migration requires manual review')

        if apply and targets:
            backup = folder / 'Backups/Content/FestaFiesta/Maps/Test_Main.umap'
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, backup)
            require(digest(backup) == before_hash, 'Backup hash mismatch')
            report['backup'] = str(backup)
            transaction = AssetSaveTransaction([MAP], 'CameraZoneParentRepair')
            for zone in targets:
                zone.modify()
                zone.get_editor_property('scene_root').modify()
                zone.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD,
                                       unreal.DetachmentRule.KEEP_WORLD,
                                       unreal.DetachmentRule.KEEP_WORLD)
                require(zone.get_attach_parent_actor() is None, 'Zone did not detach')
            require(authored() == before, 'Migration changed a world transform, zone polygon, anchor or folder')
            require(unreal.EditorLoadingAndSavingUtils.save_map(world, MAP), 'Map save failed')
            require(authored() == before, 'Map save changed authored world data')
            transaction.commit()
            transaction = None
            report['saved'] = True
        report.update(status='applied' if report['saved'] else 'inspection-complete',
                      after_sha256=digest(source), authored_world_data_unchanged=authored() == before,
                      remaining_parent_count=sum(a.get_attach_parent_actor() is not None for a in zones))
        if not report['saved']:
            require(digest(source) == before_hash, 'Read-only inspection changed source bytes')
    except Exception:
        report.update(status='failed', error=traceback.format_exc())
        if transaction:
            try:
                transaction.rollback()
                report['rollback_sha256'] = digest(source)
                require(report['rollback_sha256'] == before_hash, 'Rollback hash mismatch')
            except Exception:
                report['rollback_error'] = traceback.format_exc()
                raise
        raise
    finally:
        (folder / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        unreal.log('CAMERA_ZONE_PARENT_REPAIR: ' + str(folder / 'report.json'))


if __name__ == '__main__':
    run()
