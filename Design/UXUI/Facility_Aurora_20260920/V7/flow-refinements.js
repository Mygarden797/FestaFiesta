// 역할: 시설 정보·운영·매출 순위의 정보 순서를 정리한 검토용 패널을 만든다.
// ui.js의 기본 패널만 교체하며 게임 데이터나 배경 스크린샷은 수정하지 않는다.
(()=>{
  if(!['03-info','05-operation','08-ranking'].includes(id))return;
  const header=(title,ic='tent')=>`<div class="topline"><span class="title">${icon(ic)}${title}</span>${btn('×','iconbutton')}</div>`;
  const mini=(label,value)=>`<div class="flow-stat"><span>${label}</span><b>${value}</b></div>`;
  const slots=()=>`<div class="flow-slots" aria-hidden="true">${Array.from({length:5},()=>'<span><i></i></span>').join('')}</div>`;
  const row=(label,value)=>`<div class="flow-detail-row"><span>${label}</span><b>${value}</b></div>`;
  // 현재 화면에 해당하는 HTML을 하나만 만든 뒤 기존 시설 패널 자리에 넣는다.
  const original=scene.querySelector('.inspector');
  let output='';
  if(id==='03-info'){
    output=`<aside class="overlay pearl inspector flow-panel flow-info" data-overlay>
      ${header('시설 정보')}
      <div class="identity">${thumb('fan')}<div><div class="eyebrow">부스 · 공예·제작 체험</div><h2>나만의 부채 공방</h2><span class="small flow-status"><i></i>운영 준비 완료</span></div>${btn('☆','iconbutton favorite')}</div>
      ${tabs('시설 정보',false)}
      <div class="flow-body">
        <section class="flow-hero">
          <div class="row between"><h3>${icon('people')}대기·이용 정원</h3>${tag('운영 시작 전')}</div>
          <div class="flow-capacity"><strong>5<span>명</span></strong><div class="flow-capacity-side">${slots()}<span>이용 중인 인원을 포함한 정원</span></div></div>
          <div class="flow-stats two">${mini('동시 이용','1명')}${mini('1인 서비스','5초')}</div>
          <p class="tiny muted flow-note">운영을 시작하면 실제 대기 현황이 표시됩니다.</p>
        </section>
        <section class="flow-facts"><h3>${icon('coin')}수익 · 배치</h3>
          <div class="flow-inline-income"><span>1인 예상 수익 <b>3 Gold</b></span><span>오늘 매출 <b>0 Gold</b></span></div>
          ${row('시설 크기 / 안전 여유','4.91 × 4.2 m / 1 m')}
          ${row('설치 비용 / 철거 회수액','300 / 150 Gold')}
        </section>
        <section class="flow-program"><div class="row between"><h3>${icon('calendar')}유료 프로그램</h3>${tag('편성 가능','purple')}</div><div class="row between"><span class="small">오늘 편성 없음</span><span class="tiny muted">공예·제작 체험 지원</span></div>${btn('프로그램 선택　→','wide primary')}</section>
      </div>${footer()}</aside>`;
  }
  if(id==='05-operation'){
    output=`<aside class="overlay pearl inspector flow-panel flow-operation" data-overlay>
      ${header('시설 운영')}
      ${identity()}
      ${tabs('운영')}
      <div class="flow-body">
        <section class="flow-hero">
          <div class="row between"><h3>${icon('people')}실시간 대기열</h3>${tag('운영 종료')}</div>
          <div class="flow-capacity"><strong>0<span> / 5명</span></strong><div class="flow-capacity-side"><span>대기·이용 인원</span>${slots()}</div></div>
          <div class="flow-stats two">${mini('대기·처리 중','0팀')}${mini('남은 처리 인원','0명')}</div>
          <div class="flow-time"><span>1인 이용 완료 예상</span><b>계산 불가</b><span>처리 속도</span><b>10명 / 분</b></div>
          <p class="tiny muted flow-note">이용 중인 팀도 정원에 포함됩니다.<br>예상 시간은 본인 서비스까지 포함합니다.</p>
          <div class="flow-queue-action"><span class="tiny muted">진입 → 대기 → 서비스 → 퇴장</span>${btn('대기열 보기','primary')}</div>
        </section>
        <section class="flow-facts"><div class="row between"><h3>${icon('coin')}시설 수익</h3><span class="tiny muted">결제 0건 · 누적 0건</span></div><div class="flow-stats two">${mini('오늘 매출','0 Gold')}${mini('축제 누적','0 Gold')}</div><div class="row between flow-revenue-foot"><span class="tiny muted">오늘 매출 비중 0%</span>${btn(icon('chart')+'매출 순위　→')}</div></section>
      </div>${footer()}</aside>`;
  }
  if(id==='08-ranking'){
    output=`<aside class="overlay pearl inspector flow-panel flow-ranking" data-overlay>
      ${header('시설별 매출 순위','chart')}
      <div class="flow-period"><div class="tabs"><div class="tab">오늘</div><div class="tab active">축제 누적</div></div></div>
      <div class="flow-body">
        <section class="flow-ranking-total"><div class="row between"><span class="small">축제 누적 매출</span>${tag('설치 시설 1개')}</div><div class="row between"><strong>0 <span>Gold</span></strong>${icon('coin')}</div><div class="flow-total-bottom"><span>매출 높은 순</span><span>같은 매출은 공동 순위</span></div></section>
        <div class="flow-rank-label"><span>순위 · 시설</span><span>매출 / 비중</span></div>
        <article class="rankrow flow-rank-selected"><div class="rankseal">1</div>${thumb('archery')}<div class="flow-rank-name"><div class="eyebrow">선택한 시설</div><b>미니 양궁장</b><span class="tiny muted">다대포 · 813, 362 m</span></div><div class="flow-rank-amount"><b>0 <small>Gold</small></b><span>비중 —</span></div></article>
        <div class="flow-revenue-state">${icon('coin')}<div><b>첫 매출을 기다리는 중</b><p>손님이 이용을 마치면 매출이 반영됩니다.<br>총매출이 0이므로 비중은 표시하지 않습니다.</p></div></div>
        <div class="flow-ranking-space"><div class="flow-ledger-lines"></div><span>현재 설치된 수익 시설만 표시됩니다.</span></div>
      </div>
      <div class="flow-ranking-footer"><div class="row between"><div><div class="eyebrow">선택 시설</div><strong>미니 양궁장</strong></div><span class="badge">1위</span></div>${btn('선택 시설 정보 보기　→','wide primary')}<p class="tiny muted">철거된 시설은 목록에서 제외됩니다.</p></div>
    </aside>`;
  }
  if(original)original.outerHTML=output;
  window.ready = waitForPreviewAssets();
})();
