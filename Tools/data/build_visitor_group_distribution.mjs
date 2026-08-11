/**
 * 방문객 그룹 크기·아동 구성 분포 — 2025 국민여행조사 원자료(국내여행) 가중 집계
 *
 * 목적: FFF_VisitorDistributionBuilder 가 "개체 하나하나에 CompanionSizeTag·
 * bIsDependentChildSlot 을 태그"하려면 그룹 크기 분포와 그룹 내 아동 수 분포가 필요하다.
 * 2026-08-06까지 이 두 분포의 출처가 프로젝트에 없었다(companion_type_modifier_v0.csv 는
 * 카테고리별 응답률만 있고 비중이 없었고, 그 출처인 Tour_Stat 2018 표는 `가구원수`였다).
 *
 * 사용 변수 (SAV 변수 라벨로 확인, 2026-08-06):
 *   A6            "함께 여행한 일행은 귀하를 포함하여 총 몇 명이었습니까?[단위:명]"
 *   A6A           "(2명 이상인 경우) 일행 중 만 15세 미만은 몇 명이었습니까?[단위:명]"
 *   A6B_1..12     "함께 여행한 일행은 어떤 분들이었습니까?" (복수응답)
 *   D_TRA{n}_CASE  여행유형 — 1/2/4 가 관광·휴양 활동 포함, 3/5 는 단순 방문·출장
 *   D_TRA{n}_S_Day 여행일수(박) — 0 이면 당일여행
 *   D_TRA{n}_CHECK 본설문진행여부 — A 블록이 어느 여행에 대한 것인지 지정
 *   WT_DOM         국내여행 분석 가중치
 *
 * 쓰지 말아야 할 변수: `D_TRA{n}_NUM` 은 "지출비용에 포함되는 인원수"(비용 분담 인원)이며
 * 여행 인원도 동반자 수도 아니다. ONE_COST == COST/NUM 이 100% 성립하는 것으로 확인했고,
 * 이 값으로 그룹 크기 분포를 만들면 공표표와 25~30pp 어긋난다.
 *
 * 검증: 산출된 분포를 통계편 표 14-1-1/2/3 과 대조한다(아래 PUBLISHED). 최대오차 0.1pp
 * 이내여야 하며, 넘으면 이 스크립트는 실패한다 — 집계 단위가 틀어졌다는 뜻이다.
 */
import { readFileSync, writeFileSync, openSync, readSync, fstatSync, closeSync, createReadStream, existsSync } from 'node:fs';
import { createInflateRaw } from 'node:zlib';

const XLSX = 'data/extracted/tourism/national_travel_2025/2025년 국민여행조사 원자료(국내여행).xlsx';
const GROUP_SIZE_CAP = 6; // 6명 초과는 6명으로 합친다 (2026-08-06 사용자 결정, 실측상 초과분 4.3%)

if (!existsSync(XLSX)) {
  console.error(`원자료가 없다: ${XLSX}`);
  console.error('data/extracted 는 git 제외 대상이다. ZIP(data/raw/tourism/2025년 국민여행조사 원자료(국내여행).zip)을 그 경로에 풀고 다시 실행할 것.');
  process.exit(1);
}

