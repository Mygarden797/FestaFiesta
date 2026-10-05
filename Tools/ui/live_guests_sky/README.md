# 라이브 P4 — 관객 화면

왼쪽에는 현재 관객·오늘 누적 입장객, 오른쪽에는 관객 불편 비율을 표시한다.
인원은 각 ViewModel 원본을 별도로 읽으며 임의로 더하거나 대체하지 않는다.
막대는 실제 미해소 비율 0~100%를 그대로 표시한다. 표본이 없으면 수치는 `—`, 상태는 `집계 준비 중`이며 막대도 숨긴다.

## 에디터 편집

- 전체 위치·크기: `WBP_FF_LiveHudScreen → Class Defaults → Panel Layouts → Guests`.
  최초 크기1100×640. Offset X 양수는 오른쪽, Y 양수는 아래.
- 인원 카드 폭: `WBP_FF_LivePanelGuests → Guests_SizeBox_Counts`(330).
  두 카드 높이는 남은 세로 공간을 반씩 사용한다. 배경/여백은 `Guests_Current_Border`, `Guests_Admitted_Border`.
- 목록 행: `/Game/FestaFiesta/UI/LiveHUD/Rows/WBP_FF_LiveNeedRow`.
  `SizeBox_Row` 최소높이64, `SizeBox_Progress` 막대높이16. 글꼴/색은 각 Text 위젯에서 편집한다.
- 이름·퍼센트·상태 폭: 행의 `SizeBox_Name`108 / `SizeBox_Percent`64 / `SizeBox_Status`90.
  막대가 남은 폭을 채운다. 안내 문구는 아래로 줄바꿈한다.
- 상태별 막대색/배지색/문구: 행 WBP Class Defaults.
  아이콘도 `Need Icons`에서 교체 가능하며 색·크기는 Designer의 `Image_NeedIcon`과 부모 SizeBox에서 편집한다.
- 불편 목록 스크롤: `Scroll_Needs`. 인원 카드·제목·상단 집계 안내·하단 요약은 고정이다.
- 인원 포맷/안내 문구/주의 기준/갱신 주기: 관객 WBP Class Defaults.
  기존 주의25%·나쁨50%·1초 갱신 설정은 유지한다. 이 임계는 UI 설정이며 게임 규칙을 바꾸지 않는다.

Compile/Save 후 새 PIE에서 확인한다. 행은 갱신마다 새로 만들지 않고 재사용한다.
이전 장식은 접힌 `Guests_LegacyDecorations` 및 `Sky_ScrollBox_Body`에 보관한다.

## 적용

최신 C++ 에디터 빌드 후 열린 에디터를 종료하고 실행한다.

```powershell
& Tools/ui/live_guests_sky/run.ps1
```

최초1회 `FF.ApplyLiveGuestsSkyDesign`으로 구성하며 재실행은 Designer 편집과 다른 탭의 Layout을 보존한다.
백업·결과는 `Saved/LiveGuestsSky/`. C++ 빌드·WBP 저장·바인딩만 확인하고 반복 PIE/렌더/전체 자동화는 실행하지 않는다.

| 사용자 확인 | 기대 표시 | 이상할 때 확인 |
| --- | --- | --- |
| 라이브 → 관객 | 중앙에 인원2카드와 불편 목록 | Guests 크기·Offset |
| 현재/누적 | 각 원본 값, 0과 미확인을 구분 | Crowd/Progression 공급 상태 |
| 6% 항목 | 막대 전체 길이의6%, 퍼센트도6% | Progress 스타일·Fill 설정 |
| 표본 없음 | —/집계 준비 중, 막대 숨김 | 표본 수·Severity |
| 높은 불편 | 상태 라벨과 대처 문구, 긴 내용은 목록 스크롤 | 행 높이·열 폭·스크롤 |
