const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const sharp = require('C:/Users/jt/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');

const projectRoot = path.resolve(__dirname, '../..');
const mockupRoot = path.join(projectRoot, 'docs/ui/mockups/live-hud-menu-states-v2-58px');
const sourceRoot = path.join(mockupRoot, 'source-legacy');
const themeRoot = path.join(projectRoot, 'SourceArt/UI/LiveHUD/LightTealParts_v1');

const WIDTH = 1920;
const HEIGHT = 1080;
const MENU_TOP = 1022;
const STATUS_TOP = 1051;
const ROW_HEIGHT = 29;
const MENU_CLEAN_TOP = 991;

const COLORS = {
  white: '#FFFFFF',
  selected: '#386F5A',
  selectedBorder: '#2E5D4B',
  ink: '#1F2925',
  railBorder: '#CFD8D2',
  status: '#E6EBE7'
};

const states = [
  { file: '01-operations.png', key: 'operations', cleanTop: 991, oldMenuTop: MENU_TOP },
  { file: '02-facilities.png', key: 'facilities', cleanTop: 991, oldMenuTop: MENU_TOP },
  { file: '03-guests.png', key: 'guests', cleanTop: 980, oldMenuTop: 1014 },
  { file: '04-queues.png', key: 'queues', cleanTop: 991, oldMenuTop: MENU_TOP },
  { file: '05-density.png', key: 'density', cleanTop: 970, oldMenuTop: 1005 },
  { file: '06-events.png', key: 'events', cleanTop: 991, oldMenuTop: MENU_TOP },
  { file: '07-sns.png', key: 'sns', cleanTop: 930, oldMenuTop: 977 },
  { file: '08-live-report.png', key: 'report', cleanTop: 991, oldMenuTop: MENU_TOP },
  { file: '09-staff.png', key: 'staff', cleanTop: 991, oldMenuTop: MENU_TOP }
];

const nav = [
  ['operations', '운영', 'Operations'],
  ['facilities', '시설', 'Facilities'],
  ['guests', '관객', 'Guests'],
  ['queues', '대기열', 'Queues'],
  ['density', '밀집도', 'Density'],
  ['events', '사건', 'Events'],
  ['sns', 'SNS', 'SNS'],
  ['report', '리포트', 'Report'],
  ['staff', '스태프', 'Staff']
];

