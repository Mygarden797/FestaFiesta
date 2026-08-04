// 생애주기 아키타입 후보의 인구 비중 (주민 시나리오 vs 데이터랩 관측 시나리오)
import { readFileSync } from 'node:fs';
function readCsv(path) {
  const t = readFileSync(path, 'utf8').replace(/^﻿/, '').trim();
  const [h, ...rest] = t.split(/\r?\n/);
  const cols = h.split(',');
  return rest.map(l => Object.fromEntries(l.split(',').map((v, i) => [cols[i], v])));
}
const rows = readCsv('data/derived/visitor_population_prior_v0.csv');
const ageOf = k => k.replace(/_(male|female)$/, '');
const bands = [...new Set(rows.map(r => ageOf(r.segment_key)))];
const scenarios = [...new Set(rows.map(r => r.scenario_id))];

console.log('연령대'.padEnd(14) + scenarios.map(s => s.padStart(20)).join(''));
for (const b of bands) {
  const line = scenarios.map(s => {
    const sub = rows.filter(r => r.scenario_id === s && ageOf(r.segment_key) === b);
    return sub.reduce((a, r) => a + +r.visitor_share_pct, 0);
  });
  console.log(b.padEnd(14) + line.map(x => x.toFixed(2).padStart(20)).join(''));
}
