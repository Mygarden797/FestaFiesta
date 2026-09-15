const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

function loadSharp() {
  const candidates = [
    process.env.FF_SHARP_MODULE,
    'C:/Users/jt/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp',
    'sharp'
  ].filter(Boolean);

  for (const candidate of candidates) {
    try {
      return require(candidate);
    } catch (error) {
      if (candidate === candidates[candidates.length - 1]) throw error;
    }
  }
  throw new Error('sharp module not found');
}

const sharp = loadSharp();
const projectRoot = path.resolve(__dirname, '../..');
const assetRoot = path.join(projectRoot, 'SourceArt/UI/Title/CleanCasualParts_v2');
const buttonRoot = path.join(assetRoot, 'Buttons');
const iconRoot = path.join(assetRoot, 'Icons');
const buttonVectorRoot = path.join(assetRoot, 'VectorSource/Buttons');
const iconVectorRoot = path.join(assetRoot, 'VectorSource/Icons');
const scalableRoot = path.join(assetRoot, 'Scalable');
const surfaceRoot = path.join(scalableRoot, 'Surfaces');
const plateRoot = path.join(scalableRoot, 'IconPlates');
const surfaceVectorRoot = path.join(assetRoot, 'VectorSource/Scalable/Surfaces');
const plateVectorRoot = path.join(assetRoot, 'VectorSource/Scalable/IconPlates');
const previewRoot = path.join(assetRoot, 'Preview');

// 620 x 120 UMG display size at exactly 2x. This prevents anisotropic stretching.
const sourceSize = { width: 1240, height: 240 };
const displaySize = { width: 620, height: 120 };
const iconSize = { width: 256, height: 256 };
const plateSize = { width: 256, height: 256 };

const nineSlice = {
  brushImageSize: { width: 620, height: 120 },
  marginPixels: { left: 40, top: 40, right: 40, bottom: 44 },
  marginNormalized: {
    left: 40 / 620,
    top: 40 / 120,
    right: 40 / 620,
    bottom: 44 / 120
  }
};

const geometry = {
  surface: { x: 8, y: 8, width: 1224, height: 208, radius: 64 },
  lip: { x: 10, y: 16, width: 1220, height: 208, radius: 64 },
  shadow: { x: 14, y: 22, width: 1212, height: 208, radius: 64 },
  iconPlate: { cx: 124, cy: 112, radius: 80 }
};

const plateStyle = {
  shadow: '#315E48',
  shadowOpacity: 0.10,
  border: '#C5E5CD',
  disabledOpacity: 0.72
};

const states = {
  Normal: {
    surfaceTop: '#FFFEFB',
    surfaceBottom: '#F8FAF7',
    border: '#B9DDC3',
    inner: '#FFFFFF',
    innerOpacity: 0.82,
    lip: '#AFD8BA',
    shadow: '#315E48',
    shadowOpacity: 0.12,
    dash: null,
    plateEffect: 'normal'
  },
  Focused: {
    surfaceTop: '#E7F7E6',
    surfaceBottom: '#D1ECD2',
    border: '#75BD84',
    inner: '#F8FFF8',
    innerOpacity: 0.92,
    lip: '#58B675',
    shadow: '#2B6B46',
    shadowOpacity: 0.18,
    dash: null,
    plateEffect: 'focused'
  },
  Pressed: {
    surfaceTop: '#D1EDD3',
    surfaceBottom: '#C2E4C7',
    border: '#56AD70',
    inner: '#9FD3AA',
    innerOpacity: 0.55,
    lip: '#489B62',
    shadow: '#315E48',
    shadowOpacity: 0.055,
    dash: null,
    plateEffect: 'pressed',
    pressedDetail: true
  },
  Disabled: {
    surfaceTop: '#F7F8F5',
    surfaceBottom: '#F0F3EF',
    border: '#B9C7BD',
    inner: '#FFFFFF',
    innerOpacity: 0.64,
    lip: '#D4DDD6',
    shadow: '#52645A',
    shadowOpacity: 0.03,
    dash: '18 14',
    plateEffect: 'disabled'
  }
};

function xmlEscape(value) {
  return value
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;');
}

function svgDocument(width, height, body, title) {
  return [
    `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`,
    `  <title>${xmlEscape(title)}</title>`,
    body,
    '</svg>',
    ''
  ].join('\n');
}

function roundedRect(rect, attributes) {
  return `<rect x="${rect.x}" y="${rect.y}" width="${rect.width}" height="${rect.height}" rx="${rect.radius}" ry="${rect.radius}" ${attributes}/>`;
}

