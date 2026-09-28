// 역할: 설치 시설의 프로그램 가능 표시와 오늘 매출을 정적 시안에 덧붙인다.
// ui.js와 flow-refinements.js가 만든 화면을 대상으로 하며 실제 편성이나 매출 계산은 하지 않는다.
const v5ProgramBadge=(className='')=>`<span class="program-iridescent ${className}">${icon('calendar')}<strong>프로그램 가능</strong><span class="program-star">✧</span></span>`;
// 시설 정보·조합·프로그램 화면에만 장면 위 표시를 붙여 배치 중 화면과 구분한다.
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