function escapeXml(value) {
  return String(value)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function svg(width, height, body) {
  return Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">${body}</svg>`);
}

async function iconBuffer(iconName, tint) {
  const file = path.join(
    themeRoot,
    `Icons/Navigation/T_FF_LH_Icon_${iconName}_White_256.png`
  );
  return sharp(file)
    .resize(13, 13, { fit: 'contain', kernel: sharp.kernel.lanczos3 })
    .tint(tint)
    .png()
    .toBuffer();
}

async function makeMenuRail(selectedKey) {
  // Preserve the approved reference rail width. The interior uses compact,
  // fixed-width controls so icons and labels no longer look horizontally
  // scaled even though the white rail keeps the same silhouette.
  const railWidth = 1078;
  const railLeft = Math.round((WIDTH - railWidth) / 2);
  const paddingX = 65;
  const gap = 6;
  const buttonWidth = 100;
  const layers = [
    {
      input: svg(
        railWidth,
        ROW_HEIGHT,
        `<rect x="0.5" y="0.5" width="${railWidth - 1}" height="28" rx="4" fill="${COLORS.white}" stroke="${COLORS.railBorder}"/>`
      ),
      left: 0,
      top: 0
    }
  ];

  for (let index = 0; index < nav.length; index += 1) {
    const [key, label, iconName] = nav[index];
    const selected = key === selectedKey;
    const left = paddingX + index * (buttonWidth + gap);
    const foreground = selected ? COLORS.white : COLORS.ink;

    if (selected) {
      layers.push({
        input: svg(
          buttonWidth,
          25,
          `<rect x="0.5" y="0.5" width="99" height="24" rx="4" fill="${COLORS.selected}" stroke="${COLORS.selectedBorder}"/>`
        ),
        left,
        top: 2
      });
    }

    layers.push({
      input: await iconBuffer(iconName, foreground),
      left: left + 17,
      top: 7
    });

    layers.push({
      input: svg(
        62,
        ROW_HEIGHT,
        `<text x="5" y="18.5" fill="${foreground}" font-family="Malgun Gothic, sans-serif" font-size="11.5" font-weight="${selected ? 700 : 600}">${escapeXml(label)}</text>`
      ),
      left: left + 36,
      top: 0
    });
  }

  const rail = await sharp({
    create: {
      width: railWidth,
      height: ROW_HEIGHT,
      channels: 4,
      background: { r: 255, g: 255, b: 255, alpha: 0 }
    }
  })
    .composite(layers)
    .png()
    .toBuffer();

  return { rail, railLeft };
}

async function compressGroup(source, left, width, targetWidth) {
  const { data, info } = await sharp(source)
    .extract({ left, top: STATUS_TOP, width, height: ROW_HEIGHT })
    .resize(targetWidth, ROW_HEIGHT, {
      fit: 'fill',
      kernel: sharp.kernel.lanczos3
    })
    .ensureAlpha()
    .raw()
    .toBuffer({ resolveWithObject: true });

  // Remove only the pale status-bar background from the bitmap. The
  // foreground stays raster-identical to the approved reference, while the
  // common full-width base below prevents visible rectangular splice seams.
  for (let x = 0; x < info.width; x += 1) {
    const sampleRows = [0, 1, 2];
    const background = [0, 1, 2].map(channel => {
      const values = sampleRows
        .map(y => data[(y * info.width + x) * info.channels + channel])
        .sort((a, b) => a - b);
      return values[1];
    });

    for (let y = 0; y < info.height; y += 1) {
      const index = (y * info.width + x) * info.channels;
      const dr = data[index] - background[0];
      const dg = data[index + 1] - background[1];
      const db = data[index + 2] - background[2];
      const distance = Math.sqrt(dr * dr + dg * dg + db * db);
      data[index + 3] = Math.max(0, Math.min(255, Math.round((distance - 3) * 24)));
    }
  }

  return sharp(data, {
    raw: {
      width: info.width,
      height: info.height,
      channels: info.channels
    }
  })
    .png()
    .toBuffer();
}

async function makeStatusBar(referenceFile) {
  // Keep the approved bitmap styling. Only undo the horizontal distortion,
  // independently anchoring the left, centre and right information groups.
  const leftGroup = await compressGroup(referenceFile, 0, 500, 435);
  const centreGroup = await compressGroup(referenceFile, 820, 240, 209);
  const rightGroup = await compressGroup(referenceFile, 1260, 660, 575);

  return sharp({
    create: {
      width: WIDTH,
      height: ROW_HEIGHT,
      channels: 4,
      background: COLORS.status
    }
  })
    .composite([
      { input: leftGroup, left: 0, top: 0 },
      { input: centreGroup, left: Math.round((WIDTH - 209) / 2), top: 0 },
      { input: rightGroup, left: WIDTH - 575, top: 0 }
    ])
    .png()
    .toBuffer();
}

async function makeMenuBackground(sourceFile, cleanTop) {
  // Restore the world behind the legacy menu before drawing the corrected
  // compact rail. Sampling from a clean strip farther above avoids carrying
  // the old wide rail's antialiased top edge into the new composition.
  return sharp(sourceFile)
    .extract({ left: 0, top: cleanTop ?? MENU_CLEAN_TOP, width: WIDTH, height: ROW_HEIGHT })
    .png()
    .toBuffer();
}

async function makePreMenuRepair(sourceFile, cleanTop, oldMenuTop) {
  const repairHeight = MENU_TOP - oldMenuTop;
  if (repairHeight <= 0) return null;
  return sharp(sourceFile)
    .extract({ left: 0, top: cleanTop, width: WIDTH, height: repairHeight })
    .png()
    .toBuffer();
}

async function normalizeStates() {
  const referenceFile = path.join(sourceRoot, '01-operations.png');
  const statusBar = await makeStatusBar(referenceFile);

  for (const state of states) {
    const sourceFile = path.join(sourceRoot, state.file);
    const metadata = await sharp(sourceFile).metadata();
    if (metadata.width !== WIDTH || metadata.height !== HEIGHT) {
      throw new Error(`${state.file}: expected 1920x1080 source`);
    }

    const world = await sharp(sourceFile)
      .extract({ left: 0, top: 0, width: WIDTH, height: MENU_TOP })
      .png()
      .toBuffer();
    const menuBackground = await makeMenuBackground(sourceFile, state.cleanTop);
    const preMenuRepair = await makePreMenuRepair(
      sourceFile,
      state.cleanTop,
      state.oldMenuTop
    );
    const { rail, railLeft } = await makeMenuRail(state.key);

    const layers = [{ input: world, left: 0, top: 0 }];
    if (preMenuRepair) {
      layers.push({ input: preMenuRepair, left: 0, top: state.oldMenuTop });
    }
    layers.push(
      { input: menuBackground, left: 0, top: MENU_TOP },
      { input: rail, left: railLeft, top: MENU_TOP },
      { input: statusBar, left: 0, top: STATUS_TOP }
    );

    await sharp({
      create: {
        width: WIDTH,
        height: HEIGHT,
        channels: 4,
        background: '#000000'
      }
    })
      .composite(layers)
      .png()
      .toFile(path.join(mockupRoot, state.file));
  }
}

async function makeContactSheet() {
  const layers = [];
  for (let index = 0; index < states.length; index += 1) {
    const row = Math.floor(index / 3);
    const column = index % 3;
    const thumbnail = await sharp(path.join(mockupRoot, states[index].file))
      .resize(640, 360, { fit: 'fill' })
      .png()
      .toBuffer();
    layers.push({ input: thumbnail, left: column * 640, top: row * 360 });
  }

  await sharp({
    create: {
      width: WIDTH,
      height: HEIGHT,
      channels: 4,
      background: COLORS.status
    }
  })
    .composite(layers)
    .png()
    .toFile(path.join(mockupRoot, '00-menu-states-contact-sheet.png'));
}

async function verify() {
  const statusHashes = [];
  for (const state of states) {
    const file = path.join(mockupRoot, state.file);
    const metadata = await sharp(file).metadata();
    if (metadata.width !== WIDTH || metadata.height !== HEIGHT) {
      throw new Error(`${state.file}: invalid output dimensions`);
    }
    const status = await sharp(file)
      .extract({ left: 0, top: STATUS_TOP, width: WIDTH, height: ROW_HEIGHT })
      .raw()
      .toBuffer();
    statusHashes.push(crypto.createHash('sha256').update(status).digest('hex'));
  }
  if (new Set(statusHashes).size !== 1) {
    throw new Error('All nine status rows must be pixel-identical.');
  }
}

async function main() {
  if (!fs.existsSync(sourceRoot)) {
    throw new Error(`Missing preserved source folder: ${sourceRoot}`);
  }
  await normalizeStates();
  await makeContactSheet();
  await verify();
  console.log('Restored 9 mockups to the approved HUD design and corrected horizontal distortion.');
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
