#!/usr/bin/env node
'use strict';

// Actual catalog HTML; 114 representative cards and a mocked UE bridge.
// Browser timings compare the current document to HEAD, not Unreal/CEF frame times.
// Usage: node Tools/Build/test_catalog_performance.cjs [--output-dir <directory>]
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { pathToFileURL } = require('node:url');
const root = path.resolve(__dirname, '../..');
const relativeSource = 'Content/FestaFiesta/Building/FF_WBP_Building/WebUI/catalog-ribbon.html';
const outputArg = process.argv.indexOf('--output-dir');
const output = outputArg < 0 ? path.join(root, 'Saved/CatalogPerformance') : path.resolve(process.argv[outputArg + 1]);
const report = { scope: 'Actual HTML, mocked UE bridge, 114 cards. CPU timings and representative payload sizes are not PIE/CEF measurements.', checks: [], errors: [] };
fs.mkdirSync(output, { recursive: true });
function loadPlaywright() {
  for (const name of [process.env.PLAYWRIGHT_MODULE_PATH, 'playwright', path.join(os.homedir(), '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright')].filter(Boolean)) {
    try { return require(name); } catch (error) { if (error.code !== 'MODULE_NOT_FOUND') throw error; }
  }
  throw new Error('Playwright is required. Set PLAYWRIGHT_MODULE_PATH.');
}
async function launchBrowser(chromium) {
  if (process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE_PATH) return chromium.launch({ headless: true, executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE_PATH });
  try { return await chromium.launch({ headless: true }); }
  catch (error) {
    const edge = path.join(process.env['ProgramFiles(x86)'] || 'C:/Program Files (x86)', 'Microsoft/Edge/Application/msedge.exe');
    if (!fs.existsSync(edge)) throw error;
    return chromium.launch({ headless: true, executablePath: edge });
  }
}
function withCoin(source) {
  return source.replace('__FF_HUD_GOLD_ICON__', 'data:image/png;base64,' + fs.readFileSync(path.join(root, 'Tools/ui/assets/StarEmbossedGoldCoin.png')).toString('base64'));
}
const thumbnail = 'data:image/png;base64,' + fs.readFileSync(path.join(root, 'Content/FestaFiesta/Building/FF_WBP_Building/Catalog/Images/T_UI_BuildCatalog_MainInformation.png')).toString('base64');
const full = {
  version: 1, loading: false, canInteract: true, logicalWidth: 1200, logicalHeight: 210,
  selectedId: '', showLightingIntroduction: false,
  items: Array.from({ length: 114 }, (_, index) => ({ id: 'facility-' + index, thumbnailKey: 'facility-' + index + '|thumbnail', thumbnail, name: '시설 ' + index, cost: 300, costText: '300 Gold', affordable: true, favorite: false })),
};
const delta = { ...full, items: full.items.map(({ thumbnail: omitted, ...item }) => item) };
const check = name => report.checks.push({ name, status: 'passed' });
async function main() {
  const browser = await launchBrowser(loadPlaywright().chromium);
  report.browser = browser.version();
  try {
    const page = await browser.newPage({ viewport: { width: 1200, height: 240 }, reducedMotion: 'reduce' });
    page.on('console', message => { if (message.type() === 'error') report.errors.push(message.text()); });
    page.on('pageerror', error => report.errors.push(String(error)));
    await page.addInitScript(() => { window.ue = { ffbuilding: { ready: async () => true, select: async () => true, favorite: async () => true } }; });
    const fixture = path.join(output, 'catalog-fixture.html');
    fs.writeFileSync(fixture, withCoin(fs.readFileSync(path.join(root, relativeSource), 'utf8')));
    await page.goto(pathToFileURL(fixture).href);
    await page.evaluate(({ full, delta }) => {
      window.__full = full; window.__delta = delta;
      window.ffBuilding.render(full);
      window.__card = document.querySelector('.card');
      window.__image = window.__card.querySelector('img');
      window.__favorite = window.__card.querySelector('.favorite');
    }, { full, delta });
    await page.waitForFunction(() => window.ffBuilding.metrics().readyAcknowledged);
    await page.locator('.select[data-facility-id="facility-0"]').focus();
    assert.deepEqual(await page.evaluate(() => {
      const snapshot = { ...window.__delta, selectedId: 'facility-0', items: window.__delta.items.map(item => ({ ...item, costText: '301 Gold', favorite: true })) };
      window.ffBuilding.render(snapshot);
      return {
        cardRetained: window.__card === document.querySelector('.card'),
        imageRetained: window.__image === document.querySelector('.thumb'),
        favoriteRetained: window.__favorite === document.querySelector('.favorite'),
        focusRetained: document.activeElement.dataset.facilityId === 'facility-0',
        favoriteUpdated: window.__favorite.getAttribute('aria-pressed') === 'true',
        costUpdated: window.__card.querySelector('.price-value').textContent === '301 Gold',
        cachedImage: window.__image.getAttribute('src') === window.__full.items[0].thumbnail,
        deferredDecode: window.__image.loading === 'lazy' && window.__image.decoding === 'async',
      };
    }), { cardRetained: true, imageRetained: true, favoriteRetained: true, focusRetained: true, favoriteUpdated: true, costUpdated: true, cachedImage: true, deferredDecode: true });
    check('Gold/selection/favorite delta retains card, decoded image and keyboard focus while updating authoritative state');
    assert.equal(await page.evaluate(() => {
      window.ffBuilding.render({ ...window.__delta, items: window.__delta.items.slice(1) });
      window.ffBuilding.render(window.__delta);
      return window.__card === document.querySelector('.card') && window.__image === document.querySelector('.thumb');
    }), true);
    check('Removing a filter restores the existing card and image nodes');

    // An unrelated state snapshot must not cancel an active drag.
    await page.mouse.move(100, 65);
    await page.mouse.down();
    await page.mouse.move(50, 65, { steps: 3 });
    assert.equal(await page.evaluate(() => window.ffBuilding.metrics().dragging), true);
    assert.equal(await page.evaluate(() => {
      const before = window.ffBuilding.metrics();
      window.ffBuilding.render({ ...window.__delta, selectedId: 'facility-1' });
      const after = window.ffBuilding.metrics();
      return after.dragging && before.ribbonCss.scrollLeft === after.ribbonCss.scrollLeft;
    }), true);
    await page.mouse.up();
    check('Selection/gold snapshot preserves pointer capture and scroll position during drag');

    // A retained busy card must be re-enabled after an in-flight bridge request,
    // even if another authoritative snapshot arrives before the promise resolves.
    await page.evaluate(() => {
      window.ffBuilding.render(window.__delta);
      window.ue.ffbuilding.favorite = () => new Promise(resolve => { window.__resolveFavorite = resolve; });
      document.querySelector('.favorite').click();
      window.ffBuilding.render({ ...window.__delta, selectedId: 'facility-2' });
    });
    assert.equal(await page.locator('.favorite').first().isDisabled(), true);
    await page.evaluate(() => window.__resolveFavorite(true));
    await page.waitForFunction(() => !document.querySelector('.favorite').disabled && !document.querySelector('.card').hasAttribute('aria-busy'));
    check('Snapshot during a pending favorite request does not leave retained buttons permanently busy');

    // An authored image replacement and explicit placeholder must invalidate the cache.
    assert.equal(await page.evaluate(() => {
      const replacement = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aH1EAAAAASUVORK5CYII=';
      window.ffBuilding.render({ ...window.__delta, items: [{ ...window.__delta.items[0], thumbnail: replacement }] });
      const replaced = document.querySelector('img')?.getAttribute('src') === replacement;
      window.ffBuilding.render({ ...window.__delta, items: [{ ...window.__delta.items[0], thumbnail: '' }] });
      return replaced && !document.querySelector('img') && !!document.querySelector('.placeholder');
    }), true);
    check('Replacing image bytes for the same key or explicitly clearing them updates the thumbnail');

    report.payloadBytes = { legacyStateUpdate: Buffer.byteLength(JSON.stringify(full)), cachedStateUpdate: Buffer.byteLength(JSON.stringify(delta)) };
    assert.ok(report.payloadBytes.cachedStateUpdate < report.payloadBytes.legacyStateUpdate / 100);
    check('Representative 114-card state payload omits previously sent image bytes');

    const git = spawnSync('git', ['show', 'HEAD:' + relativeSource], { cwd: root, encoding: 'utf8', maxBuffer: 10 * 1024 * 1024 });
    if (git.status === 0) {
      const baseline = path.join(output, 'catalog-baseline.html');
      fs.writeFileSync(baseline, withCoin(git.stdout));
      async function measure(file, snapshots) {
        await page.goto(pathToFileURL(file).href);
        return page.evaluate(({ full, snapshots }) => {
          window.ffBuilding.render(full);
          const samples = [];
          const firstImage = document.querySelector('img');
          for (let index = 0; index < 35; index++) {
            const next = { ...snapshots, selectedId: 'facility-' + index, items: snapshots.items.map(item => ({ ...item, costText: String(300 + index) + ' Gold' })) };
            const start = performance.now();
            window.ffBuilding.render(next);
            void document.querySelector('#ribbon').offsetWidth;
            if (index >= 5) samples.push(performance.now() - start);
          }
          samples.sort((a, b) => a - b);
          return { samples: samples.length, medianMs: samples[Math.floor(samples.length / 2)], maxMs: samples.at(-1), imageNodeRetained: firstImage === document.querySelector('img') };
        }, { full, snapshots });
      }
      report.browserCpuTiming = { baseline: await measure(baseline, full), current: await measure(fixture, delta) };
      assert.equal(report.browserCpuTiming.current.imageNodeRetained, true);
      check('Repeated 114-card authoritative updates retain the first image node');
    }
    assert.deepEqual(report.errors, [], 'Browser console/page errors');
    report.status = 'passed';
  } catch (error) {
    report.status = 'failed'; report.failure = error.stack; throw error;
  } finally {
    fs.writeFileSync(path.join(output, 'browser-report.json'), JSON.stringify(report, null, 2));
    await browser.close();
  }
  console.log(JSON.stringify({ status: report.status, checks: report.checks.length, payloadBytes: report.payloadBytes, browserCpuTiming: report.browserCpuTiming, report: path.join(output, 'browser-report.json') }));
}
main().catch(error => { console.error(error); process.exitCode = 1; });
