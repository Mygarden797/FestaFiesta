const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const os = require('os');
let sharp;
try { sharp = require('sharp'); }
catch (_) { sharp = require(path.join(os.homedir(), '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp')); }

const projectRoot = path.resolve(__dirname, '../..');
const liveHudRoot = path.join(projectRoot, 'SourceArt/UI/LiveHUD');
const baseRoot = path.join(liveHudRoot, 'LightTealParts_v1');

const fixed = {
  white: '#FFFFFF',
  beige: '#E7E3E3',
  amber: '#E39A22',
  danger: '#D74E43',
  info: '#3D86B4'
};

const themes = [
  {
    id: 'ocean-blue',
    folder: 'OceanBlueParts_v1',
    label: 'Ocean Blue',
    mood: '시원하고 개방적인 해안·야외 축제 운영 UI',
    palette: {
      ...fixed,
      teal: '#46788C',
      tealDeep: '#294F60',
      tealSoft: '#B9CED5',
      ink: '#24343A',
      line: '#CDD8DC',
      lineStrong: '#9FB6BF',
      neutral: '#737E82'
    },
    auxiliaries: {
      '#10251D': '#132830', '#244438': '#203F4D', '#274A3C': '#244857',
      '#628F7C': '#568A9D', '#89A899': '#78A5B4', '#8FA69A': '#91ACB5',
      '#B9C4BD': '#C2CED2', '#C7D0CA': '#CBD6DA', '#C9D1CC': '#CDD7DB',
      '#DCE3DE': '#DEE7EA', '#E9F0EC': '#EBF2F4', '#F2F7F4': '#F1F6F7',
      '#F3F5F3': '#F3F6F7', '#F3F7F4': '#F2F7F8', '#F8FAF8': '#F8FAFB'
    }
  },
  {
    id: 'festival-violet',
    folder: 'FestivalVioletParts_v1',
    label: 'Festival Violet',
    mood: '야간 공연과 음악 축제에 어울리는 세련된 보랏빛 UI',
    palette: {
      ...fixed,
      teal: '#74658D',
      tealDeep: '#4C405F',
      tealSoft: '#CDC4D8',
      ink: '#302C38',
      line: '#D8D3DC',
      lineStrong: '#B3A9BF',
      neutral: '#7E7885'
    },
    auxiliaries: {
      '#10251D': '#271F31', '#244438': '#3C324C', '#274A3C': '#453A57',
      '#628F7C': '#85759F', '#89A899': '#A293B5', '#8FA69A': '#AEA4BA',
      '#B9C4BD': '#CBC5D0', '#C7D0CA': '#D2CDD6', '#C9D1CC': '#D4CFD8',
      '#DCE3DE': '#E4E0E7', '#E9F0EC': '#F0EDF3', '#F2F7F4': '#F6F3F8',
      '#F3F5F3': '#F5F3F6', '#F3F7F4': '#F6F3F8', '#F8FAF8': '#FBF9FC'
    }
  },
  {
    id: 'graphite-mint',
    folder: 'GraphiteMintParts_v1',
    label: 'Graphite Mint',
    mood: '짙은 차콜과 절제된 민트를 결합한 전문적인 운영·관리 중심 UI',
    palette: {
      ...fixed,
      teal: '#48635E',
      tealDeep: '#263A36',
      tealSoft: '#B8CBC5',
      ink: '#222D2A',
      line: '#CAD5D1',
      lineStrong: '#96AAA4',
      neutral: '#697571'
    },
    auxiliaries: {
      '#10251D': '#14231F', '#244438': '#20312D', '#274A3C': '#243832',
      '#628F7C': '#58756F', '#89A899': '#809C94', '#8FA69A': '#899F98',
      '#B9C4BD': '#C1CCC8', '#C7D0CA': '#C9D4D0', '#C9D1CC': '#CCD6D2',
      '#DCE3DE': '#DDE6E3', '#E9F0EC': '#EBF1EF', '#F2F7F4': '#F1F6F4',
      '#F3F5F3': '#F3F6F5', '#F3F7F4': '#F2F7F5', '#F8FAF8': '#F8FAF9'
    }
  },
  {
    id: 'royal-indigo',
    folder: 'RoyalIndigoParts_v1',
    label: 'Royal Indigo',
    mood: '현대적이고 게임다운 선명한 인디고 운영 UI',
    palette: {
      ...fixed,
      teal: '#566C98',
      tealDeep: '#35466B',
      tealSoft: '#C0CAE0',
      ink: '#293044',
      line: '#D1D6E1',
      lineStrong: '#A6B1CB',
      neutral: '#747A89'
    },
    auxiliaries: {
      '#10251D': '#1C2438', '#244438': '#2B3958', '#274A3C': '#304064',
      '#628F7C': '#687FAB', '#89A899': '#8D9DC0', '#8FA69A': '#99A6C1',
      '#B9C4BD': '#C7CDDB', '#C7D0CA': '#CFD4DF', '#C9D1CC': '#D1D6E0',
      '#DCE3DE': '#E1E5ED', '#E9F0EC': '#EEF0F6', '#F2F7F4': '#F4F5F9',
      '#F3F5F3': '#F5F6F8', '#F3F7F4': '#F4F6FA', '#F8FAF8': '#FAFAFC'
    }
  }
];

