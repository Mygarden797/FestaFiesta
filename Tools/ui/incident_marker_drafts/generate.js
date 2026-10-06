// 사건 월드 아이콘(Frostpunk 방식) 초안 v2를 만든다.
// 기호(? 알림 / ! 문제·기회 / 출동 중)는 흰 단색 256px 텍스처, 원판·링은 런타임(코드 브러시·UI 머티리얼)이 그린다.
// 기호 텍스처는 원판과 같은 정사각형을 꽉 채우는 캔버스로 그려, 위젯에서 패딩 계산 없이 겹친다.
// 링 하나가 두 의미를 갈아 끼운다: 출동 전 = 긴급도 색 · 남은 시간만큼 줄어듦 / 출동 후 = Teal · 작업 진행만큼 차오름.
// 월드 마커는 HUD 아이콘(선 4.5/64, 18~24px)과 달리 40px 안팎에서 읽혀야 해서 면 채움 기호를 쓴다(LightTeal 예외).
// sharp 대신 설치된 Chrome 헤드리스로 SVG를 PNG로 굽는다.
//
// 실행: node Tools/ui/incident_marker_drafts/generate.js

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const VERSION = 'v2';
const projectRoot = path.resolve(__dirname, '../../..');
const outRoot = path.join(projectRoot, 'Design/UXUI/IncidentMarker_LightTeal');
const vectorDir = path.join(outRoot, 'VectorSource');
const pngDir = path.join(outRoot, 'Generated');
const UNREAL_DEST = '/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Incident';

const CHROME_CANDIDATES = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'
];

const C = {
  ink: '#24332E',
  teal: '#527C6B',
  tealBright: '#6FB59A',
  tealSoft: '#B4C8BB',
  beige: '#E7E3E3',
  white: '#FFFFFF'
};

const URGENCY = [
  { key: 'Low', label: '낮음', color: '#F1F4F2', palette: 'IncidentUrgencyLow (신규)', motion: '없음' },
  { key: 'Medium', label: '보통', color: '#F2C230', palette: 'IncidentUrgencyMedium (신규)', motion: '없음' },
  { key: 'High', label: '높음', color: '#E39A22', palette: 'Amber', motion: '느린 펄스' },
  { key: 'Immediate', label: '즉시', color: '#D74E43', palette: 'Danger', motion: '빠른 깜빡임 · 링 +1px' }
];

const DISPATCH = { color: C.tealBright, palette: 'IncidentDispatch (신규, Teal 밝은 톤)' };

const GEOM = {
  discR: 0.45,
  discOpacity: 0.88,
  ringInner: 0.355,
  ringOuter: 0.425,
  trackOpacity: 0.14,
  glyphScale: 0.9
};

const GLYPHS = {
  Quest: `
    <path d="M113 62H143L137 150H119Z" fill="#FFFFFF" stroke="#FFFFFF" stroke-width="12" stroke-linejoin="round"/>
    <circle cx="128" cy="185" r="17" fill="#FFFFFF"/>`,
  Info: `
    <path d="M96 94C96 72 110 58 129 58C150 58 163 71 163 90C163 107 152 115 141 123C132 130 128 136 128 150"
          fill="none" stroke="#FFFFFF" stroke-width="27" stroke-linecap="round" stroke-linejoin="round"/>
    <circle cx="128" cy="185" r="17" fill="#FFFFFF"/>`,
  Dispatched: `
    <g transform="translate(-6 0)">
      <circle cx="110" cy="90" r="27" fill="#FFFFFF"/>
      <path d="M60 190C60 148 81 128 110 128C139 128 160 148 160 190Z" fill="#FFFFFF" stroke="#FFFFFF" stroke-width="8" stroke-linejoin="round"/>
      <path d="M182 74V130M154 102H210" fill="none" stroke="#FFFFFF" stroke-width="20" stroke-linecap="round"/>
    </g>`
};

