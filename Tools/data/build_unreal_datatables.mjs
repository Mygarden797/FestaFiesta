/**
 * derived CSV -> Unreal DataTable CSV 변환 (wide, 구조체 필드명 일치).
 *
 * 입력은 전부 data/derived/ 에서만 읽는다. data/unreal/ 은 출력 전용이며,
 * 이 스크립트가 그 결과를 다시 입력으로 읽는 일은 없다 (한때 이 규칙을
 * 어겨서 utility_coefficients_v0.csv 가 재실행 때마다 빈 값으로 깨졌었다).
 *
 * 출력 (data/unreal/) — 헤더가 C++ USTRUCT 프로퍼티명과 정확히 일치해야 UE CSV 임포트가 된다.
 *   npc_archetypes_v0.csv          -> FFF_ArchetypeRow
 *   companion_type_modifier_v0.csv -> FFF_CompanionModifierRow
 *   utility_coefficients_v0.csv    -> FFF_UtilityCoefficientRow
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
    ArchetypeLabelDraft: resident.archetype_label_draft,
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
    CompanionLabelDraft: r.label_draft,
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

writeFileSync('data/unreal/README.md', `# Unreal DataTable CSV

이 폴더의 CSV는 UE5 \`Import as DataTable\` 로 바로 임포트할 수 있도록 헤더를 구조체 프로퍼티명과 일치시킨 파일이다.
원본 계산은 \`data/derived/\` 에 있고, 이 폴더는 그 결과를 UE 임포트용으로 재배열한 것뿐이다. 여기서 직접 값을 고치지 않는다.

구조체 이름은 프로젝트 태그 \`FF\`(FestaFiesta) + Unreal 구조체 필수 접두 \`F\` 를 합쳐 \`FFF_\` 로 시작한다.
헤더 파일명은 관례상 필수 접두 글자 하나(F)만 뺀 \`FF_\` 로 시작한다.

| CSV | 대응 구조체 | 헤더 |
|---|---|---|
| npc_archetypes_v0.csv | FFF_ArchetypeRow | Source/FestaFiesta/DataTables/FF_ArchetypeRow.h |
| companion_type_modifier_v0.csv | FFF_CompanionModifierRow | Source/FestaFiesta/DataTables/FF_CompanionModifierRow.h |
| utility_coefficients_v0.csv | FFF_UtilityCoefficientRow | Source/FestaFiesta/DataTables/FF_UtilityCoefficientRow.h |

재생성: \`node Tools/data/build_unreal_datatables.mjs\`

아키타입 5종·주축 구조는 2026-08-04 확정됐다. 다만 표시 명칭(\`ArchetypeLabelDraft\`, \`CompanionLabelDraft\`)은 UI 담당자 논의 전까지 가칭이고, 활동→카테고리 매핑과 카테고리 delta 수치도 여전히 \`assumption\`/\`draft_assumption\`이다.
`, 'utf8');
console.log('생성: data/unreal/README.md');
