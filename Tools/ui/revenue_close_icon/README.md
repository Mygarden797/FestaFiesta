# 매출 순위 닫기 아이콘 수정

에디터를 저장·종료한 뒤 `run.ps1`을 실행한다. 프로젝트 빌드가 먼저 완료되어 있어야 한다.

- `FF.FixFacilityRevenueCloseIcon`으로 기존 `Button_Close_Label` 글자만 공통 18×18 닫기 아이콘으로 교체한다.
- 버튼 내용은 중앙 정렬·여백 0으로 맞춘다. 사용자 패널 높이·너비와 목록·다른 버튼은 유지한다.
- 원본 에셋은 `Saved/UIBackups/RevenueCloseIcon`에 백업한다. 재실행은 이미 적용된 아이콘을 보존한다.
- 결과와 치수/주변 에셋 보존 기록은 `Saved/RevenueCloseIcon/apply-report.json`에 남긴다.
- 실행 중 개인 설정·저장 파일은 백업 후 복원한다. 이 도구는 매출 순위 전체 스타일 생성기를 실행하지 않는다.
