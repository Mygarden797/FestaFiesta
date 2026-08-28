# 검증 보고서

- 검증일: 2026-08-28
- 범위: LightTeal 시안, 신규 PrepBuild C++/WBP/PAL, 최소 BuildMode opt-in, Notion 가이드
- 원칙: 팀 파일 삭제·이동·덮어쓰기 금지, 기존 동작은 opt-in 기본값 `false`로 보존

## 결과

| 항목 | 결과 |
|---|---|
| 원본 시안 2종 디코딩 | PASS — 각 1672×941 PNG |
| Notion 프리뷰 2종 디코딩 | PASS — 각 640×360 JPEG |
| HTML 시각 검토 자료 | PASS — 86,838 bytes, 200 KiB 제한 이내 |
| HTML 내 이미지 | PASS — JPEG data URI 2개, 로컬 `file://` 참조 0개 |
| HTML 생성 스크립트 문법 | PASS — Node `--check` |
| 문서의 프로젝트 참조 경로 | PASS — 실제 `Public/Building`, `Private/Building`, `UI/PrepHUD` 경로 확인 |
| Notion 제목 | PASS — `시설 배치 UI · LightTeal UMG 개발 가이드` |
| Notion 시각 자료 첨부 | PASS — HTML embed 확인 |
| Notion 핵심 섹션 | PASS — UMG 결정, 팀 파일 보호, QA, 실제 소스 경로 확인 |
| Unreal 5.7 Editor 빌드 | PASS — UHT, 신규 소스·테스트 컴파일, 모듈 링크 성공 |
| PrepBuild 자동화 | PASS — 전체 7종 카테고리 계약 포함 7/7 성공, 경고 포함 성공 0, 실패 0, 미실행 0 |
| 기존 BuildMode 회귀 자동화 | PASS — 상태 전환·회전 입력 2/2 성공 |
| 전체 카테고리 계약 | PASS — 부스·화장실·안내·휴게·공연·안전·포토스팟 순서·한글 라벨·각 필터 검증 |
| 카테고리 반응형 UI | PASS — `전체 + 7종` LightTeal 선택 칩, 가로 ScrollBox, 필터 선택 상태 동기화 |
| 카테고리 재진입·포커스 | PASS — Construct/Destruct 재바인딩, 선택 칩 자동 스크롤, CatalogOpen 포커스 복원 |
| 부분 생성 실패 | PASS — 동적 행 전체 롤백 후 기존 버튼 유지, 불완전·중복 행 방지 |
| WBP 위젯 계약 | PASS — 필수 named widget, 전용 TileView, entry class 로드 성공 |
| Cook 계약 | PASS — WBP 2개와 공용 ChoiceButton 의존성을 별도 PrimaryAssetLabel로 명시, 모두 `AlwaysCook` |
| 레거시 기본값 | PASS — external catalog presentation 기본값 `false` 자동화 검증 |
| 검색 단축키 충돌 | PASS — 편집 텍스트 포커스 중 `B/ㅠ`가 BuildMode 토글로 소비되지 않음 |
| 열기 거절 처리 | PASS — 단계/존 정책상 CatalogOpen 실패 시 빈 헤더 패널 대신 닫기 요청 |
| 생성기 재실행 | PASS — WBP 2개와 PAL 2개 모두 `REUSE`; 수정하지 않음 |
| 재실행 해시 | PASS — 실행 전후 네 에셋 SHA-256 동일 |
| Git 비삭제 | PASS — tracked `D`/`R` 0건, `git ls-files --deleted` 출력 0건 |
| 기존 팀 에셋/설정 변경 | PASS — 기존 `.uasset`, 이미지, Config 수정 0건 |
| 기존 C++ 변경 | REVIEW — BuildMode `.h/.cpp` 2개에만 opt-in/event 가산 변경 |
| 동시 작업 보존 | PASS — 별도 변경 중인 `FF_LiveHudMenuBar.h`와 `.claude/settings.local.json`은 읽거나 수정하지 않고 작업 범위에서 제외 |

## 원본 이미지 무결성

| 파일 | SHA-256 |
|---|---|
| `FF_PrepBuild_CatalogOpen_LightTeal_v1.png` | `CD717011B2736615408465C86956E501A6F7B3D31CCC6F3EF19EA88106436B8F` |
| `FF_PrepBuild_Placing_LightTeal_v1.png` | `E981674609E4698C18111AA2FFFFF93E5B2E7503881C2D8BBAC4A8A3212E005F` |

## 생성 UMG 에셋 무결성

| 파일 | SHA-256 |
|---|---|
| `WBP_FF_PrepBuildTile.uasset` | `E621E7ECEC5F03951224976F64748B2C1700169977E2270410C591B604D0072D` |
| `WBP_FF_PrepPanelBuild.uasset` | `2955EE29AB5D570E635ADF0A0EC6B298F0A7E35BC158B0D4F2C9D9A2E574C354` |
| `PAL_FF_PrepBuildUI.uasset` | `1A6CE0D614550FBD64CF4728AE28E18CCDA8FA29D60CE4935107F42A2082A58D` |
| `PAL_FF_PrepBuildCategoryDependencies.uasset` | `C77DB9AC23597ED860E571EFF42AFD1DB5CC9EF923E1A980B10DACD361094E16` |

최종 자동화 리포트:

- `Saved/Automation/PrepBuildAllCategoriesFinal/index.json` — 신규 UI 및 전체 카테고리 7/7
- `Saved/Automation/BuildModeRegressionAllCategoriesFinal/index.json` — 기존 BuildMode 2/2

## 현재 제한

헤드리스 `NullRHI` 자동화는 코드·에셋·Cook 계약을 검증했지만 실제 맵의 시각적 PIE, 여러 DPI/화면비, 게임패드 포커스, Development 패키지 실행 성능을 대신하지 않는다. 이 항목은 팀 통합 QA 체크리스트에 남긴다. 현재 작업은 테스트를 통과한 구현본이며 패키징 포함 최종 출시는 수동 QA 이후 판단한다.

작업 범위인 BuildMode 2개와 신규 PrepBuild 소스는 `diff --check` 및 후행 공백 검사를 통과했다. 저장소 전체 검사에서는 동시 작업 파일 `FF_LiveHudMenuBar.h`의 기존 후행 공백 1건이 보였으나, 사용자 지시에 따라 해당 팀원 파일을 수정하지 않았다.