function buttonBody(stateName, id, includePlate = true) {
  const token = states[stateName];
  const surface = geometry.surface;
  const plate = geometry.iconPlate;
  const dash = token.dash ? ` stroke-dasharray="${token.dash}"` : '';
  const innerY = token.pressedDetail ? 18 : 14;
  const innerHeight = token.pressedDetail ? 190 : 196;
  const plateOpacity = token.plateEffect === 'disabled' ? plateStyle.disabledOpacity : 1;
  const plateEffect = token.plateEffect === 'focused'
    ? `<g clip-path="url(#plate-clip-${id})" filter="url(#plate-soft-${id})">
      <ellipse cx="98" cy="73" rx="54" ry="30" fill="#FFFFFF" fill-opacity="0.20"/>
      <ellipse cx="150" cy="177" rx="76" ry="38" fill="#4F9F67" fill-opacity="0.14"/>
    </g>`
    : token.plateEffect === 'pressed'
      ? `<g clip-path="url(#plate-clip-${id})" filter="url(#plate-soft-${id})">
      <ellipse cx="98" cy="76" rx="50" ry="27" fill="#FFFFFF" fill-opacity="0.12"/>
      <ellipse cx="150" cy="172" rx="78" ry="42" fill="#438C5B" fill-opacity="0.20"/>
    </g>`
      : '';
  const plateMarkup = includePlate ? `<g opacity="${plateOpacity}">
    <circle cx="${plate.cx}" cy="${plate.cy + 6}" r="${plate.radius}" fill="${plateStyle.shadow}" fill-opacity="${plateStyle.shadowOpacity}"/>
    <circle cx="${plate.cx}" cy="${plate.cy}" r="${plate.radius}" fill="url(#plate-base-${id})" stroke="${plateStyle.border}" stroke-width="4"/>
    ${plateEffect}
  </g>` : '';
  const plateDefinitions = includePlate ? `<radialGradient id="plate-base-${id}" cx="0.34" cy="0.28" r="0.78">
      <stop offset="0" stop-color="#F7FCF7"/>
      <stop offset="0.45" stop-color="#EAF7EC"/>
      <stop offset="1" stop-color="#DDF1E2"/>
    </radialGradient>
    <clipPath id="plate-clip-${id}">
      <circle cx="${plate.cx}" cy="${plate.cy}" r="76"/>
    </clipPath>
    <filter id="plate-soft-${id}" x="-20%" y="-20%" width="140%" height="140%" color-interpolation-filters="sRGB">
      <feGaussianBlur stdDeviation="4"/>
    </filter>` : '';

  return `  <defs>
    <linearGradient id="surface-${id}" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="${token.surfaceTop}"/>
      <stop offset="1" stop-color="${token.surfaceBottom}"/>
    </linearGradient>
    ${plateDefinitions}
  </defs>
  <!-- A single shallow shadow silhouette avoids the old multi-outline banding. -->
  ${roundedRect(geometry.shadow, `fill="${token.shadow}" fill-opacity="${token.shadowOpacity}"`)}
  ${roundedRect(geometry.lip, `fill="${token.lip}"`)}
  ${roundedRect(surface, `fill="url(#surface-${id})" stroke="${token.border}" stroke-width="4"${dash}`)}
  <rect x="14" y="${innerY}" width="1212" height="${innerHeight}" rx="58" ry="58"
        fill="none" stroke="${token.inner}" stroke-opacity="${token.innerOpacity}" stroke-width="2"/>
  ${plateMarkup}
  ${token.pressedDetail ? '<path d="M42 204h1156" fill="none" stroke="#3E8E58" stroke-opacity="0.48" stroke-width="3" stroke-linecap="round"/>' : ''}`;
}

function buttonSvg(stateName) {
  return svgDocument(
    sourceSize.width,
    sourceSize.height,
    buttonBody(stateName, stateName.toLowerCase()),
    `FestaFiesta clean casual title button - ${stateName.toLowerCase()}`
  );
}

function surfaceSvg(stateName) {
  return svgDocument(
    sourceSize.width,
    sourceSize.height,
    buttonBody(stateName, `surface-${stateName.toLowerCase()}`, false),
    `FestaFiesta clean casual scalable surface - ${stateName.toLowerCase()}`
  );
}

function standalonePlateBody(stateName, id) {
  const token = states[stateName];
  const opacity = token.plateEffect === 'disabled' ? plateStyle.disabledOpacity : 1;
  const effect = token.plateEffect === 'focused'
    ? `<g clip-path="url(#plate-clip-${id})" filter="url(#plate-soft-${id})">
      <ellipse cx="90" cy="72" rx="78" ry="44" fill="#FFFFFF" fill-opacity="0.20"/>
      <ellipse cx="166" cy="222" rx="110" ry="55" fill="#4F9F67" fill-opacity="0.14"/>
    </g>`
    : token.plateEffect === 'pressed'
      ? `<g clip-path="url(#plate-clip-${id})" filter="url(#plate-soft-${id})">
      <ellipse cx="90" cy="78" rx="72" ry="40" fill="#FFFFFF" fill-opacity="0.12"/>
      <ellipse cx="166" cy="210" rx="112" ry="60" fill="#438C5B" fill-opacity="0.20"/>
    </g>`
      : '';

  return `  <defs>
    <radialGradient id="plate-base-${id}" cx="0.34" cy="0.28" r="0.78">
      <stop offset="0" stop-color="#F7FCF7"/>
      <stop offset="0.45" stop-color="#EAF7EC"/>
      <stop offset="1" stop-color="#DDF1E2"/>
    </radialGradient>
    <clipPath id="plate-clip-${id}">
      <circle cx="128" cy="128" r="111"/>
    </clipPath>
    <filter id="plate-soft-${id}" x="-20%" y="-20%" width="140%" height="140%" color-interpolation-filters="sRGB">
      <feGaussianBlur stdDeviation="5"/>
    </filter>
  </defs>
  <g opacity="${opacity}">
    <circle cx="128" cy="134" r="116" fill="${plateStyle.shadow}" fill-opacity="${plateStyle.shadowOpacity}"/>
    <circle cx="128" cy="128" r="116" fill="url(#plate-base-${id})" stroke="${plateStyle.border}" stroke-width="5"/>
    ${effect}
  </g>`;
}

