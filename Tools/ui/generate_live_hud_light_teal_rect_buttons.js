const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const sharp = require('C:/Users/jt/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');

const root = path.resolve(__dirname, '../../SourceArt/UI/LiveHUD/LightTealParts_v1');
const buttonRoot = path.join(root, 'Buttons');
const vectorRoot = path.join(root, 'VectorSource', 'Buttons');
const manifestPath = path.join(root, 'asset-manifest.json');
const menuPartMapPath = path.join(root, 'menu-part-map.json');
const readmePath = path.join(root, 'README_UNREAL_KO.md');

const C = {
  white: '#FFFFFF',
  teal: '#527C6B',
  tealDeep: '#315747',
  tealSoft: '#B4C8BB',
  beige: '#E7E3E3',
  line: '#CCD5CF',
  danger: '#D74E43'
};

const variants = [
  ['Nav_Default', C.white, 'transparent', 0, 1, '사각형 메뉴 기본 상태.'],
  ['Nav_Hover', '#E9F0EC', C.tealSoft, 4, 1, '사각형 메뉴 Hover/Focus 상태.'],
  ['Nav_Selected', C.teal, C.tealDeep, 4, 1, '사각형 메뉴 선택 상태.'],
  ['Nav_Pressed', C.tealDeep, '#244438', 4, 1, '사각형 메뉴 Pressed 상태.'],
  ['Nav_Disabled', C.beige, C.line, 3, .72, '사각형 메뉴 비활성 상태.'],
  ['Primary_Default', C.teal, C.tealDeep, 4, 1, '사각형 주 행동 기본 상태.'],
  ['Primary_Hover', '#628F7C', '#274A3C', 5, 1, '사각형 주 행동 Hover/Focus 상태.'],
  ['Secondary_Default', C.white, C.teal, 4, 1, '사각형 보조 행동 기본 상태.'],
  ['Danger_Default', C.danger, '#9F2D28', 4, 1, '사각형 위험 행동 기본 상태.']
];

function svg(width, height, body) {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}"><defs></defs>${body}</svg>`;
}

function rectBody(fill, stroke, strokeWidth, opacity) {
  const inset = 4 + strokeWidth / 2;
  const width = 504 - strokeWidth;
  const height = 120 - strokeWidth;
  return `<rect x="${inset}" y="${inset}" width="${width}" height="${height}" fill="${fill}" fill-opacity="${opacity}" stroke="${stroke}" stroke-width="${strokeWidth}" shape-rendering="geometricPrecision"/>`;
}

function sha256(filePath) {
  return crypto.createHash('sha256').update(fs.readFileSync(filePath)).digest('hex').toUpperCase();
}

async function writeButtons() {
  await fs.promises.mkdir(buttonRoot, { recursive: true });
  await fs.promises.mkdir(vectorRoot, { recursive: true });

  const records = [];
  for (const [state, fill, stroke, strokeWidth, opacity, usage] of variants) {
    const name = `T_FF_LH_Button_Rect_${state}_512x128_9Slice`;
    const source = svg(512, 128, rectBody(fill, stroke, strokeWidth, opacity));
    const svgPath = path.join(vectorRoot, `${name}.svg`);
    const pngPath = path.join(buttonRoot, `${name}.png`);
    await fs.promises.writeFile(svgPath, source, 'utf8');
    await sharp(Buffer.from(source)).png().toFile(pngPath);
    const info = await sharp(pngPath).metadata();
    records.push({
      file: `Buttons/${name}.png`,
      source_svg: `VectorSource/Buttons/${name}.svg`,
      width: 512,
      height: 128,
      draw_as: 'Box',
      margin_px: 12,
      tintable: false,
      tileable: false,
      target_size_1080p: '높이 29-44px',
      usage,
      has_alpha: !!info.hasAlpha,
      compression: 'UserInterface2D (RGBA)',
      texture_group: 'UI',
      srgb: true,
      mip_gen: 'NoMipmaps',
      never_stream: true,
      address: 'Clamp',
      sha256: sha256(pngPath)
    });
  }
  return records;
}

async function updateManifest(records) {
  const manifest = JSON.parse(await fs.promises.readFile(manifestPath, 'utf8'));
  manifest.assets = manifest.assets
    .filter(entry => !entry.file.startsWith('Buttons/T_FF_LH_Button_Rect_'))
    .concat(records)
    .sort((a, b) => a.file.localeCompare(b.file));
  manifest.version = '1.2.0';
  manifest.authored_date = '2026-08-14';
  await fs.promises.writeFile(manifestPath, JSON.stringify(manifest, null, 2) + '\n', 'utf8');
}

