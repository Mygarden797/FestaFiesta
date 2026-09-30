# P5 개장 전 점검

상단 날씨·일일 목표·예상 방문객·금화, 수용 상태, 준비 현황·오늘 견적, 하단 진행 사유와 버튼으로 구성한다. 표시 수치는 현재 축제 데이터를 사용한다.

## 위치와 크기

`/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar`

Class Defaults → **Panel Layouts → Check**:

- `Dock`: Center
- `Size`: 기본1140×700
- `Offset`: 기본(0,0), X는오른쪽/Y는아래가양수

Compile/Save 후 PIE를 다시 시작한다. 작은 창에서는 기존 가용 영역 제한을 적용하며 본문만 스크롤한다. 같은 실행에서 드래그한 위치는 재오픈해도 유지된다.

## 내부 디자인

`/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck`의 Designer에서 편집한다.

| 대상 | 위젯 |
| --- | --- |
| 패널 배경·선 | `Border_PanelRoot` |
| 헤더 높이·일차 | `SizeBox_Header`, `Border_DayBadge`, `Text_DayLabel` |
| 닫기 버튼/아이콘 크기 | `SizeBox_Close`, `SizeBox_CloseIcon` |
| 요약 카드 폭·최소 높이 | `SizeBox_WeatherCard`, `SizeBox_DailyGoalCard`, `SizeBox_ExpectedVisitorsCard`, `SizeBox_GoldCard` |
| 요약 카드 간격 | `WrapBox_Metrics`의 Inner Slot Padding |
| 수용 상태 구획 | `Border_CapacitySummary` |
| 준비·견적 여백 | `HorizontalBox_Details`의 자식 슬롯 |
| 필수 시설 목록 | `Text_RequiredFacilities` |
| 실행 사유·버튼 | `Text_OpenGateReason`, `SizeBox_ContinueBuild`, `SizeBox_OpenGate` |

글자색·폰트·크기는 각 TextBlock, 버튼색은 Button의 Style에서 조절한다. 날씨 선택지는 전용 `WBP_FF_PrepCheckSkyWeatherChoice`의 `SizeBox_Choice`, `Button_Root`, `Text_Label`을 사용한다. 표시 내용이 바뀌는 항목은 Class Defaults의 Check/Text 포맷을 수정한다.

| 문구 속성 | 치환 값 |
| --- | --- |
| `Day Label Format` | `{Day}` 일차 |
| `Daily Goal Value Format` | `{Count}` 목표 인원 |
| `Daily Goal Bonus Format`, `Gold Value Format` | `{Amount}` 금액 |
| `Required Facility Format` | `{Name}`, `{Installed}`, `{Required}`, `{State}` |
| `No Required Facilities Text`, `Open Gate Ready Text` | 고정 안내 문구 |

중괄호 이름은 유지하고 주변 문구를 바꾼다. 수용 위험은 문자 기호와 설명을 함께 표시하며, `Capacity Caution Ratio`/`Capacity Danger Ratio`는 기존 표시 기준 설정이다. 이 수치를 바꿔도 실제 개장 규칙은 바뀌지 않는다.

## 동작과 검증 범위

- 진행 조건과 요청은 기존 HUD → Flow → Stage를 따른다. 실제 필수 시설 상태와 진행 불가 사유를 표시한다.
- 고용·홍보·프로그램·피날레 구매를 새로운 시작 조건으로 추가하지 않는다.
- 당일 프로그램과 고용비, 단일 예상 방문객 수, 미확인 값 표시를 유지한다. 예상 견적은 실제 매출 보장이 아니다.
- `FF.ApplyPrepCheckSkyDesign`은 최초 생성용이다. 이미 새 구조이면 Designer 편집을 덮어쓰지 않는다.
- `apply_and_verify.py`는 대상 백업, 다른 패널·HUD 보존 검사, 점검 자동검사를 실행한다. 결과는 `Saved/PrepCheckSky/report.json`, 렌더는 `Layout/`.
- 실제 마우스로 날씨 변경·배치 복귀·개장 진행을 확인하는 통합 검사는 자동 렌더와 별도다.