function plateSvg(stateName) {
  return svgDocument(
    plateSize.width,
    plateSize.height,
    standalonePlateBody(stateName, stateName.toLowerCase()),
    `FestaFiesta clean casual icon plate - ${stateName.toLowerCase()}`
  );
}

function focusOverlaySvg() {
  return svgDocument(256, 256, `  <defs>
    <clipPath id="focus-overlay-clip">
      <circle cx="128" cy="128" r="111"/>
    </clipPath>
    <filter id="focus-overlay-soft" x="-20%" y="-20%" width="140%" height="140%" color-interpolation-filters="sRGB">
      <feGaussianBlur stdDeviation="5"/>
    </filter>
  </defs>
  <g clip-path="url(#focus-overlay-clip)" filter="url(#focus-overlay-soft)">
    <ellipse cx="90" cy="72" rx="78" ry="44" fill="#FFFFFF" fill-opacity="0.20"/>
    <ellipse cx="166" cy="222" rx="110" ry="55" fill="#4F9F67" fill-opacity="0.14"/>
  </g>`, 'FestaFiesta clean casual icon plate focus overlay');
}

function iconBody(kind, color, id) {
  if (kind === 'Play') {
    return `<g transform="translate(-10.667 5.333) translate(128 128) scale(1.51 1.30) translate(-128 -128)">
      <path fill="${color}" d="M88.5 59c0-12.5 14-20 24.5-13l93 62c12 8 12 25.5 0 33.5l-93 62c-10.5 7-24.5-.5-24.5-13.5V59z"/>
    </g>`;
  }
  if (kind === 'Settings') {
    return `<defs>
      <mask id="gear-${id}">
        <rect width="256" height="256" fill="#000000"/>
        <g fill="#FFFFFF" transform="translate(0 5.333) translate(128 128) scale(1.03) translate(-128 -128)">
          <rect x="112" y="14" width="32" height="60" rx="10"/>
          <rect x="112" y="182" width="32" height="60" rx="10"/>
          <rect x="182" y="112" width="60" height="32" rx="10"/>
          <rect x="14" y="112" width="60" height="32" rx="10"/>
          <rect x="166" y="30" width="32" height="62" rx="10" transform="rotate(45 182 61)"/>
          <rect x="58" y="138" width="32" height="62" rx="10" transform="rotate(45 74 169)"/>
          <rect x="166" y="138" width="32" height="62" rx="10" transform="rotate(135 182 169)"/>
          <rect x="58" y="30" width="32" height="62" rx="10" transform="rotate(135 74 61)"/>
          <circle cx="128" cy="128" r="78"/>
        </g>
        <circle cx="128" cy="133.333" r="34" fill="#000000"/>
      </mask>
    </defs>
    <rect width="256" height="256" fill="${color}" mask="url(#gear-${id})"/>`;
  }
  if (kind === 'Power') {
    return `<g transform="translate(0 -10.667) translate(128 128) scale(1.10 0.99) translate(-128 -128)"
               fill="none" stroke="${color}" stroke-width="20" stroke-linecap="round">
      <path d="M128 27.5v95"/>
      <path d="M87 60a88.5 88.5 0 1 0 82 0"/>
    </g>`;
  }
  if (kind === 'Chevron') {
    return `<g transform="translate(128 128) scale(0.82 0.77) translate(-128 -128)">
      <path d="M91 46l82 82-82 82" fill="none" stroke="${color}" stroke-width="20" stroke-linecap="round" stroke-linejoin="round"/>
    </g>`;
  }
  throw new Error(`Unknown icon kind: ${kind}`);
}

function iconSvg(kind) {
  return svgDocument(256, 256, iconBody(kind, '#FFFFFF', kind.toLowerCase()), `FestaFiesta clean casual ${kind.toLowerCase()} icon`);
}

function previewIcon(kind, x, y, size, color, id) {
  const scale = size / 256;
  return `<g transform="translate(${x} ${y}) scale(${scale})">${iconBody(kind, color, id)}</g>`;
}

