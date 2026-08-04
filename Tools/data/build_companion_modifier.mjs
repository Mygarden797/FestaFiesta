/**
 * 단계 F4 — 동반유형 오버레이 모디파이어
 *
 * 생애주기 아키타입과 별개로, 모든 아키타입에 공통으로 적용되는 동반유형(1인/2인/3인+)
 * 보정값을 만든다. 나이×동반유형 교차표가 없으므로 이 보정은 "그 생애주기에서 그
 * 동반유형이면 얼마나 다른가"가 아니라 "전체 평균에서 그 동반유형이면 얼마나 다른가"이며,
 * 모든 생애주기에 동일하게 적용한다(독립가정, assumption).
 *
 * 효용계수(β)가 아니다. Codex 리뷰 반영: 응답률 차이는 순위·경향 참고용이며
 * 그대로 weight_* 계수에 대입하지 않는다.
 */
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

const activity = readCsv('data/cleaned/tourism/tour_stat_2018_companion_activity.csv');
const reason = readCsv('data/cleaned/tourism/tour_stat_2018_companion_reason.csv');
const mapping = readCsv('data/derived/festival_activity_category_mapping_v0.csv');
const CATEGORIES = [...new Set(mapping.map(m => m.festival_category))].filter(c => c !== 'not_applicable');

const rateOf = (table, groupKey, item) => {
  const row = table.find(r => r.group_key === groupKey && r.item === item);
  return row ? +row.response_rate_pct : null;
};
function categoryScore(table, groupKey, category) {
  const items = mapping.filter(m => m.festival_category === category).map(m => m.tour_stat_item);
  const vals = items.map(it => rateOf(table, groupKey, it)).filter(v => v !== null);
  return vals.length ? vals.reduce((a, b) => a + b, 0) / vals.length : null;
}

const GROUPS = [
  { key: 'solo', label_draft: '1인 동반' },
  { key: 'pair', label_draft: '2인 동반' },
  { key: 'group_3_plus', label_draft: '3인 이상 동반' },
];

const rowsOut = [];
for (const g of GROUPS) {
  const row = { companion_type: g.key, label_draft: g.label_draft, status: 'draft_assumption' };
  for (const c of CATEGORIES) {
    const score = categoryScore(activity, g.key, c);
    const baseline = categoryScore(activity, 'all', c);
    row[`${c}_response_rate_pct`] = score !== null ? score.toFixed(2) : '';
    row[`${c}_delta_vs_all_pp`] = (score !== null && baseline !== null) ? (score - baseline).toFixed(2) : '';
  }
  // 방문지 선택 이유 중 전체 대비 편차가 큰 상위 2개 (참고용)
  const deltas = reason.filter(r => r.group_key === g.key).map(r => {
    const base = +reason.find(b => b.group_key === 'all' && b.item === r.item).response_rate_pct;
    return { item: r.item, delta: +r.response_rate_pct - base };
  }).sort((a, b) => Math.abs(b.delta) - Math.abs(a.delta));
  row.top_reason_delta_1 = `${deltas[0].item} (${deltas[0].delta >= 0 ? '+' : ''}${deltas[0].delta.toFixed(1)}pp)`;
  row.top_reason_delta_2 = `${deltas[1].item} (${deltas[1].delta >= 0 ? '+' : ''}${deltas[1].delta.toFixed(1)}pp)`;
  rowsOut.push(row);
}

const cols = Object.keys(rowsOut[0]);
writeFileSync('data/derived/companion_type_modifier_v0.csv',
  '﻿' + [cols.join(','), ...rowsOut.map(o => cols.map(c => `"${o[c]}"`).join(','))].join('\r\n') + '\r\n', 'utf8');

writeFileSync('data/derived/companion_type_modifier_v0.json', JSON.stringify({
  schema_version: '0.1.0',
  created_at: '2026-08-04',
  dataset_id: 'companion_type_modifier_v0',
  status: 'draft — 사용자 확인 전, 이름·필드 확정 아님',
  design_rule: '이 표의 보정값은 모든 생애주기 아키타입에 동일한 공식으로 적용되는 개체 오버레이다 (2026-08-04 확정: 옵션 1). 나이×동반유형 교차표가 없으므로 생애주기별로 다르게 적용하지 않는다.',
  row_unit: '한 동반유형',
  independence_assumption: '동반유형 효과가 생애주기와 무관하게 일정하다고 가정한다. 실제로는 상호작용이 있을 수 있으나 이를 관측한 자료가 없다.',
  columns: {
    '{category}_response_rate_pct': '해당 동반유형의 카테고리 매핑 항목 평균 응답률(%)',
    '{category}_delta_vs_all_pp': '전체 대비 percentage-point 차이. 효용계수가 아니라 경향 신호',
    top_reason_delta_1: '전체 대비 편차가 가장 큰 방문지 선택 이유 (참고용)',
  },
  prohibited_uses: [
    '이 표의 delta 값을 그대로 weight_* 계수에 대입하는 것',
    'label_draft를 최종 명칭으로 사용하는 것',
  ],
  source_id: 'tourism_legacy_tables_2018',
}, null, 2) + '\n', 'utf8');

console.log(`생성: data/derived/companion_type_modifier_v0.csv (${rowsOut.length}행)\n`);
console.log('동반유형'.padEnd(12) + CATEGORIES.map(c => c.slice(0, 8).padStart(10)).join('') + '  주요 편차 항목');
for (const r of rowsOut) {
  console.log(r.label_draft.padEnd(12) + CATEGORIES.map(c => (r[`${c}_delta_vs_all_pp`] || '-').padStart(10)).join('') + '  ' + r.top_reason_delta_1);
}
