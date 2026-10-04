# 라이브 7개 패널 에디터 수정 안내

`confirmed` — 2026-10-04, P2~P8 적용 도구의 결과와 각 단계 안내를 기준으로 정리했다.
아래 크기는 `Saved/LiveDensitySky/apply-report.json`에 기록된 **P8 저장 시점 값**이다. 이후 에디터에서 자유롭게 수정할 수 있으며, 최신 편집값은 해당 에셋을 기준으로 한다.
`confirmed` — P9의 `Saved/LivePanelsSky/finish-report.json`에서도 7패널 연결과 아래 저장값을 확인했다. 대상14개 에셋은 변경하지 않았다.
`to_verify` — 완성된 7개 패널의 실제 화면 확인은 사용자 진행 항목이다. P9에서 추가 빌드·PIE·렌더·커밋은 하지 않았다.

## 위치와 전체 크기

콘텐츠 브라우저에서 `/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen`을 열고 **Class Defaults → Panel Layouts → 원하는 탭**을 수정한다.

- **Dock=Center**: 화면 중앙. **Width / Height**: 패널 전체 크기(DPI 적용 전 단위).
- **Offset X / Y**: 중앙에서 이동할 거리. X 양수는 오른쪽, Y 양수는 아래다.
- 작은 창에서는 표시 크기·위치가 화면과 하단 HUD 안으로 제한된다. 저장된 설정값은 유지한다.
- 플레이 중 드래그한 위치는 해당 실행에서만 유지된다. 다음 실행에도 쓸 위치는 Class Defaults에서 저장한다.

