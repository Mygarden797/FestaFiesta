# 라이브 P2 — 운영 화면

오늘 매출·보유 금화·관객 만족도·안전 카드와 일일 목표를 셋업 스타일로 구성한다.
실제 오늘 누적 입장객/목표/보너스를 표시하며 안전 더미 값은 표시하지 않는다.

## 에디터 편집

- 위치·전체 크기: `WBP_FF_LiveHudScreen → Class Defaults → Panel Layouts → Operations`.
  Width/Height 기본 1100×620. Offset X 양수는 오른쪽, Y 양수는 아래.
- 카드 크기: `WBP_FF_LivePanelOperations` Designer의 `Ops_Revenue_Size`, `Ops_Gold_Size`,
  `Ops_Satisfaction_Size`, `Ops_Safety_Size` (기본 폭250/최소높이144).
  `Ops_Metrics`의 Inner Slot Padding으로 카드 간격을 바꾼다.
- 색·테두리: 각 `Ops_*_Border`, `Ops_GoalBorder`, `Ops_InfoBorder`의 Brush.
- 글꼴: 각 `Text_*` 위젯의 Font. 실행 중 스타일을 덮어쓰지 않는다.
- 목표 막대 높이: `Ops_GoalProgressSize`. 색은 `Bar_GoalProgress`.
- 동적 문구: 운영 WBP `Class Defaults → FestaFiesta → UI → Live → Operations → Text`.
  `{Current}`, `{Goal}`, `{Remaining}`, `{Bonus}`, `{Percent}`, `{Day}` 토큰을 유지한다.
- 헤더·닫기: 1단계의 `Sky_SizeBox_Header`, `Sky_SizeBox_Close`, `Sky_SizeBox_CloseIcon`.

Compile/Save 후 새 PIE에서 확인한다. 기존 제목·닫기·값 위젯을 재사용했으며,
사용하지 않는 이전 장식은 `Ops_LegacyDecorations` 안에 접어 두었다.

## 최초 적용

에디터를 닫은 뒤 최신 C++ 에디터 빌드로 아래 스크립트를 실행한다.

```powershell
& Tools/ui/live_operations_sky/run.ps1
```

`FF.ApplyLiveOperationsSkyDesign`이 운영 패널을 만들고 적용 스크립트가 운영 탭 크기를 추가한다.
버전 메타데이터가 있는 재실행에서는 Designer 편집을 보존한다.
백업·결과는 `Saved/LiveOperationsSky/`, 에디터 로그는 `Saved/BrightGlobe/live_operations_sky_apply.log`.

사용자 요청에 따라 빌드·WBP 컴파일·필수 바인딩만 확인하고 반복 렌더·다중 해상도·전체 자동화는 생략한다.
2026-10-01 빌드·에셋 적용 통과. 아래 화면 확인은 사용자 진행 항목이다.

| 사용자 확인 | 기대 표시 | 이상할 때 확인 |
| --- | --- | --- |
| 라이브 → 운영 | 중앙의 카드4개·일일 목표·하단 안내 | Operations Width/Height, 카드 폭/간격 |
| 관객 입장 | 오늘 입장객/진행률/남은 인원 갱신 | 누적 입장객 이벤트·목표 데이터 |
| 안전 | `—`, 집계 준비 중 | 임시값88이 표시되면 구 빌드 여부 |
| 편집 후 새 PIE | 위치·크기·폰트 변경 유지 | Compile/Save 여부 |
