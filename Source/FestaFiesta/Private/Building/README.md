# 건설 코드와 테스트 위치

공개 계약은 `Source/FestaFiesta/Public/Building`, 구현은 이 폴더에서 관리한다. 파일명에서는 `Codex`를 제거했으며 Unreal 반영 타입 이름은 Blueprint·저장 데이터의 호환성을 위해 유지한다.

| 위치 | 책임 |
| --- | --- |
| 루트 | 건설 세션, 배치·그룹 편집, 시설 카탈로그, 강화와 이름 규칙 |
| `UI` | 시설 카탈로그 화면과 카드 |
| `Presentation` | 배치 표시, 도구 패널, 도움말과 UI 공용 자원 |
| `Synergy` | 시설 조합의 순수 평가와 월드 변경에 따른 캐시 갱신 |
| `FacilityCollection` | 시설 조합 도감의 발견·보상·저장과 화면 |
| `FacilityInfo` | 선택한 시설의 정보·조명·유료 프로그램 화면 |
| `FacilityRevenue` | 시설 수익 순위와 월드 배지 |
| `Testing` | `Test_Main`에서 사용하는 개발용 시설 수명주기 패널 |

자동화 코드는 검증 대상 폴더 아래 `Tests`에 둔다. 루트 기능의 테스트와 공유 실패 주입 관찰자는 `Building/Tests`에 있다. `Testing`의 패널은 실제 개발 화면이므로 자동화 코드와 구분하며, 패널을 검증하는 자동화만 `Testing/Tests`에 둔다.

에셋 생성·저장과 Widget Blueprint 제작은 `Source/FestaFiestaEditor/Private/Building`에서 수행한다. 조합 표 생성기와 CSV·참조·추가 작업 테스트도 에디터 모듈의 `Synergy`와 `Synergy/Tests`에서 관리한다. 게임 모듈이 에디터 모듈을 참조해서는 안 된다.

새 테스트는 기존 `FestaFiesta.Building.*` 경로를 사용한다. 파일 위치를 바꿀 때 자동화 이름과 에셋 경로를 함께 바꿀 필요는 없다. 비공개 헤더는 `Building/...`처럼 모듈 내부 경로를 명시해 테스트 이동 후에도 같은 구현을 참조하게 한다.
