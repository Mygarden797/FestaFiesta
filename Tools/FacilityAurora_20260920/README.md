# 시설 Aurora UI 적용 기록 — 2026-09-20

## 적용 내용
- 실제 WBP 15개에 초록 계열 이리디센스·메쉬 그라디언트 UI 재질 저장.
- 카탈로그 후보 선택과 배치 시작 분리, Gold/비용/잔액 강조.
- 왼쪽 배치 정보·도움말, 시설 이름 아래 빠른 조작, 대기열 정원 표시.
- 시설 정보·조합·운영·강화 및 조명 편집 탭, 실제 데이터 기반 매출 순위·도감·프로그램.
- 조명 색상환/SV/HSV/RGB/HEX, 즉시 미리 보기, 적용·취소 복원, 이동·접기와 입력 포커스 보호.
- 프로그램/오늘 매출 월드 배지와 큰 금액 표시 보정.

사용 가이드: [USER_GUIDE_KO.md](USER_GUIDE_KO.md)
노션: [시설 UI 개선 안내](https://app.notion.com/p/3e196c43a7f7805ab4f3dd0134d2056e)

## 저장된 UI 반영
`apply_and_verify.py`를 UE 5.7 Editor의 Entry 맵에서 ExecutePythonScript로 실행한다.
이 스크립트는 `create_materials.py` 실행 후 명시적인 에디터 마이그레이션 명령을 순서대로 호출한다.
런타임 위젯 트리 재생성 방식이 아니며, 결과는 Content의 WBP와 재질 에셋에 저장되어 있다.
단계별 메타데이터로 재실행 시 기존 Designer 편집과 이미 적용한 결과를 보존한다.

최종 반영 결과:
- `Saved/FacilityAurora_20260920/migration-result.json`: PASS
- WBP 15개 확인, 그 밖의 UI 에셋 425개 SHA-256 불변
- 명령 2회 실행 후 대상 에셋 해시 동일
- 사전 백업: `Saved/FacilityAurora_20260920/before_assets/`

## 검증
- UE 5.7.4, Win64 Development
- FestaFiestaEditor 빌드: 성공
- FestaFiesta 게임 빌드: 성공
- 최종 자동 검사: **41개 통과, 실패 0개** (39개 성공 + 경고 있는 성공 2개)
- 보고서: `Saved/FacilityAurora_20260920/Tests-Complete/index.json`
- 로그: `build-editor.log`, `build-game.log`, `tests-complete.log` (위 Saved 폴더)
- `git diff --check`: 통과

검사 범위는 FacilityInfo, FacilityRevenue, FacilityCodex.UI, FacilityProgramMarker,
PrepBuild.AuroraSelectionRequiresExplicitPlacement, Building.BuildTools, PrepBuild.FullCatalogHudLayout이다.
매출/구매/조명/조합의 상태 처리와 UMG 표시 계약, 화면 크기 및 재질 렌더링을 포함한다.

검사 경고 2종: 방문객 없는 프로그램 시험의 정산·정원 경고, 공통 LiveHUD 아이콘 6개 누락.
실제 Test_Main의 일부 구역 이동 경로 경고는 별도 맵 문제로 남기고 맵/내비게이션은 수정하지 않았다.
최종 배포 패키지의 전체 플레이나 모든 시설의 물리적 마우스·키보드 조작 검증을 주장하지 않는다.

## 실제 프로젝트 캡처
- 스크립트: `Tools/ui/capture_facility_aurora_runtime.py`
- 실제 축제 시작 → 실제 PrepHUD Build 탭 → 카탈로그 닫기/열기 → 기존 시설 카드 선택
- 실제 시설 114종 로드, 후보 게임장 300 Gold / 보유 7,600 Gold / 예상 잔액 7,300 Gold 확인
- 캡처: `Saved/FacilityAurora_20260920/runtime-20260920-200144/01-catalog-Test_Main.png`
- 보고서: 같은 폴더 `report.json`
- 화면 1598×890, 제목줄 포함 PNG 1604×928
- 캡처 전후 맵·데이터·UI·설정 490개 파일 SHA-256 동일
- 실 캡처를 눈으로 검토했다. 오프스크린 위젯 좌표는 캐시가 없어 사용 불가로 표시했다.
- 검은 배경의 Saved/Automation PNG는 실제 저장 WBP를 검사용 데이터로 따로 렌더링한 화면이다.

## 노션 게시
원래 빈 페이지에 사용자 가이드를 게시했다. 제목, 16개 사용/검증 절, 변경 요약,
표 14개, 이미지 6개를 다시 읽어 확인했다. 원래 미승인 시안은 실제 실행 화면과 구분한다.