const ASSETS = [
  {
    name: 'T_FF_LH_Icon_IncidentQuest_White_256', glyph: 'Quest',
    usage: '사건 월드 마커 기호 "!". Tone = Problem · Opportunity (처리가 필요한 사건).'
  },
  {
    name: 'T_FF_LH_Icon_IncidentInfo_White_256', glyph: 'Info',
    usage: '사건 월드 마커 기호 "?". Tone = Notice (확인만 하면 되는 사건).'
  },
  {
    name: 'T_FF_LH_Icon_IncidentDispatched_White_256', glyph: 'Dispatched',
    usage: '사건 월드 마커 기호 "출동 중". ResponseStage != None 일 때 Tone 기호를 대신한다. HUD용 AssignStaff의 면 채움 월드 버전.'
  }
];

const glyphBody = (glyph) =>
  `<g transform="translate(128 128) scale(${GEOM.glyphScale}) translate(-128 -128)">${GLYPHS[glyph]}</g>`;

const svg = (w, h, body) =>
  `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${body}</svg>`;

function marker({ glyph, color, progress = 1, size = 256 }) {
  const s = size;
  const c = s / 2;
  const ringMid = ((GEOM.ringInner + GEOM.ringOuter) / 2) * s;
  const ringW = (GEOM.ringOuter - GEOM.ringInner) * s;
  const circ = 2 * Math.PI * ringMid;
  return svg(s, s, `
    <circle cx="${c}" cy="${c}" r="${GEOM.discR * s}" fill="${C.ink}" fill-opacity="${GEOM.discOpacity}"/>
    <circle cx="${c}" cy="${c}" r="${ringMid}" fill="none" stroke="${C.white}" stroke-opacity="${GEOM.trackOpacity}" stroke-width="${ringW}"/>
    <circle cx="${c}" cy="${c}" r="${ringMid}" fill="none" stroke="${color}" stroke-width="${ringW}"
            stroke-dasharray="${circ * progress} ${circ}" transform="rotate(-90 ${c} ${c})"/>
    <g transform="scale(${s / 256})">${glyphBody(glyph)}</g>`);
}

function findChrome() {
  const found = CHROME_CANDIDATES.find((p) => fs.existsSync(p));
  if (!found) throw new Error('Chrome/Edge 를 찾지 못했다.');
  return found;
}

function renderPng(chrome, htmlPath, pngPath, w, h, transparent) {
  const args = [
    '--headless=new', '--disable-gpu', '--hide-scrollbars', '--force-device-scale-factor=1',
    `--window-size=${w},${h}`, `--screenshot=${pngPath}`
  ];
  if (transparent) args.push('--default-background-color=00000000');
  args.push('file:///' + htmlPath.replace(/\\/g, '/'));
  execFileSync(chrome, args, { stdio: 'ignore' });
}

function page(body, w, h, bg = 'transparent') {
  return `<!doctype html><html><head><meta charset="utf-8"><style>
    html,body{margin:0;width:${w}px;height:${h}px;background:${bg};overflow:hidden}
    body{font-family:'Pretendard','Malgun Gothic',sans-serif}
  </style></head><body>${body}</body></html>`;
}