const baseColors = {
  '#FFFFFF': 'white',
  '#527C6B': 'teal',
  '#315747': 'tealDeep',
  '#B4C8BB': 'tealSoft',
  '#E7E3E3': 'beige',
  '#24332E': 'ink',
  '#CCD5CF': 'line',
  '#9DB1A5': 'lineStrong',
  '#E39A22': 'amber',
  '#D74E43': 'danger',
  '#3D86B4': 'info',
  '#777F7B': 'neutral'
};

function assertSafeTarget(targetRoot, folder) {
  const relative = path.relative(liveHudRoot, targetRoot);
  if (relative !== folder || relative.startsWith('..') || path.isAbsolute(relative)) {
    throw new Error(`Unsafe output path: ${targetRoot}`);
  }
}

function themeColorMap(theme) {
  const map = { ...theme.auxiliaries };
  for (const [source, key] of Object.entries(baseColors)) map[source] = theme.palette[key];
  // Semantic colors and true black stay identical across themes.
  map['#9F2D28'] = '#9F2D28';
  map['#A96912'] = '#A96912';
  map['#F1F7FA'] = '#F1F7FA';
  map['#FFF8E9'] = '#FFF8E9';
  map['#000000'] = '#000000';
  return map;
}

function recolorSvg(source, map) {
  return source.replace(/#[0-9A-Fa-f]{6}/g, color => map[color.toUpperCase()] || color.toUpperCase());
}

async function renderSvgToPng(source, pngPath, tileable) {
  await fs.promises.mkdir(path.dirname(pngPath), { recursive: true });
  await sharp(Buffer.from(source)).png().toFile(pngPath);
  if (!tileable) return;

  const { data, info } = await sharp(pngPath).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  const { width, height, channels } = info;
  for (let y = 0; y < height; y++) {
    for (let c = 0; c < channels; c++) {
      data[(y * width + width - 1) * channels + c] = data[y * width * channels + c];
    }
  }
  for (let x = 0; x < width; x++) {
    for (let c = 0; c < channels; c++) {
      data[((height - 1) * width + x) * channels + c] = data[x * channels + c];
    }
  }
  const tempPath = `${pngPath}.tile.png`;
  await sharp(data, { raw: info }).png().toFile(tempPath);
  await fs.promises.rename(tempPath, pngPath);
}

function svg(width, height, body) {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">${body}</svg>`;
}

async function makePreview(root, theme) {
  const C = theme.palette;
  const width = 2560;
  const height = 1440;
  const background = { create: { width, height, channels: 4, background: { r: 231, g: 227, b: 227, alpha: 1 } } };
  const layers = [];
  const text = (label, x, y, size = 30, color = C.ink) => {
    const escaped = label.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    layers.push({ input: Buffer.from(svg(Math.max(560, label.length * size), size + 24, `<text x="0" y="${size}" fill="${color}" font-family="Segoe UI, Arial" font-size="${size}" font-weight="700">${escaped}</text>`)), left: x, top: y });
  };
  const add = async (relative, x, y, w, h, tint = null) => {
    let image = sharp(path.join(root, relative)).resize(w, h, { fit: 'contain' });
    if (tint) image = image.tint(tint);
    layers.push({ input: await image.png().toBuffer(), left: x, top: y });
  };

  text(`FESTA FIESTA · LIVE HUD ${theme.label.toUpperCase()} PARTS`, 70, 45, 40, C.tealDeep);
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
  for (let i = 0; i < nav.length; i++) {
    await add(`Icons/Navigation/T_FF_LH_Icon_${nav[i]}_White_256.png`, 80 + i * 170, 790, 110, 110, C.tealDeep);
  }

  text('METRICS / ACTIONS / STATUS', 70, 930, 28, C.teal);
  const rows = [
    ['Metrics', 'Gold'], ['Metrics', 'Visitors'], ['Metrics', 'StaffCount'], ['Metrics', 'Clock'],
    ['Metrics', 'Satisfaction'], ['Metrics', 'Safety'], ['Metrics', 'Revenue'], ['Metrics', 'Wait'],
    ['Actions', 'Locate'], ['Actions', 'AssignStaff'], ['Actions', 'Play'], ['Actions', 'Pause'],
    ['Actions', 'FastForward'], ['Status', 'StatusWarning'], ['Status', 'StatusDanger']
  ];
  for (let i = 0; i < rows.length; i++) {
    const tint = i === 0 ? null : (i === 13 ? C.amber : (i === 14 ? C.danger : C.tealDeep));
    await add(`Icons/${rows[i][0]}/T_FF_LH_Icon_${rows[i][1]}_White_256.png`, 80 + i * 155, 985, 92, 92, tint);
  }

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
  const filename = `Preview_FF_LiveHUD_${theme.folder.replace('Parts_v1', '')}Parts_2560x1440.png`;
  await sharp(background).composite(layers).png().toFile(path.join(previewDir, filename));
  return path.join(previewDir, filename);
}

async function makeIconQa(root, theme, assets) {
  const iconAssets = assets.filter(asset => asset.file.startsWith('Icons/')).sort((a, b) => a.file.localeCompare(b.file));
  const width = 1920;
  const height = 1080;
  const columns = 14;
  const startX = 72;
  const startY = 135;
  const cellW = 128;
  const cellH = 150;
  const layers = [];
  const title = `${theme.label.toUpperCase()} · ICON QA · 24px THEME / 48px WHITE · ${iconAssets.length} ASSETS`;
  layers.push({ input: Buffer.from(svg(1800, 70, `<text x="0" y="48" fill="#FFFFFF" font-family="Segoe UI, Arial" font-size="38" font-weight="700">${title}</text>`)), left: 42, top: 28 });

  for (let i = 0; i < iconAssets.length; i++) {
    const col = i % columns;
    const row = Math.floor(i / columns);
    const x = startX + col * cellW;
    const y = startY + row * cellH;
    const file = path.join(root, iconAssets[i].file);
    const small = await sharp(file).resize(24, 24, { fit: 'contain' }).tint(theme.palette.tealSoft).png().toBuffer();
    const large = await sharp(file).resize(48, 48, { fit: 'contain' }).png().toBuffer();
    layers.push({ input: small, left: x + 10, top: y });
    layers.push({ input: large, left: x, top: y + 45 });
  }

  await sharp({ create: { width, height, channels: 4, background: theme.palette.ink } })
    .composite(layers)
    .png()
    .toFile(path.join(root, 'Preview', `Preview_FF_LiveHUD_${theme.folder.replace('Parts_v1', '')}_IconQA_1920x1080.png`));
}

function makeReadme(theme) {
  const p = theme.palette;
  return `# FestaFiesta Live HUD ${theme.label} 파츠 v1\n\n` +
    `기존 Light Teal 파츠의 구조·크기·파일명을 유지하고 색상 토큰만 교체한 테마입니다. ${theme.mood}를 목표로 합니다.\n\n` +
    `## 팔레트\n\n` +
    `- 메인: \`${p.teal}\`\n- 진한색: \`${p.tealDeep}\`\n- 연한색: \`${p.tealSoft}\`\n` +
    `- 베이지: \`${p.beige}\`\n- 흰색: \`${p.white}\`\n- 본문 글자: \`${p.ink}\`\n` +
    `- 경고: \`${p.amber}\` / 위험: \`${p.danger}\` / 정보: \`${p.info}\`\n\n` +
    `## Unreal 사용 원칙\n\n` +
    `- 문구와 숫자는 텍스처에 굽지 않고 Unreal \`TextBlock\`으로 배치합니다.\n` +
    `- \`_White_\` 아이콘과 \`WhiteMask\` 진행바는 Brush Tint로 색을 지정합니다.\n` +
    `- \`_9Slice\` 파일은 Brush \`Draw As = Box\`로 설정하고 \`asset-manifest.json\`의 \`margin_px\`를 적용합니다.\n` +
    `- HeatMap의 \`_Tile\` 3종만 Address X/Y를 \`Wrap\`, Mip Gen을 \`FromTextureGroup\`으로 설정합니다.\n` +
    `- Preview와 \`VectorSource\`는 Unreal 임포트 대상이 아닙니다.\n` +
    `- 각 테마의 파일명이 같으므로 여러 테마를 동시에 임포트할 때는 Content 하위 폴더를 분리해야 합니다.\n\n` +
    `## 1920×1080 배치 기준\n\n` +
    `- 메뉴 행 29px + 상태 행 29px = 총 58px\n- 메뉴는 화면 중앙 정렬\n` +
    `- 클릭 패널은 하단 HUD에서 12px 이상 띄움\n- 아이콘은 256px 원본을 14~24px로 축소 사용\n\n` +
    `프로젝트 자체 제작 에셋이며 외부 에셋 의존성이 없습니다.\n`;
}

async function buildTheme(theme, baseManifest, baseTokens) {
  const root = path.join(liveHudRoot, theme.folder);
  assertSafeTarget(root, theme.folder);
  await fs.promises.rm(root, { recursive: true, force: true });
  await fs.promises.mkdir(root, { recursive: true });
  const map = themeColorMap(theme);

  for (const asset of baseManifest.assets) {
    const sourceSvgPath = path.join(baseRoot, asset.source_svg);
    const targetSvgPath = path.join(root, asset.source_svg);
    const targetPngPath = path.join(root, asset.file);
    const recolored = recolorSvg(await fs.promises.readFile(sourceSvgPath, 'utf8'), map);
    await fs.promises.mkdir(path.dirname(targetSvgPath), { recursive: true });
    await fs.promises.writeFile(targetSvgPath, recolored, 'utf8');
    await renderSvgToPng(recolored, targetPngPath, asset.tileable);
  }

  const assets = [];
  for (const baseAsset of baseManifest.assets) {
    const full = path.join(root, baseAsset.file);
    const info = await sharp(full).metadata();
    if (info.width !== baseAsset.width || info.height !== baseAsset.height || !info.hasAlpha) {
      throw new Error(`${theme.label}: invalid output ${baseAsset.file}`);
    }
    const data = await fs.promises.readFile(full);
    assets.push({
      ...baseAsset,
      has_alpha: !!info.hasAlpha,
      sha256: crypto.createHash('sha256').update(data).digest('hex').toUpperCase()
    });
  }

  const manifest = {
    ...baseManifest,
    pack: `FestaFiesta Live HUD ${theme.label} Parts`,
    version: '1.0.0',
    authored_date: '2026-08-12',
    theme_id: theme.id,
    derived_from: 'LightTealParts_v1',
    mood: theme.mood,
    palette: theme.palette,
    assets
  };
  const tokens = {
    ...baseTokens,
    theme: { id: theme.id, name: theme.label, mood: theme.mood, derived_from: 'LightTealParts_v1' },
    palette: theme.palette
  };

  await fs.promises.copyFile(path.join(baseRoot, 'menu-part-map.json'), path.join(root, 'menu-part-map.json'));
  await fs.promises.writeFile(path.join(root, 'asset-manifest.json'), JSON.stringify(manifest, null, 2) + '\n', 'utf8');
  await fs.promises.writeFile(path.join(root, 'style-tokens.json'), JSON.stringify(tokens, null, 2) + '\n', 'utf8');
  await fs.promises.writeFile(path.join(root, 'README_UNREAL_KO.md'), makeReadme(theme), 'utf8');
  const previewPath = await makePreview(root, theme);
  await makeIconQa(root, theme, assets);

  return { theme, root, previewPath, assetCount: assets.length };
}

async function makeContactSheet(results) {
  const width = 2560;
  const height = 1440;
  const layers = [];
  const title = 'FESTA FIESTA · LIVE HUD COLOR THEMES';
  layers.push({ input: Buffer.from(svg(1800, 70, `<text x="0" y="48" fill="#24332E" font-family="Segoe UI, Arial" font-size="42" font-weight="700">${title}</text>`)), left: 70, top: 24 });
  const positions = [[70, 110], [1305, 110], [70, 770], [1305, 770]];
  for (let i = 0; i < results.length; i++) {
    const image = await sharp(results[i].previewPath).resize(1185, 667, { fit: 'contain' }).png().toBuffer();
    layers.push({ input: image, left: positions[i][0], top: positions[i][1] });
  }
  const output = path.join(liveHudRoot, 'Preview_FF_LiveHUD_4ColorThemes_2560x1440.png');
  await sharp({ create: { width, height, channels: 4, background: '#F3F1EF' } }).composite(layers).png().toFile(output);
  return output;
}

async function main() {
  const baseManifest = JSON.parse(await fs.promises.readFile(path.join(baseRoot, 'asset-manifest.json'), 'utf8'));
  const baseTokens = JSON.parse(await fs.promises.readFile(path.join(baseRoot, 'style-tokens.json'), 'utf8'));
  const results = [];
  for (const theme of themes) results.push(await buildTheme(theme, baseManifest, baseTokens));
  const contactSheet = await makeContactSheet(results);

  const catalog = {
    version: '1.0.0',
    base_theme: { id: 'forest-teal', folder: 'LightTealParts_v1' },
    variants: themes.map(theme => ({ id: theme.id, name: theme.label, folder: theme.folder, palette: theme.palette, mood: theme.mood })),
    shared_semantic_colors: { amber: fixed.amber, danger: fixed.danger, info: fixed.info },
    import_note: 'Asset filenames are intentionally identical for drop-in theme replacement. Use separate Unreal Content subfolders when importing more than one theme.'
  };
  await fs.promises.writeFile(path.join(liveHudRoot, 'theme-catalog.json'), JSON.stringify(catalog, null, 2) + '\n', 'utf8');

  for (const result of results) console.log(`${result.theme.label}: ${result.assetCount} PNG + SVG assets -> ${result.root}`);
  console.log(`Contact sheet -> ${contactSheet}`);
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
