/**
 * 생애주기 아키타입 5종 Row 생성.
 *
 * 결합 대상: visitor_population_prior_v0(인구비중), tour_stat_2018_age_activity/reason
 * (연령별 응답률), festival_activity_category_mapping_v0(활동→카테고리 판단 매핑).
 *
 * 아키타입 구조(5종·주축·오버레이)는 2026-08-04 확정했지만, label(표시 명칭)은
 * 가칭이며 UI 담당자 논의 후 확정한다. 카테고리 delta 값 자체도 여전히
 * assumption이다 (Tour_Stat 응답률 기반 경향 신호, 효용계수 아님).
 */
// 해상도 불일치 — 반드시 남겨야 하는 경고
// - population의 age_10_or_under 는 0~19세를 묶는데 Tour_Stat 행동자료는
//   15~19세부터 시작한다. 0~14세의 독립 행동 신호는 어떤 자료에도 없다.
//   -> life_stage_teen 의 활동 점수는 15~19세만의 대리값이며 0~14세 아동에게는
//      적용하지 않는다 (아동은 별도 종속 NPC로 처리, dependent_child_npc_spec.md).
// - 2026-08-04 수정: population_share_pct도 이제 age_10_or_under(0~19세 통합)를
//   그대로 쓰지 않는다. 예전에는 0~14세 인구까지 life_stage_teen 인구비중에
//   들어가 있어서, 종속 아동 메커닉이 그 위에 아동을 또 얹으면 이중 계산되는
//   버그가 있었다 (population_group_generation_spec.md 2절 참고). 원본 단일세
//   인구(saha_population_age_sex_2026_07.csv)에서 0~14세/15~19세 인구비를 구해
//   age_10_or_under의 visitor_share_pct를 그 비율대로 나눈다 — 15~19세 몫만
//   life_stage_teen에 반영하고, 0~14세 몫은 dependent_child_population_target_v0
//   로 따로 산출한다. 두 연령대의 방문 성향이 같다는 독립가정이다(데이터랩
//   해상도가 age_10_or_under 통합이라 더 세밀한 신호가 없다).
// - Tour_Stat 은 60세 이상을 하나로 묶어 population의 60대/70대+ 를 구분하지 못한다.
// - 30~40대 아키타입은 population 두 세그먼트를 합치지만 Tour_Stat 값은 두 그룹의
//   단순평균이다(인구가중 아님). 이 자체가 assumption.
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';

function readCsv(path) {
  const t = readFileSync(path, 'utf8').replace(/^﻿/, '').trim();
  const [h, ...rest] = t.split(/\r?\n/);
  const cols = h.split(',').map(c => c.replace(/^"|"$/g, ''));
  return rest.map(l => {
    const vals = l.match(/(".*?"|[^,]+)(?=,|$)/g).map(v => v.replace(/^"|"$/g, ''));
    return Object.fromEntries(cols.map((c, i) => [c, vals[i]]));
  });
}

const pop = readCsv('data/derived/visitor_population_prior_v0.csv');
const activity = readCsv('data/cleaned/tourism/tour_stat_2018_age_activity.csv');
const reason = readCsv('data/cleaned/tourism/tour_stat_2018_age_reason.csv');
const mapping = readCsv('data/derived/festival_activity_category_mapping_v0.csv');

// age_10_or_under(0~19세 통합) 세그먼트를 0~14세/15~19세로 쪼개는 비율.
// build_visitor_population_prior.mjs와 동일하게 region_level='gu'(사하구 전체)만 쓴다.
const singleYearPop = readCsv('data/cleaned/population/saha_population_age_sex_2026_07.csv')
  .filter(r => r.region_level === 'gu');
const residentCountInAgeRange = (sex, minAge, maxAge) => singleYearPop
  .filter(r => r.sex === sex && +r.age >= minAge && +r.age <= maxAge)
  .reduce((sum, r) => sum + +r.population_count, 0);

