/**
 * 그룹 구성 규칙 — crowdAI 가 코드로 읽을 수 있는 형태
 *
 * population_group_generation_spec.md 의 규칙 중 **실측으로 대체되지 않고 남은 것만** 담는다.
 * 원자료 의존성이 없다(순수 규칙 저작) — 419MB 원자료 없이도 재생성된다.
 *
 * 2026-08-06에 실측(2025 국민여행조사)으로 대체돼 이 표에서 빠진 규칙:
 *   - 그룹 크기 비율 solo/pair/group_3_plus  → visitor_group_size_distribution_v0.csv
 *   - group_3_plus 내부 3명 70% / 4명 30%   → 같은 파일 (3~6인 실측 분포)
 *   - pair 유형 couple_or_friends 80% / parent_and_child 20%
 *   - group_3_plus 유형 family 50% / friends 50%
 *   - child_attach_probability_given_group3plus 0.4, child_count_mean_given_attached 1.5
 *     → 위 4개는 group_child_composition_v0.csv 의 P(아동 수 | 그룹 크기) 하나로 통합됐다.
 *       "유형을 먼저 뽑고 그 유형에 자녀 슬롯이 있는지 정한다"는 2단 구조가 필요 없어졌다.
 */
import { writeFileSync } from 'node:fs';

const RULES = [
  {
    rule_id: 'group_size_cap',
    value: '6',
    value_type: 'int',
    status: 'derived',
    source_basis: 'user_decision_2026_08_06 + national_travel_survey_2025',
    description: '그룹 크기 상한. 이 값을 넘는 일행은 이 값으로 합쳐서 취급한다. 실측상 초과분이 당일여행 기준 4.295%뿐이라 왜곡이 작다. 최대 관측은 80명(학교 단체).',
  },
  {
    rule_id: 'group_size_distribution_table',
    value: 'visitor_group_size_distribution_v0',
    value_type: 'table_ref',
    status: 'derived',
    source_basis: 'national_travel_survey_2025_A6',
    description: '그룹 크기를 뽑는 분포표. scope_id=tourism_daytrip 이 기본이며 tourism_all/tourism_overnight 은 민감도 비교용.',
  },
  {
    rule_id: 'child_composition_table',
    value: 'group_child_composition_v0',
    value_type: 'table_ref',
    status: 'derived',
    source_basis: 'national_travel_survey_2025_A6A',
    description: '그룹 크기별 아동 수 분포 P(아동 수 | 그룹 크기). 그룹을 만든 뒤 이 표에서 아동 수를 뽑아 그만큼 bIsDependentChildSlot 을 세운다.',
  },
  {
    rule_id: 'child_age_definition',
    value: 'under_15',
    value_type: 'enum',
    status: 'derived',
    source_basis: 'national_travel_survey_2025_A6A',
    description: '아동 슬롯의 연령 정의. 원자료 문항이 "만 15세 미만"이라 그 경계를 그대로 쓴다. 기존 문서들이 쓰던 0~14세와 같은 구간이다.',
  },
  {
    rule_id: 'leader_count_per_group',
    value: '1',
    value_type: 'int',
    status: 'confirmed',
    source_basis: 'design_decision_2026_08_04',
    description: '그룹당 리더 수. 그룹 크기·유형과 무관하게 항상 1명.',
  },
  {
    rule_id: 'leader_must_be_adult',
    value: 'true',
    value_type: 'bool',
    status: 'confirmed',
    source_basis: 'design_decision_2026_08_04',
    description: '리더는 아동 슬롯이 아닌 성인이어야 한다. 아동은 자체 목적지 계산을 하지 않으므로 리더가 될 수 없다.',
  },
  {
    rule_id: 'destination_decision_scope',
    value: 'leader_only',
    value_type: 'enum',
    status: 'confirmed',
    source_basis: 'design_decision_2026_08_04',
    description: '리더만 Utility 목적지 계산을 하고 나머지 그룹원(성인 포함) 전원이 리더의 목적지를 상속한다. "임시 분리 후 재합류"는 v1 이후.',
  },
  {
    rule_id: 'non_leader_adult_archetype_rule',
    value: 'same_as_leader',
    value_type: 'enum',
    status: 'assumption',
    source_basis: 'none',
    description: '리더가 아닌 성인 그룹원의 아키타입 결정 규칙. 실측 근거가 없다 — 실제로는 부부(같은 생애주기)와 부모+성인자녀(다른 생애주기)가 섞여 있다. companion_type_response_v0.csv 의 유형별 응답률로 나중에 정교화할 수 있다.',
  },
  {
    rule_id: 'spawn_group_together',
    value: 'true',
    value_type: 'bool',
    status: 'confirmed',
    source_basis: 'design_decision_2026_08_04',
    description: '그룹원 전원을 시차 없이 한 번에 스폰한다. 입장 타이밍의 유일한 제약(visitor_arrival_curve_spec.md §4)과 같은 규칙.',
  },
  {
    rule_id: 'group_shared_traits',
    value: 'walk_speed_multiplier;stay_duration_multiplier',
    value_type: 'list',
    status: 'confirmed',
    source_basis: 'design_decision_2026_08_04',
    description: 'GroupID 로 시드해서 그룹 전체가 공유하는 개체 변주 특성. 나머지 4개(patience/crowd_tolerance/preference_strength/reevaluate_interval)는 개체별 독립.',
  },
  {
    rule_id: 'child_trait_overrides',
    value: 'movement_speed;attention_span;wait_tolerance',
    value_type: 'list',
    status: 'assumption',
    source_basis: 'none',
    description: '아동 슬롯에만 추가로 적용되는 노브 3종(이동속도 낮음·주의집중 짧음·대기인내 낮음). 값은 아직 미정 — dependent_child_npc_spec.md 2절 참조.',
  },
];