function previewSheet() {
  const W = 1400;
  const H = 1180;
  const cell = (inner, cap) => `<div class="cell">${inner}<div class="cap">${cap}</div></div>`;
  const arrow = '<div class="arrow">→</div>';

  const urgencyRow = (glyph) => URGENCY.map((u) =>
    cell(marker({ glyph, color: u.color, progress: 0.72 }), `${u.label} · ${u.key}<br><span>${u.motion}</span>`)).join('');

  const flow = [
    cell(marker({ glyph: 'Quest', color: URGENCY[2].color, progress: 1 }), '① 발생<br><span>높음 · 남은 시간 100%</span>'),
    arrow,
    cell(marker({ glyph: 'Quest', color: URGENCY[2].color, progress: 0.4 }), '② 방치<br><span>남은 시간 40% · 줄어듦</span>'),
    arrow,
    cell(marker({ glyph: 'Dispatched', color: DISPATCH.color, progress: 0.08 }), '③ 출동<br><span>Teal · 작업 0%에서 시작</span>'),
    arrow,
    cell(marker({ glyph: 'Dispatched', color: DISPATCH.color, progress: 0.6 }), '④ 작업 중<br><span>작업 60% · 차오름</span>'),
    arrow,
    cell(marker({ glyph: 'Dispatched', color: DISPATCH.color, progress: 0.95 }), '⑤ 완료 직전<br><span>완료 시 마커 제거</span>')
  ].join('');

  const scenes = [
    { name: '야간 축제', bg: 'radial-gradient(circle at 30% 40%,#5b4a33 0,#1d2226 55%,#121518 100%)' },
    { name: '주간 광장', bg: 'linear-gradient(135deg,#d9d2c3 0,#c4cbb8 50%,#e8e4da 100%)' },
    { name: 'LightTeal 톤', bg: `linear-gradient(135deg,${C.tealSoft} 0,${C.beige} 100%)` }
  ];
  const actual = (size) => [
    marker({ glyph: 'Info', color: URGENCY[0].color, progress: 1, size }),
    marker({ glyph: 'Quest', color: URGENCY[1].color, progress: 0.8, size }),
    marker({ glyph: 'Quest', color: URGENCY[2].color, progress: 0.45, size }),
    marker({ glyph: 'Quest', color: URGENCY[3].color, progress: 0.18, size }),
    marker({ glyph: 'Dispatched', color: DISPATCH.color, progress: 0.55, size })
  ].join('');
  const sceneRows = scenes.map((s) => `
    <div class="scene" style="background:${s.bg}">
      <div class="sname">${s.name}</div>
      <div class="sizes"><div><b>48px</b>${actual(48)}</div><div><b>40px</b>${actual(40)}</div><div><b>32px</b>${actual(32)}</div></div>
    </div>`).join('');

  const body = `
  <style>
    .wrap{padding:26px 36px;color:#24332E;background:#F3F5F3;width:${W - 72}px;height:${H - 52}px}
    h1{font-size:26px;margin:0 0 4px} .sub{font-size:14px;color:#5b6a63;margin-bottom:14px;line-height:1.5}
    h2{font-size:15px;margin:10px 0 6px;color:#315747}
    .row{display:flex;gap:14px;align-items:flex-start}
    .tag{width:110px;font-weight:700;font-size:15px;padding-top:62px}
    .cell{width:170px;text-align:center} .cell svg{width:144px;height:144px}
    .cap{font-size:13px;font-weight:600;line-height:1.4} .cap span{font-weight:400;color:#6b7a73}
    .flow{display:flex;align-items:flex-start;gap:4px}
    .flow .cell{width:200px}
    .arrow{font-size:28px;color:#9DB1A5;padding-top:56px}
    .scene{border-radius:14px;padding:10px 18px;margin-bottom:8px;display:flex;align-items:center;gap:24px}
    .sname{width:110px;font-size:14px;font-weight:700;color:#fff;background:#24332Ecc;padding:6px 10px;border-radius:8px}
    .sizes{display:flex;gap:36px} .sizes>div{display:flex;align-items:center;gap:10px}
    .sizes b{font-size:12px;color:#fff;background:#24332Ecc;padding:2px 5px;border-radius:5px}
  </style>
  <div class="wrap">
    <h1>사건 월드 아이콘 초안 ${VERSION}</h1>
    <div class="sub">기호 = Tone (? 알림 / ! 문제·기회) · 출동하면 기호가 출동 중으로 바뀐다<br>
      링 하나가 의미를 갈아 끼운다 — <b>출동 전</b>: 긴급도 색, 남은 시간만큼 줄어듦 · <b>출동 후</b>: Teal, 작업 진행만큼 차오름 (둘 다 12시에서 시계방향)</div>
    <h2>출동 전 · 긴급도</h2>
    <div class="row"><div class="tag">! 문제·기회</div>${urgencyRow('Quest')}</div>
    <div class="row"><div class="tag">? 알림</div>${urgencyRow('Info')}</div>
    <h2>한 사건의 흐름</h2>
    <div class="flow">${flow}</div>
    <h2>실제 크기 · 배경</h2>
    ${sceneRows}
  </div>`;
  return { html: page(body, W, H, '#F3F5F3'), W, H };
}