// TEEN_RATIO[sex] = 그 성별 age_10_or_under 인구 중 15~19세가 차지하는 비율.
// "두 연령대의 방문 성향이 같다"는 독립가정으로, age_10_or_under의 visitor_share_pct를
// 이 인구비대로 나눠 15~19세 몫만 life_stage_teen에 반영한다 (population_group_generation_spec.md 2절).
const TEEN_RATIO = {};
for (const sex of ['male', 'female']) {
  const under15 = residentCountInAgeRange(sex, 0, 14);
  const teen15to19 = residentCountInAgeRange(sex, 15, 19);
  TEEN_RATIO[sex] = teen15to19 / (under15 + teen15to19);
}

const mapOf = Object.fromEntries(mapping.map(m => [m.tour_stat_item, m]));
const CATEGORIES = [...new Set(mapping.map(m => m.festival_category))].filter(c => c !== 'not_applicable');

const rateOf = (table, groupKey, item) => {
  const row = table.find(r => r.group_key === groupKey && r.item === item);
  return row ? +row.response_rate_pct : null;
};

// 카테고리 하나의 평균 응답률. tourStatGroups가 2개면(30~40대) 항목별로 먼저 단순평균한다
function categoryScore(table, tourStatGroups, category) {
  const items = mapping.filter(m => m.festival_category === category).map(m => m.tour_stat_item);
  const perItem = items.map(it => {
    const vals = tourStatGroups.map(g => rateOf(table, g, it)).filter(v => v !== null);
    return vals.length ? vals.reduce((a, b) => a + b, 0) / vals.length : null;
  }).filter(v => v !== null);
  return perItem.length ? perItem.reduce((a, b) => a + b, 0) / perItem.length : null;
}

const ARCHETYPES = [
  {
    id: 'life_stage_teen', label_draft: '주니어 체험형', tourStatGroups: ['age_15_19'],
    popSegments: ['age_10_or_under_male', 'age_10_or_under_female'],
    ageSplitRatioBySex: TEEN_RATIO, // age_10_or_under의 visitor_share_pct 중 15~19세 몫만 반영 (0~14세는 이중계산 방지를 위해 뺀다)
    resolution_note: 'Tour_Stat의 15~19세 응답을 이 아키타입의 활동 점수로 쓴다. population_share_pct는 2026-08-04부터 age_10_or_under(0~19세 통합)를 그대로 쓰지 않고, 원본 단일세 인구로 15~19세만 분리해서 반영한다 — 0~14세 몫은 dependent_child_population_target_v0.csv로 따로 산출해 종속 아동 생성의 목표치로 쓴다.',
  },
  {
    id: 'life_stage_20s', label_draft: '청년 유흥형', tourStatGroups: ['age_20s'],
    popSegments: ['age_20s_male', 'age_20s_female'],
    resolution_note: null,
  },
  {
    id: 'life_stage_30_40s', label_draft: '청장년 휴식형', tourStatGroups: ['age_30s', 'age_40s'],
    popSegments: ['age_30s_male', 'age_30s_female', 'age_40s_male', 'age_40s_female'],
    resolution_note: '30대와 40대 Tour_Stat 응답률을 단순평균했다 (인구가중 아님, assumption).',
  },
  {
    id: 'life_stage_50s', label_draft: '중년 액티브형', tourStatGroups: ['age_50s'],
    popSegments: ['age_50s_male', 'age_50s_female'],
    resolution_note: null,
  },
  {
    id: 'life_stage_60plus', label_draft: '시니어 여유형', tourStatGroups: ['age_60_or_over'],
    popSegments: ['age_60s_male', 'age_60s_female', 'age_70_or_over_male', 'age_70_or_over_female'],
    resolution_note: 'Tour_Stat이 60세 이상을 하나로 묶어 60대와 70대 이상을 구분하지 못한다. 동일한 활동 점수를 두 인구 세그먼트에 함께 적용한다.',
  },
];

