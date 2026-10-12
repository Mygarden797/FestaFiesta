# P4 일일 결산 UI

기존 셋업 스타일에 맞춰 하루 결산을 정리한다. 결산 데이터가 준비되면 Report 단계에서 한 번 열리고, 닫은 뒤 강제로 다시 열리지 않는다. 기존 다음 날 진행 동작은 유지한다.

- 오늘 매출·지출·순이익과 보유 금화를 구분한다. 금화 아이콘은 **보유 금화 옆 한 곳**에만 표시한다.
- 수입·지출 내역과 환급을 나눠 표시한다. 값이 준비되지 않았으면 임의의 수치를 만들지 않는다.
- 시뮬레이션 만족도와 평가 만족도를 구분하며, 평가 요소와 일일 목표를 함께 표시한다.
- 최종 결산 화면은 다음 단계 작업이다.

## 적용

에디터를 닫고 최신 C++ 빌드 후 실행한다.

```powershell
powershell -ExecutionPolicy Bypass -File Tools/ui/live_report_sky/run.ps1
```

`FF.ApplyLiveReportSkyDesign`으로 결산 패널과 내역 행을 적용한다. 기존 패널의 옛 위젯은 `Report_Legacy`에 숨겨 보존한다. 재실행 시 이미 적용된 디자인을 강제로 다시 만들지 않는다.

`WBP_FF_LiveHudScreen`의 `PanelLayouts → Report`가 이전 기본값(중앙, 500 또는 520 × 620, 위치 보정 0)이거나 없을 때만 중앙 1140 × 800으로 변경한다. 사용자 지정 위치·크기, 다른 탭, 하단 두 행 높이는 보존한다. 최초 적용 후 Report 위치를 수정해도 재실행이 덮어쓰지 않는다.

보고서와 적용 전 에셋 백업은 `Saved/LiveReportSky`에 남는다. 에디터 개인 설정과 저장 파일은 실행 전 백업하고 종료 후 복원한다. 필수 바인딩·WBP 컴파일·기존 HUD 설정 보존만 확인하며 PIE나 렌더 검증은 실행하지 않는다.

## 에디터에서 수정할 곳

| 대상 | 에셋 / 설정 |
| --- | --- |
| 결산 창 위치·크기 | `/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen` → Class Defaults → `PanelLayouts`의 `Report` |
| 결산 화면 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelReport` |
| 내역 한 줄 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_DailyReportLedgerRow` |

패널 Designer의 `Report_HeaderSize`, `Report_ScrollBody`, `Report_SummaryCards`, `Report_DetailColumns`, `Report_SatisfactionCard`, `Report_GoalCard`, `Report_Footer`에서 높이·여백·색·폰트를 수정한다. `Report_Legacy`는 이전 위젯 보관 영역이다.

순이익 글자색은 Designer 색 대신 패널 Class Defaults → `Net Profit Style`(양수 초록·음수 빨강·0 기본)이 적용되고, 양수 앞 `+`는 `Positive Amount Format`에서 바꾼다.
동적 날짜·상태 문구와 내역 이름은 패널 Class Defaults의 문구 설정 및 `EarnLabelOverrides` / `SpendLabelOverrides`에서 바꾼다. 내역 행의 `Text_Label`, `Text_Amount`와 패널의 `RowSpacing`으로 글꼴·금액 정렬·행 간격을 조절한다. 바인딩용 위젯 이름은 유지한다.

사용자 확인: 하루 마감 시 한 번 열리는지, 닫았다가 수동으로 다시 열 수 있는지, 긴 내역이 스크롤되고 다음 날 버튼을 가리지 않는지 확인한다.
