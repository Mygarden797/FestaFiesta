#!/usr/bin/env node
'use strict';

// Runs the actual catalog HTML in a browser, with only its UE bridge mocked.
// Usage: node Tools/Build/test_catalog_lock.cjs [--output-dir <directory>]
// Optional environment: PLAYWRIGHT_MODULE_PATH, PLAYWRIGHT_CHROMIUM_EXECUTABLE_PATH.
// This does not validate UE snapshot production, toast rendering, or placement code.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

const root = path.resolve(__dirname, '../..');
const sourcePath = path.join(root, 'Content/FestaFiesta/Building/FF_WBP_Building/WebUI/catalog-ribbon.html');
const outputArg = process.argv.indexOf('--output-dir');
const outputDir = outputArg < 0 ? path.join(root, 'Saved/CatalogTutorialLock') : path.resolve(process.argv[outputArg + 1]);
const report = {
  scope: 'Actual catalog-ribbon.html, mocked UE bridge and snapshots; no UE integration assertions.',
  source: path.relative(root, sourcePath),
  checks: [],
  screenshots: [],
  errors: [],
};
fs.mkdirSync(outputDir, { recursive: true });

function loadPlaywright() {
  const modules = [process.env.PLAYWRIGHT_MODULE_PATH, 'playwright',
    path.join(os.homedir(), '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright')];
  for (const name of modules.filter(Boolean)) {
    try { return require(name); } catch (error) { if (error.code !== 'MODULE_NOT_FOUND') throw error; }
  }
  throw new Error('Playwright is required. Set PLAYWRIGHT_MODULE_PATH to its installed package directory.');
}

async function launchBrowser(chromium) {
  if (process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE_PATH) {
    return chromium.launch({ headless: true, executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE_PATH });
  }
  try { return await chromium.launch({ headless: true }); }
  catch (error) {
    const edge = path.join(process.env['ProgramFiles(x86)'] || 'C:/Program Files (x86)', 'Microsoft/Edge/Application/msedge.exe');
    if (!fs.existsSync(edge)) throw error;
    return chromium.launch({ headless: true, executablePath: edge });
  }
}

const reason = '메인안내소: 건축 한도에 도달했습니다. (설치 1/1)\n기존 시설을 철거하면 다시 건축할 수 있습니다.';
const locked = {
  id: 'info', name: '메인안내소', cost: 300, costText: '300 Gold', lighting: true,
  buildLimitReached: true, affordable: false, stateText: reason, stateLabel: '잠김',
  required: true, requiredSatisfied: true, requiredMinimum: 1,
};
const available = { id: 'stage', name: '메인공연장', cost: 0, costText: '0 Gold', affordable: true, lighting: true };
const snapshot = overrides => ({
  version: 1, loading: false, canInteract: true, logicalWidth: 1200, logicalHeight: 210,
  selectedId: '', showLightingIntroduction: false, items: [locked, available], ...overrides,
});

