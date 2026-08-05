/**
 * MVP 아키타입 후보 — 세그먼트를 취향 벡터로 군집화
 *
 * data/derived/affinity_prior_equal_site_v0.csv (Codex 리뷰를 거친 검증본)의
 * centered_log_share_ratio를 대분류 4종(역사관광/자연관광/문화관광/쇼핑) 평균으로
 * 세그먼트별 4차원 벡터를 만들고, 유클리드 거리로 계층적 군집화한다.
 *
 * 목적: "가족", "커플" 같은 이름을 먼저 정하지 않고, 실제로 취향이 비슷하게
 * 관측된 세그먼트끼리 데이터로 묶는다.
 */
import { readFileSync } from 'node:fs';

function readCsv(path) {
  const t = readFileSync(path, 'utf8').replace(/^﻿/, '').trim();
  const [h, ...rest] = t.split(/\r?\n/);
  const cols = h.split(',');
  return rest.map(l => Object.fromEntries(l.split(',').map((v, i) => [cols[i], v])));
}

const rows = readCsv('data/derived/affinity_prior_equal_site_v0.csv');
const CATS = [...new Set(rows.map(r => r.major_category))];
const SEGS = [...new Set(rows.map(r => r.segment_key))];

// 대분류별 관광지 수 (신뢰도 판단용)
const siteCountByCat = {};
for (const c of CATS) siteCountByCat[c] = new Set(rows.filter(r => r.major_category === c).map(r => r.site_name)).size;

// 세그먼트 × 대분류 평균 centered_log_share_ratio
const vec = {};
for (const s of SEGS) {
  vec[s] = CATS.map(c => {
    const sub = rows.filter(r => r.segment_key === s && r.major_category === c);
    return sub.reduce((a, r) => a + +r.centered_log_share_ratio, 0) / sub.length;
  });
}
const label = s => rows.find(r => r.segment_key === s).segment_label;

console.log('=== 대분류별 관광지 수 (신뢰도) ===');
CATS.forEach(c => console.log(`  ${c.padEnd(8)} ${siteCountByCat[c]}곳`));

console.log('\n=== 세그먼트 × 대분류 평균 affinity ===');
console.log('세그먼트'.padEnd(14) + CATS.map(c => c.padStart(10)).join(''));
SEGS.forEach(s => console.log(label(s).padEnd(14) + vec[s].map(x => x.toFixed(2).padStart(10)).join('')));

// 유클리드 거리 행렬
const dist = (a, b) => Math.sqrt(a.reduce((sum, x, i) => sum + (x - b[i]) ** 2, 0));

console.log('\n=== 가장 가까운 세그먼트 쌍 (거리 오름차순, 상위 10) ===');
const pairs = [];
for (let i = 0; i < SEGS.length; i++)
  for (let j = i + 1; j < SEGS.length; j++)
    pairs.push({ a: SEGS[i], b: SEGS[j], d: dist(vec[SEGS[i]], vec[SEGS[j]]) });
pairs.sort((x, y) => x.d - y.d).slice(0, 10)
  .forEach(p => console.log(`  ${label(p.a).padEnd(12)} - ${label(p.b).padEnd(12)}  d=${p.d.toFixed(3)}`));

// 단순 계층적 군집화 (average linkage) — k=4,5,6 절단점 확인용으로 병합 순서만 출력
console.log('\n=== 계층적 병합 순서 (average linkage) ===');
let clusters = SEGS.map(s => ({ members: [s], vec: vec[s] }));
const clusterDist = (c1, c2) => {
  let sum = 0, n = 0;
  for (const m1 of c1.members) for (const m2 of c2.members) { sum += dist(vec[m1], vec[m2]); n++; }
  return sum / n;
};
const merges = [];
while (clusters.length > 1) {
  let best = { i: 0, j: 1, d: Infinity };
  for (let i = 0; i < clusters.length; i++)
    for (let j = i + 1; j < clusters.length; j++) {
      const d = clusterDist(clusters[i], clusters[j]);
      if (d < best.d) best = { i, j, d };
    }
  const merged = { members: [...clusters[best.i].members, ...clusters[best.j].members] };
  merges.push({ size: merged.members.length, d: best.d, members: merged.members.map(label) });
  clusters = clusters.filter((_, k) => k !== best.i && k !== best.j).concat([merged]);
}
merges.forEach(m => console.log(`  n=${String(m.size).padStart(2)}  d=${m.d.toFixed(3)}  [${m.members.join(', ')}]`));
