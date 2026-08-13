# Image generation record

모든 생성은 내장 `image_gen`의 `ui-mockup` 모드로 진행했고, 프로젝트 내부의 기존 화면·시설 이미지·v2 컴포넌트 연락판을 시각 참조로 사용했습니다. 생성 원본은 보존하고 결과만 이 폴더로 비파괴 복사했습니다.

## 01 — Spatial Operations

```text
Use case: ui-mockup
Asset type: shippable high-fidelity Unreal Engine 5 PC game UI concept, one 16:9 landscape screen
Primary request: Redesign a refined FESTA FIESTA digital-twin “spatial operations” screen for a live festival at Dadaepo beach, Busan. Use the three supplied reference images for project identity, existing information architecture, festival-site composition, and teal/amber facility language, but create an original professional layout. Abstractly apply the best viewport hierarchy of enterprise digital-twin viewers and the clear layer/time controls of modern geospatial tools; do not copy any product or layout.
Input images: current live-operations UI, current density-and-queues UI, and project facility visual-language reference.
Scene/backdrop: a believable Unreal Engine real-time 3D digital twin of Dadaepo’s beachfront festival at blue-hour evening, angled aerial/isometric camera. Visible stage, food-truck lane, medical tent, restrooms, booths, boardwalk, shoreline, trees and paths. Authored 3D site model, not satellite imagery and not a globe/GIS map.
Subject and layout: viewport-first spatial command center. Slim top status bar; compact left rail with collapsible scene tree and layer toggles; large unobstructed center 3D viewport with only four clean anchored labels and restrained crowd-density cells; right contextual operations panel with two actionable alerts; bottom two-tier timeline/scenario control strip. Clear z-order, generous spacing, 8px-like rhythm, practical buttons, large readable state colors, strong focus hierarchy.
Style/medium: realistic polished product UI mockup, production-ready Unreal PC interface. Semi-opaque graphite glass panels with crisp thin borders, deep festival teal, mint highlights, warm amber from lanterns, coral only for risk. Typography resembles clean Pretendard Korean sans-serif, strong numeric tabular alignment.
Text (verbatim): “FESTA FIESTA”, “다대포 바다축제”, “DAY 2 · LIVE”, “19:42”, “관객 284 / 500”, “스태프 4 / 6”, “공간 관제”, “레이어”, “시설”, “관객 흐름”, “대기열”, “밀집도”, “메인 무대”, “푸드트럭”, “의료 텐트”, “화장실”, “현장 알림”, “푸드트럭 대기 18명”, “메인 무대 앞 혼잡”, “위치 보기”, “조치 배정”, “20:00 메인 공연”, “남은 시간 01:18”, “실시간”. Do not add placeholder paragraphs or random tiny labels.
Composition/framing: exact desktop 16:9 full-screen UI screenshot; left panel approximately 18%, right panel approximately 22%, center dominant, shallow bottom strip. Restrained density overlay using green/amber/coral. No watermark, browser chrome, device frame, copied branding, illegible microtext, excessive neon, sci-fi holograms, satellite imagery, world map, survey coordinates, militaristic UI, multiplayer/chat, RPG quests, random graphs or clutter.
```

## 04 — Build Workspace

