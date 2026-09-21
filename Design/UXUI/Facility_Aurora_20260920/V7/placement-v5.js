// Static placement presentation. Existing screenshot is cropped via CSS, never repainted.
if(id==='02-placement'){
 const k=(text)=>`<kbd>${text}</kbd>`;
 const help=(keys,label)=>`<div class="placement-help-row"><span>${keys}</span><b>${label}</b></div>`;
 const stat=(value,label)=>`<div><strong>${value}</strong><span>${label}</span></div>`;
 scene.innerHTML=`<img class="world" src="../V3/source/155437.png" alt="실제 프로젝트 시설 배치 원본 화면을 확대한 구성">${stamp()}
 <div class="overlay placement-mode" data-overlay>${icon('grid')}<strong>시설 배치</strong><span>1일차 · 준비</span></div>
 <section class="overlay pearl placement-summary" data-overlay>
  <div class="topline"><span class="title">배치 정보</span>${tag('✓ 배치 가능','green')}</div>
  <div class="placement-summary-body"><div class="placement-cost"><span>${icon('coin')}설치 비용</span><strong>300 <small>Gold</small></strong></div>
  <div class="placement-balance"><span>보유 7,450</span><span>설치 후 <b>7,150 Gold</b></span></div>
  <div class="placement-dimensions">${stat('4.91 × 4.2 m','시설 크기')}${stat('1 m','안전 여유')}</div>
  <div class="placement-queue-caption">${icon('people')}대기열 정보</div><div class="placement-queue-stats">${stat('5명','대기·이용 정원')}${stat('1명','동시 이용')}${stat('5초','1인 서비스')}</div>
  <div class="placement-legends"><span><i class="swatch"></i>시설</span><span><i class="swatch dashed"></i>안전 여유</span><span><i class="swatch queue"></i>대기열</span></div></div>
 </section>
 <aside class="overlay placement-help" data-overlay><div class="placement-help-title">조작 안내 <span>배치 중</span></div>
  ${help(k('좌클릭')+' / '+k('Enter'),'설치 확정')}${help(k('Q')+' / '+k('E'),'45°씩 회전')}${help(k('Shift')+' + '+k('Q / E'),'누르는 동안 자유 회전')}${help(k('Ctrl'),'25 cm 미세 이동')}${help(k('Alt'),'1 cm 정밀 이동')}${help(k('G'),'그리드 켜기 / 끄기')}${help(k('B')+' / '+k('우클릭'),'시설 목록으로')}${help(k('W A S D'),'카메라 이동')}
  <div class="placement-help-bottom">${k('Esc')} 일시정지 <span>세부 조정 ${btn('열기 +')}</span></div>
 </aside>
 <div class="overlay placement-queue-map" data-overlay><svg viewBox="0 0 390 220"><defs><pattern id="v5queue" width="9" height="9" patternUnits="userSpaceOnUse" patternTransform="rotate(40)"><rect width="9" height="9" fill="#78aebc22"/><path d="M0 0v9" stroke="#cbece98a" stroke-width="1"/></pattern><marker id="v5arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto"><path d="M0 0 10 5 0 10Z" fill="#c8ffe0"/></marker></defs>
 <polygon points="125,60 315,60 315,148 125,155" fill="url(#v5queue)" stroke="#b8dce9" stroke-width="2"/>
 <path d="M313 105H131" stroke="#bff7dc" stroke-width="2" marker-end="url(#v5arrow)"/>
 ${[0,1,2,3,4].map(i=>`<circle cx="${145+32*i}" cy="91" r="11" fill="#315e50" stroke="#d1f2d9"/><text x="${145+32*i}" y="95" text-anchor="middle">${i+1}</text>`).join('')}
 <path d="M126 156 170 186H267" fill="none" stroke="#cef4d2" stroke-width="2" stroke-dasharray="5 4" marker-end="url(#v5arrow)"/>
 <path d="M17 143V172H128V137" fill="none" stroke="#cbdaa2" stroke-width="2" stroke-dasharray="5 4"/>
 </svg></div>
 <section class="overlay placement-quick" data-overlay><div class="placement-target"><span>선택한 시설</span><span>✓ 배치 가능</span></div><h2>나만의 부채 공방</h2>
  <div class="placement-frequent"><div class="placement-install">${btn('배치 확정 <span class="key">Enter</span>','primary')}</div><div class="placement-rotate">${btn('↶ Q')}<span>0°</span>${btn('E ↷')}</div>
  <div class="placement-snap">${btn('1 m','primary')}${btn('25 cm')}${btn('1 cm')}</div>${btn(icon('grid')+'그리드 <span class="key">G</span>','placement-grid')}</div>
  <div class="placement-quick-bottom"><span>${icon('coin')}300 Gold</span><span>설치 확정 시 차감</span></div>
 </section>
 <div class="overlay placement-queue-label" data-overlay>진입 → 대기 → 서비스 → 퇴장</div>`;
 const scale=s.h/525;const world=scene.querySelector('.world');world.style.width=(scale*100)+'%';world.style.height='auto';
 window.screenSpec.referenceFrame='Original 155437 screenshot scaled by height/525 and clipped to viewport, removing the baked lower placement panel; no world repaint.';
 window.ready=Promise.all([...document.images].map(img=>img.decode().catch(()=>{}))).then(()=>document.fonts.ready);
}