function writeManifest() {
  const assets = ASSETS.map((a) => ({
    file: `Generated/${a.name}.png`,
    source_svg: `VectorSource/${a.name}.svg`,
    unreal_destination: `${UNREAL_DEST}/${a.name}`,
    width: 256,
    height: 256,
    draw_as: 'Image',
    margin_px: null,
    srgb: true,
    never_stream: true,
    tintable: false,
    tileable: false,
    target_size_1080p: '40px 월드 마커 (원판과 같은 정사각형)',
    usage: a.usage
  }));
  const manifest = {
    pack: `IncidentMarker_LightTeal_${VERSION}`,
    import: {
      compression_settings: 'TC_EditorIcon',
      lod_group: 'TEXTUREGROUP_UI',
      mip_gen_settings: 'TMGS_NoMipmaps',
      address: 'Clamp'
    },
    exception: '월드 마커 기호는 LightTeal HUD 아이콘 규칙(흰 선 4.5/64, 18~24px)의 예외로 면 채움을 쓴다. 40px 안팎 월드 위에서 읽혀야 하기 때문이다.',
    runtime: {
      disc: { radius_uv: GEOM.discR, color: 'Palette InGame.Ink', opacity: GEOM.discOpacity, brush: 'RoundedBox · HalfHeightRadius' },
      ring_material: {
        asset: '/Game/FestaFiesta/UI/Material/M_UI_IncidentTimerRing',
        domain: 'User Interface', blend: 'Translucent',
        params: {
          Progress: '0~1. 12시에서 시계방향으로 이만큼 칠한다',
          CenterUV: [0.5, 0.5],
          InnerRadius: GEOM.ringInner,
          OuterRadius: GEOM.ringOuter,
          FillColor: '상태별 (아래 ring_states)',
          TrackColor: 'White',
          TrackOpacity: GEOM.trackOpacity
        }
      },
      glyph_scale_in_texture: GEOM.glyphScale,
      ring_states: {
        before_dispatch: {
          when: 'ResponseStage == None',
          glyph: 'Tone == Notice ? IncidentInfo : IncidentQuest',
          fill_color: 'Urgency 색',
          progress: '남은 시간 / MinutesUntilLapse (줄어듦). MinutesUntilLapse <= 0 이면 1 고정'
        },
        dispatched: {
          when: 'ResponseStage != None',
          glyph: 'IncidentDispatched',
          fill_color: DISPATCH.palette,
          progress: '1 - RemainingWorkMinutes / 총 작업분 (차오름). 이동 단계에서는 0'
        }
      },
      urgency: URGENCY.map(({ key, color, palette, motion }) => ({ key, color, palette, motion }))
    },
    assets
  };
  fs.writeFileSync(path.join(outRoot, 'asset-manifest.json'), JSON.stringify(manifest, null, 2), 'utf8');
}

function main() {
  fs.mkdirSync(vectorDir, { recursive: true });
  fs.mkdirSync(pngDir, { recursive: true });
  const chrome = findChrome();
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'ff-incident-'));

  for (const a of ASSETS) {
    const source = svg(256, 256, glyphBody(a.glyph));
    fs.writeFileSync(path.join(vectorDir, `${a.name}.svg`), source, 'utf8');
    const htmlPath = path.join(tmpDir, `${a.name}.html`);
    fs.writeFileSync(htmlPath, page(source, 256, 256), 'utf8');
    renderPng(chrome, htmlPath, path.join(pngDir, `${a.name}.png`), 256, 256, true);
  }

  const sheet = previewSheet();
  const sheetHtml = path.join(outRoot, `FF_IncidentMarker_Preview_${VERSION}.html`);
  fs.writeFileSync(sheetHtml, sheet.html, 'utf8');
  renderPng(chrome, sheetHtml, path.join(outRoot, `FF_IncidentMarker_Preview_${VERSION}.png`), sheet.W, sheet.H, false);

  writeManifest();
  fs.rmSync(path.join(outRoot, 'geometry.json'), { force: true });
  fs.rmSync(tmpDir, { recursive: true, force: true });

  console.log(`done → ${path.relative(projectRoot, outRoot)}`);
}

main();