```text
Use case: ui-mockup
Asset type: collaboration-ready, shippable 16:9 Unreal Engine 5 PC game construction workspace UI
Input images: current FESTA BUILD screen, approved FESTA FIESTA v2 button-state design system, approved v2 digital-twin widget system, project facility reference sheet.
Primary request: Create a refined next-generation “FESTA BUILD · 건축 워크스페이스” screen for the Dadaepo coastal festival digital-twin management game. Combine a game-friendly asset browser, spacious central 3D placement viewport, selected-asset inspector, and contextual action bar. Make it directly implementable with Unreal UMG/CommonUI.
Composition: deep-teal top bar; left mode rail and asset browser; dominant central 3D viewport; right inspector; bottom action/history bar. Show “FESTA FIESTA”, “DAY 2 · MORNING CHECK”, “10,480 Gold”; workspace switcher “운영”, “건축”, “분석” with 건축 selected. Asset browser includes search, filters and selected “선셋 메인 스테이지”. Central viewport has valid footprint, clearance, queue reservation, world badge “배치 가능”, transform gizmo and 90° rotation arc. Inspector includes “기본 정보”, “운영 조건”, “배치 검사”, exact cost/area/power/access values and three successful validation rows. Bottom hints: “Q / E 회전”, “M 이동”, “Enter 확정”, “Esc 종료”, undo/redo and “배치 기록 3”.
Visual language: graphite/navy glass, restrained mint/aqua, ivory, amber for cost/queue and coral only for error. 8 px grid, readable Korean, selection uses shape/checkmark plus color, minimum 44 px controls, visible focus ring. No multiplayer, chat, cloud/account UI, CAD jargon, Blueprint graph, satellite imagery, global GIS, RPG, watermark, third-party marks or cyberpunk overload.
```

## 05 — Settings CommonUI

```text
Use case: ui-mockup
Asset type: polished production-ready 16:9 PC game settings screen for an Unreal Engine digital-twin festival management game
Primary request: Create a significantly more refined FESTA FIESTA v2 settings UI consistent with the approved button and widget kit. Use a dimmed Unreal aerial Dadaepo background, top title bar, centered settings shell, left category rail, wide settings panel, right current-summary panel, and persistent bottom action bar.
Exact categories: “화면”, “오디오”, “게임플레이”, “조작”, “접근성”; select 게임플레이. Main cards: “카메라” and “디지털 트윈 표시”. Include camera movement/rotation/zoom sliders, 45° snap, cursor zoom and edge scroll toggles, facility labels, congestion legend, and three-way density display with “색상 + 패턴” selected. Right summary: resolution 1920×1080, fullscreen, UI 100%, master 80%, Korean. Bottom: 기본값 복원, 취소, 변경 사항 적용, 선택 and 뒤로 hints.
CommonUI behavior cues: background screen → activatable settings layer → selected category → focused control. Show selected and focused states separately using a crisp double focus ring. Deep ink-teal, mint, warm white, restrained amber/coral. No multiplayer, account, sign-in, networking, chat, satellite GIS, copied third-party branding, watermark or sci-fi clutter.
```

## 02 — Facility Asset Inspector

```text
Create one polished production-grade 16:9 Unreal Engine PC game UI mockup for FESTA FIESTA, a digital-twin festival management game set at Dadaepo Beach, Busan. This is a FACILITY ASSET / STATUS INSPECTOR screen. Use the attached images only as visual references for the established FESTA FIESTA brand, Dadaepo site, teal/ivory glass panels, warm sunset-orange highlights, and the exact stylized food-truck asset. Do not copy any third-party product UI; abstract only the universal enterprise patterns of a hierarchical asset tree, a spatial selection viewport, property cards, an event history, and an alert.

Visual direction: premium collaboration-ready game UI, crisp Unreal PC interface, 16:9 widescreen, high information hierarchy, restrained dark navy-teal translucent glass with warm ivory surfaces, sea-glass cyan selection glow, coral/amber warning accent, subtle rounded corners, precise 8px spacing rhythm, realistic coastal dusk atmosphere, legible sans-serif Korean. Avoid generic sci-fi HUD clutter. No Autodesk, Tandem, Cesium, ArcGIS, or other third-party logos/branding. No multiplayer, chat, avatars, quests, fantasy RPG, satellite feeds, sensor fantasy, maintenance work orders, or invented IoT telemetry.

Composition:
- Thin top global bar. Left wordmark exactly “FESTA FIESTA”. Main section label exactly “시설 자산”. Center-right status chips exactly “DAY 2 · LIVE” and “19:42”. Currency at far right exactly “10,480 Gold”.
- Left 23% panel: header “시설 자산”, compact search field placeholder “시설 검색”. Tree headed “다대포 축제장”. Category rows: “공연 시설”, “부스”, “편의 시설”, “안전 시설”. Expand “부스” and show three child items: “다대포차 푸드트럭” selected with cyan highlight, “선셋펍”, “기념품 숍”. Use small original category glyphs, indentation, chevrons, and clear selected state; do not mimic any commercial UI icons.
- Center 47% main viewport: showcase the attached stylized teal-roof, warm-wood food truck as the selected facility in a clean 3D digital-twin scene on the Dadaepo festival ground, with a subtle white/cyan selection outline and a small footprint grid. Background shows a softly defocused festival promenade and ocean at dusk, with no people dominating. A restrained locator inset can show the selected booth on the site. Floating label exactly “다대포차 푸드트럭” and green badge exactly “운영 중”. Bottom viewport controls only: “회전”, “확대”, “위치 보기”.
- Right 30% inspector panel: title “다대포차 푸드트럭”, small secondary line “Booth_Food_01”, tabs “개요”, “속성”, “이력” with “개요” active. Prominent amber alert card: “대기열 주의”, “정원까지 2명”, “18 / 20명”. Section “운영 상태”: “수명주기—활성”, “구역—푸드트럭 골목”, “대기 상태—운영”, “예상 대기—6분”, “처리율—42명/시간”. Section “운영 이력”: “19:31 대기열 주의”, “19:18 시설 열림”, “17:00 배치 검증 완료”. Bottom actions: “위치 보기”, “시설 닫기”.
Keep Korean copy short and exact. Prioritize crisp typography. Show no other gibberish labels. Coherent shipped game screen, original layout.
```

