const fs = require('fs');
const path = require('path');
const sharp = require('sharp');

const projectRoot = path.resolve(__dirname, '../..');
const outputRoot = path.join(
  projectRoot,
  'Design',
  'UXUI',
  'FestaFiesta_DesignSystem_v2',
  'ComponentKit'
);

const palette = {
  abyss: '#071314',
  navy: '#0B1D1E',
  panel: '#102627',
  panel2: '#173031',
  glassLine: '#335352',
  mint: '#57D4B8',
  mintBright: '#8BE9D4',
  mintDeep: '#238C79',
  ivory: '#F6F5EF',
  ink: '#18312E',
  muted: '#91AAA5',
  amber: '#F0B44D',
  coral: '#F16D5C',
  info: '#5BA6D9',
  safe: '#72C49A',
  transparent: 'none'
};

const records = [];

function esc(value) {
  return String(value)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function svg(w, h, body, defs = '') {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><defs>${defs}</defs>${body}</svg>`;
}

function rounded(x, y, w, h, r, fill, stroke = 'none', sw = 0, opacity = 1) {
  return `<rect x="${x + sw / 2}" y="${y + sw / 2}" width="${w - sw}" height="${h - sw}" rx="${r}" fill="${fill}" fill-opacity="${opacity}" stroke="${stroke}" stroke-width="${sw}"/>`;
}

function pill(x, y, w, h, fill, stroke = 'none', sw = 0, opacity = 1) {
  return rounded(x, y, w, h, h / 2, fill, stroke, sw, opacity);
}

const defs = `
  <linearGradient id="g-primary" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="${palette.mintBright}"/>
    <stop offset="1" stop-color="${palette.mintDeep}"/>
  </linearGradient>
  <linearGradient id="g-panel" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#193536" stop-opacity=".98"/>
    <stop offset="1" stop-color="#0B1C1D" stop-opacity=".98"/>
  </linearGradient>
  <linearGradient id="g-light" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#FFFFFF"/>
    <stop offset="1" stop-color="#EAEFEA"/>
  </linearGradient>
  <linearGradient id="g-danger" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="#FF8A74"/>
    <stop offset="1" stop-color="#C94C43"/>
  </linearGradient>
  <filter id="shadow" x="-40%" y="-60%" width="180%" height="240%">
    <feDropShadow dx="0" dy="14" stdDeviation="18" flood-color="#001010" flood-opacity=".48"/>
  </filter>
  <filter id="softShadow" x="-30%" y="-40%" width="160%" height="200%">
    <feDropShadow dx="0" dy="8" stdDeviation="10" flood-color="#001010" flood-opacity=".32"/>
  </filter>
  <filter id="glow" x="-50%" y="-80%" width="200%" height="260%">
    <feGaussianBlur stdDeviation="7" result="blur"/>
    <feMerge><feMergeNode in="blur"/><feMergeNode in="SourceGraphic"/></feMerge>
  </filter>
  <pattern id="grid" width="24" height="24" patternUnits="userSpaceOnUse">
    <path d="M24 0H0V24" fill="none" stroke="#65BDAA" stroke-width="1" stroke-opacity=".10"/>
  </pattern>
  <pattern id="hatch" width="16" height="16" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
    <line x1="0" y1="0" x2="0" y2="16" stroke="#FFFFFF" stroke-width="4" stroke-opacity=".28"/>
  </pattern>`;

async function writeAsset(category, name, w, h, body, meta = {}) {
  const categoryDir = path.join(outputRoot, category);
  const vectorDir = path.join(outputRoot, 'VectorSource', category);
  await fs.promises.mkdir(categoryDir, { recursive: true });
  await fs.promises.mkdir(vectorDir, { recursive: true });
  const source = svg(w, h, body, defs);
  const pngPath = path.join(categoryDir, `${name}.png`);
  const svgPath = path.join(vectorDir, `${name}.svg`);
  await fs.promises.writeFile(svgPath, source, 'utf8');
  await sharp(Buffer.from(source)).png().toFile(pngPath);
  records.push({
    file: path.relative(outputRoot, pngPath).replace(/\\/g, '/'),
    vector_source: path.relative(outputRoot, svgPath).replace(/\\/g, '/'),
    size: [w, h],
    type: meta.type || 'Image',
    draw_as: meta.drawAs || 'Image',
    margin_px: meta.margin || null,
    recommended_1080p: meta.target || null,
    state: meta.state || null,
    runtime_text: meta.runtimeText !== false,
    usage: meta.usage || ''
  });
}

function buttonSurface(kind, state) {
  const map = {
    Primary: {
      Default: ['url(#g-primary)', '#A1F2DE', 3, 1],
      Hover: ['#76E1C8', '#D1FFF2', 4, 1],
      Pressed: ['#2A9B85', '#76D8C0', 3, 1],
      Selected: ['#50CBB1', '#F6F5EF', 4, 1],
      Focus: ['url(#g-primary)', '#F6F5EF', 5, 1],
      Disabled: ['#36514F', '#4F6764', 2, .62]
    },
    Secondary: {
      Default: ['#102728', '#4E746F', 3, .98],
      Hover: ['#183B3A', '#72D9C2', 4, 1],
      Pressed: ['#0A1A1B', '#3CA88F', 3, 1],
      Selected: ['#214A46', '#7CE4CC', 4, 1],
      Focus: ['#173536', '#F6F5EF', 5, 1],
      Disabled: ['#162425', '#344342', 2, .55]
    },
    Ghost: {
      Default: ['#102627', '#335352', 2, .18],
      Hover: ['#2B4D4A', '#64C7B2', 3, .70],
      Pressed: ['#15312F', '#3CA68F', 3, .75],
      Selected: ['#183633', '#70D9C1', 4, .92],
      Focus: ['#1B3937', '#F6F5EF', 5, .88],
      Disabled: ['#102020', '#273938', 2, .28]
    },
    Danger: {
      Default: ['url(#g-danger)', '#FFAB9D', 3, 1],
      Hover: ['#FF816E', '#FFD1C8', 4, 1],
      Pressed: ['#B94039', '#F27D70', 3, 1],
      Selected: ['#D65449', '#FFF0EC', 4, 1],
      Focus: ['url(#g-danger)', '#F6F5EF', 5, 1],
      Disabled: ['#513B39', '#66504D', 2, .58]
    },
    Tab: {
      Default: ['#0A1A1B', '#294544', 2, .82],
      Hover: ['#18312F', '#4CA994', 3, .96],
      Pressed: ['#0A1718', '#2F8B78', 3, 1],
      Selected: ['#1C4B43', '#69D8BF', 3, 1],
      Focus: ['#1C4B43', '#F6F5EF', 5, 1],
      Disabled: ['#152323', '#2B3B3A', 2, .50]
    }
  };
  return map[kind][state];
}

async function makeButtons() {
  const families = ['Primary', 'Secondary', 'Ghost', 'Danger', 'Tab'];
  const states = ['Default', 'Hover', 'Pressed', 'Selected', 'Focus', 'Disabled'];
  for (const family of families) {
    for (const state of states) {
      const [fill, stroke, sw, opacity] = buttonSurface(family, state);
      const focusRing = state === 'Focus'
        ? rounded(6, 6, 500, 116, 24, 'none', palette.mintBright, 3, .95)
        : '';
      const inset = state === 'Pressed'
        ? `<path d="M28 28H484" stroke="#001010" stroke-width="3" stroke-opacity=".34" stroke-linecap="round"/>`
        : `<path d="M30 27H482" stroke="#FFFFFF" stroke-width="2" stroke-opacity=".14" stroke-linecap="round"/>`;
      const accent = family === 'Tab' && (state === 'Pressed' || state === 'Selected' || state === 'Focus')
        ? `<rect x="96" y="112" width="320" height="8" rx="4" fill="${palette.mintBright}"/>`
        : '';
      const selectedMark = state === 'Selected' && family !== 'Tab'
        ? `<circle cx="58" cy="64" r="18" fill="#071314" fill-opacity=".30" stroke="#F6F5EF" stroke-width="2"/><path d="M49 64L56 71L69 56" fill="none" stroke="#F6F5EF" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>`
        : '';
      await writeAsset(
        `Buttons/${family}`,
        `FF_DT_Button_${family}_${state}_512x128`,
        512,
        128,
        `<g filter="url(#softShadow)">${rounded(12, 12, 488, 104, 22, fill, stroke, sw, opacity)}${inset}${accent}${selectedMark}</g>${focusRing}`,
        {
          type: 'ButtonStyle',
          drawAs: 'Box',
          margin: 32,
          target: family === 'Tab' ? 'height 40-48 px' : 'height 44-56 px',
          state,
          usage: `${family} button ${state.toLowerCase()} state; overlay icon and label in UMG/CommonUI.`
        }
      );
    }
  }

  const circleStates = {
    Default: ['#102728', '#4D706D', 3],
    Hover: ['#1D4541', '#73DEC6', 4],
    Pressed: ['#0A1A1B', '#3CA68F', 3],
    Selected: ['#22534D', '#8BE9D4', 4],
    Focus: ['#1A3D3A', '#F6F5EF', 5],
    Disabled: ['#172525', '#354443', 2]
  };
  for (const [state, [fill, stroke, sw]] of Object.entries(circleStates)) {
    await writeAsset(
      'Buttons/IconCircle',
      `FF_DT_Button_IconCircle_${state}_256`,
      256,
      256,
      `<g filter="url(#softShadow)"><circle cx="128" cy="128" r="104" fill="${fill}" fill-opacity="${state === 'Disabled' ? '.55' : '.98'}" stroke="${stroke}" stroke-width="${sw}"/><circle cx="128" cy="128" r="87" fill="none" stroke="#FFFFFF" stroke-opacity=".08" stroke-width="2"/></g>`,
      {
        type: 'ButtonStyle',
        target: '44-52 px',
        state,
        usage: 'Circular icon button background. Place project icon in the center.'
      }
    );
  }
}

async function makeControls() {
  const toggleStates = {
    Off: ['#223737', '#61706D', 88, '#F6F5EF'],
    On: ['url(#g-primary)', '#9CF0DD', 276, '#F6F5EF'],
    Hover: ['#276056', '#7CE3CA', 276, '#F6F5EF'],
    Focus: ['#276056', '#F6F5EF', 276, '#F6F5EF'],
    Disabled: ['#2B3938', '#465352', 88, '#71807D']
  };
  for (const [state, [fill, stroke, knobX, knobFill]] of Object.entries(toggleStates)) {
    await writeAsset('Controls/Toggle', `FF_DT_Toggle_${state}_384x128`, 384, 128,
      `<g filter="url(#softShadow)">${pill(24, 24, 336, 80, fill, stroke, state === 'Focus' ? 5 : 3, state === 'Disabled' ? .65 : 1)}<circle cx="${knobX}" cy="64" r="30" fill="${knobFill}"/><circle cx="${knobX}" cy="64" r="22" fill="#FFFFFF" fill-opacity=".18"/></g>`,
      { type: 'Control', target: '52-64 x 24-32 px', state, usage: `Toggle ${state.toLowerCase()} state.` });
  }

  const checkStates = ['Unchecked', 'Checked', 'Indeterminate', 'Focus', 'Disabled'];
  for (const state of checkStates) {
    const active = state === 'Checked' || state === 'Focus';
    const fill = active ? 'url(#g-primary)' : '#102728';
    const stroke = state === 'Focus' ? '#F6F5EF' : state === 'Disabled' ? '#43514F' : active ? palette.mintBright : '#58716E';
    const mark = active
      ? `<path d="M54 98L84 126L142 64" fill="none" stroke="#071314" stroke-width="14" stroke-linecap="round" stroke-linejoin="round"/>`
      : state === 'Indeterminate'
        ? `<path d="M54 96H138" stroke="${palette.mintBright}" stroke-width="14" stroke-linecap="round"/>`
        : '';
    await writeAsset('Controls/Checkbox', `FF_DT_Checkbox_${state}_192`, 192, 192,
      `<g filter="url(#softShadow)">${rounded(28, 28, 136, 136, 26, fill, stroke, state === 'Focus' ? 8 : 5, state === 'Disabled' ? .55 : 1)}${mark}</g>`,
      { type: 'Control', target: '24-32 px', state, usage: `Checkbox ${state.toLowerCase()} state.` });
  }

  for (const state of ['Off', 'On', 'Focus', 'Disabled']) {
    const active = state === 'On' || state === 'Focus';
    await writeAsset('Controls/Radio', `FF_DT_Radio_${state}_192`, 192, 192,
      `<g filter="url(#softShadow)"><circle cx="96" cy="96" r="66" fill="#102728" fill-opacity="${state === 'Disabled' ? '.55' : '1'}" stroke="${state === 'Focus' ? '#F6F5EF' : active ? palette.mintBright : '#58716E'}" stroke-width="${state === 'Focus' ? 8 : 5}"/>${active ? `<circle cx="96" cy="96" r="34" fill="url(#g-primary)"/>` : ''}</g>`,
      { type: 'Control', target: '24-32 px', state, usage: `Radio ${state.toLowerCase()} state.` });
  }

  const chipStates = {
    Default: ['#102728', '#486360', 1],
    Hover: ['#183B38', '#64CBB4', 1],
    Selected: ['#22534D', '#8BE9D4', 1],
    Warning: ['#382F1F', palette.amber, 1],
    Disabled: ['#1A2626', '#384644', .5]
  };
  for (const [state, [fill, stroke, opacity]] of Object.entries(chipStates)) {
    const mark = state === 'Selected'
      ? `<circle cx="58" cy="48" r="14" fill="${palette.mintBright}"/><path d="M51 48L56 53L65 42" fill="none" stroke="#071314" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>`
      : '';
    await writeAsset('Controls/Chip', `FF_DT_Chip_${state}_384x96`, 384, 96,
      `<g filter="url(#softShadow)">${pill(10, 10, 364, 76, fill, stroke, 3, opacity)}${mark}</g>`,
      { type: 'Control', drawAs: 'Box', margin: 30, target: 'height 28-36 px', state, usage: `Filter/status chip ${state.toLowerCase()} state; runtime text overlay.` });
  }

  const inputStates = {
    Default: ['#0C1C1D', '#45615E', 3, palette.muted],
    Focus: ['#102827', palette.mintBright, 5, palette.ivory],
    Error: ['#251B1A', palette.coral, 4, palette.coral],
    Disabled: ['#182323', '#374441', 2, '#576562']
  };
  for (const [state, [fill, stroke, sw, cursor]] of Object.entries(inputStates)) {
    await writeAsset('Controls/Input', `FF_DT_Input_${state}_768x128`, 768, 128,
      `<g filter="url(#softShadow)">${rounded(12, 12, 744, 104, 22, fill, stroke, sw, state === 'Disabled' ? .64 : 1)}<circle cx="64" cy="64" r="20" fill="none" stroke="${cursor}" stroke-width="4"/><path d="M78 78L92 92" stroke="${cursor}" stroke-width="4" stroke-linecap="round"/>${state === 'Focus' ? `<rect x="116" y="42" width="3" height="44" rx="1.5" fill="${palette.mintBright}"/>` : ''}</g>`,
      { type: 'Control', drawAs: 'Box', margin: 30, target: 'height 44-52 px', state, usage: `Search/text input ${state.toLowerCase()} state; runtime hint text overlay.` });
  }

  for (const state of ['Default', 'Hover', 'Focus', 'Disabled']) {
    const disabled = state === 'Disabled';
    const focus = state === 'Focus';
    const hover = state === 'Hover';
    await writeAsset('Controls/Slider', `FF_DT_Slider_${state}_1024x128`, 1024, 128,
      `${pill(28, 55, 968, 18, '#344D4A', 'none', 0, disabled ? .48 : 1)}${pill(28, 55, 612, 18, disabled ? '#5A6966' : 'url(#g-primary)')}<circle cx="640" cy="64" r="${hover || focus ? 28 : 23}" fill="${disabled ? '#71807D' : palette.ivory}" stroke="${focus ? '#F6F5EF' : palette.mint}" stroke-width="${focus ? 7 : 5}" filter="url(#softShadow)"/>`,
      { type: 'Control', drawAs: 'Image', target: 'width variable, height 24-36 px', state, usage: `Slider ${state.toLowerCase()} state.` });
  }

  for (const selected of [0, 1, 2]) {
    const x = 12 + selected * 248;
    await writeAsset('Controls/Segmented', `FF_DT_Segmented_Selected${selected + 1}_768x128`, 768, 128,
      `<g filter="url(#softShadow)">${rounded(12, 12, 744, 104, 22, '#0C1C1D', '#45615E', 3)}${rounded(x, 12, 248, 104, 22, '#22534D', palette.mintBright, 3)}<path d="M260 28V100M508 28V100" stroke="#45615E" stroke-width="2"/></g>`,
      { type: 'Control', drawAs: 'Image', target: 'width 300-480 px, height 44-52 px', state: `Selected${selected + 1}`, usage: 'Three-way segmented control background; runtime labels overlay.' });
  }
}

function linePlaceholder(x, y, w, h = 12, opacity = .45, fill = palette.muted) {
  return pill(x, y, w, h, fill, 'none', 0, opacity);
}

function eyeIcon(x, y, color = palette.mintBright) {
  return `<path d="M${x} ${y + 12}C${x + 11} ${y - 2} ${x + 29} ${y - 2} ${x + 40} ${y + 12}C${x + 29} ${y + 26} ${x + 11} ${y + 26} ${x} ${y + 12}Z" fill="none" stroke="${color}" stroke-width="3"/><circle cx="${x + 20}" cy="${y + 12}" r="6" fill="${color}"/>`;
}

function warningIcon(x, y, color = palette.amber) {
  return `<path d="M${x + 20} ${y}L${x + 40} ${y + 36}H${x}Z" fill="${color}" fill-opacity=".16" stroke="${color}" stroke-width="3"/><path d="M${x + 20} ${y + 10}V${y + 23}" stroke="${color}" stroke-width="4" stroke-linecap="round"/><circle cx="${x + 20}" cy="${y + 29}" r="2.5" fill="${color}"/>`;
}

async function makeWidgets() {
  await writeAsset('Widgets', 'FF_DT_Widget_Panel_Dark_512_9Slice', 512, 512,
    `<g filter="url(#shadow)">${rounded(18, 18, 476, 468, 28, 'url(#g-panel)', palette.glassLine, 3, .98)}<rect x="38" y="38" width="436" height="428" rx="20" fill="url(#grid)"/></g>`,
    { type: 'Panel', drawAs: 'Box', margin: 48, target: 'variable', usage: 'Primary dark glass workbench panel.' });

  await writeAsset('Widgets', 'FF_DT_Widget_Panel_Light_512_9Slice', 512, 512,
    `<g filter="url(#shadow)">${rounded(18, 18, 476, 468, 28, 'url(#g-light)', '#C8D5D0', 3, .99)}<path d="M40 56H472" stroke="#FFFFFF" stroke-width="3" stroke-opacity=".85"/></g>`,
    { type: 'Panel', drawAs: 'Box', margin: 48, target: 'variable', usage: 'Ivory data panel for reports, settings, and planning.' });

  await writeAsset('Widgets', 'FF_DT_Widget_KPI_Card_640x240', 640, 240,
    `<g filter="url(#shadow)">${rounded(12, 12, 616, 216, 28, 'url(#g-panel)', palette.glassLine, 3, .98)}<rect x="12" y="12" width="12" height="216" rx="6" fill="${palette.mint}"/><circle cx="82" cy="78" r="34" fill="${palette.mint}" fill-opacity=".14" stroke="${palette.mint}" stroke-width="3"/>${linePlaceholder(140, 48, 206, 14, .55)}${linePlaceholder(140, 80, 326, 34, .93, palette.ivory)}${linePlaceholder(48, 158, 496, 18, .18, palette.ivory)}${pill(48, 158, 356, 18, 'url(#g-primary)')}<circle cx="566" cy="72" r="9" fill="${palette.safe}" filter="url(#glow)"/></g>`,
    { type: 'Widget', target: '280-360 x 104-132 px', usage: 'Live metric card; all textual content is a UMG overlay.' });

  await writeAsset('Widgets', 'FF_DT_Widget_AssetStatus_Card_960x320', 960, 320,
    `<g filter="url(#shadow)">${rounded(12, 12, 936, 296, 28, 'url(#g-panel)', palette.glassLine, 3, .98)}<rect x="12" y="12" width="12" height="296" rx="6" fill="${palette.mint}"/><rect x="48" y="50" width="204" height="204" rx="22" fill="#0A1819" stroke="#34504F" stroke-width="3"/><path d="M82 205L130 124L169 169L204 97L232 205Z" fill="${palette.mint}" fill-opacity=".18" stroke="${palette.mint}" stroke-width="3"/><circle cx="204" cy="97" r="10" fill="${palette.amber}"/>${linePlaceholder(292, 48, 340, 18, .82, palette.ivory)}${linePlaceholder(292, 84, 208, 12, .45)}${pill(744, 42, 148, 44, palette.safe, palette.safe, 0, .20)}${linePlaceholder(768, 58, 100, 10, .78, palette.safe)}${linePlaceholder(292, 140, 560, 12, .23, palette.ivory)}${linePlaceholder(292, 174, 492, 12, .23, palette.ivory)}${linePlaceholder(292, 208, 530, 12, .23, palette.ivory)}${pill(292, 250, 164, 38, '#21413E', '#5ECAB2', 2)}${pill(474, 250, 164, 38, '#142B2C', '#3E5A58', 2)}</g>`,
    { type: 'Widget', target: '420-560 x 140-190 px', usage: 'Facility asset list/detail summary card.' });

  await writeAsset('Widgets', 'FF_DT_Widget_LayerRow_960x128', 960, 128,
    `<g filter="url(#softShadow)">${rounded(10, 10, 940, 108, 20, '#112829', palette.glassLine, 3, .98)}<circle cx="56" cy="64" r="24" fill="#1B3B39" stroke="#4A7770" stroke-width="2"/>${eyeIcon(38, 52)}${linePlaceholder(104, 40, 268, 16, .72, palette.ivory)}${linePlaceholder(104, 72, 180, 10, .38)}${linePlaceholder(432, 58, 324, 12, .15, palette.ivory)}${pill(432, 58, 224, 12, palette.mint)}<circle cx="656" cy="64" r="16" fill="${palette.ivory}" stroke="${palette.mint}" stroke-width="4"/><circle cx="842" cy="64" r="24" fill="#183332" stroke="#4B6A67" stroke-width="2"/><path d="M832 64H852M842 54V74" stroke="${palette.mintBright}" stroke-width="3" stroke-linecap="round"/></g>`,
    { type: 'Widget', target: '320-480 x 44-64 px', usage: 'Layer visibility, opacity, and expand row.' });

  await writeAsset('Widgets', 'FF_DT_Widget_TimelineController_1536x176', 1536, 176,
    `<g filter="url(#shadow)">${rounded(12, 12, 1512, 152, 30, 'url(#g-panel)', palette.glassLine, 3, .98)}<circle cx="72" cy="88" r="28" fill="#193C38" stroke="${palette.mint}" stroke-width="3"/><path d="M64 71L88 88L64 105Z" fill="${palette.ivory}"/>${pill(136, 82, 1190, 12, '#35514F')}<rect x="136" y="82" width="760" height="12" rx="6" fill="url(#g-primary)"/><circle cx="896" cy="88" r="18" fill="${palette.ivory}" stroke="${palette.mint}" stroke-width="5"/><circle cx="402" cy="88" r="7" fill="${palette.ivory}"/><circle cx="1154" cy="88" r="7" fill="${palette.amber}"/><path d="M402 56V120M1154 56V120" stroke="#BBD0CB" stroke-width="2" stroke-opacity=".32" stroke-dasharray="5 5"/>${pill(1364, 54, 116, 68, '#163331', '#47736C', 3)}<path d="M1392 88H1452M1422 68V108" stroke="${palette.mintBright}" stroke-width="4" stroke-linecap="round"/></g>`,
    { type: 'Widget', target: '960-1536 x 80-96 px', usage: 'Simulation time scrubber with event markers and speed control.' });

  await writeAsset('Widgets', 'FF_DT_Widget_DensityLegend_480x920', 480, 920,
    `<g filter="url(#shadow)">${rounded(14, 14, 452, 892, 30, 'url(#g-panel)', palette.glassLine, 3, .98)}${linePlaceholder(52, 52, 252, 22, .82, palette.ivory)}${linePlaceholder(52, 92, 168, 11, .36)}
      ${[
        [148, palette.safe, .14],
        [252, '#91C878', .20],
        [356, '#D0CA63', .25],
        [460, palette.amber, .28],
        [564, '#E98852', .32],
        [668, palette.coral, .36]
      ].map(([y, color, opacity], index) => `<rect x="52" y="${y}" width="74" height="74" rx="15" fill="${color}" fill-opacity="${opacity}" stroke="${color}" stroke-width="3"/>${index >= 3 ? `<rect x="52" y="${y}" width="74" height="74" rx="15" fill="url(#hatch)"/>` : ''}${linePlaceholder(158, y + 8, 98, 17, .72, palette.ivory)}${linePlaceholder(158, y + 40, 208, 12, .36)}`).join('')}
      ${pill(52, 806, 376, 56, '#163332', '#416763', 2)}${linePlaceholder(86, 827, 308, 13, .48, palette.mintBright)}</g>`,
    { type: 'Widget', target: '200-260 x 390-500 px', usage: 'Six-step crowd-density legend with redundant patterns.' });

  await writeAsset('Widgets', 'FF_DT_Widget_ScenarioCompare_1120x520', 1120, 520,
    `<g filter="url(#shadow)">${rounded(14, 14, 1092, 492, 32, 'url(#g-panel)', palette.glassLine, 3, .98)}${linePlaceholder(54, 48, 350, 22, .78, palette.ivory)}${linePlaceholder(54, 88, 224, 11, .38)}
    <rect x="54" y="138" width="478" height="288" rx="24" fill="#0A191A" stroke="#3E5A58" stroke-width="3"/><rect x="588" y="138" width="478" height="288" rx="24" fill="#0A191A" stroke="${palette.mint}" stroke-width="4"/>
    <path d="M92 348C162 286 246 326 310 238S430 246 492 178" fill="none" stroke="#58726F" stroke-width="7" stroke-linecap="round"/><path d="M626 348C696 286 780 326 844 238S964 246 1026 178" fill="none" stroke="url(#g-primary)" stroke-width="8" stroke-linecap="round"/>
    ${[0,1,2,3].map(i => `<circle cx="${128 + i * 110}" cy="${324 - i * 40}" r="11" fill="${i === 3 ? palette.amber : palette.muted}"/><circle cx="${662 + i * 110}" cy="${324 - i * 40}" r="11" fill="${i === 3 ? palette.amber : palette.mint}"/>`).join('')}
    ${pill(426, 454, 264, 38, '#163B37', palette.mint, 2)}${linePlaceholder(472, 468, 172, 10, .72, palette.mintBright)}</g>`,
    { type: 'Widget', target: '520-720 x 240-340 px', usage: 'Before/after or scenario A/B comparison card.' });

  await writeAsset('Widgets', 'FF_DT_Widget_Toast_Warning_1024x224', 1024, 224,
    `<g filter="url(#shadow)">${rounded(12, 12, 1000, 200, 26, '#142829', '#735A2F', 3, .99)}<rect x="12" y="12" width="14" height="200" rx="7" fill="${palette.amber}"/>${warningIcon(58, 72)}${linePlaceholder(132, 50, 430, 20, .84, palette.ivory)}${linePlaceholder(132, 92, 650, 13, .42)}${linePlaceholder(132, 126, 520, 13, .32)}${pill(826, 70, 142, 64, '#2B352D', palette.amber, 2)}<path d="M874 102H922" stroke="${palette.amber}" stroke-width="4" stroke-linecap="round"/></g>`,
    { type: 'Widget', target: '420-580 x 92-126 px', usage: 'Actionable warning toast.' });

  await writeAsset('Widgets', 'FF_DT_Widget_Toast_Danger_1024x224', 1024, 224,
    `<g filter="url(#shadow)">${rounded(12, 12, 1000, 200, 26, '#2A1E1D', '#7D403A', 3, .99)}<rect x="12" y="12" width="14" height="200" rx="7" fill="${palette.coral}"/><circle cx="78" cy="92" r="28" fill="${palette.coral}" fill-opacity=".18" stroke="${palette.coral}" stroke-width="3"/><path d="M66 80L90 104M90 80L66 104" stroke="${palette.coral}" stroke-width="5" stroke-linecap="round"/>${linePlaceholder(132, 50, 430, 20, .84, palette.ivory)}${linePlaceholder(132, 92, 650, 13, .42)}${linePlaceholder(132, 126, 520, 13, .32)}${pill(826, 70, 142, 64, '#392321', palette.coral, 2)}<path d="M874 102H922" stroke="${palette.coral}" stroke-width="4" stroke-linecap="round"/></g>`,
    { type: 'Widget', target: '420-580 x 92-126 px', usage: 'Blocking error or safety-alert toast.' });

  await writeAsset('Widgets', 'FF_DT_Widget_Tooltip_720x224', 720, 224,
    `<g filter="url(#shadow)">${rounded(12, 12, 696, 184, 26, '#071314', '#4C706C', 3, .98)}<path d="M330 196L360 222L390 196Z" fill="#071314" stroke="#4C706C" stroke-width="3" stroke-linejoin="round"/>${linePlaceholder(50, 48, 270, 19, .82, palette.ivory)}${linePlaceholder(50, 90, 550, 13, .40)}${linePlaceholder(50, 122, 460, 13, .32)}${pill(560, 40, 112, 42, '#143632', '#55B9A3', 2)}</g>`,
    { type: 'Widget', target: '280-420 x 92-132 px', usage: 'World-pin or control tooltip background.' });

  await writeAsset('Widgets', 'FF_DT_Widget_InspectorPanel_720x1180', 720, 1180,
    `<g filter="url(#shadow)">${rounded(14, 14, 692, 1152, 30, 'url(#g-panel)', palette.glassLine, 3, .99)}<rect x="14" y="14" width="692" height="118" rx="30" fill="#173837"/><path d="M14 104H706" stroke="#466A66" stroke-width="2"/>${linePlaceholder(54, 48, 330, 22, .86, palette.ivory)}${pill(544, 42, 116, 44, palette.safe, palette.safe, 0, .18)}<rect x="54" y="164" width="612" height="280" rx="24" fill="#091819" stroke="#385654" stroke-width="3"/><path d="M108 390L210 264L290 334L398 208L502 286L612 190" fill="none" stroke="url(#g-primary)" stroke-width="8" stroke-linecap="round"/>
      ${[500,570,640,710,780].map((y, i) => `${linePlaceholder(54, y, 210 + i * 28, 15, .58, palette.ivory)}${linePlaceholder(404, y, 222 - i * 14, 15, .34)}`).join('')}
      <path d="M54 842H666" stroke="#3B5553" stroke-width="2"/>
      ${[890,966,1042].map((y, i) => `<circle cx="76" cy="${y}" r="16" fill="${i === 0 ? palette.amber : palette.mint}" fill-opacity=".18" stroke="${i === 0 ? palette.amber : palette.mint}" stroke-width="3"/>${linePlaceholder(112, y - 12, 330, 14, .62, palette.ivory)}${linePlaceholder(112, y + 15, 420, 10, .30)}`).join('')}
    </g>`,
    { type: 'Widget', target: '340-440 x viewport-height', usage: 'Selected facility inspector with preview, properties, and history.' });

  await writeAsset('Widgets', 'FF_DT_Widget_Modal_1280x800', 1280, 800,
    `<g filter="url(#shadow)">${rounded(18, 18, 1244, 764, 34, 'url(#g-panel)', '#4B6E69', 4, .995)}<rect x="18" y="18" width="1244" height="112" rx="34" fill="#183A38"/><path d="M18 116H1262" stroke="#517771" stroke-width="2"/>${linePlaceholder(64, 56, 430, 26, .86, palette.ivory)}<circle cx="1194" cy="72" r="26" fill="#102829" stroke="#476B66" stroke-width="2"/><path d="M1184 62L1204 82M1204 62L1184 82" stroke="${palette.ivory}" stroke-width="4" stroke-linecap="round"/><circle cx="134" cy="270" r="76" fill="${palette.mint}" fill-opacity=".14" stroke="${palette.mint}" stroke-width="4"/><path d="M104 272L126 294L168 246" fill="none" stroke="${palette.mintBright}" stroke-width="9" stroke-linecap="round" stroke-linejoin="round"/>${linePlaceholder(250, 204, 690, 28, .84, palette.ivory)}${linePlaceholder(250, 258, 820, 15, .40)}${linePlaceholder(250, 298, 724, 15, .32)}<rect x="64" y="410" width="1152" height="2" fill="#3C5754"/>${linePlaceholder(64, 466, 426, 16, .48, palette.ivory)}${linePlaceholder(64, 510, 920, 14, .28)}${linePlaceholder(64, 548, 812, 14, .22)}${pill(744, 674, 194, 72, '#112728', '#486B66', 3)}${pill(960, 674, 256, 72, 'url(#g-primary)', '#A0F0DD', 3)}</g>`,
    { type: 'Widget', target: '640-880 x 400-560 px', usage: 'Confirmation/detail modal with secondary and primary actions.' });
}

async function makeStatusIcons() {
  const iconDefs = [
    ['Layers', `<path d="M32 10L56 22L32 34L8 22Z M8 32L32 44L56 32 M8 42L32 54L56 42"/>`],
    ['Pin', `<path d="M32 58S52 40 52 24A20 20 0 1 0 12 24C12 40 32 58 32 58Z"/><circle cx="32" cy="24" r="7"/>`],
    ['Clock', `<circle cx="32" cy="32" r="24"/><path d="M32 17V34L44 41"/>`],
    ['Asset', `<rect x="11" y="12" width="42" height="40" rx="6"/><path d="M11 26H53M22 12V52M36 35H46"/>`],
    ['Filter', `<path d="M8 12H56L40 31V50L25 57V31Z"/>`],
    ['Compare', `<path d="M9 18H47M39 10L47 18L39 26M55 46H17M25 38L17 46L25 54"/>`],
    ['Warning', `<path d="M32 7L58 55H6Z"/><path d="M32 23V39M32 47H32.1"/>`],
    ['Search', `<circle cx="27" cy="27" r="17"/><path d="M40 40L56 56"/>`],
    ['Eye', `<path d="M6 32C20 14 44 14 58 32C44 50 20 50 6 32Z"/><circle cx="32" cy="32" r="8"/>`],
    ['Timeline', `<path d="M8 32H56M14 22V42M32 16V48M50 22V42"/>`],
    ['Route', `<circle cx="12" cy="48" r="6"/><circle cx="52" cy="16" r="6"/><path d="M18 46C31 44 27 20 46 18"/>`],
    ['Sensor', `<circle cx="32" cy="32" r="7"/><path d="M18 18C10 26 10 38 18 46M46 18C54 26 54 38 46 46M11 11C-1 23-1 41 11 53M53 11C65 23 65 41 53 53"/>`]
  ];
  for (const [name, body] of iconDefs) {
    await writeAsset('Icons', `FF_DT_Icon_${name}_256`, 256, 256,
      `<g transform="scale(4)" fill="none" stroke="#FFFFFF" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">${body}</g>`,
      { type: 'Icon', target: '18-28 px', usage: `${name} icon. Tint in UMG.` });
  }
}

