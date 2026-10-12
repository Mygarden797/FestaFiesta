# 결산 3단계 적용

FestaFiestaEditor를 빌드하고 일반 에디터를 종료한 뒤 `./run.ps1`을 실행한다.

- `FF.ApplyFinalReportSatisfactionColumn`으로 기존 최종 결산 화면·일별 행 끝에 만족도 열을 하나씩 추가한다.
- 기존 5열·스타일·일별 행 클래스 연결을 보존한다. 재실행 시 추가한 열도 재사용한다.
- 원본 WBP 백업: `Saved/UIBackups/FinalReportSatisfaction/`.
- 일일·최종 보유 금화 옆 아이콘만 남아 있는지 확인한다. 일일 결산 WBP는 변경하지 않는다.
- 날짜 누락·0점·종료 후 마지막 날을 다루는 `RecordedDailySatisfaction` 회귀 한 건만 실행한다.
- 개인 에디터 설정과 세이브는 실행 전 백업하고 종료 뒤 해시를 확인해 복원한다.
- 결과: `Saved/ReportPhase3/apply-report.json`, 빌드 로그: `Saved/ReportPhase3/build.log`.

실제 화면과 여백은 사용자가 플레이하며 확인한다. 전체 HUD 재생성이나 PIE는 실행하지 않는다.