## 03 — Scenario Layer Analysis

```text
Create one polished production-grade 16:9 Unreal Engine PC game UI mockup for FESTA FIESTA, a digital-twin festival management game set at Dadaepo Beach, Busan. This is a FESTIVAL SCENARIO COMPARISON / LAYER ANALYSIS screen. Use the attached images only as visual references for the established FESTA FIESTA brand, the same oblique Dadaepo festival-ground map, teal/ivory glass panels, warm sunset-orange highlights, crowd-density colors, protected-route overlays, and precise management-game density. Do not copy any third-party product UI; abstract only universal geospatial analysis principles: a layer panel, filters, synchronized A/B map comparison, a fixed-time simulation timeline, and metric deltas.

Visual direction: premium collaboration-ready game UI, crisp Unreal PC interface, 16:9 widescreen, restrained dark navy-teal translucent glass over a richly modeled oblique coastal festival map, sea-glass cyan, ivory text, green/yellow/orange/red data layers, subtle warm Dadaepo sunset. Clear professional hierarchy, original iconography, realistic management simulation. No Autodesk, Tandem, Cesium, ArcGIS, or other product names/logos/branding. No multiplayer, chat, avatars, quests, fantasy RPG, satellites, orbit imagery, live satellite or live sensor claims.

Composition:
- Top: “FESTA FIESTA”, “시나리오 분석”, “DAY 2”, “19:30”, and badge “시뮬레이션 결과”.
- Left panel “레이어”: enabled rows “시설”, “군중 밀도”, “대기열”, “보호 동선”, “출입구”. Filters “구역 전체”, “상태 혼잡 이상”. Scenario cards “A 현재 배치”, “B 대기열 분산안”, with B selected.
- Center: synchronized same-camera same-time split comparison. Labels “A · 현재 배치”, “B · 대기열 분산안”; draggable vertical divider. A has orange/red hotspot and queue, B has plausible yellow/green redistribution and open cyan route. Small legend “여유”, “주의”, “혼잡”.
- Right “비교 결과”: “최대 밀도 2.36 → 1.58 명/m² -33%”, “혼잡 셀 18 → 7개 -11”, “평균 대기 6 → 4분 -2분”, “안전 동선 주의 → 정상”. “분석 요약”: “대기열 분산으로 무대 앞 혼잡이 완화됩니다.” Buttons “B안 저장”, “비교 닫기”.
- Bottom timeline “시간” with ticks 17:00–21:00, scrubber 19:30, event markers “입장”, “메인 공연”, “퇴장”, footer “동일 조건 · 동일 시점 비교”.
Keep Korean copy exact, map camera/time identical, and the change plausible. Original shipped game-analysis screen.
```