const rowsOut = [];
for (const scenario of ['resident_neutral_v0', 'datalab_observed_v0']) {
  for (const a of ARCHETYPES) {
    const popShare = a.popSegments.reduce((sum, seg) => {
      const share = +pop.find(r => r.scenario_id === scenario && r.segment_key === seg)?.visitor_share_pct || 0;
      const sex = seg.endsWith('_male') ? 'male' : 'female';
      const ratio = a.ageSplitRatioBySex ? a.ageSplitRatioBySex[sex] : 1;
      return sum + share * ratio;
    }, 0);

    const row = {
      scenario_id: scenario,
      archetype_id: a.id,
      archetype_label_draft: a.label_draft,
      population_share_pct: popShare.toFixed(4),
      tour_stat_proxy_groups: a.tourStatGroups.join('+'),
    };
    for (const c of CATEGORIES) {
      const score = categoryScore(activity, a.tourStatGroups, c);
      const baseline = categoryScore(activity, ['all'], c);
      row[`${c}_response_rate_pct`] = score !== null ? score.toFixed(2) : '';
      row[`${c}_delta_vs_all_pp`] = (score !== null && baseline !== null) ? (score - baseline).toFixed(2) : '';
    }
    // 방문지 선택 이유는 카테고리 매핑 대상이 아니므로 상위 항목만 참고용으로 별도 기록
    const topReasons = reason.filter(r => a.tourStatGroups.includes(r.group_key))
      .reduce((acc, r) => { acc[r.item] = (acc[r.item] || 0) + +r.response_rate_pct / a.tourStatGroups.length; return acc; }, {});
    row.top_visit_reason_1 = Object.entries(topReasons).sort((x, y) => y[1] - x[1])[0]?.[0] ?? '';
    row.top_visit_reason_1_pct = Object.entries(topReasons).sort((x, y) => y[1] - x[1])[0]?.[1]?.toFixed(1) ?? '';
    row.status = 'draft_assumption';
    row.resolution_note = a.resolution_note ?? '';
    rowsOut.push(row);
  }
}

// age_10_or_under의 visitor_share_pct 중 life_stage_teen(15~19세)에 안 들어간 0~14세 몫.
// population_group_generation_spec.md 2.2절 "방식 A"의 목표 아동 수 입력값이다 —
// dependent_child_npc_spec.md의 부착 확률·평균 수 노브가 이 값에 맞게 조정돼야 한다.
const childTargetRows = ['resident_neutral_v0', 'datalab_observed_v0'].map(scenario => {
  const childSharePct = ['male', 'female'].reduce((sum, sex) => {
    const seg = `age_10_or_under_${sex}`;
    const totalShare = +pop.find(r => r.scenario_id === scenario && r.segment_key === seg)?.visitor_share_pct || 0;
    return sum + totalShare * (1 - TEEN_RATIO[sex]);
  }, 0);
  return { scenario_id: scenario, target_child_visitor_share_pct: childSharePct.toFixed(4), status: 'derived' };
});

mkdirSync('data/derived', { recursive: true });
const cols = Object.keys(rowsOut[0]);
writeFileSync('data/derived/npc_archetypes_v0.csv',
  '﻿' + [cols.join(','), ...rowsOut.map(o => cols.map(c => `"${o[c]}"`).join(','))].join('\r\n') + '\r\n', 'utf8');

writeFileSync('data/derived/npc_archetypes_v0.json', JSON.stringify({
  schema_version: '0.2.0',
  created_at: '2026-08-04',
  dataset_id: 'npc_archetypes_v0',
  status: '아키타입 구조(5종·주축·오버레이)는 확정(2026-08-04). label(표시 명칭)은 가칭, UI 담당자 논의 필요. 카테고리 delta 값도 여전히 assumption',
  decision_basis: '2026-08-04 사용자가 생애주기 5종 주 아키타입 + 동반유형 개체 오버레이 + 어린이 종속 NPC 구조를 확정함. 표시 명칭은 UI 담당자 논의 후 확정 예정',
  row_unit: '한 시나리오 × 한 아키타입',
  output_row_count: rowsOut.length,
  archetypes: ARCHETYPES.map(a => ({ id: a.id, label_draft: a.label_draft, population_segments: a.popSegments, age_split_ratio_by_sex: a.ageSplitRatioBySex ?? null, tour_stat_proxy_groups: a.tourStatGroups, resolution_note: a.resolution_note })),
  festival_categories_scored: CATEGORIES,
  columns: {
    population_share_pct: 'visitor_population_prior_v0의 해당 시나리오·세그먼트 합. life_stage_teen만 age_split_ratio_by_sex로 15~19세 몫만 반영(2026-08-04부터, 0~14세는 dependent_child_population_target_v0로 분리)',
    '{category}_response_rate_pct': 'Tour_Stat 연령별 활동 응답률을 해당 카테고리 매핑 항목들로 평균한 값 (%, 복수응답 응답률이며 구성비 아님)',
    '{category}_delta_vs_all_pp': '전체(all) 응답률과의 percentage-point 차이. 양수면 해당 생애주기가 전체 평균보다 그 카테고리를 더 선호한다는 경향 신호',
    top_visit_reason_1: '해당 생애주기에서 응답률이 가장 높은 방문지 선택 이유 (참고용, 카테고리 매핑 대상 아님)',
  },
  prohibited_uses: [
    '이 표의 수치를 그대로 lambda_affinity 나 weight_* 계수로 대입하는 것',
    'archetype_label_draft 를 UI 담당자 논의 없이 최종 명칭으로 사용하는 것',
    'life_stage_teen 점수를 0~14세 아동에게 그대로 적용하는 것 (종속 NPC로 별도 처리)',
    'life_stage_60plus 점수로 60대와 70대 이상의 행동을 구분하는 것 (Tour_Stat이 구분하지 않음)',
  ],
  status_field: 'draft_assumption (모든 행)',
}, null, 2) + '\n', 'utf8');

