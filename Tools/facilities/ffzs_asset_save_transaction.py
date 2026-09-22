# -*- coding: utf-8 -*-
# ffzs_asset_save_transaction.py
# 역할: 여러 에셋의 저장 실패 시 복구할 수 있도록 저장 작업을 관리한다.

"""Recoverable multi-package saves for FFZS Unreal Python commandlets.

Unreal's package save API is not an atomic filesystem transaction.  This
helper snapshots every target package (including sidecar files), verifies the
existing files are writable before mutation, and restores the snapshot if any
save in the batch fails.  The commandlet must terminate after rollback because
its loaded UObject state can still be dirty even though disk state is restored.
"""

import glob
import os
import shutil
import stat
import tempfile

import unreal


def _package_name(value):
    path = value.get_path_name() if hasattr(value, "get_path_name") else str(value)
    return path.split(".", 1)[0]


def _package_file_base(value):
    package_name = _package_name(value)
    if not package_name.startswith("/Game/"):
        raise RuntimeError("Only /Game packages are supported: " + package_name)
    relative = package_name[len("/Game/") :].replace("/", os.sep)
    content_root = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir()
    )
    return os.path.normpath(os.path.join(content_root, relative))


def _is_writable_file(path):
    attributes = getattr(os.stat(path), "st_file_attributes", 0)
    read_only_flag = getattr(stat, "FILE_ATTRIBUTE_READONLY", 0)
    return os.access(path, os.W_OK) and not (attributes & read_only_flag)


def _nearest_existing_directory(path):
    candidate = os.path.dirname(path)
    while candidate and not os.path.isdir(candidate):
        parent = os.path.dirname(candidate)
        if parent == candidate:
            break
        candidate = parent
    return candidate


class AssetSaveTransaction:
    """Snapshot package files now; call commit or rollback exactly once."""

    def __init__(self, package_values, label):
        package_names = sorted({_package_name(value) for value in package_values})
        if not package_names:
            raise RuntimeError("Asset save transaction has no target packages")

        self._active = False
        self._entries = []
        saved_root = unreal.Paths.convert_relative_path_to_full(
            unreal.Paths.project_saved_dir()
        )
        self._backup_root = tempfile.mkdtemp(
            prefix="FFZS_{0}_".format(label), dir=saved_root
        )

        try:
            for index, package_name in enumerate(package_names):
                file_base = _package_file_base(package_name)
                existing_files = sorted(glob.glob(file_base + ".*"))
                parent = _nearest_existing_directory(file_base)
                if not parent or not os.access(parent, os.W_OK):
                    raise RuntimeError(
                        "Package directory is not writable: " + package_name
                    )
                for existing_file in existing_files:
                    if not _is_writable_file(existing_file):
                        raise RuntimeError(
                            "Package file is read-only; check it out first: "
                            + existing_file
                        )

                entry_root = os.path.join(self._backup_root, str(index))
                os.makedirs(entry_root)
                backups = []
                for existing_file in existing_files:
                    backup_file = os.path.join(entry_root, os.path.basename(existing_file))
                    shutil.copy2(existing_file, backup_file)
                    backups.append((existing_file, backup_file))
                self._entries.append((file_base, backups))
            self._active = True
        except Exception:
            shutil.rmtree(self._backup_root, ignore_errors=True)
            raise

    def commit(self):
        if not self._active:
            raise RuntimeError("Asset save transaction is no longer active")
        self._active = False
        try:
            shutil.rmtree(self._backup_root, ignore_errors=False)
        except Exception as error:
            unreal.log_warning(
                "[FF_FACILITY_SAVE] Save committed, but backup cleanup failed; "
                "the backup was preserved at {0}: {1}".format(
                    self._backup_root, error
                )
            )

    def rollback(self):
        if not self._active:
            return
        failures = []
        for file_base, backups in self._entries:
            original_paths = {original for original, _backup in backups}
            for current_file in glob.glob(file_base + ".*"):
                if current_file not in original_paths:
                    try:
                        os.remove(current_file)
                    except Exception as error:
                        failures.append("remove {0}: {1}".format(current_file, error))
            for original_file, backup_file in backups:
                try:
                    shutil.copy2(backup_file, original_file)
                except Exception as error:
                    failures.append("restore {0}: {1}".format(original_file, error))
        self._active = False
        if failures:
            raise RuntimeError(
                "Package rollback was incomplete. The recovery snapshot was "
                "preserved at {0}; restore from that snapshot or version control: {1}".format(
                    self._backup_root, " | ".join(failures)
                )
            )
        shutil.rmtree(self._backup_root, ignore_errors=True)
