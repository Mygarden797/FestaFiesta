# 라이브 P3 — 시설 화면

배치 시설·운영 중·확인 필요 요약과 시설 상태 표를 셋업 스타일로 구성한다.
시설명/운영 상태/대기 팀/이용 팀/지금 서면 예상 대기를 표시한다.
기존 ViewModel 조회와 급한 순 정렬을 유지하며 시설 개폐·이동 명령은 추가하지 않는다.

## 에디터 편집

- 전체 위치·크기: `WBP_FF_LiveHudScreen → Class Defaults → Panel Layouts → Facilities`.
  기본 Width1100 / Height600. Offset X 양수 오른쪽, Y 양수 아래.
- 요약 카드: 시설 WBP의 `Fac_Total_Size`, `Fac_Active_Size`, `Fac_Attention_Size`.
  기본 폭336/최소높이110, 간격은 `Fac_Metrics`의 Inner Slot Padding.
- 목록 행: `/Game/FestaFiesta/UI/LiveHUD/Rows/WBP_FF_LiveFacilityRow`.
  글꼴은 각 Text 위젯, 행 높이는 `SizeBox_Row`(최소72), 배경은 `Border_Row`.
  상태별 배경색/문구는 행 WBP Class Defaults에서 편집한다.
- 열 폭: 행 WBP의 `SizeBox_Status/Waiting/InService/ExpectedWait`와
  시설 패널의 `Fac_StatusColumn/Waiting/InService/ExpectedWait`를 같은 값으로 바꾼다.
  기본 상태240·대기100·이용100·예상145, 시설명은 남은 폭을 채운다.
- 목록 스크롤: `Scroll_Facilities`. 제목·요약·열 머리글·하단 안내는 고정이다.
- 목록 제한: 시설 WBP Class Defaults의 `Max Rows`. **0은 전체**이며 양수는 그 개수까지 표시한다.
  갱신 간격/주의 임계는 기존 설정을 유지한다. 미확인 시설도 확인 필요에 포함한다.
- 안내·숫자 문구: 시설 WBP와 행 WBP Class Defaults의 Text 속성.

Compile/Save 후 새 PIE에서 확인한다. 목록 값은 기존 위젯을 재사용해 갱신한다.
사용하지 않는 이전 장식은 `Fac_LegacyDecorations` 및 접힌 `Sky_ScrollBox_Body`에 보관한다.

## 적용

에디터 종료 후 최신 C++ 에디터 빌드로 실행한다.

```powershell
& Tools/ui/live_facilities_sky/run.ps1
```

`FF.ApplyLiveFacilitiesSkyDesign`은 최초 한 번 프레임/행을 만든다.
재실행은 Designer 편집과 기존 탭 Layout을 보존한다.
백업·결과: `Saved/LiveFacilitiesSky/`. 반복 렌더/PIE/전체 자동화는 실행하지 않는다.

| 사용자 확인 | 기대 표시 | 이상할 때 확인 |
| --- | --- | --- |
| 라이브 → 시설 | 중앙 요약3개와 상태표 | Facilities 크기, 카드 폭 |
| 시설 여러 개 | 긴 이름/사유 줄바꿈, 목록만 스크롤 | 행 높이·열 폭, Max Rows |
| 대기 변화 | 대기/이용은 팀, 예상은 초, 미집계는 — | 원본 조회·행 바인딩 |
| 상태 미확인 | 확인 필요 수에 포함, 정상으로 요약하지 않음 | 상태 원본·요약 문구 |
