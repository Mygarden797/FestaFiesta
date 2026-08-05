/**
 * 단계 B — cleaned 인구표 생성
 *
 * 입력: data/raw/population/202607_202607_연령별인구현황_월간.csv (CP949, 310열 넓은 표)
 * 출력:
 *   data/cleaned/population/saha_population_age_sex_2026_07.csv   (long 형식)
 *   data/cleaned/population/saha_population_age_sex_2026_07.json  (변환 규칙·검증 결과)
 *
 * 한 행의 의미: "한 지역 × 한 성별 × 한 살 나이 → 그 인구 수"
 *
 * 설계 결정
 *  - '계' 성별 행은 출력하지 않는다. 남 + 여 로 항상 복원되며, 그대로 두면
 *    합계를 낼 때 인구를 두 번 세는 사고가 난다. 검증에만 사용한다.
 *  - 사하구 합계 행과 행정동 행이 섞여 있으므로 region_level 열로 구분한다.
 *    이것도 같은 이유(중복 합산 방지)다.
 *  - age = 100 은 '100세 이상' 개방구간이다. 단순 100세가 아니다.
 *  - age_group / segment_key 는 데이터랩 affinity 파일의 14세그먼트 정의와
 *    글자 그대로 일치시킨다. 나중에 두 표를 key 로 조인하기 위해서다.
 */
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { createHash } from 'node:crypto';

const RAW = 'data/raw/population/202607_202607_연령별인구현황_월간.csv';
const OUT_DIR = 'data/cleaned/population';
const STEM = 'saha_population_age_sex_2026_07';
const SOURCE_ID = 'population_mois_saha_age_sex_2026_07';

const bytes = readFileSync(RAW);
const sha256 = createHash('sha256').update(bytes).digest('hex').toUpperCase();
const lines = new TextDecoder('euc-kr').decode(bytes).trim().split(/\r?\n/).filter(l => l.trim());
const parse = l => l.split('","').map(x => x.replace(/^"|"$/g, '').replace(/,/g, '').trim());

const header = parse(lines[0]);
const rows = lines.slice(1).map(parse);
const period = header[1].match(/^(\d{4})년(\d{2})월/);
const referenceMonth = `${period[1]}-${period[2]}`;

// 데이터랩 14세그먼트와 동일한 연령 구간 정의
function ageGroupOf(age) {
  if (age <= 19) return { key: 'age_10_or_under', label: '10대 이하' };
  if (age >= 70) return { key: 'age_70_or_over', label: '70대 이상' };
  const d = Math.floor(age / 10);
  return { key: `age_${d}0s`, label: `${d}0대` };
}
const SEX = { 남: { key: 'male', label: '남성' }, 여: { key: 'female', label: '여성' } };

// 연령 열 인덱스 수집
const ageColIdx = {};
for (const sex of ['계', '남', '여']) {
  ageColIdx[sex] = [];
  header.forEach((h, i) => {
    const m = h.match(new RegExp(`_${sex}_(\\d+)세( 이상)?$`));
    if (m) ageColIdx[sex].push({ i, age: +m[1] });
  });
}
const colOf = name => header.indexOf(name);
const P = `${period[1]}년${period[2]}월`;

// ---------- 변환 ----------
const out = [];
const validation = { age_sum_vs_band_total: [], male_female_vs_total: [], dong_sum_vs_gu: null };

for (const r of rows) {
  const rawName = r[0];
  const m = rawName.match(/^(.*?)\s*\((\d+)\)$/);
  const regionName = m[1].trim();
  const regionCode = m[2];
  const isGu = /사하구$/.test(regionName);
  const regionLevel = isGu ? 'gu' : 'dong';

  for (const [sexKo, sexMeta] of Object.entries(SEX)) {
    for (const { i, age } of ageColIdx[sexKo]) {
      const count = +r[i] || 0;
      const g = ageGroupOf(age);
      out.push({
        reference_month: referenceMonth,
        region_code: regionCode,
        region_name: regionName,
        region_level: regionLevel,
        sex: sexMeta.key,
        age,
        age_is_open_interval: age === 100 ? 'true' : 'false',
        age_group: g.key,
        age_group_label: g.label,
        segment_key: `${g.key}_${sexMeta.key}`,
        segment_label: `${g.label} ${sexMeta.label}`,
        population_count: count,
      });
    }
  }

  // 검증 1: 성별별 연령 합 = 연령구간인구수
  for (const sex of ['계', '남', '여']) {
    const band = +r[colOf(`${P}_${sex}_연령구간인구수`)];
    const sum = ageColIdx[sex].reduce((a, { i }) => a + (+r[i] || 0), 0);
    if (band !== sum) validation.age_sum_vs_band_total.push({ region: regionName, sex, band, sum });
  }
  // 검증 2: 남 + 여 = 계 (연령별로)
  for (let k = 0; k < ageColIdx['계'].length; k++) {
    const t = +r[ageColIdx['계'][k].i] || 0;
    const mm = +r[ageColIdx['남'][k].i] || 0;
    const ff = +r[ageColIdx['여'][k].i] || 0;
    if (t !== mm + ff) validation.male_female_vs_total.push({ region: regionName, age: ageColIdx['계'][k].age, total: t, male: mm, female: ff });
  }
}

