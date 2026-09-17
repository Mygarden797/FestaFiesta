/**
 * derived CSV -> Unreal DataTable CSV 변환 (wide, 구조체 필드명 일치).
 *
 * 입력은 전부 data/derived/ 에서만 읽는다. data/unreal/ 은 출력 전용이며,
 * 이 스크립트가 그 결과를 다시 입력으로 읽는 일은 없다 (한때 이 규칙을
 * 어겨서 utility_coefficients_v0.csv 가 재실행 때마다 빈 값으로 깨졌었다).
 *
 * 출력 (data/unreal/) — 헤더가 C++ USTRUCT 프로퍼티명과 정확히 일치해야 UE CSV 임포트가 된다.
 *   npc_archetypes_v0.csv            -> FFF_ArchetypeRow
 *   companion_type_modifier_v0.csv   -> FFF_CompanionModifierRow
 *   utility_coefficients_v0.csv      -> FFF_UtilityCoefficientRow
 *   individual_variation_knobs_v0.csv -> FFF_IndividualVariationRow
 */
// 카테고리 키 → 구조체 필드명 매핑은 이 스크립트와 C++ 헤더 양쪽에서 동일해야 한다.
// 값은 전부 delta_vs_all_pp(전체 대비 percentage-point 차이)이며 효용계수가 아니다.
import { readFileSync, writeFileSync } from 'node:fs';

function readCsv(path) {
  const t = readFileSync(path, 'utf8').replace(/^﻿/, '').trim();
  const [h, ...rest] = t.split(/\r?\n/);
  const cols = h.split(',').map(c => c.replace(/^"|"$/g, ''));
  return rest.map(l => {
    const vals = l.match(/(".*?"|[^,]+)(?=,|$)/g).map(v => v.replace(/^"|"$/g, ''));
    return Object.fromEntries(cols.map((c, i) => [c, vals[i]]));
  });
}

// UI 담당자가 data/ui/*.csv 의 ui_override_label 을 채우면 여기서 우선 적용한다.
// 파일이 없거나 override 열이 비어 있으면 엔지니어링 쪽 기본 가칭을 그대로 쓴다 — 안 깨진다.
// 이 함수는 값만 바꾼다. DataStatus 는 그대로 draft_assumption 이다 (여전히 가칭).
function readLabelOverrides(path, keyField) {
  try {
    const rows = readCsv(path);
    return new Map(rows.map(r => [r[keyField], (r.ui_override_label ?? '').trim()]).filter(([, v]) => v !== ''));
  } catch {
    return new Map();
  }
}
const archetypeLabelOverrides = readLabelOverrides('data/ui/archetype_label_overrides.csv', 'archetype_id');
const companionLabelOverrides = readLabelOverrides('data/ui/companion_label_overrides.csv', 'companion_type');
function writeCsv(path, cols, rows) {
  writeFileSync(path, '﻿' + [cols.join(','), ...rows.map(o => cols.map(c => `"${o[c] ?? ''}"`).join(','))].join('\r\n') + '\r\n', 'utf8');
}

// 카테고리 키 → 구조체 필드명 (C++ 헤더와 반드시 동일하게 유지)
const CATEGORY_FIELD = {
  ambience_photo_zone: 'AmbiencePhotoZoneDeltaPp',
  food_stall: 'FoodStallDeltaPp',
  activity_game: 'ActivityGameDeltaPp',
  rest_area: 'RestAreaDeltaPp',
  market_goods: 'MarketGoodsDeltaPp',
  performance_stage: 'PerformanceStageDeltaPp',
  festival_engagement_baseline: 'FestivalEngagementBaselineDeltaPp',
  craft_experience: 'CraftExperienceDeltaPp',
  exploration_wander: 'ExplorationWanderDeltaPp',
  nightlife_entertainment: 'NightlifeEntertainmentDeltaPp',
};