function textLabel(x, y, value, size = 24, color = '#DCE9E5', weight = 600, anchor = 'start') {
  return `<text x="${x}" y="${y}" fill="${color}" font-family="Segoe UI, Arial, sans-serif" font-size="${size}" font-weight="${weight}" text-anchor="${anchor}">${esc(value)}</text>`;
}

async function makeContactSheets() {
  const buttonFiles = records.filter(item => item.type === 'ButtonStyle');
  const buttonItems = [];
  const families = ['Primary', 'Secondary', 'Ghost', 'Danger', 'Tab', 'IconCircle'];
  const states = ['Default', 'Hover', 'Pressed', 'Selected', 'Focus', 'Disabled'];
  for (let row = 0; row < families.length; row++) {
    const family = families[row];
    buttonItems.push(textLabel(56, 164 + row * 142, family, 28, '#F6F5EF', 700));
    for (let col = 0; col < states.length; col++) {
      const state = states[col];
      const match = buttonFiles.find(item => item.file.includes(`_${family}_${state}`));
      if (!match) continue;
      const href = `data:image/png;base64,${(await fs.promises.readFile(path.join(outputRoot, match.file))).toString('base64')}`;
      const x = 278 + col * 460;
      const y = 116 + row * 142;
      if (family === 'IconCircle') {
        buttonItems.push(`<image href="${href}" x="${x + 148}" y="${y}" width="96" height="96"/>`);
      } else {
        buttonItems.push(`<image href="${href}" x="${x}" y="${y}" width="390" height="98" preserveAspectRatio="none"/>`);
        buttonItems.push(textLabel(x + 195, y + 60, 'Label', 22, family === 'Primary' || family === 'Danger' ? '#071314' : '#F6F5EF', 700, 'middle'));
      }
      if (row === 0) buttonItems.push(textLabel(x + 195, 94, state, 20, '#91AAA5', 600, 'middle'));
    }
  }
  const buttonSheet = svg(3200, 1080,
    `<rect width="3200" height="1080" fill="#061112"/><rect x="24" y="24" width="3152" height="1032" rx="30" fill="#0A1819" stroke="#2E4A48" stroke-width="2"/>${textLabel(56, 56, 'FESTA FIESTA · DIGITAL TWIN BUTTON STATES', 28, '#8BE9D4', 800)}${buttonItems.join('')}`,
    defs
  );
  await sharp(Buffer.from(buttonSheet)).png().toFile(path.join(outputRoot, 'buttons-contact-sheet.png'));

  const widgetFiles = records.filter(item => item.type === 'Widget');
  const layout = [
    ['KPI_Card', 44, 116, 480, 180],
    ['AssetStatus_Card', 568, 116, 660, 220],
    ['LayerRow', 1272, 116, 640, 86],
    ['TimelineController', 44, 388, 1000, 114],
    ['DensityLegend', 1084, 264, 260, 500],
    ['ScenarioCompare', 1386, 258, 700, 326],
    ['Toast_Warning', 44, 574, 660, 144],
    ['Toast_Danger', 44, 758, 660, 144],
    ['Tooltip', 744, 594, 420, 132],
    ['Modal', 1386, 632, 600, 375]
  ];
  const widgetItems = [];
  for (const [token, x, y, w, h] of layout) {
    const match = widgetFiles.find(item => item.file.includes(token));
    if (!match) continue;
    const href = `data:image/png;base64,${(await fs.promises.readFile(path.join(outputRoot, match.file))).toString('base64')}`;
    widgetItems.push(textLabel(x, y - 16, token.replace(/_/g, ' '), 20, '#91AAA5', 650));
    widgetItems.push(`<image href="${href}" x="${x}" y="${y}" width="${w}" height="${h}" preserveAspectRatio="none"/>`);
  }
  const widgetSheet = svg(2160, 1080,
    `<rect width="2160" height="1080" fill="#061112"/><rect x="24" y="24" width="2112" height="1032" rx="30" fill="#0A1819" stroke="#2E4A48" stroke-width="2"/>${textLabel(44, 72, 'FESTA FIESTA · DIGITAL TWIN WIDGET KIT', 28, '#8BE9D4', 800)}${widgetItems.join('')}`,
    defs
  );
  await sharp(Buffer.from(widgetSheet)).png().toFile(path.join(outputRoot, 'widgets-contact-sheet.png'));

  const controlFiles = records.filter(item => item.type === 'Control');
  const controlLayout = [
    ['Toggle_Off', 56, 146, 300, 100, 'Off'], ['Toggle_On', 392, 146, 300, 100, 'On'], ['Toggle_Focus', 728, 146, 300, 100, 'Focus'], ['Toggle_Disabled', 1064, 146, 300, 100, 'Disabled'],
    ['Checkbox_Unchecked', 56, 350, 112, 112, 'Unchecked'], ['Checkbox_Checked', 204, 350, 112, 112, 'Checked'], ['Checkbox_Indeterminate', 352, 350, 112, 112, 'Mixed'], ['Checkbox_Focus', 500, 350, 112, 112, 'Focus'], ['Checkbox_Disabled', 648, 350, 112, 112, 'Disabled'],
    ['Radio_Off', 824, 350, 112, 112, 'Off'], ['Radio_On', 972, 350, 112, 112, 'On'], ['Radio_Focus', 1120, 350, 112, 112, 'Focus'], ['Radio_Disabled', 1268, 350, 112, 112, 'Disabled'],
    ['Chip_Default', 56, 550, 280, 70, 'Default'], ['Chip_Selected', 366, 550, 280, 70, 'Selected'], ['Chip_Warning', 676, 550, 280, 70, 'Warning'], ['Chip_Disabled', 986, 550, 280, 70, 'Disabled'],
    ['Input_Default', 1450, 146, 700, 116, 'Default'], ['Input_Focus', 1450, 304, 700, 116, 'Focus'], ['Input_Error', 1450, 462, 700, 116, 'Error'], ['Input_Disabled', 1450, 620, 700, 116, 'Disabled'],
    ['Slider_Default', 56, 728, 900, 112, 'Default'], ['Slider_Focus', 56, 876, 900, 112, 'Focus'],
    ['Segmented_Selected2', 1120, 862, 720, 120, 'Second selected']
  ];
  const controlItems = [];
  controlItems.push(textLabel(56, 116, 'TOGGLES', 18, '#8BE9D4', 750));
  controlItems.push(textLabel(56, 316, 'CHECKBOX', 18, '#8BE9D4', 750));
  controlItems.push(textLabel(824, 316, 'RADIO', 18, '#8BE9D4', 750));
  controlItems.push(textLabel(56, 516, 'CHIPS', 18, '#8BE9D4', 750));
  controlItems.push(textLabel(1450, 116, 'INPUTS', 18, '#8BE9D4', 750));
  controlItems.push(textLabel(56, 694, 'SLIDERS', 18, '#8BE9D4', 750));
  controlItems.push(textLabel(1120, 828, 'SEGMENTED CONTROL', 18, '#8BE9D4', 750));
  for (const [token, x, y, w, h, label] of controlLayout) {
    const match = controlFiles.find(item => item.file.includes(token));
    if (!match) continue;
    const href = `data:image/png;base64,${(await fs.promises.readFile(path.join(outputRoot, match.file))).toString('base64')}`;
    controlItems.push(textLabel(x + w / 2, y + h + 24, label, 14, '#91AAA5', 650, 'middle'));
    controlItems.push(`<image href="${href}" x="${x}" y="${y}" width="${w}" height="${h}" preserveAspectRatio="none"/>`);
  }
  const controlSheet = svg(2240, 1080,
    `<rect width="2240" height="1080" fill="#061112"/><rect x="24" y="24" width="2192" height="1032" rx="30" fill="#0A1819" stroke="#2E4A48" stroke-width="2"/>${textLabel(56, 70, 'FESTA FIESTA · DIGITAL TWIN CONTROLS', 28, '#8BE9D4', 800)}${controlItems.join('')}`,
    defs
  );
  await sharp(Buffer.from(controlSheet)).png().toFile(path.join(outputRoot, 'controls-contact-sheet.png'));
}

