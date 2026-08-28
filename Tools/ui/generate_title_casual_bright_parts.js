const fs = require('fs');
const path = require('path');
const sharp = require('C:/Users/jt/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');

const projectRoot = path.resolve(__dirname, '../..');
const assetRoot = path.join(projectRoot, 'SourceArt/UI/Title/CasualBrightParts_v1');
const buttonRoot = path.join(assetRoot, 'Buttons');
const iconRoot = path.join(assetRoot, 'Icons');
const buttonVectorRoot = path.join(assetRoot, 'VectorSource/Buttons');
const iconVectorRoot = path.join(assetRoot, 'VectorSource/Icons');

const buttonSize = { width: 1024, height: 256 };
const iconSize = { width: 256, height: 256 };

function svgDocument(width, height, body, description, viewBox = `0 0 ${width} ${height}`) {
  return [
    `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="${viewBox}">`,
    `  <title>${description}</title>`,
    body,
    '</svg>',
    ''
  ].join('\n');
}

function buttonDefinitions(id, top, bottom, shadowOpacity = 0.16) {
  return `  <defs>
    <linearGradient id="surface-${id}" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="${top}"/>
      <stop offset="1" stop-color="${bottom}"/>
    </linearGradient>
    <linearGradient id="plate-${id}" x1="0.18" y1="0.1" x2="0.82" y2="0.9">
      <stop offset="0" stop-color="#F7FFFA"/>
      <stop offset="0.42" stop-color="#DDF8E9"/>
      <stop offset="1" stop-color="#BDEBCF"/>
    </linearGradient>
    <filter id="shadow-${id}" x="-10%" y="-35%" width="120%" height="180%" color-interpolation-filters="sRGB">
      <feGaussianBlur in="SourceAlpha" stdDeviation="13" result="blur"/>
      <feOffset in="blur" dy="10" result="offset"/>
      <feFlood flood-color="#498E70" flood-opacity="${shadowOpacity}" result="color"/>
      <feComposite in="color" in2="offset" operator="in" result="shadow"/>
      <feMerge>
        <feMergeNode in="shadow"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>
    <filter id="mint-glow-${id}" x="-10%" y="-45%" width="120%" height="200%" color-interpolation-filters="sRGB">
      <feGaussianBlur stdDeviation="10"/>
    </filter>
  </defs>`;
}

function normalButton() {
  return svgDocument(buttonSize.width, buttonSize.height, `${buttonDefinitions('normal', '#FFFFFF', '#F4FFF8')}
  <g filter="url(#shadow-normal)">
    <rect x="28" y="33" width="968" height="188" rx="80" ry="94" fill="url(#surface-normal)" stroke="#A9DFC6" stroke-width="5"/>
    <rect x="37" y="42" width="950" height="170" rx="72" ry="85" fill="none" stroke="#FFFFFF" stroke-opacity="0.9" stroke-width="3"/>
    <ellipse cx="127" cy="127" rx="69" ry="84" fill="url(#plate-normal)" stroke="#91D8B7" stroke-width="5"/>
    <ellipse cx="127" cy="117" rx="54" ry="59" fill="#FFFFFF" fill-opacity="0.34"/>
  </g>`, 'FestaFiesta Casual Bright title button — normal');
}

function focusedButton() {
  return svgDocument(buttonSize.width, buttonSize.height, `${buttonDefinitions('focused', '#FFFFFF', '#F1FFF7', 0.2)}
  <rect x="20" y="24" width="984" height="206" rx="88" ry="103" fill="none" stroke="#76D7AD" stroke-opacity="0.28" stroke-width="16" filter="url(#mint-glow-focused)"/>
  <g filter="url(#shadow-focused)">
    <rect x="24" y="28" width="976" height="198" rx="84" ry="99" fill="url(#surface-focused)" stroke="#57C794" stroke-width="8"/>
    <rect x="36" y="40" width="952" height="174" rx="72" ry="87" fill="none" stroke="#C7F4DD" stroke-width="4"/>
    <ellipse cx="127" cy="127" rx="72" ry="88" fill="#BFEFD4" stroke="#42BA83" stroke-width="7"/>
    <ellipse cx="127" cy="115" rx="55" ry="60" fill="#FFFFFF" fill-opacity="0.38"/>
  </g>`, 'FestaFiesta Casual Bright title button — focused');
}

