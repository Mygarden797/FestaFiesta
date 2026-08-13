// Report 화면 목업 생성기.
//
// UFF_ReportScreen / UFF_MetricRowWidget의 헤더 계약을 그림으로 옮긴 것이다.
// WBP를 코드로 생성하기 전에 레이아웃을 합의하는 용도이며, 합의된 이 좌표가
// 그대로 FF_ReportScreenWbpBuilder의 입력이 된다.
//
// 화면 문구는 FF_ReportScreen.cpp의 LOCTEXT와 글자까지 같게 유지한다.
// 문구를 바꿔야 하면 .cpp를 먼저 바꾸고 여기를 맞춘다. 반대로 하지 않는다.
//
// 실행: node Tools/ui/generate_report_screen_mockups.js

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const sharp = require('C:/Users/jt/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');

const W = 1920;
const H = 1080;
const projectRoot = path.resolve(__dirname, '../..');
const iconRoot = path.join(projectRoot, 'SourceArt/UI/Icons');
const outputRoot = path.join(projectRoot, 'SourceArt/UI/Mockups/ReportScreen');

// 모던 블랙 팔레트. 다른 목업 생성기와 같은 값을 쓴다.
const C = {
  void: '#05090A', surface: '#101719', surface2: '#161F21', raised: '#1C282A',
  line: '#2A3B3E', lineSoft: '#222F31',
  text: '#F2F1E9', muted: '#8FA3A2', dim: '#5E7071',
  teal: '#159B7B', tealLight: '#6DD2BA',
  // FF_UIMetricBandSettings.cpp의 기본 구간 색을 sRGB로 옮긴 값이다.
  good: '#3DB870', caution: '#E6AE33', bad: '#D94F4A',
  anno: '#FF62C8'
};

// 구간 경계 70 / 40 — FF_UIMetricBandSettings.cpp 2026-08-06 결정.
const BANDS = [
  { min: 70, label: '좋음', color: C.good, icon: 'T_UI_Band_Good.png' },
  { min: 40, label: '주의', color: C.caution, icon: 'T_UI_Band_Caution.png' },
  { min: 0, label: '나쁨', color: C.bad, icon: 'T_UI_Band_Bad.png' }
];

function findBand(value) {
  return BANDS.find(b => value >= b.min) || null;
}

// FF.SetMetrics 62 82 88 — 헤더 주석이 검증용으로 지정한 값이다.
const SAMPLE = { day: 1, sim: 62, score: 82, safety: 88, revenue: 12400 };
const FINALE = SAMPLE.score - SAMPLE.sim; // 임시 계산. 소유자가 값을 주면 사라진다.

const FONT = 'Malgun Gothic, Segoe UI, sans-serif';
const MONO = 'Consolas, monospace';

// --- 레이아웃 상수. 이 숫자들이 WBP 생성기로 그대로 넘어간다 ---
const L = {
  marginX: 160,
  titleBase: 104, subBase: 146, ruleY: 180,
  colTop: 232, colBottom: 892,
  bodyTop: 328,
  rowH: 92, rowGap: 14,
  c1: { x: 160, w: 620 },
  c2: { x: 820, w: 470 },
  c3: { x: 1330, w: 430 },
  footRuleY: 928,
  btn: { x: 1480, y: 958, w: 280, h: 68 }
};

