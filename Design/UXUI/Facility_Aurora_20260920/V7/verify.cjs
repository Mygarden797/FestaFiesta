const fs=require('node:fs');const path=require('node:path');const crypto=require('node:crypto');const {pathToFileURL}=require('node:url');
const {chromium}=require('C:/Users/gksro/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const hash=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
(async()=>{
 const sourceChecks=fs.readdirSync(path.join(__dirname,'../V3/source')).filter(n=>n.endsWith('.png')).map(n=>({file:n,identical:hash(path.join(__dirname,'../V3/source',n))===hash('C:/Users/gksro/OneDrive/사진/Screenshots/스크린샷 2026-09-20 '+n)}));
 const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe'});const page=await browser.newPage({viewport:{width:1440,height:1000}});const errors=[];page.on('pageerror',e=>errors.push(String(e)));
 await page.goto(pathToFileURL(path.join(__dirname,'index.html')).href);
 await page.evaluate(async()=>{document.querySelectorAll('img').forEach(i=>i.loading='eager');await Promise.all([...document.images].map(i=>i.decode()));await document.fonts.ready;});
 const count=await page.locator('article').count();let toggles=0;
 for(const card of await page.locator('article').all()){
  for(const mode of ['source','previous','proposal']){await card.locator('[data-mode='+mode+']').click();const ok=await card.evaluate((el,mode)=>el.querySelector('img').getAttribute('src').startsWith(mode==='source'?'../V3/source/':mode==='previous'?'../V6/rendered/':'rendered/')&&el.querySelector('[data-mode='+mode+']').getAttribute('aria-pressed')==='true',mode);if(!ok)throw Error('Comparison toggle failed');toggles++;}
 }
 await page.evaluate(()=>scrollTo(0,0));await page.screenshot({path:path.join(__dirname,'gallery-preview.png')});
 const desktopOverflow=await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth);
 await page.setViewportSize({width:760,height:950});const mobileOverflow=await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth);
 const report={sourceChecks,galleryCards:count,comparisonTogglesPassed:toggles,pageErrors:errors,desktopHorizontalOverflow:desktopOverflow,mobileHorizontalOverflow:mobileOverflow,scope:'Static design gallery only; not a game runtime test.'};
 fs.writeFileSync(path.join(__dirname,'VERIFICATION.json'),JSON.stringify(report,null,2));await browser.close();console.log(JSON.stringify(report,null,2));
 if(count!==12||sourceChecks.some(x=>!x.identical)||errors.length||desktopOverflow||mobileOverflow)process.exit(1);
})().catch(e=>{console.error(e);process.exit(1)});