function previewButton(stateName, x, y, id, label, iconKind) {
  const textColor = stateName === 'Disabled' ? '#839087' : '#063F1B';
  const iconColor = textColor;
  return `<g transform="translate(${x} ${y})">
    <g transform="scale(0.5)">${buttonBody(stateName, id)}</g>
    ${iconKind ? previewIcon(iconKind, 38, 32, 48, iconColor, `${id}-${iconKind.toLowerCase()}`) : ''}
    ${label ? `<text x="154" y="76" font-family="Malgun Gothic, Noto Sans KR, Arial, sans-serif" font-size="34" font-weight="700" fill="${textColor}">${xmlEscape(label)}</text>` : ''}
    ${label && stateName !== 'Disabled' ? previewIcon('Chevron', 559, 32, 48, textColor, `${id}-chevron`) : ''}
  </g>`;
}

function previewSvg() {
  const contextButtons = [
    previewButton('Focused', 82, 176, 'preview-play', '플레이', 'Play'),
    previewButton('Normal', 82, 314, 'preview-options', '옵션', 'Settings'),
    previewButton('Normal', 82, 452, 'preview-quit', '게임 종료', 'Power')
  ].join('\n');

  const stateButtons = [
    ['Normal', 846, 160],
    ['Focused', 846, 326],
    ['Pressed', 846, 492],
    ['Disabled', 846, 658]
  ].map(([stateName, x, y], index) => `
    <text x="${x}" y="${y - 14}" font-family="Arial, sans-serif" font-size="18" font-weight="700" letter-spacing="1.5" fill="#315D49">${stateName.toUpperCase()}</text>
    ${previewButton(stateName, x, y, `preview-state-${index}`, '', '')}`
  ).join('\n');

  return svgDocument(1560, 920, `  <defs>
    <linearGradient id="preview-bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#FFFDF5"/>
      <stop offset="0.6" stop-color="#F2FAF5"/>
      <stop offset="1" stop-color="#E8F7F0"/>
    </linearGradient>
  </defs>
  <rect width="1560" height="920" fill="url(#preview-bg)"/>
  <circle cx="55" cy="48" r="160" fill="#FFF9E9" fill-opacity="0.82"/>
  <path d="M0 814C330 748 488 873 778 823s467-126 782-55v152H0z" fill="#DDF4E6" fill-opacity="0.55"/>
  <text x="82" y="70" font-family="Arial, sans-serif" font-size="26" font-weight="800" letter-spacing="1" fill="#315D49">FESTA FIESTA · CLEAN CASUAL BUTTONS V2</text>
  <text x="82" y="112" font-family="Arial, sans-serif" font-size="18" font-weight="700" fill="#5A796A">IN-GAME SCALE · 620 × 120 · EXACT 2× SOURCE</text>
  <text x="82" y="150" font-family="Malgun Gothic, Noto Sans KR, Arial, sans-serif" font-size="20" font-weight="600" fill="#6B8075">두 번째 이미지의 낮은 라운드와 얕은 그림자에 맞춘 실제 메뉴 조합</text>
  <line x1="782" y1="116" x2="782" y2="842" stroke="#CDE3D5" stroke-width="2"/>
  <text x="846" y="70" font-family="Arial, sans-serif" font-size="24" font-weight="800" letter-spacing="1" fill="#315D49">IMPORT SKINS · NO BAKED LABELS</text>
  <text x="846" y="108" font-family="Arial, sans-serif" font-size="17" font-weight="600" fill="#6B8075">IDENTICAL GEOMETRY ACROSS ALL STATES</text>
  ${contextButtons}
  ${stateButtons}`, 'FestaFiesta clean casual button skin preview');
}

function scalableSurfacePreviewBody(stateName, width, id) {
  const token = states[stateName];
  const dash = token.dash ? ' stroke-dasharray="9 7"' : '';
  const innerY = token.pressedDetail ? 9 : 7;
  const innerHeight = token.pressedDetail ? 95 : 98;
  return `<defs>
    <linearGradient id="responsive-surface-${id}" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="${token.surfaceTop}"/>
      <stop offset="1" stop-color="${token.surfaceBottom}"/>
    </linearGradient>
  </defs>
  <rect x="7" y="11" width="${width - 14}" height="104" rx="32" fill="${token.shadow}" fill-opacity="${token.shadowOpacity}"/>
  <rect x="5" y="8" width="${width - 10}" height="104" rx="32" fill="${token.lip}"/>
  <rect x="4" y="4" width="${width - 8}" height="104" rx="32" fill="url(#responsive-surface-${id})" stroke="${token.border}" stroke-width="2"${dash}/>
  <rect x="7" y="${innerY}" width="${width - 14}" height="${innerHeight}" rx="29" fill="none"
        stroke="${token.inner}" stroke-opacity="${token.innerOpacity}" stroke-width="1"/>
  ${token.pressedDetail ? `<path d="M21 102H${width - 21}" fill="none" stroke="#3E8E58" stroke-opacity="0.48" stroke-width="1.5" stroke-linecap="round"/>` : ''}`;
}

