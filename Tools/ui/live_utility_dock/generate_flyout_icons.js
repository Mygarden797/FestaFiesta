// SNS·음악 펼침 패널(2026-10-07 승인안)에 필요한 아이콘. 자체 제작.
// LightTeal 아이콘과 같은 규격(64 격자 ×4 = 256px, 흰 선 4.5, 둥근 끝)으로 그리고 색은 위젯에서 칠한다.
// 재생 조작은 시안처럼 속이 찬 모양(Fill)으로 따로 둔다. 기존 선형 Play/Pause/Next 는 다른 화면이 쓰므로 건드리지 않는다.
const fs = require('fs');
const path = require('path');
const os = require('os');
let sharp;
try { sharp = require('sharp'); }
catch (_) { sharp = require(path.join(os.homedir(), '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp')); }

const root = path.resolve(__dirname, '../../..');
const stroke = body => '<g transform="scale(4)" fill="none" stroke="#FFFFFF" stroke-width="4.5" stroke-linecap="round" stroke-linejoin="round">' + body + '</g>';
const solid = body => '<g transform="scale(4)" fill="#FFFFFF" stroke="#FFFFFF" stroke-width="3" stroke-linejoin="round">' + body + '</g>';
const repeatLoop = '<path d="M13 30V27C13 22.6 16.6 19 21 19H49"/><path d="M43 12.5L49.5 19L43 25.5"/>'
  + '<path d="M51 34V37C51 41.4 47.4 45 43 45H15"/><path d="M21 51.5L14.5 45L21 38.5"/>';

const icons = {
  // 교차 화살표
  Shuffle: stroke('<path d="M8 20H17C29 20 33 44 45 44H54"/><path d="M48 37.5L54.5 44L48 50.5"/>'
    + '<path d="M8 44H17C21.5 44 24.5 40.6 27 36.5"/><path d="M37 27.5C39.5 23.4 42.5 20 47 20H54"/><path d="M48 13.5L54.5 20L48 26.5"/>'),
  // 순환 화살표
  Repeat: stroke(repeatLoop),
  // 순환 화살표 + 안쪽 작은 1 (한 곡 반복)
  RepeatOne: stroke(repeatLoop + '<path d="M29.5 28.5L33 26V38" stroke-width="3.6"/>'),
  // 지금 곡 표시용 막대 세 개
  Equalizer: solid('<rect x="12" y="30" width="9" height="22" rx="2.5"/><rect x="27.5" y="14" width="9" height="38" rx="2.5"/><rect x="43" y="23" width="9" height="29" rx="2.5"/>'),
  PlayFill: solid('<path d="M21 13L51 32L21 51Z"/>'),
  PauseFill: solid('<rect x="16" y="13" width="11" height="38" rx="3"/><rect x="37" y="13" width="11" height="38" rx="3"/>'),
  NextFill: solid('<path d="M13 15L39 32L13 49Z"/><rect x="45" y="14" width="7" height="36" rx="2.5"/>'),
  PreviousFill: solid('<path d="M51 15L25 32L51 49Z"/><rect x="12" y="14" width="7" height="36" rx="2.5"/>'),
};

const vectorDir = path.join(root, 'SourceArt/UI/LiveHUD/LightTealParts_v1/VectorSource/Icons/Actions');
const pngDir = path.join(root, 'SourceArt/UI/LiveHUD/LightTealParts_v1/Icons/Actions');
fs.mkdirSync(vectorDir, { recursive: true });
fs.mkdirSync(pngDir, { recursive: true });
Promise.all(Object.entries(icons).map(([key, body]) => {
  const name = `T_FF_LH_Icon_${key}_White_256`;
  const svg = '<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256"><defs></defs>' + body + '</svg>';
  fs.writeFileSync(path.join(vectorDir, name + '.svg'), svg);
  return sharp(Buffer.from(svg)).png().toFile(path.join(pngDir, name + '.png'))
    .then(info => console.log('FF_FLYOUT_ICON', name, info.width + 'x' + info.height));
})).catch(error => { console.error('FF_FLYOUT_ICON_FAIL', error); process.exit(1); });
