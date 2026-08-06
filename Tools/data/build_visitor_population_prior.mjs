/**
 * 단계 C — Synthetic Population 기초분포
 *
 * 인수인계 문서 7절 C의 3층 분리를 그대로 구현한다.
 *
 *   ResidentPopulationPrior          사하구에 "사는" 사람의 성·연령 구성 (주민등록, confirmed)
 *     → VisitorPropensityAssumption  그중 누가 축제에 "오는가" (실측 없음, assumption 노브)
 *     → FestivalVisitorSyntheticPopulation  축제장에 "있는" 방문객 구성 (시나리오 산출물)
 *
 * 왜 층을 나누는가
 *   주민등록 인구는 거주자이고, 축제 방문객은 거주자 + 외지인이다. 두 모집단이 다르다.
 *   그래서 주민 분포에 데이터랩 관광지 집중률을 곧바로 곱하지 않는다. 곱하면
 *   "사하구에 사는 사람 중 관광지에 간 사람"이라는, 어느 자료도 관측한 적 없는
 *   숫자가 만들어진다.
 *
 * 대신 두 자료를 각각 독립된 시나리오로 둔다.
 *   scenario A (resident_neutral_v0)  방문 성향이 세그먼트별로 같다고 가정 → 주민 구성 그대로
 *   scenario B (datalab_observed_v0)  사하구 관광지에서 실제 관측된 구성을 그대로 사용
 *   두 시나리오의 차이가 곧 이 프로젝트가 정직하게 말할 수 있는 불확실성 폭이다.
 *
 * 출력
 *   data/derived/visitor_population_prior_v0.csv
 *   data/derived/visitor_population_prior_v0.json
 */
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';

const POP = 'data/cleaned/population/saha_population_age_sex_2026_07.csv';
const DATALAB = 'data/derived/affinity_prior_equal_site_v0.csv';
const OUT_DIR = 'data/derived';
const STEM = 'visitor_population_prior_v0';

// BOM 제거 후 헤더 있는 UTF-8 CSV를 객체 배열로
function readCsv(path) {
  const t = readFileSync(path, 'utf8').replace(/^﻿/, '').trim();
  const [h, ...rest] = t.split(/\r?\n/);
  const cols = h.split(',');
  return rest.map(l => Object.fromEntries(l.split(',').map((v, i) => [cols[i], v])));
}

// ---------- 층 1: ResidentPopulationPrior ----------
// 행정동 행은 사하구 합계와 중복이므로 gu 수준만 사용한다.
const pop = readCsv(POP).filter(r => r.region_level === 'gu');
const resident = new Map();
for (const r of pop) {
  const k = r.segment_key;
  if (!resident.has(k)) resident.set(k, { label: r.segment_label, count: 0 });
  resident.get(k).count += +r.population_count;
}
const residentTotal = [...resident.values()].reduce((a, v) => a + v.count, 0);

// ---------- 층 2 입력: 데이터랩 관측 구성 ----------
// affinity 파일에서 세그먼트별 equal_site_baseline_share_pct 를 회수한다.
// (관광지마다 같은 값이 반복되므로 첫 등장분만 취한다)
const datalab = new Map();
for (const r of readCsv(DATALAB)) {
  if (!datalab.has(r.segment_key)) datalab.set(r.segment_key, +r.equal_site_baseline_share_pct);
}
const datalabTotal = [...datalab.values()].reduce((a, v) => a + v, 0);

// ---------- 층 3: 시나리오별 방문객 구성 ----------
const ORDER = ['age_10_or_under', 'age_20s', 'age_30s', 'age_40s', 'age_50s', 'age_60s', 'age_70_or_over'];
const keys = [...resident.keys()].sort((a, b) => {
  const ga = ORDER.findIndex(o => a.startsWith(o)), gb = ORDER.findIndex(o => b.startsWith(o));
  return ga - gb || (a.endsWith('male') && !a.endsWith('female') ? -1 : 1);
});

const rowsOut = [];
for (const k of keys) {
  const res = resident.get(k);
  const residentSharePct = res.count / residentTotal * 100;
  const datalabSharePct = (datalab.get(k) ?? 0) / datalabTotal * 100;

  // scenario A: 방문 성향 배수를 모두 1.0 으로 두는 중립 가정
  rowsOut.push({
    scenario_id: 'resident_neutral_v0',
    segment_key: k,
    segment_label: res.label,
    resident_count: res.count,
    resident_share_pct: residentSharePct.toFixed(4),
    visitor_propensity_multiplier: '1.0000',
    visitor_share_pct: residentSharePct.toFixed(4),
    reference_datalab_share_pct: datalabSharePct.toFixed(4),
    implied_propensity_vs_resident: (datalabSharePct / residentSharePct).toFixed(4),
    status: 'assumption',
    source_id: 'population_mois_saha_age_sex_2026_07',
  });

  // scenario B: 사하구 관광지에서 관측된 구성을 그대로 방문객 구성으로 사용
  rowsOut.push({
    scenario_id: 'datalab_observed_v0',
    segment_key: k,
    segment_label: res.label,
    resident_count: res.count,
    resident_share_pct: residentSharePct.toFixed(4),
    visitor_propensity_multiplier: (datalabSharePct / residentSharePct).toFixed(4),
    visitor_share_pct: datalabSharePct.toFixed(4),
    reference_datalab_share_pct: datalabSharePct.toFixed(4),
    implied_propensity_vs_resident: (datalabSharePct / residentSharePct).toFixed(4),
    status: 'assumption',
    source_id: 'tourism_datalab_saha_2025_07_2026_06',
  });
}

mkdirSync(OUT_DIR, { recursive: true });
const cols = Object.keys(rowsOut[0]);
writeFileSync(`${OUT_DIR}/${STEM}.csv`,
  '﻿' + [cols.join(','), ...rowsOut.map(o => cols.map(c => o[c]).join(','))].join('\r\n') + '\r\n', 'utf8');