async function writeManifest() {
  const manifest = {
    version: 2,
    generated_at: new Date().toISOString(),
    product: 'FESTA FIESTA',
    engine_target: 'Unreal Engine 5 / UMG / CommonUI',
    canvas_reference: '1920x1080',
    palette,
    rules: {
      runtime_text: 'Textures are label-free. Add localized text and project icons in UMG/CommonUI.',
      focus: 'Use the dedicated Focus textures for keyboard/gamepad focus; never rely on color alone.',
      nine_slice: 'Assets marked Box should use the recorded pixel margin as the source 9-slice margin.',
      accessibility: 'Minimum 44 px interaction height at 1080p, pattern plus color for risk states.'
    },
    assets: records
  };
  await fs.promises.writeFile(
    path.join(outputRoot, 'manifest.json'),
    `${JSON.stringify(manifest, null, 2)}\n`,
    'utf8'
  );
}

async function makeScreenOverview() {
  const screens = [
    ['01-spatial-operations.png', 'SPATIAL OPERATIONS'],
    ['02-facility-asset-inspector.png', 'FACILITY ASSET INSPECTOR'],
    ['03-scenario-layer-analysis.png', 'SCENARIO & LAYER ANALYSIS'],
    ['04-build-workspace.png', 'BUILD WORKSPACE'],
    ['05-settings-commonui.png', 'COMMONUI SETTINGS']
  ];
  const available = [];
  for (const [file, label] of screens) {
    const fullPath = path.join(path.dirname(outputRoot), file);
    if (fs.existsSync(fullPath)) available.push({ fullPath, label });
  }
  if (available.length !== screens.length) return;

  const canvasW = 3840;
  const canvasH = 2160;
  const tileW = 1200;
  const tileH = 675;
  const placements = [
    [60, 210], [1320, 210], [2580, 210],
    [690, 1120], [1950, 1120]
  ];
  const composites = [];
  for (let index = 0; index < available.length; index++) {
    const item = available[index];
    const [left, top] = placements[index];
    const resized = await sharp(item.fullPath)
      .resize(tileW, tileH, { fit: 'cover' })
      .png()
      .toBuffer();
    composites.push({ input: resized, left, top });
  }
  const labels = available.map((item, index) => {
    const [x, y] = placements[index];
    return `${rounded(x, y - 54, tileW, 42, 13, '#102627', '#335352', 2, .98)}${textLabel(x + 22, y - 25, `${String(index + 1).padStart(2, '0')}  ${item.label}`, 22, '#8BE9D4', 750)}`;
  }).join('');
  const board = svg(canvasW, canvasH,
    `<rect width="${canvasW}" height="${canvasH}" fill="#050D0E"/><rect x="28" y="28" width="3784" height="2104" rx="36" fill="#081718" stroke="#294746" stroke-width="3"/>${textLabel(64, 92, 'FESTA FIESTA · DIGITAL TWIN UI/UX SYSTEM v2', 42, '#F6F5EF', 800)}${textLabel(64, 142, 'Unreal Engine 5 · Dadaepo Coastal Festival · Collaboration Review Board', 23, '#91AAA5', 550)}${labels}${textLabel(64, 2076, 'VIEWPORT-FIRST · PROGRESSIVE DISCLOSURE · LAYERED ANALYSIS · EXPLICIT FOCUS', 22, '#57D4B8', 700)}`,
    defs
  );
  await sharp(Buffer.from(board))
    .composite(composites)
    .png()
    .toFile(path.join(path.dirname(outputRoot), '00-v2-screen-overview.png'));
}

async function main() {
  await fs.promises.mkdir(outputRoot, { recursive: true });
  await makeButtons();
  await makeWidgets();
  await makeControls();
  await makeStatusIcons();
  await makeContactSheets();
  await writeManifest();
  await makeScreenOverview();
  process.stdout.write(`Generated ${records.length} assets in ${outputRoot}\n`);
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
