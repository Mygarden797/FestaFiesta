# 시설 설치 가능 지역을 나누는 방법

기준: 2026-09-11, FestaFiesta / Unreal Engine 5.7.4, `Test_Main` 맵.

## 이번에 바꾼 규칙

**길 연결 여부를 시설 설치의 필수 조건에서 제외했다.** 이제 도로나 NavMesh가 없어도 구역·지면·겹침 등 나머지 배치 조건을 만족하면 시설을 설치하거나 이동할 수 있다.

`Config/DefaultGame.ini`에서 이 프로젝트의 기존 설정 하나를 변경했다.

```ini
[/Script/FF_ZoneSystem.FFZSDeveloperSettings]
bEnableNavigationProjectionChecks=False
```

에디터에서는 `Edit > Project Settings > Game > FF Zone System > Navigation > Enable Navigation Projection Checks`에 해당한다. 설정 파일 변경 후에는 에디터를 다시 열어 적용하고, 이미 만들어진 실행 파일에는 새 패키징 결과를 사용한다.

이전 메시지는 길 타일과 붙었는지만 검사해서 나온 것이 아니다. 시설의 입구·대기줄 자리·서비스 위치·출구를 **NavMesh에 올릴 수 있는지, 서로 완전한 경로로 연결되는지** 검사하던 규칙이다. 이 배치 검사를 해제했다. 관람객의 실제 길찾기, 시설의 장애물 처리, 대기줄 우회 비용은 그대로 사용한다.

NavMesh는 관람객이 보는 보행 지도다. 건축 허가는 관리자가 내고, 손님은 보행 지도를 보고 찾아오는 것으로 생각하면 된다. **설치 가능과 손님의 접근 가능은 서로 다른 조건**이다. 연결되지 않은 지형에 놓은 시설까지 손님이 자동으로 찾아오게 만드는 변경은 아니다.

## 현재 맵의 실제 구조

읽기 전용 검사에서 다음 상태를 확인했다. 맵을 저장하거나 경계를 바꾸지는 않았다.

| 구성 | 현재 상태 | 쉬운 설명 |
| --- | --- | --- |
| `BuildArea_Main` / `FFZSBuildArea` | 정확히 1개, 준비 완료 | 전체 건축 허가 도면을 관리하는 담당자 |
| `FFZSGameplayZone` | 31개 | 기존 구역 도면 31장. 관람객 소속·혼잡 계산에도 사용 |
| 통합 건설 영역 | 31개 구역의 실제 모양을 합친 영역 | 도면 사이의 내부 선을 지워 하나로 사용하는 것 |
| `FFZSProtectedArea` | 현재 배치된 액터 0개 | 별도 건축 금지 표시를 만들 수 있는 기능. 현재 맵에 금지 박스를 새로 추가한 것은 아님 |
| 구역별 시설 종류 허용 | 31개 모두 `Allow All Facility Categories = true` | 구역 쪽에서 시설 종류를 제한하지 않음. 시설 데이터의 개별 허용 구역은 별도 조건 |

근거: `Artifacts/Validation/PlacementRules/map-readonly.json`. 맵 SHA-256은 검사 전후 `6283c33e87927cb4a2d2591f44e98250edbde3e570218016da41492ce91f257c`로 같았다.

통합 영역은 모든 구역을 감싸는 큰 사각형이 아니다. **실제 구역들이 덮는 부분만 합치므로, 구역 사이 빈틈·구멍·외부는 계속 제외된다.** 서로 맞닿은 구역 사이의 내부 선은 시설이 넘어갈 수 있다.

## 설치 가능 여부를 판단하는 순서

축제장 도면 위에 부스와 줄 서는 공간을 그린 종이를 놓는다고 생각하면 된다.

`설치 가능한 자리 = 통합 건설 영역 안의 자리 - 금지 영역 - 이미 쓰는 공간 - 부적합한 지면`

위 식은 지역 판단을 이해하기 위한 설명이다. 실제 설치에는 시설 데이터의 유효성, 비용, 운영 단계, 설치 수 제한도 적용된다.