// ---------- 1. npc_archetypes_v0 (long → wide, 시나리오 2개를 한 행의 두 열로) ----------
const arch = readCsv('data/derived/npc_archetypes_v0.csv');
const archIds = [...new Set(arch.map(r => r.archetype_id))];
const archOut = archIds.map(id => {
  const resident = arch.find(r => r.archetype_id === id && r.scenario_id === 'resident_neutral_v0');
  const datalab = arch.find(r => r.archetype_id === id && r.scenario_id === 'datalab_observed_v0');
  const row = {
    Name: id,
    ArchetypeId: id,
    ArchetypeLabelDraft: archetypeLabelOverrides.get(id) ?? resident.archetype_label_draft,
    PopulationSharePct_ResidentNeutral: resident.population_share_pct,
    PopulationSharePct_DatalabObserved: datalab.population_share_pct,
    TourStatProxyGroups: resident.tour_stat_proxy_groups,
    TopVisitReason1: resident.top_visit_reason_1,
    TopVisitReason1Pct: resident.top_visit_reason_1_pct,
    ResolutionNote: resident.resolution_note,
    DataStatus: 'draft_assumption',
  };
  for (const [key, field] of Object.entries(CATEGORY_FIELD)) row[field] = resident[`${key}_delta_vs_all_pp`];
  return row;
});
writeCsv('data/unreal/npc_archetypes_v0.csv',
  ['Name', 'ArchetypeId', 'ArchetypeLabelDraft', 'PopulationSharePct_ResidentNeutral', 'PopulationSharePct_DatalabObserved',
    ...Object.values(CATEGORY_FIELD), 'TourStatProxyGroups', 'TopVisitReason1', 'TopVisitReason1Pct', 'ResolutionNote', 'DataStatus'],
  archOut);
console.log(`생성: data/unreal/npc_archetypes_v0.csv (${archOut.length}행)`);

// ---------- 2. companion_type_modifier_v0 (이미 wide, 열 이름만 구조체에 맞춤) ----------
const comp = readCsv('data/derived/companion_type_modifier_v0.csv');
const compOut = comp.map(r => {
  const row = {
    Name: r.companion_type,
    CompanionType: r.companion_type,
    CompanionLabelDraft: companionLabelOverrides.get(r.companion_type) ?? r.label_draft,
    TopReasonDelta1: r.top_reason_delta_1,
    TopReasonDelta2: r.top_reason_delta_2,
    DataStatus: 'draft_assumption',
  };
  for (const [key, field] of Object.entries(CATEGORY_FIELD)) row[field] = r[`${key}_delta_vs_all_pp`];
  return row;
});
writeCsv('data/unreal/companion_type_modifier_v0.csv',
  ['Name', 'CompanionType', 'CompanionLabelDraft', ...Object.values(CATEGORY_FIELD), 'TopReasonDelta1', 'TopReasonDelta2', 'DataStatus'],
  compOut);
console.log(`생성: data/unreal/companion_type_modifier_v0.csv (${compOut.length}행)`);

// ---------- 3. utility_coefficients_v0 (derived의 snake_case 원본에 Name 열만 추가) ----------
const coef = readCsv('data/derived/utility_coefficients_v0.csv');
const coefOut = coef.map(r => ({
  Name: r.coefficient_id,
  CoefficientId: r.coefficient_id,
  DefaultValue: r.default_value,
  MinValue: r.min_value,
  MaxValue: r.max_value,
  Unit: r.unit,
  Term: r.term,
  Status: r.status,
  SourceBasis: r.source_basis,
  SensitivityRole: r.sensitivity_role,
  Description: r.description,
}));
writeCsv('data/unreal/utility_coefficients_v0.csv',
  ['Name', 'CoefficientId', 'DefaultValue', 'MinValue', 'MaxValue', 'Unit', 'Term', 'Status', 'SourceBasis', 'SensitivityRole', 'Description'],
  coefOut);
console.log(`갱신: data/unreal/utility_coefficients_v0.csv (${coefOut.length}행, Name 열 추가)`);

// ---------- 4. individual_variation_knobs_v0 (이미 wide, 열 이름만 구조체에 맞춤) ----------
const variation = readCsv('data/derived/individual_variation_knobs_v0.csv');
const variationOut = variation.map(r => ({
  Name: r.trait_key,
  TraitKey: r.trait_key,
  DistributionType: r.distribution_type,
  MinValue: r.min_value,
  ModeValue: r.mode_value,
  MaxValue: r.max_value,
  Unit: r.unit,
  RandomPurpose: r.random_purpose,
  AffectsField: r.affects_field,
  CorrelatedWith: r.correlated_with,
  Status: r.status,
  Description: r.description,
}));
writeCsv('data/unreal/individual_variation_knobs_v0.csv',
  ['Name', 'TraitKey', 'DistributionType', 'MinValue', 'ModeValue', 'MaxValue', 'Unit', 'RandomPurpose', 'AffectsField', 'CorrelatedWith', 'Status', 'Description'],
  variationOut);
