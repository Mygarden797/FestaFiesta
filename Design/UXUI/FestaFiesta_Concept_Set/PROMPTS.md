# ImageGen Prompt Set

이 세트는 **Codex 내장 ImageGen**으로 생성했습니다. CLI/API fallback은 사용하지 않았습니다. 아래는 팀이 같은 방향으로 변형을 만들 수 있도록 정리한 최종 재생성 프롬프트 세트입니다.

## 공통 기준

```text
Use case: ui-mockup
Asset type: production-ready 16:9 desktop PC game UX/UI mockup
Product: FESTA FIESTA, a single-player digital-twin festival management and construction simulation set at Dadaepo Beach, Busan
World: authored Unreal Engine strategy-game world; elevated 3/4 top-down camera; beach, ocean, boardwalk, forest edge, plaza and festival zones; teal-roof Korean-inspired wood facilities; warm amber lanterns; small low-poly visitor groups
UI: practical, shippable game interface; Pretendard-like Korean typography; Michroma-like English display typography; strict alignment; clear hierarchy; generous safe margins
Palette: white #FFFFFF, teal #527C6B, deep teal #315747, soft teal #B4C8BB, beige #E7E3E3, ink #24332E, amber #E39A22, danger #D74E43, info #3D86B4; build accent aqua #14F2E0; report graphite #05090A and raised #161F21
Accessibility: never communicate status by color alone; add icon, label, outline or pattern; readable controls and visible selected/focus states
Constraints: 16:9 desktop game screen; no mobile frame; no website dashboard; no multiplayer lobby, chat, invite or voice; no RPG quest, inventory or achievement UI; no satellite GIS; no purple cyberpunk; no watermark; no random extra text
```

공통 시설 아트 참조:

- `Saved/NotionFacilityStyledContactSheet.png`: 시설 건축·재질·조명 스타일 참조
- `Saved/FacilityThumbnails_QA.png`: 핵심 시설의 아이소메트릭 비율 참조
- `Content/FestaFiesta/Building/FF_WBP_Building/Catalog/Images/T_UI_BuildCatalog_Backdrop.png`: 해안 청사진·디지털 트윈 분위기 참조
- 각 단일 `T_UI_BuildCatalog_*.png`: 해당 시설의 형태 참조

## 00 — Main Menu

```text
Use case: ui-mockup
Asset type: FESTA FIESTA title and main-menu screen
Primary request: Create a polished full-bleed title screen using the existing GlassOrbit marine identity. Show Dadaepo Beach at a luminous west-coast sunset, calm sea and wet-sand reflection, with a tasteful teal-roof festival venue glowing along the shore. Add restrained translucent orbital arcs, marine map contours and a pearl/wave planet mark. Keep the beach view prominent on the right two-thirds; place the brand and one vertical menu group on the left.
Text (verbatim): "FESTA FIESTA", "부산 바다축제", "플레이", "설정", "게임 종료"
UI: exactly three accessible buttons. Focus "플레이" with a bright aqua orbit ring, play icon and clear shape change; settings and power icons on the other buttons.
Avoid: gameplay HUD, extra buttons, space-combat imagery, close-up characters, black letterboxing.
```

## 01 — Live Operations

```text
Use case: ui-mockup
Asset type: live festival operations gameplay screen
Primary request: Create a real in-game screenshot for Day 2 Live operations. Preserve 65–70% of the frame for the 3D Dadaepo digital-twin world at sunset-to-evening. Include the main stage, sunset wave landmark, Dada-pocha food truck, information booth, rest shade, restroom and medical tent. Show small visitor groups and short physical queues.
Layout: slim top strip; compact left "운영 현황" KPI panel; right selected-facility card connected to the Dada-pocha food truck; subtle zone outlines and patterned crowd-density cells; compact density legend; nine-tab white bottom menu rail; pale-teal status row with timeline and playback controls.
Text (verbatim): "FESTA FIESTA", "부산 바다축제", "DAY 2 · LIVE", "19:42", "G 10,480", "관객 284 / 500", "스태프 4 / 6", "운영 현황", "관객 만족도 82", "안전 88", "오늘 매출 6,400", "다대포차 푸드트럭", "대기 18명", "예상 6분", "운영", "시설", "관객", "대기열", "밀집도", "사건", "SNS", "리포트", "스태프", "20:00 메인 공연", "남은 시간 01:18"
Selected state: "운영" uses a deep-teal rounded shape and reversed white icon/text.
Avoid: oversized panels, invented quest tracker, dominant hero character.
```

## 02 — Build Mode / Valid