| 확인할 것 | 설치 가능한 예 | 설치할 수 없는 예 |
| --- | --- | --- |
| 전체 건축 범위 | 시설 본체·관람 공간·최대 대기줄이 모두 통합 영역 안 | 본체는 안에 있지만 대기줄 일부가 바깥이나 구멍에 걸림 |
| 금지·보호 영역 | `NoBuild`나 보호 동선과 겹치지 않음 | 금지 박스나 비워 두기로 한 통로를 침범 |
| 기존 점유 | 다른 시설의 본체·관람 공간·대기줄을 침범하지 않음 | 작은 부스라도 다른 부스 대기줄에 들어감 |
| 실제 장애물 | 벽·장애물과 겹치지 않음 | 허용 구역 안이어도 건물 벽과 겹침 |
| 바닥 | 시설 밑을 지지하는 바닥이 있고 경사·높이 차이가 기준 안 | 바닥 표본 누락, 급경사, 울퉁불퉁한 바닥 |
| 길·NavMesh 연결 | **이번 변경으로 설치 필수 조건 아님** | 길이 없다는 이유만으로는 거부하지 않음 |

**2026-10-07 안전 여유 제거:** 시설 둘레의 추가 안전 여유 거리를 배치·이동·경계·겹침 판정에서 제외했다. 기존 데이터의 `ClearanceCells` 값도 적용하지 않는다. 실제 본체·대기열·관람 공간과 보호 통로 검사는 유지된다.

일반 시설과 길의 설치 지면 기준은 최대 경사 **15도**, 시설 아래 표본의 최고·최저 높이 차이 **80cm**다. 두 조건을 모두 만족해야 한다. 기본적으로 100cm 간격 이하의 표본을 사용하며 모든 표본에서 바닥을 찾아야 한다. 1cm 저장 격자와 지면 검사 간격은 서로 다른 설정이다. 수치 변경과 적용 절차는 [시설 설치 지면 안내](../../Collaboration/FacilityGroundPlacement_UserGuide_KO.md)를 참고한다.

**2026-10-07 타일 규칙 추가:** 타일은 전용 기준인 89도·2000cm를 사용하고, 바닥 전체의 지면 최고점 위에 수평으로 놓인다. 건축 구역·금지 영역·점유 제한은 유지된다. 옆 타일과의 단차를 자동으로 없애거나 손님 이동 경로를 넓히지는 않는다. 메시 지면 식별용 `FFZS_TileGround` 태그와 적용 절차는 [타일 기초 바닥 안내](../../Collaboration/TileFoundation_UserGuide_KO.md)를 참고한다.

예를 들어 평평한 잔디가 통합 건설 영역 안에 있고 다른 시설과 겹치지 않으면 길 없이 설치할 수 있다. 같은 잔디라도 구역 바깥이면 설치할 수 없다. 평평해 보여도 충돌용 바닥이 없으면 지면 검사에서 거부될 수 있다.

## 설치 허용 범위를 넓히거나 줄이기

1. Content Browser에서 `/Game/FestaFiesta/Maps/Test_Main`을 연다.
2. World Outliner에서 수정할 `FFZSGameplayZone`을 선택한다. `Zone Id`로 담당 구역을 구분한다.
3. Components에서 `BoundarySpline`을 선택한다. 스플라인은 경계선을 이루는 꼭짓점들의 모음이다.
4. 원하는 꼭짓점을 움직여 영역을 넓히거나 줄인다. **Actor Scale은 1, 1, 1을 유지**한다. 이 프로젝트는 Scale 대신 스플라인 점으로 구역 크기를 조절한다.
5. 점이 최소 3개이고, 모든 점의 Type이 `Linear`이며 `Closed Loop`가 켜져 있는지 확인한다. 경계가 스스로 교차해서는 안 된다. 구역 Actor의 Pitch/Roll은 0, Yaw는 0/90/180/270도 규칙을 따른다.
6. World Outliner에서 `BuildArea_Main`을 선택한다.
7. Details의 `Rebuild From World Regions`를 누르고 `Boundary Status`를 확인한다. 플레이 시작 시에도 통합 경계를 다시 계산하지만, 편집 중에는 이 버튼으로 결과를 갱신한다.
8. 맵을 저장하고 Play에서 작은 시설과 큰 시설을 각각 경계 가까이에 놓아 본다. 대기줄까지 포함해서 확인한다.

