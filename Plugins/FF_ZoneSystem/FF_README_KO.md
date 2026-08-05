# FF Zone System 완전 가이드 — 설치·설계·통합·검증·운영

> 문서 정본: `Plugins/FF_ZoneSystem/FF_README_KO.md`  
> 대상: 기획자, 레벨·UI·AI 담당, Blueprint·C++ 개발자, QA, 패키징 담당  
> 버전: `2.0.0` — 1cm Cell/압축 Region 스키마  
> 엔진 기준: Unreal Engine `5.7.4` 설치본, 프로젝트 연결값 `5.7`  
> 현재 검증일: **2026-08-05** — Editor Development, Game Development, Game Shipping 빌드 성공; 자동화 `20/20` 성공, 실패 `0`, 종료 코드 `0`  
> 아직 미검증: 최신 소스의 독립 BuildPlugin, Cook/Stage/Pak/IoStore, 패키지 실행, 실제 Nav·NPC·운영 UI·다대포 맵 전체 흐름  
> 고정 단위: `1uu=1cm`, Festa 배치 Grid `1 Cell=1cm=1uu`

이 문서는 `FF_ZoneSystem`의 의도한 경험부터 설치, 데이터 저작, 런타임 흐름, 외부 시스템
연결, 저장·이관, 테스트, 패키징, 운영 점검까지 한 파일에서 확인하는 유일한 최신 안내서다.
“코드에 존재”, “자동 검증”, “수동·운영 검증 대기”를 구분해 구현 결과를 과장하지 않는다.

### 문서 사용 순서

