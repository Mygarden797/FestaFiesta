// validate_festafiesta_dt_ui_v2_kit.js
// 역할: UI 이미지·화면 에셋 작업에서 저장된 산출물의 구조와 품질을 검증한다.

const fs = require('fs');
const path = require('path');
const sharp = require('sharp');

const projectRoot = path.resolve(__dirname, '../..');
const designRoot = path.join(projectRoot, 'Design', 'UXUI', 'FestaFiesta_DesignSystem_v2');
const kitRoot = path.join(designRoot, 'ComponentKit');
const manifestPath = path.join(kitRoot, 'manifest.json');

const expectedScreens = [
  ['00-v2-screen-overview.png', 3840, 2160],
  ['01-spatial-operations.png', 1672, 941],
  ['02-facility-asset-inspector.png', 1672, 941],
  ['03-scenario-layer-analysis.png', 1672, 941],
  ['04-build-workspace.png', 1672, 941],
  ['05-settings-commonui.png', 1672, 941]
];

async function main() {
  const errors = [];
  const manifest = JSON.parse(await fs.promises.readFile(manifestPath, 'utf8'));
  if (manifest.assets.length !== 91) {
    errors.push(`Expected 91 manifest assets; received ${manifest.assets.length}.`);
  }

  const counts = {};
  for (const asset of manifest.assets) {
    counts[asset.type] = (counts[asset.type] || 0) + 1;
    const pngPath = path.join(kitRoot, asset.file);
    const svgPath = path.join(kitRoot, asset.vector_source);
    if (!fs.existsSync(pngPath)) errors.push(`Missing PNG: ${asset.file}`);
    if (!fs.existsSync(svgPath)) errors.push(`Missing SVG: ${asset.vector_source}`);
    if (!fs.existsSync(pngPath)) continue;
    const metadata = await sharp(pngPath).metadata();
    if (metadata.width !== asset.size[0] || metadata.height !== asset.size[1]) {
      errors.push(`Size mismatch: ${asset.file}`);
    }
    if (!metadata.hasAlpha) errors.push(`Expected alpha channel: ${asset.file}`);
  }

  const screenResults = [];
  for (const [file, width, height] of expectedScreens) {
    const filePath = path.join(designRoot, file);
    if (!fs.existsSync(filePath)) {
      errors.push(`Missing screen: ${file}`);
      continue;
    }
    const metadata = await sharp(filePath).metadata();
    const ok = metadata.width === width && metadata.height === height;
    if (!ok) errors.push(`Screen size mismatch: ${file}`);
    screenResults.push({ file, width: metadata.width, height: metadata.height, ok });
  }

  const contactSheets = [
    'buttons-contact-sheet.png',
    'controls-contact-sheet.png',
    'widgets-contact-sheet.png'
  ];
  for (const file of contactSheets) {
    if (!fs.existsSync(path.join(kitRoot, file))) errors.push(`Missing contact sheet: ${file}`);
  }

  const result = {
    ok: errors.length === 0,
    manifest_assets: manifest.assets.length,
    counts,
    screens: screenResults,
    contact_sheets: contactSheets,
    errors
  };
  process.stdout.write(`${JSON.stringify(result, null, 2)}\n`);
  if (errors.length) process.exitCode = 1;
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