function pressedButton() {
  return svgDocument(buttonSize.width, buttonSize.height, `${buttonDefinitions('pressed', '#D9F8E7', '#BCECCD', 0.1)}
  <g filter="url(#shadow-pressed)">
    <rect x="30" y="41" width="964" height="184" rx="78" ry="92" fill="url(#surface-pressed)" stroke="#4AAF7D" stroke-width="7"/>
    <rect x="41" y="52" width="942" height="162" rx="67" ry="81" fill="none" stroke="#79CCA2" stroke-width="4"/>
    <ellipse cx="127" cy="133" rx="68" ry="82" fill="#F8FFFB" stroke="#62BD90" stroke-width="6"/>
    <ellipse cx="127" cy="124" rx="52" ry="57" fill="#DDF7E8" fill-opacity="0.55"/>
  </g>`, 'FestaFiesta Casual Bright title button — pressed');
}

function disabledButton() {
  return svgDocument(buttonSize.width, buttonSize.height, `${buttonDefinitions('disabled', '#FAFCFA', '#EDF2EE', 0.06)}
  <g opacity="0.72" filter="url(#shadow-disabled)">
    <rect x="28" y="33" width="968" height="188" rx="80" ry="94" fill="url(#surface-disabled)" stroke="#C7D5CD" stroke-width="5" stroke-dasharray="18 12"/>
    <rect x="39" y="44" width="946" height="166" rx="69" ry="83" fill="none" stroke="#FFFFFF" stroke-opacity="0.86" stroke-width="3"/>
    <ellipse cx="127" cy="127" rx="69" ry="84" fill="#E6EEE9" stroke="#B9C9C0" stroke-width="5"/>
    <path d="M82 127h90" fill="none" stroke="#AABCB2" stroke-width="8" stroke-linecap="round"/>
  </g>`, 'FestaFiesta Casual Bright title button — disabled');
}

const buttons = [
  ['T_FF_Title_CasualBright_Button_Normal_1024x256', normalButton()],
  ['T_FF_Title_CasualBright_Button_Focused_1024x256', focusedButton()],
  ['T_FF_Title_CasualBright_Button_Pressed_1024x256', pressedButton()],
  ['T_FF_Title_CasualBright_Button_Disabled_1024x256', disabledButton()]
];

// 기존 GlassOrbit V2 실루엣의 path와 stroke를 그대로 재사용한다.
const icons = [
  ['T_FF_Title_CasualBright_Icon_Play_White_256', svgDocument(256, 256,
    '  <path fill="#FFFFFF" d="M177 118c0-25 28-40 49-26l186 124c24 16 24 51 0 67L226 407c-21 14-49-1-49-27V118z"/>',
    'FestaFiesta Casual Bright play icon', '0 0 512 512')],
  ['T_FF_Title_CasualBright_Icon_Settings_White_256', svgDocument(256, 256,
    `  <mask id="gear">
    <rect width="128" height="128" fill="#000000"/>
    <g fill="#FFFFFF">
      <rect x="56" y="7" width="16" height="30" rx="5"/>
      <rect x="56" y="91" width="16" height="30" rx="5"/>
      <rect x="91" y="56" width="30" height="16" rx="5"/>
      <rect x="7" y="56" width="30" height="16" rx="5"/>
      <rect x="83" y="15" width="16" height="31" rx="5" transform="rotate(45 91 30.5)"/>
      <rect x="29" y="69" width="16" height="31" rx="5" transform="rotate(45 37 84.5)"/>
      <rect x="83" y="69" width="16" height="31" rx="5" transform="rotate(135 91 84.5)"/>
      <rect x="29" y="15" width="16" height="31" rx="5" transform="rotate(135 37 30.5)"/>
      <circle cx="64" cy="64" r="39"/>
    </g>
    <circle cx="64" cy="64" r="17" fill="#000000"/>
  </mask>
  <rect width="128" height="128" fill="#FFFFFF" mask="url(#gear)"/>`,
    'FestaFiesta Casual Bright settings icon', '0 0 128 128')],
  ['T_FF_Title_CasualBright_Icon_Power_White_256', svgDocument(256, 256,
    `  <g fill="none" stroke="#FFFFFF" stroke-width="30" stroke-linecap="round">
    <path d="M256 55v190"/>
    <path d="M174 120a177 177 0 1 0 164 0"/>
  </g>`,
    'FestaFiesta Casual Bright power icon', '0 0 512 512')],
  ['T_FF_Title_CasualBright_Icon_Chevron_White_256', svgDocument(256, 256,
    '  <path d="M182 92l164 164-164 164" fill="none" stroke="#FFFFFF" stroke-width="30" stroke-linecap="round" stroke-linejoin="round"/>',
    'FestaFiesta Casual Bright chevron icon', '0 0 512 512')]
];

