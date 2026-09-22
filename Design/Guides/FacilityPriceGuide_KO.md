# 시설 설치비 설정

시설 설치비는 현재 축제 정의의 `FacilityCostTable`에서 읽는다. 기본 프로젝트 비용표는
`/Game/FestaFiesta/Data/Economy/DT_FacilityCosts`이며 행 구조체는 `FFF_FacilityCostRow`다.

## 무료 시설 설정

1. 대상 시설 정의의 `Spec.InstallCostKey`를 확인한다. 값이 있으면 해당 행을 사용한다.
2. 키가 `None`이면 `FacilityDefinitionId`에 대응하는 `Cost.<대분류>.<소분류>.<고유이름>` 행을 사용한다.
   예: `Performance_MainStage_SunsetMainStage` → `Cost.Performance.MainStage.SunsetMainStage`.
3. 해당 행의 `InstallCost`를 `0`으로 저장한다. 재임포트 시 덮어쓰지 않도록 사용하는 원본 CSV도 같은 값으로 관리한다.
4. 실행 중인 세션을 다시 시작하여 가격을 새로 읽는다. 화면에는 `0 Gold`가 표시되고 잔액 0에서도 설치할 수 있다.

`InstallCostKey=None`이나 가격 행 삭제는 무료 설정이 아니다. 자동 키는 시설별 행 → 구형 소분류 행
→ `Cost.<대분류>.Default` 순서로 조회한다. 명시한 키에 해당하는 행이 없으면 카테고리 Default로만 폴백한다.
시설 하나만 무료로 만들 때는 공용 Default 행 대신 해당 시설의 행을 수정한다.

## 코드 유지보수 기준

- `FFF_FacilityCostResolver::FindInstallCost()`가 가격 조회와 검증을 함께 담당한다.
  성공은 0 이상의 가격과 실제 매칭 행을 보장한다. 선택된 행이 음수이면 다른 행으로 폴백하지 않고 실패한다.
- `IsValidInstallCost()`가 금액 범위를 정의한다. 0은 정상 무료 가격, 음수는 잘못된 설정이다.
- UI 카탈로그는 `FFF_BuildCatalogEntry::HasValidInstallCost()`를 사용한다.
  조회 성공 플래그와 금액을 함께 확인하므로 초기값 0이나 조회 실패를 무료로 오인하지 않는다.
- 새로운 화면에 `InstallCost > 0` 같은 별도 유효성 조건을 추가하지 않는다.
- 시설의 건축 한도, 배치 조건, 축제 상태 검증은 무료 시설에도 그대로 적용된다.

철거 환급과 시설 강화비는 설치비에 비례하므로 설치비가 0이면 둘 다 0이다.
관람객 이용 매출은 별도 수익 설정을 따른다.

회귀 검증은 `FacilityCostResolver.FreeAndInvalidPrices`, `ZoneSystemBridge.ZeroCostInstallsWithoutGoldAndRefundsZero`,
`ZoneSystemBridge.NegativeCostFailsClosed`, `PrepBuild.FreeInstallCost` 및 카드·툴팁·시설 정보 테스트에서 수행한다.
