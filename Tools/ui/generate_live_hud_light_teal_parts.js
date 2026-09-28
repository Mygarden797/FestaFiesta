const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const os = require('os');
let sharp;
try { sharp = require('sharp'); }
catch (_) { sharp = require(path.join(os.homedir(), '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp')); }

const root = path.resolve(__dirname, '../../SourceArt/UI/LiveHUD/LightTealParts_v1');
const vectorRoot = path.join(root, 'VectorSource');
const goldCoinSource = path.join(__dirname, 'assets/StarEmbossedGoldCoin.png');
const records = [];

const C = {
  white: '#FFFFFF',
  teal: '#527C6B',
  tealDeep: '#315747',
  tealSoft: '#B4C8BB',
  beige: '#E7E3E3',
  ink: '#24332E',
  line: '#CCD5CF',
  lineStrong: '#9DB1A5',
  amber: '#E39A22',
  danger: '#D74E43',
  info: '#3D86B4',
  neutral: '#777F7B'
};

function svg(w, h, body, defs = '') {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><defs>${defs}</defs>${body}</svg>`;
}

function rounded(x, y, w, h, r, fill, stroke = 'none', sw = 0, opacity = 1) {
  return `<rect x="${x + sw / 2}" y="${y + sw / 2}" width="${w - sw}" height="${h - sw}" rx="${r}" fill="${fill}" fill-opacity="${opacity}" stroke="${stroke}" stroke-width="${sw}"/>`;
}

function line(x1, y1, x2, y2, stroke = C.white, sw = 4) {
  return `<path d="M${x1} ${y1}L${x2} ${y2}" fill="none" stroke="${stroke}" stroke-width="${sw}" stroke-linecap="round" stroke-linejoin="round"/>`;
}

const shadowDefs = `<filter id="shadow" x="-30%" y="-30%" width="160%" height="180%"><feDropShadow dx="0" dy="10" stdDeviation="12" flood-color="#10251D" flood-opacity=".20"/></filter>`;

async function writeAsset(category, name, w, h, body, meta = {}) {
  const pngDir = path.join(root, category);
  const svgDir = path.join(vectorRoot, category);
  await fs.promises.mkdir(pngDir, { recursive: true });
  await fs.promises.mkdir(svgDir, { recursive: true });
  const source = svg(w, h, body, meta.defs || '');
  const svgPath = path.join(svgDir, `${name}.svg`);
  const pngPath = path.join(pngDir, `${name}.png`);
  await fs.promises.writeFile(svgPath, source, 'utf8');
  await sharp(Buffer.from(source)).png().toFile(pngPath);
  if (meta.tileable) {
    const { data, info } = await sharp(pngPath).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
    const channels = info.channels;
    for (let y = 0; y < h; y++) {
      for (let channel = 0; channel < channels; channel++) {
        data[(y * w + (w - 1)) * channels + channel] = data[(y * w) * channels + channel];
      }
    }
    for (let x = 0; x < w; x++) {
      for (let channel = 0; channel < channels; channel++) {
        data[((h - 1) * w + x) * channels + channel] = data[x * channels + channel];
      }
    }
    await sharp(data, { raw: info }).png().toFile(`${pngPath}.tile.png`);
    await fs.promises.rename(`${pngPath}.tile.png`, pngPath);
  }
  records.push({
    file: path.relative(root, pngPath).replace(/\\/g, '/'),
    source_svg: path.relative(root, svgPath).replace(/\\/g, '/'),
    width: w,
    height: h,
    draw_as: meta.drawAs || 'Image',
    margin_px: meta.margin || null,
    tintable: !!meta.tintable,
    tileable: !!meta.tileable,
    target_size_1080p: meta.target || null,
    usage: meta.usage || ''
  });
}

async function writeIcon(group, name, content, usage, target = '18-24px') {
  const body = `<g transform="scale(4)" fill="none" stroke="#FFFFFF" stroke-width="4.5" stroke-linecap="round" stroke-linejoin="round">${content}</g>`;
  await writeAsset(`Icons/${group}`, name, 256, 256, body, {
    tintable: true,
    target,
    usage
  });
}

// Gold is full-colour authored artwork. Keep its original RGBA pixels in the
// generated source pack; Unreal limits the resident UI texture to 256px.
async function writeGoldCoinAsset(category, name, usage, target) {
  const metadata = await sharp(goldCoinSource).metadata();
  if (!metadata.hasAlpha || metadata.width !== metadata.height || metadata.width < 256) {
    throw new Error(`Invalid Gold coin source: ${goldCoinSource}`);
  }
  const pngDir = path.join(root, category);
  const svgDir = path.join(vectorRoot, category);
  await fs.promises.mkdir(pngDir, { recursive: true });
  await fs.promises.mkdir(svgDir, { recursive: true });
  const pngPath = path.join(pngDir, `${name}.png`);
  const svgPath = path.join(svgDir, `${name}.svg`);
  await fs.promises.copyFile(goldCoinSource, pngPath);
  const encoded = (await fs.promises.readFile(goldCoinSource)).toString('base64');
  await fs.promises.writeFile(svgPath, svg(metadata.width, metadata.height,
    `<image width="${metadata.width}" height="${metadata.height}" href="data:image/png;base64,${encoded}"/>`), 'utf8');
  records.push({
    file: path.relative(root, pngPath).replace(/\\/g, '/'),
    source_svg: path.relative(root, svgPath).replace(/\\/g, '/'),
    width: metadata.width,
    height: metadata.height,
    draw_as: 'Image',
    margin_px: null,
    tintable: false,
    tileable: false,
    target_size_1080p: target,
    usage
  });
}

async function makePanels() {
  await writeAsset('Panels', 'T_FF_LH_Panel_Body_512_9Slice', 512, 512,
    `<g filter="url(#shadow)">${rounded(24, 18, 464, 466, 24, C.white, C.lineStrong, 4, .985)}</g>`,
    { defs: shadowDefs, drawAs: 'Box', margin: 48, target: '패널 가변', usage: '클릭 시 열리는 공통 패널 본문. 약한 그림자 포함.' });

  await writeAsset('Panels', 'T_FF_LH_Panel_Header_1024x128_9Slice', 1024, 128,
    rounded(4, 4, 1016, 120, 20, C.teal, C.tealDeep, 4) + `<path d="M20 113H1004" stroke="#89A899" stroke-opacity=".45" stroke-width="3"/>`,
    { drawAs: 'Box', margin: 24, target: '높이 48-58px', usage: '공통 패널 제목 헤더.' });

  await writeAsset('Panels', 'T_FF_LH_Panel_Footer_1024x96_9Slice', 1024, 96,
    rounded(4, 2, 1016, 90, 16, '#F3F5F3', C.line, 3) + `<path d="M20 4H1004" stroke="${C.lineStrong}" stroke-opacity=".55" stroke-width="3"/>`,
    { drawAs: 'Box', margin: 20, target: '높이 36-48px', usage: '패널 안내 문구용 푸터.' });

  await writeAsset('Panels', 'T_FF_LH_Panel_Inset_512_9Slice', 512, 512,
    rounded(8, 8, 496, 496, 18, '#F8FAF8', C.line, 4),
    { drawAs: 'Box', margin: 28, target: '가변', usage: '패널 안쪽 섹션·목록 컨테이너.' });

  await writeAsset('Panels', 'T_FF_LH_Panel_Alert_512x144_9Slice', 512, 144,
    rounded(4, 4, 504, 136, 18, '#FFF8E9', C.amber, 4) + `<rect x="4" y="4" width="16" height="136" rx="8" fill="${C.amber}"/>`,
    { drawAs: 'Box', margin: 24, target: '높이 52-72px', usage: '주의·사건 카드.' });

  await writeAsset('Panels', 'T_FF_LH_Panel_Info_512x144_9Slice', 512, 144,
    rounded(4, 4, 504, 136, 18, '#F1F7FA', C.info, 4) + `<rect x="4" y="4" width="16" height="136" rx="8" fill="${C.info}"/>`,
    { drawAs: 'Box', margin: 24, target: '높이 52-72px', usage: '정보·대기 사건 카드.' });

  await writeAsset('Panels', 'T_FF_LH_Tooltip_Dark_512x128_9Slice', 512, 128,
    rounded(8, 8, 496, 112, 18, C.ink, C.white, 3, .94),
    { drawAs: 'Box', margin: 24, target: '높이 36-48px', usage: '월드 툴팁·작은 검정 라벨.' });

  await writeAsset('Panels', 'T_FF_LH_HUD_MenuRail_2048x128_9Slice', 2048, 128,
    `<g filter="url(#shadow)">${rounded(24, 18, 2000, 96, 20, C.white, C.lineStrong, 4, .985)}</g>`,
    { defs: shadowDefs, drawAs: 'Box', margin: 40, target: '1920×29px 메뉴 행', usage: '상단 메뉴 행 배경. 중앙 정렬용.' });

  await writeAsset('Panels', 'T_FF_LH_HUD_StatusBar_2048x128_9Slice', 2048, 128,
    `<rect x="0" y="12" width="2048" height="116" fill="${C.tealSoft}"/><path d="M0 12H2048" stroke="#8FA69A" stroke-width="4"/>`,
    { drawAs: 'Box', margin: 24, target: '1920×29px 상태 행', usage: '화면 하단 고정 상태바.' });

  await writeAsset('Panels', 'T_FF_LH_Divider_Horizontal_1024x8', 1024, 8,
    `<rect x="0" y="3" width="1024" height="2" fill="${C.line}"/>`,
    { target: '두께 1px', usage: '패널 내부 가로 구분선.' });

  await writeAsset('Panels', 'T_FF_LH_Divider_Vertical_8x256', 8, 256,
    `<rect x="3" y="0" width="2" height="256" fill="${C.line}"/>`,
    { target: '두께 1px', usage: '상태바 세로 구분선.' });
}

async function makeButtons() {
  const button = async (name, fill, stroke, sw, usage, opacity = 1) => writeAsset('Buttons', name, 512, 128,
    rounded(4, 4, 504, 120, 18, fill, stroke, sw, opacity),
    { drawAs: 'Box', margin: 24, target: '높이 29-44px', usage });

  await button('T_FF_LH_Button_Nav_Default_512x128_9Slice', C.white, 'transparent', 0, '메뉴 기본 상태.');
  await button('T_FF_LH_Button_Nav_Hover_512x128_9Slice', '#E9F0EC', C.tealSoft, 4, '메뉴 Hover/Focus 상태.');
  await button('T_FF_LH_Button_Nav_Selected_512x128_9Slice', C.teal, C.tealDeep, 4, '메뉴 선택 상태.');
  await button('T_FF_LH_Button_Nav_Pressed_512x128_9Slice', C.tealDeep, '#244438', 4, '메뉴 Pressed 상태.');
  await button('T_FF_LH_Button_Nav_Disabled_512x128_9Slice', C.beige, C.line, 3, '메뉴 비활성 상태.', .72);

  await button('T_FF_LH_Button_Primary_512x128_9Slice', C.teal, C.tealDeep, 4, '주 행동 버튼.');
  await button('T_FF_LH_Button_PrimaryHover_512x128_9Slice', '#628F7C', '#274A3C', 5, '주 행동 Hover/Focus 상태.');
  await button('T_FF_LH_Button_Secondary_512x128_9Slice', C.white, C.teal, 4, '보조 행동 버튼.');
  await button('T_FF_LH_Button_Danger_512x128_9Slice', C.danger, '#9F2D28', 4, '위험 행동 버튼.');

  const chip = async (name, fill, stroke, usage) => writeAsset('Buttons', name, 384, 96,
    rounded(4, 4, 376, 88, 28, fill, stroke, 4),
    { drawAs: 'Box', margin: 28, target: '높이 24-32px', usage });
  await chip('T_FF_LH_Chip_Default_384x96_9Slice', C.white, C.line, '필터 칩 기본.');
  await chip('T_FF_LH_Chip_Selected_384x96_9Slice', C.teal, C.tealDeep, '필터 칩 선택.');
  await chip('T_FF_LH_Chip_Warning_384x96_9Slice', '#FFF8E9', C.amber, '주의 상태 칩.');
  await chip('T_FF_LH_Chip_Live_384x96_9Slice', C.tealDeep, '#244438', 'LIVE 상태 칩.');

  await writeAsset('Buttons', 'T_FF_LH_Button_IconCircle_Default_256', 256, 256,
    `<circle cx="128" cy="128" r="112" fill="${C.white}" stroke="${C.lineStrong}" stroke-width="8"/>`,
    { target: '24-32px', usage: '시간 제어 원형 버튼 기본.' });
  await writeAsset('Buttons', 'T_FF_LH_Button_IconCircle_Selected_256', 256, 256,
    `<circle cx="128" cy="128" r="112" fill="${C.teal}" stroke="${C.tealDeep}" stroke-width="8"/>`,
    { target: '24-32px', usage: '시간 제어 원형 버튼 선택.' });
}

async function makeCardsAndProgress() {
  const card = async (name, w, h, fill, stroke, usage, accent = null) => writeAsset('Cards', name, w, h,
    rounded(4, 4, w - 8, h - 8, 18, fill, stroke, 4) + (accent ? `<rect x="4" y="4" width="14" height="${h - 8}" rx="7" fill="${accent}"/>` : ''),
    { drawAs: 'Box', margin: 24, target: '가변', usage });
  await card('T_FF_LH_Card_Metric_512x192_9Slice', 512, 192, '#F8FAF8', C.line, '지표 요약 카드.');
  await card('T_FF_LH_Card_Summary_768x224_9Slice', 768, 224, '#F3F7F4', C.tealSoft, '상단 요약 카드.');
  await card('T_FF_LH_Card_ListRow_1024x144_9Slice', 1024, 144, C.white, C.line, '시설·스태프·원인 목록 행.');
  await card('T_FF_LH_Card_ListRowSelected_1024x144_9Slice', 1024, 144, '#F2F7F4', C.teal, '선택된 목록 행.', C.teal);
  await card('T_FF_LH_Card_SocialRow_1024x192_9Slice', 1024, 192, C.white, C.line, 'SNS 게시물 행.');

  await writeAsset('Progress', 'T_FF_LH_Progress_Track_1024x32_9Slice', 1024, 32,
    rounded(2, 2, 1020, 28, 14, '#DCE3DE', '#C7D0CA', 2),
    { drawAs: 'Box', margin: 14, target: '높이 3-6px', usage: '진행률·원인 막대 트랙.' });
  await writeAsset('Progress', 'T_FF_LH_Progress_Fill_WhiteMask_1024x32_9Slice', 1024, 32,
    rounded(2, 2, 1020, 28, 14, C.white, C.white, 0),
    { drawAs: 'Box', margin: 14, tintable: true, target: '높이 3-6px', usage: 'Tint 가능한 진행률 채움.' });
  await writeAsset('Progress', 'T_FF_LH_Progress_ProgramTrack_1024x24_9Slice', 1024, 24,
    rounded(2, 2, 1020, 20, 10, '#C9D1CC', '#B9C4BD', 2),
    { drawAs: 'Box', margin: 10, target: '높이 2-4px', usage: '다음 공연 진행선 트랙.' });
}

async function makeNavigationIcons() {
  const icons = [
    ['T_FF_LH_Icon_Operations_White_256', '<path d="M10 52V24L32 10L54 24V52M22 52V35H42V52"/>', '운영 메뉴'],
    ['T_FF_LH_Icon_Facilities_White_256', '<path d="M8 54V24H26V54M26 54V10H56V54M16 33H18M16 42H18M36 20H46M36 31H46M36 42H46"/>', '시설 메뉴'],
    ['T_FF_LH_Icon_Guests_White_256', '<circle cx="22" cy="21" r="9"/><circle cx="45" cy="23" r="7"/><path d="M5 56C5 40 11 32 22 32S39 40 39 56M38 37C50 37 57 43 57 56"/>', '관객 메뉴'],
    ['T_FF_LH_Icon_Queues_White_256', '<path d="M8 48C17 28 27 52 36 30S50 35 56 14M8 56H56"/>', '대기열 메뉴'],
    ['T_FF_LH_Icon_Density_White_256', '<path d="M10 50C20 16 44 16 54 50M14 38H50M20 27H44"/><circle cx="16" cy="49" r="3" fill="white" stroke="none"/><circle cx="32" cy="49" r="3" fill="white" stroke="none"/><circle cx="48" cy="49" r="3" fill="white" stroke="none"/>', '밀집도 메뉴'],
    ['T_FF_LH_Icon_Events_White_256', '<path d="M32 8L58 55H6Z"/><path d="M32 24V39M32 48H32.1"/>', '사건 메뉴'],
    ['T_FF_LH_Icon_SNS_White_256', '<path d="M8 12H56V45H26L12 56V45H8Z"/><path d="M19 25H45M19 34H37"/>', 'SNS 메뉴'],
    ['T_FF_LH_Icon_Report_White_256', '<path d="M10 54V30M24 54V16M38 54V38M52 54V8M7 54H57"/>', '리포트 메뉴'],
    ['T_FF_LH_Icon_Staff_White_256', '<circle cx="25" cy="20" r="9"/><path d="M7 56C7 40 14 31 25 31S43 40 43 56M48 18V38M38 28H58"/>', '스태프 메뉴']
  ];
  for (const [name, body, usage] of icons) await writeIcon('Navigation', name, body, usage);
}

async function makeMetricAndActionIcons() {
  const metrics = [
    ['T_FF_LH_Icon_Visitors_White_256', '<circle cx="22" cy="22" r="9"/><circle cx="45" cy="24" r="7"/><path d="M5 56C5 40 11 33 22 33S39 40 39 56M38 39C50 39 57 45 57 56"/>', '현재 관객'],
    ['T_FF_LH_Icon_StaffCount_White_256', '<circle cx="32" cy="21" r="10"/><path d="M9 57C9 41 18 33 32 33S55 41 55 57"/>', '스태프 수'],
    ['T_FF_LH_Icon_Clock_White_256', '<circle cx="32" cy="32" r="25"/><path d="M32 17V34L44 41"/>', '현재 시각'],
    ['T_FF_LH_Icon_Satisfaction_White_256', '<circle cx="32" cy="32" r="25"/><circle cx="23" cy="27" r="2" fill="white" stroke="none"/><circle cx="41" cy="27" r="2" fill="white" stroke="none"/><path d="M19 39Q32 50 45 39"/>', '만족도'],
    ['T_FF_LH_Icon_Safety_White_256', '<path d="M32 7L54 16V31C54 45 45 54 32 58C19 54 10 45 10 31V16Z"/><path d="M21 32L29 40L44 23"/>', '안전'],
    ['T_FF_LH_Icon_Revenue_White_256', '<path d="M10 51L25 36L35 43L54 18M39 18H54V33"/>', '수익'],
    ['T_FF_LH_Icon_Wait_White_256', '<path d="M18 8H46M18 56H46M22 8C22 21 27 25 32 32C37 25 42 21 42 8M22 56C22 43 27 39 32 32C37 39 42 43 42 56"/>', '대기 시간'],
    ['T_FF_LH_Icon_Throughput_White_256', '<path d="M8 32H50M39 20L52 32L39 44M14 16V48"/>', '처리량'],
    ['T_FF_LH_Icon_Available_White_256', '<circle cx="25" cy="21" r="9"/><path d="M7 56C7 40 14 33 25 33S43 40 43 56M42 43L48 49L59 35"/>', '가용 상태'],
    ['T_FF_LH_Icon_Assigned_White_256', '<circle cx="25" cy="21" r="9"/><path d="M7 56C7 40 14 33 25 33S43 40 43 56M45 32H58M51.5 25V39"/>', '배치 중 상태']
  ];
  for (const [name, body, usage] of metrics) await writeIcon('Metrics', name, body, usage);
  await writeGoldCoinAsset('Icons/Metrics', 'T_FF_LH_Icon_Gold_White_256',
    '보유 골드. 원본 컬러를 유지하고 Tint하지 않는다.', '18-24px');

  const actions = [
    ['T_FF_LH_Icon_Close_White_256', '<path d="M14 14L50 50M50 14L14 50"/>', '닫기'],
    ['T_FF_LH_Icon_Locate_White_256', '<path d="M32 58S53 39 53 24A21 21 0 0 0 11 24C11 39 32 58 32 58Z"/><circle cx="32" cy="24" r="7"/>', '위치 보기'],
    ['T_FF_LH_Icon_AssignStaff_White_256', '<circle cx="23" cy="20" r="9"/><path d="M6 56C6 40 12 32 23 32S40 40 40 56M47 19V43M38 31H56"/>', '스태프 배치'],
    ['T_FF_LH_Icon_Play_White_256', '<path d="M23 14L51 32L23 50Z"/>', '재생'],
    ['T_FF_LH_Icon_Pause_White_256', '<path d="M21 14V50M43 14V50"/>', '일시정지'],
    ['T_FF_LH_Icon_FastForward_White_256', '<path d="M8 16L30 32L8 48ZM31 16L53 32L31 48Z"/>', '빠르게'],
    ['T_FF_LH_Icon_Next_White_256', '<path d="M12 16L40 32L12 48ZM48 16V48"/>', '다음'],
    ['T_FF_LH_Icon_More_White_256', '<circle cx="16" cy="32" r="4" fill="white" stroke="none"/><circle cx="32" cy="32" r="4" fill="white" stroke="none"/><circle cx="48" cy="32" r="4" fill="white" stroke="none"/>', '더보기'],
    ['T_FF_LH_Icon_Calendar_White_256', '<rect x="9" y="14" width="46" height="40" rx="5"/><path d="M9 26H55M20 8V20M44 8V20M20 36H24M30 36H34M40 36H44M20 45H24M30 45H34"/>', 'Morning Check 안내'],
    ['T_FF_LH_Icon_TrendUp_White_256', '<path d="M10 48L25 33L35 42L54 18M41 18H54V31"/>', '상승 추세'],
    ['T_FF_LH_Icon_TrendDown_White_256', '<path d="M10 16L25 31L35 22L54 46M41 46H54V33"/>', '하락 추세'],
    ['T_FF_LH_Icon_NoData_White_256', '<circle cx="32" cy="32" r="25"/><path d="M20 32H44"/>', '데이터 없음']
  ];
  for (const [name, body, usage] of actions) await writeIcon('Actions', name, body, usage);
}

async function makeFacilityAndStatusIcons() {
  const facilities = [
    ['T_FF_LH_Icon_FacilityFood_White_256', '<path d="M12 18H52L48 56H16Z"/><path d="M22 18V10M42 18V10M25 31V46M39 31V46M25 38H39"/>', '푸드 부스'],
    ['T_FF_LH_Icon_FacilityDrink_White_256', '<path d="M16 15H49L45 56H20Z"/><path d="M42 15L49 7M23 28H43"/>', '음료 부스'],
    ['T_FF_LH_Icon_FacilityInfo_White_256', '<circle cx="32" cy="32" r="25"/><path d="M32 28V46M32 18H32.1"/>', '안내소'],
    ['T_FF_LH_Icon_FacilityStage_White_256', '<path d="M8 52H56M13 52V22H51V52M13 22L32 10L51 22M21 33H43M21 42H43"/>', '메인 무대']
  ];
  for (const [name, body, usage] of facilities) await writeIcon('Facilities', name, body, usage);

  const statuses = [
    ['T_FF_LH_Icon_StatusCheck_White_256', '<circle cx="32" cy="32" r="25"/><path d="M19 32L28 41L46 23"/>', '정상'],
    ['T_FF_LH_Icon_StatusWarning_White_256', '<path d="M32 8L58 55H6Z"/><path d="M32 24V39M32 48H32.1"/>', '주의'],
    ['T_FF_LH_Icon_StatusDanger_White_256', '<path d="M32 7L57 32L32 57L7 32Z"/><path d="M22 22L42 42M42 22L22 42"/>', '위험'],
    ['T_FF_LH_Icon_StatusPending_White_256', '<circle cx="32" cy="32" r="25"/><path d="M32 17V34L44 41"/>', '대기'],
    ['T_FF_LH_Icon_StatusLive_White_256', '<circle cx="32" cy="32" r="24"/><circle cx="32" cy="32" r="7" fill="white" stroke="none"/><path d="M13 20C6 29 6 39 13 47M51 20C58 29 58 39 51 47"/>', 'LIVE']
  ];
  for (const [name, body, usage] of statuses) await writeIcon('Status', name, body, usage);
}

async function makeGuestAndSocialIcons() {
  const needs = [
    ['T_FF_LH_Icon_NeedEntertainment_White_256', '<path d="M32 7L39 23L57 25L43 37L47 55L32 46L17 55L21 37L7 25L25 23Z"/>', '즐길거리 욕구'],
    ['T_FF_LH_Icon_NeedRest_White_256', '<path d="M10 35H54V49H10ZM15 22H49V35H15ZM14 49V57M50 49V57"/>', '휴식 욕구'],
    ['T_FF_LH_Icon_NeedFood_White_256', '<path d="M15 8V28M9 8V20C9 26 21 26 21 20V8M15 28V56M43 8V56M43 8C54 16 54 31 43 36"/>', '음식 욕구'],
    ['T_FF_LH_Icon_NeedRestroom_White_256', '<circle cx="20" cy="14" r="6"/><path d="M20 23V43M11 31H29M15 43L12 57M25 43L28 57M39 8V57M50 8V57"/>', '화장실 욕구'],
    ['T_FF_LH_Icon_NeedNoData_White_256', '<circle cx="32" cy="32" r="25"/><path d="M16 48L48 16M20 32H44"/>', '욕구 데이터 없음']
  ];
  for (const [name, body, usage] of needs) await writeIcon('Needs', name, body, usage);

  const groups = [
    ['T_FF_LH_Icon_GroupFamily_White_256', '<circle cx="18" cy="20" r="7"/><circle cx="45" cy="20" r="7"/><circle cx="32" cy="32" r="6"/><path d="M5 55C5 42 10 34 18 34M59 55C59 42 54 34 45 34M20 57C20 44 24 38 32 38S44 44 44 57"/>', '가족 관객군'],
    ['T_FF_LH_Icon_GroupFriends_White_256', '<circle cx="21" cy="22" r="8"/><circle cx="43" cy="22" r="8"/><path d="M5 56C5 41 11 33 21 33S37 41 37 56M27 56C27 41 33 33 43 33S59 41 59 56"/>', '친구 관객군'],
    ['T_FF_LH_Icon_GroupSolo_White_256', '<circle cx="32" cy="20" r="10"/><path d="M9 57C9 40 18 32 32 32S55 40 55 57"/>', '1인 관객군']
  ];
  for (const [name, body, usage] of groups) await writeIcon('AudienceGroups', name, body, usage);

  const social = [
    ['T_FF_LH_Icon_SentimentPositive_White_256', '<circle cx="32" cy="32" r="25"/><circle cx="23" cy="27" r="2" fill="white" stroke="none"/><circle cx="41" cy="27" r="2" fill="white" stroke="none"/><path d="M19 39Q32 50 45 39"/>', 'SNS 긍정'],
    ['T_FF_LH_Icon_SentimentNeutral_White_256', '<circle cx="32" cy="32" r="25"/><circle cx="23" cy="27" r="2" fill="white" stroke="none"/><circle cx="41" cy="27" r="2" fill="white" stroke="none"/><path d="M20 42H44"/>', 'SNS 중립'],
    ['T_FF_LH_Icon_SentimentNegative_White_256', '<circle cx="32" cy="32" r="25"/><circle cx="23" cy="27" r="2" fill="white" stroke="none"/><circle cx="41" cy="27" r="2" fill="white" stroke="none"/><path d="M19 45Q32 34 45 45"/>', 'SNS 부정'],
    ['T_FF_LH_Icon_SocialUnread_White_256', '<path d="M8 12H56V45H26L12 56V45H8Z"/><circle cx="47" cy="18" r="8" fill="white" stroke="none"/>', 'SNS 새 글']
  ];
  for (const [name, body, usage] of social) await writeIcon('Social', name, body, usage);
}

async function makeDecor() {
  await writeAsset('Decor', 'T_FF_LH_IconPlate_Square_256_9Slice', 256, 256,
    rounded(8, 8, 240, 240, 34, C.white, C.line, 6),
    { drawAs: 'Box', margin: 32, tintable: true, target: '32-48px', usage: '시설·상태 아이콘용 정사각 플레이트. Tint 가능.' });
  await writeAsset('Decor', 'T_FF_LH_AvatarCircle_256', 256, 256,
    `<circle cx="128" cy="128" r="112" fill="${C.teal}" stroke="${C.white}" stroke-width="12"/><circle cx="128" cy="128" r="96" fill="none" stroke="${C.tealDeep}" stroke-width="6"/>`,
    { tintable: true, target: '32-48px', usage: 'SNS 이니셜·스태프 아바타 배경.' });
  await writeAsset('Decor', 'T_FF_LH_SelectionRing_512', 512, 512,
    `<circle cx="256" cy="256" r="214" fill="none" stroke="#FFFFFF" stroke-width="24"/><circle cx="256" cy="256" r="238" fill="none" stroke="#FFFFFF" stroke-opacity=".26" stroke-width="24"/>`,
    { tintable: true, target: '월드 선택 영역', usage: '선택 시설·사건 위치 강조 링.' });
  await writeAsset('Decor', 'T_FF_LH_FocusRing_512x128_9Slice', 512, 128,
    rounded(4, 4, 504, 120, 18, 'none', C.white, 8),
    { drawAs: 'Box', margin: 24, tintable: true, target: '버튼 가변', usage: '키보드·게임패드 포커스 링.' });
  await writeAsset('Decor', 'T_FF_LH_SparklineGrid_1024x256', 1024, 256,
    `<path d="M0 32H1024M0 96H1024M0 160H1024M0 224H1024" stroke="#FFFFFF" stroke-opacity=".62" stroke-width="3"/><path d="M128 0V256M384 0V256M640 0V256M896 0V256" stroke="#FFFFFF" stroke-opacity=".38" stroke-width="3"/>`,
    { tintable: true, target: '리포트 차트 배경', usage: '리포트 그래프용 Tint 가능한 격자.' });
}

async function makeHeatMapAndWorldMarkers() {
  await writeAsset('HeatMap', 'T_FF_LH_HeatMap_SafeDots_512_Tile', 512, 512,
    `<pattern id="p" width="64" height="64" patternUnits="userSpaceOnUse"><circle cx="16" cy="16" r="8" fill="#FFFFFF" fill-opacity=".80"/><circle cx="48" cy="48" r="8" fill="#FFFFFF" fill-opacity=".80"/></pattern><rect width="512" height="512" fill="url(#p)"/>`,
    { tileable: true, tintable: true, target: '월드 반복 타일', usage: '안전 밀집도 성긴 점 패턴.' });
  await writeAsset('HeatMap', 'T_FF_LH_HeatMap_CautionDiagonal_512_Tile', 512, 512,
    `<pattern id="p" width="64" height="64" patternUnits="userSpaceOnUse"><path d="M-16 16L16 -16M0 64L64 0M48 80L80 48" fill="none" stroke="#FFFFFF" stroke-opacity=".82" stroke-width="10"/></pattern><rect width="512" height="512" fill="url(#p)"/>`,
    { tileable: true, tintable: true, target: '월드 반복 타일', usage: '주의 밀집도 사선 패턴.' });
  await writeAsset('HeatMap', 'T_FF_LH_HeatMap_DangerCross_512_Tile', 512, 512,
    `<pattern id="p" width="64" height="64" patternUnits="userSpaceOnUse"><path d="M0 0L64 64M64 0L0 64" stroke="#FFFFFF" stroke-opacity=".86" stroke-width="10"/></pattern><rect width="512" height="512" fill="url(#p)"/>`,
    { tileable: true, tintable: true, target: '월드 반복 타일', usage: '위험 밀집도 교차선 패턴.' });

  await writeAsset('WorldMarkers', 'T_FF_LH_WorldLabel_Neutral_768x192_9Slice', 768, 192,
    `<g filter="url(#shadow)">${rounded(24, 20, 720, 140, 24, C.white, C.teal, 6, .97)}</g>`,
    { defs: shadowDefs, drawAs: 'Box', margin: 36, target: '높이 44-58px', usage: '중립 월드 라벨 배경.' });
  await writeAsset('WorldMarkers', 'T_FF_LH_WorldLabel_Caution_768x192_9Slice', 768, 192,
    `<g filter="url(#shadow)">${rounded(24, 20, 720, 140, 24, C.white, C.amber, 7, .97)}</g>`,
    { defs: shadowDefs, drawAs: 'Box', margin: 36, target: '높이 44-58px', usage: '주의 월드 라벨 배경.' });
  await writeAsset('WorldMarkers', 'T_FF_LH_WorldLabel_Danger_768x192_9Slice', 768, 192,
    `<g filter="url(#shadow)">${rounded(24, 20, 720, 140, 24, C.white, C.danger, 7, .97)}</g>`,
    { defs: shadowDefs, drawAs: 'Box', margin: 36, target: '높이 44-58px', usage: '위험 월드 라벨 배경.' });
  await writeAsset('WorldMarkers', 'T_FF_LH_WorldLabel_Dark_768x192_9Slice', 768, 192,
    `<g filter="url(#shadow)">${rounded(24, 20, 720, 140, 24, C.ink, C.white, 4, .94)}</g>`,
    { defs: shadowDefs, drawAs: 'Box', margin: 36, target: '높이 36-52px', usage: '대기열·사건 월드 라벨 배경.' });
  await writeAsset('WorldMarkers', 'T_FF_LH_WorldPin_White_256', 256, 256,
    `<path d="M128 234S214 157 214 96A86 86 0 0 0 42 96C42 157 128 234 128 234Z" fill="#FFFFFF"/><circle cx="128" cy="96" r="30" fill="none" stroke="#000000" stroke-opacity=".18" stroke-width="10"/>`,
    { tintable: true, target: '20-32px', usage: '사건·시설 위치 핀.' });
  await writeAsset('WorldMarkers', 'T_FF_LH_WorldConnector_White_512x128', 512, 128,
    `<path d="M16 64H416L486 16" fill="none" stroke="#FFFFFF" stroke-width="12" stroke-linecap="round" stroke-linejoin="round"/><circle cx="16" cy="64" r="14" fill="#FFFFFF"/>`,
    { tintable: true, target: '월드 태그 연결선', usage: '라벨과 월드 지점 연결선.' });
}

async function makeBadges() {
  await writeAsset('Badges', 'T_FF_LH_Badge_FF_256', 256, 256,
    `<circle cx="128" cy="128" r="112" fill="${C.teal}" stroke="${C.white}" stroke-width="12"/><circle cx="128" cy="128" r="96" fill="none" stroke="${C.tealDeep}" stroke-width="6"/><text x="128" y="138" text-anchor="middle" dominant-baseline="middle" fill="#FFFFFF" font-family="Georgia" font-size="68" font-weight="800">FF</text>`,
    { target: '20-28px', usage: '하단 상태바 FF 배지.' });
  await writeGoldCoinAsset('Badges', 'T_FF_LH_Badge_Gold_256',
    '하단 상태바 Gold 배지. 원본 컬러를 유지하고 Tint하지 않는다.', '20-28px');
}

async function makePreview() {
  const w = 2560, h = 1440;
  const background = { create: { width: w, height: h, channels: 4, background: { r: 231, g: 227, b: 227, alpha: 1 } } };
  const layers = [];
  const text = (label, x, y, size = 30, color = C.ink) => layers.push({ input: Buffer.from(svg(Math.max(420, label.length * size), size + 20, `<text x="0" y="${size}" fill="${color}" font-family="Segoe UI" font-size="${size}" font-weight="700">${label}</text>`)), left: x, top: y });
  const add = async (rel, x, y, width, height, tint = null) => {
    let image = sharp(path.join(root, rel)).resize(width, height, { fit: 'contain' });
    if (tint) image = image.tint(tint);
    layers.push({ input: await image.png().toBuffer(), left: x, top: y });
  };

  text('FESTA FIESTA · LIVE HUD LIGHT TEAL PARTS', 70, 45, 40, C.tealDeep);
  text('PANELS / BUTTONS / CARDS', 70, 120, 28, C.teal);
  await add('Panels/T_FF_LH_Panel_Body_512_9Slice.png', 70, 180, 440, 420);
  await add('Panels/T_FF_LH_Panel_Header_1024x128_9Slice.png', 90, 198, 400, 52);
  await add('Panels/T_FF_LH_Panel_Alert_512x144_9Slice.png', 550, 185, 460, 130);
  await add('Panels/T_FF_LH_Panel_Info_512x144_9Slice.png', 550, 335, 460, 130);
  await add('Cards/T_FF_LH_Card_Metric_512x192_9Slice.png', 1050, 185, 430, 160);
  await add('Cards/T_FF_LH_Card_ListRowSelected_1024x144_9Slice.png', 1050, 370, 650, 90);
  await add('Buttons/T_FF_LH_Button_Nav_Default_512x128_9Slice.png', 1740, 185, 320, 80);
  await add('Buttons/T_FF_LH_Button_Nav_Selected_512x128_9Slice.png', 1740, 280, 320, 80);
  await add('Buttons/T_FF_LH_Button_Primary_512x128_9Slice.png', 2080, 185, 360, 90);
  await add('Buttons/T_FF_LH_Button_Secondary_512x128_9Slice.png', 2080, 300, 360, 90);
  await add('Panels/T_FF_LH_HUD_MenuRail_2048x128_9Slice.png', 550, 500, 1450, 90);
  await add('Panels/T_FF_LH_HUD_StatusBar_2048x128_9Slice.png', 550, 600, 1450, 90);

  text('NAVIGATION ICONS · TINTABLE WHITE + ALPHA', 70, 730, 28, C.teal);
  const nav = ['Operations', 'Facilities', 'Guests', 'Queues', 'Density', 'Events', 'SNS', 'Report', 'Staff'];
  for (let i = 0; i < nav.length; i++) await add(`Icons/Navigation/T_FF_LH_Icon_${nav[i]}_White_256.png`, 80 + i * 170, 790, 110, 110, C.tealDeep);

  text('METRICS / ACTIONS / STATUS', 70, 930, 28, C.teal);
  const rows = [
    ['Metrics', 'Gold'], ['Metrics', 'Visitors'], ['Metrics', 'StaffCount'], ['Metrics', 'Clock'], ['Metrics', 'Satisfaction'], ['Metrics', 'Safety'], ['Metrics', 'Revenue'], ['Metrics', 'Wait'],
    ['Actions', 'Locate'], ['Actions', 'AssignStaff'], ['Actions', 'Play'], ['Actions', 'Pause'], ['Actions', 'FastForward'], ['Status', 'StatusWarning'], ['Status', 'StatusDanger']
  ];
  for (let i = 0; i < rows.length; i++) await add(`Icons/${rows[i][0]}/T_FF_LH_Icon_${rows[i][1]}_White_256.png`, 80 + i * 155, 985, 92, 92,
    i === 0 ? null : (i >= 13 ? (i === 13 ? C.amber : C.danger) : C.tealDeep));

  text('HEAT MAP / WORLD MARKERS', 70, 1140, 28, C.teal);
  await add('HeatMap/T_FF_LH_HeatMap_SafeDots_512_Tile.png', 80, 1195, 160, 160, '#4F9E70');
  await add('HeatMap/T_FF_LH_HeatMap_CautionDiagonal_512_Tile.png', 270, 1195, 160, 160, C.amber);
  await add('HeatMap/T_FF_LH_HeatMap_DangerCross_512_Tile.png', 460, 1195, 160, 160, C.danger);
  await add('WorldMarkers/T_FF_LH_WorldLabel_Neutral_768x192_9Slice.png', 690, 1200, 500, 120);
  await add('WorldMarkers/T_FF_LH_WorldLabel_Caution_768x192_9Slice.png', 1220, 1200, 500, 120);
  await add('WorldMarkers/T_FF_LH_WorldLabel_Danger_768x192_9Slice.png', 1750, 1200, 500, 120);
  await add('WorldMarkers/T_FF_LH_WorldPin_White_256.png', 2290, 1190, 130, 130, C.amber);

  const previewDir = path.join(root, 'Preview');
  await fs.promises.mkdir(previewDir, { recursive: true });
  await sharp(background).composite(layers).png().toFile(path.join(previewDir, 'Preview_FF_LiveHUD_LightTealParts_2560x1440.png'));
}

async function writeManifest() {
  const assets = [];
  for (const record of records.sort((a, b) => a.file.localeCompare(b.file))) {
    const full = path.join(root, record.file);
    const info = await sharp(full).metadata();
    const data = await fs.promises.readFile(full);
    assets.push({
      ...record,
      has_alpha: !!info.hasAlpha,
      compression: 'UserInterface2D (RGBA)',
      texture_group: 'UI',
      srgb: true,
      mip_gen: record.tileable ? 'FromTextureGroup' : 'NoMipmaps',
      never_stream: true,
      address: record.tileable ? 'Wrap' : 'Clamp',
      sha256: crypto.createHash('sha256').update(data).digest('hex').toUpperCase()
    });
  }
  const manifest = {
    pack: 'FestaFiesta Live HUD Light Teal Parts',
    version: '1.0.0',
    reference_resolution: '1920x1080',
    authored_date: '2026-08-12',
    license: 'Project-authored assets; no external asset dependency.',
    palette: C,
    import_defaults: { compression: 'UserInterface2D (RGBA)', texture_group: 'UI', srgb: true, mip_gen: 'NoMipmaps', never_stream: true, address: 'Clamp' },
    assets
  };
  await fs.promises.writeFile(path.join(root, 'asset-manifest.json'), JSON.stringify(manifest, null, 2) + '\n', 'utf8');
  await fs.promises.writeFile(path.join(root, 'style-tokens.json'), JSON.stringify({ palette: C, hud: { total_height_1080p: 58, menu_row_height: 29, status_row_height: 29, panel_gap_above_hud: 12 }, icon: { source_size: 256, minimum_stroke_px: 18, runtime_size_1080p: '14-24' } }, null, 2) + '\n', 'utf8');
}

async function writeReadme() {
  const readme = `# FestaFiesta Live HUD Light Teal 파츠 v1\n\n` +
`Gold 코인 2종은 Tools/ui/assets/StarEmbossedGoldCoin.png 정본을 원본 RGBA 그대로 복사한다. 파일명에 White_256이 있어도 컬러 코인이므로 UMG Tint는 흰색을 사용하고, Unreal Max Texture Size만 256으로 제한한다.\n\n` +
`최종 58px HUD 목업을 Unreal UMG에서 재조립하기 위한 프로젝트 자체 제작 원본 팩이다. 화면 캡처를 잘라 키운 파일이 아니라 SVG 도형에서 각 PNG를 직접 렌더링했다.\n\n` +
`## 포함 범위\n\n- 공통 패널·헤더·푸터·알림·툴팁\n- 메뉴/액션/칩/시간 제어 버튼 상태\n- 카드·구분선·진행바\n- 메뉴 9종, 지표, 행동, 시설, 상태 아이콘\n- 밀집도 반복 패턴과 월드 라벨·핀·연결선\n- 모든 PNG의 SVG 원본(\`VectorSource/\`)\n\n` +
`## 핵심 원칙\n\n- 문구·숫자는 텍스처에 굽지 않는다. Unreal \`TextBlock\`으로 배치한다.\n- 이름에 \`_White_\`가 있는 아이콘과 \`WhiteMask\` 진행바는 \`Color and Opacity\`로 Tint한다.\n- \`_9Slice\` 파일은 Brush \`Draw As = Box\`로 설정하고 \`asset-manifest.json\`의 \`margin_px\`를 적용한다.\n- HeatMap의 \`_Tile\` 3종만 Address X/Y를 \`Wrap\`, Mip Gen을 \`FromTextureGroup\`으로 둔다.\n- Preview는 검토용이며 Unreal에 임포트하지 않는다. \`VectorSource\`도 임포트 대상이 아니다.\n\n` +
`## 공통 임포트 설정\n\n| 항목 | 값 |\n| --- | --- |\n| Compression | UserInterface2D (RGBA) |\n| Texture Group | UI |\n| sRGB | On |\n| Mip Gen | NoMipmaps |\n| Never Stream | On |\n| Address | Clamp |\n\n` +
`## 1920×1080 배치 기준\n\n- 메뉴 행 29px + 상태 행 29px = 총 58px\n- 메뉴는 화면 중앙 정렬\n- 클릭 패널은 하단 HUD에서 12px 이상 띄움\n- 아이콘은 보통 14~24px로 표시하며 256px 원본을 축소 사용\n- 패널/버튼은 DPI Curve와 Safe Zone을 사용해 배치\n\n` +
`## 색상\n\n- Deep teal \`#527C6B\`\n- Pale teal \`#B4C8BB\`\n- Beige \`#E7E3E3\`\n- White \`#FFFFFF\`\n- Text \`#24332E\`\n\n` +
`외부 에셋을 사용하지 않았으므로 THIRD_PARTY_NOTICES 추가 등록은 필요하지 않다.\n`;
  await fs.promises.writeFile(path.join(root, 'README_UNREAL_KO.md'), readme, 'utf8');
}

async function verify() {
  const failures = [];
  for (const record of records) {
    const full = path.join(root, record.file);
    const info = await sharp(full).metadata();
    if (info.width !== record.width || info.height !== record.height) failures.push(`${record.file}: expected ${record.width}x${record.height}, got ${info.width}x${info.height}`);
    if (!info.hasAlpha) failures.push(`${record.file}: alpha channel missing`);
  }
  if (failures.length) throw new Error(failures.join('\n'));
}

async function main() {
  await fs.promises.rm(root, { recursive: true, force: true });
  await fs.promises.mkdir(root, { recursive: true });
  await makePanels();
  await makeButtons();
  await makeCardsAndProgress();
  await makeNavigationIcons();
  await makeMetricAndActionIcons();
  await makeFacilityAndStatusIcons();
  await makeGuestAndSocialIcons();
  await makeHeatMapAndWorldMarkers();
  await makeBadges();
  await makeDecor();
  await makePreview();
  await writeManifest();
  await writeReadme();
  await verify();
  process.stdout.write(`Generated ${records.length} high-resolution Live HUD PNG parts with SVG sources in ${root}\n`);
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