async function updateMenuPartMap(records) {
  const map = JSON.parse(await fs.promises.readFile(menuPartMapPath, 'utf8'));
  map.alternatives = map.alternatives || {};
  map.alternatives.rect_buttons = records.map(record => record.file);
  await fs.promises.writeFile(menuPartMapPath, JSON.stringify(map, null, 2) + '\n', 'utf8');
}

async function updateReadme() {
  let readme = await fs.promises.readFile(readmePath, 'utf8');
  const section = `## 사각형 버튼 대체 세트\n\n- \`Buttons/T_FF_LH_Button_Rect_*\` 9종은 기존 둥근 버튼과 같은 팔레트·상태 체계를 쓰는 모서리 반경 0의 대체 세트다.\n- Nav 5상태(Default·Hover·Selected·Pressed·Disabled)와 Action 4상태(Primary Default·Primary Hover·Secondary·Danger)를 제공한다.\n- 원본은 512×128, Brush \`Draw As = Box\`, \`margin_px = 12\`다. 1920×1080에서는 기존 버튼과 같은 높이 29~44px로 사용한다.\n- 둥근 버튼을 대체할 때 텍스트·아이콘·포커스 표시 규칙은 그대로 유지한다.\n\n`;
  readme = readme.replace(/## 사각형 버튼 대체 세트[\s\S]*?(?=## |$)/, '');
  readme = readme.replace('## 공통 임포트 설정\n', section + '## 공통 임포트 설정\n');
  await fs.promises.writeFile(readmePath, readme, 'utf8');
}

async function writePreview() {
  const rows = variants.map(([state, fill, stroke, strokeWidth, opacity], index) => {
    const y = 130 + index * 78;
    const label = state.replace('_', ' · ');
    const rounded = `<rect x="220" y="${y}" width="420" height="58" rx="12" fill="${fill}" fill-opacity="${opacity}" stroke="${stroke}" stroke-width="${Math.max(0, strokeWidth * .6)}"/>`;
    const square = `<rect x="780" y="${y}" width="420" height="58" fill="${fill}" fill-opacity="${opacity}" stroke="${stroke}" stroke-width="${Math.max(0, strokeWidth * .6)}"/>`;
    return `<text x="36" y="${y + 36}" fill="#315747" font-family="Segoe UI, Arial, sans-serif" font-size="20" font-weight="700">${label}</text>${rounded}${square}`;
  }).join('');
  const source = svg(1240, 880,
    `<rect width="1240" height="880" fill="#E7E3E3"/>` +
    `<text x="36" y="50" fill="#315747" font-family="Segoe UI, Arial, sans-serif" font-size="30" font-weight="800">LIGHT TEAL BUTTON SHAPE COMPARISON</text>` +
    `<text x="220" y="100" fill="#527C6B" font-family="Segoe UI, Arial, sans-serif" font-size="20" font-weight="700">기존 둥근 버튼</text>` +
    `<text x="780" y="100" fill="#527C6B" font-family="Segoe UI, Arial, sans-serif" font-size="20" font-weight="700">신규 사각형 버튼</text>` + rows);
  const previewPath = path.join(root, 'Preview', 'Preview_FF_LiveHUD_LightTeal_RectButtons_1240x880.png');
  await fs.promises.mkdir(path.dirname(previewPath), { recursive: true });
  await sharp(Buffer.from(source)).png().toFile(previewPath);
}

async function verify(records) {
  const failures = [];
  for (const record of records) {
    const filePath = path.join(root, record.file);
    const info = await sharp(filePath).metadata();
    if (info.width !== 512 || info.height !== 128) failures.push(`${record.file}: ${info.width}x${info.height}`);
    if (!info.hasAlpha) failures.push(`${record.file}: alpha missing`);
    if (sha256(filePath) !== record.sha256) failures.push(`${record.file}: sha256 mismatch`);
  }
  if (failures.length) throw new Error(failures.join('\n'));
}

async function main() {
  const records = await writeButtons();
  await updateManifest(records);
  await updateMenuPartMap(records);
  await updateReadme();
  await writePreview();
  await verify(records);
  process.stdout.write(`Generated and registered ${records.length} LightTeal rectangular button assets.\n`);
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