const cols = ['rule_id', 'value', 'value_type', 'status', 'source_basis', 'description'];
writeFileSync('data/derived/group_composition_rules_v0.csv',
  '﻿' + [cols.join(','), ...RULES.map(r => cols.map(c => `"${r[c]}"`).join(','))].join('\r\n') + '\r\n', 'utf8');
console.log(`생성: data/derived/group_composition_rules_v0.csv (${RULES.length}행)`);

writeFileSync('data/derived/group_composition_rules_v0.json', JSON.stringify({
  schema_version: '0.1.0',
  created_at: '2026-08-06',
  dataset_id: 'group_composition_rules_v0',
  status: 'mixed — rule_id 별 status 열 참조',
  purpose: 'population_group_generation_spec.md 의 그룹 구성 규칙을 crowdAI 가 마크다운이 아니라 데이터로 읽게 한다. 규칙이 바뀔 때 crowdAI 코드를 고치지 않아도 되게 하는 것이 목적이다.',
  row_unit: '규칙 한 개',
  consumer_note: 'value 열은 전부 문자열이다. value_type 을 보고 파싱한다(int/bool/enum/list/table_ref). list 는 세미콜론 구분. table_ref 는 data/derived 또는 data/unreal 의 dataset_id.',
  superseded_by_measurement_2026_08_06: {
    note: '아래 draft_assumption 들은 2025 국민여행조사 실측으로 대체돼 이 표에 없다.',
    removed: [
      'solo/pair/group_3_plus 비율 (출처 자체가 없었음) → visitor_group_size_distribution_v0',
      'group_3_plus 내부 3명 70% / 4명 30% → visitor_group_size_distribution_v0 (3~6인 실측)',
      'pair 유형 couple_or_friends 80% / parent_and_child 20% → group_child_composition_v0',
      'group_3_plus 유형 family 50% / friends 50% → group_child_composition_v0',
      'child_attach_probability_given_group3plus 0.4 / child_count_mean_given_attached 1.5 → group_child_composition_v0',
    ],
    why_simpler: '"그룹 유형을 먼저 뽑고(가족/친구) 그 유형에 자녀 슬롯이 있는지 정한다"는 2단 구조가 P(아동 수 | 그룹 크기) 한 장으로 대체됐다. 아동 배치에 필요한 정보가 그 표에 이미 다 있어서 중간 단계인 "유형"을 거칠 필요가 없다.',
    still_useful: 'companion_type_response_v0.csv(동반자 유형 응답률)는 아동 배치에는 안 쓰이지만, non_leader_adult_archetype_rule 을 나중에 정교화할 때의 근거로 남긴다.',
  },
  regeneration: 'node Tools/data/build_group_composition_rules.mjs',
  spec: 'data/metadata/spec/population_group_generation_spec.md',
}, null, 2) + '\n', 'utf8');
console.log('생성: data/derived/group_composition_rules_v0.json');
console.log(`\n규칙 ${RULES.length}개 — status 별: ` +
  Object.entries(RULES.reduce((a, r) => (a[r.status] = (a[r.status] ?? 0) + 1, a), {})).map(([k, v]) => `${k} ${v}`).join(', '));
