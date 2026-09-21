const fs=require('node:fs');const path=require('node:path');
let html=fs.readFileSync(path.join(__dirname,'../V6/index.html'),'utf8');
const screens=JSON.parse(html.match(/const screens=(\[[\s\S]*?\]);/)[1]);
const copy={
 '03-info':['프로그램과 매출 · 녹색 진주광','프로그램 가능과 오늘 매출을 에메랄드·민트·연두의 진주광으로 연결했습니다. 실제 0 Gold와 금색 동전은 읽기 쉽게 유지하고, 시설 패널과 버튼에도 같은 녹색 톤을 사용했습니다.'],
 '11-color-picker':['조명 편집 · 공통 녹색 메뉴','시설 정보·조합·운영·강화 옆 조명 편집 탭을 유지하며 탭·창 테두리·밝기 조절·실행 버튼을 녹색 계열로 통일했습니다. 색상환과 선택 색상은 조절 대상인 실제 색을 그대로 표시합니다.'],
 '10-lighting':['시설 정보 · 일관된 녹색 톤','연한 세이지 정보 면, 짙은 숲색 글자와 버튼, 민트·연두 진주광 헤더를 사용합니다. 시설 역할·배치·이용 정보와 현재 조명을 같은 재질과 색상 규칙으로 보여줍니다.'],
 '12-light-camera':['장면 확인 · 축소 창까지 같은 톤','창을 접은 상태에도 녹색 진주광과 짙은 숲색 조작 안내를 적용했습니다. 시설명·조명 탭·선택값을 유지하는 구성입니다. 실제 원본을 확대해 장면 확인 상태를 표현한 정적 시안입니다.'],
 '09-program':['유료 프로그램 · 녹색 선택 강조','프로그램 배지·매출 표식·선택한 프로그램·실행 버튼에 공통 녹색 소재를 적용했습니다. 편성 비용과 스태프·시간 조건, 확정 전 확인 흐름은 유지합니다.'],
 '04-combo':['시설 조합 · 녹색 효과 카드','프로그램 및 매출 표식과 조합 효과 카드의 톤을 일치시켰습니다. 실제 시설 썸네일과 원본 장면은 유지하며, 미충족 조건은 경고색으로 구분합니다.'],
 '01-catalog':['시설 건축 · 녹색 카탈로그와 골드','카탈로그 헤더·선택 카드·선택해 배치 버튼을 녹색 계열로 통일했습니다. 보유 7,600 Gold, 설치 300 Gold, 설치 후 7,300 Gold는 금색 강조를 유지해 비용을 빠르게 비교합니다.'],
 '02-placement':['배치·그리드·대기열 · 통일된 안내','왼쪽 조작 안내와 시설 이름 아래 자주 쓰는 도구를 유지했습니다. 정보 카드·대기열 가이드·빠른 도구를 녹색으로 통일했습니다. 실제 원본을 확대하여 기존 중앙 하단 패널을 화면 밖으로 잘랐습니다.'],
 '05-operation':['시설 운영 · 녹색 정보 위계','대기·이용 인원은 녹색 메쉬 면으로 강조하고 세부 수치와 매출은 담백한 세이지 면에 정리했습니다. 실행 버튼과 선택 상태는 같은 숲색을 사용합니다.'],
 '08-ranking':['시설별 매출 순위 · 녹색 진주광','매출 합계·선택한 시설·순위 문양을 녹색 계열로 통일했습니다. 기간 선택과 시설 정보 보기 버튼도 공통 숲색을 사용합니다. 원본의 미니 양궁장 1개, 매출 0 Gold 상태를 유지합니다.'],
 '06-upgrade':['시설 강화 · 변화와 비용 구분','강화 전후 표와 강화 버튼을 공통 녹색 톤으로 맞췄습니다. 비용·잔액·시설 크기 변화와 대기열 공간 확인은 유지했습니다.'],
 '07-codex':['시설 조합 도감 · 공통 녹색 재질','도감 헤더·선택 레시피·효과 카드와 상세 패널까지 같은 녹색 진주광을 적용했습니다. 원본 시설 이미지와 발견 기록·현재 효과 구분은 유지합니다.']
};
for(const screen of screens){[screen.title,screen.desc]=copy[screen.id];}
html=html.replaceAll('V6','V7').replaceAll('-v6.png','-v7.png').replaceAll('이전 V5','이전 V6').replaceAll('../V5/','../V6/').replaceAll('-v5.png','-v6.png');
html=html.replace(/const screens=\[[\s\S]*?\];/,'const screens='+JSON.stringify(screens,null,2)+';');
html=html.replace(/<header>[\s\S]*?<\/header>/,`<header><div class="eyebrow">FACILITY UI · ACTUAL PROJECT <span class="status">미승인 시안</span></div><h1>초록빛 이리디센스,<br>모든 시설 화면에 같은 톤</h1><p>에메랄드·민트·연두의 간섭광과 짙은 숲색 버튼으로 전체 화면을 맞췄습니다.<br>카탈로그부터 배치·운영·매출·조명 편집까지 같은 색과 재질 규칙을 사용합니다.</p><div class="principles"><span>녹색 계열 이리디센스</span><span>연한 세이지 정보 면</span><span>짙은 숲색 선택·실행</span><span>골드·경고·실제 색상 구분</span></div><div class="palette" aria-label="V7 공통 색상"><span><i style="background:#205039"></i>숲색</span><span><i style="background:#58b994"></i>에메랄드</span><span><i style="background:#a6e7bb"></i>민트</span><span><i style="background:#d7efa7"></i>연두</span><span><i style="background:#f3f8ee"></i>세이지</span></div></header>`);
html=html.replace('</style>',`/* V7 review page uses the same green palette as its mockups. */
:root{--ink:#264a33;--muted:#5d755f;--line:#c4d7bb;--green:#315f3d}body{background:#eef4e8}aside{background:radial-gradient(ellipse at 0 0,#bfe4c5,transparent 62%),radial-gradient(ellipse at 100% 60%,#dceabe,transparent 65%),#edf3e3}header h1{color:#254f34}.toolbar{background:radial-gradient(ellipse at 0 0,#c4e5c9,transparent 65%),radial-gradient(ellipse at 100% 0,#daebb5,transparent 52%),#f6fbef}.tabs button[aria-pressed="true"]{background:linear-gradient(115deg,#36784c,#215438,#3b7652);box-shadow:inset 0 1px #c2e2b5}.reviewnote{background:linear-gradient(100deg,#cce5c8,#e2edc7,#d9eccd);border-left-color:#598d58}.frame{border-color:#a7c899}button:focus-visible,a:focus-visible{outline-color:#4d875b}.palette{display:flex;gap:10px;margin:-8px 0 26px;flex-wrap:wrap}.palette span{display:inline-flex;align-items:center;gap:6px;font-size:11px;color:#59704e}.palette i{display:inline-block;width:16px;height:16px;border:1px solid #a4bd97;border-radius:50%}
</style>`);
fs.writeFileSync(path.join(__dirname,'index.html'),html);console.log('V7 green gallery written: '+screens.length+' screens');