function scalablePreviewButton(stateName, width, x, y, id, label, iconKind) {
  const textColor = stateName === 'Disabled' ? '#839087' : '#063F1B';
  return `<g transform="translate(${x} ${y})">
    ${scalableSurfacePreviewBody(stateName, width, `${id}-surface`)}
    <g transform="translate(18 12) scale(0.34375)">${standalonePlateBody(stateName, `${id}-plate`)}</g>
    ${previewIcon(iconKind, 38, 32, 48, textColor, `${id}-${iconKind.toLowerCase()}`)}
    <text x="154" y="76" font-family="Malgun Gothic, Noto Sans KR, Arial, sans-serif" font-size="34" font-weight="700" fill="${textColor}">${xmlEscape(label)}</text>
    ${stateName !== 'Disabled' ? previewIcon('Chevron', width - 61, 32, 48, textColor, `${id}-chevron`) : ''}
  </g>`;
}

function scalablePreviewSvg() {
  return svgDocument(1600, 900, `  <defs>
    <linearGradient id="scalable-preview-bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#FFFDF5"/>
      <stop offset="0.58" stop-color="#F4FBF6"/>
      <stop offset="1" stop-color="#E7F6EF"/>
    </linearGradient>
  </defs>
  <rect width="1600" height="900" fill="url(#scalable-preview-bg)"/>
  <circle cx="32" cy="22" r="180" fill="#FFF8E7" fill-opacity="0.82"/>
  <path d="M0 810C310 755 520 884 828 818s480-105 772-42v124H0z" fill="#DDF4E6" fill-opacity="0.58"/>
  <text x="80" y="66" font-family="Arial, sans-serif" font-size="26" font-weight="800" letter-spacing="1" fill="#315D49">SCALABLE BUTTON PARTS · 9-SLICE + SEPARATE PLATE</text>
  <text x="80" y="106" font-family="Malgun Gothic, Noto Sans KR, Arial, sans-serif" font-size="19" font-weight="600" fill="#667F72">버튼 폭만 변하고 원형 플레이트와 아이콘 크기는 그대로 유지됩니다</text>

  <text x="80" y="158" font-family="Arial, sans-serif" font-size="17" font-weight="800" fill="#315D49">440 × 120</text>
  ${scalablePreviewButton('Normal', 440, 80, 172, 'responsive-440', '옵션', 'Settings')}
  <text x="80" y="318" font-family="Arial, sans-serif" font-size="17" font-weight="800" fill="#315D49">620 × 120 · FOCUSED</text>
  ${scalablePreviewButton('Focused', 620, 80, 332, 'responsive-620', '플레이', 'Play')}
  <text x="80" y="478" font-family="Arial, sans-serif" font-size="17" font-weight="800" fill="#315D49">820 × 120</text>
  ${scalablePreviewButton('Normal', 820, 80, 492, 'responsive-820', '게임 종료', 'Power')}

  <rect x="980" y="150" width="540" height="500" rx="28" fill="#FFFFFF" fill-opacity="0.74" stroke="#CDE4D5" stroke-width="2"/>
  <text x="1020" y="202" font-family="Arial, sans-serif" font-size="21" font-weight="800" fill="#315D49">UNREAL PARTS</text>
  <text x="1020" y="242" font-family="Arial, sans-serif" font-size="17" font-weight="700" fill="#6B8075">1 · SURFACE / DRAW AS BOX</text>
  <g transform="translate(1020 258)">${scalableSurfacePreviewBody('Normal', 450, 'parts-surface')}</g>
  <text x="1020" y="420" font-family="Arial, sans-serif" font-size="17" font-weight="700" fill="#6B8075">2 · PLATE BASE + FOCUS SHADE</text>
  <g transform="translate(1020 440) scale(0.34375)">${standalonePlateBody('Focused', 'parts-plate')}</g>
  <text x="1140" y="480" font-family="Arial, sans-serif" font-size="17" font-weight="700" fill="#315D49">88 × 88 SLOT</text>
  <text x="1140" y="510" font-family="Arial, sans-serif" font-size="15" font-weight="600" fill="#6B8075">SAME CIRCLE IN EVERY STATE</text>
  ${previewIcon('Play', 1040, 548, 48, '#063F1B', 'parts-play')}
  ${previewIcon('Settings', 1120, 548, 48, '#063F1B', 'parts-settings')}
  ${previewIcon('Power', 1200, 548, 48, '#063F1B', 'parts-power')}
  ${previewIcon('Chevron', 1280, 548, 48, '#063F1B', 'parts-chevron')}
  <text x="1020" y="626" font-family="Arial, sans-serif" font-size="15" font-weight="700" fill="#6B8075">ALL ICON SLOTS 48 × 48</text>

  <text x="80" y="700" font-family="Malgun Gothic, Noto Sans KR, Arial, sans-serif" font-size="19" font-weight="700" fill="#315D49">폭 변경: Surface만 늘리기 · 전체 확대: WBP 프리셋을 함께 키우기</text>
  <text x="80" y="738" font-family="Arial, sans-serif" font-size="16" font-weight="600" fill="#6B8075">SAFE UNIFORM SCALE: 1.0× → 2.0× · RECOMMENDED HEIGHT TOKENS: 96 / 120 / 144</text>`, 'FestaFiesta scalable clean casual button preview');
}