// ---------- zip 엔트리 스트리밍 (의존성 없이 xlsx 를 읽는다) ----------
function findEntries(path) {
  const fd = openSync(path, 'r');
  const size = fstatSync(fd).size;
  const tailLen = Math.min(size, 65557 + 64);
  const tail = Buffer.alloc(tailLen);
  readSync(fd, tail, 0, tailLen, size - tailLen);
  let eocd = -1;
  for (let i = tail.length - 22; i >= 0; i--) if (tail.readUInt32LE(i) === 0x06054b50) { eocd = i; break; }
  if (eocd < 0) throw new Error('zip EOCD 를 못 찾음');
  let cdSize = tail.readUInt32LE(eocd + 12), cdOff = tail.readUInt32LE(eocd + 16);
  if (cdOff === 0xffffffff || cdSize === 0xffffffff) {
    for (let i = eocd - 20; i >= 0; i--) if (tail.readUInt32LE(i) === 0x07064b50) {
      const z64 = Number(tail.readBigUInt64LE(i + 8));
      const z = Buffer.alloc(56); readSync(fd, z, 0, 56, z64);
      cdSize = Number(z.readBigUInt64LE(40)); cdOff = Number(z.readBigUInt64LE(48));
      break;
    }
  }
  const cd = Buffer.alloc(cdSize); readSync(fd, cd, 0, cdSize, cdOff);
  const out = new Map();
  let p = 0;
  while (p < cd.length && cd.readUInt32LE(p) === 0x02014b50) {
    const csize = cd.readUInt32LE(p + 20);
    const nlen = cd.readUInt16LE(p + 28), elen = cd.readUInt16LE(p + 30), clen = cd.readUInt16LE(p + 32);
    const lho = cd.readUInt32LE(p + 42);
    const name = cd.toString('utf8', p + 46, p + 46 + nlen);
    const lh = Buffer.alloc(30); readSync(fd, lh, 0, 30, lho);
    out.set(name, { dataOff: lho + 30 + lh.readUInt16LE(26) + lh.readUInt16LE(28), csize });
    p += 46 + nlen + elen + clen;
  }
  closeSync(fd);
  return out;
}

function streamEntry(path, entry, onChunk) {
  return new Promise((resolve, reject) => {
    const rs = createReadStream(path, { start: entry.dataOff, end: entry.dataOff + entry.csize - 1, highWaterMark: 1 << 20 });
    const inf = createInflateRaw();
    let stopped = false;
    const stop = () => { if (!stopped) { stopped = true; rs.destroy(); inf.destroy(); resolve(); } };
    inf.on('data', d => { if (!stopped) { try { onChunk(d.toString('utf8'), stop); } catch (e) { stopped = true; rs.destroy(); inf.destroy(); reject(e); } } });
    inf.on('end', () => !stopped && resolve());
    inf.on('error', e => !stopped && reject(e));
    rs.on('error', e => !stopped && reject(e));
    rs.pipe(inf);
  });
}

const entries = findEntries(XLSX);

