# 건축용 조명

건축 카탈로그에서 조명 색상·밝기를 조절할 수 있는 시설은 **전구 아이콘**으로 표시한다.
해당 시설이 처음 보일 때 기능과 `설치한 시설 선택 → 조명` 경로를 한 번 안내한다.
`확인`을 누른 기록은 사용자 설정에 저장하며 기존 시설 정보의 조명 소개 기록과 공유한다.

건축 메뉴의 **조명**에서 다음 시설을 선택해 배치한다. 설치된 시설을 선택하면
**조명 설정**에서 전원, 밝기(0~500%), 색상 프리셋/직접 입력, 기본값 복원을 사용할 수 있다.
이동·회전과 저장/불러오기에도 각 시설의 설정이 유지된다.

시설 정보의 기본 화면에는 조명 리그가 있는 메인 공연장과 조명 시설에
**조명 색상 변경** 버튼이 표시된다. 처음에는 빨간 `!`로 강조하며, 이 버튼이나
기존 조명 탭을 누르면 색상 선택·실시간 미리보기·적용·취소 방법을 안내한다.
**색상 바꾸기**를 누르면 편집 화면으로 이어진다. 안내를 연 뒤에는 빨간 강조를
다시 표시하지 않으며, 다른 시설 선택·화면 재생성·게임 재시작에도 유지된다.
확인 이력은 사용자 `GameUserSettings.ini`의 `[FestaFiesta.FacilityLighting]`
`bHasSeenColorIntroduction`에 저장한다. 조명이 없는 시설에는 버튼을 표시하지 않는다.

첫 사용 안내 WBP를 재적용하려면 에디터 모듈 빌드 후 Entry 맵에서
`Tools/FacilityLighting/apply_introduction.py`를 실행한다. 기존 패널을 백업하고
안내 위젯만 추가하며, 두 번째 실행은 이미 적용한 에셋을 변경하지 않는다.

| 소분류 | 에셋 | 표시 이름 |
|---|---|---|
| 장식 조명 | CurtainArch | 커튼 아치 조명 |
| 장식 조명 | Festoon | 페스툰 조명 |
| 장식 조명 | GoldenTree | 골든 트리 조명 |
| 장식 조명 | StarTree | 스타 트리 조명 |
| 펜던트 조명 | RattanPendant | 라탄 펜던트 조명 |
| 가로등 | FestivalStreetlamp | 축제 가로등 |

정의는 `/Game/FestaFiesta/Building/FF_Data_Building/Lighting`의 PDA에서 관리한다.
가격은 기존 시설과 동일하게 `DT_FacilityCosts`에서 조회하며 추가 6종의 초기 가격은
각 300 Gold다. 재수입용 원본은 `data/unreal/facility_costs_v0.csv`다.
조명은 고객 서비스·관람·대기열을 사용하지 않으므로 정원은 0, 관련 배열은 비운다.

각 원본 폴더의 `BP_FF_<이름>_Lighting`에서 광원의 위치·색온도·기본 밝기를 편집할 수 있다.
시설 메시의 발광 재질은 다음 파라미터로 각 설치 인스턴스의 실제 광원과 함께 제어된다.
원본 강도·색상·텍스처 마스크는 보존된다.

- `FF_LightingBrightness`: 절대 밝기 배수, 기본 1
- `FF_LightingColor`: 사용자 지정 RGB
- `FF_LightingUseColorOverride`: 원래 색상 0 / 사용자 색상 1

프로젝트를 빌드한 뒤 기존 조명 에셋과 썸네일을 갱신·검증하는 명령
(각 PDA의 `CatalogThumbnail`이 기존 `Thumbnails/T_UI_Lighting_*`를 참조해야 한다):

```powershell
& Tools/FacilityLighting/Run-EditorScript.ps1 -Script Tools/FacilityLighting/create_buildable_lights.py -Label buildable_create
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --factory-startup --python-exit-code 1 --python Tools/FacilityLighting/render_catalog_thumbnails.py
& Tools/FacilityLighting/Run-EditorScript.ps1 -Script Tools/FacilityLighting/import_catalog_thumbnails.py -Label thumbnail_import -NullRHI
& Tools/FacilityLighting/Run-EditorScript.ps1 -Script Tools/FacilityLighting/capture_buildable_lights.py -Label buildable_capture
& Tools/FacilityLighting/Run-EditorScript.ps1 -Script Tools/FacilityLighting/verify_buildable_lights.py -Label buildable_verify
& Tools/FacilityLighting/Run-LightingTests.ps1 -Filter 'FF_ZoneSystem.Lighting.BuildCatalogAssets+FF_ZoneSystem.Lighting.EmissiveIsolationAndMaterialReplacement+FestaFiesta.Building.Catalog.ProductionLightingAssetsAndEconomy' -Label buildable_regression
```