async function rasterize(svgSource, outputPath) {
  await sharp(Buffer.from(svgSource, 'utf8'))
    .ensureAlpha()
    .png({ compressionLevel: 9, adaptiveFiltering: false, palette: false })
    .toFile(outputPath);
}

function pixelAt(data, info, x, y) {
  const index = (y * info.width + x) * info.channels;
  return data.subarray(index, index + info.channels);
}

function alphaBounds(data, info) {
  let minX = info.width;
  let minY = info.height;
  let maxX = -1;
  let maxY = -1;
  for (let y = 0; y < info.height; y += 1) {
    for (let x = 0; x < info.width; x += 1) {
      const alpha = data[(y * info.width + x) * info.channels + 3];
      if (alpha === 0) continue;
      minX = Math.min(minX, x);
      minY = Math.min(minY, y);
      maxX = Math.max(maxX, x);
      maxY = Math.max(maxY, y);
    }
  }
  return { minX, minY, maxX, maxY };
}

function alphaCentroid(data, info) {
  let weight = 0;
  let weightedX = 0;
  let weightedY = 0;
  for (let y = 0; y < info.height; y += 1) {
    for (let x = 0; x < info.width; x += 1) {
      const alpha = data[(y * info.width + x) * info.channels + 3] / 255;
      if (alpha === 0) continue;
      weight += alpha;
      weightedX += x * alpha;
      weightedY += y * alpha;
    }
  }
  return { x: weightedX / weight, y: weightedY / weight };
}

async function verifyButton(filePath) {
  const image = sharp(filePath);
  const metadata = await image.metadata();
  if (metadata.width !== sourceSize.width || metadata.height !== sourceSize.height || !metadata.hasAlpha) {
    throw new Error(`${path.basename(filePath)}: expected ${sourceSize.width}x${sourceSize.height} RGBA`);
  }

  const { data, info } = await image.raw().toBuffer({ resolveWithObject: true });
  const corners = [
    pixelAt(data, info, 0, 0),
    pixelAt(data, info, info.width - 1, 0),
    pixelAt(data, info, 0, info.height - 1),
    pixelAt(data, info, info.width - 1, info.height - 1)
  ];
  if (corners.some(pixel => pixel[3] !== 0)) {
    throw new Error(`${path.basename(filePath)}: transparent corner check failed`);
  }

  const bounds = alphaBounds(data, info);
  if (bounds.minX <= 0 || bounds.minY <= 0 || bounds.maxX >= info.width - 1 || bounds.maxY >= info.height - 1) {
    throw new Error(`${path.basename(filePath)}: visible pixels touch the canvas edge`);
  }

  // Render exactly as Unreal will show it. Equal X/Y scale is guaranteed by the 2x source ratio.
  const runtimeBuffer = await image
    .resize(displaySize.width, displaySize.height, { fit: 'fill', kernel: sharp.kernel.lanczos3 })
    .png({ compressionLevel: 9, adaptiveFiltering: false, palette: false })
    .toBuffer();
  const runtimeMetadata = await sharp(runtimeBuffer).metadata();
  if (runtimeMetadata.width !== displaySize.width || runtimeMetadata.height !== displaySize.height) {
    throw new Error(`${path.basename(filePath)}: runtime preview size mismatch`);
  }

  return bounds;
}

async function verifyIcon(filePath) {
  const image = sharp(filePath);
  const metadata = await image.metadata();
  if (metadata.width !== iconSize.width || metadata.height !== iconSize.height || !metadata.hasAlpha) {
    throw new Error(`${path.basename(filePath)}: expected ${iconSize.width}x${iconSize.height} RGBA`);
  }

  const { data, info } = await image.raw().toBuffer({ resolveWithObject: true });
  const corners = [
    pixelAt(data, info, 0, 0),
    pixelAt(data, info, info.width - 1, 0),
    pixelAt(data, info, 0, info.height - 1),
    pixelAt(data, info, info.width - 1, info.height - 1)
  ];
  if (corners.some(pixel => pixel[3] !== 0)) {
    throw new Error(`${path.basename(filePath)}: transparent corner check failed`);
  }

  for (let index = 0; index < data.length; index += info.channels) {
    if (data[index + 3] === 0) continue;
    if (data[index] !== 255 || data[index + 1] !== 255 || data[index + 2] !== 255) {
      throw new Error(`${path.basename(filePath)}: visible RGB must remain pure white for UMG tinting`);
    }
  }

  const bounds = alphaBounds(data, info);
  if (bounds.minX <= 0 || bounds.minY <= 0 || bounds.maxX >= info.width - 1 || bounds.maxY >= info.height - 1) {
    throw new Error(`${path.basename(filePath)}: visible pixels touch the canvas edge`);
  }
  return { bounds, centroid: alphaCentroid(data, info) };
}

