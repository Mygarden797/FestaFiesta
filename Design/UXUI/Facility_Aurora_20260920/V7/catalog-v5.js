/* V5 review artwork only. Buttons describe proposed actions; no game events or purchases run. */
(()=>{
  if(id!=='01-catalog')return;
  const items=[['game','게임장'],['basketball','농구 슛 챌린지'],['rhythm','리듬 게임 부스'],['archery','미니 양궁장'],['boardgame','보드게임 텐트'],['surf','서핑 밸런스 체험장']];
  const coin=()=>'<span class="v5-gold-coin" aria-hidden="true">G</span>';
  const categoryNames=['전체','부스','화장실','안내','휴게','공연','안전','포토스팟','조명','길','타일'];
  const categoryIcons=['grid','tent','tent','shield','people','tent','shield','tent','bulb','route','grid'];
  const proposal=`<section class="overlay pearl catalog catalog-v5" data-overlay>
    <div class="topline v5-catalog-top">
      <div class="v5-catalog-heading"><span class="title">${icon('tent')}시설 건축</span><span class="v5-catalog-subtitle">시설 선택 → 배치 시작</span></div>
      <div class="row v5-catalog-utilities"><div class="search">${icon('search')}시설 이름 검색</div>${btn(icon('chart')+'매출 순위')}${btn(icon('book')+'조합 도감')}
        <div class="v5-wallet">${coin()}<div><span>보유 골드</span><strong>7,600 <small>Gold</small></strong></div></div>${btn('×','iconbutton')}
      </div>
    </div>
    <div class="categories">${categoryNames.map((name,i)=>`<div class="category ${i===1?'active':''}">${icon(categoryIcons[i])}${name}</div>`).join('')}<span class="v5-required">메인공연장 설치 필요</span></div>
    <div class="filterrow"><div class="filters">${['전체 종류','게임·활동 체험','공예·제작 체험','먹거리·음료','상품 판매','야간 주점'].map((name,i)=>`<span class="filter ${i===0?'active':''}">${name}</span>`).join('')}</div><span>☆ 즐겨찾기만　□</span></div>
    <div class="facilitycards">${items.map(([asset,name],i)=>`<article class="facilitycard ${i===0?'selected':''}" aria-label="${name}, 설치 비용 300 Gold${i===0?', 선택됨':''}">
      <div class="facilityart">${art(asset)}</div>${i===0?'<span class="checkpill">✓ 선택됨</span>':''}${btn('☆','fav')}
      <h3>${name}</h3><div class="v5-card-cost"><div class="v5-gold-price">${coin()}<strong>300</strong><span>Gold</span></div><span class="status">✓ 배치 가능</span></div>
    </article>`).join('')}</div>
    <div class="v5-catalog-selection">
      <div class="v5-selected-name"><span class="v5-selected-check">✓</span><div><span>선택한 시설</span><strong>게임장</strong></div><span class="v5-selection-hint">시설을 고르고 비용을 확인하세요.</span></div>
      <div class="v5-selection-budget"><div><span>설치 비용</span><b>${coin()}300 <small>Gold</small></b></div><span class="v5-budget-arrow">→</span><div><span>설치 후 잔액</span><b>7,300 <small>Gold</small></b></div></div>
      <div class="v5-placement-action">${btn(icon('grid')+'선택해 배치 <span aria-hidden="true">→</span>','primary v5-place-button')}<span>월드에서 배치를 확정하면 300 Gold가 차감됩니다.</span></div>
    </div>
    <div class="catalogfoot"><span>카드 선택 · ☆ 즐겨찾기</span><div class="row">${btn('시설 확인 B')}<span>시설 31종</span>${btn('‹')}<b>1 / 6</b>${btn('›')}</div></div>
  </section>`;
  scene.querySelector('.catalog')?.remove();
  scene.querySelector('.context')?.remove();
  scene.insertAdjacentHTML('beforeend',context('시설 건축','카드 선택 → 비용 확인 → 선택해 배치')+proposal);
  window.ready=Promise.all([...document.images].map(img=>img.decode().catch(()=>{}))).then(()=>document.fonts.ready);
})();
