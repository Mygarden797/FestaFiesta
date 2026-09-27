# repair_packaging_animations.py
# 역할: Windows 빌드·패키징·검증 작업에서 확인된 데이터 오류만 수정한다.

"""Repair three known missing animation skeleton references after bone checks.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>. The default is
read-only. Set FESTAFIESTA_APPLY_ANIMATION_REPAIR=1 to apply after inspection.
Requires the editor-only FF.Packaging.RestoreAnimationSkeleton console command.
No pose-space conversion, asset replacement, or skeleton creation is performed. Existing
non-null skeleton assignments other than the expected target are rejected.
"""
import hashlib
import json
import os
import shutil
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
APPLY = os.environ.get('FESTAFIESTA_APPLY_ANIMATION_REPAIR') == '1'
RUN_ID = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
OUT = ROOT / 'Artifacts/AnimationSkeletonRepair' / RUN_ID
CROWD = '/Game/FestaFiesta/Character/Crowd/'
PLANS = (
    ('50_Man/Breathing_Idle', '50_Man/50_Man', '50_Man/50_Man_',
     '50_Man/Walk_With_Briefcasemixamo_com'),
    ('50_Man/Walk_With_BriefcaseTake_001', '50_Man/50_Man', '50_Man/50_Man_',
     '50_Man/Walk_With_Briefcasemixamo_com'),
    ('Elder_Woman/Idle__2_Take_001', 'Elder_Woman/Elder_woman', 'Elder_Woman/Elder_woman_',
     'Elder_Woman/Idle__2_mixamo_com'),
)
REPORT = {'mode': 'apply' if APPLY else 'inspect', 'assets': [], 'loadWarnings': [],
          'status': 'running', 'reportDirectory': str(OUT)}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load_asset(path, expected_class):
    try:
        asset = unreal.load_asset(path)
    except RuntimeError as error:
        # Missing-skeleton postload emits an error while creating the source
        # data model. The object remains loaded; a second lookup can inspect it.
        REPORT['loadWarnings'].append({'path': path, 'message': str(error)})
        asset = unreal.load_asset(path)
    require(isinstance(asset, expected_class), 'Unexpected or missing asset: ' + path)
    return asset


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def animation_state(animation):
    return {
        # Bone keys are FNames: compare their case-insensitive identity, not
        # whichever display spelling was first loaded into the editor process.
        'tracks': [str(name).casefold() for name in unreal.AnimationLibrary.get_animation_track_names(animation)],
        'keyCount': unreal.AnimationLibrary.get_num_keys(animation),
        'lengthSeconds': animation.get_play_length(),
    }


def mesh_state(mesh):
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    names = [str(component.get_bone_name(index)) for index in range(component.get_num_bones())]
    return {'bones': names, 'parents': {name: str(component.get_parent_bone(name)) for name in names}}


def asset_file(package_path):
    require(package_path.startswith('/Game/'), 'Asset outside project content')
    path = (ROOT / 'Content' / (package_path[len('/Game/'):] + '.uasset')).resolve()
    require(path.is_relative_to((ROOT / 'Content').resolve()), 'Asset escaped Content root')
    require(path.is_file(), 'Source asset file missing: ' + str(path))
    return path


def backup(package_path):
    source = asset_file(package_path)
    records = []
    for suffix in ('.uasset', '.uexp', '.ubulk'):
        sidecar = source.with_suffix(suffix)
        if not sidecar.exists():
            continue
        target = OUT / 'Backups' / sidecar.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        require(not target.exists(), 'Refusing to overwrite an existing backup: ' + str(target))
        shutil.copy2(sidecar, target)
        digest = sha256(sidecar)
        require(sha256(target) == digest, 'Backup verification failed: ' + str(target))
        records.append({'source': str(sidecar), 'backup': str(target), 'sha256': digest})
    return records


def run():
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT, 'Wrong Unreal project')
    OUT.mkdir(parents=True, exist_ok=False)
    pending = []
    # Inspect all three assets before changing any of them. Folder names merely
    # select candidates: actual mesh assignment, a working animation reference,
    # and exact ordered bone-track agreement are the required evidence.
    for animation_name, skeleton_name, mesh_name, reference_name in PLANS:
        animation_path, skeleton_path, mesh_path, reference_path = [
            CROWD + name for name in (animation_name, skeleton_name, mesh_name, reference_name)]
        skeleton = load_asset(skeleton_path, unreal.Skeleton)
        mesh = load_asset(mesh_path, unreal.SkeletalMesh)
        reference = load_asset(reference_path, unreal.AnimSequence)
        animation = load_asset(animation_path, unreal.AnimSequence)
        require(mesh.get_editor_property('skeleton') == skeleton, 'Candidate mesh uses another skeleton')
        require(reference.get_editor_property('skeleton') == skeleton, 'Reference animation uses another skeleton')
        before = animation_state(animation)
        mesh_bones = mesh_state(mesh)
        reference_state = animation_state(reference)
        require(len(before['tracks']) == 31, 'Unexpected track count: ' + animation_path)
        require(before['tracks'] == [name.casefold() for name in mesh_bones['bones']],
                'Animation tracks do not match mesh bones: ' + animation_path)
        require(before['tracks'] == reference_state['tracks'], 'Animation tracks do not match working reference: ' + animation_path)
        require(before['keyCount'] > 0 and before['lengthSeconds'] > 0, 'Animation data is empty')
        current = animation.get_editor_property('skeleton')
        require(current is None or current == skeleton, 'Refusing to replace an existing different skeleton')
        row = {
            'animation': animation_path, 'targetSkeleton': skeleton.get_path_name(),
            'compatibleMesh': mesh_path, 'workingReferenceAnimation': reference_path,
            'meshHierarchy': mesh_bones, 'before': before,
            'existingSkeleton': current.get_path_name() if current else None,
            'sourceSha256': sha256(asset_file(animation_path)),
            'status': 'already-correct' if current == skeleton else 'compatible-missing-reference',
        }
        REPORT['assets'].append(row)
        if current is None:
            pending.append((animation, skeleton, row))

    if APPLY:
        for _, _, row in pending:
            row['backups'] = backup(row['animation'])
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        require(world is not None, 'An editor world is required for the repair command')
        for animation, skeleton, row in pending:
            command = 'FF.Packaging.RestoreAnimationSkeleton {} {}'.format(
                animation.get_path_name(), skeleton.get_path_name())
            unreal.SystemLibrary.execute_console_command(world, command)
            require(animation.get_editor_property('skeleton') == skeleton,
                    'Native skeleton repair did not succeed; check the editor build and log')
            require(animation_state(animation) == row['before'],
                    'Animation tracks/key count/duration changed; refusing to save')
            require(unreal.EditorAssetLibrary.save_loaded_asset(animation, only_if_is_dirty=False),
                    'Saving repaired animation failed: ' + row['animation'])
            require(animation_state(animation) == row['before'], 'Animation state changed while saving')
            row['after'] = animation_state(animation)
            row['resultSha256'] = sha256(asset_file(row['animation']))
            row['status'] = 'repaired'
    REPORT['status'] = 'passed'
    REPORT['changedAssetCount'] = len(pending) if APPLY else 0
    REPORT['freshProcessReadbackRequired'] = bool(APPLY and pending)


if __name__ == '__main__':
    try:
        run()
    except Exception:
        REPORT['status'] = 'failed'
        REPORT['error'] = traceback.format_exc()
        raise
    finally:
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / 'report.json').write_text(json.dumps(REPORT, indent=2), encoding='utf-8')
