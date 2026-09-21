const fs=require('node:fs');
const path=require('node:path');
const {pathToFileURL}=require('node:url');
const deps='C:/Users/gksro/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules';
const {chromium}=require(path.join(deps,'playwright'));
const sharp=require(path.join(deps,'sharp'));
const root=__dirname;
const ids=['01-catalog','02-placement','03-info','04-combo','05-operation','06-upgrade','07-codex','08-ranking','09-program','10-lighting','11-color-picker','12-light-camera'];
const chosen=process.argv.slice(2);const targets=chosen.length?chosen:ids;
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',args:['--force-color-profile=srgb','--hide-scrollbars','--allow-file-access-from-files']});
 const page=await browser.newPage({viewport:{width:1612,height:731},deviceScaleFactor:1});
 const report=[];
 for(const id of targets){
  await page.goto(pathToFileURL(path.join(root,'mockup.html')).href+'?screen='+id);
  await page.evaluate(()=>window.ready);
  const spec=await page.evaluate(()=>window.screenSpec);await page.setViewportSize({width:spec.w,height:spec.h});
  const imgErrors=await page.evaluate(()=>[...document.images].filter(x=>!x.naturalWidth).map(x=>x.src));
  if(imgErrors.length)throw Error('Missing images '+JSON.stringify(imgErrors));
  const boxes=await page.locator('[data-overlay]').evaluateAll(els=>els.map(e=>{const r=e.getBoundingClientRect();return{x:r.x-64,y:r.y-64,w:r.width+128,h:r.height+128};}));
  const buf=await page.screenshot({path:path.join(root,'rendered',id+'-v7.png')});
  const css=await page.addStyleTag({content:'.overlay{visibility:hidden!important}'});
  const baseline=await page.screenshot();await css.evaluate(e=>e.remove());
  const sourcePath=path.join(root,'../V3/source',spec.source+'.png');
  const [original,b,c]=await Promise.all([sharp(sourcePath).removeAlpha().raw().toBuffer(),sharp(baseline).removeAlpha().raw().toBuffer(),sharp(buf).removeAlpha().raw().toBuffer()]);
  const a=spec.referenceFrame?b:original; let baselineDiff=0,outsideDiff=0,outsideCount=0,beyondTolerance=0,maxChannelDelta=0;
  for(let y=0;y<spec.h;y++)for(let x=0;x<spec.w;x++){
   const k=(y*spec.w+x)*3;const diff=a[k]!==b[k]||a[k+1]!==b[k+1]||a[k+2]!==b[k+2];if(diff)baselineDiff++;
   if(boxes.some(r=>x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h))continue;
   outsideCount++;const d=Math.max(Math.abs(a[k]-c[k]),Math.abs(a[k+1]-c[k+1]),Math.abs(a[k+2]-c[k+2]));if(d)outsideDiff++;maxChannelDelta=Math.max(maxChannelDelta,d);if(d>(spec.referenceFrame?1:0))beyondTolerance++;
  }
  const layout=await page.evaluate(()=>[...document.querySelectorAll('.inspector .content,.catalog,.codexgrid,.codexdetail,.lighting-body,.picker-content,.flow-body,.placement-summary-body,.placement-help,.placement-quick,.program-world-v5,.facility-lighting-body-v6')].map(e=>({class:e.className,scrollHeight:e.scrollHeight,clientHeight:e.clientHeight})));
  report.push({...spec,source:sourcePath,baselinePixelsChanged:spec.referenceFrame?null:baselineDiff,uncoveredWorldPixelsChecked:outsideCount,uncoveredWorldPixelsChanged:outsideDiff,maxUncoveredChannelDelta:maxChannelDelta,uncoveredWorldPixelsChangedBeyondTolerance:beyondTolerance,channelTolerance:spec.referenceFrame?1:0,layout});
  console.log(id+': source baseline '+baselineDiff+' changed; world '+outsideDiff+'/'+outsideCount+' changed');
 }
 fs.writeFileSync(path.join(root,'QA_REPORT'+(chosen.length?'-partial':'')+'.json'),JSON.stringify(report,null,2));await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
