const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const sharp = require('C:/Users/jt/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');

const projectRoot = path.resolve(__dirname, '../..');
const packRoot = path.join(projectRoot, 'SourceArt/UI/Title/CasualBrightParts_v1');

const required = {
  'Brand/T_FF_Title_CasualBright_Logo_512.png': [512, 512, true],
  'Brand/T_FF_Title_CasualBright_Wordmark_1024x512.png': [1024, 512, true],
  'Buttons/T_FF_Title_CasualBright_Button_Normal_1024x256.png': [1024, 256, true],
  'Buttons/T_FF_Title_CasualBright_Button_Focused_1024x256.png': [1024, 256, true],
  'Buttons/T_FF_Title_CasualBright_Button_Pressed_1024x256.png': [1024, 256, true],
  'Buttons/T_FF_Title_CasualBright_Button_Disabled_1024x256.png': [1024, 256, true],
  'Icons/T_FF_Title_CasualBright_Icon_Play_White_256.png': [256, 256, true],
  'Icons/T_FF_Title_CasualBright_Icon_Settings_White_256.png': [256, 256, true],
  'Icons/T_FF_Title_CasualBright_Icon_Power_White_256.png': [256, 256, true],
  'Icons/T_FF_Title_CasualBright_Icon_Chevron_White_256.png': [256, 256, true],
  'Background/T_FF_Title_CasualBright_Background_1920x1080.png': [1920, 1080, false],
  'Earth/T_FF_Earth_CasualBright_Albedo_2048x1024.png': [2048, 1024, false],
  'Earth/T_FF_Earth_CasualBright_Clouds_2048x1024.png': [2048, 1024, true]
};

function toPosix(value) {
  return value.split(path.sep).join('/');
}

function importProfile(relative) {
  if (relative.includes('/GenerationSource/')) return { import: false };
  if (relative.includes('_Clouds_')) {
    return {
      compression: 'Masks (no sRGB)', texture_group: 'World', srgb: false,
      mip_gen: 'FromTextureGroup', never_stream: false,
      address_x: 'Wrap', address_y: 'Clamp'
    };
  }
  if (relative.startsWith('Earth/')) {
    return {
      compression: 'Default', texture_group: 'World', srgb: true,
      mip_gen: 'FromTextureGroup', never_stream: false,
      address_x: 'Wrap', address_y: 'Clamp'
    };
  }
  if (relative.startsWith('Background/')) {
    return {
      compression: 'Default', texture_group: 'UI', srgb: true,
      mip_gen: 'FromTextureGroup', never_stream: false,
      address_x: 'Clamp', address_y: 'Clamp', brush_draw_as: 'Image'
    };
  }
  if (relative.startsWith('Preview/')) return { import: false };
  return {
    compression: 'UserInterface2D (RGBA)', texture_group: 'UI', srgb: true,
    mip_gen: 'NoMipmaps', never_stream: true,
    address_x: 'Clamp', address_y: 'Clamp', brush_draw_as: 'Image'
  };
}

async function walk(directory) {
  const entries = await fs.promises.readdir(directory, { withFileTypes: true });
  const files = [];
  for (const entry of entries) {
    const absolute = path.join(directory, entry.name);
    if (entry.isDirectory()) files.push(...await walk(absolute));
    else files.push(absolute);
  }
  return files;
}

async function alphaStats(file, metadata) {
  if (!metadata.hasAlpha) return { has_alpha: false, transparent_pixel_ratio: 0 };
  const { data, info } = await sharp(file).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  let transparent = 0;
  for (let index = 3; index < data.length; index += info.channels) {
    if (data[index] < 255) transparent++;
  }
  return {
    has_alpha: true,
    transparent_pixel_ratio: Number((transparent / (info.width * info.height)).toFixed(6))
  };
}

async function main() {
  const failures = [];
  for (const [relative, expected] of Object.entries(required)) {
    const file = path.join(packRoot, ...relative.split('/'));
    if (!fs.existsSync(file)) {
      failures.push(`필수 파일 없음: ${relative}`);
      continue;
    }
    const metadata = await sharp(file).metadata();
    if (metadata.width !== expected[0] || metadata.height !== expected[1]) {
      failures.push(`크기 오류: ${relative} (${metadata.width}x${metadata.height})`);
    }
    if (expected[2] && !metadata.hasAlpha) failures.push(`알파 채널 없음: ${relative}`);
  }

  const pngs = (await walk(packRoot))
    .filter(file => path.extname(file).toLowerCase() === '.png')
    .sort((a, b) => a.localeCompare(b));
  const assets = [];
  for (const file of pngs) {
    const relative = toPosix(path.relative(packRoot, file));
    const metadata = await sharp(file).metadata();
    const buffer = await fs.promises.readFile(file);
    assets.push({
      file: relative,
      width: metadata.width,
      height: metadata.height,
      format: metadata.format,
      color_space: metadata.space,
      ...await alphaStats(file, metadata),
      sha256: crypto.createHash('sha256').update(buffer).digest('hex').toUpperCase(),
      import_profile: importProfile(relative)
    });
  }

  const manifest = {
    schema_version: '1.0',
    pack: 'FestaFiesta Title Casual Bright Parts',
    version: '1.0.0',
    authored_date: '2026-08-27',
    status: 'assumption',
    reference_mockup: 'docs/ui/mockups/title-main-menu-casual-bright-globe-v1.png',
    rules: {
      wordmark_is_image: true,
      localized_button_labels_are_text_blocks: true,
      earth_is_existing_3d_sphere_with_new_albedo: true,
      color_only_focus_forbidden: true
    },
    assets
  };
  await fs.promises.writeFile(
    path.join(packRoot, 'asset-manifest.json'),
    `${JSON.stringify(manifest, null, 2)}\n`,
    'utf8'
  );

  if (failures.length) {
    for (const failure of failures) console.error(failure);
    process.exitCode = 1;
    return;
  }
  console.log(`검증 완료: 필수 ${Object.keys(required).length}개, 전체 PNG ${assets.length}개`);
  console.log(path.join(packRoot, 'asset-manifest.json'));
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