async function rasterize(svgSource, outputPath) {
  await sharp(Buffer.from(svgSource, 'utf8'))
    .ensureAlpha()
    .png({ compressionLevel: 9, adaptiveFiltering: false, palette: false })
    .toFile(outputPath);
}

function pixelAt(data, info, x, y) {
  const index = (y * info.width + x) * info.channels;
  return Array.from(data.subarray(index, index + info.channels));
}

async function verifyPng(filePath, expected, whiteOnly) {
  const image = sharp(filePath);
  const metadata = await image.metadata();
  if (metadata.width !== expected.width || metadata.height !== expected.height) {
    throw new Error(`${path.basename(filePath)}: ${metadata.width}x${metadata.height}, expected ${expected.width}x${expected.height}`);
  }
  if (!metadata.hasAlpha) {
    throw new Error(`${path.basename(filePath)}: alpha channel missing`);
  }

  const { data, info } = await image.raw().toBuffer({ resolveWithObject: true });
  const corners = [
    pixelAt(data, info, 0, 0),
    pixelAt(data, info, info.width - 1, 0),
    pixelAt(data, info, 0, info.height - 1),
    pixelAt(data, info, info.width - 1, info.height - 1)
  ];
  if (corners.some(pixel => pixel[3] !== 0)) {
    throw new Error(`${path.basename(filePath)}: one or more corners are not transparent`);
  }

  let visiblePixels = 0;
  for (let i = 0; i < data.length; i += info.channels) {
    if (data[i + 3] === 0) continue;
    visiblePixels += 1;
    if (whiteOnly && (data[i] !== 255 || data[i + 1] !== 255 || data[i + 2] !== 255)) {
      throw new Error(`${path.basename(filePath)}: non-white visible RGB pixel found`);
    }
  }
  if (visiblePixels === 0) {
    throw new Error(`${path.basename(filePath)}: image is empty`);
  }
  return { metadata, corners, visiblePixels };
}

async function main() {
  for (const directory of [buttonRoot, iconRoot, buttonVectorRoot, iconVectorRoot]) {
    await fs.promises.mkdir(directory, { recursive: true });
  }

  const summary = [];
  for (const [name, source] of buttons) {
    const svgPath = path.join(buttonVectorRoot, `${name}.svg`);
    const pngPath = path.join(buttonRoot, `${name}.png`);
    await fs.promises.writeFile(svgPath, source, 'utf8');
    await rasterize(source, pngPath);
    const result = await verifyPng(pngPath, buttonSize, false);
    summary.push(`${path.relative(projectRoot, pngPath)} | ${result.metadata.width}x${result.metadata.height} | alpha | corners transparent`);
  }

  for (const [name, source] of icons) {
    const svgPath = path.join(iconVectorRoot, `${name}.svg`);
    const pngPath = path.join(iconRoot, `${name}.png`);
    await fs.promises.writeFile(svgPath, source, 'utf8');
    await rasterize(source, pngPath);
    const result = await verifyPng(pngPath, iconSize, true);
    summary.push(`${path.relative(projectRoot, pngPath)} | ${result.metadata.width}x${result.metadata.height} | white+alpha | corners transparent`);
  }

  process.stdout.write(`${summary.join('\n')}\n`);
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