// 두 시나리오의 총변동거리 — 불확실성 폭의 한 줄 요약
const tvd = keys.reduce((a, k) => {
  const rs = resident.get(k).count / residentTotal;
  const ds = (datalab.get(k) ?? 0) / datalabTotal;
  return a + Math.abs(rs - ds);
}, 0) / 2;

const meta = {
  schema_version: '0.1.0',
  created_at: '2026-08-03',
  dataset_id: STEM,
  row_unit: '시나리오 하나 × 성·연령 세그먼트 하나',
  output_row_count: rowsOut.length,
  layer_model: {
    layer_1_resident_population_prior: {
      description: '사하구에 거주 등록된 사람의 성·연령 구성',
      source_id: 'population_mois_saha_age_sex_2026_07',
      status: 'confirmed',
      total_population: residentTotal,
    },
    layer_2_visitor_propensity_assumption: {
      description: '각 세그먼트가 축제에 방문할 상대적 성향. 실측된 적 없음',
      status: 'assumption',
      note: '이 층이 비어 있다는 사실이 이 프로젝트 최대의 데이터 공백이다. 방문 전환율을 관측한 자료가 없다.',
    },
    layer_3_festival_visitor_synthetic_population: {
      description: '축제장에 있는 방문객의 성·연령 구성 (시나리오 산출물)',
      status: 'assumption',
    },
  },
  scenarios: {
    resident_neutral_v0: {
      assumption: '모든 세그먼트의 축제 방문 성향이 동일하다',
      result: '방문객 구성 = 사하구 주민 구성',
      why_useful: '외지 유입이 적고 동네 축제 성격이 강한 경우의 하한 시나리오',
      status: 'assumption',
    },
    datalab_observed_v0: {
      assumption: '축제 방문객 구성이 사하구 관광지에서 관측된 구성과 같다',
      result: '방문객 구성 = 데이터랩 13개소 동일가중 구성',
      why_useful: '관광 목적 방문이 지배적인 경우의 대비 시나리오',
      status: 'assumption',
      inherited_limitation: '데이터랩 13개소를 동일 가중한 값이며 사하구 전체 관광객 구성이 아니다',
    },
  },
  measurement_bias_warning: {
    segment: ['age_10_or_under_male', 'age_10_or_under_female'],
    observed_multiplier: '약 0.50',
    caution: '이 값을 "아이들은 관광지에 잘 가지 않는다"로 읽으면 안 된다. 한국관광 데이터랩의 집중률은 이동통신 기반 추정일 가능성이 높고, 그 경우 휴대폰을 소지하지 않은 아동이 구조적으로 과소집계된다. 실제 행동 차이인지 측정 편향인지 구분되지 않는다.',
    status: 'to_verify',
    action: '데이터랩 집중률의 산출 방식과 기준 모집단을 확인해야 한다',
  },
  divergence_between_scenarios: {
    total_variation_distance: +tvd.toFixed(4),
    interpretation: '0이면 두 시나리오가 같고 1이면 완전히 다르다. 이 값이 현재 방문객 구성에 대한 정직한 불확실성 폭이다.',
  },
  columns: {
    scenario_id: '시나리오 식별자',
    segment_key: '성·연령 세그먼트 키 (다른 표와 조인)',
    resident_count: '사하구 주민등록 인구 (명)',
    resident_share_pct: '주민 구성비 (%)',
    visitor_propensity_multiplier: '주민 구성 대비 방문 성향 배수. 시나리오 A는 전부 1.0',
    visitor_share_pct: '해당 시나리오의 방문객 구성비 (%)',
    reference_datalab_share_pct: '참고값. 데이터랩 관측 구성비 (%)',
    implied_propensity_vs_resident: '데이터랩 구성 ÷ 주민 구성. 1보다 크면 관광지에서 과대표현되는 세그먼트',
  },
  prohibited_uses: [
    '주민 구성비와 데이터랩 집중률을 곱해서 제3의 분포를 만드는 것',
    'visitor_share_pct 를 실측된 방문객 비율이라고 부르는 것',
    '총 방문객 수를 이 표에서 유도하는 것 (규모는 별도 노브)',
  ],
  open_knobs: [
    'total_visitor_count — 총 방문객 수. 실측 없음, 민감도 노브',
    'resident_vs_external_mix — 배후 주민 유래 방문객과 외지 방문객의 비율. 실측 없음',
    'visitor_propensity_multiplier — 세그먼트별 방문 성향. 시나리오로 대체 중',
  ],
  status: 'assumption',
};
writeFileSync(`${OUT_DIR}/${STEM}.json`, JSON.stringify(meta, null, 2) + '\n', 'utf8');

// ---------- 콘솔 요약 ----------
console.log(`사하구 주민등록 인구 합계: ${residentTotal.toLocaleString()}명\n`);
console.log('세그먼트'.padEnd(16) + '주민(%)'.padStart(9) + '데이터랩(%)'.padStart(12) + '  배수');
for (const k of keys) {
  const rs = resident.get(k).count / residentTotal * 100;
  const ds = (datalab.get(k) ?? 0) / datalabTotal * 100;
  console.log(resident.get(k).label.padEnd(16) + rs.toFixed(2).padStart(9) + ds.toFixed(2).padStart(12) + '  ' + (ds / rs).toFixed(2).padStart(5));
}
console.log(`\n두 시나리오 총변동거리 TVD = ${tvd.toFixed(4)}`);
console.log(`\n생성: ${OUT_DIR}/${STEM}.csv (${rowsOut.length}행)`);
console.log(`생성: ${OUT_DIR}/${STEM}.json`);
