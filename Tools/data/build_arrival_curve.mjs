/**
 * 시간대별 목표 유입 곡선 (수요 곡선) 생성
 *
 * crowdai_handoff_note.md §4의 "시간대별 목표 유입량"을 crowdAI가 코드로 읽을 수 있는
 * 데이터로 만든다. 이 곡선이 정하는 것은 "언제 얼마나 들어와야 하는가"의 목표 분포뿐이며,
 * 실제로 어떤 그룹을 언제 Pool에서 꺼낼지는 crowdAI가 자유롭게 정한다.
 *
 * 설계 결정 4가지 (자세한 근거는 data/metadata/spec/visitor_arrival_curve_spec.md):
 *   1. 유입(신규 도착) 곡선이다 — 동시 체류 인원 목표가 아니다.
 *   2. 절대 인원이 아니라 비율(%)이다 — 합계 100. 소비자가 자기 N을 곱해서 쓴다.
 *   3. 도메인은 Live 단계의 정규화 진행도 [0,1) — 절대 시각이 아니라서
 *      live_stage_duration_minutes 가 바뀌어도 곡선이 그대로 유효하다.
 *   4. 시나리오 여러 개를 함께 낸다 — 곡선 모양이 전부 assumption 이므로
 *      crowdAI가 여러 모양에 대해 견고한지 시험할 수 있어야 한다.
 *
 * 곡선 모양은 Beta 커널 w(p) = p^(a-1) * (1-p)^(b-1) 을 버킷 중앙점에서 평가해 정규화한다.
 * [0,1] 밖으로 새지 않고(축제 시간 안에서 닫힘), 파라미터 2개로 피크 위치와 뾰족함을
 * 따로 조절할 수 있으며, 특수함수 없이 계산된다.
 */
import { readFileSync, writeFileSync } from 'node:fs';

const BUCKET_COUNT = 24; // 기본 240분 기준 버킷 하나 = 10분

// ---- Live 단계 길이·체류시간 기준값은 utility_coefficients_v0.csv 에서 읽는다 (중복 정의 금지) ----
function readCsv(path) {
  const t = readFileSync(path, 'utf8').replace(/^﻿/, '').trim();
  const [h, ...rest] = t.split(/\r?\n/);
  const cols = h.split(',').map(c => c.replace(/^"|"$/g, ''));
  return rest.map(l => {
    const vals = l.match(/(".*?"|[^,]+)(?=,|$)/g).map(v => v.replace(/^"|"$/g, ''));
    return Object.fromEntries(cols.map((c, i) => [c, vals[i]]));
  });
}
const coef = readCsv('data/derived/utility_coefficients_v0.csv');
const coefValue = id => {
  const row = coef.find(r => r.coefficient_id === id);
  if (!row) throw new Error(`utility_coefficients_v0.csv 에 ${id} 가 없다`);
  return +row.default_value;
};
const knobs = readCsv('data/derived/individual_variation_knobs_v0.csv');
const stayKnob = knobs.find(k => k.trait_key === 'stay_duration_multiplier');
if (!stayKnob) throw new Error('individual_variation_knobs_v0.csv 에 stay_duration_multiplier 가 없다');

const LIVE_MINUTES = coefValue('live_stage_duration_minutes');       // 240 (confirmed)
const STAY_SHARE = coefValue('stay_duration_share_of_live_stage_mean'); // 0.5 (assumption)
const STAY_TRI = { min: +stayKnob.min_value, mode: +stayKnob.mode_value, max: +stayKnob.max_value };
const LIVE_START_CLOCK_MINUTES = 17 * 60; // 실제 프로그램 시간표 17:00 (표시용, 곡선 계산에는 안 쓴다)

// ---- 시나리오 정의 ----
// mode(피크 위치) = (a-1)/(a+b-2), concentration(뾰족함) = a+b. 둘 다 draft_assumption.
const SCENARIOS = [
  {
    scenario_id: 'flat_v0',
    a: 1.0,
    b: 1.0,
    label: '균등 유입',
    rationale: '피크가 전혀 없는 기준선. 곡선 모양이 결과에 얼마나 영향을 주는지 재는 대조군이고, crowdAI 구현이 "가장 쉬운 입력"에서 먼저 동작하는지 확인하는 용도다. 현실 가정이 아니다.',
  },
  {
    scenario_id: 'single_peak_v0',
    a: 2.6,
    b: 3.9,
    label: '초중반 완만한 피크',
    rationale: '개장 직후 몰렸다가 서서히 줄어드는 완만한 단봉 곡선. 평균 체류(약 128분)를 고려하면 후반 도착은 축제 종료에 잘려 비현실적이므로 피크를 앞쪽에 둔다. 실측 아님.',
  },
  {
    scenario_id: 'sunset_peak_v0',
    a: 5.0,
    b: 4.3,
    label: '일몰 전후 뾰족한 피크',
    rationale: '야간 프로그램(선셋클럽·선셋펍 등 Nightlife) 시작 전에 몰리는 뾰족한 곡선. 8월 초 부산 일몰이 19:30경이라는 사실은 천문값이지만 "그 시각에 사람이 몰린다"는 것은 우리 추론이며 관측된 값이 아니다.',
  },
];

