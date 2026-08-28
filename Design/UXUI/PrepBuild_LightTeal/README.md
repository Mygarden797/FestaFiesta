# Prep Build LightTeal UI 구현

## 결론

종태님이 만든 준비 단계 HUD의 `LightTeal` 톤앤매너를 유지하면서, 시설 카탈로그와 배치 조작 UI는 **Unreal UMG로 구현**하는 것이 현업 기준에 가장 적합하다. 웹 UI는 빠른 시안 검토용으로는 쓸 수 있지만 실제 게임 입력, 포커스, DPI, 패키징, 플랫폼 대응까지 고려하면 런타임 UI의 주 구현체로 사용하지 않는다.

기존 WBP, 이미지, 설정 파일은 덮어쓰기·이동·삭제하지 않았다. 실제 연동에 꼭 필요한 팀 소유 C++ 2개에는 기본값 `false`의 opt-in과 상태 이벤트만 가산적으로 추가했고, 기존 전체 화면 카탈로그 경로는 기본 동작으로 그대로 남겼다.

## 현재 산출물

- `Generated/FF_PrepBuild_CatalogOpen_LightTeal_v1.png`
  - 하단 도킹형 시설 카탈로그가 열린 상태의 콘셉트 시안
- `Generated/FF_PrepBuild_Placing_LightTeal_v1.png`
  - 시설을 선택한 뒤 월드 배치에 집중하는 축약 상태 시안
- `UMG_PRODUCTION_GUIDE.md`
  - UMG 계층, 데이터 흐름, 성능, QA, 개발 순서를 담은 구현 가이드
- `OWNER_INTEGRATION_PROPOSAL.md`
  - 팀 소유 파일에 적용한 최소 연동 지점과 회귀 안전장치 기록
- `IMAGEGEN_PROVENANCE.md`
  - 이미지 생성 목적, 프롬프트, 실제 에셋 사용 원칙
- `VALIDATION_REPORT.md`
  - 이미지·Unreal 빌드·자동화·에셋 무변경 재실행·Git 비삭제 검증 결과
- `Source/FestaFiesta/.../UI/PrepHUD/Build/`
  - 표현 규칙, 타일 데이터, 풀링 모델, 가상화 엔트리, 에디터 에셋 빌더와 자동화 테스트
- `Source/FestaFiesta/.../UI/PrepHUD/Panels/FF_PrepPanelBuild.*`
  - 기존 PrepHUD가 이름 규칙으로 찾는 신규 시설 배치 패널
- `Content/FestaFiesta/UI/PrepHUD/...`
  - `WBP_FF_PrepBuildTile`, `WBP_FF_PrepPanelBuild`, `PAL_FF_PrepBuildUI`, `PAL_FF_PrepBuildCategoryDependencies`
- `Saved/Automation/PrepBuildAllCategoriesFinal/index.json`
  - 전체 7종 카테고리 계약을 포함한 최종 자동화 테스트 7/7 성공 리포트
- `Saved/Automation/BuildModeRegressionAllCategoriesFinal/index.json`
  - 기존 BuildMode 상태 전환·회전 입력 회귀 테스트 2/2 성공 리포트

## 추천 운영 방식

| 구분 | 권장 방식 | 이유 |
|---|---|---|
| 실제 게임 UI | UMG / CommonUI | 입력 모드, 게임패드 포커스, DPI, 현지화, Cook 및 패키징과 직접 통합 |
| 카탈로그 데이터 | 기존 `UFF_BuildCatalogSubsystem` 재사용 | 시설 정의, 카테고리, 썸네일 로딩 로직 중복 방지 |
| 가격 표시 | `UFF_BuildCatalogEconomyReadModel`로 표시 시점에 투영 | 축제 상태에 따른 실제 비용을 일관되게 표시 |
| 배치 상태 | 기존 `UFF_BuildModeSubsystem` 재사용 | 배치 검증, 회전, 확정, 취소, 저장 로직을 새로 만들지 않음 |
| 웹 UI | 리뷰용 프로토타입에만 선택적 사용 | 시안 공유는 빠르지만 출시 런타임 계층으로는 이중 입력·상태 동기화 비용이 큼 |
| 생성 이미지 | 콘셉트와 빈 상태 일러스트에만 사용 | 가격, 상태, 텍스트는 UMG로 유지해야 DPI·현지화·접근성을 보장 |

## 디자인 핵심

- 기존 HUD의 밝고 차분한 종이 질감과 저채도 청록색을 이어 간다.
- 시설을 찾는 동안만 하단 패널을 열고, 배치를 시작하면 패널을 축약해 월드 시야를 확보한다.
- 화면 전체를 어둡게 덮는 모달 대신 기존 HUD와 연결된 하단 도킹 구조를 사용한다.
- 색상만으로 상태를 전달하지 않고 아이콘, 문구, 버튼 상태를 함께 사용한다.
- AI 시안의 문자는 방향 확인용이며 최종 제품의 텍스트, 숫자, 아이콘은 모두 UMG 위젯으로 다시 구성한다.

## 현재 완료 범위와 경계

LightTeal UMG 패널과 가상화 카드, **전체 7종 카테고리(부스·화장실·안내·휴게·공연·안전·포토스팟)**, 검색, 가격/잔액 상태, 비동기 썸네일, 카탈로그↔배치 축약 상태, 기존 BuildMode의 실제 `StartPlacement()` 연결, AlwaysCook 라벨까지 구현했다. 카테고리는 기존 WBP를 덮어쓰지 않고 공용 LightTeal 선택 칩 8개(전체 포함)를 가로 스크롤 행에 런타임 생성하며 선택 상태를 필터와 동기화한다. UI는 Tick 없이 카탈로그·썸네일·골드·BuildMode delegate로만 갱신된다.

`UFF_BuildModeSubsystem`에는 표현 선택만 담당하는 외부 카탈로그 opt-in과 상태 변경 delegate를 추가했다. opt-in 기본값은 `false`이며 비용·검증·회전·확정·취소·저장 로직은 분기하지 않았다. 따라서 기존 카탈로그 사용자는 새 패널을 열지 않는 한 이전 경로를 그대로 사용한다.

Unreal 5.7 Editor 빌드, 신규 UI 자동화 7/7, 기존 BuildMode 회귀 자동화 2/2를 통과했다. 에셋 생성 명령을 다시 실행해 WBP 2개와 PAL 2개가 모두 `REUSE`이고 SHA-256이 동일함도 확인했다. 검색 입력 중 `B/ㅠ` 단축키 충돌, 건축 불가 단계에서 빈 패널이 남는 문제, 기존 에셋 재사용 시 계약 오검출도 교차 리뷰에서 보강했다. 실제 맵에서의 해상도·DPI·게임패드·패키지 실행 체감 QA는 팀 통합 빌드에서 수행할 수동 체크로 남긴다.
