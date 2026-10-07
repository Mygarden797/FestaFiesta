// LightTeal 내비게이션 아이콘과 같은 규격(64 격자 ×4, 흰 선 4.5)으로 음표 아이콘을 만든다.
// 폰트의 ♫ 글리프는 프로젝트 글꼴에 없어 대체 글꼴 상자로 그려졌다. 그래서 이미지로 바꾼다. 자체 제작.
const fs = require('fs');
const path = require('path');
const os = require('os');
let sharp;
try { sharp = require('sharp'); }
catch (_) { sharp = require(path.join(os.homedir(), '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp')); }

const root = path.resolve(__dirname, '../..');
const name = 'T_FF_LH_Icon_Music_White_256';
const svg = [
  '<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256"><defs></defs>',
  '<g transform="scale(4)" fill="none" stroke="#FFFFFF" stroke-width="4.5" stroke-linecap="round" stroke-linejoin="round">',
  // 두 기둥과 이어진 꼬리. 꼬리는 두 번 그어 8분음표 한 쌍(♫)으로 읽히게 한다.
  '<path d="M25 45V15L52 9V39"/><path d="M25 23L52 17"/>',
  '</g>',
  '<g transform="scale(4)" fill="#FFFFFF" stroke="none">',
  '<ellipse cx="18" cy="46" rx="8" ry="6" transform="rotate(-18 18 46)"/>',
  '<ellipse cx="45" cy="40" rx="8" ry="6" transform="rotate(-18 45 40)"/>',
  '</g></svg>',
].join('');

const vectorDir = path.join(root, 'SourceArt/UI/LiveHUD/LightTealParts_v1/VectorSource/Icons/Navigation');
const pngDir = path.join(root, 'SourceArt/UI/LiveHUD/LightTealParts_v1/Icons/Navigation');
fs.mkdirSync(vectorDir, { recursive: true });
fs.mkdirSync(pngDir, { recursive: true });
fs.writeFileSync(path.join(vectorDir, name + '.svg'), svg);
sharp(Buffer.from(svg)).png().toFile(path.join(pngDir, name + '.png'))
  .then(info => console.log('FF_MUSIC_ICON', name, info.width + 'x' + info.height))
  .catch(error => { console.error('FF_MUSIC_ICON_FAIL', error); process.exit(1); });