// ---- Beta 커널로 버킷별 비율 계산 ----
function bucketShares({ a, b }) {
  const mids = Array.from({ length: BUCKET_COUNT }, (_, i) => (i + 0.5) / BUCKET_COUNT);
  const w = mids.map(p => Math.pow(p, a - 1) * Math.pow(1 - p, b - 1));
  const sum = w.reduce((x, y) => x + y, 0);
  return w.map(v => (v / sum) * 100);
}

// ---- 삼각분포 체류시간을 이산화해서 "도착 후 t분까지 아직 남아있을 확률" 을 만든다 ----
// 평균 체류만 쓰면 전원이 같은 시각에 한꺼번에 빠져나가는 계단형 점유가 나와서
// 피크 점유가 실제보다 과장된다. 생존확률로 풀면 퇴장이 자연스럽게 번진다.
function survivalCurve(stepMinutes, steps) {
  const { min, mode, max } = STAY_TRI;
  const base = LIVE_MINUTES * STAY_SHARE;
  // 삼각분포 CDF (multiplier 공간) → 체류시간(분) 공간
  const c = (mode - min) / (max - min);
  const cdfMultiplier = m => {
    if (m <= min) return 0;
    if (m >= max) return 1;
    return m < mode
      ? ((m - min) * (m - min)) / ((max - min) * (mode - min))
      : 1 - ((max - m) * (max - m)) / ((max - min) * (max - mode));
  };
  return Array.from({ length: steps + 1 }, (_, s) => 1 - cdfMultiplier((s * stepMinutes) / base));
}

// ---- 점유(동시 체류) 진단: 유입 곡선 + 체류 분포 → 피크 점유 비율 ----
// 결과는 "총 유입 N명 중 몇 배수가 동시에 있는가"이며, 축제 종료 시점에 남은 인원은
// 그 자리에서 전원 퇴장한다고 본다(창 밖으로 새는 체류는 자르지 않고 그대로 둔다).
function occupancyDiagnostics(shares) {
  const stepMinutes = LIVE_MINUTES / BUCKET_COUNT;
  const survival = survivalCurve(stepMinutes, BUCKET_COUNT);
  const occupancy = Array.from({ length: BUCKET_COUNT }, (_, t) => {
    let acc = 0;
    for (let i = 0; i <= t; i++) acc += (shares[i] / 100) * survival[t - i];
    return acc; // 총 유입 N 대비 비율
  });
  const peak = Math.max(...occupancy);
  return { occupancy, peakOccupancyRatio: peak, peakOccupancyBucket: occupancy.indexOf(peak) };
}

const fmtClock = minutesFromMidnight => {
  const total = Math.round(minutesFromMidnight);
  return `${String(Math.floor(total / 60) % 24).padStart(2, '0')}:${String(total % 60).padStart(2, '0')}`;
};