console.log(`생성: data/unreal/individual_variation_knobs_v0.csv (${variationOut.length}행)`);

// ---------- 5. 그룹 생성용 3종 (derived 의 snake_case → 구조체 필드명) ----------
// crowdAI 가 그룹을 묶을 때 읽는 표다. 계보와 근거는 data/derived/*.json 참조.
const groupSize = readCsv('data/derived/visitor_group_size_distribution_v0.csv');
const groupSizeOut = groupSize.map(r => ({
  Name: `${r.scope_id}_${r.group_size}`,
  ScopeId: r.scope_id,
  GroupSize: r.group_size,
  CompanionSizeTag: r.companion_size_tag,
  SharePct: r.share_pct,
  bIsCapBucket: r.is_cap_bucket,
  Status: r.status,
}));
writeCsv('data/unreal/visitor_group_size_distribution_v0.csv',
  ['Name', 'ScopeId', 'GroupSize', 'CompanionSizeTag', 'SharePct', 'bIsCapBucket', 'Status'], groupSizeOut);
console.log(`생성: data/unreal/visitor_group_size_distribution_v0.csv (${groupSizeOut.length}행)`);

const childComp = readCsv('data/derived/group_child_composition_v0.csv');
const childCompOut = childComp.map(r => ({
  Name: `${r.scope_id}_${r.group_size}_${r.child_count}`,
  ScopeId: r.scope_id,
  GroupSize: r.group_size,
  ChildCount: r.child_count,
  ShareWithinGroupSizePct: r.share_within_group_size_pct,
  Status: r.status,
}));
writeCsv('data/unreal/group_child_composition_v0.csv',
  ['Name', 'ScopeId', 'GroupSize', 'ChildCount', 'ShareWithinGroupSizePct', 'Status'], childCompOut);
console.log(`생성: data/unreal/group_child_composition_v0.csv (${childCompOut.length}행)`);

const groupRules = readCsv('data/derived/group_composition_rules_v0.csv');
const groupRulesOut = groupRules.map(r => ({
  Name: r.rule_id,
  RuleId: r.rule_id,
  Value: r.value,
  ValueType: r.value_type,
  Status: r.status,
  SourceBasis: r.source_basis,
  Description: r.description,
}));
writeCsv('data/unreal/group_composition_rules_v0.csv',
  ['Name', 'RuleId', 'Value', 'ValueType', 'Status', 'SourceBasis', 'Description'], groupRulesOut);
console.log(`생성: data/unreal/group_composition_rules_v0.csv (${groupRulesOut.length}행)`);

// ---------- 6. sns_post_templates_v0 (이미 wide, 열 이름만 구조체에 맞춤) ----------
const snsPosts = readCsv('data/derived/sns_post_templates_v0.csv');
const snsPostsOut = snsPosts.map(r => ({
  Name: r.template_id,
  TemplateId: r.template_id,
  TopicId: r.topic_id,
  TopicName: r.topic_name,
  Sentiment: r.sentiment,
  bUsesFacilityPlaceholder: r.uses_facility_placeholder,
  Text: r.text,
}));
writeCsv('data/unreal/sns_post_templates_v0.csv',
  ['Name', 'TemplateId', 'TopicId', 'TopicName', 'Sentiment', 'bUsesFacilityPlaceholder', 'Text'],
  snsPostsOut);
console.log(`생성: data/unreal/sns_post_templates_v0.csv (${snsPostsOut.length}행)`);