생성 스크립트를 다시 실행해도 기존 PDA 설정, 리그 배치와 가격은 유지된다.
카탈로그 썸네일은 기존 시설과 같은 직교 카메라, 밝은 스튜디오 조명, 투명 배경의
1024×1024 RGBA PNG로 생성한다. 렌더 스크립트는 로컬 제작 원본 `SourceArt`와
`Saved/StreetlightProduction_20260909`를 읽으며 원본 모델을 저장하지 않는다.
PNG 원본은 조명 `Thumbnails` 폴더에 함께 보관하므로 재수입에는 Blender 원본이 필요 없다.
수입 스크립트는 기존 시설 이미지의 텍스처 설정을 복사하고 조명 텍스처 6개만 저장한다.
검증 결과는 `Saved/FacilityLighting_20260911`에 저장한다.
`FF_LIGHTING_VERIFY_ONLY=1` 환경 변수를 설정하고 수입 스크립트를 새 에디터 세션에서
실행하면 저장된 크기·설정·원본 해시·시설 참조를 읽기 전용으로 검증한다.

캡처 스크립트는 실제 메시와 리그의 개별 미리보기 및 점등/소등/색상 변경 비교 이미지를
갱신한다. 이 검증용 배경 이미지는 카탈로그 텍스처를 덮어쓰지 않는다.
결과와 변경 전 에셋 백업은 `Saved/FacilityLighting_20260910`에 저장한다.

## 메인 무대의 원본 조명과 설치용 조명

`BP_MainStage_Grand`는 원본 미리보기용 스포트라이트 10개를 유지한다.
시설 설치에 사용하는 `BP_MainStage_Grand_Lighting`은 아티스트의
`67483cfb` 변경(2026-09-10, 공연장 조명 변경)의 무대 조명 **9개**에
관객용 워시 조명 **2개**를 추가해 총 **11개**다.
해당 변경에서 `Overhead_08_Spot`이 제거되었으므로 원본 개수에 맞추기 위해
다시 추가하지 않는다. 현재 설치용 광원은 다음과 같다.

- 천장 5개: `Overhead_00_Spot`, `Overhead_02_Spot`, `Overhead_04_Spot`, `Overhead_06_Spot`, `Overhead_10_Spot`
- 데크 2개: `DeckSpot_Spot`, `DeckSpot_Spot_001`
- 런웨이 2개: `RunwaySpot_Spot`, `RunwaySpot_Spot_001`
- 관객용 2개: `AudienceWash_Left_Spot`, `AudienceWash_Right_Spot`

`create_assets.py`는 기존 리그와 디자이너 배치를 보존하며, 리그가 없을 때도
무대 조명 9개만 복사한 뒤 `mainstage_audience_lights.py`로 누락된 관객 조명을 추가한다.
기존 관객 조명의 디자이너 수정값도 보존한다. `verify_assets.py`와 `ProductionMainStageInstallation`은
개수뿐 아니라 정확한 광원 이름을 검증한다. 설치·밝기·전원·색상·저장 복원·철거
검증도 계속 수행한다. 아티스트가 구성 자체를 바꾸면 변경 근거와 함께 세 곳의
기대 이름을 갱신한다. 조명 설정값은 기존 리그 에셋이 기준이다.

관객 조명은 현재 PDA의 500자리(25열 × 20줄), 시설 로컬 Y=1212.5~3492.5cm를
비춘다. 무대 앞 상단에서 양쪽 관객 영역을 향하며 내부/외부 반각은 45°/55°,
원본 감쇠 반경은 10000cm다. 설치 시 메시 배율을 적용한 반경은 약 52m다.
각 120000lm의 옅은 청색 광원이며 군중 그림자 패스 비용을 줄이기 위해 그림자는 끈다.
시설의 전원·밝기·색상·복원 제어가 기존 조명과 함께 적용된다.

기존 리그에 관객 조명만 추가하려면 UE 5.7에서 `mainstage_audience_lights.py`를 실행한다.
반복 실행은 기존 조명을 변경하지 않는다. `ProductionMainStageInstallation`은 실제 설치된
리그가 모든 관객 자리의 몸통과 머리를 비추는지 및 이동·회전 후 제어를 검증한다.
`capture_mainstage_audience.py`는 Entry 임시 월드에서 실제 관객 메시 63개를 좌석에
샘플 배치하고, EV100=4의 동일 노출로 관객 조명 전후를 촬영한다. 결과는
`Saved/MainStageAudienceLighting/before.png`, `after.png`이며 실제 게임 군중 시뮬레이션은 아니다.
GPU와 `-RenderOffscreen`이 필요하고 에셋이나 맵을 저장하지 않는다.