`BuildArea_Main`을 확대·이동해서 허용 범위를 바꾸는 방식이 아니다. 이 액터의 범위는 기존 Gameplay Zone 경계에서 계산된다. `Source Region IDs`와 `Buildable Area (m²)`는 확인용 값이다.

기존 `Zone Id`를 바꾸거나 구역을 삭제해서 경계를 정리하면 관람객 소속·저장 데이터 참조도 영향을 받는다. 단순한 범위 조정에는 기존 ID를 유지하고 경계 점을 편집한다.

## 허용 범위 안에 건축 금지 지역 만들기

허용 구역 전체를 잘게 뜯어고치는 대신 **금지 스티커를 붙이는 방식**이 이해하기 쉽다.

1. Place Actors에서 `FFZS`를 검색해 `FFZSProtected Area` 표시명의 액터를 맵에 배치한다. 코드 클래스명은 `FFZSProtectedArea`다.
2. Details의 `Protected Area Id`에 고유한 이름을 넣는다. 예: `NB_Main_001`.
3. `Linked Zone Id`는 `None`으로 두거나 실제 존재하는 주변 구역 ID를 넣는다. 예: 그 장소의 `GZ_TestMain_01`. 값을 넣을 때 임의의 없는 ID를 쓰지 않는다.
4. `Area Type`을 **No Build**로 설정한다. 코드 값은 `NoBuild`다.
5. `AreaBounds`의 `Box Extent`와 액터 위치를 조절해 금지할 장소를 덮는다. Actor Scale은 **1, 1, 1**, Pitch/Roll은 **0**으로 유지하고 크기는 Box Extent로 바꾼다. Extent는 전체 길이의 절반이다. 예를 들어 X=250, Y=150이면 실제 박스의 가로·세로는 500cm × 300cm다. Z 범위도 실제 시설 높이와 겹치게 잡는다.
6. 맵을 저장하고 Play에서 박스 안·경계·바깥에 같은 시설을 놓아 결과를 비교한다.

현재처럼 통합 건설 영역이 있는 맵에서는 연결한 구역 ID가 달라도 실제로 금지 박스와 겹치면 보호된다. `Linked Zone Id`를 지정했다면 유효한 구역을 가리켜야 한다. 통합 건설 영역이 없는 맵에서는 ID를 지정한 보호 영역이 해당 구역에만 적용되며, `None`인 보호 영역은 구역에 관계없이 적용된다.

`GeneralRoute`, `EmergencyServiceRoute`, `AccessibilityRoute`는 비워 둘 동선용 유형이다. **길 연결을 설치 조건에서 뺐어도 보호 통로를 건물로 막지 못하게 하는 규칙은 유지된다.** 일반 건축 금지 구역을 만들 때는 의미가 명확한 `NoBuild`를 사용한다.

## 시설별로 허용 지역을 다르게 하고 싶다면

- 구역의 `Allow All Facility Categories`를 켜면 구역 쪽에서 모든 종류를 허용한다.
- 이 값을 끄면 `Allowed Facility Categories`에 선택한 종류만 해당 구역에서 허용한다.
- 시설 Definition의 `Facility Specification > Allowed Zone IDs`가 비어 있지 않으면 그 시설은 지정된 구역 ID에서만 허용된다.

예를 들어 휴식 공간에는 휴식 시설만 두고 싶으면 구역의 허용 종류를 제한한다. 특정 무대 하나만 특정 장소에 두고 싶으면 해당 시설의 `Allowed Zone Ids`를 사용한다. 허용 구역을 바꾸는 것과 시설 종류를 제한하는 것은 서로 다른 편집이다.

통합 영역에서 시설이 여러 구역에 걸치더라도 종류 제한과 시설의 허용 구역 목록은 **배치 명령의 기준 구역 한 곳**에 적용된다. 일반 배치에서는 위치를 기준으로 구역을 선택한다. 시설이 걸친 모든 구역의 종류 제한을 동시에 검사하는 방식은 아니다. 특정 장소를 시설 일부도 침범하지 못하게 막으려면 `No Build` 보호 영역을 사용한다.

## 자주 헷갈리는 점