writeFileSync('data/unreal/README.md', `# Unreal DataTable CSV

이 폴더의 CSV는 UE5 \`Import as DataTable\` 로 바로 임포트할 수 있도록 헤더를 구조체 프로퍼티명과 일치시킨 파일이다.
원본 계산은 \`data/derived/\` 에 있고, 이 폴더는 그 결과를 UE 임포트용으로 재배열한 것뿐이다. 여기서 직접 값을 고치지 않는다.

구조체 이름은 프로젝트 태그 \`FF\`(FestaFiesta) + Unreal 구조체 필수 접두 \`F\` 를 합쳐 \`FFF_\` 로 시작한다.
헤더 파일명은 관례상 필수 접두 글자 하나(F)만 뺀 \`FF_\` 로 시작한다.

| CSV | 대응 구조체 | 헤더 |
|---|---|---|
| npc_archetypes_v0.csv | FFF_ArchetypeRow | Source/FestaFiesta/Public/DataTables/FF_ArchetypeRow.h |
| companion_type_modifier_v0.csv | FFF_CompanionModifierRow | Source/FestaFiesta/Public/DataTables/FF_CompanionModifierRow.h |
| utility_coefficients_v0.csv | FFF_UtilityCoefficientRow | Source/FestaFiesta/Public/DataTables/FF_UtilityCoefficientRow.h |
| individual_variation_knobs_v0.csv | FFF_IndividualVariationRow | Source/FestaFiesta/Public/DataTables/FF_IndividualVariationRow.h |
| visitor_arrival_curve_v0.csv | FFF_ArrivalCurveRow | Source/FestaFiesta/Public/DataTables/FF_ArrivalCurveRow.h |
| visitor_group_size_distribution_v0.csv | FFF_GroupSizeDistributionRow | Source/FestaFiesta/Public/DataTables/FF_GroupSizeDistributionRow.h |
| group_child_composition_v0.csv | FFF_GroupChildCompositionRow | Source/FestaFiesta/Public/DataTables/FF_GroupChildCompositionRow.h |
| group_composition_rules_v0.csv | FFF_GroupCompositionRuleRow | Source/FestaFiesta/Public/DataTables/FF_GroupCompositionRuleRow.h |
| sns_post_templates_v0.csv | FFF_SnsPostTemplateRow | Source/FestaFiesta/Public/DataTables/FF_SnsPostTemplateRow.h |

재생성: \`node Tools/data/build_unreal_datatables.mjs\`

단, \`visitor_arrival_curve_v0.csv\` 는 이 스크립트가 만들지 않는다 — \`node Tools/data/build_arrival_curve.mjs\` 로 따로 생성한다
(입력이 derived CSV 두 개의 계수라서 계보가 다르다. 설계는 \`data/metadata/spec/visitor_arrival_curve_spec.md\` 참조).

그룹 생성용 3종의 \`data/derived/\` 원본을 다시 만들려면:
\`node Tools/data/build_visitor_group_distribution.mjs\` (2025 국민여행조사 원자료 419MB 필요 — \`data/extracted/\` 에 풀려 있어야 한다)
\`node Tools/data/build_group_composition_rules.mjs\` (원자료 불필요)

\`companion_type_response_v0.csv\`(동반자 유형 응답률)는 DataTable 로 내보내지 않는다 — 아동 배치에는 쓰이지 않고
\`non_leader_adult_archetype_rule\` 을 나중에 정교화할 때의 근거 자료로만 \`data/derived/\` 에 둔다.

아키타입 5종·주축 구조는 2026-08-04 확정됐다. 다만 표시 명칭(\`ArchetypeLabelDraft\`, \`CompanionLabelDraft\`)은 UI 담당자 논의 전까지 가칭이고, 활동→카테고리 매핑과 카테고리 delta 수치도 여전히 \`assumption\`/\`draft_assumption\`이다.

\`individual_variation_knobs_v0.csv\`의 6개 multiplier 범위와 \`leisureliness_axis\`는 초안이다. 자세한 설계는 \`data/metadata/spec/individual_variation_spec.md\` 참조.

표시 명칭은 \`data/ui/archetype_label_overrides.csv\`·\`data/ui/companion_label_overrides.csv\`의 \`ui_override_label\`이 비어있지 않으면 그 값이 우선 적용된다. UI 담당자가 이 두 파일만 편집하고 이 스크립트를 재실행하면 된다 — DataStatus는 여전히 draft_assumption.
`, 'utf8');
console.log('생성: data/unreal/README.md');