| 탭 | 저장된 크기 | 패널 에셋 | 상세 안내 |
| --- | --- | --- | --- |
| Operations · 운영 | 1100×620 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelOperations` | [운영](../live_operations_sky/README.md) |
| Facilities · 시설 | 1100×600 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelFacilities` | [시설](../live_facilities_sky/README.md) |
| Guests · 관객 | 1100×640 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelGuests` | [관객](../live_guests_sky/README.md) |
| Queues · 대기열 | 1100×700 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelQueues` | [대기열](../live_queues_sky/README.md) |
| SNS | 960×700 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelSNS` | [SNS](../live_sns_sky/README.md) |
| Staff · 스태프 | 1040×660 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff` | [스태프](../live_staff_sky/README.md) |
| Density · 밀집 | 1100×760 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelDensity` | [밀집](../live_density_sky/README.md) |

위 7개 탭은 모두 **Center / Offset X=0, Y=0**으로 저장되어 있다.

## 글꼴·색·간격·문구를 바꾸는 곳

| 바꿀 항목 | 편집 위치 |
| --- | --- |
| 패널 바탕·테두리 | 각 패널 **Designer → Border_PanelRoot → Brush / Outline** |
| 제목·헤더·닫기 버튼 | `Text_PanelTitle`, `Sky_SizeBox_Header`, `Sky_SizeBox_Close`, `Sky_SizeBox_CloseIcon` |
| 카드 크기·간격·여백 | 각 패널 Designer의 `*_Size`, `*_Metrics`, `*_Border` 및 Slot Padding |
| 글꼴·정적인 글자색 | 패널 또는 행 WBP Designer의 해당 `Text_* → Font / Color and Opacity` |
| 반복 목록의 높이·열 폭 | 아래 행 WBP Designer. 시설·대기열은 패널 머리글 열 폭도 함께 변경 |
| 상태에 따라 바뀌는 색·아이콘 | 해당 패널/행 WBP **Class Defaults**의 상태별 Color / Icon 속성 |
| 숫자 형식·상태 안내 | 해당 패널/행 WBP **Class Defaults**의 Text / Format / Label 속성. `{Amount}` 등 기존 토큰 유지 |

실행 중 갱신되는 문구는 Designer의 예시 Text를 바꾸어도 다음 데이터 갱신 때 달라진다. 영구 변경은 Class Defaults의 포맷·안내 속성에서 한다. 수정 후 **Compile → Save → 새 실행**으로 확인한다. 이름으로 연결된 기존 위젯은 이름을 유지한다.

## 패널별 내부 편집 위치

| 패널 | 주요 Designer 위젯 | 동적 설정 |
| --- | --- | --- |
| 운영 | `Ops_*_Size`, `Ops_Metrics`, `Ops_GoalProgressSize`, `Bar_GoalProgress` | Operations → Text의 매출·목표·안내 포맷 |
| 시설 | `Fac_*_Size`, `Scroll_Facilities`; 행 `Rows/WBP_FF_LiveFacilityRow`의 `SizeBox_Row`와 열 SizeBox | 행 상태별 배경·문구, 패널 `Max Rows`(0=전체) |
| 관객 | `Guests_SizeBox_Counts`, `Scroll_Needs`; 행 `Rows/WBP_FF_LiveNeedRow`의 높이·막대·열 폭 | 행 상태별 막대/배지색·문구·Need Icons, 패널 집계 안내 |
| 대기열 | `Queue_*_Size`, `Queue_HighlightBorder`, `Scroll_Queues`; 행 `Rows/WBP_FF_LiveQueueRow` | 행 `State Background Colors / Labels / Icons`, 패널 `Max Rows` |
| SNS | `Sns_SizeBox_Filter0/1/2`, `Sns_SizeBox_NewPosts`; 글 카드 `Panels/WBP_FF_LiveSnsPostEntry` | 패널 필터 선택색·문구·행 간격, 글 카드 반응별 배경/글자색·기호 |
| 스태프 | `Staff_*_Size`, `Staff_Columns`, `Staff_SizeBox_CapacityBar`, `Staff_Scroll_Details` | Staff의 숫자/안내 포맷·Response Icons. 두 정원 막대의 색·브러시는 Designer |
| 밀집 | `Density_*_Size`, `Scroll_Density`, `Density_SizeBox_Toggle`; 행 `Rows/WBP_FF_LiveDensityLegendRow` | 행 기본/최대 배경색·문구, 패널 Legend Row Padding. 등급 색상 칩은 군중 시스템 원본 사용 |

위 행·카드 경로의 공통 앞부분은 `/Game/FestaFiesta/UI/LiveHUD/`이다. 목록 스크롤과 고정된 제목·요약·하단 안내는 나누어 두었다. 전체 높이를 줄일 때에는 고정 카드 높이와 중앙 목록 공간을 함께 조절한다.

## 하단 HUD

두 HUD의 보유 금화는 **Gold Text Format=`{Amount}`**로 숫자만 표시한다. 금화 Image의 Tint는 흰색으로 두어 원본 금색이 보이게 한다. P1에서 보존한 메뉴/상태 행 높이는 셋업 **32/32**, 라이브 **32/36**이며, 이후 사용자가 바꾼 에셋 값이 우선이다. 하단 크기는 각 HUD Designer의 `SizeBox_MenuRow`, `SizeBox_StatusRow`에서 수정한다.

## 사용자 화면 확인

| 확인 | 통과 기준 | 실패 시 위치 |
| --- | --- | --- |
| 7개 탭 열기·닫기 | 중앙 배치, 제목·X·하단 HUD가 겹치지 않음. Live 시간 계속 진행 | HUD Panel Layouts, 패널 Header/Close SizeBox |
| 긴 이름·글·많은 행 | 내용이 줄바꿈되며 중앙 목록만 스크롤, 마지막 행 접근 가능 | 각 행 WBP Font/SizeBox/열 폭, 해당 ScrollBox |
| 색과 수치 구분 | 숫자 가독성 확보, 상태에 문구/기호가 함께 표시. 미집계 `—`와 실제 0 구분 | 패널/행 Class Defaults의 상태색·포맷, 원본 데이터 |
| 탭별 동작 | SNS 필터·새 글 확인, 스태프 구성 막대, 밀집 히트맵 상태가 안내와 일치 | 각 단계 상세 안내의 기능 경계·사용자 확인표 |
| 에디터 수정 반영 | 위치·크기·폰트 수정 후 새 실행에도 유지 | 수정한 WBP Compile/Save, HUD의 해당 탭 Layout |

적용 스크립트는 최초 구성용이며, 일반적인 크기·색·문구 수정에는 재실행이 필요 없다. 전체 HUD 재생성 없이 위 에셋을 직접 편집한다.