const childCols = Object.keys(childTargetRows[0]);
writeFileSync('data/derived/dependent_child_population_target_v0.csv',
  '﻿' + [childCols.join(','), ...childTargetRows.map(o => childCols.map(c => `"${o[c]}"`).join(','))].join('\r\n') + '\r\n', 'utf8');

writeFileSync('data/derived/dependent_child_population_target_v0.json', JSON.stringify({
  schema_version: '0.1.0',
  created_at: '2026-08-04',
  dataset_id: 'dependent_child_population_target_v0',
  status: 'derived — population_group_generation_spec.md 2.2절 "방식 A"의 목표 아동 수 입력값',
  method: 'age_10_or_under(0~19세 통합) 세그먼트의 visitor_share_pct 중, 원본 단일세 인구(saha_population_age_sex_2026_07.csv)로 계산한 0~14세 인구비만큼을 떼어낸 값. life_stage_teen의 population_share_pct(15~19세 몫)와 합치면 age_10_or_under의 원래 visitor_share_pct와 같아진다 — 이중 계산 없음.',
  independence_assumption: '0~14세와 15~19세의 방문 성향이 같다고 가정한다(데이터랩 해상도가 age_10_or_under 통합이라 더 세밀한 신호가 없다).',
  columns: {
    target_child_visitor_share_pct: '이 시나리오에서 방문객 중 0~14세가 차지해야 하는 비율(%). dependent_child_npc_spec.md 3절의 부착 확률·평균 수 노브가 이 값에 맞게 조정돼야 한다',
  },
  regenerate_with: 'Tools/data/build_npc_archetypes.mjs',
}, null, 2) + '\n', 'utf8');

console.log(`생성: data/derived/npc_archetypes_v0.csv (${rowsOut.length}행 = 시나리오 2 × 아키타입 5)`);
console.log(`카테고리: ${CATEGORIES.join(', ')}\n`);
console.log('생성: data/derived/dependent_child_population_target_v0.csv/.json');
console.log(`TEEN_RATIO(15~19세 몫): male=${TEEN_RATIO.male.toFixed(4)}, female=${TEEN_RATIO.female.toFixed(4)}`);
for (const r of childTargetRows) {
  console.log(`  ${r.scenario_id}: 목표 아동 비율 ${r.target_child_visitor_share_pct}%`);
}

console.log('=== datalab_observed_v0 시나리오 요약 ===');
console.log('아키타입'.padEnd(14) + '인구%'.padStart(8) + CATEGORIES.map(c => c.slice(0, 8).padStart(10)).join(''));
for (const a of ARCHETYPES) {
  const r = rowsOut.find(r => r.scenario_id === 'datalab_observed_v0' && r.archetype_id === a.id);
  console.log(a.label_draft.padEnd(14) + r.population_share_pct.padStart(8) +
    CATEGORIES.map(c => (r[`${c}_response_rate_pct`] || '-').padStart(10)).join(''));
}
