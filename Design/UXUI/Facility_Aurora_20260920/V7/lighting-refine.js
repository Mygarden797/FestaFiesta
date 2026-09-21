// Presentation-only layout states. These controls do not change the game.
if(id==='11-color-picker'){
 scene.querySelector('.lighting-panel')?.remove();
 const panel=scene.querySelector('.picker');panel.classList.add('unified-picker');
 panel.querySelector('.title').innerHTML='<span class="grip">⠿</span>조명 편집';
 const content=panel.querySelector('.picker-content');
 content.querySelector('.camera-hint')?.remove();content.querySelector('.picker-guidance')?.remove();
 content.insertAdjacentHTML('beforeend',`<div class="picker-brightness"><div class="row between small"><b>밝기</b><span>100% · 기본</span></div><div class="light-slider"></div><div class="slider-ends"><span>0%</span><span>500%</span></div></div><div class="presetbar">${btn('원래 색')}${btn('청록','primary')}${btn('노을')}${btn('보라')}${btn('● 켜짐','primary')}</div><p class="picker-guidance">${icon('route')}색상환·사각 영역·밝기를 드래그해 확인</p>`);
 panel.querySelector('.picker-foot').innerHTML=btn('기본값 복구')+btn('취소')+btn('설정 적용','primary');
}
if(id==='12-light-camera'){
 scene.querySelector('.lighting-panel')?.remove();
 const mini=scene.querySelector('.minipicker');mini.style.left='auto';mini.style.right='80px';mini.style.top='119px';
 mini.querySelector('.mini-body .tiny').textContent='메인공연장 · 미리보기';
 mini.querySelector('.mini-bottom').innerHTML='<div class="row between"><span>색상·밝기를 유지하고 장면 확인</span>'+btn('편집 계속')+'</div>';
 const world=scene.querySelector('.world');world.style.width=(s.w/1320*100)+'%';world.style.height='auto';
 window.screenSpec.referenceFrame='Original screenshot enlarged by width/1320; original sidebar and lower HUD clipped by viewport. No world repaint.';
 scene.querySelector('.context .muted').textContent='　·　설정창을 접고 카메라로 주변을 확인하세요';
}
if(id==='10-lighting'){
 const panel=scene.querySelector('.lighting-panel');panel.querySelectorAll('.section')[3]?.remove();
 panel.querySelector('.lighting-body').insertAdjacentHTML('beforeend','<div class="camera-hint">'+icon('search')+'색상 편집을 열면 색상환·밝기·장면 확인을 한 창에서 조절할 수 있어요.</div>');
}