function esc(v) {
  return String(v).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function rr(x, y, w, h, fill, r = 8, stroke = 'none', sw = 0, opacity = 1) {
  return `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${r}" fill="${fill}" fill-opacity="${opacity}" stroke="${stroke}" stroke-width="${sw}"/>`;
}

function txt(x, y, s, { size = 22, fill = C.text, weight = 400, anchor = 'start', family = FONT, opacity = 1, spacing = 0 } = {}) {
  return `<text x="${x}" y="${y}" font-family="${family}" font-size="${size}" font-weight="${weight}" fill="${fill}" fill-opacity="${opacity}" text-anchor="${anchor}" letter-spacing="${spacing}">${esc(s)}</text>`;
}

function line(x1, y1, x2, y2, stroke, sw = 1, dash = null) {
  const d = dash ? ` stroke-dasharray="${dash}"` : '';
  return `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${stroke}" stroke-width="${sw}"${d}/>`;
}

function fmt(n) {
  return String(n).replace(/\B(?=(\d{3})+(?!\d))/g, ',');
}

// --- 배경 ---
// Report는 전체 화면이지만 뒤의 월드가 완전히 사라지지 않는다.
// 실제 화면은 BackgroundBlur를 쓰고, 여기서는 흐린 월드를 근사한다.
function backdrop() {
  return `
    ${rr(0, 0, W, H, C.void, 0)}
    <ellipse cx="430" cy="760" rx="620" ry="330" fill="#0E4A48" fill-opacity="0.30"/>
    <ellipse cx="1500" cy="300" rx="700" ry="360" fill="#123A52" fill-opacity="0.26"/>
    <ellipse cx="980" cy="1020" rx="900" ry="260" fill="#4A3A12" fill-opacity="0.14"/>
    ${rr(0, 0, W, H, '#05090A', 0, 'none', 0, 0.68)}`;
}

function header() {
  let s = '';
  s += txt(L.marginX, L.titleBase, `Day ${SAMPLE.day} 결과`, { size: 62, weight: 700, spacing: -1 });
  s += txt(L.marginX, L.subBase, '부산 다대포 바다축제 · 1일차 마감', { size: 22, fill: C.muted });
  // 축제 이름은 데이터에서 온다. 공통 UI에 부산을 하드코딩하지 않는다 (AGENTS.md 규칙 5).
  s += rr(W - L.marginX - 132, 62, 132, 40, 'none', 20, C.line, 1.5);
  s += txt(W - L.marginX - 66, 89, 'REPORT', { size: 18, fill: C.tealLight, weight: 700, anchor: 'middle', spacing: 3 });
  s += line(L.marginX, L.ruleY, W - L.marginX, L.ruleY, C.line, 1);
  return s;
}

function columnHead(col, num, label, sub) {
  let s = '';
  const cy = L.colTop + 20;
  s += `<circle cx="${col.x + 20}" cy="${cy}" r="19" fill="none" stroke="${C.line}" stroke-width="1.5"/>`;
  s += txt(col.x + 20, cy + 8, String(num), { size: 20, fill: C.tealLight, weight: 700, anchor: 'middle' });
  s += txt(col.x + 54, cy + 9, label, { size: 27, weight: 700 });
  s += txt(col.x + 54, cy + 40, sub, { size: 17, fill: C.dim });
  return s;
}

// 지표 한 줄. UFF_MetricRowWidget 하나에 대응한다.
// 색(왼쪽 바) + 라벨(좋음/주의) + 아이콘 세 가지를 항상 같이 둔다 — AT-011.
function metricRow(col, y, caption, value, { band = true, suffix = '', ghost = false } = {}) {
  const b = band ? findBand(value) : null;
  const icons = [];
  let s = rr(col.x, y, col.w, L.rowH, C.surface2, 10, C.lineSoft, 1);
  if (b) {
    s += `<path d="M${col.x} ${y + 10} a10 10 0 0 1 10 -10 l0 ${L.rowH} a10 10 0 0 1 -10 -10 z" fill="${b.color}"/>`;
    s += rr(col.x + 4, y, 2, L.rowH, b.color, 0);
  }
  s += txt(col.x + 30, y + 38, caption, { size: 22, fill: C.muted });
  if (b) {
    icons.push({ file: b.icon, x: col.x + 30, y: y + 52, size: 20, tint: b.color });
    s += txt(col.x + 58, y + 69, b.label, { size: 19, fill: b.color, weight: 700 });
  } else {
    s += txt(col.x + 30, y + 69, '구간 표시 없음 — 금액 지표', { size: 17, fill: C.dim });
  }
  s += txt(col.x + col.w - 26, y + 62, fmt(value) + suffix, { size: 46, weight: 700, anchor: 'end', fill: ghost ? C.dim : C.text });
  return { svg: s, icons };
}

// 2단 — 분해식. 지표를 왜 둘로 나눴는지가 이 카드 하나로 설명된다.
function breakdownCard(col, y) {
  const h = 226;
  let s = rr(col.x, y, col.w, h, C.surface2, 10, C.lineSoft, 1);
  const rowsY = [y + 62, y + 112, y + 186];
  s += txt(col.x + 62, rowsY[0], '관객 만족도', { size: 22, fill: C.muted });
  s += txt(col.x + col.w - 28, rowsY[0] + 6, fmt(SAMPLE.sim), { size: 34, weight: 700, anchor: 'end' });
  s += txt(col.x + 30, rowsY[1], '+', { size: 28, fill: C.dim, weight: 700 });
  s += txt(col.x + 62, rowsY[1], '피날레 보너스', { size: 22, fill: C.muted });
  s += txt(col.x + col.w - 28, rowsY[1] + 6, fmt(FINALE), { size: 34, weight: 700, anchor: 'end' });
  s += line(col.x + 30, y + 142, col.x + col.w - 28, y + 142, C.line, 1);
  s += txt(col.x + 30, rowsY[2], '=', { size: 30, fill: C.tealLight, weight: 700 });
  s += txt(col.x + 62, rowsY[2], '평가 점수', { size: 24, fill: C.text, weight: 700 });
  s += txt(col.x + col.w - 28, rowsY[2] + 8, fmt(SAMPLE.score), { size: 48, weight: 700, fill: C.tealLight, anchor: 'end' });
  s += txt(col.x + 30, y + 30, '관객 만족도 + 피날레 = 평가 점수', { size: 16, fill: C.dim });
  return { svg: s, bottom: y + h };
}

// 3단 — 8/14에는 자리만 잡는다. 빈 칸이 아니라 "무엇이 올지"를 보여준다.
function drilldownPlaceholder(col, y, bottom) {
  const h = bottom - y;
  let s = rr(col.x, y, col.w, h, '#0C1213', 10, 'none', 0, 0.7);
  s += `<rect x="${col.x}" y="${y}" width="${col.w}" height="${h}" rx="10" fill="none" stroke="${C.line}" stroke-width="1.5" stroke-dasharray="8 7"/>`;
  s += txt(col.x + 28, y + 48, '원인 분석', { size: 24, weight: 700, fill: C.muted });
  s += txt(col.x + 28, y + 80, '관객 만족도가 왜 62인지', { size: 18, fill: C.dim });
  s += txt(col.x + 28, y + 106, '여기서 두 번 눌러 내려간다.', { size: 18, fill: C.dim });
  const ghosts = ['대기 시간이 길었던 시설', '수요 대비 부족한 화장실', '내일 바꿀 행동'];
  ghosts.forEach((g, i) => {
    const gy = y + 148 + i * 62;
    s += rr(col.x + 24, gy, col.w - 48, 50, C.surface, 8, C.lineSoft, 1, 0.55);
    s += txt(col.x + 42, gy + 32, g, { size: 18, fill: C.dim, opacity: 0.75 });
  });
  s += rr(col.x + 24, bottom - 68, col.w - 48, 40, '#1A1410', 8, '#4A3A18', 1);
  s += txt(col.x + 40, bottom - 42, '데이터 준비 중 — CauseId 롤업 대기', { size: 16, fill: C.caution });
  return s;
}

function footer({ lastDay = false } = {}) {
  let s = line(L.marginX, L.footRuleY, W - L.marginX, L.footRuleY, C.line, 1);
  if (lastDay) {
    // 비활성 버튼만 두면 왜 못 누르는지 알 수 없다 (AT-011).
    s += txt(L.marginX, L.btn.y + 44, '마지막 날입니다. 축제가 곧 마무리됩니다.', { size: 21, fill: C.caution });
    s += rr(L.btn.x, L.btn.y, L.btn.w, L.btn.h, C.surface2, 10, C.line, 1.5);
    s += txt(L.btn.x + L.btn.w / 2, L.btn.y + 43, '다음 날', { size: 25, weight: 700, fill: C.dim, anchor: 'middle' });
  } else {
    s += txt(L.marginX, L.btn.y + 44, '진행 상황은 자동으로 저장되었습니다.', { size: 20, fill: C.dim });
    s += rr(L.btn.x, L.btn.y, L.btn.w, L.btn.h, C.teal, 10);
    s += rr(L.btn.x, L.btn.y, L.btn.w, L.btn.h, C.tealLight, 10, 'none', 0, 0.14);
    s += txt(L.btn.x + L.btn.w / 2 - 14, L.btn.y + 43, '다음 날', { size: 25, weight: 700, fill: '#05201B', anchor: 'middle' });
    s += `<path d="M${L.btn.x + L.btn.w - 54} ${L.btn.y + 26} l14 8 l-14 8" fill="none" stroke="#05201B" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>`;
  }
  return s;
}

// --- 화면 조립 ---
function buildBase({ lastDay = false, drilldownFilled = false } = {}) {
  const icons = [];
  let s = backdrop() + header();

  s += columnHead(L.c1, 1, '관측 사실', '오늘 무슨 일이 있었나');
  s += columnHead(L.c2, 2, '지표 영향', '그래서 점수가 어떻게 됐나');
  s += columnHead(L.c3, 3, '내일 바꿀 행동', '무엇을 고칠 수 있나');

  const rows = [
    ['관객 만족도', SAMPLE.sim, {}],
    ['평가 점수', SAMPLE.score, {}],
    ['안전', SAMPLE.safety, {}],
    ['오늘 매출', SAMPLE.revenue, { band: false }]
  ];
  rows.forEach(([caption, value, opt], i) => {
    const y = L.bodyTop + i * (L.rowH + L.rowGap);
    const r = metricRow(L.c1, y, caption, value, opt);
    s += r.svg;
    icons.push(...r.icons);
  });

  const bd = breakdownCard(L.c2, L.bodyTop);
  s += bd.svg;
  s += txt(L.c2.x, bd.bottom + 44, '피날레 보너스는 지불로 결정된다.', { size: 17, fill: C.dim });
  s += txt(L.c2.x, bd.bottom + 70, '원인 분석은 왼쪽 항에만 붙는다.', { size: 17, fill: C.dim });

  if (drilldownFilled) {
    s += drilldownFilledPanel(L.c3, L.bodyTop, L.colBottom);
  } else {
    s += drilldownPlaceholder(L.c3, L.bodyTop, L.colBottom);
  }
  s += footer({ lastDay });
  return { svg: s, icons };
}

// 확장 후 모습. 지금 레이아웃이 드릴다운을 받아도 안 무너지는지 확인하는 용도다.
function drilldownFilledPanel(col, y, bottom) {
  let s = rr(col.x, y, col.w, bottom - y, C.surface2, 10, C.lineSoft, 1);
  s += txt(col.x + 28, y + 44, '원인 분석', { size: 24, weight: 700 });
  s += txt(col.x + 28, y + 74, '관객 만족도 62의 근거', { size: 17, fill: C.dim });
  const items = [
    ['화장실 대기 14분', '슬롯 3 · 미해소 38%', C.bad],
    ['그늘 부족', '해변 구역 · 미해소 21%', C.caution],
    ['입장 대기 6분', '정문 · 미해소 9%', C.caution]
  ];
  items.forEach((it, i) => {
    const iy = y + 104 + i * 96;
    s += rr(col.x + 22, iy, col.w - 44, 82, C.raised, 8, C.lineSoft, 1);
    s += rr(col.x + 22, iy, 4, 82, it[2], 2);
    s += txt(col.x + 44, iy + 34, it[0], { size: 21, weight: 700 });
    s += txt(col.x + 44, iy + 62, it[1], { size: 16, fill: C.muted });
    s += `<path d="M${col.x + col.w - 48} ${iy + 32} l10 9 l-10 9" fill="none" stroke="${C.dim}" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>`;
  });
  s += rr(col.x + 22, bottom - 96, col.w - 44, 72, '#0F1A18', 8, C.teal, 1);
  s += txt(col.x + 42, bottom - 64, '내일 화장실 2개를 슬롯 3에 배치', { size: 18, fill: C.tealLight, weight: 700 });
  s += txt(col.x + 42, bottom - 40, '예상 관객 만족도 +9', { size: 16, fill: C.muted });
  return s;
}

// BindWidget 이름 주석판. 이 이름들이 WBP 생성기와 C++ 헤더의 단일 계약이다.
function annotations() {
  // place: 라벨 칩을 어디에 붙일지. 칩이 아래 줄의 구간 라벨(주의·좋음)을 덮으면
  // AT-011을 설명하려고 만든 그림이 정작 그 부분을 가린다. 그래서 줄 안쪽에 넣는다.
  const marks = [
    { x: L.marginX - 8, y: 60, w: 430, h: 56, name: 'Text_Title', place: 'above' },
    { x: L.c1.x - 8, y: L.bodyTop - 8, w: L.c1.w + 16, h: L.rowH + 16, name: 'Row_SimSatisfaction', place: 'in-br' },
    { x: L.c1.x - 8, y: L.bodyTop + 106 - 8, w: L.c1.w + 16, h: L.rowH + 16, name: 'Row_ScoreSatisfaction', place: 'in-br' },
    { x: L.c1.x - 8, y: L.bodyTop + 212 - 8, w: L.c1.w + 16, h: L.rowH + 16, name: 'Row_Safety', place: 'in-br' },
    { x: L.c1.x - 8, y: L.bodyTop + 318 - 8, w: L.c1.w + 16, h: L.rowH + 16, name: 'Row_TodayRevenue', place: 'in-br' },
    { x: L.c2.x - 8, y: L.bodyTop + 84, w: L.c2.w + 16, h: 54, name: 'Text_FinaleBonus', place: 'in-bl' },
    { x: L.c2.x - 8, y: L.bodyTop + 150, w: L.c2.w + 16, h: 84, name: 'Text_SatisfactionBreakdown', place: 'in-bl' },
    { x: L.c3.x - 8, y: L.bodyTop - 8, w: L.c3.w + 16, h: L.colBottom - L.bodyTop + 16, name: 'Panel_CauseDrilldown', place: 'above' },
    { x: L.marginX - 8, y: L.btn.y + 16, w: 520, h: 44, name: 'Text_NextDayHint', place: 'above' },
    { x: L.btn.x - 8, y: L.btn.y - 8, w: L.btn.w + 16, h: L.btn.h + 16, name: 'Button_NextDay', place: 'above' }
  ];
  let s = rr(0, 0, W, H, '#05090A', 0, 'none', 0, 0.42);
  marks.forEach(m => {
    s += `<rect x="${m.x}" y="${m.y}" width="${m.w}" height="${m.h}" rx="6" fill="none" stroke="${C.anno}" stroke-width="2" stroke-dasharray="7 5"/>`;
    const lw = m.name.length * 9.4 + 16;
    let cx = m.x;
    let cy = m.y - 26;
    if (m.place === 'in-br') { cx = m.x + m.w - lw - 6; cy = m.y + m.h - 28; }
    if (m.place === 'in-bl') { cx = m.x + 6; cy = m.y + m.h - 28; }
    s += rr(cx, cy, lw, 23, C.anno, 4);
    s += txt(cx + 8, cy + 17, m.name, { size: 14, fill: '#180310', family: MONO, weight: 700 });
  });
  s += rr(L.marginX, 748, 620, 96, '#12090F', 8, C.anno, 1.5);
  s += txt(L.marginX + 20, 782, '분홍 상자 = C++ BindWidget 대상.', { size: 18, fill: C.anno, weight: 700 });
  s += txt(L.marginX + 20, 808, 'WBP의 위젯 이름이 이것과 정확히 같아야 한다.', { size: 16, fill: C.anno });
  s += txt(L.marginX + 20, 830, 'Text_FinaleBonus는 임시 계산(Score - Sim). 값이 오면 제거.', { size: 14, fill: C.muted });
  return s;
}

async function render(name, svgBody, iconSpecs) {
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">${svgBody}</svg>`;
  const layers = [];
  for (const ic of iconSpecs || []) {
    const buf = await sharp(path.join(iconRoot, ic.file))
      .resize(ic.size, ic.size, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
      .tint(ic.tint)
      .png()
      .toBuffer();
    layers.push({ input: buf, left: Math.round(ic.x), top: Math.round(ic.y) });
  }
  const file = path.join(outputRoot, `${name}.png`);
  await sharp(Buffer.from(svg)).composite(layers).png().toFile(file);
  const sha = crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex').slice(0, 16);
  console.log(`${name}.png  sha256:${sha}`);
  return { name: `${name}.png`, sha256: sha };
}

async function main() {
  fs.mkdirSync(outputRoot, { recursive: true });
  const manifest = [];

  const base = buildBase();
  manifest.push(await render('01_report_2026-08-14', base.svg, base.icons));
  manifest.push(await render('02_report_widget-names', base.svg + annotations(), base.icons));

  const later = buildBase({ drilldownFilled: true });
  manifest.push(await render('03_report_drilldown-later', later.svg, later.icons));

  const last = buildBase({ lastDay: true });
  manifest.push(await render('04_report_last-day', last.svg, last.icons));

  fs.writeFileSync(
    path.join(outputRoot, 'manifest.json'),
    JSON.stringify({ generator: 'Tools/ui/generate_report_screen_mockups.js', size: [W, H], layout: L, files: manifest }, null, 2)
  );
  console.log(`\n출력: ${outputRoot}`);
}

main().catch(err => { console.error(err); process.exit(1); });