async function verifyTransparentAsset(filePath, expectedSize) {
  const image = sharp(filePath);
  const metadata = await image.metadata();
  if (metadata.width !== expectedSize.width || metadata.height !== expectedSize.height || !metadata.hasAlpha) {
    throw new Error(`${path.basename(filePath)}: expected ${expectedSize.width}x${expectedSize.height} RGBA`);
  }
  const { data, info } = await image.raw().toBuffer({ resolveWithObject: true });
  const corners = [
    pixelAt(data, info, 0, 0),
    pixelAt(data, info, info.width - 1, 0),
    pixelAt(data, info, 0, info.height - 1),
    pixelAt(data, info, info.width - 1, info.height - 1)
  ];
  if (corners.some(pixel => pixel[3] !== 0)) {
    throw new Error(`${path.basename(filePath)}: transparent corner check failed`);
  }
  const bounds = alphaBounds(data, info);
  if (bounds.minX <= 0 || bounds.minY <= 0 || bounds.maxX >= info.width - 1 || bounds.maxY >= info.height - 1) {
    throw new Error(`${path.basename(filePath)}: visible pixels touch the canvas edge`);
  }
  return bounds;
}

function sha256(filePath) {
  return crypto.createHash('sha256').update(fs.readFileSync(filePath)).digest('hex');
}

