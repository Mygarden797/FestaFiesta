/**
 * 단계 A 검증 — 행정안전부 주민등록 연령별 인구현황 원본 검사 (읽기 전용)
 *
 * 목적: data/raw 에 등록하기 전에 파일이 전체 연령 분포를 담고 있는지 확인한다.
 * 원본 파일은 절대 수정하지 않는다.
 *
 * 사용법: node Tools/data/verify_population.mjs <원본CSV경로>
 */
import { readFileSync } from 'node:fs';

const src = process.argv[2];
if (!src) { console.error('사용법: node verify_population.mjs <csv>'); process.exit(2); }

const text = new TextDecoder('euc-kr').decode(readFileSync(src));
const lines = text.trim().split(/\r?\n/).filter(l => l.trim());

/** CP949 CSV 한 줄을 셀 배열로 분해. 숫자의 천단위 콤마를 제거한다. */
const parse = l => l.split('","').map(x => x.replace(/^"|"$/g, '').replace(/,/g, '').trim());

const header = parse(lines[0]);
const rows = lines.slice(1).map(parse);

const checks = [];
const ok = (name, pass, detail) => checks.push({ name, pass, detail });

// 1) 필수 연령 열 존재 여부 — 인수인계 문서 6절의 판정 기준
const required = ['0세', '1세', '20세', '50세', '100세 이상'];
for (const age of required) {
  const found = header.some(h => h.endsWith(`_계_${age}`));
  ok(`연령 열 '${age}' 존재`, found, found ? '' : '누락');
}

// 2) 성별 3계열이 모두 있는지
for (const sex of ['계', '남', '여']) {
  const n = header.filter(h => /_(계|남|여)_\d+세|_(계|남|여)_100세 이상/.test(h) && h.includes(`_${sex}_`)).length;
  ok(`성별 '${sex}' 연령 열 개수`, n === 101, `${n}개 (기대 101)`);
}

// 3) 열 개수: 행정구역 1 + (총인구수 + 연령구간인구수 + 101연령) × 3성별 = 310
ok('총 열 개수', header.length === 310, `${header.length}개 (기대 310)`);

// 4) 지역 구성
const regions = rows.map(r => r[0]);
ok('행 수', rows.length === 17, `${rows.length}행 (사하구 합계 1 + 행정동 16 기대)`);
ok('사하구 합계 행 위치', /사하구 \(/.test(regions[0]), regions[0]);

// 5) 값 검증 — 연령 합계 = 연령구간인구수, 남+여 = 계
const idx = name => header.indexOf(name);
const period = header[1].match(/^(\d{4}년\d{2}월)/)[1];
const ageCols = sex => header.map((h, i) => ({ h, i }))
  .filter(o => new RegExp(`_${sex}_(\\d+세|100세 이상)$`).test(o.h)).map(o => o.i);

let sumFail = 0, sexFail = 0;
for (const r of rows) {
  for (const sex of ['계', '남', '여']) {
    const bandTotal = +r[idx(`${period}_${sex}_연령구간인구수`)];
    const ageSum = ageCols(sex).reduce((a, i) => a + (+r[i] || 0), 0);
    if (bandTotal !== ageSum) sumFail++;
  }
  const t = +r[idx(`${period}_계_총인구수`)];
  const m = +r[idx(`${period}_남_총인구수`)];
  const f = +r[idx(`${period}_여_총인구수`)];
  if (t !== m + f) sexFail++;
}
ok('연령 합 = 연령구간인구수', sumFail === 0, `불일치 ${sumFail}건`);
ok('남 + 여 = 계', sexFail === 0, `불일치 ${sexFail}건`);

// 6) 행정동 합 vs 사하구 합계 (행정구역 정의 차이 확인용 — 불일치 자체는 실패가 아님)
const totalIdx = idx(`${period}_계_총인구수`);
const sahaTotal = +rows[0][totalIdx];
const dongSum = rows.slice(1).reduce((a, r) => a + (+r[totalIdx] || 0), 0);
ok('행정동 합 = 사하구 합계', sahaTotal === dongSum,
  `사하구 ${sahaTotal.toLocaleString()} vs 행정동 합 ${dongSum.toLocaleString()} (차 ${sahaTotal - dongSum})`);

console.log(`기준시점: ${period}`);
console.log(`파일: ${src}\n`);
for (const c of checks) console.log(`  ${c.pass ? 'PASS' : 'FAIL'}  ${c.name}${c.detail ? '  — ' + c.detail : ''}`);

const failed = checks.filter(c => !c.pass);
console.log(`\n판정: ${failed.length === 0 ? '채택 가능' : `검토 필요 (${failed.length}건)`}`);
process.exit(failed.length === 0 ? 0 : 1);
