# -*- coding: utf-8 -*-
# resave_facility_definitions.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 현재 클래스 구조로 에셋을 다시 저장한다.

"""Resave every UFFZSFacilityDefinition after reflected bundle changes."""

import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)
from ffzs_definition_source import (
    read_facility_definitions,
    require_unattended_commandlet,
)
from ffzs_asset_save_transaction import AssetSaveTransaction


def run():
    require_unattended_commandlet()
    definitions = read_facility_definitions(require_visual_mesh=False)
    transaction = AssetSaveTransaction(
        [record["definition"] for record in definitions],
        "FacilityDefinitionResave",
    )
    try:
        saved = 0
        for record in definitions:
            definition = record["definition_object"]
            # Python cannot reliably marshal TArray<FText>& on every UE minor version,
            # so save-time C++ IsDataValid plus the CI DataValidation commandlet remains
            # the authority. Here we force PreSave to rebuild Primary Asset bundle data.
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                definition, only_if_is_dirty=False
            ):
                raise RuntimeError(
                    "Could not resave {0}".format(record["definition"])
                )
            saved += 1
        transaction.commit()
    except Exception:
        transaction.rollback()
        raise
    unreal.log("[FF_FACILITY_DEFINITION] Resaved {0} definitions".format(saved))


run()
