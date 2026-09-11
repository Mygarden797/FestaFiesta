// 프런트엔드용 자체 제작 SVG 아이콘·국기. 글자는 텍스처에 넣지 않는다.
const fs = require('fs'), path = require('path');
const sharp = require('C:/Users/jt/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
const out=path.resolve(__dirname,'../../SourceArt/UI/FrontendCasual');fs.mkdirSync(out,{recursive:true});
const icons={
Plus:'<path d="M32 12v40M12 32h40"/>',
Chevron:'<path d="m23 12 20 20-20 20"/>',Back:'<path d="m40 12-20 20 20 20"/>',
Check:'<path d="m12 33 13 13 28-29"/>',
Folder:'<path d="M7 18h20l6 7h24v29H7z" fill="white" stroke="none"/><path d="M7 25v-9h19l7 9"/>',
Save:'<ellipse cx="32" cy="15" rx="22" ry="9" fill="white" stroke="none"/><path d="M10 26c10 9 34 9 44 0v11c-10 9-34 9-44 0zM10 42c10 9 34 9 44 0v9c-10 9-34 9-44 0z" fill="white" stroke="none"/>',
Clock:'<circle cx="32" cy="32" r="23"/><path d="M32 17v17l12 8"/>',
Continue:'<rect x="14" y="8" width="37" height="47" rx="5"/><path d="M23 20h19M23 29h19M7 44h29m-8-8 8 8-8 8"/>',
Map:'<path d="m7 15 17-7 17 7 16-7v42l-16 7-17-7-17 7zM24 8v42M41 15v42"/>',
Lock:'<rect x="13" y="28" width="38" height="29" rx="5"/><path d="M21 28V18a11 11 0 0 1 22 0v10M32 39v8"/>',
Language:'<circle cx="32" cy="32" r="24"/><ellipse cx="32" cy="32" rx="12" ry="24"/><path d="M8 32h48M13 18h38M13 46h38"/>',
Accessibility:'<circle cx="32" cy="11" r="6" fill="white" stroke="none"/><path d="M12 24h40M32 24v14M32 38 20 56M32 38l12 18"/>',
Sound:'<path d="M9 25h11l14-13v40L20 39H9zM43 23c7 5 7 13 0 18M49 14c15 11 15 25 0 36"/>',
Graphics:'<rect x="7" y="10" width="50" height="35" rx="4"/><path d="M23 55h18M32 45v10M10 36h44"/>',
Controls:'<path d="M18 18h28c7 0 17 29 11 34-6 5-14-9-17-9H24c-3 0-11 14-17 9-6-5 4-34 11-34zM18 25v14M11 32h14"/><circle cx="44" cy="28" r="3" fill="white"/><circle cx="50" cy="36" r="3" fill="white"/>',
Info:'<circle cx="32" cy="32" r="24"/><path d="M32 29v18"/><circle cx="32" cy="18" r="2" fill="white"/>',
Spark:'<path d="M32 7c2 17 8 23 25 25-17 2-23 8-25 25-2-17-8-23-25-25C24 30 30 24 32 7z"/>'
};
const star=(x,y,r,n=5)=>'<polygon fill="white" points="'+Array.from({length:n*2},(_,i)=>{let a=i*Math.PI/n-Math.PI/2,rr=i%2?r*0.42:r;return `${x+Math.cos(a)*rr},${y+Math.sin(a)*rr}`}).join(' ')+'"/>';
const flags={
DE:'<path fill="#191919" d="M0 0h96v20H0z"/><path fill="#dc303b" d="M0 20h96v20H0z"/><path fill="#ffcd38" d="M0 40h96v20H0z"/>',
JP:'<path fill="white" d="M0 0h96v60H0z"/><circle cx="48" cy="30" r="17" fill="#cf2445"/>',
TH:'<path fill="#b4233b" d="M0 0h96v60H0z"/><path fill="white" d="M0 10h96v40H0z"/><path fill="#23335d" d="M0 20h96v20H0z"/>',
BR:'<path fill="#15934f" d="M0 0h96v60H0z"/><path fill="#ffcc34" d="m48 6 40 24-40 24L8 30z"/><circle cx="48" cy="30" r="17" fill="#264b8f"/><path d="M32 24q16-3 31 11" fill="none" stroke="white" stroke-width="3"/>',
ES:'<path fill="#c8213f" d="M0 0h96v60H0z"/><path fill="#ffce36" d="M0 15h96v30H0z"/><path fill="#bd3040" d="M25 24h12v13l-6 5-6-5z"/><path fill="#ad7124" d="M24 21h14v4H24zM21 26h3v14h-3zM38 26h3v14h-3z"/>',
US:'<path fill="white" d="M0 0h96v60H0z"/>'+Array.from({length:7},(_,i)=>`<path fill="#c83246" d="M0 ${i*120/13}h96v${60/13}H0z"/>`).join('')+'<path fill="#25466e" d="M0 0h40v32H0z"/>'+Array.from({length:9},(_,r)=>Array.from({length:r%2?5:6},(_,c)=>star(3.3+c*6.7+(r%2?3.3:0),2.4+r*3.35,1.65)).join('')).join(''),
AU:'<path fill="#234976" d="M0 0h96v60H0z"/><path stroke="white" stroke-width="10" d="m0 0 44 28M44 0 0 28"/><path stroke="#cf354a" stroke-width="3" d="m0 0 44 28M44 0 0 28"/><path stroke="white" stroke-width="12" d="M22 0v28M0 14h44"/><path stroke="#cf354a" stroke-width="7" d="M22 0v28M0 14h44"/>'+star(23,45,9,7)+star(74,12,5,7)+star(62,31,5,7)+star(85,27,5,7)+star(74,49,5,7)+star(80,37,3),
KR:'<path fill="white" d="M0 0h96v60H0z"/><g transform="translate(48 30) rotate(33)"><circle r="16" fill="#cd3344"/><path d="M-16 0a16 16 0 0 0 32 0 8 8 0 0 0-16 0 8 8 0 0 1-16 0" fill="#27629c"/></g>'+[[21,13,-56,0],[75,47,-56,7],[75,13,56,5],[21,47,56,2]].map(([x,y,a,mask])=>`<g transform="translate(${x} ${y}) rotate(${a})" fill="#26332f">`+[0,1,2].map(i=>mask&(1<<i)?`<rect x="-10" y="${i*5-6}" width="8" height="3"/><rect x="2" y="${i*5-6}" width="8" height="3"/>`:`<rect x="-10" y="${i*5-6}" width="20" height="3"/>`).join('')+'</g>').join('')
};
(async()=>{
for(const [name,body] of Object.entries(icons)){
 let svg=`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><g fill="none" stroke="white" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">${body}</g></svg>`;
 fs.writeFileSync(path.join(out,'T_FF_'+name+'.svg'),svg);await sharp(Buffer.from(svg)).resize(128,128).png().toFile(path.join(out,'T_FF_'+name+'.png'));
}
for(const [name,body] of Object.entries(flags)){
 let svg=`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 60"><defs><clipPath id="c"><rect x="0" y="0" width="96" height="60" rx="8"/></clipPath></defs><g clip-path="url(#c)">${body}</g><rect x="0.8" y="0.8" width="94.4" height="58.4" rx="8" fill="none" stroke="#b5c8b9" stroke-width="1.6"/></svg>`;
 fs.writeFileSync(path.join(out,'T_FF_Flag_'+name+'.svg'),svg);await sharp(Buffer.from(svg)).resize(192,120).png().toFile(path.join(out,'T_FF_Flag_'+name+'.png'));
}
console.log('자체 제작 아이콘 '+Object.keys(icons).length+'종 / 국기 8종 생성');
})();