// ---- 산출 ----
const bucketRows = [];
const summaries = [];
for (const sc of SCENARIOS) {
  const shares = bucketShares(sc);
  const meanShare = 100 / BUCKET_COUNT;
  const peakShare = Math.max(...shares);
  const diag = occupancyDiagnostics(shares);
  let cumulative = 0;
  shares.forEach((share, i) => {
    cumulative += share;
    const progressStart = i / BUCKET_COUNT;
    const progressEnd = (i + 1) / BUCKET_COUNT;
    bucketRows.push({
      scenario_id: sc.scenario_id,
      bucket_index: i,
      bucket_count: BUCKET_COUNT,
      progress_start: progressStart.toFixed(6),
      progress_end: progressEnd.toFixed(6),
      minutes_start_at_default: (progressStart * LIVE_MINUTES).toFixed(1),
      minutes_end_at_default: (progressEnd * LIVE_MINUTES).toFixed(1),
      clock_start_at_default: fmtClock(LIVE_START_CLOCK_MINUTES + progressStart * LIVE_MINUTES),
      clock_end_at_default: fmtClock(LIVE_START_CLOCK_MINUTES + progressEnd * LIVE_MINUTES),
      arrival_share_pct: share.toFixed(4),
      cumulative_share_pct: cumulative.toFixed(4),
      status: 'draft_assumption',
    });
  });
  summaries.push({
    scenario_id: sc.scenario_id,
    label: sc.label,
    rationale: sc.rationale,
    beta_a: sc.a,
    beta_b: sc.b,
    peak_bucket_index: shares.indexOf(peakShare),
    peak_clock_at_default: fmtClock(LIVE_START_CLOCK_MINUTES + ((shares.indexOf(peakShare) + 0.5) / BUCKET_COUNT) * LIVE_MINUTES),
    peak_to_mean_arrival_ratio: +(peakShare / meanShare).toFixed(4),
    peak_concurrent_occupancy_ratio: +diag.peakOccupancyRatio.toFixed(4),
    peak_occupancy_bucket_index: diag.peakOccupancyBucket,
    peak_occupancy_clock_at_default: fmtClock(LIVE_START_CLOCK_MINUTES + ((diag.peakOccupancyBucket + 0.5) / BUCKET_COUNT) * LIVE_MINUTES),
    sum_check_pct: +shares.reduce((x, y) => x + y, 0).toFixed(6),
    status: 'draft_assumption',
  });
}

const cols = Object.keys(bucketRows[0]);
writeFileSync('data/derived/visitor_arrival_curve_v0.csv',
  '﻿' + [cols.join(','), ...bucketRows.map(r => cols.map(c => `"${r[c]}"`).join(','))].join('\r\n') + '\r\n', 'utf8');

writeFileSync('data/derived/visitor_arrival_curve_v0.json', JSON.stringify({
  schema_version: '0.1.0',
  created_at: '2026-08-06',
  dataset_id: 'visitor_arrival_curve_v0',
  status: 'draft — 곡선 인터페이스(형태·단위·도메인)는 확정, 곡선 모양 수치는 전부 draft_assumption',
  purpose: 'crowdAI가 그룹 입장 타이밍을 정할 때 맞춰야 할 시간대별 목표 유입 분포. crowdai_handoff_note.md §4의 "시간대별 목표 유입량"의 실제 형태.',
  row_unit: '한 시나리오 × 한 시간 버킷',
  interface_contract: {
    quantity: '신규 유입(그 버킷에 새로 입장하는 인원)이다. 동시 체류 인원 목표가 아니다.',
    unit: '전체 유입 대비 비율(%). 시나리오별 합계 = 100. 절대 인원이 아니다.',
    consumer_formula: '그 버킷의 목표 인원 = arrival_share_pct / 100 × (소비자가 정한 총 유입 인원)',
    domain: 'Live 단계의 정규화 진행도 [0,1) 을 24개 버킷으로 나눈 것. 절대 시각이 아니다.',
    default_bucket_minutes: LIVE_MINUTES / BUCKET_COUNT,
    why_normalized: 'live_stage_duration_minutes(현재 240, 범위 120~480)가 바뀌어도 곡선을 다시 만들 필요가 없다. 소비자가 진행도를 자기 시계에 곱하면 된다.',
    group_wholeness_tolerance: '그룹은 통째로 입장해야 하므로(§4 제약) 그룹 크기가 1~4명으로 덩어리져 버킷 목표를 정확히 맞출 수 없다. 버킷별 오차는 정상이며, 관리해야 할 것은 누적 유입이 cumulative_share_pct 를 크게 벗어나지 않는 것뿐이다.',
  },
  scenarios: summaries,
  derived_metrics_note: {
    peak_to_mean_arrival_ratio: 'public_data_decision_log.md 2026-08-05 항목의 "피크·평균 비율 1.4(draft_assumption)"를 대체하는 값이다. 이제 곡선에서 계산되는 파생값이며, 따로 가정할 노브가 아니다.',
    peak_concurrent_occupancy_ratio: '총 유입 N명일 때 동시 체류 피크가 N의 몇 배인지. 유입 곡선과 체류시간 삼각분포(stay_duration_multiplier)를 합성해 계산했다.',
    prohibited_use: 'peak_concurrent_occupancy_ratio 로 total_visitor_count 를 역산해 낮추지 말 것. 2026-08-05에 같은 방식(700에서 950 역산)을 시도했다가 근거 없는 숫자에서 또 역산하면 거짓 정밀도만 남는다는 이유로 되돌린 전례가 있다. 이 값은 "수요가 스폰 상한을 넘을 때 흡수 정책이 감당해야 할 규모"를 가늠하는 진단용이다.',
  },
  inputs: {
    live_stage_duration_minutes: { value: LIVE_MINUTES, status: 'confirmed', source: 'utility_coefficients_v0.csv' },
    stay_duration_share_of_live_stage_mean: { value: STAY_SHARE, status: 'assumption', source: 'utility_coefficients_v0.csv' },
    stay_duration_multiplier_triangular: { ...STAY_TRI, status: 'draft_assumption', source: 'individual_variation_knobs_v0.csv' },
  },
  shape_basis: {
    what_is_real: [
      'Live 단계가 17:00~21:00(240분)이라는 것 — 실제 부산바다축제 프로그램 시간표 기준, status confirmed.',
      '야간 Nightlife 프로그램(선셋클럽·선셋펍)이 실제로 존재한다는 것 — 리플렛 대조 확인(facility_naming_convention.md).',
    ],
    what_is_assumed: [
      '곡선이 단봉(하나의 피크)이라는 것.',
      '피크 위치와 뾰족함(Beta a·b) 수치 전부.',
      '"일몰·야간 프로그램 직전에 사람이 몰린다"는 방향성.',
    ],
    rejected_basis: {
      source_id: 'transport_expressway_hourly_2025_06_30',
      why: '전국 고속도로 영업소 시간대별 교통량은 통행 목적이 없고 통근 통행이 지배해 16~17시에 피크를 찍고 저녁 내내 감소한다 — 저녁 축제의 유입 형태와 방향이 반대다. 탑승 인원·축제 방문 여부도 알 수 없다. 축제 유입 곡선의 근거로 쓰지 않는다.',
    },
  },
  regeneration: 'node Tools/data/build_arrival_curve.mjs',
  spec: 'data/metadata/spec/visitor_arrival_curve_spec.md',
}, null, 2) + '\n', 'utf8');