**길이 있어야 설치하는 것과 길 위에 시설을 겹쳐 놓는 것은 다르다.** 길 시설 데이터의 `길 위 시설 설치 허용` / `bAllowFacilityPlacementOnTop`은 길 위에 다른 시설을 놓을 수 있는지를 정한다. 이번 변경은 길 연결 필수 조건만 해제하므로 이 노면 겹침 규칙은 그대로다.

**에디터의 Layers 이름을 Surface나 Obstacle로 바꾸는 것만으로 설치 허가가 바뀌지 않는다.** 현재 프로젝트는 그런 레이어 이름을 설치 규칙으로 읽지 않는다. 지면·장애물은 `Visibility` 물리 질의, 설치 범위는 구역과 BuildArea, 명시적 금지는 ProtectedArea가 담당한다.

**녹색 NavMesh 표시와 설치 가능 표시도 다르다.** 사람이 걸을 수 있는 지면이라도 경사·겹침·허용 범위 때문에 시설은 못 놓을 수 있다. 반대로 이번 설정에서는 NavMesh가 없어도 정상적인 건축 조건을 만족하면 설치할 수 있다.

## 확인할 코드와 설정

- `Config/DefaultGame.ini`: 프로젝트의 설치 검사 설정.
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSWorldSubsystem.cpp`: 범위·점유·보호 영역·지면·장애물·선택적 내비게이션 검사.
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Public/FF_FFZSBuildArea.h`: 통합 건설 영역과 에디터 갱신 버튼.
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Public/FF_FFZSZoneActor.h`: 개별 구역 경계와 시설 종류 허용.
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Public/FF_FFZSProtectedArea.h`: NoBuild 및 보호 통로.

## 변경 후 검증과 실행 파일

새 빌드 이름은 `FestaFiesta-Win64-SM5SM6-FreePlacement-20260911`이다. 기존 ZIP은 이전 규칙을 포함하므로 새 ZIP을 별도 폴더에 풀고 실행한다.

- ZIP: `Artifacts/Releases/FestaFiesta-Win64-SM5SM6-FreePlacement-20260911/FestaFiesta-Win64-SM5SM6-FreePlacement-20260911.zip`
- 전체 자동 검사 **761개 통과, 실패 0개**: FestaFiesta 648개 + FF_ZoneSystem 113개. 경고를 포함해 통과한 검사도 있다.
- 새 검사 `FestaFiesta.Building.Placement.NoRoadRequiredByProjectDefault`: 길·NavMesh 없이 서비스 시설 설치와 이동 성공, 기존 시설 겹침·구역 바깥·NoBuild 침범 거부를 확인했다. 같은 장소에서 내비게이션 설치 조건만 다시 켜면 거부되는지도 대조했다.
- 새 검사는 실제 지형이 없는 격리된 테스트 월드에서 지면·물리 충돌 검사만 일시 제외한다. 프로젝트 기본값에서 두 검사가 켜져 있는지도 별도로 확인하며, 검사가 끝나면 설정을 복원한다.
- 콘텐츠 DataValidation: 오류 **0개**, 경고 **1개**. 경고는 검증 전용 월드에 Queue 서브시스템이 없다는 경제 시스템 메시지다.
- Shipping 패키징과 패키지 검사 통과. 실제 **D3D11/SM5**, **D3D12/SM6** 엔진 시작 및 정상 종료 확인.
- 실제 배포 pak에서 `DefaultGame.ini` 한 파일을 별도 추출해 길 연결 검사 `False`, 지면·충돌·모든 지면 표본 검사 `True`를 확인했다. 추출 전후 원본 pak의 SHA-256이 같아 배포 파일을 변경하지 않았음도 확인했다. 근거: `Artifacts/Validation/PlacementRules/Config-20260911-113447-9951c9a7/report.json`.
- 그래픽 검사는 엔진 시작 범위다. 패키지에서 전체 게임 플레이를 수동으로 끝까지 진행한 결과를 뜻하지 않는다.

검증 보고서는 새 릴리스 폴더의 `Validation` 및 `Metadata/verification.json`에 있다. 설치 관련 지도 점검 근거는 `Artifacts/Validation/PlacementRules/map-readonly.json`이다.