async function main() {
  for (const directory of [
    buttonRoot,
    iconRoot,
    buttonVectorRoot,
    iconVectorRoot,
    surfaceRoot,
    plateRoot,
    surfaceVectorRoot,
    plateVectorRoot,
    previewRoot
  ]) {
    await fs.promises.mkdir(directory, { recursive: true });
  }

  const manifest = {
    status: 'assumption',
    sourceSize,
    displaySize,
    scale: 0.5,
    drawAs: 'Image',
    scalable: {
      surfaceDrawAs: 'Box',
      brushImageSize: nineSlice.brushImageSize,
      marginPixels: nineSlice.marginPixels,
      marginNormalized: nineSlice.marginNormalized,
      plateSlot: { x: 18, y: 12, width: 88, height: 88 },
      iconSlot: { x: 38, y: 32, width: 48, height: 48 }
    },
    files: []
  };

  for (const stateName of Object.keys(states)) {
    const baseName = `T_FF_Title_CleanCasual_Button_${stateName}_1240x240`;
    const svgSource = buttonSvg(stateName);
    const svgPath = path.join(buttonVectorRoot, `${baseName}.svg`);
    const pngPath = path.join(buttonRoot, `${baseName}.png`);
    await fs.promises.writeFile(svgPath, svgSource, 'utf8');
    await rasterize(svgSource, pngPath);
    const bounds = await verifyButton(pngPath);
    manifest.files.push({
      type: 'button',
      state: stateName,
      file: path.relative(assetRoot, pngPath).replaceAll('\\', '/'),
      sha256: sha256(pngPath),
      alphaBounds: bounds
    });
  }

  for (const kind of ['Play', 'Settings', 'Power', 'Chevron']) {
    const baseName = `T_FF_Title_CleanCasual_Icon_${kind}_White_256`;
    const svgSource = iconSvg(kind);
    const svgPath = path.join(iconVectorRoot, `${baseName}.svg`);
    const pngPath = path.join(iconRoot, `${baseName}.png`);
    await fs.promises.writeFile(svgPath, svgSource, 'utf8');
    await rasterize(svgSource, pngPath);
    const result = await verifyIcon(pngPath);
    manifest.files.push({
      type: 'icon',
      icon: kind,
      file: path.relative(assetRoot, pngPath).replaceAll('\\', '/'),
      sha256: sha256(pngPath),
      alphaBounds: result.bounds,
      alphaCentroid: {
        x: Number(result.centroid.x.toFixed(3)),
        y: Number(result.centroid.y.toFixed(3))
      }
    });
  }

  for (const stateName of Object.keys(states)) {
    const baseName = `T_FF_Title_CleanCasual_Surface_${stateName}_1240x240_9Slice`;
    const svgSource = surfaceSvg(stateName);
    const svgPath = path.join(surfaceVectorRoot, `${baseName}.svg`);
    const pngPath = path.join(surfaceRoot, `${baseName}.png`);
    await fs.promises.writeFile(svgPath, svgSource, 'utf8');
    await rasterize(svgSource, pngPath);
    const bounds = await verifyButton(pngPath);
    manifest.files.push({
      type: 'scalable-surface',
      state: stateName,
      file: path.relative(assetRoot, pngPath).replaceAll('\\', '/'),
      sha256: sha256(pngPath),
      alphaBounds: bounds,
      drawAs: 'Box',
      brushImageSize: nineSlice.brushImageSize,
      marginNormalized: nineSlice.marginNormalized
    });
  }

  const plateAssets = [
    ['Base', plateSvg('Normal')],
    ['FocusOverlay', focusOverlaySvg()]
  ];
  for (const [name, svgSource] of plateAssets) {
    const baseName = `T_FF_Title_CleanCasual_IconPlate_${name}_256`;
    const svgPath = path.join(plateVectorRoot, `${baseName}.svg`);
    const pngPath = path.join(plateRoot, `${baseName}.png`);
    await fs.promises.writeFile(svgPath, svgSource, 'utf8');
    await rasterize(svgSource, pngPath);
    const bounds = await verifyTransparentAsset(pngPath, plateSize);
    manifest.files.push({
      type: 'icon-plate',
      name,
      file: path.relative(assetRoot, pngPath).replaceAll('\\', '/'),
      sha256: sha256(pngPath),
      alphaBounds: bounds,
      drawAs: 'Image'
    });
  }

  const previewSource = previewSvg();
  const previewSvgPath = path.join(previewRoot, 'Preview_FF_Title_CleanCasual_Buttons_1560x920.svg');
  const previewPngPath = path.join(previewRoot, 'Preview_FF_Title_CleanCasual_Buttons_1560x920.png');
  await fs.promises.writeFile(previewSvgPath, previewSource, 'utf8');
  await rasterize(previewSource, previewPngPath);

  const scalablePreviewSource = scalablePreviewSvg();
  const scalablePreviewSvgPath = path.join(previewRoot, 'Preview_FF_Title_CleanCasual_Scalable_1600x900.svg');
  const scalablePreviewPngPath = path.join(previewRoot, 'Preview_FF_Title_CleanCasual_Scalable_1600x900.png');
  await fs.promises.writeFile(scalablePreviewSvgPath, scalablePreviewSource, 'utf8');
  await rasterize(scalablePreviewSource, scalablePreviewPngPath);

  const layoutReference = {
    status: 'assumption',
    sourceSize,
    displaySize,
    scale: { x: 0.5, y: 0.5 },
    brushDrawAs: 'Image',
    surfaceAtDisplay: { x: 4, y: 4, width: 612, height: 104, cornerRadius: 32 },
    iconPlateAtDisplay: { centerX: 62, centerY: 56, radius: 40 },
    contentAtDisplay: {
      iconSlot: { x: 38, y: 32, width: 48, height: 48 },
      labelStartX: 154,
      labelFontSize: 34,
      chevronSlot: { x: 559, y: 32, width: 48, height: 48 }
    },
    note: 'Keep labels and icons as UMG widgets; the PNG contains only the surface and icon plate.'
  };
  await fs.promises.writeFile(
    path.join(assetRoot, 'button-layout-reference.json'),
    `${JSON.stringify(layoutReference, null, 2)}\n`,
    'utf8'
  );
  const scalableLayoutReference = {
    status: 'assumption',
    designSize: { width: 620, height: 120 },
    safeWidthRangeAtStandardHeight: { min: 420, max: 1200, height: 120 },
    recommendedHeightTokens: [96, 120, 144],
    safeUniformScale: { min: 0.8, max: 2.0 },
    surface: {
      textureSize: sourceSize,
      brushDrawAs: 'Box',
      brushImageSize: nineSlice.brushImageSize,
      marginPixels: nineSlice.marginPixels,
      marginNormalized: nineSlice.marginNormalized
    },
    plate: {
      textureSize: plateSize,
      slotAtStandard: { x: 18, y: 12, width: 88, height: 88 },
      visibleCircleDiameterAtStandard: 80,
      focusOverlayWidgetName: 'Image_FocusRing'
    },
    contentAtStandard: {
      iconSlot: { x: 38, y: 32, width: 48, height: 48 },
      labelStartX: 154,
      labelFontSize: 34,
      chevronSlot: { xFromRight: 61, y: 32, width: 48, height: 48 }
    },
    scalingRule: 'For width-only changes, stretch only the 9-slice surface. For height changes, scale plate, icon, font, padding, and brush image size by ButtonHeight / 120.'
  };
  await fs.promises.writeFile(
    path.join(assetRoot, 'scalable-button-layout-reference.json'),
    `${JSON.stringify(scalableLayoutReference, null, 2)}\n`,
    'utf8'
  );
  await fs.promises.writeFile(
    path.join(assetRoot, 'asset-manifest-buttons.json'),
    `${JSON.stringify(manifest, null, 2)}\n`,
    'utf8'
  );

  for (const item of manifest.files) {
    const label = item.state || item.icon || item.name;
    const centroid = item.alphaCentroid ? ` | centroid ${JSON.stringify(item.alphaCentroid)}` : '';
    process.stdout.write(`${label.padEnd(8)} | ${item.file} | bounds ${JSON.stringify(item.alphaBounds)}${centroid} | ${item.sha256}\n`);
  }
  process.stdout.write(`Preview  | ${path.relative(projectRoot, previewPngPath)}\n`);
  process.stdout.write(`Scalable | ${path.relative(projectRoot, scalablePreviewPngPath)}\n`);
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
