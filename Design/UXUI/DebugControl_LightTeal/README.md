# Debug Control LightTeal

`Test_Main`에서 시설 Lifecycle 권위 API를 관찰하고 수동 실행하는 비모달 개발자 도구다. 실제 게임 UI와 섞여 보여도 이질적이지 않도록 인게임 `LightTeal` 팔레트와 Pretendard를 사용하며, 기본 상태에서는 보이지 않는다.

- 런타임 WBP: `/Game/FestaFiesta/Building/FF_WBP_Building/Debug/WBP_FF_ZoneLifecycleTestPanel`
- 네이티브 클래스: `UFF_ZoneLifecycleTestPanel`
- 표시 범위: Editor·Shipping 패키지의 `Test_Main` Game/PIE
- 표시 토글: 게임 뷰포트 포커스 상태에서 `Ctrl+G`

## 배치 계약

| 항목 | 값 |
|---|---:|
| Anchor / Alignment | `(0, 0)` / `(0, 0)` |
| Canvas Position | `(24, 72)` |
| 고정 크기 | `480 × 540` |
| Canvas AutoSize | `false` |
| 런타임 초기 표시 | `Collapsed` |

좌측 상단에 배치해 우측 시설 정보창(`388 × 500`, 우측 24px, 상단 72px)과 분리한다. 1080p 기준 디버그 카드 하단은 `612px`, Build HUD 상단은 `690px`이므로 78px 안전 간격이 남는다. 긴 상태 문자열은 `ScrollBox_Controls` 안에서 처리해 카드가 하단 HUD 쪽으로 늘어나지 않는다.

## 정보 구조

1. `Border_Header`: 도구명, Test_Main 문맥, `Ctrl+G` 키캡.
2. `Border_StatusSection`: Stage, FFZS Phase/Revision, 시설·Queue 집계.
3. `Border_StageSection`: Setup, MorningCheck, Live, Closing 단계 전환과 이전/다음 날 이동.
4. `Border_LifecycleSection`: 전체 개장, 정상 폐쇄, 긴급 폐쇄, 새로고침.
5. `Border_LastResult`: 최근 Lifecycle 실행 결과. 본문 스크롤과 무관하게 항상 고정 표시한다.

## Designer 편집 경계

`Panel_Size`, `Border_Panel`, `Border_Header`, `ScrollBox_Controls`, 각 `Border_*Section`, `Border_LastResult`가 주요 편집 단위다. 색상 원천은 `UFF_UIPalette::Get()->InGame`, 폰트 원천은 `/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard`다.

아래 BindWidget 이름과 타입은 네이티브 계약이므로 변경·삭제하지 않는다.

- 상태 텍스트: `TXT_Stage`, `TXT_PhaseRevision`, `TXT_FacilityCounts`, `TXT_QueueCounts`, `TXT_LastResult`
- 단계 버튼: `BTN_Setup`, `BTN_MorningCheck`, `BTN_Live`, `BTN_Closing`, `BTN_PreviousDay`, `BTN_NextDay`
- 운영 버튼: `BTN_OpenAll`, `BTN_CloseAll`, `BTN_EmergencyCloseAll`, `BTN_Refresh`
- 클릭 경계: `Border_Panel`

버튼 레이블, 설명, 브러시, 패딩, 타이포 위계는 Designer에서 수정할 수 있다. 표시 데이터와 버튼 동작, `Ctrl+G` 토글, 월드 클릭 관통 방지는 C++가 소유한다.

`BTN_PreviousDay`와 `BTN_NextDay`는 `Day_Button_Row`에서 같은 폭으로 표시한다. Day 1에서는 이전 날,
Day 3에서는 다음 날 버튼을 비활성화한다. 이전 날 이동은 Time 서브시스템을 확장하지 않고 디버그 패널 전용
`FFZoneLifecycleDebugDayControl`이 공개 Stage 상태와 전환 API만 사용한다. Day 2·3은 `MorningCheck`, Day 1은
실제 시작 단계인 `Setup`으로 진입한다. 이동 전에 Active 시설을 권위 `Close` 배치로 동기 폐쇄하며 하나라도
실패하면 날짜를 바꾸지 않는다. 이 기능은 시설 Lifecycle QA를 위한 **Stage 시간축 이동**이므로 이미 확정된
경제·원장·유료 프로그램 예약과 Day 3 종료 방송 상태는 되돌리지 않는다.

`BTN_NextDay`는 날짜 값을 직접 수정하지 않는다. 마지막 날이 아니라면 현재 단계를 `Closing → Report`로
전환한 뒤 `UFF_FestivalStageSubsystem::AdvanceToNextDay()`를 호출한다. 따라서 시설 폐쇄 안전망,
`OnDayEnded`, 일일 경제·유료 프로그램 예약 초기화와 다음 날 `MorningCheck` 진입이 실제 게임과 같은 권위
경로를 통과한다. 마지막 날에는 상태를 바꾸지 않고 결과 문구만 표시한다.

## 생성·리프레시

- `FF.ZoneLifecycleTestUMG`: 에셋이 없을 때만 생성한다. 기존 WBP와 Designer 수정은 보존한다.
- `FF.RefreshZoneLifecycleTestUMG`: 승인된 스타일 갱신 때만 사용한다. 기존 WidgetTree를 LightTeal 템플릿으로 교체하고 Compile/Save한다.

PIE 테스트 시 에디터의 `Ctrl+G` Regroup 단축키가 아니라 게임 입력으로 전달되도록 게임 뷰포트를 먼저 클릭한다.
