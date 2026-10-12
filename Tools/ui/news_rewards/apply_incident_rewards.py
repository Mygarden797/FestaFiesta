"""병목·분실물의 기존 성공 효과에 사건 보상 100금화를 한 번만 추가한다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/NewsRewards'
OUT.mkdir(parents=True, exist_ok=True)
TARGETS = {
    '/Game/FestaFiesta/Data/Incident/FF_ID_NPCCrowded': 1,
    '/Game/FestaFiesta/Data/Incident/FF_ID_ItemLost': 2,
}
REWARD_NAME = 'RewardGold_Phase8'
REWARD_GOLD = 100
REPORT = {'status': 'running', 'assets': [], 'saved_assets': []}


def preserved_rules(asset):
    fields = ('incident_tag', 'title', 'body', 'tone', 'required_staff',
              'minute_untile_lapse', 'max_active', 'max_per_day', 'on_failed_effect')
    result = {field: str(asset.get_editor_property(field)) for field in fields}
    result['options'] = [
        {field: str(option.get_editor_property(field)) for field in
         ('label', 'cost_gold', 'cost_staff', 'min_staff', 'work_amount')}
        for option in asset.get_editor_property('options')
    ]
    return result


def ensure_reward(option):
    effects = list(option.get_editor_property('on_resolved_effect'))
    rewards = [effect for effect in effects if isinstance(effect, unreal.FF_IncidentEffect_Reward)]
    # 새로운 팀 보상 설정을 덮거나 중복 지급하지 않는다. 이번 스크립트가 만든 것만 갱신한다.
    assert all(effect.get_name() == REWARD_NAME for effect in rewards), '다른 금화 보상 설정이 있어 병합 확인이 필요합니다.'
    assert len(rewards) <= 1, '기존 금화 보상이 중복되었습니다.'
    changed = False
    if rewards:
        reward = rewards[0]
    else:
        reward = unreal.new_object(unreal.FF_IncidentEffect_Reward, outer=option, name=REWARD_NAME)
        effects.append(reward)
        option.set_editor_property('on_resolved_effect', effects)
        changed = True
    if reward.get_editor_property('reward_gold') != REWARD_GOLD:
        reward.set_editor_property('reward_gold', REWARD_GOLD)
        changed = True
    return changed


try:
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'PIE 중에는 사건 데이터를 수정하지 않습니다.'
    assets = {path: unreal.load_asset(path) for path in TARGETS}
    assert all(assets.values()), '대상 사건 에셋을 찾지 못했습니다.'
    dirty = {package.get_path_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assert not dirty.intersection(TARGETS), '대상 사건의 미저장 변경을 보존합니다.'
    before = {}
    before_effects = {}
    # 모든 대상 구성을 먼저 확인한 뒤 한정된 두 파일만 백업·수정한다.
    for path, asset in assets.items():
        options = list(asset.get_editor_property('options'))
        assert len(options) == TARGETS[path] and all(options), '사건 선택지 구성이 달라졌습니다: ' + path
        before[path] = preserved_rules(asset)
        before_effects[path] = [[effect for effect in option.get_editor_property('on_resolved_effect')
                                if not isinstance(effect, unreal.FF_IncidentEffect_Reward)] for option in options]
        for option in options:
            rewards = [effect for effect in option.get_editor_property('on_resolved_effect')
                       if isinstance(effect, unreal.FF_IncidentEffect_Reward)]
            assert len(rewards) <= 1 and all(effect.get_name() == REWARD_NAME for effect in rewards), path
    backup = ROOT / 'Saved/UIBackups/NewsRewards' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    backup.mkdir(parents=True)
    for path in TARGETS:
        source = ROOT / 'Content' / (path[6:] + '.uasset')
        shutil.copy2(source, backup / source.name)
    REPORT['backup'] = str(backup)
    for path, asset in assets.items():
        options = list(asset.get_editor_property('options'))
        changed = False
        for option in options:
            changed = ensure_reward(option) or changed
        assert preserved_rules(asset) == before[path], '보상 이외 규칙이 변경됐습니다: ' + path
        for index, option in enumerate(options):
            effects = list(option.get_editor_property('on_resolved_effect'))
            retained = [effect for effect in effects if not isinstance(effect, unreal.FF_IncidentEffect_Reward)]
            assert retained == before_effects[path][index], '기존 효과 보존 실패'
            rewards = [effect for effect in effects if isinstance(effect, unreal.FF_IncidentEffect_Reward)]
            assert len(rewards) == 1 and rewards[0].get_editor_property('reward_gold') == REWARD_GOLD
            assert not ensure_reward(option), '동일 적용 재실행이 무변경이어야 합니다.'
        if changed:
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), path
            REPORT['saved_assets'].append(path)
        REPORT['assets'].append({'path': path, 'reward_per_option': REWARD_GOLD,
                                 'option_count': len(options), 'other_rules_preserved': True})
    REPORT.update(status='passed', idempotence_verified=True)
except Exception:
    REPORT.update(status='failed', error=traceback.format_exc())
    raise
finally:
    (OUT / 'incident-rewards-apply-report.json').write_text(
        json.dumps(REPORT, ensure_ascii=False, indent=2), encoding='utf-8')