```text
Use case: ui-mockup
Asset type: facility catalog and valid-placement gameplay screen
Primary request: Show the same Dadaepo world during Day 2 Morning Check, slightly dimmed while build mode is active. A translucent ghost of the sunset main stage is positioned in a valid open area. Show a green footprint, separate cyan clearance perimeter, yellow queue reservation lane, 90-degree rotation handle, grid snap and zone boundary. Add an icon plus the label "배치 가능".
Layout: top status strip; deep-navy translucent catalog filling about 42% of the left; world placement preview on the right; bottom key guide; bottom-right confirmation toast.
Catalog text (verbatim): "FESTA FIESTA", "DAY 2 · MORNING CHECK", "G 10,480", "FESTA BUILD", "건축", "스킨", "닫기 B", "부스", "공연", "화장실", "안내", "안전", "휴식", "포토스팟", "선셋 메인 스테이지", "지역 아티스트 무대", "버스킹 무대", "댄스 배틀 무대", "건설 비용 1,500 Gold", "배치 가능", "시설을 선택하면 배치 모드로 전환됩니다.", "Q / E  90° 회전", "좌클릭  배치", "우클릭  취소", "ESC  건설 모드 종료"
Selected state: "공연" category and main-stage card use aqua underline/outline plus check icon.
Avoid: inventory-RPG styling, fantasy shop, error-red placement.
```

## 02b — Build Mode / Invalid

```text
Use case: precise-object-edit
Input image: 02-build-mode.png is the edit target and visual source of truth.
Primary request: Keep the complete catalog, header, world, key guide and composition unchanged. Move only the stage ghost so its footprint crosses the protected route or dashed gameplay-zone boundary.
Validation overlays: danger-red footprint with diagonal hazard hatching and a large X; separate cyan clearance perimeter; separate yellow queue reservation area; protected boundary visibly continues beneath the conflict.
Badge text (verbatim): "배치 불가"
Toast text (verbatim): "보호 동선과 겹칩니다.", "시설을 안쪽으로 이동하세요."
Constraints: encode failure with red + X + hatching + text. Preserve Gold, catalog selection, facility thumbnails and every unrelated label.
```

## 03 — Density & Queues

```text
Use case: ui-mockup
Asset type: live density and queue operations gameplay screen
Primary request: Use the same Live world and HUD system. Overlay square Crowd DensityPerSqm cells only on walkable land: safe green dots, caution/amber patterns and danger-red crosshatch. Highlight the main-stage hotspot. Do not depict Zone soft-capacity or occupancy percentage as the heat map.
Left panel text (verbatim): "실시간 밀집도", "최대 2.36 명/㎡", "LOS F · 위험", "Crowd 셀 DensityPerSqm (명/㎡)", "A · 여유 0.00–0.30", "B · 원활 0.31–0.42", "C · 보통 0.43–0.71", "D · 혼잡 0.72–1.07", "E · 매우 혼잡 1.08–2.16", "F · 위험 2.17 이상", "구역 순위", "혼잡 셀 18개 · LOS D+"
Hotspot text (verbatim): "2.36 명/㎡ · LOS F"
Right panel: title "대기열 경고" with three cards. Dada-pocha food truck: 18 people, 6 minutes, 42 people/hour. Restroom: 12 people, 4 minutes, 60 people/hour. Main-stage entry: 26 people, 8 minutes, 36 people/hour. Include "위치 보기" and "스태프 배치" actions.
Bottom tabs: "운영", "시설", "관객", "대기열", "밀집도", "사건", "SNS", "리포트", "스태프"; select "밀집도" by shape, icon and teal fill.
Avoid: blob heat map, percentages, queue throughput without people/hour unit.
```

## 04 — Promotion Planning

```text
Use case: ui-mockup
Asset type: pre-Day-1 planning and promotion preview screen
Primary request: Show the three-day festival plan and one promotion choice that applies to all three days. Use a deep-teal top bar, a slim DAY 1–3 journey strip, a large ivory "3일 운영 계획" panel with three program cards, a "홍보 선택" panel with three options, a right "시작 전 전망" summary and a sticky bottom action bar.
Text (verbatim): "FESTA FIESTA", "DAY 1 · 홍보 프리뷰", "10,000 Gold", "스태프 6명", "DAY 1", "DAY 2", "DAY 3", "3일 운영 계획", "다대불꽃쇼", "헤이즈 무대", "폴킴 무대", "홍보 선택", "홍보 안 함", "기본 홍보", "집중 홍보", "0 Gold", "500 Gold", "1,200 Gold", "×1.00", "×1.08", "×1.18", "시작 전 전망", "방문객 목표", "500명", "예상 방문객", "590명", "남은 예산", "8,800 Gold", "가용 스태프", "6명", "배치 수정", "홍보 확정 · DAY 1 시작"
Information design: show 590 versus 500 with a progress bar and +90 delta; show 10,000 − 1,200 = 8,800; show exactly six staff pictograms. Select focused promotion with filled teal card, checkmark and orbit focus ring.
Avoid: quest list, objectives sidebar, player avatar, spreadsheet overload.
```