// ---------- sharedStrings (헤더 이름 해석용) ----------
let ssBuf = '';
await streamEntry(XLSX, entries.get('xl/sharedStrings.xml'), t => { ssBuf += t; });
const sharedStrings = [];
for (const m of ssBuf.matchAll(/<si>([\s\S]*?)<\/si>/g)) {
  sharedStrings.push([...m[1].matchAll(/<t[^>]*>([\s\S]*?)<\/t>/g)].map(x => x[1]).join('')
    .replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&quot;/g, '"').replace(/&#39;/g, "'"));
}
ssBuf = '';

// ---------- 헤더 행에서 변수명 → 열문자 ----------
let headerRow = '';
await streamEntry(XLSX, entries.get('xl/worksheets/sheet1.xml'), (text, stop) => {
  headerRow += text;
  const end = headerRow.indexOf('</row>');
  if (end >= 0) { headerRow = headerRow.slice(0, end); stop(); }
});
const colOf = new Map();
for (const m of headerRow.matchAll(/<c r="([A-Z]+)1"(?:[^>]*?\st="([^"]+)")?[^>]*>([\s\S]*?)<\/c>/g)) {
  const v = /<v>([\s\S]*?)<\/v>/.exec(m[3])?.[1];
  if (v === undefined) continue;
  colOf.set(m[2] === 's' ? sharedStrings[+v] : v, m[1]);
}
headerRow = '';

const needed = ['A6', 'A6A', 'WT_DOM'];
for (let i = 1; i <= 12; i++) needed.push(`A6B_${i}`);
for (let n = 1; n <= 6; n++) needed.push(`D_TRA${n}_CHECK`, `D_TRA${n}_CASE`, `D_TRA${n}_S_Day`);
const L = new Map();
for (const name of needed) {
  const c = colOf.get(name);
  if (!c) throw new Error(`원자료 헤더에 ${name} 이 없다 — 원자료 버전이 바뀐 것일 수 있다`);
  L.set(name, c);
}
const col = n => L.get(n);

// ---------- 본문 스트리밍 집계 ----------
const alt = [...new Set(L.values())].sort((a, b) => b.length - a.length).join('|');
const cellRe = new RegExp('<c r="(' + alt + ')(\\d+)"[^>]*?(?:/>|>([\\s\\S]*?)</c>)', 'g');
const valRe = /<v>([\s\S]*?)<\/v>/;

const TOURISM_CASES = new Set(['1', '2', '4']); // 관광·휴양 활동 포함
const sizeW = new Map();   // `${scope}|${size}` -> 가중합
const sizeN = new Map();   // 표본 건수
const childW = new Map();  // `${scope}|${cappedSize}|${nChild}` -> 가중합
const typeW = new Map();   // `${scope}|${cappedSize}|${typeCode}` -> 가중합
let rowCount = 0, used = 0;
const bump = (m, k, v) => m.set(k, (m.get(k) ?? 0) + v);

function handleRow(row) {
  rowCount++;
  if (rowCount === 1) return;
  cellRe.lastIndex = 0;
  const cells = new Map();
  let m;
  while ((m = cellRe.exec(row)) !== null) {
    if (m[3] === undefined) continue;
    const v = valRe.exec(m[3])?.[1];
    if (v !== undefined && v !== '') cells.set(m[1], v);
  }
  const wt = +cells.get(col('WT_DOM'));
  if (!Number.isFinite(wt) || wt <= 0) return;

  // A 블록(본설문)이 어느 여행 블록에 대한 것인지
  let picked = null;
  for (let n = 1; n <= 6; n++) if (cells.get(col(`D_TRA${n}_CHECK`)) !== undefined) { picked = n; break; }
  if (picked === null) return;

  const size = Math.round(+cells.get(col('A6')));
  if (!Number.isFinite(size) || size < 1) return;
  used++;

  const caseCode = String(cells.get(col(`D_TRA${picked}_CASE`)));
  const sDayRaw = cells.get(col(`D_TRA${picked}_S_Day`));
  const sDay = sDayRaw === undefined ? null : Math.round(+sDayRaw);
  if (!TOURISM_CASES.has(caseCode)) return;

  const scopes = ['tourism_all', sDay === 0 ? 'tourism_daytrip' : 'tourism_overnight'];
  const cappedSize = Math.min(size, GROUP_SIZE_CAP);
  const nChildRaw = Math.round(+cells.get(col('A6A')));
  const nChild = Number.isFinite(nChildRaw) && nChildRaw >= 0 ? Math.min(nChildRaw, cappedSize) : 0;
  for (const sc of scopes) {
    bump(sizeW, `${sc}|${size}`, wt);
    bump(sizeN, `${sc}|${size}`, 1);
    bump(childW, `${sc}|${cappedSize}|${nChild}`, wt);
    for (let i = 1; i <= 12; i++) {
      const t = cells.get(col(`A6B_${i}`));
      if (t !== undefined && +t >= 1 && +t <= 12) bump(typeW, `${sc}|${cappedSize}|${t}`, wt);
    }
  }
}

let carry = '';
await streamEntry(XLSX, entries.get('xl/worksheets/sheet1.xml'), (text) => {
  carry += text;
  let i;
  while ((i = carry.indexOf('</row>')) >= 0) { handleRow(carry.slice(0, i)); carry = carry.slice(i + 6); }
});

// ---------- 공표표 검증 ----------
const PUBLISHED = {
  tourism_all:       { table: '14-1-1 관광여행', v: [3.1, 54.7, 35.1, 5.2, 0.9, 1.1] },
  tourism_overnight: { table: '14-1-2 관광 숙박여행', v: [3.0, 49.9, 38.4, 7.2, 0.9, 0.6] },
  tourism_daytrip:   { table: '14-1-3 관광 당일여행', v: [3.1, 58.2, 32.7, 3.7, 0.8, 1.6] },
};
const pubBucket = n => n === 1 ? 0 : n <= 3 ? 1 : n <= 5 ? 2 : n <= 10 ? 3 : n <= 20 ? 4 : 5;
const TOLERANCE_PP = 0.1;
const validation = [];
for (const [scope, { table, v }] of Object.entries(PUBLISHED)) {
  const b = [0, 0, 0, 0, 0, 0];
  let tot = 0, n = 0;
  for (const [k, w] of sizeW) { const [sc, s] = k.split('|'); if (sc === scope) { b[pubBucket(+s)] += w; tot += w; } }
  for (const [k, c] of sizeN) { if (k.split('|')[0] === scope) n += c; }
  const dist = b.map(x => (x / tot) * 100);
  const maxDiff = Math.max(...v.map((p, i) => Math.abs(dist[i] - p)));
  validation.push({ scope, published_table: table, sample_trips: n, computed: dist.map(x => +x.toFixed(2)), published: v, max_abs_diff_pp: +maxDiff.toFixed(3) });
  console.log(`검증 ${table.padEnd(18)} n=${String(n).padStart(6)}  최대오차 ${maxDiff.toFixed(3)}pp  ${maxDiff <= TOLERANCE_PP ? 'OK' : '실패'}`);
  if (maxDiff > TOLERANCE_PP) {
    console.error(`\n공표표와 ${maxDiff.toFixed(2)}pp 어긋난다 (허용 ${TOLERANCE_PP}pp). 집계 단위가 틀어졌다 — 산출물을 쓰지 말 것.`);
    console.error(`  계산 [${dist.map(x => x.toFixed(1)).join(' ')}]\n  공표 [${v.map(x => x.toFixed(1)).join(' ')}]`);
    process.exit(1);
  }
}
console.log();

// ---------- 산출 1: 그룹 크기 분포 (상한 적용) ----------
const SCOPES = ['tourism_daytrip', 'tourism_all', 'tourism_overnight'];
const tagOf = size => size === 1 ? 'solo' : size === 2 ? 'pair' : 'group_3_plus';
const sizeRows = [];
const scopeStats = {};
for (const scope of SCOPES) {
  const capped = new Map();
  let tot = 0, overCap = 0, sampleN = 0;
  for (const [k, w] of sizeW) {
    const [sc, s] = k.split('|'); if (sc !== scope) continue;
    const n = +s; tot += w;
    if (n > GROUP_SIZE_CAP) overCap += w;
    const c = Math.min(n, GROUP_SIZE_CAP);
    capped.set(c, (capped.get(c) ?? 0) + w);
  }
  for (const [k, c] of sizeN) { if (k.split('|')[0] === scope) sampleN += c; }
  let mean = 0;
  for (let n = 1; n <= GROUP_SIZE_CAP; n++) {
    const pct = (capped.get(n) ?? 0) / tot * 100;
    mean += n * pct / 100;
    sizeRows.push({
      scope_id: scope,
      group_size: n,
      companion_size_tag: tagOf(n),
      share_pct: pct.toFixed(4),
      is_cap_bucket: n === GROUP_SIZE_CAP ? 'true' : 'false',
      status: 'derived',
    });
  }
  const tagShare = t => sizeRows.filter(r => r.scope_id === scope && r.companion_size_tag === t)
    .reduce((s, r) => s + +r.share_pct, 0);
  scopeStats[scope] = {
    sample_trips: sampleN,
    over_cap_share_pct: +(overCap / tot * 100).toFixed(4),
    mean_group_size: +mean.toFixed(4),
    solo_pct: +tagShare('solo').toFixed(4),
    pair_pct: +tagShare('pair').toFixed(4),
    group_3_plus_pct: +tagShare('group_3_plus').toFixed(4),
  };
}

// ---------- 산출 2: 그룹 크기별 아동(만 15세 미만) 수 분포 ----------
const childRows = [];
const childShareByScope = {};
for (const scope of SCOPES) {
  let childPersons = 0, allPersons = 0;
  const sizeShare = new Map(sizeRows.filter(r => r.scope_id === scope).map(r => [r.group_size, +r.share_pct / 100]));
  for (let size = 1; size <= GROUP_SIZE_CAP; size++) {
    let tot = 0;
    const byChild = new Map();
    for (const [k, w] of childW) {
      const [sc, s, c] = k.split('|'); if (sc !== scope || +s !== size) continue;
      byChild.set(+c, (byChild.get(+c) ?? 0) + w); tot += w;
    }
    if (tot === 0) continue;
    for (let nc = 0; nc <= size; nc++) {
      const pct = (byChild.get(nc) ?? 0) / tot * 100;
      if (pct === 0 && nc > 0) continue;
      childRows.push({
        scope_id: scope,
        group_size: size,
        child_count: nc,
        share_within_group_size_pct: pct.toFixed(4),
        status: 'derived',
      });
      childPersons += (sizeShare.get(size) ?? 0) * (pct / 100) * nc;
    }
    allPersons += (sizeShare.get(size) ?? 0) * size;
  }
  childShareByScope[scope] = +(childPersons / allPersons * 100).toFixed(4);
}

// ---------- 산출 3: 동반자 유형 응답률 (참고용, 복수응답) ----------
const TYPE_LABEL = {
  1: '배우자', 2: '자녀', 3: '손자/손녀', 4: '부모', 5: '조부모', 6: '형제/자매',
  7: '친구', 8: '연인', 9: '직장동료', 10: '친목단체', 11: '학교단체', 12: '기타',
};
const typeRows = [];
for (const scope of SCOPES) {
  for (let size = 2; size <= GROUP_SIZE_CAP; size++) {
    let base = 0;
    for (const [k, w] of childW) { const [sc, s] = k.split('|'); if (sc === scope && +s === size) base += w; }
    if (base === 0) continue;
    for (let t = 1; t <= 12; t++) {
      const w = typeW.get(`${scope}|${size}|${t}`) ?? 0;
      typeRows.push({
        scope_id: scope,
        group_size: size,
        companion_type_code: t,
        companion_type_label: TYPE_LABEL[t],
        response_rate_pct: (w / base * 100).toFixed(4),
        status: 'derived',
      });
    }
  }
}

// ---------- 쓰기 ----------
const writeCsv = (path, rows) => {
  const cols = Object.keys(rows[0]);
  writeFileSync(path, '﻿' + [cols.join(','), ...rows.map(r => cols.map(c => `"${r[c]}"`).join(','))].join('\r\n') + '\r\n', 'utf8');
  console.log(`생성: ${path} (${rows.length}행)`);
};
writeCsv('data/derived/visitor_group_size_distribution_v0.csv', sizeRows);
writeCsv('data/derived/group_child_composition_v0.csv', childRows);
writeCsv('data/derived/companion_type_response_v0.csv', typeRows);

writeFileSync('data/derived/visitor_group_size_distribution_v0.json', JSON.stringify({
  schema_version: '0.1.0',
  created_at: '2026-08-06',
  dataset_id: 'visitor_group_size_distribution_v0',
  status: 'derived — 공표표로 검증된 가중 집계값. 다만 모집단이 축제 방문객이 아니라 전국 국내 관광여행자다(아래 population_mismatch).',
  source: {
    source_id: 'tourism_national_travel_2025_microdata',
    file: XLSX,
    variables: {
      A6: '함께 여행한 일행은 귀하를 포함하여 총 몇 명이었습니까?[단위:명] — 그룹 크기의 근거',
      A6A: '(2명 이상인 경우) 일행 중 만 15세 미만은 몇 명이었습니까?[단위:명] — 아동 슬롯의 근거',
      'A6B_1..12': '함께 여행한 일행은 어떤 분들이었습니까? (복수응답) — 참고용',
      'D_TRA{n}_CASE': '여행유형. 1/2/4 = 관광·휴양 활동 포함(공표표의 "관광여행"), 3/5 = 단순 방문·출장',
      'D_TRA{n}_S_Day': '여행일수(박). 0 이면 당일여행',
      'D_TRA{n}_CHECK': '본설문진행여부 — A 블록이 어느 여행에 대한 응답인지 지정',
      WT_DOM: '국내여행 분석 가중치. 응답자 × 본설문 여행 1건 단위로 적용',
    },
    variable_labels_read_from: '동봉된 SAV 파일의 변수 사전(레코드 타입 2/3/4/7)에서 직접 읽었다',
  },
  do_not_use: {
    'D_TRA{n}_NUM': '"지출비용에 포함되는 인원수"(비용 분담 인원)이며 여행 인원도 동반자 수도 아니다. ONE_COST == COST/NUM 이 28,564건 전부에서 성립해 확인했다. 이 값으로 그룹 크기를 만들면 공표표와 25~30pp 어긋난다. variable_catalog.json 이 "여행 인원 또는 동반 인원 후보"로 추정해둔 것은 틀렸다.',
  },
  validation: {
    method: '산출 분포를 통계편 표 14-1-1/2/3(관광여행/숙박/당일 동반자 수 분포, 본인 포함 기준)의 공표 버킷(1/2~3/4~5/6~10/11~20/21+)으로 묶어 대조',
    tolerance_pp: TOLERANCE_PP,
    results: validation,
    what_this_confirms: [
      'A6 가 공표 동반자 수 표의 근거 변수다',
      'WT_DOM 을 응답자 × 본설문 여행 1건 단위로 적용하는 것이 공표값을 재현한다 — variable_catalog.json 의 미해결 항목("WT_DOM 의 사람 단위·여행 단위 적용 방법 확인")이 이것으로 해소된다',
      '공표표의 "관광여행" 모집단 = 여행유형 코드 1/2/4',
    ],
  },
  group_size_cap: {
    value: GROUP_SIZE_CAP,
    rule: `${GROUP_SIZE_CAP}명 초과 일행은 ${GROUP_SIZE_CAP}명으로 합친다`,
    decided_at: '2026-08-06',
    rationale: `상한을 없애는 것이 크리티컬하지 않다는 판단. 실측상 ${GROUP_SIZE_CAP}명 초과는 당일여행 기준 ${scopeStats.tourism_daytrip.over_cap_share_pct}% 뿐이라 잘라내도 분포 왜곡이 작다. 최대 일행은 80명(학교 단체 소풍 — A6B 에 "학교 단체(소풍, 수학여행 등)" 코드가 있고 조사 정의상 같은 반 학생 수를 기준으로 산정한다)까지 관측된다.`,
    supersedes: 'population_group_generation_spec.md 1.4절의 "group_3_plus 는 3명 70% / 4명 30%" draft_assumption',
  },
  scope_stats: scopeStats,
  child_share_of_all_visitors_pct: childShareByScope,
  primary_scope: 'tourism_daytrip',
  primary_scope_reason: '축제 방문은 숙박을 전제하지 않는 당일 방문에 가깝다. 다른 두 scope 는 민감도 비교용으로 함께 낸다.',
  population_mismatch: '전국 국내 관광여행자의 일행 규모이며 부산바다축제 저녁 방문객을 관측한 값이 아니다. 축제 실측이 생기면 교체한다. 이 표를 "축제 방문객 그룹 크기 실측"이라고 부르면 안 된다.',
  regeneration: 'node Tools/data/build_visitor_group_distribution.mjs',
  spec: 'data/metadata/spec/population_group_generation_spec.md',
}, null, 2) + '\n', 'utf8');
console.log('생성: data/derived/visitor_group_size_distribution_v0.json');

// ---------- 콘솔 요약 ----------
const st = scopeStats.tourism_daytrip;
console.log(`\n[관광 당일여행 = 기본 scope] 표본 ${st.sample_trips.toLocaleString()}건`);
console.log(`  solo ${st.solo_pct}%  pair ${st.pair_pct}%  group_3_plus ${st.group_3_plus_pct}%`);
console.log(`  평균 그룹 크기 ${st.mean_group_size}명, ${GROUP_SIZE_CAP}명 초과(합쳐진 분) ${st.over_cap_share_pct}%`);
console.log(`  그룹 크기별: ` + sizeRows.filter(r => r.scope_id === 'tourism_daytrip').map(r => `${r.group_size}인 ${(+r.share_pct).toFixed(2)}%`).join('  '));
console.log(`  전체 인원 중 만 15세 미만 ${childShareByScope.tourism_daytrip}%`);
