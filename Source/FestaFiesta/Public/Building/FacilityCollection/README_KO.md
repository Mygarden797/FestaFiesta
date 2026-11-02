# 시설 조합 도감

FestaFiesta의 시설 조합 발견·수집 보상을 담당한다. 이전 `Building/Codex`와 `UI/FacilityCodex`의 코드는 이 기능 아래에 모았다.

| 위치 | 책임 |
| --- | --- |
| 이 폴더의 공개 헤더 | 조합 항목, 계산 모델, 설정, 발견·보상 API |
| `UI/` | 도감 화면, 카드, 미리보기, 비동기 썸네일, 화면 수명 관리 |
| `Private/Building/FacilityCollection/Tests/` | 발견 조건, 보상 중복 방지, 축제 슬롯 저장 검증 |
| `Private/Building/FacilityCollection/UI/Tests/` | 화면, 입력 복원, 썸네일·알림 회귀 검증 |
| `FestaFiestaEditor/Private/Building/FacilityCollection/` | 에디터 전용 위젯 제작·디자인 적용 명령 |

`Model`은 현재 배치를 계산하고 `ProgressSubsystem`은 축제 슬롯별 발견·수령 기록을 소유한다. `Subsystem`은 두 결과를 합쳐 읽기 모델을 공개하며 `UI/PresenterSubsystem`은 화면과 입력 소유권의 획득·반납을 담당한다. 시설 조합의 거리·중첩 규칙은 `Building/Synergy`의 수익 평가기를 공유한다.

코드의 `FF_FacilityCodex*` 반영 타입명은 기존 Blueprint·저장 데이터의 식별자이므로 유지한다. 파일명에서는 `Codex`를 제거하고 include와 생성 헤더 이름을 함께 갱신했다. Unreal 에셋 파일명은 AssetTools로 변경해 관련 패키지 참조를 저장하고 참조가 없는 리다이렉터를 정리했다. 타입명을 바꾸는 작업은 별도의 Core Redirect 및 에셋 이행 검증과 함께 진행한다.

기능을 수정할 때는 기존 `FestaFiesta.Building.Codex` 자동화 그룹과 이 기능의 `Tests`, `UI/Tests`에서 선언한 UI 자동화 그룹을 함께 확인한다. 테스트 이름은 자동 실행 스크립트의 호환성을 위해 유지한다.
