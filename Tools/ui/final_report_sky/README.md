# P5 최종 결산 UI

축제 종료 시 확정된 평가와 전체 운영 결과를 표시하는 최종 결산 화면이다. 평가 등급과 점수는 경제 원본의 확정 결과를 그대로 표시하며 UI에서 다시 계산하지 않는다.

- 최종 보유 금화 옆에만 금화 아이콘을 한 개 표시한다. 매출·지출·순이익·환급과 일별 금액은 숫자로 표시한다.
- 전체 매출·지출은 축제 기간 전체 원장을 사용하며, 환급은 매출과 별도로 표시한다.
- 일별 행은 실제 진행 일수에 맞춰 만든다. 3일로 고정하지 않으며, 기록이 없는 값은 0으로 꾸미지 않는다.
- 누적 입장객과 마지막 날 입장객은 원본 값을 표시한다. 원본에 과거 날짜 조회 API가 없어서, 화면 데이터(ViewModel)가 매일 결산을 확정할 때 그날 입장객을 남겨 1·2일차 칸을 채운다(2026-10-06).
  축제 도중 세이브를 불러오면 그 이전 날짜는 남긴 값이 없어 `—`와 안내로 표시한다. 입장료 수입이나 누적값으로 역산하지 않는다.
- `로비로`는 기존 UI Flow의 축제 종료 요청을 사용한다.

## 적용

에디터를 닫고 최신 C++ 빌드 후 실행한다.

```powershell
powershell -ExecutionPolicy Bypass -File Tools/ui/final_report_sky/run.ps1
```

`FF.ApplyFinalReportSkyDesign`으로 최종 결산 화면과 일별 행 두 WBP를 만든다. 이미 적용된 Designer 편집값은 재생성하지 않는다. `Project Settings → FestaFiesta UI Flow → Final Report Screen Class`가 새 화면 클래스를 가리켜야 한다.

보고서와 적용 전 에셋 백업은 `Saved/FinalReportSky`에 저장한다. 에디터 개인 설정과 저장 파일은 실행 전 백업하고 종료 후 복원한다. 두 WBP의 부모·필수 바인딩·행 클래스·컴파일, 금화 아이콘 수와 기존 HUD 보존만 확인한다. PIE·렌더·게임플레이 검증은 실행하지 않는다.

## 에디터에서 수정할 곳

| 대상 | 에셋 / 설정 |
| --- | --- |
| 최종 결산 화면 | `/Game/FestaFiesta/UI/Report/WBP_FF_FinalReportScreen` |
| 일별 결산 행 | `/Game/FestaFiesta/UI/Report/WBP_FF_FinalReportDayRow` |

화면 Designer의 `CanvasPanel_Root` 아래 `ScaleBox_Viewport` 슬롯으로 화면 여백·위치를, `SizeBox_ReportPanel`로 기본 크기(1280 × 900)를 수정한다. `Border_ReportPanel`은 배경·테두리, `ScrollBox_ReportContent`는 본문 스크롤 영역이다. `ScrollBox_DayRows` 안 `Box_DayRows`에 실제 일별 행이 들어간다.

일별 행의 `Text_Day`, `Text_Revenue`, `Text_Expense`, `Text_NetProfit`, `Text_AdmittedVisitors`에서 글꼴·색·열 크기를 조정한다.
단 순이익(카드와 일별 칸)의 글자색은 각 Class Defaults → `Net Profit Style`(양수 초록·음수 빨강·0 기본)이 적용된다. 화면의 날짜 형식·미기록 안내 등 동적 문구는 Class Defaults에서 수정한다. 바인딩용 위젯 이름은 유지한다.

사용자 확인: 축제 종료 후 확정 등급·네 평가 항목·전체/일별 정산이 표시되고, 긴 내용이 잘리지 않으며 `로비로`로 돌아가는지 확인한다.

현재 마지막 날은 Report 단계가 없는 일정도 지원한다. 마지막 날 Closing에서 일일 결산의 `축제 마치기`를 누르면 최종 화면이 열리고, 이 화면의 `로비로`로 국가 지도에 복귀한다. ESC/게임패드 뒤로는 종료된 월드만 남기지 않도록 복귀 버튼 안내를 표시한다. UI가 시간 정지나 저장·레벨 이동을 별도로 실행하지 않는다.
