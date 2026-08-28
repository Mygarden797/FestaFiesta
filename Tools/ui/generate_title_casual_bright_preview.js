const fs = require('fs');
const path = require('path');
const sharp = require('C:/Users/jt/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');

const projectRoot = path.resolve(__dirname, '../..');
const root = path.join(projectRoot, 'SourceArt/UI/Title/CasualBrightParts_v1');
const output = path.join(root, 'Preview/Preview_FF_Title_CasualBright_Parts_1920x1440.png');

function label(text, width = 700, height = 48, size = 26) {
  const escaped = text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  return Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}">
    <text x="0" y="${size + 5}" fill="#315747" font-family="Segoe UI, Arial, sans-serif" font-size="${size}" font-weight="700">${escaped}</text>
  </svg>`);
}

function iconTile() {
  return Buffer.from('<svg xmlns="http://www.w3.org/2000/svg" width="160" height="160"><rect x="2" y="2" width="156" height="156" rx="38" fill="#315747" stroke="#8AD8B5" stroke-width="4"/></svg>');
}

async function image(relative, width, height, options = {}) {
  const source = path.join(root, ...relative.split('/'));
  if (!fs.existsSync(source)) throw new Error(`파일 없음: ${relative}`);
  return sharp(source)
    .resize(width, height, {
      fit: options.fit || 'contain',
      position: options.position || 'centre',
      background: options.background || { r: 0, g: 0, b: 0, alpha: 0 }
    })
    .png()
    .toBuffer();
}

async function main() {
  await fs.promises.mkdir(path.dirname(output), { recursive: true });
  const layers = [
    { input: label('FESTA FIESTA · CASUAL BRIGHT · IMPORT PARTS', 1500, 70, 36), left: 40, top: 24 },
    { input: label('BRIGHT BACKGROUND', 700), left: 40, top: 82 },
    { input: label('EARTH ALBEDO · 2:1 EQUIRECTANGULAR', 1000), left: 800, top: 82 },
    { input: await image('Background/T_FF_Title_CasualBright_Background_1920x1080.png', 720, 405, { fit: 'cover' }), left: 40, top: 130 },
    { input: await image('Earth/T_FF_Earth_CasualBright_Albedo_2048x1024.png', 1080, 405, { fit: 'cover' }), left: 800, top: 130 },
    { input: label('BRAND · TRANSPARENT PNG', 780), left: 40, top: 565 },
    { input: await image('Brand/T_FF_Title_CasualBright_Logo_512.png', 240, 240), left: 40, top: 615 },
    { input: await image('Brand/T_FF_Title_CasualBright_Wordmark_1024x512.png', 560, 280), left: 310, top: 595 },
    { input: label('TINTABLE ICONS · WHITE + ALPHA', 800), left: 980, top: 565 },
    { input: iconTile(), left: 1000, top: 630 },
    { input: iconTile(), left: 1200, top: 630 },
    { input: iconTile(), left: 1400, top: 630 },
    { input: iconTile(), left: 1600, top: 630 },
    { input: await image('Icons/T_FF_Title_CasualBright_Icon_Play_White_256.png', 120, 120), left: 1020, top: 650 },
    { input: await image('Icons/T_FF_Title_CasualBright_Icon_Settings_White_256.png', 120, 120), left: 1220, top: 650 },
    { input: await image('Icons/T_FF_Title_CasualBright_Icon_Power_White_256.png', 120, 120), left: 1420, top: 650 },
    { input: await image('Icons/T_FF_Title_CasualBright_Icon_Chevron_White_256.png', 120, 120), left: 1620, top: 650 },
    { input: label('BUTTON SKINS · NO BAKED TEXT', 1000), left: 40, top: 900 },
    { input: label('NORMAL', 300, 38, 20), left: 40, top: 943 },
    { input: label('FOCUSED', 300, 38, 20), left: 980, top: 943 },
    { input: label('PRESSED', 300, 38, 20), left: 40, top: 1163 },
    { input: label('DISABLED', 300, 38, 20), left: 980, top: 1163 },
    { input: await image('Buttons/T_FF_Title_CasualBright_Button_Normal_1024x256.png', 860, 215), left: 40, top: 985 },
    { input: await image('Buttons/T_FF_Title_CasualBright_Button_Focused_1024x256.png', 860, 215), left: 980, top: 985 },
    { input: await image('Buttons/T_FF_Title_CasualBright_Button_Pressed_1024x256.png', 860, 215), left: 40, top: 1205 },
    { input: await image('Buttons/T_FF_Title_CasualBright_Button_Disabled_1024x256.png', 860, 215), left: 980, top: 1205 }
  ];

  await sharp({ create: { width: 1920, height: 1440, channels: 4, background: '#EFF8F2' } })
    .composite(layers)
    .png()
    .toFile(output);
  console.log(output);
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