- 처음 설치·실행: [1. 목적과 P0 경계](#1-목적과-p0-경계), [2. 정본과 공존 상태](#2-정본과-기존-플러그인-공존-상태)
- 구조·데이터·배치: [3. 모듈 구조](#3-모듈-구조)부터 [10. 배치 검증](#10-배치-검증-순서)
- 명령·Policy·Queue·저장: [11. 명령](#11-명령과-낙관적-동시성)부터 [19. 성능](#19-성능-원칙)
- 현재 증거·재현: [20. 자동화](#20-자동화-테스트-현황)부터 [22. 빌드·패키징](#22-빌드자동화패키징-명령)
- 이식·스키마 변환: [23. 다른 프로젝트 이식](#23-다른-프로젝트-이식), [24. 마이그레이션](#24-마이그레이션-계약)
- 출시 판단: [25. QA Gate](#25-qa-gate), [26. 제한 사항](#26-현재-제한-사항과-후속-작업), [27. 증거](#27-완료-보고서에-들어갈-증거)
- 연관 기준: [28. 관련 문서](#28-관련-문서)

---

## 1. 목적과 P0 경계

시스템이 증명할 경험:

```text
플레이어가 시설을 놓는다
→ 배치가 공간·통행·Queue에 미치는 영향을 미리 이해한다
→ NPC가 선택·이동·FIFO 대기·서비스·Exit를 수행한다
→ 대기·혼잡·운영 결과가 Read Model과 이벤트로 보인다
→ 플레이어가 원인을 읽고 다음 배치를 수정한다
```

P0는 시설 종류를 많이 만드는 작업이 아니다. `GZ_DadaPocha`, 음식 트럭, 화장실, 고정 Seed
NPC 20명으로 위 인과를 끝까지 연결하고, 100명·300명으로 기능과 성능을 확장하는 작업이다.

플레이어 권리:

- 미리보기와 실제 실행 결과가 설명 가능해야 한다.
- 실패 시 원인과 해결 행동을 제공한다.
- 명령 실패가 비용이나 월드 상태를 부분 변경하지 않는다.
- Ground 관람을 강제 보상 경로로 만들지 않는다.
- ESC·옵션에서 정지하고 안전하게 중단할 수 있어야 한다.

### 1.1 처음 실행하는 사람을 위한 빠른 시작

#### A. 프로젝트 연결 확인

1. Unreal Editor를 닫는다.
2. 플러그인 루트가 `Plugins/FF_ZoneSystem/FF_ZoneSystem.uplugin`인지 확인한다.
3. `FestaFiesta.uproject`의 `Plugins` 배열에서 `FF_ZoneSystem`이 `Enabled=true`인지 확인한다.
4. 프로젝트 C++가 플러그인 API를 직접 사용하면 `Source/FestaFiesta/FestaFiesta.Build.cs`의 Runtime 의존성에 `"FF_ZoneSystem"`을 둔다.
5. 다음 Editor 빌드가 성공해야 Editor에서 타입과 Blueprint 노드가 보인다.

```powershell
$ffzsUE = 'C:\Program Files\Epic Games\UE_5.7'
$ffzsProjectRoot = (Resolve-Path -LiteralPath '.').Path
& "$ffzsUE\Engine\Build\BatchFiles\Build.bat" `
  FestaFiestaEditor Win64 Development `
  -Project="$ffzsProjectRoot\FestaFiesta.uproject" `
  -WaitMutex -NoHotReload
```

#### B. 준비된 개발 Slice 실행

1. Editor에서 `/Game/FestaFiesta/FFZoneSystem/Tests/Maps/L_FFZS_PlacementTest`를 연다.
2. **Edit → Project Settings → Game → FF Zone System**에서 고정 표시값 `Placement Cell Size Cm = 1`을 확인한다.
3. **Play**를 누른다. 프로젝트 설정의 map allow-list 때문에 배치 도구는 이 테스트 맵에서만 자동 활성화된다.
4. 화면 안내에 따라 시설 선택, 회전, 설치, 철거를 시험한다.
5. 초록 Preview는 “현재 시점의 검증 통과”일 뿐이다. 커밋은 최신 Revision·공간·Policy를 다시 검증한다.
6. `OnPlacementRejected`의 안정 Enum과 설명을 확인하고, 문자열 비교로 실패 원인을 분기하지 않는다.

이 Slice는 개발용 입력·C++ Widget·배치 확인 도구다. 운영 UMG, 실제 Economy, 실제 Crowd/NPC,
Production Nav·Safety 정책, 멀티플레이 권위를 대신하지 않는다.

#### C. 빈 맵에서 첫 Zone과 시설 만들기

1. 빈 Level에 `FFZSGameplayZone`을 배치한다.
2. **Details → Zone → Presets**에서 Accepted Zone ID를 적용하거나 프로젝트 Stable `ZoneId`를 입력한다.
3. `BoundarySpline`을 닫힌 선형 단순 폴리곤으로 편집한다. Actor Scale은 `1,1,1`, Pitch/Roll은 `0`, Yaw는 90도 단위, 원점은 1cm 격자에 맞춘다.
4. 설치 금지·통행 공간에는 `FFZSProtectedArea`를 배치한다. `LinkedZoneId=NAME_None`이면 모든 Zone에 적용되는 전역 보호영역이고, 값이 있으면 해당 Zone 전용이다.
5. Content Browser에서 **Miscellaneous → Data Asset → FFZSFacilityDefinition**을 만들고 `Spec.SchemaVersion=2`를 확인한다.
6. 크기는 실제 cm 수를 Cell 수로 적는다. 6m×3m는 `FootprintSizeCells=(600,300)`, 1m 여유는 `ClearanceCells=100`이다.
7. Queue 예약은 deprecated `QueueReservedCells`가 아니라 반열림 `QueueReservedRects [Min,Max)`로 작성한다.
8. Entry·Service·Exit Anchor와 QueueSpline을 작성하고 **Asset Actions → Validate Assets**로 모든 오류를 해결한다.
9. `FFZSPlacementPreviewActor`에 Install 명령을 설정해 `RefreshPreview`로 읽기 검증하고 `CommitPreview`로 실행한다.
10. 일반 Blueprint/C++ 변경은 `UFFZSWorldSubsystem::ExecuteCommand` 한 경로만 사용한다. FacilityActor나 QueueComponent의 내부 상태를 직접 바꾸지 않는다.

#### D. 첫 성공 뒤 확인할 최소 항목

| 확인 | 성공 기준 |
|---|---|
| Zone 등록 | `FindZone(ZoneId)`가 같은 Actor를 반환 |
| 제작 데이터 | `IsDefinitionValid` 오류 0 |
| 통합 준비 | Prototype은 필요한 Policy만, Production Gate는 6개 도메인 정확히 1개 |
| 설치 | `bSucceeded=true`, Revision 1 증가, `OnWorldChanged` 1회 |
| 중복 replay | 같은 CommandId·같은 payload는 새 변경 없이 저장 결과 반환 |
| Queue | Join → Slot 도착 → Service 도착 → 서비스 → Exit 도착 순 |
| 저장 | Snapshot v4 캡처·복원 후 Agent Actor를 Stable AgentId로 Rebind |
| 사용자 피드백 | 실패 Enum, 원인, 다음 해결 행동을 표시 |
| 검증 | Editor/Development/Shipping 빌드와 자동화 20/20 |

---

## 2. 정본과 기존 플러그인 공존 상태

현재 저장소의 정본 플러그인은 `Plugins/FF_ZoneSystem` 하나다.
`FestaFiesta.Build.cs`와 `FestaFiesta.uproject`도 이 Runtime 모듈을 참조한다.
중복 `ZoneFacilityPlaytest` 소스·설정·맵·재질·생성 스크립트와 100cm 레거시 문서는 제거했다.

그러나 소스 삭제와 직렬화 자산 이관은 다른 작업이다. 기본 맵
`/Game/FestaFiesta/Maps/L_Dadaepo_DigitalTwin`에는 아직 `/Script/ZoneFacilitySystem`과
`ZFFacilityActor_1` export가 남아 최신 자동화 시작 로그에서 로드 경고가 발생한다.
이 맵은 프로젝트 기본 Game/Editor 맵이므로 삭제하거나, 누락 Actor가 열린 상태에서 무심코
저장하지 않는다. 먼저 백업·Reference Viewer·Asset Audit으로 참조를 식별하고 다음 순서로
이관한다.

1. 구 class·property·ID와 새 `AFFZSGameplayZone`·`AFFZSFacilityActor`·Spec v2의 명시적 변환표를 승인한다.
2. 필요한 경우 한정된 Core Redirect를 작성하고 Editor 로드 로그로 실제 적용 범위를 확인한다.
3. 별도 복제 맵에서 Actor·Data Asset·SaveGame을 변환한다.
4. `UFFZSWorldSubsystem` 명령 한 경로로 설치·Queue·저장 흐름을 재연결한다.
5. Reference Viewer와 로그에서 레거시 script import가 0인지 확인한 뒤에만 정본 맵을 저장한다.
6. Editor·Development·Shipping 빌드, 자동화, 대표 Slice, 20→100→300명 Gate를 다시 검증한다.

Core Redirect나 승인된 Save migration 없이 이름만 비슷한 새 타입으로 자동 추측 변환하지 않는다.

### 2.1 정리 manifest — 2026-08-05

삭제:

- `Source/FestaFiesta/ZoneFacilityPlaytest/`, `Content/FestaFiesta/ZoneFacility/`, 구 생성 Script와 설정 block
- `Docs/ZONE_FACILITY_*`, 관련 PNG, 분산 `Docs/FF_ZONE_SYSTEM_*.md`; 유효 내용은 이 정본에 흡수
- 구 패키지·Cook 결과만 있던 Git 미추적 `BuildArtifacts/`
- 외부 참조 0인 `Maps/test.umap`, `Maps/wraewqwdasdasadwqd.umap`
- 재생성 가능한 `Plugins/FF_ZoneSystem/Binaries/`, `Intermediate/`

보존:

- `Plugins/FF_ZoneSystem`의 `Source/`, `Config/`, `.uplugin`, `FF_README_KO.md`
- 현재 Slice의 `Content/FestaFiesta/FFZoneSystem/`, `Source/FestaFiesta/FFZoneSystemPlaytest/`,
  `Content/Python/build_ff_zone_system_playtest.py`
- 프로젝트 기본 맵 `L_Dadaepo_DigitalTwin.umap`과 기존 사용자 변경; 레거시 import 이관 전 재저장 금지

`Binaries/Intermediate/BuildArtifacts`는 source distribution에 넣지 않고 필요할 때 빈 경로에
재생성한다. `Saved` 로그는 검증 증거로만 로컬 보관하며 배포물·정본으로 취급하지 않는다.

삭제 항목은 대부분 Git 미추적이어서 Git으로 복구할 수 없다. 다대포 맵 이관에 정확한 구
`ZoneFacilitySystem` schema가 필요하면 별도 백업이나 과거 패키지를 외부에서 확보한다.

---

## 3. 모듈 구조

```text
FF_ZoneSystem.uplugin
├─ Config/DefaultFF_ZoneSystem.ini  출하 기본값
├─ Config/FilterPlugin.ini          패키지 포함 규칙
└─ Source/
   ├─ FF_ZoneSystem       Runtime, LoadingPhase=Default
   └─ FF_ZoneSystemTests  Editor, LoadingPhase=PostEngineInit
```

### 파일 이름 계약

직접 작성하는 C++·문서 파일의 basename은 한 번만 `FF_`로 시작한다. UAT와 Unreal Config가
고정 이름으로 찾는 `Config/FilterPlugin.ini`, `Config/DefaultFF_ZoneSystem.ini`는 예약 예외다.
`Binaries`·`Intermediate`는 UBT/UHT 생성물이므로 직접 이름을 바꾸지 않고 필요할 때
재생성한다.

`AFFZS*`, `UFFZS*`, `FFFZS*`, `EFFZS*`, 모듈 `FF_ZoneSystem`,
`/Script/FF_ZoneSystem`은 현재 직렬화·API 정체성이다. 이름 변경은 Blueprint, Data Asset,
SaveGame, Cook 결과에 영향을 주므로 Core Redirect·자산 재저장·전체 Cook을 같은 변경 단위에서
검증한다.

`.uplugin`의 현재 값은 `Version=2`, `VersionName=2.0.0`,
`CanContainContent=true`, `Installed=false`다. 플러그인 자체 Content는 현재 비어 있고,
프로젝트 전용 검증 자산은 `/Game/FestaFiesta/FFZoneSystem`에 있다.

### Runtime Public API

| 파일 | 단일 책임 |
|---|---|
| `FF_FFZSTypes.h` | Enum, 명령, 결과, Spec, Snapshot, Read Model, Policy Context |
| `FF_FFZSDeveloperSettings.h` | 프로젝트 공통 Prototype·기술 기본값 |
| `FF_FFZSGridLibrary.h` | 순수 Cell 회전·예약 영역·Preset·명령 fingerprint |
| `FF_FFZSZoneActor.h` | Gameplay Zone 경계·이웃·수용량·밀도 상태 |
| `FF_FFZSProtectedArea.h` | 보호영역 OBB와 겹침 판정 |
| `FF_FFZSFacilityDefinition.h` | `UPrimaryDataAsset` 기반 시설 정적 데이터; Primary Asset Type `FFZSFacilityDefinition` |
| `FF_FFZSFacilityActor.h` | 설치 시설 인스턴스와 표현·상태·Queue 보유 |
| `FF_FFZSFacilityAnchorComponent.h` | Entry·Service·Exit Anchor 식별 |
| `FF_FFZSQueueComponent.h` | 결정론적 FIFO·서비스·Drain·Cancel·Snapshot |
| `FF_FFZSQueueAgent.h` | 프로젝트 NPC에 대한 최소 Interface |
| `FF_FFZSExternalPolicy.h` | Nav·Economy·Time·UI·Safety·Experience 원자적 Adapter |
| `FF_FFZSPlacementPreviewActor.h` | 권위 없는 배치 Ghost와 Debug Cell |
| `FF_FFZSWorldSubsystem.h` | Registry·점유·검증·Command·Policy·Queue·Save의 권위 |

---

## 4. SOLID 적용

### S — 단일 책임

- Grid 수학은 `UFFZSGridLibrary`에만 둔다.
- 시설 정적 데이터는 `UFFZSFacilityDefinition`에 둔다.
- 시설 런타임 상태는 `AFFZSFacilityActor`에 둔다.
- 전역 점유와 원자적 명령은 `UFFZSWorldSubsystem`에 둔다.
- Queue 순서와 서비스는 `UFFZSFacilityQueueComponent`에 둔다.
- UI는 Read Model만 읽는다.

### O — 개방·폐쇄

새 시설은 `FFFZSFacilitySpec` Data Asset으로 추가한다. 기존 Queue·서비스로 표현할 수 없는
새 능력이 생길 때만 새 Component 또는 Subclass를 추가한다. Economy·Safety 공식이 바뀌어도
플러그인 코드를 고치지 않고 Policy Adapter를 교체한다.

현재 Snapshot 복원은 항상 정확한 `AFFZSFacilityActor`를 생성하며 Subclass factory 확장점은
없다. Subclass를 실제 저장·복원까지 지원하려면 class 식별자, 허용 목록, 생성 factory,
스키마 이관과 회귀 테스트를 먼저 추가한다.

### L — 리스코프 치환

시설 Subclass는 `AFFZSFacilityActor`의 명령·점유·Snapshot 불변식을 깨면 안 된다.
Queue Agent 구현은 어떤 Character·Pawn·Actor여도 같은 Stable ID와 도착 보고 계약을 지켜야
한다.

### I — 인터페이스 분리

- NPC는 `IFFZSQueueAgent`만 구현한다.
- 외부 규칙은 `IFFZSExternalPolicy`만 구현한다.
- UI는 `FFFZSReadModel`과 명령 결과를 사용한다.

프로젝트 Character, GameMode, Economy 클래스를 Runtime 모듈이 직접 include하지 않는다.

### D — 의존 역전

World Subsystem이 시리·민서·정원 등의 구체 클래스를 참조하지 않는다. 외부 Owner가
Policy Interface를 구현하고 런타임에 등록한다.

---

## 5. 데이터와 런타임 상태 분리

### 정적 데이터

`UFFZSFacilityDefinition::Spec`의 현재 `SchemaVersion`은 `2`다. 독립 Spec v1은 현재 Runtime
등록에서 거절하고, 제한된 World Snapshot v3 복원 경로에서만 검증 후 변환한다.

`Spec` 필드:

```text
SchemaVersion
FacilityDefinitionId
DisplayName
Category
FootprintSizeCells
ClearanceCells
QueueReservedRects              권위 표현, 반열림 [Min,Max)
QueueReservedCells              deprecated 호환 필드, 신규 데이터 사용 금지
QueueSplinePointsCm
QueueRules
Anchors
AllowedZoneIds
InstallCostKey
RevenueRuleKey
ExperienceEffectKey
VisualMesh
PlacementObstacleHeightCm
```

### 런타임 상태

시설 인스턴스:

```text
FacilityInstanceId
FacilitySpec snapshot
ZoneId
GridPlacement
FacilityState
WorldTransform
NeedsRelocation
Queue runtime state
```

### 외부 원본 데이터

- 실제 잔액·거래·매출
- Stage·Simulation Time
- NPC 욕구·Utility·Satisfaction
- Nav path·회피
- Safety 공식
- UI 화면 상태

동일 값이 Data Asset, Blueprint, C++ Default에 여러 번 복사되지 않게 한다. 내장 Preset은
테스트·초기 폴백이고, 운영 수치는 프로젝트 Data Asset이 정본이어야 한다. 현재
`/Game/FestaFiesta/FFZoneSystem/Data/Facilities`에는 Facility Definition `.uasset`이 0개다.
FoodTruck·Toilet을 포함한 7종은 아직 C++ built-in이며 운영 Data Asset을 별도로 만들어야 한다.
`/Game/FestaFiesta/FFZoneSystem/Tests`는 개발 맵·머티리얼 Slice 전용이다.
Info·Rest·Performance·Safety·PhotoSpot은 `Hypothesis`이며 측정 없이 Accepted로 취급하지 않는다.

---

## 6. Zone 계약

### 세 공간의 분리

| 공간 | 판정 권한 |
|---|---:|
| 2025 Reference Zone | 없음 |
| Gameplay Zone | 있음 |
| 252m Work Cell | 없음 |

`AFFZSGameplayZone`은 닫힌 선형 `BoundarySpline`을 사용한다. Spline 점은 cm 기준이다.

필드:

```text
ZoneId
DisplayName
SourceReferenceId
SchemaVersion
ContentVersion
bAuthoritativeGISBoundary
VerticalExtentCm
bAllowAllFacilityCategories
AllowedFacilityCategories
NeighborZoneIds
EntryExitAnchors
UsablePublicAreaSquareMeters
SoftCapacityOverride
```

Accepted 12개 ID는 `UFFZSGridLibrary::GetAcceptedGameplayZoneIds`와 Editor 전용
`ApplyAcceptedZonePreset`에서 제공한다. `bRestrictZoneIdsToProjectPresets=true`가 기본이고,
`ProjectZoneIds`는 생성자에서 이 12개로 초기화된다. Restricted mode는 Zone ID 소속과 설정된
그래프를 검증한다. 이식 프로젝트는 목록을 자체 Stable ID로 교체하거나 제한을 꺼 사용자 등록
그래프를 허용할 수 있다. Preset 적용 함수는 Blueprint 실행 API가 아니며, 이미 World
Subsystem에 등록된 Zone의 ID를 바꾸려 하면 거절한다.

```text
GZ_SunsetBeachClub   GZ_DadaPocha             GZ_DadaYajang
GZ_DadaepoForest     GZ_FountainPirates       GZ_SunsetLounge
GZ_FleaMarket        GZ_SunsetDogPark         GZ_SunsetWellness
GZ_PublicParkingWest GZ_TemporaryParkingEast  GZ_Operations
```

Facility `AllowedZoneIds`는 별도다. 빈 배열은 ID 제한 없음이고, 채운 배열은 None·중복을
거절하며 Festa 12개가 아닌 이식 프로젝트 ID도 허용한다. Placement와 Snapshot restore는 대상
Zone이 이 배열에 있는지 확인한다.

불변식:

- `ZoneId` 중복 금지.
- 경계 자기 교차 금지.
- 1 Cell = 1uu = 1cm이며 Scale은 1, Origin XY는 1cm 정수, Pitch/Roll은 0, Yaw는 90° 단위다.
- 이웃 관계는 양방향.
- 각 압축 예약 Rect 전체는 정확히 하나의 Gameplay Zone에 속해야 하며 경계 밖·두 Zone 중첩은
  모두 거절한다.
- Zone 변경 후 시설을 자동 삭제하지 않는다. `InitialSetup/Reconfigure`에서만
  `RevalidateFacilitiesInZone`으로 `NeedsRelocation`을 갱신하고 다른 Phase의 함정은
  17절을 따른다.
- Zone 소속 이동에는 50cm 히스테리시스를 사용한다.

Soft Capacity:

```text
SoftCapacityOverride > 0이면 Override 사용
아니면 UsablePublicAreaM2 / SquareMetersPerPersonForSoftCapacity
```

이는 게임 수치이며 실제 법정 수용 인원이 아니다.

---

## 7. 보호구역 계약

`AFFZSProtectedArea`는 `UBoxComponent` 기반 명시적 보호 영역이다.

`LinkedZoneId=NAME_None`은 모든 Zone에 적용되는 전역 보호영역이다. 값이 있으면 그 Zone이
등록돼 있어야 한다. 판정은 OBB를 사용하고 경계 접촉도 차단으로 취급한다. `ViewingArea`도
별도 override가 없으면 접근성 통로 기본 폭을 상속한다.

종류:

```text
NoBuild
GeneralRoute
EmergencyServiceRoute
AccessibilityRoute
ViewingArea
```

필드:

```text
ProtectedAreaId
LinkedZoneId
AreaType
RequiredClearWidthCm
AreaBounds
```

판정은 회전된 로컬 OBB를 사용해야 하며 단순 World AABB로 과잉 거절하지 않는다. Pitch/Roll은
검증에서 거절하고 Yaw만 허용한다. `RequiredClearWidthCm == 0`이면 Area Type별 Project
Settings 기본 폭을 동적으로 상속하고, 0보다 크면 Actor override다. Editor의
`ApplyDefaultWidthForType`은 현재 기본값을 Actor에 복사해 상속을 끊는다. 보호 폭
300/400/150cm는 Prototype 값이다.

---

## 8. 시설과 Anchor 계약

시설은 `AFFZSFacilityActor` 하나로 데이터 주도 생성한다.

상태는 선형 순서가 아니다.

| 명령·경로 | 허용 시작 상태 | 성공 뒤 상태 |
|---|---|---|
| Install / Initialize | 새 Actor | `Validated` |
| Move | Queue가 비어 있는 `Planned/Validated/Closed/MovePending` | `Validated` |
| Open | `Validated/Closed/MovePending` | `Active` |
| Close | 모든 non-`Removed` | `Closed` 후 비었거나 drain 완료 시 `MovePending` |
| EmergencyClose | 모든 non-`Removed` | Facility는 즉시 `Closed`; 물리 Agent Exit는 계속될 수 있음 |
| Remove | Queue가 비어 있는 `Planned/Validated/Closed/MovePending` | `Removed` 후 Actor destroy |

Actor C++ 기본은 `Planned`지만 정상 Install의 `InitializeFacility` 기본 인자는 `Validated`다.

Anchor:

```text
Entry      최소 1개
Service    ServiceCapacity 이상
Exit       최소 1개
```

QueueSpline은 시설 로컬 cm 좌표에서 서비스 앞쪽부터 꼬리 방향으로 저장한다. 슬롯 `i`의
중심은 Spline 거리 `i × SlotSpacingCm`에 있다. `i=0..MaxWaitingAgents-1`의 모든 중심이
`QueueReservedRects` 안에 있어야 하고, Spline은 마지막 중심점까지 길어야 한다. 기본
115cm는 슬롯 간격일 뿐 별도의 Agent Capsule 반경이나 꼬리를 예약하지 않으므로 실제
Nav/동선은 별도 검증한다. 피벗은 바닥 중앙, 전방은 +X, Scale은 1이다.

P0 내장 Preset:

| Preset | Footprint | Queue | Service | Duration |
|---|---:|---:|---:|---:|
| FoodTruckBasic | 600×300 Cells (6m×3m) | 12명 | 1 | 6초 |
| ToiletBasic | 400×400 Cells (4m×4m) | 8명 | 2 | 4초 |

두 시설 모두 Clearance 100 Cells(실제 1m), Slot 115cm, 도착 반경 25cm, Nav 투영 30cm,
Blocked 5초, Repath 2회다.

### FoodTruckBasic 완전 저작 예시

아래 값은 C++ Accepted baseline과 동일하다.

```yaml
SchemaVersion: 2
FacilityDefinitionId: FD_Booth_FoodTruckBasic
DisplayName: Basic Food Truck
Category: Booth
FootprintSizeCells: [600, 300]
ClearanceCells: 100
PlacementObstacleHeightCm: 250
AllowedZoneIds: []
QueueReservedRects:
  - { Min: [500, 300], Max: [600, 900] }
  - { Min: [0, 800],   Max: [500, 900] }
  - { Min: [0, 300],   Max: [100, 800] }
QueueSplinePointsCm:
  - [250, 200, 0]
  - [250, 750, 0]
  - [-250, 750, 0]
  - [-250, 200, 0]
QueueRules:
  MaxWaitingAgents: 12
  ServiceCapacity: 1
  ServiceDurationSeconds: 6
  AbandonAfterSeconds: 30
  SlotSpacingCm: 115
  SlotArrivalToleranceCm: 25
  NavigationProjectionExtentCm: 30
  BlockedTimeoutSeconds: 5
  MaxRepathAttempts: 2
Anchors:
  - { Type: Entry,   Index: 0, LocationCm: [250, 200, 0],  Yaw: -90 }
  - { Type: Service, Index: 0, LocationCm: [250, 150, 0],  Yaw: -90 }
  - { Type: Exit,    Index: 0, LocationCm: [-250, 200, 0], Yaw: 90 }
ExternalKeys:
  InstallCostKey: Cost.FoodTruck.Basic
  RevenueRuleKey: Revenue.FoodTruck.Basic
  ExperienceEffectKey: Experience.FoodTruck.Basic
```

Rect 좌표는 시설 로컬 1cm Cell이고 `Max`는 포함하지 않는다. `VisualMesh`는 프로젝트가
Soft Reference로 지정하며 논리 Spec 검증과 독립이다.

추가 Hypothesis Preset:

| Preset | ID / Category | Footprint | Wait / Service | Duration / Abandon | Key suffix |
|---|---|---:|---:|---:|---|
| InfoBasic | `FD_Info_Basic` / Info | 300×200 (3m×2m) | 6 / 1 | 4s / 25s | `Info.Basic` |
| RestBasic | `FD_Rest_Basic` / Rest | 400×300 (4m×3m) | 4 / 2 | 12s / 60s | `Rest.Basic` |
| PerformanceBasic | `FD_Performance_Basic` / Performance | 800×500 (8m×5m) | 16 / 2 | 10s / 45s | `Performance.Basic` |
| SafetyBasic | `FD_Safety_Basic` / Safety | 400×300 (4m×3m) | 6 / 2 | 8s / 15s | `Safety.Basic` |
| PhotoSpotBasic | `FD_PhotoSpot_Basic` / PhotoSpot | 500×300 (5m×3m) | 8 / 1 | 5s / 25s | `PhotoSpot.Basic` |

각 suffix는 `Cost.`, `Revenue.`, `Experience.` 세 prefix와 결합한다. 공통값은 Clearance 100 Cells(1m),
Slot 115cm, 도착 25cm, Nav 30cm, Blocked 5s, Repath 2, obstacle height 250cm, 빈
`AllowedZoneIds`다. Queue 예약 Rect·Spline·Entry/Service/Exit Anchor Data Validation은 통과했지만
수동 개발 Slice의 `1`/`2` 선택은 FoodTruck/Toilet만 지원한다.

---

## 9. Grid와 예약 영역

`UFFZSGridLibrary`의 계산은 World Actor를 탐색하지 않는 순수 함수여야 한다. Runtime 격자는
`UFFZSDeveloperSettings::DefaultPlacementCellSizeCm = 1.0f` 상수만 사용한다. Project
Settings의 `PlacementCellSizeCm`은 private UI readout이고 `.ini` override가 아니다.

1cm 좌표를 셀마다 배열과 맵에 저장하면 시설당 수십만 항목이 생기므로 권위 공간은
직사각형/Span 기반 압축 Region으로 유지한다.

```text
FootprintRegion
ClearanceRegions
QueueRegions
ReservedRegions = Normalize(Footprint ∪ Clearance ∪ Queue)
```

기존 Cell 배열 API가 남아 있더라도 소규모 디버그 물질화 경계로만 사용한다. 권위 배치,
점유, Snapshot 검증과 fingerprint는 압축 Region을 사용해야 한다.

압축 Rect는 `[Min,Max)` 반열림이다. `Max` 좌표는 점유에 포함되지 않으므로 인접 Rect가
경계를 공유해도 겹치지 않는다. Region 면적은 `int64`로 계산한다.

회전은 `QuarterTurns`를 0~3으로 정규화한다. 직사각형 시설은 90°/270°에서 X/Y가 바뀌며
Queue·Anchor도 같은 방향으로 변환된다.

시설 배치 성공 조건:

- 모든 압축 Reserved Rect가 정확히 하나의 요청 Zone 안에 존재.
- Zone Category와 `AllowedZoneIds`가 허용.
- 지면·경사·높이 차 통과.
- 기존 시설과 월드 충돌 없음.
- 보호영역과 겹치지 않음.
- 모든 Anchor와 `MaxWaitingAgents`개 Queue slot이 Nav에 투영 가능.
- 첫 Entry에서 모든 투영점까지 동기 경로가 존재하고 partial path가 아님.
- 외부 Policy가 허용.

배치 한 번의 Region 검증에서는 대상 Zone과 다른 Zone의 변환된 Polygon·Bounds를 한 번만
준비한다. Broad Phase로 후보를 줄인 뒤 Region과 Zone/시설/보호영역을 정확히 비교한다.
Ground는 Cell 해상도와 분리된 기본 100cm 물리 표본 간격을 사용한다.

Nav 투영 extent는 `max(1cm, min(Project NavigationProjectionExtentCm,
Spec.QueueRules.NavigationProjectionExtentCm))`다. 현재 경로 검사는 Game Thread의 동기
`FindPathToLocationSynchronously`이므로 Queue slot 수가 큰 데이터는 Preview 비용을 측정한다.
Preview Debug는 개별 수십만 Cell이나 QueueSpline이 아니라 압축 Footprint·Clearance·Queue

---

## 10. 배치 검증 순서

Preview와 Commit은 같은 단계 목록이 아니다.

```text
Preview / ValidatePlacementLocal
1. 명령 type·Phase·Facility ID·Spec
2. 요청 Zone 존재·Category·AllowedZoneIds
3. Footprint·Clearance·Queue·Reserved Rect와 Transform
4. 요청 Zone 하나에만 완전히 속하는지
5. 기존 시설 점유
6. 보호영역
7. Ground·경사·높이 차
8. World Collision
9. 모든 Anchor·Queue slot Nav 투영과 Entry 기준 full path
10. local valid일 때 Policy PreValidate
```

Spec·Zone·빈 Region 같은 선행 오류는 조기 반환한다. 따라서 `Reasons`는 “모든 가능한 오류”가
아니라 조기 반환 전까지 해당 단계에서 누적한 안정 순서 오류다.

```text
Commit / ExecuteCommand
1. CommandId·FacilityInstanceId·fingerprint·receipt/in-flight
2. Production Policy readiness
3. ExpectedWorldRevision
4. Install·Move·Remove Phase Gate
5. Install·Move는 Preview/local + Policy PreValidate 재검증
6. 명령별 Queue 점유·Actor 상태
7. Policy tentative reservation
8. 로컬 Actor·점유 변경
9. Policy commit → Revision 증가 → event, 또는 역순 rollback
```

Move의 Queue·상태 검사는 Preview의 Policy PreValidate 뒤 Commit 경로에서 수행되므로 초록
Preview도 `FacilityOccupied`·`InvalidFacilityState`로 거절될 수 있다. Remove는 Queue·상태
검사 뒤 Policy를 호출하고, lifecycle은 Policy tentative 뒤 Open 상태 적용을 시도한다.

Public `EFFZSPlacementFailure` 전체:

```text
None
ZoneNotFound
FacilityNotFound
DuplicateZoneId
DuplicateFacilityId
CategoryNotAllowed
OutsideZone
InvalidGround
ExcessSlope
HeightVarianceExceeded
PlacementCollision
InsufficientClearance
QueueFootprintBlocked
ProtectedRouteBlocked
RequiredRouteDisconnected
AccessibilityRouteBlocked
WrongStage
FacilityOccupied
InvalidFacilityState
StaleWorldRevision
DuplicatePayloadMismatch
NavigationUnavailable
NavRebuildTimeout
DataInvalid
ExternalPolicyRejected
InternalError
```

UI 해결 행동:

| 코드군 | 사용자에게 제시할 다음 행동 |
|---|---|
| NotFound·Duplicate·DataInvalid | 대상 선택·Stable ID·Data Asset 오류를 수정 |
| Outside·Ground·Collision·Clearance·Route | Preview 표시 영역으로 이동·회전하거나 원인 자산 수정 |
| `WrongStage` | `InitialSetup/Reconfigure`에서 다시 실행 |
| `FacilityOccupied/InvalidFacilityState` | Close 후 Waiting·Service·Exiting이 0이 될 때까지 Drain |
| `StaleWorldRevision` | 최신 Read Model을 다시 읽고 새 확인·명령으로 재시도 |
| `DuplicatePayloadMismatch` | 기존 ID를 재사용하지 말고 새 `CommandId` 발급 |
| Navigation·External·Internal | 프로젝트 Owner 로그와 Policy 결과를 표시하고 안전하게 중단 |

`NavRebuildTimeout`은 현재 enum·현지화 문구만 있고 Runtime emit 경로는 없다.

UI는 C++ `Message` 문자열로 분기하지 않고 Enum으로 분기한 뒤 현지화 문구를 표시한다.

---

## 11. 명령과 낙관적 동시성

시설 설치·이동·철거·Open/Close 같은 수명주기의 공개 상태 변경 경계:

```cpp
FFFZSCommandResult UFFZSWorldSubsystem::ExecuteCommand(
    const FFFZSCommand& Command);
```

Zone·보호영역 등록, Phase, Policy, Queue 보고도 반드시 `UFFZSWorldSubsystem`의 명명된 facade를
사용한다. 모든 변경은 Game Thread에서 실행하며 Command·Restore·Policy Callback 재진입 중이면
fail-closed한다.

여기서 Subsystem의 “권위”는 단일 로컬 논리 소유자를 뜻하며 Unreal 네트워크 Authority 기능이
아니다. 플러그인에는 Replication, RPC, `HasAuthority` 분기가 없다. 멀티플레이 프로젝트는
Server가 명령을 소유하고 결과·Read Model을 복제해야 하며 Client가 독립 실행하면 안 된다.
`WorldRevision`은 성공한 `ExecuteCommand`에서만 증가한다. Phase, Population, Queue 가입·
도착·차단·재검증 같은 facade 변경은 현재 Revision을 올리지 않는다.

Command:

```text
CommandId
TransactionId
ExpectedWorldRevision
Type
FacilityInstanceId
FacilitySpec
ZoneId
Placement.OriginCell
Placement.QuarterTurns
```

`BuildCommandFingerprint`의 “canonical”은 결정적 직렬화 형식이라는 뜻이다.
`QuarterTurns`만 정규화하고 Queue Rect/Cell/Spline, Anchor, `AllowedZoneIds` 배열 순서는
그대로 identity에 포함한다. 의미가 같아도 배열 순서를 바꾸면 같은 `CommandId`에서
`DuplicatePayloadMismatch`가 되므로 새 ID를 쓴다. durable 비교는 전체 fingerprint이며
CRC32는 진단용일 뿐 권위 비교값이 아니다.

멱등성:

- 새 `CommandId`: 정상 실행.
- 같은 ID·같은 fingerprint: 저장된 원래 결과 반환, Revision·이벤트 중복 없음.
- 같은 ID·다른 fingerprint: `DuplicatePayloadMismatch`.
- `ExpectedWorldRevision >= 0`이고 현재와 다름: `StaleWorldRevision`.
- `-1`은 최신 Revision 허용이므로 경쟁 상태 검증이 필요한 UI에서는 사용을 최소화한다.
- 유효 ID와 재진입 Guard를 통과한 뒤의 성공·실패는 모두 durable receipt가 된다.
- 실패 원인을 수정했어도 같은 `CommandId`는 실패를 replay하므로 새 ID로 다시 요청한다.

명령 영수증이 Snapshot에 저장되므로 Save/Load 뒤 replay도 같은 결과를 내야 한다.
`CommandId` 재사용 정책과 receipt 보존 기간은 프로젝트 Gateway가 명시한다.

상세 상태 전이는 8절 표를 따른다. Phase Gate는 별도다.

| `CurrentPhase` | Install·Move·Remove | Lifecycle 명령 |
|---|---|---|
| `InitialSetup`, `Reconfigure` | 허용; 나머지 검증 적용 | 모든 Phase에서 허용 |
| `MorningCheck`, `LiveOperation`, `EveningSettlement` | `WrongStage` | 모든 Phase에서 허용 |

Lifecycle 중 Open만 `Validated/Closed/MovePending` 상태 제한이 있다. Close/EmergencyClose는
등록된 non-`Removed` 시설이면 반복 호출도 성공하고 Revision을 증가시킬 수 있다.

`SetChangePhase`는 유효 enum이면 임의 단계 간 전이를 허용하며 순서 그래프를 강제하지 않는다.
단계 순서·저장·UI 갱신·멀티플레이 전파는 프로젝트 Stage Owner 책임이다.

---

## 12. 원자적 World Change와 Policy

`IFFZSExternalPolicy` 도메인:

```text
Navigation
Economy
SimulationTime
UserInterface
Safety
Experience
Custom
```

Production에서는 각 필수 도메인에 정확히 하나의 Adapter를 등록하고
`ValidateIntegrationReadiness`를 실행한다.
`bRequireProductionPolicyDomains`의 기본값은 `false`이므로 이 Gate는 운영 통합에서
명시적으로 켠다. 실행 목록은 `Domain + Name`으로 안정 정렬한 snapshot이며 중간 등록·해제가
현재 순서를 바꾸지 않는다.

Subsystem은 Provider를 `TWeakObjectPtr`로만 보관하므로 등록이 GC root가 아니다. 프로젝트
Owner가 `UPROPERTY` strong reference를 수명 전체에 유지한다. Policy Name은 전역 unique이고
non-`Custom` Domain은 도메인당 하나만 허용하며 `Custom`만 복수 등록할 수 있다. 정렬 key인
Name·Domain은 등록 후 바꾸지 말고 변경하려면 안전한 시점에 unregister/register한다. Provider가
소멸하면 readiness는 오류를 내며 Production Gate가 꺼진 transaction에서는 해당 항목이 건너뛰어질
수 있으므로 운영에서는 fail-open 상태를 허용하지 않는다.

호출 계약:

```text
PreValidateFFZoneSystemChange
  읽기 전용. 여러 번 호출돼도 외부 상태 무변경.

ApplyTentativeFFZoneSystemChange
  실패 가능한 예약·임시 적용. 부분 적용 후 실패할 수 있음.

CommitFFZoneSystemChange
  실패하면 안 됨. 실패 가능 작업은 Tentative로 이동.

RollbackFFZoneSystemChange
  부분 적용 뒤에도 안전하고 여러 번 호출해도 같은 결과.
```

전체 순서:

```text
local Validate
→ Domain + Name 안정 정렬 Policy snapshot
→ 모든 Policy PreValidate
→ 플러그인 상태 보존
→ Policy별 ApplyTentative
→ 월드·점유·시설 임시 적용
→ 사후 무결성 확인
→ 성공: Policy Commit + WorldRevision 증가 + OnWorldChanged
→ 실패: 적용 Policy 역순 Rollback + 플러그인 상태 원복
```

실패한 Policy도 부분 적용했을 수 있으므로 Rollback 대상에 포함하고 실제 적용의 역순으로
되돌린다. Callback 안에서 Zone·Protected Area·Phase·Policy 등록·Queue·Command·Snapshot
Restore를 다시 변경하려는 호출은 `false` 또는 거절 결과로 끝난다. Adapter는 후속 쓰기를
다음 안전한 Tick/작업 Queue로 넘긴다.

Preview도 `PreValidate`를 반복 호출하므로 이 함수는 빠르고 부작용이 없어야 한다. Policy
Callback 안에서 `BuildReadModel`이나 Snapshot을 캡처하면 tentative 중간 상태를 볼 수
있으므로 금지한다.

Economy 권장 구현:

```text
PreValidate: Key·잔액·해금 조회
ApplyTentative: 비용 예약
Commit: 예약을 실제 거래로 확정
Rollback: 예약 해제
```

UI Policy는 원본 상태를 수정하기보다 입력 권한·확인 흐름을 검증하고 결과 이벤트를 소비한다.

---

## 13. Queue 상태와 불변식

Queue State:

- `Open ↔ Full`: Waiting 수로 다시 계산한다.
- Facility가 `Active`일 때만 `Blocked`를 설정할 수 있다. 해제하면 과거 상태로 복귀하는 것이
  아니라 drain intent와 현재 Waiting 수로 `Draining/Closed/Open/Full`을 재계산하고 Service를 채운다.
- 일반 Close는 신규 가입을 막고 `Draining → Closed`로 간다.
- EmergencyCancel은 Actor 없는 Agent만 있으면 즉시 `Closed`다. 물리 Agent가 있으면 모두
  `Exiting`으로 유지한 `Draining` 상태이며 Exit 도착 뒤 `Closed`가 된다.

Facility State와 Queue State는 서로 다른 값이므로 UI에서 하나의 상태로 합치지 않는다.

Queue Agent 흐름:

```text
Join
→ Queue Slot 이동
→ Slot 도착 보고
→ FIFO 선두와 빈 Service 선택
→ Service Anchor 이동
→ Service 도착 보고 후 timer 시작
→ 완료 또는 취소
→ Exit Anchor 이동
→ Exit 도착 보고
→ 전역 소유권 해제
```

물리 Actor가 없는 Agent는 완료 시 즉시 해제될 수 있지만, Actor가 있으면 Exit 도착까지 소유권을 유지한다.

불변식:

1. 한 Agent는 전역 Queue Registry에 최대 한 번 존재한다.
2. Waiting 순서는 가입 순서이며 Queue Index는 0부터 연속이다.
3. 같은 Simulation Step의 순서는 JoinSequence로 결정한다.
4. Active Service 수는 Capacity를 넘지 않는다.
5. Service 중 Agent는 Waiting에 동시에 존재하지 않는다.
6. Draining·Closed·Blocked·Full은 신규 가입을 받지 않는다.
7. 이동 실패를 도착으로 처리하지 않는다.
8. Exit 이동 중에도 Facility 점유와 전역 Queue 소유권을 유지한다.
9. Service Anchor 도착 전에는 서비스 시간을 시작하지 않는다.
10. Move/Remove는 Waiting·Service·Exiting 중 하나라도 있으면 거절한다.
11. Service 배정은 도착한 FIFO 선두만, 비어 있는 가장 낮은 Service Index부터 한다.
12. Service 완료 통계는 물리 Exit가 끝나기 전에 증가할 수 있다.
13. Blocked는 신규 가입·새 Service 배정만 막고 기존 대기 포기·서비스 시간은 계속 진행한다.
14. 이동 실패는 설정된 Repath 한도까지 재요청하고 초과 시 안전하게 해제한다.

Queue Component의 공개 API는 조회 전용이고, 가입·도착·서비스·Exit·Drain·Cancel·Restore 같은
mutation은 private다. `AFFZSFacilityActor::GetQueueComponent()`도 const 포인터만 반환한다.
게임 코드는 `UFFZSWorldSubsystem::JoinFacilityQueue`, 도착/실패 보고, Rebind facade를 사용하고,
통로 차단은 Facility State가 `Active`일 때만
`SetFacilityQueueBlocked(FacilityInstanceId, bBlocked)`로 변경할 수 있다. Subsystem만
friend 경로로 mutable Queue에 접근한다. Queue State 변경은 public delegate 구독 여부와 무관하게
소유 Facility의 불변식을 먼저 갱신한 뒤 알림을 방송한다.

---

## 14. Queue 시간과 현재 구현 주의

현재 Component는 일반 플레이에서 Component Tick으로, 테스트·저장 시나리오에서는
`AdvanceSimulation(DeltaSeconds)`로 진행할 수 있다. 기본
`QueueSimulationTickIntervalSeconds=0.1`이 Component TickInterval을 정하며 `0`은 매 프레임이다.
Agent별 판단 Tick은 없다.

TickInterval은 Component 초기화 때 한 번 읽는다. 실행 중 Settings를 바꿔도 이미 존재하는
Queue Component에는 자동 반영되지 않으므로 재생성 또는 명시적 갱신 경로가 필요하다.

필수 통합 조건:

- Queue가 사용하는 Delta는 권위 있는 게임 Simulation Time과 일치해야 한다.
- Pause 중 Delta가 0이어야 한다.
- 배속은 프로젝트 시간 정책과 같은 배율을 사용해야 한다.
- 서비스 완료·동률 순서가 프레임 분할에 따라 달라지면 안 된다.
- 한 호출은 최대 4,096개 event boundary만 처리하며 상한 이후 남은 Delta는 처리하지 않는다.
- Blocked 중에도 기존 Waiting의 포기 시계와 Active Service 시계가 진행됨을 UI가 알려야 한다.

현재 Snapshot은 대기·서비스 elapsed, JoinSequence, `CurrentPhase`, `bDrainRequested`를
보존하지만 Close 원인은 보존하지 않는다. 상세 제약은 17절을 따른다. World Change 이벤트는
`EventId`를 발급하지만 Queue delegate와의 durable correlation, `ParentEventId`, 외부
authoritative `Day/Stage/PhaseElapsed`는 프로젝트 Telemetry와 Save Owner가 보강해야 한다.

---

## 15. `IFFZSQueueAgent` 계약

필수 Interface 함수:

```text
GetFFZoneSystemAgentId
OnFFZoneSystemQueueJoined
OnFFZoneSystemQueueIndexChanged
OnFFZoneSystemServiceAssigned
OnFFZoneSystemServiceStarted
OnFFZoneSystemExitRequested
OnFFZoneSystemQueueReleased
```

NPC Adapter 책임:

- Stable ID 생성·저장·복원.
- Queue Slot, Service, Exit로 실제 비동기 MoveTo.
- 성공 시 정확한 위치를 World Subsystem에 보고.
- 실패 시 `ReportQueueMovementFailed` 호출.
- 순간이동 없이 안전한 Exit·재선택.
- Load 후 새 Actor를 `RebindQueueAgentActor`로 연결.

Queue는 Character·Controller·StateTree를 직접 참조하지 않는다. StateTree를 쓰더라도
프로젝트 Adapter 내부에서 Interface 이벤트를 상태 전환으로 번역한다.

---

## 16. Read Model과 UI 경계

`BuildReadModel`은 UI·디버그가 원본 Actor 배열을 순회하지 않도록 Snapshot을 제공한다. 최상위 `FFFZSReadModel::WorldRevision`으로 화면이 어떤 월드 버전을 읽었는지 비교한다.

Zone:

```text
ZoneId
SoftCapacity
ReportedPopulation
DensityRatio
DensityState
```

Facility:

```text
FacilityInstanceId
FacilityDefinitionId
ZoneId
Category
State
NeedsRelocation
QueueState
WaitingCount
ActiveServiceCount
AverageCompletedWaitSeconds
```

UI는 Read Model을 수정하지 않는다. 버튼은 Command Request를 만들고, 성공·실패 결과와
OnWorldChanged/OnPlacementRejected 이벤트를 소비한다. 플러그인은 운영 UMG 자산을 포함하지
않는다. `Source/FestaFiesta/FFZoneSystemPlaytest/`의 C++ Widget은 개발 검증 Slice이며
Shipping에서는 비활성화된다.

반면 플러그인 `AFFZSPlacementPreviewActor`의 `bDrawDebugCells` 기본값은 `true`이고
Shipping compile guard가 없다. 운영 빌드는 Preview Actor를 생성하지 않거나 Debug Draw를
명시적으로 끄고 패키지 Smoke로 확인한다.

권장 갱신:

- 배치 Preview는 Cell·회전·Spec·Revision 변경 때만.
- 시설·Queue 패널은 상태 변경 이벤트 또는 낮은 주기 Snapshot.
- Density UI는 Crowd 표본 주기와 맞춤.
- 디버그 Draw와 상세 로그는 Development/Editor에서만.

---

## 17. 저장·복원 경계

`FFFZSWorldSnapshot` 현재 SchemaVersion은 `4`다. `RestoreWorldSnapshot`은 알려진 100cm
Schema 3 형식만 검사 후 메모리상 Schema 4로 변환한다. Schema 2에는 `CurrentPhase`가 없어
같은 명령 허용 상태를 재구성할 수 없으므로 거절한다.

포함:

- `WorldRevision`, `CurrentPhase`
- Facility Spec·Instance ID·Zone·Grid·실제 World Transform·Facility State
- `NeedsRelocation`
- Queue Waiting·Active Service·Exiting·elapsed·JoinSequence·repath·`bDrainRequested`
- Command Receipt와 canonical payload fingerprint

Schema 4에는 normal Close와 EmergencyClose를 구분하는 명시적 intent 필드가 없다. 물리 Agent가
남은 EmergencyClose도 Facility=`Closed`, Queue=`Draining`,
`bDrainRequested=true`로 저장되며 복원은 이를 normal Close처럼 해석해 Exit 완료 뒤
`MovePending`으로 바꿀 수 있다. 명시적 close intent 필드·새 schema·회귀 Test를 추가하기
전에는 이 순간 저장을 금지하거나 exact restore를 약속하지 않는다.

Snapshot 값 그래프의 저장 대상 property는 모두 `UPROPERTY(SaveGame)`다.
Editor 자동화는 테스트용 `USaveGame` root에 Snapshot을 넣고
`UGameplayStatics::SaveGameToMemory` → `LoadGameFromMemory` 실제 바이트 왕복 뒤 주요 필드를
비교한다.

제외:

- 맵의 Gameplay Zone·보호영역 생성
- NPC 위치·욕구·AI 상태와 Agent Actor 수명
- Economy 잔액·예약
- 외부 Stage·Simulation Time 원본
- Nav path·회피 속도
- UI 캐시
- 디스크 파일·Save 슬롯·클라우드·서버 I/O

복원 순서:

```text
Game Thread·재진입 Guard 확인
→ Schema·CurrentPhase·ID·Spec·Transform·Queue·Receipt 전체 검증
→ 임시 Actor·점유·Queue·전역 Agent Registry·Receipt staging
→ staging 전체 무결성 확인
→ 기존 플러그인 상태를 한 번에 atomic publish/교체
→ WorldRevision·CurrentPhase 확정
→ 외부 시스템 복원
→ AgentId Rebind
→ Policy·상호 참조 검증
→ 플레이 재개
```

Command 실행 중, Policy Callback 중, 다른 Restore 중인 복원은 fail-closed한다. staging이 하나라도
실패하면 임시 객체만 폐기하고 현재 WorldRevision·Actor·점유·Queue·Receipt·Phase를 바꾸지
않는다. 일부 시설만 조용히 삭제하는 폴백을 사용하지 않는다. 플러그인의 byte round-trip 성공은
프로젝트 전체 Save 슬롯과 외부 복원 orchestration 완료를 뜻하지 않는다.

Restore는 저장 데이터의 구조·ID·점유 무결성을 검사하지만 현재 맵의 Ground, World Collision,
Navigation을 다시 조회하지 않고 `OnWorldChanged`도 방송하지 않는다.
`RevalidateFacilitiesInZone`은 현재 Move의 Phase Gate를 재사용하므로
`InitialSetup/Reconfigure`에서만 공간 재검증에 사용한다. 다른 Phase에서 호출하면
`WrongStage` 때문에 정상 시설도 `NeedsRelocation=true`가 될 수 있으므로, phase-neutral
validator가 분리되기 전에는 호출하지 않는다.

재검증은 외부 Policy를 호출하거나 Revision·이벤트를 만들지 않는다. 맵·GIS·Nav 자산이 바뀐
Save는 허용 Phase에서 재검증한 뒤 프로젝트 UI/Telemetry를 명시적으로 refresh하고, Agent
Actor를 Stable AgentId로 다시 bind한다.

Receipt 검증은 성공 결과가 가리키는 Facility GUID의 실재를 강제하지 않는다. 변조된 Snapshot을
신뢰 경계 밖에서 받지 말고 프로젝트 Save 계층에서 서명·버전·소유권을 검증한다.

---

## 18. 실패 피드백과 로그

현재 `FFFZSWorldChangeEvent`가 직접 제공하는 payload:

```text
EventId
CommandId
TransactionId
Command Type
FacilityInstanceId
PreviousZoneId
NewZoneId
WorldRevision
```

Queue delegate는 더 작은 운영 신호다. 이벤트 종류에 따라 AgentId·TicketId·Queue Index 또는
Service/Release reason만 전달하며, 위 World Change 식별자 전체를 자동으로 복사하지 않는다.
따라서 아래는 “현재 모든 이벤트의 필드”가 아니라 프로젝트 Telemetry Adapter가 World Change,
Command result, Queue delegate, 외부 Simulation Clock을 correlation해 만들 목표 계약이다.

```text
EventId                    World Change에는 이미 존재; Queue 이벤트는 상위 Adapter가 발급/연결
ParentEventId
CommandId / TransactionId
FacilityInstanceId / FacilityDefinitionId / ZoneId / AgentId
WorldRevision / ReasonCode
External Day / Stage / PhaseElapsed / Simulation Time / Sequence
ExpectedWaitAtJoin / ActualWait / ServiceTime
```

Snapshot Schema 4의 `CurrentPhase`는 플러그인 변경 허용 Gate를 복원하는 값이다. 외부 authoritative
Day·Stage clock과 같지 않으며, 외부 시간 원본은 Snapshot 범위 밖이다.

필수 지표:

- 배치 검증 평균·p95
- 명령 성공·실패·Rollback·중복 replay 수
- ReasonCode 분포
- Queue Join 성공률·포기율
- 대기 p50·p95와 예측 MAE
- Throughput·Service Capacity 활용률
- Queue Intrusion 지속시간
- Slot 이동 실패율·Repath·Stuck
- Zone Density 평균·Peak·임계 지속시간
- 중복 Queue Membership 0
- 원자성 위반 0
- Event lineage 누락 0

---

## 19. 성능 원칙

1. Agent별 Queue 판단 Tick을 만들지 않는다.
2. Queue Component 단위로 대기·서비스를 갱신한다.
3. 슬롯이 변한 Agent에만 새 MoveTo를 보낸다.
4. UI는 매 프레임 Actor를 탐색하지 않는다.
5. Grid·Definition 같은 저비용 검증을 Collision·Nav보다 먼저 수행한다.
6. Soft Reference Mesh는 선택 화면에서 사전 비동기 로드한다.
7. 원거리 표현 LOD가 논리 Agent·Queue 순서를 삭제하면 안 된다.
8. 디버그 Draw·상세 로그는 운영 프로젝트에서 명시적으로 비활성화한다. 플러그인 Preview의
   `bDrawDebugCells`는 기본 `true`다.
9. 20→100→300 순으로 확장하고 각 단계의 맵·Seed·빌드를 고정한다.
10. 300명 Gate 실패 원인을 Game/Render/AI/Nav/Animation/Queue로 분해한 뒤 구조를 바꾼다.
11. Region 면적은 `int64`로 보수 계산하고, 정상 권위 연산은 압축 Region을 유지한다.
    `MaxEnumeratedCellsPerFacility` 기본 1,000,000은 expanded footprint 이론 면적과 canonical
    Queue 면적을 합친 권위 입력·작업 예산이며 출력 Cell 수나 물질화 한도가 아니다.
    legacy Blueprint/debug Cell view는 고정 10,000 상한을 넘으면 부분 결과 없이 빈 배열이고,
    deprecated `QueueReservedCells` 변환도 같은 상한을 따른다.

500명은 P1 Gate B 후보이며, Mass 전환은 300명 결과를 본 뒤 검토한다. 성급하게 Mass를
전제로 Public 계약을 바꾸지 않는다.

---

## 20. 자동화 테스트 현황

2026-08-05 현재 소스를 UE **5.7.4-51494982**에서 다시 빌드한 뒤 `FF_ZoneSystem` 필터를
실행했다. Unit 14 + Integration 6 = 총 20개를 발견했고 `Result={Success}` 20개,
실패 0개, UnrealEditor-Cmd 종료 코드 `0`을 확인했다.

검증 과정에서 실제로 발견한 결함도 먼저 수정했다.

- UHT가 헤더의 C++ digit separator를 파싱하지 못한 문제를 일반 정수 리터럴로 교정했다.
- 1cm 대형 영역 Test를 deprecated Cell 배열이 아닌 권위 `ReservedRects` 기준으로 교정했다.
- 자기 배열 원소를 `TArray::Add`에 직접 넘겨 재할당 중 참조가 깨진 Test crash를 값 복사로 교정했다.

XGE 라이선스 경고는 standalone executor fallback으로 처리됐고 빌드 자체는 성공했다.

검증 증거:

| 항목 | 값 |
|---|---|
| Engine | 5.7.4-51494982 |
| Git 기준 | branch `MAPSDASD`, HEAD `f29a8c1fe7a1e75e7f8946ddee3903b5d231a934` |
| Working tree | FF plugin과 관련 연결 파일이 미커밋·미추적 상태이므로 위 HEAD만으로 재현 불가 |
| 자동화 로그 | `Saved/Logs/FestaFiesta.log`, 2026-08-05 03:25:25~03:25:28 로그 시각 |
| 판정 | Found 20, `Result={Success}` 20, 다른 Result 0, `RequestExitWithStatus(..., 0)` |

`Saved`는 ignore되어 다음 실행에 덮어쓸 수 있다. 릴리스 Gate에서는 로그를 CI artifact로 보존한다.

### Unit 14개

```text
FF_ZoneSystem.Unit.Definitions.OneCentimeterCompressedBuiltInPresets
FF_ZoneSystem.Unit.Definitions.OneCentimeterCompressedRegionValidation
FF_ZoneSystem.Unit.Definitions.OneCentimeterEditorDataValidation
FF_ZoneSystem.Unit.Units.OnePlacementCellEqualsOneCentimeter
FF_ZoneSystem.Unit.Grid.OneCentimeterCellAndRectQuarterTurns
FF_ZoneSystem.Unit.Grid.OneCentimeterCompressedRegions
FF_ZoneSystem.Unit.Commands.OneCentimeterCompressedRegionPayloadIdentity
FF_ZoneSystem.Unit.Queue.FIFOArrivalAndCapacity
FF_ZoneSystem.Unit.Queue.DrainAndEmergencyCancel
FF_ZoneSystem.Unit.Queue.SaveRestoreMovementState
FF_ZoneSystem.Unit.Queue.SaveRestoreTickAndDrain
FF_ZoneSystem.Unit.Queue.EventStepTieAndDeltaIndependence
FF_ZoneSystem.Unit.Queue.BlockedAndMovementFailure
FF_ZoneSystem.Unit.SaveGame.WorldSnapshotByteRoundTrip
```

### Integration 6개

```text
FF_ZoneSystem.Integration.World.CommandLifecycleQueueAndSnapshot
FF_ZoneSystem.Integration.World.ZoneExclusivityAndProtectedArea
FF_ZoneSystem.Integration.World.InvalidFacilitySpecStopsBeforeNavigation
FF_ZoneSystem.Integration.World.PortableZoneGraphAndPrimaryAssetType
FF_ZoneSystem.Integration.World.SnapshotPhaseReentrancyAndDrainIntent
FF_ZoneSystem.Integration.Policy.AtomicCommitAndReverseRollback
```

테스트 소스:

```text
Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/Tests/
Plugins/FF_ZoneSystem/Source/FF_ZoneSystemTests/Private/FF_FFZSWorldTests.cpp
Plugins/FF_ZoneSystem/Source/FF_ZoneSystemTests/Private/FF_FFZSPolicyTests.cpp
Plugins/FF_ZoneSystem/Source/FF_ZoneSystemTests/Private/FF_FFZSSaveGameTests.cpp
```

Session Frontend 목록만 보거나 로그 일부만 보고 통과로 기록하지 않는다. `Automation Test Queue
Empty`, Started/Success/Failed 합계, 첫 Error, UnrealEditor-Cmd 종료 코드를 함께 확인한다.

---

## 21. 프로젝트 연결 절차

### `.uproject`

현재 `FestaFiesta.uproject`에는 `FF_ZoneSystem=true`가 저장돼 있고 2026-08-05 Editor 빌드로
로드를 확인했다. 새 프로젝트에서는 기술 담당자가 Editor의 **Edit → Plugins → Festa Fiesta →
FF_ZoneSystem → Enabled**로 활성화한다. 수치 담당자는 `.uproject` JSON을 직접 편집하지
않는다.

### 게임 모듈

프로젝트 C++가 Public API를 include할 때 `Source/FestaFiesta/FestaFiesta.Build.cs`에 다음 Runtime
의존성이 필요하며 현재 FestaFiesta에는 이미 연결돼 있다.

```csharp
"FF_ZoneSystem"
```

Tests 모듈을 게임 Runtime 의존성에 추가하지 않는다.

### 설정

Editor의 **Edit → Project Settings → Game → FF_ZoneSystem**에서 저장한다. 찾기 어렵다면 상단 검색창에 `FF_ZoneSystem`을 입력한다.

현재 CDO 기준 필수 확인값:

| 영역 | 필드 | 기본값·단위 |
|---|---|---|
| 격자 | `PlacementCellSizeCm` | 읽기 전용 1cm; Runtime 상수 |
| 작업량 | `MaxEnumeratedCellsPerFacility` | 1,000,000; clamp 1..1,000,000 |
| 지면 | `GroundSampleSpacingCm` | 100cm; clamp 1..1,000cm |
| Queue | `QueueSimulationTickIntervalSeconds` | 0.1s; 0이면 매 프레임 |
| 지면 | `MaxGroundSlopeDegrees` / `MaxGroundHeightVarianceCm` | 5° / 20cm |
| Trace | `GroundTraceAboveCm` / `GroundTraceBelowCm` | 1,000cm / 1,000cm |
| 검사 | Ground / every sample / world collision / nav projection | 모두 `true` |
| Nav | `NavigationProjectionExtentCm` | 30cm |
| Nav 부재 | `bFailWhenNavigationSystemIsUnavailable` | `false`; 운영은 `true` 검토 |
| Zone | `ZoneMembershipHysteresisCm` | 50cm |
| 보호 폭 | General / Emergency / Accessibility | 300 / 400 / 150cm |
| 수용량 | `SquareMetersPerPersonForSoftCapacity` | 4m²/인 |
| 밀도 | Caution / Warning / Critical | 0.8 / 1.0 / 1.25 |
| 승격 유지 | Warning / Critical | 5s / 3s |
| Zone ID | `bRestrictZoneIdsToProjectPresets` | `true`; Festa 기본 12개 |
| 통합 Gate | `bRequireProductionPolicyDomains` | `false`; 운영 연결 후 `true` |
| Workflow | `InitialPhase` | `InitialSetup` |

현재 권위 기본값은 `FF_FFZSDeveloperSettings.h`의 CDO initializer와 constructor다.
`Config/DefaultFF_ZoneSystem.ini`는 배포에 필요한 일부 값만 중복 명시하며, 생략된 Trace·
검사 bool·ProjectZoneIds·InitialPhase 등은 CDO/constructor 값을 쓴다.

설정 클래스는 `UCLASS(Config=Game, DefaultConfig)`다. 이 저장소에는 Plugin ini의 비기본
override가 실제 Editor·Cook CDO에 들어오는지 검증하는 자동화가 아직 없다. 기본값이 아닌
설정을 배포하기 전에는 시작 시 CDO 값을 assert/log하는 Test를 추가하고, 필요하면 Config
category와 파일 이름을 Unreal 설정 계층에 맞게 통일한다.

Runtime 격자 수학은 `DefaultPlacementCellSizeCm=1.0`만 사용하며 ini로 바꿀 수 없다.
`MaxEnumeratedCellsPerFacility`는 비정상 Spec의 expanded footprint+Queue 작업 예산을
fail-closed하고 정상 권위 검증은 압축 Region을 사용한다. legacy Blueprint/debug Cell 보기의
별도 고정 상한은 10,000이며 초과하면 빈 배열이다.
Queue Tick 변경은 기존 Component 재생성 여부와 Pause·배속·동률·frame split 회귀 테스트를
함께 확인한다.

현재 프로젝트 전용 검증 Slice:

```text
Map    /Game/FestaFiesta/FFZoneSystem/Tests/Maps/L_FFZS_PlacementTest
Code   Source/FestaFiesta/FFZoneSystemPlaytest/
Script Content/Python/build_ff_zone_system_playtest.py
```

Editor `-game -nullrhi` 5초 Smoke에서 Map bring-up, Playtest Actor spawn, C++ UI와 Preview
초기화만 확인했다. 수동 클릭 배치, Nav/NPC Queue 전체 흐름, 추가 5개 Preset 선택, 패키징된
Shipping exe 실행을 검증한 결과가 아니다. 이 코드는 map allow-list와 non-Shipping guard가
있으며 독립 BuildPlugin에는 포함되지 않는다.

### 프로젝트 전용 Adapter

현재 checkout에는 `FF_FestivalRuntime`, `UFF_Festival...` 클래스나 완성된 운영 Policy
자산이 없다. 아래는 존재하는 자산 목록이 아니라 프로젝트가 구현해야 할 책임표다.

| Policy Domain | 프로젝트 Owner가 연결할 것 |
|---|---|
| Navigation | Nav rebuild 예약·취소·완료·실패와 실제 경로 검증 |
| Economy | 비용·환불·수익 key의 tentative reservation/commit/rollback |
| SimulationTime | Pause·배속·Stage와 Queue Delta의 단일 시간 원본 |
| UserInterface | 입력 권한, 확인 대화상자, 결과·ReasonCode 현지화 |
| Safety | 통로·접근성·긴급 동선의 승인된 프로젝트 규칙 |
| Experience | 만족도·혼잡·시설 효과의 외부 모델 |

명령 Gateway는 매 요청마다 새 `CommandId`, 필요 시 같은 `TransactionId`, 읽은
`WorldRevision`을 넣고 오직 WorldSubsystem facade만 호출한다. Adapter의
`PreValidate`는 읽기 전용, `ApplyTentative`는 실패 가능, `Commit`은 실패 불가,
`Rollback`은 멱등이어야 한다.

등록 직후 `ValidateIntegrationReadiness`를 실행하고 누락·중복 도메인이 있으면 운영 시작을
막는다. `RefreshWorldActors`는 현재 빈 함수이므로 자동 발견을 기대하지 말고 Zone·보호영역·
Policy를 명시적으로 등록한다. 등록 실패 Actor는 월드에 남지만 Subsystem에는 없으므로 로그를
오류로 승격하고 잘못된 Actor를 수정·제거한다.

---

## 22. 빌드·자동화·패키징 명령

2026-08-05 UE 5.7.4 현재 소스 결과:

| 검증 경로 | 현재 결과 |
|---|---|
| `FestaFiestaEditor Win64 Development` | 성공 |
| `FestaFiesta Win64 Development` | 성공 |
| `FestaFiesta Win64 Shipping` | 성공 |
| `FF_ZoneSystem` 자동화 | 20/20 성공, 실패 0, exit 0 |
| 현재 소스 독립 `BuildPlugin` | 이번 검증에서 미실행 |
| 현재 소스 Cook·Pak·IoStore·패키지 exe Smoke | 이번 검증에서 미실행 |

아래 명령은 동일 환경에서 결과를 재현하고 릴리스 Gate를 확장하는 절차다. 프로젝트 루트에서
PowerShell을 열고 공통 변수 block을 한 번 실행한 뒤 필요한 명령을 실행한다.

```powershell
$ffzsUE = 'C:\Program Files\Epic Games\UE_5.7'
$ffzsProjectRoot = (Resolve-Path -LiteralPath '.').Path
$ffzsArtifactRoot = Join-Path $ffzsProjectRoot 'BuildArtifacts'
$ffzsRunTag = Get-Date -Format 'yyyyMMdd_HHmmss'
$ffzsPluginPackage = Join-Path $ffzsArtifactRoot "FF_ZoneSystemPackage_$ffzsRunTag"
$ffzsShippingArchive = Join-Path $ffzsArtifactRoot "FF_ZoneSystemPlaytestShipping_$ffzsRunTag"
```

### Editor

```powershell
& "$ffzsUE\Engine\Build\BatchFiles\Build.bat" `
  FestaFiestaEditor Win64 Development `
  -Project="$ffzsProjectRoot\FestaFiesta.uproject" `
  -WaitMutex -FromMSBuild
```

### Development Game

```powershell
& "$ffzsUE\Engine\Build\BatchFiles\Build.bat" `
  FestaFiesta Win64 Development `
  -Project="$ffzsProjectRoot\FestaFiesta.uproject" `
  -WaitMutex -FromMSBuild
```

### Shipping Game

```powershell
& "$ffzsUE\Engine\Build\BatchFiles\Build.bat" `
  FestaFiesta Win64 Shipping `
  -Project="$ffzsProjectRoot\FestaFiesta.uproject" `
  -WaitMutex -FromMSBuild
```

### 자동화

```powershell
& "$ffzsUE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$ffzsProjectRoot\FestaFiesta.uproject" `
  -unattended -nop4 -nullrhi -nosplash -NoSound `
  -ExecCmds='Automation RunTests FF_ZoneSystem; Quit' `
  -TestExit='Automation Test Queue Empty'
```

### 독립 플러그인

```powershell
& "$ffzsUE\Engine\Build\BatchFiles\RunUAT.bat" `
  BuildPlugin `
  -Plugin="$ffzsProjectRoot\Plugins\FF_ZoneSystem\FF_ZoneSystem.uplugin" `
  -Package="$ffzsPluginPackage" `
  -TargetPlatforms=Win64 -Rocket
```

배포할 때만 빈 출력 경로에 새로 만들고 `HostProject/Plugins/FF_ZoneSystem`의 소스·Config·
`FF_README_KO.md` 포함 여부와 세 Target 결과를 확인한다.

### Shipping Cook

```powershell
& "$ffzsUE\Engine\Build\BatchFiles\RunUAT.bat" `
  BuildCookRun `
  -project="$ffzsProjectRoot\FestaFiesta.uproject" `
  -noP4 -unattended -platform=Win64 -clientconfig=Shipping `
  -build -cook -map='/Game/FestaFiesta/FFZoneSystem/Tests/Maps/L_FFZS_PlacementTest' `
  -AdditionalCookerOptions='-noxgeshadercompile' `
  -stage -pak -iostore -archive `
  -archivedirectory="$ffzsShippingArchive"
```

현재 소스에는 이 Cook·Archive를 다시 실행하지 않았다. 성공 판정은 UAT exit 0뿐 아니라
`Windows/FestaFiesta.exe`, Shipping binary, `.pak`, `.ucas`, `.utoc`, Global container의
존재와 패키지 실행 Smoke를 함께 본다. 그 다음 별도 PC와 프로젝트 전체 맵 Cook을 수행한다.
XGE ShaderCompileWorker 라이선스가 없으면 위처럼 `-noxgeshadercompile`을 유지한다.

---

## 23. 다른 프로젝트 이식

### 소스 플러그인 이식

```text
<TargetProject>/Plugins/FF_ZoneSystem
```

1. 양쪽 Editor를 닫는다.
2. 현재 `Plugins/FF_ZoneSystem`을 복사하되 `Binaries`, `Intermediate`, `DerivedDataCache`는 제외한다.
3. 대상 `.uproject`에서 프로젝트 파일을 재생성한다.
4. 대상 `Development Editor / Win64`를 빌드한다.
5. Plugin을 활성화하고 재시작한다.
6. Project Settings에서 private readout `Placement Cell Size Cm = 1`과 대상 프로젝트의
   Stable `Project Zone Ids`를 확인한다.
7. 새 평면 테스트 맵과 Gameplay Zone을 만든다.
8. Facility Definition·Mesh·Material·Texture를 Content Browser의 **Asset Actions → Migrate**로 옮긴다.
9. Policy·Queue Agent·전체 Save·운영 UMG를 대상 프로젝트에 맞게 구현한다.
10. `FF_ZoneSystem` 20개 자동화, 실제 맵 Smoke, Shipping Cook을 대상 프로젝트에서 다시 수행한다.

바이너리 이식이 필요하면 기술 담당자가 현재 소스로 빈 출력 경로에 BuildPlugin을 새로 만들고,
대상과 동일한 UE patch version·플랫폼·configuration에서 자동화와 Cook을 다시 검증한 뒤
사용한다. 과거 BuildArtifacts를 배포 근거로 재사용하지 않는다.

### Asset Manager

현재 Plugin에는 Facility Definition `.uasset`이 없고 Project 설정에도
`PrimaryAssetTypesToScan` 규칙이 없다. 7개 내장 Preset은 C++ factory 데이터이며 운영
Content를 대신하지 않는다.

Data Asset을 런타임 검색하고 Soft Reference Mesh를 Cook하려면 **Edit → Project Settings →
Asset Manager**에서 클래스 `UFFZSFacilityDefinition`, 정확한 Primary Asset Type `FFZSFacilityDefinition` 규칙을 추가한다.

권장 검색 디렉터리:

```text
/Game/FestaFiesta/FFZoneSystem/Data/Facilities
```

대상 프로젝트가 다른 경로를 쓰면 그 경로로 변경한다. Asset Audit에서 실제 Primary Asset
Type을 확인하고 Shipping 별도 PC에서 Mesh·Material·Texture를 검증한다.

---

## 24. 마이그레이션 계약

### 기존 100cm 플러그인

물리 크기를 보존하는 기본 변환:

```text
새 1cm Footprint/Clearance/q0 Origin = 기존 100cm Cell 정수 × 100
기존 Queue Cell 1개 = 새 100×100 반열린 Queue Region
```

- 90°·180°·270° 배치 Origin에는 각각 필요한 99cm 보정을 적용한다.
- `QueueSplinePointsCm`, Anchor cm, 시간·거리 cm 값은 이미 물리 단위이므로 바꾸지 않는다.
- 타입명·클래스명이 다르면 Data Asset을 새 `UFFZSFacilityDefinition`으로 생성한다.
- 독립 Facility Spec Schema 1은 1cm Runtime이 등록·검증에서 거절한다.
- World Snapshot Schema 3은 Spec1·빈 새 Rect·정상 회전·안전 범위라는 알려진 형식일 때만
  `RestoreWorldSnapshot`이 100배·Queue Region·99cm 회전 보정을 적용해 메모리상 Schema 4로 변환한다.
  구 Queue Cell은 정렬·span 병합으로 `O(N log N)`에 압축하며 10,000개를 넘으면 오프라인 이관을 요구한다.
- 완료 Receipt는 불투명 값으로 보존되고 외부 경제/Stage 상태는 Snapshot 밖이므로, 운영 Save는
  프로젝트 저장 계층의 승인된 트랜잭션 이관과 복원 직후 Schema 4 재저장이 필요하다.

### 과거 1cm Beta 플러그인

좌표 숫자가 이미 1cm여도 타입, Queue 표현, 스키마와 영수증 지문은 새 정본과 다르다.

- 위치·Footprint·Clearance·점유·명령 영수증·SaveGame을 명시적으로 매핑한다.
- 이전 CommandId·fingerprint를 새 스키마에 임의 재사용하지 않는다.
- 변환 전·후의 q0/q1/q2/q3 Origin, Reserved Rect, Queue 중심, Anchor Transform을 표로 저장하고
  이 절의 공식과 자동화 Test를 정본으로 삼는다.

### ID

- FestaFiesta 내부 이관은 승인된 12개 `GZ_` ID를 유지한다.
- 다른 프로젝트는 자체 Stable `Project Zone Ids` 목록으로 교체하고 변환표를 저장한다.
- Facility Definition ID와 프로젝트 Stable Agent ID를 유지한다.
- 표시 이름 변경은 ID 변경과 분리한다.

---

## 25. QA Gate

### 정적 검증

- [ ] Public 헤더가 프로젝트 전용 클래스를 참조하지 않는다.
- [ ] Runtime이 Editor 모듈에 의존하지 않는다.
- [ ] 거리 필드에 Cm, 격자 필드에 Cells가 명시된다.
- [ ] 12개 Zone ID·P0 시설 ID가 일치한다.
- [ ] Reason enum과 UI 현지화 표가 일치한다.

### 기능

- [ ] 유효 설치는 Actor·점유·Revision·비용이 각각 한 번 반영된다.
- [ ] 무효 설치는 모든 외부 예약과 월드 상태를 원복한다.
- [ ] 같은 Command replay가 중복 이벤트를 만들지 않는다.
- [ ] 시설 이동 전 Queue Drain이 필요하다.
- [ ] Emergency Close가 Exit 요청과 최종 해제를 구분한다.
- [ ] Snapshot 실패가 현재 월드를 바꾸지 않는다.
- [ ] GIS 경계 변경이 시설을 삭제하지 않고 NeedsRelocation으로 표시한다.

### 사용자

- [ ] 처음 보는 사용자가 Zone, Footprint, Clearance, Queue를 구분한다.
- [ ] 실패 이유와 해결 행동을 설명할 수 있다.
- [ ] Queue 앞·뒤와 Full·Blocked·Closed를 구분한다.
- [ ] NPC 이동이 순간이동·새치기처럼 보이지 않는다.
- [ ] Ground를 사용하지 않아도 손해가 없다.
- [ ] 설치 모드와 게임을 자유롭게 중단할 수 있다.

### 성능

- [ ] 20명 Smoke
- [ ] 100명 Scale
- [ ] 300명 Gate A
- [ ] Overview/Ground 각각 FPS·1% Low·Game·Render·AI/Nav·Queue·Animation 기록
- [ ] 변경 전후 동일 맵·Seed·빌드 사용

---

## 26. 현재 제한 사항과 후속 작업

- 2026-08-05 현재 소스의 Editor Development, Game Development, Game Shipping 빌드와
  자동화 20/20은 성공했다.
- 현재 소스의 독립 BuildPlugin, Cook·Pak·IoStore, 패키지 exe, 별도 PC Smoke는 미검증이다.
- 기본 맵 `L_Dadaepo_DigitalTwin.umap`에는 아직 `/Script/ZoneFacilitySystem` import와
  `ZFFacilityActor_1` 계열 직렬화 흔적이 있다. 복제 맵에서 Reference Viewer·로그로
  이관 완료를 확인하기 전 원본을 무작정 재저장하지 않는다.
- 개발용 배치 입력·C++ Widget·`L_FFZS_PlacementTest`는 있으나 수동 클릭·Nav/NPC 전체 Queue,
  운영 UMG, 기존 다대포 맵 이관은 완료되지 않았다.
- Production Navigation·Economy·SimulationTime·UI·Safety·Experience Adapter 6종은 아직 연결되지 않았다.
- 운영 Facility Definition Data Asset, Plugin Content, Asset Manager scan rule은 아직 없다.
- 플러그인 Snapshot의 실제 SaveGame byte round-trip은 통과했지만 전체 게임 슬롯·파일·클라우드 I/O와 외부 상태·Agent 복원 orchestration은 없다.
- Restore는 Ground·Collision·Nav 재검증과 `OnWorldChanged`를 수행하지 않는다.
- Snapshot v4는 진행 중 normal Close와 물리-Agent EmergencyClose의 intent를 구분하지 못해,
  후자를 복원하면 Exit 완료 뒤 `MovePending`으로 바뀔 수 있다.
- `RevalidateFacilitiesInZone`은 `InitialSetup/Reconfigure` 밖에서 `WrongStage`를
  relocation으로 기록할 수 있어 phase-neutral validator가 생기기 전 운영 Phase 호출을 금지한다.
- World Change에는 EventId가 있지만 Queue delegate와의 correlation, ParentEventId, 외부 Day/Stage/PhaseElapsed durable telemetry는 프로젝트 통합이 필요하다.
- `NavRebuildTimeout` enum·문구는 있으나 현재 Runtime emit 경로가 없다. 실제 비동기 rebuild의
  Pending·취소·timeout 수명주기는 Navigation Adapter가 구현해야 한다.
- `WorldRevision`은 Queue·Phase·Population 등 모든 변경의 전역 버전이 아니며 성공 Command만 센다.
- 완료 Receipt의 Facility 참조를 신뢰 경계에서 추가 검증해야 한다.
- Reference Zone·252m Work Cell이 실제 기존 맵 판정에 들어가지 않는지 아직 맵 QA가 필요하다.
- 20/100/300 NPC 사용자·성능 Gate, 기준 PC, 목표 FPS, 1% Low가 미측정·미정이다.
- Multiplayer Authority·Replication·RPC·재접속은 구현돼 있지 않다.
- `RefreshWorldActors`는 비어 있고 Actor 등록은 명시적이다.
- Soft Mesh load 실패는 논리 설치를 취소하지 않는 표현 계층 실패다.
- Plugin ini의 비기본 override가 CDO·Cook에 반영되는 자동화는 없다.
- 실제 행사 안전·법률 기준은 확정되지 않았다.
- 500명은 P1 후보, 약 700명은 비약속 Stretch다.

이 제한 사항이 남아 있는 동안 “기존 다대포를 포함한 모든 시설·Zone 통합 완료”, “실제 안전
기준 준수”, “현재 소스 전체 패키징·별도 PC 검증 완료”라고 표현하지 않는다.

---

## 27. 완료 보고서에 들어갈 증거

```text
엔진 전체 버전
Commit/브랜치
활성 Plugin 목록
Project Settings 캡처
Editor·Development·Shipping 빌드 종료 코드
FF_ZoneSystem 테스트 총수·성공·실패
BuildPlugin 산출물 경로
대표 맵·Seed·시설 데이터 버전
20/100/300 NPC 성능 표
정상·거절·Rollback·Queue·Snapshot 영상
Policy Adapter 등록 결과
수치 변경 전후 KPI
미해결 위험·Owner·기한
```

결과 화면만 남기지 않고 `의도 → 규칙·데이터 → 구현 → 테스트 → 발견 → 수정`의 연결을
보여줘야 한다.

---

## 28. 관련 문서

- [게임 디자인 강의 요약](../../GD_01_16_LECTURE_SUMMARY.md)
- [Festa Fiesta Master GDD](../../FESTA_FIESTA_MASTER_GDD_2026-07-30.md)
- [Queue System Design](../../FESTA_FIESTA_QUEUE_SYSTEM_DESIGN.md)
- [다대포 좌표 기준 명세](../../Docs/Dadaepo_GeoReference_Spec.md)
- [2025 배치 구현 명세](../../Docs/Dadaepo_2025_Layout_Spec.md)

`Plugins/FF_ZoneSystem/FF_README_KO.md`가 FF_ZoneSystem의 유일한 현재 운영 정본이다. 이전
ZoneFacility 문서와 분산된 FF Zone 보조 가이드는 이 문서에 흡수한 뒤 제거했다.

문서와 Public API가 다르면 어느 한쪽을 조용히 고치지 않는다. 차이, 영향, 마이그레이션,
테스트, 승인자를 기록하고 같은 변경 단위에서 동기화한다.