## 05 — End-of-Day Report

```text
Use case: ui-mockup
Asset type: full-screen end-of-day report and progression gate
Primary request: Create a dark graphite three-column screen expressing observed facts → metric impact → what to change tomorrow. Use approximate widths 620 / 470 / 430 with generous gutters. Column 1 has four 92px metric rows with caption, icon+state label and large value. Column 2 shows the satisfaction equation, cause contribution bars and a small trend. Column 3 shows three actionable causes and a highlighted recommendation. Add the "다음 날" primary CTA.
Text (verbatim): "FESTA FIESTA", "Day 2 결과", "관측 사실", "관객 만족도", "62", "주의", "평가 점수", "82", "좋음", "안전", "88", "오늘 매출", "6,400", "지표 영향", "관객 만족도 62 + 피날레 20 = 평가 점수 82", "내일 바꿀 행동", "관객 만족도 62의 근거", "화장실 대기 14분", "슬롯 3 · 미해소 38%", "그늘 부족", "해변 구역 · 미해소 21%", "입장 대기 6분", "정문 · 미해소 9%", "내일 화장실 2개를 슬롯 3에 배치", "예상 관객 만족도 +9", "다음 날"
Avoid: score-medal celebration, corporate slide framing, confetti.
```

## 06 — Settings

```text
Use case: ui-mockup
Asset type: complete desktop game settings screen
Primary request: Place a warm-white settings surface over a dim, strongly blurred coastal festival. Use a deep-teal header, left navigation and two-column content. Select "게임플레이". Include camera and simulation-display cards plus a current-settings summary.
Navigation text (verbatim): "화면", "오디오", "게임플레이", "조작", "접근성"
Camera settings (verbatim): "카메라 이동 속도" "65%"; "카메라 회전 속도" "120°/초"; "회전 스냅" "45°"; "줌 감도" "115%"; "커서 기준 줌"; "가장자리 스크롤"; "가장자리 감지 폭" "24 px"
Simulation settings (verbatim): "시설 라벨", "혼잡도 범례", "밀집도 표시", "색상 + 패턴", "여유", "주의", "혼잡"
Current settings (verbatim): "현재 적용", "해상도" "1920 × 1080", "화면 모드" "전체 화면", "UI 배율" "100%", "마스터 음량" "80%", "언어" "한국어", "DX12 · 최고 품질"
Actions (verbatim): "기본값 복원", "취소", "변경 사항 적용"
Constraints: show focus treatment, 44px-class controls and pattern swatches; no account or online options.
```

## 07 — Finale Purchase

```text
Use case: ui-mockup
Asset type: Day 3 full-screen finale purchase decision
Primary request: Create a graphite/navy screen for Day 3 Morning Check. On the left, show a 2×2 comparison of C/B/A/S finale tiers. On the right, show an S-grade preview using the sunset main stage and wave landmark, the predicted score equation, budget breakdown, purchase CTA and cancel. Add a small no-purchase alternative.
Header text (verbatim): "FESTA FIESTA", "DAY 3 · MORNING CHECK", "관객 만족도 62", "Gold 10,480", "피날레 선택"
Tier data: C "2,000 G", "+5", "예상 평가 점수 67"; B "4,000 G", "+12", "예상 평가 점수 74"; A "6,000 G", "+20", "예상 평가 점수 82"; S "8,000 G", "+30", "예상 평가 점수 92"
Selected state: S has a check icon, "선택됨" label and teal outline.
Right text (verbatim): "S 등급 피날레", "관객 만족도 62 + 피날레 30 = 예상 평가 점수 92", "보유 Gold 10,480", "구매 비용 -8,000", "예산 잔액 2,480", "S 등급 구매", "취소", "미구매 · 0 G · 최소 연출"
Avoid: trophy/medal screen, celebratory confetti, altered cost or bonus values.
```

## 출력

- 내장 ImageGen 기본 저장소의 원본을 유지했습니다.
- 선택된 최종 PNG는 `Design/UXUI/FestaFiesta_Concept_Set/`에 비파괴 복사했습니다.
- 개별 화면은 모두 `1672 × 941` PNG입니다.