// 검증 3: 행정동 합 = 사하구 합계
const guTotal = out.filter(o => o.region_level === 'gu').reduce((a, o) => a + o.population_count, 0);
const dongTotal = out.filter(o => o.region_level === 'dong').reduce((a, o) => a + o.population_count, 0);
validation.dong_sum_vs_gu = { gu_total: guTotal, dong_sum: dongTotal, difference: guTotal - dongTotal, match: guTotal === dongTotal };

// ---------- 출력 ----------
mkdirSync(OUT_DIR, { recursive: true });
const cols = Object.keys(out[0]);
const csv = '﻿' + [cols.join(','), ...out.map(o => cols.map(c => o[c]).join(','))].join('\r\n') + '\r\n';
writeFileSync(`${OUT_DIR}/${STEM}.csv`, csv, 'utf8');

const allPass = validation.age_sum_vs_band_total.length === 0
  && validation.male_female_vs_total.length === 0
  && validation.dong_sum_vs_gu.match;

const meta = {
  schema_version: '0.1.0',
  created_at: '2026-08-03',
  dataset_id: STEM,
  source_id: SOURCE_ID,
  source_file: RAW,
  source_sha256: sha256,
  reference_month: referenceMonth,
  row_unit: '한 지역 × 한 성별 × 한 살 나이 조합',
  output_row_count: out.length,
  region_count: { gu: 1, dong: rows.length - 1 },
  columns: {
    reference_month: '기준 연월 (YYYY-MM)',
    region_code: '행정구역 코드 (10자리)',
    region_name: '행정구역 이름',
    region_level: "gu = 사하구 합계, dong = 행정동. 두 수준을 함께 합산하면 인구를 두 번 세게 된다",
    sex: 'male 또는 female. 원본의 계 성별은 출력하지 않고 검증에만 사용했다',
    age: '만 나이. 100은 100세 이상 개방구간을 뜻한다',
    age_is_open_interval: 'age가 개방구간(100세 이상)이면 true',
    age_group: '데이터랩 14세그먼트와 동일한 연령 구간 키',
    age_group_label: '연령 구간 한글 이름',
    segment_key: 'age_group + sex. affinity_prior_equal_site_v0.csv 의 segment_key와 조인 가능',
    segment_label: '세그먼트 한글 이름',
    population_count: '주민등록 인구 수 (명)',
  },
  transformation_rules: [
    '원본 310열 넓은 표를 long 형식으로 전개했다',
    "성별 '계' 행은 출력하지 않았다 (남 + 여로 복원 가능하며 중복 합산 위험이 있다)",
    "'100세 이상' 열은 age = 100, age_is_open_interval = true 로 표기했다",
    '연령 구간 경계는 데이터랩 세그먼트 정의를 그대로 따랐다 (10대 이하 = 0~19세, 70대 이상 = 70세 이상)',
    '천단위 구분 콤마를 제거하고 정수로 변환했다',
  ],
  validation: {
    all_passed: allPass,
    age_sum_equals_band_total: { failures: validation.age_sum_vs_band_total.length },
    male_plus_female_equals_total: { failures: validation.male_female_vs_total.length },
    dong_sum_equals_gu_total: validation.dong_sum_vs_gu,
  },
  limitations: [
    '주민등록 배후 거주인구이며 축제 방문객이 아니다',
    '사하구 외부에서 오는 방문객은 이 자료에 전혀 포함되지 않는다',
    '데이터랩 관광지 집중률과 직접 곱하지 않는다. VisitorPropensityAssumption 층을 반드시 거친다',
    '주민등록 기준이므로 실거주와 다를 수 있다 (등록 지연, 학생·군인 등)',
  ],
  status: allPass ? 'confirmed' : 'to_verify',
};
writeFileSync(`${OUT_DIR}/${STEM}.json`, JSON.stringify(meta, null, 2) + '\n', 'utf8');

console.log(`출력 행 수 : ${out.length} (지역 ${rows.length} × 성별 2 × 연령 101)`);
console.log(`검증        : 연령합 불일치 ${validation.age_sum_vs_band_total.length}건, 남녀합 불일치 ${validation.male_female_vs_total.length}건`);
console.log(`행정동 합   : ${dongTotal.toLocaleString()} / 사하구 합계 ${guTotal.toLocaleString()} (차 ${guTotal - dongTotal})`);
console.log(`전체 판정   : ${allPass ? 'PASS' : 'FAIL'}`);
console.log(`\n생성: ${OUT_DIR}/${STEM}.csv`);
console.log(`생성: ${OUT_DIR}/${STEM}.json`);
