# 건축용 조명

건축 메뉴의 **조명**에서 다음 시설을 선택해 배치한다. 설치된 시설을 선택하면
**조명 설정**에서 전원, 밝기(0~500%), 색상 프리셋/직접 입력, 기본값 복원을 사용할 수 있다.
이동·회전과 저장/불러오기에도 각 시설의 설정이 유지된다.

| 소분류 | 에셋 | 표시 이름 |
|---|---|---|
| 장식 조명 | CurtainArch | 커튼 아치 조명 |
| 장식 조명 | Festoon | 페스툰 조명 |
| 장식 조명 | GoldenTree | 골든 트리 조명 |
| 장식 조명 | StarTree | 스타 트리 조명 |
| 펜던트 조명 | RattanPendant | 라탄 펜던트 조명 |
| 가로등 | FestivalStreetlamp | 축제 가로등 |

정의는 `/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Lighting`의 PDA에서 관리한다.
가격은 기존 시설과 동일하게 `DT_FacilityCosts`에서 조회하며 추가 6종의 초기 가격은
각 300 Gold다. 재수입용 원본은 `data/unreal/facility_costs_v0.csv`다.
조명은 고객 서비스·관람·대기열을 사용하지 않으므로 정원은 0, 관련 배열은 비운다.

각 원본 폴더의 `BP_FF_<이름>_Lighting`에서 광원의 위치·색온도·기본 밝기를 편집할 수 있다.
시설 메시의 발광 재질은 다음 파라미터로 각 설치 인스턴스의 실제 광원과 함께 제어된다.
원본 강도·색상·텍스처 마스크는 보존된다.

- `FF_LightingBrightness`: 절대 밝기 배수, 기본 1
- `FF_LightingColor`: 사용자 지정 RGB
- `FF_LightingUseColorOverride`: 원래 색상 0 / 사용자 색상 1

프로젝트를 빌드한 뒤 저장된 에셋을 생성·검증하는 명령:

```powershell
& Tools/FacilityLighting/Run-EditorScript.ps1 -Script Tools/FacilityLighting/create_buildable_lights.py -Label buildable_create
& Tools/FacilityLighting/Run-EditorScript.ps1 -Script Tools/FacilityLighting/capture_buildable_lights.py -Label buildable_capture
& Tools/FacilityLighting/Run-EditorScript.ps1 -Script Tools/FacilityLighting/verify_buildable_lights.py -Label buildable_verify
& Tools/FacilityLighting/Run-LightingTests.ps1 -Filter 'FF_ZoneSystem.Lighting.BuildCatalogAssets+FF_ZoneSystem.Lighting.EmissiveIsolationAndMaterialReplacement+FestaFiesta.Building.Catalog.ProductionLightingAssetsAndEconomy' -Label buildable_regression
```

생성 스크립트를 다시 실행해도 기존 PDA 설정, 리그 배치와 가격은 유지된다.
캡처 스크립트는 실제 메시와 리그로 6개 썸네일 및 점등/소등/색상 변경 비교 이미지를 갱신한다.
결과와 변경 전 에셋 백업은 `Saved/FacilityLighting_20260910`에 저장한다.
