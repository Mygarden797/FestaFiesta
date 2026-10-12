# 라이브 P5 — 대기열 화면

총 대기 팀·이용 중인 팀·완료 고객 평균 대기 요약과 시설별 대기 표를 표시한다.
가장 긴 줄과 예상 대기가 가장 긴 곳을 별도 문구로 구분한다.
표의 `지금 서면`은 현재 예상 시간, 툴팁의 평균은 완료 기록의 시간이다. 없는 값은 `—`다.
기존 요약 산식을 유지하므로 상단 평균은 **완료 기록이 있는 시설별 평균들의 산술평균**이며 고객 수로 가중한 값은 아니다.

## 에디터 편집

- 위치·전체 크기: `WBP_FF_LiveHudScreen → Class Defaults → Panel Layouts → Queues`.
  최초1100×700, Offset X 양수 오른쪽/Y 양수 아래.
- 요약 카드: `WBP_FF_LivePanelQueues`의 `Queue_Total_Size`, `Queue_InService_Size`, `Queue_Average_Size`.
  폭336·최소높이132, 간격은 `Queue_Metrics`.
- 긴 줄 강조: `Queue_HighlightBorder` 배경·여백과 `Text_Longest`, `Text_WorstWait` 글꼴.
- 행: `/Game/FestaFiesta/UI/LiveHUD/Rows/WBP_FF_LiveQueueRow`.
  `SizeBox_Row` 최소높이56, `Border_Row` 배경, 각 Text의 글꼴/색.
- 열 폭은 행의 `SizeBox_Waiting/InService/ExpectedWait/Status`와 패널 머리글의
  `Queue_Waiting/InService/ExpectedWait/StatusColumn`을 함께 수정한다.
  기본100/100/145/180, 시설명은 남은 폭을 채운다.
- 상태별 배경·문구·아이콘은 행 WBP Class Defaults의 `State Background Colors`, `State Labels`, `State Icons`.
  2026-10-06에 `공연 중 중단`(Paused, 메인 공연 직전~공연 중 일반 시설 접수 휴지)이 추가됐다. 표에 없으면 공용 문구와 `미접수` 아이콘을 쓴다.
- 메인 무대는 줄이 아니라 관람석으로 받아 대기열 집계에서 빠지고 표 맨 아래 `관람석` 행으로 나온다.
  문구·툴팁·배경·아이콘은 행 WBP Class Defaults의 `Seated` 묶음, 줄 서는 시설이 없을 때 안내는 패널의 `No Queue Facility Text`.
- 접수 중이 아닌 시설(정원 참·공연 중 중단·미접수)의 `지금 서면`은 `—`로 표시한다(원본이 0을 줄 수 있어 0초로 그리지 않는다).
  아이콘 크기는 `SizeBox_StatusIcon`20, 색은 `Image_Status`.
- `Scroll_Queues`만 스크롤하고 제목·카드·머리글·하단 안내는 고정한다.
- 대기열 WBP Class Defaults의 `Max Rows`0은 전체, 양수는 표시 개수 제한.
  기존 갱신 간격1초·대기 팀 많은 순 정렬·VM 읽기를 유지한다. 줄 개폐 명령은 추가하지 않는다.
- 인원·초·긴 줄·상태 집계·안내 문구 포맷은 패널/행 Class Defaults에서 편집한다.

Compile/Save 후 새 PIE에서 확인한다. 매초 행을 재생성하지 않고 재사용한다.
이전 장식은 접힌 `Queue_LegacyDecorations`와 `Sky_ScrollBox_Body`에 보관한다.

## 적용

최신 C++ 에디터 빌드 후 에디터를 종료하고 실행한다.

```powershell
& Tools/ui/live_queues_sky/run.ps1
```

`FF.ApplyLiveQueuesSkyDesign`은 최초1회 구성한다. 재실행은 저장된 Designer 편집과 다른 탭 Layout을 보존한다.
백업·결과는 `Saved/LiveQueuesSky/`. 빌드·WBP 저장·필수 바인딩만 확인하며 반복 PIE/렌더/전체 자동화는 실행하지 않는다.

| 사용자 확인 | 기대 표시 | 이상할 때 확인 |
| --- | --- | --- |
| 라이브 → 대기열 | 중앙 요약3개·강조문구·시설표 | Queues 크기·카드 폭 |
| 평균/예상 | 완료 기록 없으면 —, 현재 예상은 별도 열 | 데이터 센티널·평균 표본 |
| 접수 상태 | 아이콘과 접수 중/정원 참/미접수/마감 중/막힘/미확인 | 상태 맵·아이콘 |
| 긴 이름/많은 시설 | 이름 줄바꿈·목록 스크롤, 수치는 팀 단위 | 열 폭·행 높이·Max Rows |
| 대기0/시설0 | 붐비는 시설로 강조하지 않고 각각 안내 | 원본 목록·요약 문구 |
