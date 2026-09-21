// Strong program availability treatment on installed facilities only.
const v5ProgramBadge=(className='')=>`<span class="program-iridescent ${className}">${icon('calendar')}<strong>프로그램 가능</strong><span class="program-star">✧</span></span>`;
if(['03-info','04-combo','09-program'].includes(id)){
 const pos=id==='04-combo'?{x:204,y:78}:{x:618,y:207};
 scene.insertAdjacentHTML('beforeend',`<div class="overlay program-world-v5" data-overlay style="left:${pos.x}px;top:${pos.y}px">${v5ProgramBadge()}<div class="program-marker-income"><span>오늘 매출</span><b>0 Gold</b></div></div>`);
}
if(id==='03-info'){
 const area=scene.querySelector('.flow-program');
 if(area){area.classList.add('program-featured-v5');const old=area.querySelector('.badge');if(old)old.outerHTML=v5ProgramBadge('program-compact');}
}
if(id==='09-program'){
 scene.querySelector('.identity').insertAdjacentHTML('afterend',`<div class="program-panel-ribbon">${v5ProgramBadge('program-compact')}<span>이 시설에서 편성할 수 있어요</span></div>`);
}

if(id==='04-combo')scene.querySelector('.program-world-v5').classList.add('program-world-small-v5');
