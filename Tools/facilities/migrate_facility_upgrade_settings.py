# -*- coding: utf-8 -*-
"""One-time, idempotent migration from the old festival-wide upgrade defaults.

Run as an unattended Unreal Python commandlet after compiling the new structs.
Only previously supported booth definitions are initialized. A metadata marker
prevents a later deliberate 'no levels' choice from being enabled by a rerun.
An optional LegacyUpgradeConfig.json captured before the code update preserves
the project's authored prices and service multipliers.
"""

import json
import os
import sys

import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)
from ffzs_asset_save_transaction import AssetSaveTransaction
from ffzs_definition_source import require_unattended_commandlet

ROOT = '/Game/FestaFiesta/Zone/Data/FacilityDefinitions/'
MARKER = 'FF_FacilityUpgrade_PerDefinition_V1'


def run():
    require_unattended_commandlet()
    config = {
        'allowed_types': ['Booth_FoodStall', 'Booth_MarketGoods', 'Booth_CraftExperience',
                          'Booth_ActivityGame', 'Booth_Nightlife'],
        'tiers': [
            {'target_level': 2, 'cost_multiplier': .3, 'service_duration_multiplier': .8},
            {'target_level': 3, 'cost_multiplier': .6, 'service_duration_multiplier': .65},
        ],
    }
    artifact_dir = os.path.join(unreal.Paths.project_dir(), 'Artifacts/FacilityUpgrade')
    source = os.path.join(artifact_dir, 'LegacyUpgradeConfig.json')
    if os.path.isfile(source):
        with open(source, encoding='utf-8') as handle:
            config = json.load(handle)
    definitions = []
    for path in sorted(unreal.EditorAssetLibrary.list_assets(ROOT, recursive=True, include_folder=False)):
        definition = unreal.load_asset(path)
        if not isinstance(definition, unreal.FFZSFacilityDefinition):
            continue
        spec = definition.get_editor_property('spec')
        facility_id = str(spec.get_editor_property('facility_definition_id'))
        category = '_'.join(facility_id.split('_')[:2])
        if category not in config['allowed_types']:
            continue
        if unreal.EditorAssetLibrary.get_metadata_tag(definition, MARKER) == '1':
            continue
        definitions.append(definition)

    result = {'source': source if os.path.isfile(source) else 'legacy shipped defaults', 'changed': []}
    if definitions:
        transaction = AssetSaveTransaction(definitions, 'PerFacilityUpgradeMigration')
        try:
            for definition in definitions:
                spec = definition.get_editor_property('spec')
                already_authored = bool(spec.get_editor_property('uses_facility_levels')) or bool(
                    spec.get_editor_property('upgrade_tiers'))
                if not already_authored:
                    tiers = []
                    for old_tier in config['tiers']:
                        tier = unreal.FFZSFacilityUpgradeTier()
                        for key in ('target_level', 'cost_multiplier', 'service_duration_multiplier'):
                            tier.set_editor_property(key, old_tier[key])
                        # A visible starter progression; always relative to the original building.
                        tier.set_editor_property('size_multiplier', 1.0 + .15 * (old_tier['target_level'] - 1))
                        # Absolute level-1 income: each purchase adds 50% over the previous level.
                        tier.set_editor_property('revenue_multiplier', 1.5 ** (old_tier['target_level'] - 1))
                        # Visual Mesh intentionally stays unset: use the level-1 mesh.
                        tiers.append(tier)
                    spec.set_editor_property('uses_facility_levels', bool(tiers))
                    spec.set_editor_property('upgrade_tiers', tiers)
                    definition.set_editor_property('spec', spec)
                unreal.EditorAssetLibrary.set_metadata_tag(definition, MARKER, '1')
                if not unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False):
                    raise RuntimeError('Could not save ' + definition.get_path_name())
                result['changed'].append({
                    'asset': definition.get_path_name(),
                    'kept_existing_settings': already_authored,
                    'uses_facility_levels': bool(spec.get_editor_property('uses_facility_levels')),
                    'tiers': [{key: tier.get_editor_property(key) for key in (
                        'target_level', 'cost_multiplier', 'service_duration_multiplier',
                        'size_multiplier', 'revenue_multiplier')}
                        for tier in spec.get_editor_property('upgrade_tiers')],
                })
            transaction.commit()
        except Exception:
            transaction.rollback()
            raise
    os.makedirs(artifact_dir, exist_ok=True)
    report_path = os.path.join(artifact_dir, 'PerFacilityMigrationReport.json')
    with open(report_path, 'w', encoding='utf-8') as handle:
        json.dump(result, handle, ensure_ascii=False, indent=2)
    unreal.log('FACILITY_UPGRADE_MIGRATION=' + json.dumps(result, ensure_ascii=False))


run()