async function main() {
  const coinPath = path.join(root, 'Tools/ui/assets/StarEmbossedGoldCoin.png');
  // Apply only the same gold-token substitution as sync_to_umg.py. Catalog code is unchanged.
  const source = fs.readFileSync(sourcePath, 'utf8');
  assert.equal(source.split('__FF_HUD_GOLD_ICON__').length - 1, 1);
  const document = source.replace('__FF_HUD_GOLD_ICON__', 'data:image/png;base64,' + fs.readFileSync(coinPath).toString('base64'));
  const fixturePath = path.join(outputDir, 'catalog-fixture.html');
  fs.writeFileSync(fixturePath, document);
  const browser = await launchBrowser(loadPlaywright().chromium);
  report.browser = browser.version();
  const page = await browser.newPage({ viewport: { width: 1200, height: 240 }, reducedMotion: 'reduce' });
  page.on('console', message => { if (message.type() === 'error') report.errors.push(message.text()); });
  page.on('pageerror', error => report.errors.push(String(error)));
  await page.addInitScript(() => {
    window.__bridge = { calls: [], placements: [], feedback: [], blocked: {}, dismissals: 0 };
    window.ue = { ffbuilding: {
      ready: async () => true,
      select: async id => {
        window.__bridge.calls.push({ kind: 'select', id });
        if (window.__bridge.blocked[id]) {
          window.__bridge.feedback.push(window.__bridge.blocked[id]);
          return false;
        }
        window.__bridge.placements.push(id);
        return true;
      },
      favorite: async (id, value) => {
        window.__bridge.calls.push({ kind: 'favorite', id, value });
        return true;
      },
      dismisslightingintroduction: async () => { window.__bridge.dismissals++; return true; },
    } };
  });
  const check = (name, detail) => report.checks.push({ name, status: 'passed', ...(detail ? { detail } : {}) });
  const render = async (data, blocked = { info: reason }) => {
    assert.equal(await page.evaluate(({ data, blocked }) => {
      window.__bridge.blocked = blocked;
      return window.ffBuilding.render(data);
    }, { data, blocked }), true);
  };
  const inspect = async id => page.locator(`.select[data-facility-id="${id}"]`).evaluate(select => {
    const card = select.closest('.card'), state = select.querySelector('.state'), thumb = select.querySelector('.thumb');
    return {
      classes: card.className, disabled: select.disabled, label: select.getAttribute('aria-label'),
      description: select.getAttribute('aria-description'), text: state?.textContent,
      clipped: !!state && state.scrollWidth > state.clientWidth,
      background: getComputedStyle(card).backgroundColor, price: getComputedStyle(select.querySelector('.price')).color,
      thumbBackground: getComputedStyle(thumb).backgroundColor, thumbFilter: getComputedStyle(thumb).filter,
    };
  });
  const shot = async name => {
    await page.screenshot({ path: path.join(outputDir, name), fullPage: true });
    report.screenshots.push(name);
  };
  try {
    await page.goto(pathToFileURL(fixturePath).href);
    await render(snapshot());
    await page.waitForFunction(() => window.ffBuilding.metrics().readyAcknowledged);
    let card = await inspect('info');
    assert.match(card.classes, /build-limit-reached/);
    assert.equal(card.background, 'rgb(222, 224, 228)');
    assert.equal(card.thumbFilter, 'grayscale(1)');
    assert.equal(card.price, 'rgb(100, 104, 112)');
    assert.equal(card.text, '잠김');
    assert.equal(card.clipped, false);
    assert.equal(card.disabled, false);
    assert.match(card.label, /잠김.*클릭하여 이유 확인/);
    assert.ok(card.description.includes(reason));
    check('At capacity: gray card, short unclipped lock, enabled reason button, full accessible description');
    await shot('01-capacity-locked.png');

    const select = page.locator('.select[data-facility-id="info"]');
    await select.click();
    await page.waitForFunction(expected => document.querySelector('#announcer').textContent === expected, reason);
    let bridge = await page.evaluate(() => window.__bridge);
    assert.deepEqual(bridge.calls, [{ kind: 'select', id: 'info' }]);
    assert.deepEqual(bridge.placements, []);
    assert.deepEqual(bridge.feedback, [reason]);
    check('Locked pointer click invokes native selection for reason; full reason announced', 'Mocked native bridge rejects placement. Actual UE toast is not tested here.');

    const favorite = page.locator('.favorite[data-facility-id="info"]');
    await favorite.click();
    await page.waitForFunction(() => window.__bridge.calls.some(call => call.kind === 'favorite'));
    assert.deepEqual((await page.evaluate(() => window.__bridge.calls)).at(-1), { kind: 'favorite', id: 'info', value: true });
    check('Favorite stays clickable on locked card and calls its separate bridge method');

    await select.focus();
    await page.keyboard.press('Enter');
    await page.keyboard.press('Space');
    await page.waitForFunction(() => window.__bridge.calls.filter(call => call.kind === 'select').length === 3);
    assert.deepEqual(await page.evaluate(() => window.__bridge.placements), []);
    assert.equal(await select.isEnabled(), true);
    await page.keyboard.press('ArrowRight');
    assert.equal(await page.locator(':focus').getAttribute('data-facility-id'), 'stage');
    await page.keyboard.press('Home');
    assert.equal(await page.locator(':focus').getAttribute('data-facility-id'), 'info');
    check('Locked card supports Enter/Space activation and ribbon keyboard navigation');

    for (const [name, overrides] of [
      ['selected', { selectedId: 'info' }],
      ['codex focus', { items: [{ ...locked, codexFocused: true }, available] }],
      ['required missing', { items: [{ ...locked, requiredSatisfied: false }, available] }],
    ]) {
      await render(snapshot(overrides));
      await select.hover();
      card = await inspect('info');
      assert.equal(card.background, 'rgb(222, 224, 228)', name);
      assert.equal(card.thumbBackground, 'rgb(222, 224, 228)', name + ' thumbnail');
      assert.equal(card.thumbFilter, 'grayscale(1)', name);
      assert.equal(card.price, 'rgb(100, 104, 112)', name);
      check('Capacity gray takes precedence over ' + name + ' (including hover)');
    }

    await render(snapshot({ items: [{ ...locked, buildLimitReached: false, affordable: true, stateText: '', stateLabel: '' }, available] }), {});
    card = await inspect('info');
    assert.doesNotMatch(card.classes, /build-limit-reached/);
    assert.equal(card.text, undefined);
    assert.equal(card.thumbFilter, 'none');
    await select.click();
    await page.waitForFunction(() => window.__bridge.placements.length === 1);
    check('Fresh below-capacity snapshot removes lock and enables successful placement request', 'Simulates the native count-decrease snapshot; does not test native count calculation.');
    await shot('02-capacity-cleared.png');

    const insufficientFunds = '잔액이 부족합니다.';
    await render(snapshot({ items: [
      { ...locked, buildLimitReached: false, requiredSatisfied: false, stateText: insufficientFunds, stateLabel: '1개 설치' },
      { ...available, id: 'finite', name: '게임장', affordable: false, stateText: insufficientFunds, stateLabel: '0/2' },
    ] }));
    for (const id of ['info', 'finite']) {
      card = await inspect(id);
      assert.doesNotMatch(card.classes, /build-limit-reached/);
      assert.equal(card.text, insufficientFunds);
      assert.ok(card.label.includes(insufficientFunds));
      assert.ok(card.description.includes(insufficientFunds));
      assert.doesNotMatch(card.label, /1개 설치|0\/2/);
    }
    check('Non-capacity unavailable required/finite cards retain stateText reason and accessible label instead of installation instruction/count');

    await render(snapshot({ showLightingIntroduction: true }));
    assert.equal(await page.locator('#lighting-introduction').isVisible(), true);
    assert.equal(await select.isDisabled(), true);
    assert.equal(await favorite.isDisabled(), true);
    await page.waitForFunction(() => document.activeElement.id === 'dismiss-lighting-introduction');
    await shot('03-normal-introduction.png');
    await page.locator('#dismiss-lighting-introduction').click();
    assert.equal(await page.locator('#lighting-introduction').isHidden(), true);
    assert.equal(await select.isEnabled(), true);
    assert.equal(await page.evaluate(() => window.__bridge.dismissals), 1);
    check('Normal introduction remains visible, focusable and dismissible when requested');

    await render(snapshot({ showLightingIntroduction: false }));
    assert.equal(await page.locator('#lighting-introduction').isHidden(), true);
    assert.equal(await page.locator('#ribbon').getAttribute('aria-hidden'), 'false');
    assert.equal(await select.isEnabled(), true);
    await page.locator('.select[data-facility-id="stage"]').click();
    await page.waitForFunction(() => window.__bridge.placements.includes('stage'));
    check('Tutorial-style snapshot exposes facility list with no confirmation overlay', 'Only showLightingIntroduction:false rendering is tested; tutorial detection belongs to UE.');
    await shot('04-tutorial-no-introduction.png');

    await render(snapshot({ showLightingIntroduction: true, canInteract: false }));
    assert.equal(await page.locator('#lighting-introduction').isHidden(), true);
    assert.equal(await select.isDisabled(), true);
    assert.equal(await favorite.isDisabled(), true);
    check('Noninteractive snapshot keeps catalog buttons disabled and hides introduction');
    await render(snapshot({ showLightingIntroduction: true, items: [{ ...available, lighting: false }] }));
    assert.equal(await page.locator('#lighting-introduction').isHidden(), true);
    check('Introduction remains hidden when no visible facility supports lighting');

    for (const width of [960, 1486, 1920]) {
      await page.setViewportSize({ width, height: 240 });
      await render(snapshot({ logicalWidth: 1920, selectedId: 'info' }));
      card = await inspect('info');
      assert.equal(card.text, '잠김');
      assert.equal(card.clipped, false);
      check('Compact lock text does not truncate at viewport width ' + width);
    }
    assert.deepEqual(report.errors, [], 'Browser console/page errors');
    check('No browser console or page errors');
    report.status = 'passed';
  } catch (error) {
    report.status = 'failed';
    report.failure = error.stack;
    await shot('failure.png').catch(() => {});
    throw error;
  } finally {
    fs.writeFileSync(path.join(outputDir, 'browser-report.json'), JSON.stringify(report, null, 2));
    await browser.close();
  }
  console.log(JSON.stringify({ status: report.status, checks: report.checks.length, report: path.join(outputDir, 'browser-report.json') }));
}

main().catch(error => { console.error(error); process.exitCode = 1; });