// ---- Unreal DataTable CSV ----
const unrealCols = ['Name', 'ScenarioId', 'BucketIndex', 'BucketCount', 'ProgressStart', 'ProgressEnd',
  'ArrivalSharePct', 'CumulativeSharePct', 'MinutesStartAtDefault', 'ClockStartAtDefault', 'Status'];
const unrealRows = bucketRows.map(r => ({
  Name: `${r.scenario_id}_${String(r.bucket_index).padStart(2, '0')}`,
  ScenarioId: r.scenario_id,
  BucketIndex: r.bucket_index,
  BucketCount: r.bucket_count,
  ProgressStart: r.progress_start,
  ProgressEnd: r.progress_end,
  ArrivalSharePct: r.arrival_share_pct,
  CumulativeSharePct: r.cumulative_share_pct,
  MinutesStartAtDefault: r.minutes_start_at_default,
  ClockStartAtDefault: r.clock_start_at_default,
  Status: r.status,
}));
writeFileSync('data/unreal/visitor_arrival_curve_v0.csv',
  '﻿' + [unrealCols.join(','), ...unrealRows.map(r => unrealCols.map(c => `"${r[c]}"`).join(','))].join('\r\n') + '\r\n', 'utf8');

// ---- 콘솔 요약 ----
console.log(`생성: data/derived/visitor_arrival_curve_v0.csv (${bucketRows.length}행 = 시나리오 ${SCENARIOS.length} × 버킷 ${BUCKET_COUNT})`);
console.log(`생성: data/unreal/visitor_arrival_curve_v0.csv (${unrealRows.length}행)\n`);
console.log('시나리오'.padEnd(18) + '피크시각  유입피크/평균  동시체류피크(N배)  점유피크시각  합계검산');
for (const s of summaries) {
  console.log(
    s.scenario_id.padEnd(18) +
    s.peak_clock_at_default.padStart(7) +
    String(s.peak_to_mean_arrival_ratio.toFixed(2)).padStart(13) +
    String(s.peak_concurrent_occupancy_ratio.toFixed(4)).padStart(17) +
    s.peak_occupancy_clock_at_default.padStart(13) +
    String(s.sum_check_pct).padStart(11));
}
console.log('\n동시체류피크는 "총 유입 N명 중 동시에 있는 최대 인원 / N" 이다.');
console.log(`예: N=${coefValue('total_visitor_count').toLocaleString()}명(total_visitor_count) 기준 동시 체류 피크:`);
for (const s of summaries) {
  console.log(`  ${s.scenario_id.padEnd(18)} ${Math.round(coefValue('total_visitor_count') * s.peak_concurrent_occupancy_ratio).toLocaleString().padStart(9)}명`);
}
console.log('\n(위 인원 수는 진단용이다 — 이 숫자로 total_visitor_count 를 역산해 낮추지 않는다.)');
