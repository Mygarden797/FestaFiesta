/**
 * 단계 F1 — Tour_Stat 2018 원자료(HTML을 .xls로 저장한 파일) → cleaned long-format
 *
 * 입력 (data/raw/tourism/):
 *   Tour_Stat (3).xls  연령별 × 여행지에서의 활동
 *   Tour_Stat (4).xls  연령별 × 방문지 선택 이유
 *   Tour_Stat (5).xls  가구원수별 × 방문지 선택 이유
 *   Tour_Stat (6).xls  가구원수별 × 여행지에서의 활동
 *
 * 출력 (data/cleaned/tourism/):
 *   tour_stat_2018_age_activity.csv / .json
 *   tour_stat_2018_age_reason.csv / .json
 *   tour_stat_2018_companion_activity.csv / .json
 *   tour_stat_2018_companion_reason.csv / .json
 *
 * 한 행의 의미: "한 그룹(연령대 또는 가구원수) × 한 항목 → 응답률(%)"
 * 항목은 복수응답 가능한 설문이라 그룹 내 합계가 100%가 아니다. 구성비가 아니라
 * 각 항목에 대한 개별 응답률이다.
 */
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { createHash } from 'node:crypto';

const RAW_DIR = 'data/raw/tourism';
const OUT_DIR = 'data/cleaned/tourism';
const enc = new TextDecoder('euc-kr');

function parseTable(filename) {
  const bytes = readFileSync(`${RAW_DIR}/${filename}`);
  const sha256 = createHash('sha256').update(bytes).digest('hex').toUpperCase();
  const html = enc.decode(bytes);

  // 분석기간/가로분석변수/세로분석변수 메타 3행
  const metaMatches = [...html.matchAll(/<td[^>]*>\s*([^<]*?)\s*<\/td>/g)].map(m => m[1].trim()).filter(Boolean);
  const period = metaMatches[0];
  const groupVar = metaMatches[1];
  const itemVar = metaMatches[2];

  // 실제 데이터 테이블(구분 헤더로 시작하는 두 번째 table)을 찾는다
  const tables = [...html.matchAll(/<table[^>]*>([\s\S]*?)<\/table>/g)].map(m => m[1]);
  const dataTable = tables.find(t => t.includes('구분')) ?? tables[tables.length - 1];
  const rowsHtml = [...dataTable.matchAll(/<tr[^>]*>([\s\S]*?)<\/tr>/g)].map(m => m[1]);
  const cellsOf = row => [...row.matchAll(/<t[dh][^>]*>([\s\S]*?)<\/t[dh]>/g)]
    .map(m => m[1].replace(/&nbsp;/g, ' ').replace(/<[^>]+>/g, '').trim());

  const header = cellsOf(rowsHtml[0]); // ["구분", "전체", <그룹들...>]
  const groups = header.slice(1);
  const items = [];
  for (let r = 1; r < rowsHtml.length; r++) {
    const cells = cellsOf(rowsHtml[r]);
    if (cells.length < 2) continue;
    const itemName = cells[0];
    const values = cells.slice(1);
    items.push({ itemName, values });
  }

  return { sha256, bytes: bytes.length, period, groupVar, itemVar, groups, items, filename };
}

function toLong(parsed, groupKeyFn) {
  const rows = [];
  for (const { itemName, values } of parsed.items) {
    parsed.groups.forEach((g, i) => {
      const v = values[i];
      if (v === undefined || v === '') return;
      rows.push({
        reference_year: parsed.period,
        group_variable: parsed.groupVar,
        group_value: g,
        group_key: groupKeyFn(g),
        item: itemName,
        response_rate_pct: v.startsWith('.') ? '0' + v : v,
      });
    });
  }
  return rows;
}

function writeLong(stem, rows, sources, notes) {
  mkdirSync(OUT_DIR, { recursive: true });
  const cols = Object.keys(rows[0]);
  writeFileSync(`${OUT_DIR}/${stem}.csv`,
    '﻿' + [cols.join(','), ...rows.map(r => cols.map(c => `"${r[c]}"`).join(','))].join('\r\n') + '\r\n', 'utf8');

  const groups = [...new Set(rows.map(r => r.group_value))];
  const items = [...new Set(rows.map(r => r.item))];
  writeFileSync(`${OUT_DIR}/${stem}.json`, JSON.stringify({
    schema_version: '0.1.0',
    created_at: '2026-08-04',
    dataset_id: stem,
    row_unit: '한 그룹 × 한 응답 항목 → 응답률(%)',
    output_row_count: rows.length,
    group_variable: rows[0].group_variable,
    groups,
    item_count: items.length,
    note: '복수응답 가능한 설문이므로 그룹 내 항목 합계가 100%가 아니다. 구성비가 아니라 개별 응답률이다.',
    caution: notes,
    sources,
    status: 'confirmed',
  }, null, 2) + '\n', 'utf8');
  console.log(`생성: ${OUT_DIR}/${stem}.csv (${rows.length}행, 그룹 ${groups.length}개 × 항목 ${items.length}개)`);
}

// ---- 연령별 활동 (Tour_Stat 3) ----
const ageActivity = parseTable('Tour_Stat (3).xls');
const ageKeyOf = g => g === '전체' ? 'all' : g === '60세 이상' ? 'age_60_or_over' : g === '15~19세' ? 'age_15_19' : `age_${g.replace('대', 's')}`;
writeLong('tour_stat_2018_age_activity', toLong(ageActivity, ageKeyOf),
  [{ source_id: 'tourism_legacy_tables_2018', file: 'Tour_Stat (3).xls', sha256: ageActivity.sha256, bytes: ageActivity.bytes }],
  ['2018년 전국 표본이며 사하구·2025년 자료가 아니다', "연령 구간이 '60세 이상' 하나로 묶여 60대/70대+를 구분하지 못한다"]);

// ---- 연령별 방문지 선택 이유 (Tour_Stat 4) ----
const ageReason = parseTable('Tour_Stat (4).xls');
writeLong('tour_stat_2018_age_reason', toLong(ageReason, ageKeyOf),
  [{ source_id: 'tourism_legacy_tables_2018', file: 'Tour_Stat (4).xls', sha256: ageReason.sha256, bytes: ageReason.bytes }],
  ['2018년 전국 표본이며 사하구·2025년 자료가 아니다', "연령 구간이 '60세 이상' 하나로 묶여 60대/70대+를 구분하지 못한다"]);

// ---- 가구원수별 방문지 선택 이유 (Tour_Stat 5) ----
const companionKeyOf = g => g === '전체' ? 'all' : g === '1인' ? 'solo' : g === '2인' ? 'pair' : 'group_3_plus';
const companionReason = parseTable('Tour_Stat (5).xls');
writeLong('tour_stat_2018_companion_reason', toLong(companionReason, companionKeyOf),
  [{ source_id: 'tourism_legacy_tables_2018', file: 'Tour_Stat (5).xls', sha256: companionReason.sha256, bytes: companionReason.bytes }],
  ['가구원수이며 실제 여행 동반자 수와 다를 수 있다', '연령과 교차되지 않은 별도 마진분포다']);

// ---- 가구원수별 활동 (Tour_Stat 6) ----
const companionActivity = parseTable('Tour_Stat (6).xls');
writeLong('tour_stat_2018_companion_activity', toLong(companionActivity, companionKeyOf),
  [{ source_id: 'tourism_legacy_tables_2018', file: 'Tour_Stat (6).xls', sha256: companionActivity.sha256, bytes: companionActivity.bytes }],
  ['가구원수이며 실제 여행 동반자 수와 다를 수 있다', '연령과 교차되지 않은 별도 마진분포다']);
