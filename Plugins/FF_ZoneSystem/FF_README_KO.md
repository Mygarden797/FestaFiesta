# FF_ZoneSystem 기능 상세 가이드

> **2026-09-26 빌드·성능 정리:** 단순 기하 API는 필요한 타입만 선언하고, 관객 조회 API는 관람 데이터만 포함한다.
> 해당 타입의 값을 직접 만들거나 멤버를 읽는 `.cpp`는 `FF_FFZSTypes.h`를 직접 포함한다.
> 실행 코드가 없던 `FF_FFZSExternalPolicy.cpp`, `FF_FFZSQueueAgent.cpp`, `FF_FFZSFacilityAnchorComponent.cpp`는 제거했다.
> 이 타입들의 헤더와 블루프린트 API는 유지되며 리플렉션 연결 코드는 UHT가 생성한다.
> 대기열은 여러 명을 서비스로 옮길 때 대기 배열을 한 번만 당기고, 공간 검색은 같은 좌표의 재등록을 재사용한다.
> 수정 시 `FF_ZoneSystem.Unit.Queue`와 `FF_ZoneSystem.Unit.SpatialHash` 자동화 테스트로 선착순·좌표 경계 계약을 확인한다.
> 테스트는 실제 동작을 검증하므로 유지한다. 테스트가 꺼진 빌드에서는 공용 테스트 데이터 헤더의 의존성도 제외한다.

> **2026-09-22 유지보수 안내:** [담당 기능·설정 변경법·전체 파일 지도](../../Collaboration/Refactor_Plugin_Notes_KO.md)를 먼저 읽으세요.
> 현재 월드 저장 스키마는 8이며 1cm 좌표, 자유 회전, 시설 등급, Road/Tile 구분을 포함합니다.
> 아래 문서의 오래된 마이그레이션 절은 당시 이행 과정을 설명합니다. 저장 호환의 최신 기준은 `FFFZSWorldSnapshot::CurrentSchemaVersion`과 `UFFZSWorldSubsystem`입니다.

> **2026-09-10 통합 건설 영역 추가:** 레벨에 `AFFZSBuildArea`를 하나 배치하면 기존 지역의
> 공유 경계를 넘는 시설 배치를 지원한다. 기존 `AFFZSGameplayZone`은 NPC·혼잡·저장용 지역으로 유지한다.
> 아래의 “정확히 하나의 Zone 안” 규칙은 BuildArea가 없는 기존 모드에 적용된다.
> [비전공자 작업 가이드](../../Collaboration/UnifiedConstruction_UserGuide_KO.md)에서 적용·확인·복구 순서를 확인할 수 있다.
>
> 통합 모드는 UE GeometryAlgorithms의 폴리곤 합집합 캐시, 시설용 20m 희소 공간 해시,
> 지역 소속 조회용 100m 공간 해시를 사용한다. 시설의 저장 좌표와 점유는 여전히 1cm 단위다.
> 지역 경계를 런타임에 수정한 뒤에는 `UFFZSWorldSubsystem::RefreshBuildArea()`로 캐시를 갱신한다.
> 시설은 기존 Install/Move/Upgrade/Remove 명령으로 변경하면 점유 인덱스가 자동 갱신된다.

> 대상 플러그인: `Plugins/FF_ZoneSystem`<br>
> 플러그인 버전: `2.0.0`<br>
> 기준 단위: `1 Unreal Unit = 1 cm`, `1 배치 Cell = 1 cm`<br>
> 문서 기준: 2026-08-05 현재 작업 트리의 Runtime/Public API와 구현

`FF_ZoneSystem`은 Unreal Engine 월드에서 Gameplay Zone을 정의하고, 시설의 공간 배치부터 운영 상태,
대기열, 외부 시스템 연동, 저장·복원까지 일관된 규칙으로 관리하는 Runtime 플러그인이다.

이 플러그인의 핵심은 “Zone 안에 Actor가 있는지 확인하는 기능”에 그치지 않는다. 시설 배치를 하나의
권한 명령으로 처리하면서 다음 상태가 서로 어긋나지 않도록 묶는다.

- Zone 경계와 Zone 간 그래프
- 시설 Footprint, 여유 공간, 최대 Queue 영역
- 보호 동선과 설치 금지 영역
- 지면, 월드 충돌, Navigation 연결성
- 시설 설치·이동·제거·개장·폐장 상태
- 결정론적 FIFO Queue와 NPC 이동 단계
- 외부 Navigation·Economy·Time·UI·Safety·Experience 정책
- Command 멱등성, 낙관적 동시성, 변경 이벤트
- 시설·Queue·완료 명령 영수증의 Snapshot 저장과 원자적 복원

---

## 1. 한눈에 보는 담당 기능

| 기능 영역 | 플러그인이 담당하는 것 | 플러그인 밖에서 담당할 것 |
|---|---|---|
| Zone | 경계, Stable ID, 포함 판정, 인접 그래프 검증, 수용량·밀도 상태 | GIS 원본 수집, 레벨 디자인 승인 |
| 시설 배치 | 1 cm 격자, 회전, 점유, 여유 공간, Queue 예약 영역, 배치 검증 | 플레이어 입력, 카메라, 최종 UX |
| 지면·충돌 | Line Trace 표본, 경사·높이 편차, Visibility Overlap | 지형/Collision 저작 품질 |
| Navigation | Anchor·Queue Slot 투영과 동기 경로 연결성 검사 | 실제 AI 이동, 비동기 Nav rebuild, 회피 |
| 보호 영역 | NoBuild·일반·긴급·접근성·관람 영역과 OBB 교차 판정 | 실제 법규·현장 안전 승인 |
| 시설 수명주기 | Install·Move·Remove·Open·Close·EmergencyClose | 경제·시간·UI 등 외부 의미 |
| Queue | FIFO, 용량, 서비스, 포기, Drain, Blocked, Exit, 통계 | Character/Controller/StateTree 이동 구현 |
| 외부 연동 | Policy 인터페이스와 원자적 예약·Commit·Rollback 순서 | 각 도메인의 실제 상태와 Adapter |
| 저장 | Schema 8 Snapshot 생성, 검증, 논리적 원자 복원 | Save Slot·파일·클라우드 I/O |
| 조회·이벤트 | 정렬된 Read Model, 성공/거절 이벤트 | 운영 UI, 분석 저장소, 텔레메트리 |
| 멀티플레이 | 구현하지 않음 | Server Authority, RPC, Replication, 재접속 |

여기서 “권한”은 `UFFZSWorldSubsystem` 안에서 상태 변경 경로를 하나로 제한한다는 뜻이다.
네트워크 서버 권한이나 Unreal Replication이 구현되어 있다는 뜻은 아니다.

---

## 2. 모듈과 전체 구조

`FF_ZoneSystem.uplugin`은 세 모듈을 선언한다.

| 모듈 | 유형 / Loading Phase | 역할 |
|---|---|---|
| `FF_ZoneSystem` | Runtime / Default | 실제 게임에서 사용하는 모든 타입과 로직 |
| `FF_ZoneSystemEditor` | Editor / PostEngineInit | 시설 상세 창·관람 수용량·에셋 데이터 검증 |
| `FF_ZoneSystemTests` | Editor / PostEngineInit | World·Policy·Save 통합 자동화 |

Runtime 모듈은 `Core`, `CoreUObject`, `Engine`, `DeveloperSettings`,
`NavigationSystem`에 의존한다. 모듈의 `StartupModule`과 `ShutdownModule`은 전역 상태를
만들지 않는다. 실제 상태는 월드마다 생성되는 `UFFZSWorldSubsystem`이 소유한다.

~~~mermaid
flowchart TD
    UI["UI / Placement Tool / Game Code"] --> CMD["FFFZSCommand"]
    CMD --> WS["UFFZSWorldSubsystem<br/>월드별 논리 권위"]
    PREVIEW["AFFZSPlacementPreviewActor<br/>비권위 미리보기"] --> WS

    WS --> ZONE["AFFZSGameplayZone"]
    WS --> FAC["AFFZSFacilityActor"]
    WS --> PA["AFFZSProtectedArea"]
    WS --> GRID["UFFZSGridLibrary"]
    FAC --> QUEUE["UFFZSFacilityQueueComponent"]

    WS --> POLICY["IFFZSExternalPolicy<br/>Nav·Economy·Time·UI·Safety·Experience"]
    QUEUE --> AGENT["IFFZSQueueAgent<br/>NPC 이동 계약"]
    WS --> SNAP["FFFZSWorldSnapshot / FFFZSReadModel"]
~~~

### 2.1 주요 타입별 책임

| 타입 | 책임 |
|---|---|
| `UFFZSWorldSubsystem` | Registry, 압축 점유, 명령, Policy, Queue 소유권, Snapshot, Read Model |
| `AFFZSGameplayZone` | 경계 Spline, Zone 규칙, 좌표 변환, 수용량과 밀도 |
| `AFFZSProtectedArea` | 설치 금지·보호 동선 볼륨과 교차 판정 |
| `UFFZSFacilityDefinition` | 시설 `FFFZSFacilitySpec`을 담는 Primary Data Asset |
| `AFFZSFacilityActor` | 설치된 시설의 Runtime 상태와 표현 Component |
| `UFFZSFacilityQueueComponent` | 시설별 FIFO Queue와 서비스 시뮬레이션 |
| `UFFZSGridLibrary` | 상태 없는 1 cm 격자·압축 Rect 계산과 명령 생성 |
| `AFFZSPlacementPreviewActor` | 검증 결과와 Ghost를 보여 주는 비권위 Preview |
| `IFFZSExternalPolicy` | 외부 시스템의 사전 검증·임시 적용·Commit·Rollback 계약 |
| `IFFZSQueueAgent` | NPC의 Stable ID와 Queue 이동 Callback 계약 |

---

## 3. 월드 권한과 초기화

### 3.1 지원 월드

`UFFZSWorldSubsystem`은 다음 World Type에서 생성된다.

- Game
- PIE
- Editor
- Editor Preview
- Game Preview

### 3.2 초기 등록

Subsystem 초기화 순서는 다음과 같다.

1. Project Settings의 `InitialPhase`를 읽는다.
2. 월드에 이미 존재하는 `AFFZSGameplayZone`을 검색해 등록한다.
3. `AFFZSProtectedArea`를 검색해 등록한다.
4. 유효한 `FacilityInstanceId`를 가진 `AFFZSFacilityActor`를 검색해 등록한다.

각 Zone과 Protected Area도 `BeginPlay`에서 자신을 등록하고 `EndPlay`에서 해제한다.
Facility는 Install 과정에서 Spawn된 직후에는 GUID가 아직 없기 때문에 자동 등록되지 않고,
`InitializeFacility`가 끝난 뒤 Subsystem이 명시적으로 등록한다. 따라서 초기화되지 않은 시설이
권위 Registry에 잠시 노출되지 않는다.

### 3.3 Registry와 점유 상태

Subsystem이 유지하는 핵심 Registry는 다음과 같다.

- `ZoneId -> GameplayZone`
- `FacilityInstanceId -> FacilityActor`
- `FacilityInstanceId -> ZoneId + ReservedRects`
- 등록된 Protected Area 목록
- 등록 순서가 결정적으로 정렬된 External Policy 목록
- `AgentId -> FacilityInstanceId` 전역 Queue 소유권
- `CommandId -> CommandReceipt` 멱등성 영수증

`UnregisterZone`은 Zone만 Registry에서 제거한다. 해당 Zone의 시설을 자동 이동하거나 제거하지 않으며,
`WorldRevision`과 변경 이벤트도 만들지 않는다. 시설의 실제 제거는 반드시 `Remove` 명령을 사용해야 한다.

### 3.4 변경 직렬화와 재진입 방지

`ExecuteCommand`, `RestoreWorldSnapshot`과 mutation guard를 거치는 공개 변경 API는 Game Thread에서
실행하며 재진입을 fail-closed한다. 내부 Facility 등록·해제도 Game Thread 수명주기에서 호출할
계약이지만, 등록·해제 함수 자체에는 별도 Thread Guard가 없다. 다음 상황의 요청은 거부된다.

- 다른 Command가 실행 중일 때
- Snapshot 복원이 진행 중일 때
- External Policy Callback 안에서 재귀 Command를 호출할 때
- Policy 등록·해제 또는 Queue 변경이 권한 작업과 겹칠 때
- Game Thread 밖에서 Command 또는 Restore를 호출할 때

이 구조는 하나의 UWorld 안에서 논리 상태를 직렬화하지만, 멀티플레이 Replication을 대신하지 않는다.

---

## 4. 고정 1 cm 격자와 압축 영역

### 4.1 단위 계약

- `1 uu = 1 cm`
- `1 Cell = 1 cm`
- Zone 로컬 XY 정수 좌표가 배치 Cell 좌표다.
- `PlacementCellSizeCm`은 설정으로 바꾸는 수치가 아니라 코드의 고정 계약이다.

과거의 100 cm Cell과 혼용하면 안 된다. Cell 크기를 바꾸려면 좌표, 시설 Data Asset, Command 지문,
Snapshot을 모두 마이그레이션해야 한다.

### 4.2 반열림 Rect

권위 공간은 개별 1 cm Cell 배열이 아니라 `FFFZSCellRect`로 저장한다.

~~~text
[Min, Max)
Min은 포함
Max는 제외
Width  = Max.X - Min.X
Height = Max.Y - Min.Y
~~~

예를 들어 6 m x 3 m 시설은 Cell 180,000개를 저장하지 않고 보통 다음 Rect 하나로 표현한다.

~~~text
Min = (0, 0)
Max = (600, 300)
~~~

Rect는 다음 연산을 지원한다.

- 0·90·180·270도 회전
- 겹치거나 맞닿는 Rect들의 결정적 Canonical 합집합
- Rect 집합 간 실제 면적 겹침 검사
- 합집합 면적 계산
- 작은 영역에 한한 개별 Cell Materialize

### 4.3 시설의 네 공간

| 영역 | 의미 | 충돌 시 실패 |
|---|---|---|
| `FootprintRects` | 시설 본체 | `PlacementCollision` |
| `ClearanceRects` | 본체와 Queue를 제외한 필수 여유 공간 | `InsufficientClearance` |
| `QueueRects` | 만석일 때까지 예약하는 최대 대기 공간 | `QueueFootprintBlocked` |
| `ReservedRects` | 위 세 영역의 정확한 합집합 | Zone 소유·보호 영역·Snapshot 판정 |

정상 권위 로직은 Rect 집합만 사용한다. Blueprint 호환과 Debug용 개별 Cell 배열은 영역이
10,000 Cell 이하일 때만 생성되며, 이를 초과하면 빈 배열을 반환한다.

### 4.4 회전과 배치 원점

`FFFZSGridPlacement`은 다음 두 값으로 구성된다.

- `OriginCell`: 회전 전 시설 Footprint의 좌하단
- `QuarterTurns`: 시계 방향 90도 회전 수

권위 명령 생성, 시설 초기화·이동, 격자 계산 경계에서 `QuarterTurns`를 `0..3`으로 정규화한다.
수동으로 만든 raw `FFFZSGridPlacement` 필드 자체에는 임의 정수가 들어갈 수 있다. 시설 월드
Transform은 회전된 Footprint 중심과 Zone Actor의 Yaw를 조합하며 Actor Scale은 항상 1이다.

### 4.5 안전 예산

`MaxEnumeratedCellsPerFacility` 기본값은 20,000,000이다. 이 값은 원본 크기의 대형 무대를 포함한 정상 시설을 실제 Cell로
펼치는 한도가 아니라, 잘못된 Footprint·Clearance·Queue 입력을 빠르게 거부하기 위한 보수적 예산이다.
설정값도 하드 상한 20,000,000으로 제한된다. 개별 디버그 Cell 배열의 상한은 계속 10,000개다.
권위 점유·충돌 판정은 Cell이 아니라 압축 사각형 단위로 동작하므로 이 예산을 올려도 메모리와
판정 비용은 늘지 않는다. 다만 지면 표본 수는 Footprint 면적에 비례하므로
`GroundSampleSpacingCm`을 과도하게 낮춘 채 대형 시설을 배치하면 LineTrace 비용이 급증한다.

---

## 5. Gameplay Zone

### 5.1 Zone이 보유하는 데이터

`AFFZSGameplayZone`의 주요 필드는 다음과 같다.

| 필드 | 의미 |
|---|---|
| `ZoneId` | Registry·저장·점유·그래프에서 사용하는 Stable ID |
| `DisplayName` | UI 표시명 |
| `SourceReferenceId` | 외부 참고 경계와 연결하는 비권위 ID |
| `SchemaVersion / ContentVersion` | 데이터 구조·내용 버전 |
| `BoundarySpline` | Zone 로컬 cm 좌표의 닫힌 선형 폴리곤 |
| `VerticalExtentCm` | Actor 원점 중심 수직 허용 두께 |
| `bAllowAllFacilityCategories` | 모든 시설 카테고리 허용 여부 |
| `AllowedFacilityCategories` | 선택 허용 카테고리 |
| `NeighborZoneIds` | 방향 없는 Zone 그래프의 이웃 |
| `EntryExitAnchors` | Zone 단위 출입 기준점 |
| `UsablePublicAreaSquareMeters` | 밀도 계산용 공공 면적 Override |
| `SoftCapacityOverride` | 소프트 수용량 Override |

### 5.2 저작 불변식

유효한 Zone은 다음 조건을 만족해야 한다.

- 비어 있지 않고 Project Settings 정책에 맞는 `ZoneId`
- 점이 3개 이상인 Closed Loop `BoundarySpline`
- 모든 Spline Point가 Linear 보간
- 자기 교차, 중복 Edge Point, 퇴화 면적이 없는 단순 폴리곤
- Actor Scale `(1,1,1)`
- X/Y 원점이 1 cm 격자에 정렬
- Pitch/Roll 0, Yaw 0/90/180/270도
- 자신을 이웃으로 포함하지 않음
- 비어 있거나 중복된 Neighbor ID가 없음
- 전체 카테고리 허용을 끈 경우 허용 카테고리가 하나 이상 존재

Zone 등록 후 `ZoneId`를 직접 바꾸면 Registry Key와 Actor 값이 달라질 수 있다.
`ApplyAcceptedZonePreset`도 이미 등록된 Actor에는 적용되지 않는다.

### 5.3 기본 Project Zone ID

기본 설정은 다음 12개 ID로 Zone을 제한한다.

~~~text
GZ_SunsetBeachClub
GZ_DadaPocha
GZ_DadaYajang
GZ_DadaepoForest
GZ_FountainPirates
GZ_SunsetLounge
GZ_FleaMarket
GZ_SunsetDogPark
GZ_SunsetWellness
GZ_PublicParkingWest
GZ_TemporaryParkingEast
GZ_Operations
~~~

다른 프로젝트로 이식할 때는 `ProjectZoneIds`를 자체 Stable ID로 교체하거나
`bRestrictZoneIdsToProjectPresets`를 끌 수 있다.

### 5.4 포함 판정과 Hysteresis

`ContainsWorldPoint`는 월드 위치를 Zone 로컬 좌표로 바꾼 뒤 다음을 모두 검사한다.

1. Z가 `VerticalExtentCm / 2` 안에 있는가
2. XY가 폴리곤 내부 또는 경계 0.1 cm 안에 있는가

`ResolveZoneAtWorldLocation`은 이전 Zone ID가 주어졌을 때 그 Zone에만
`ZoneMembershipHysteresisCm`를 적용한다. 이전 Zone의 완충 범위에도 없으면 Zone ID 사전순으로
정확한 경계를 검사한다. 모든 Zone의 경계를 넓히는 방식이 아니므로 새 Zone을 선점하지 않는다.

### 5.5 Zone 그래프

Actor 단위 `ValidateZoneData`는 자기 데이터만 검사한다. 월드 전체 `ValidateZoneGraph`는 추가로
다음을 검사한다.

- 제한 모드에서 Project Zone ID가 모두 존재하는가
- Neighbor가 실제로 존재하는가
- A가 B를 이웃으로 가지면 B도 A를 이웃으로 가지는가
- Protected Area ID가 고유한가
- Zone 전용 Protected Area가 실제 Zone을 가리키는가

Zone ID 중복은 `ValidateZoneGraph`가 아니라 `RegisterZone`이 Registry 삽입 전에 거부한다.
Zone 간 공간 겹침은 등록 시 전역 거부하지 않는다. 대신 시설 `ReservedRects`를 검증할 때 모든 영역이
“정확히 하나의 Zone”에만 완전히 속해야 한다. 두 Zone이 겹치는 부분이나 Zone 밖 부분에 걸친 배치는
`OutsideZone`으로 거부된다.

### 5.6 수용량과 밀도

Soft Capacity 계산은 다음 순서다.

~~~text
SoftCapacityOverride > 0
    → Override 사용
그 외
    → (UsablePublicAreaSquareMeters 또는 폴리곤 면적) / 1인당 면적
    → 내림
~~~

기본 1인당 면적은 4 m²다. `ReportZonePopulation`으로 외부 시뮬레이션이 인구와 DeltaSeconds를
보고하면 다음 단계가 계산된다.

| 상태 | 기본 비율 | 상승 규칙 |
|---|---:|---|
| Normal | 0.8 미만 | 즉시 |
| Caution | 0.8 이상 | 즉시 |
| Warning | 1.0 이상 | 5초 연속 유지 후 |
| Critical | 1.25 이상 | 3초 연속 유지 후 |

기준 아래로 내려가면 누적 시간이 초기화되고 현재 비율에 따라 즉시 하향 재평가된다. 별도의 하향
Threshold를 둔 대칭형 Hysteresis는 아니다. 이 수치는 게임 Prototype용이며 실제 법정 수용량이 아니다.

---

## 6. Protected Area

`AFFZSProtectedArea`는 물리 Overlap 이벤트를 발생시키는 Gameplay Trigger가 아니라,
배치 검증 시 Subsystem이 직접 질의하는 공간 규칙 Actor다.

### 6.1 유형

| 유형 | 의미 | 교차 실패 |
|---|---|---|
| `NoBuild` | 설치 금지 영역 | `ProtectedRouteBlocked` |
| `GeneralRoute` | 일반 통행 동선 | `ProtectedRouteBlocked` |
| `EmergencyServiceRoute` | 긴급·서비스 동선 | `ProtectedRouteBlocked` |
| `AccessibilityRoute` | 접근성 동선 | `AccessibilityRouteBlocked` |
| `ViewingArea` | 보호 관람 영역 | `AccessibilityRouteBlocked` |

### 6.2 적용 Zone

- `LinkedZoneId = NAME_None`: 모든 Zone 배치에 적용
- Zone ID 지정: 해당 Zone의 배치에만 적용
- 존재하지 않는 Zone ID 지정: `DataInvalid`

현재 Runtime 구현은 `NAME_None`을 전역 영역으로 처리한다. Public Header의 해당 Property 설명은
“비어 있으면 검증 실패”라고 적혀 있어 구현과 문구가 어긋나므로, 향후 API 주석을 구현에 맞게
정리할 필요가 있다.

### 6.3 폭과 교차 판정

`RequiredClearWidthCm`이 양수면 Actor별 Override다. 0이면 유형별 Project Settings 값을 상속한다.

| 유형 | 기본 폭 |
|---|---:|
| General Route | 300 cm |
| Emergency/Service Route | 400 cm |
| Accessibility Route | 150 cm |
| Viewing Area | 150 cm |
| NoBuild | 추가 폭 0 cm |

`ApplyDefaultWidthForType`은 현재 기본값을 Actor 값으로 복사하므로 이후 전역 설정 변경을 상속하지 않는다.

교차는 Yaw가 적용된 OBB와 시설 영역의 월드 AABB를 Separating Axis Test로 검사한다.
요구 폭은 보호 Actor의 로컬 짧은 축을 확장하며, Z 구간이 분리되면 교차하지 않는다. 경계가 맞닿는
경우도 차단으로 본다. 정확한 판정을 위해 Scale은 1, Pitch/Roll은 0이어야 한다.

---

## 7. 시설 정의와 Runtime 시설

### 7.1 Data Asset

`UFFZSFacilityDefinition`은 `UPrimaryDataAsset`이며 Primary Asset Type은
`FFZSFacilityDefinition`이다. `Spec`이 유효한 경우 Primary Asset 이름은
`FacilityDefinitionId`를 사용한다.

설치 시 `FFFZSFacilitySpec` 전체를 `AFFZSFacilityActor`에 값으로 복사한다. 따라서 다음 특징이 있다.

- 원본 Data Asset이 언로드되어도 설치된 시설과 Snapshot은 동작한다.
- 원본 Data Asset을 수정해도 이미 설치된 시설에 자동 반영되지 않는다.
- Visual Mesh는 Soft Reference라 논리 설치와 로드를 분리한다.
- Facility Audio도 Soft Reference라 시설별 사운드와 오디오 정책을 Data Asset에서 교체할 수 있다.
- Host 프로젝트가 Asset Manager Scan/Cook 규칙을 별도로 설정해야 할 수 있다.

### 7.2 FacilitySpec 주요 필드

| 그룹 | 필드 | 의미 |
|---|---|---|
| Identity | `SchemaVersion` | 현재 지원값 2 |
| Identity | `FacilityDefinitionId / DisplayName / Category` | Stable ID, 표시명, 카테고리 |
| Placement | `FootprintSizeCells` | 실제 cm와 같은 1 cm Cell 크기 |
| Placement | `ClearanceCells` | 전 방향 여유 폭 |
| Placement | `QueueReservedRects` | 최대 Queue 예약 영역 |
| Queue | `QueueSplinePointsCm` | Service에서 Queue Tail 방향의 로컬 점 |
| Queue | `QueueRules` | 용량·간격·서비스·포기·이동 규칙 |
| Anchors | `Anchors` | Entry·Service·Exit Transform |
| Placement | `AllowedZoneIds` | 비어 있으면 Zone ID 제한 없음 |
| External | `InstallCostKey / RevenueRuleKey / ExperienceEffectKey` | 외부 Policy용 Key |
| Visual | `VisualMesh` | 비동기 로드 Soft Mesh |
| Visual | `VisualSizingMode` | Production Definition은 `MatchPlacementBounds`; Footprint/ObstacleHeight가 표시 크기의 권위 |
| Visual | `VisualOffsetCm / VisualRotation` | 표시 전용 로컬 보정. Match 모드는 Offset 0, 정확한 90도 회전만 허용 |
| Audio | `AudioSettings` | 시설별 3D 사운드, 감쇠·동시재생 정책, 볼륨·피치·위치·페이드·반복 설정 |
| Collision | `PlacementObstacleHeightCm` | 월드 충돌과 보호영역 박스 높이 |

`QueueReservedCells`는 작은 구 데이터 호환용 Deprecated 필드다. 새 데이터는
`QueueReservedRects`를 사용해야 한다.

### 7.3 Spec 검증

`FFFZSFacilitySpec::IsValid`은 오류를 가능한 한 누적하되 위험한 반복 전에 값싼 조건을 먼저 검사한다.

- SchemaVersion이 2인가
- ID와 DisplayName이 존재하고 Category가 유효한가
- Footprint가 양수, Clearance가 0 이상인가
- 이론적 공간 작업량이 안전 예산 안인가
- Queue 수용량, Service 수, 거리, 시간이 유효한가
- Queue Spline이 2점 이상이고 모든 Slot을 놓을 만큼 긴가
- `QueueReservedRects` 또는 구 호환 `QueueReservedCells`가 존재하는가
- Rect 또는 Cell 좌표가 유효한가
- Queue 영역이 Facility Footprint와 겹치지 않는가
- 최대 인원의 모든 Queue Slot이 예약 영역 안에 있는가
- 서로 다른 Anchor와 Queue Slot의 XY 중심이 `max(1cm, 2 × SlotArrivalToleranceCm)` 이상 떨어지는가
- Entry[0]과 Exit[0]이 존재하는가
- Service Anchor가 `0..ServiceCapacity-1`을 연속 제공하는가
- Anchor `Type + Index` 조합이 고유하고 Local Scale이 1인가
- `AllowedZoneIds`에 `NAME_None`이나 중복이 없는가
- Facility Audio의 볼륨·피치·위치·페이드 값이 유한하고 허용 범위 안인가
- `PlacementObstacleHeightCm`이 유한하고 10 cm 이상인가

### 7.4 시설별 사운드 저작

시설 Definition Data Asset의 `Spec > Facility Audio`에서 설정한다.

- `Ambient Sound`: Sound Wave, Sound Cue, MetaSound Source 중 하나를 지정한다. 비우면 무음이다.
- `Override Audible Range`: 켜면 시설마다 구형 가청 범위를 직접 사용하며 최대 거리 밖의 볼륨은 0이다.
- `Full Volume Radius`: 원래 볼륨을 유지하는 반경이다. 기본값 500cm다.
- `Maximum Audible Distance`: 완전히 들리지 않게 되는 거리다. 기본값 2,000cm다.
- `Attenuation Settings`: 공간화·리버브·오클루전 정책을 재사용한다. 범위 Override가 켜져 있으면 거리 값은 시설 설정이 우선한다.
- `Concurrency Settings`: 같은 종류의 시설이 많을 때 최대 동시 재생 수와 우선순위를 제한한다.
- `Play Only While Active`: 기본값은 켜짐이며 Open 때 페이드 인, Close·Remove 때 페이드 아웃한다.
- `Loop`: 일회성 음원도 끝나면 다시 재생한다. 이음매가 중요한 루프는 Sound Cue/MetaSound 내부에서 만든다.

런타임은 사운드·감쇠·동시재생 에셋을 한 번에 비동기 로드하며 Actor Tick을 사용하지 않는다. 시설별
코드를 추가하거나 Blueprint 파생 클래스를 만들지 말고 같은 Data Asset 필드만 교체한다. 이미 설치되어
Snapshot에 들어간 시설은 설치 시점의 Spec을 유지하므로 저작 변경 확인 시에는 새로 설치하거나 저장을
다시 생성한다.

### 7.5 FacilityDefinition Data Asset 단일 원천

프로덕션 시설의 ID, 크기, 여유 영역, Queue 영역·Spline·규칙, Anchor, 외부 Key와 Visual Mesh는
`UFFZSFacilityDefinition::Spec`에서만 저작한다. `UFFZSGridLibrary`에는 시설별 값이나 기본 시설 생성
함수가 없으며, Catalog·배치 Preview·Install Command·FacilityActor·Queue·SaveGame은 선택한 Data
Asset에서 복사한 동일한 `FFFZSFacilitySpec`을 소비한다.

명령과 설치된 Actor가 Spec 복사본을 보관하는 이유는 실행 중 Asset 수정이나 비동기 언로드가 이미
검증된 명령과 저장 게임을 바꾸지 못하게 하기 위해서다. 이 복사본은 별도 저작 원천이 아니라 해당
명령/인스턴스가 생성된 시점의 불변 Snapshot이다. 시설 수치를 바꾸려면 C++ 상수를 수정하지 말고
해당 `UFFZSFacilityDefinition` Data Asset을 수정하고 검증·Cook한다.

자동화 테스트에 필요한 대표 모양은 `WITH_DEV_AUTOMATION_TESTS`에서만 보이는
`Tests/FF_FFZSTestFixtures.h`의 `FFZSTest::MakeValidFacilitySpec`으로 중앙화되어 있다. 이 fixture는
Shipping API나 Asset Manager Scan 대상이 아니며 프로덕션 시설 정의로 사용하면 안 된다.

#### 공개 built-in API 제거 마이그레이션

단일 원천 전환에서 공개 reflected API였던 `EFFZSBuiltInFacilityPreset`,
`UFFZSGridLibrary::MakeBuiltInFacilitySpec`, `AFFZSPlacementPreviewActor::SetBuiltInPreview`를 제거했다.
프로덕션 C++ 호출은 모두 Definition 경로로 이관했다. 기존 통합 테스트 Blueprint에는 deprecated 호환 노드가
남아 있을 수 있으므로, 아래 순서로 교체하고 Compile·Save한 뒤 Data Validation 경고가 0인지 확인한다.

- enum/preset 선택은 `UFF_BuildCatalogSubsystem`의 정렬된 `UFFZSFacilityDefinition` 선택으로 교체한다.
- `MakeBuiltInFacilitySpec` 호출은 선택한 Definition을 직접 받는
  `MakeInstallCommandFromDefinition` 경로로 교체한다. 외부 코드가 `Spec`을 별도 원천처럼 복제해서는 안 된다.
- `SetBuiltInPreview` 호출은 선택한 `UFFZSFacilityDefinition`을
  `MakeInstallCommandFromDefinition`에 전달하고, 그 결과를 Preview의 `PreviewCommand`에 설정한 뒤
  `RefreshPreview`를 호출하는 흐름으로 교체한다.
- 구 노드가 들어 있는 외부 Blueprint는 해당 노드를 교체한 뒤 전부 Compile·Save한다.
- exact-name CI에서 구 `CompressedBuiltInPresets` 테스트를 호출했다면
  `OneCentimeterTestFacilityFixtures` 또는 상위 `FF_ZoneSystem` prefix로 바꾼다.

enum pin과 함수 시그니처가 달라 Core Redirect만으로 안전하게 자동 변환할 수 없으므로, 이 변경은
외부 API 관점에서 명시적 마이그레이션이 필요한 breaking change다.

### 7.6 FacilityActor

`AFFZSFacilityActor`는 다음 Component를 가진다.

- Scene Root
- 비동기 Soft Mesh를 표시하는 Visual Mesh
- Footprint Bounds
- Queue Area Bounds
- Queue Spline
- `UFFZSFacilityQueueComponent`
- Spec Anchor에서 동적으로 만든 `UFFZSFacilityAnchorComponent` 목록

Actor 자체는 Tick하지 않는다. Mesh 로드 실패는 표현 실패일 뿐 점유·명령·비용 상태를 자동 Rollback하지
않는다. 명령 수명주기 변경은 `UFFZSWorldSubsystem`이 호출한다. `UFFZSFacilityQueueComponent`도
friend로서 Queue 상태가 바뀔 때 `HandleQueueStateChanged`를 호출하여 Drain 완료를
`MovePending` 상태에 반영한다.

---

## 8. 배치 검증

### 8.1 Preview와 실행의 차이

`ValidatePlacement`는 현재 시점의 읽기 검증이다. 상태를 바꾸지 않는다.
`ExecuteCommand`는 실행 직전에 같은 검증과 Policy를 다시 적용한다.

따라서 Preview가 초록색이어도 다음 사이에 다른 시설이 설치되거나 Revision, Phase, 외부 Policy가
바뀌면 Commit은 실패할 수 있다.

### 8.2 검증 순서

Install 또는 Move의 로컬 검증은 다음 순서로 진행된다.

1. 명령 유형과 현재 Change Phase
2. FacilityInstanceId와 Install 중복 / Move 대상 존재
3. FacilitySpec 전체 데이터 계약
4. 대상 Zone 존재와 Zone 데이터 계약
5. Zone Category와 Facility `AllowedZoneIds`
6. 회전 정규화와 Footprint·Clearance·Queue·Reserved Rect 생성
7. 모든 Reserved Rect가 정확히 하나의 Zone에 완전히 포함되는지
8. 기존 시설 점유와 Footprint 충돌
9. 기존 시설 점유와 Clearance 충돌
10. 기존 시설 점유와 Queue 충돌
11. Protected Area 교차
12. 지면 표본
13. 월드 Collision Overlap
14. Navigation 투영과 경로 연결성
15. 로컬 검증 성공 시 등록된 Policy의 `PreValidate`

Move 검증에서는 호출자가 넣은 Spec을 무시하고 기존 FacilityActor가 보유한 Spec으로 덮어쓴다.
Move 요청으로 시설 정의를 몰래 바꿀 수 없도록 하기 위한 규칙이다.

### 8.3 지면 검사

`bEnableGroundChecks`가 켜져 있으면 Footprint를 `GroundSampleSpacingCm` 이하 간격으로 균등 표본화한다.
각 표본에서 Visibility Line Trace를 위·아래로 수행한다.

- 작은 시설도 최소 1개 표본 사용
- 기본 표본 간격 100 cm
- 모든 표본 요구 여부는 `bRequireEveryGroundSample`로 제어
- 성공 표본이 없으면 `InvalidGround`
- 최대 법선 경사가 설정값을 넘으면 `ExcessSlope`
- 최고·최저 Z 차가 설정값을 넘으면 `HeightVarianceExceeded`
- 통과하면 평균 지면 높이로 Placement Transform Z를 보정

1 cm 점유 해상도와 물리 Trace 밀도는 의도적으로 분리되어 있다.

### 8.4 월드 충돌

`bEnableWorldCollisionChecks`가 켜져 있으면 각 압축 Rect를 Box Overlap으로 검사한다.

- Footprint 충돌 → `PlacementCollision`
- Clearance 충돌 → `InsufficientClearance`
- Queue 충돌 → `QueueFootprintBlocked`

Visibility Channel을 기준으로 하며 GameplayZone, ProtectedArea, FacilityActor는 이 물리 검사에서
제외된다. 시설 간 충돌은 별도 압축 점유 Map이 판정하고, ProtectedArea는 별도 OBB 규칙이 판정한다.

### 8.5 Navigation 검사

`bEnableNavigationProjectionChecks`가 켜져 있으면 다음을 검사한다.

1. Navigation System 존재 여부
2. 모든 Entry·Service·Exit Anchor를 Nav에 투영할 수 있는지
3. 최대 대기 인원의 모든 Queue Slot을 Nav에 투영할 수 있는지
4. 첫 Entry에서 나머지 Anchor와 Queue Slot까지 완전한 비부분 경로가 있는지

투영 Extent는 전역 설정과 Facility Queue 규칙 중 작은 값에 1 cm 하한을 적용한다. Navigation
System이 없을 때 거부할지는 `bFailWhenNavigationSystemIsUnavailable`로 정한다.

이 검사는 현재 Nav 데이터에서의 동기 투영·경로 확인이다. 설치 후 비동기 NavMesh 재빌드, Timeout,
취소, AI 이동은 외부 Navigation Policy가 담당해야 한다.

---

## 9. Command와 시설 수명주기

시설의 `Install`, `Move`, `Remove`, `Open`, `Close`, `EmergencyClose` 권위 변경은
`UFFZSWorldSubsystem::ExecuteCommand` 한 경로로 보낸다.
FacilityActor의 Transform이나 QueueComponent 내부 배열을 직접 수정하면 Registry, 점유, Policy,
영수증, 이벤트가 서로 어긋날 수 있다.

### 9.1 Command 데이터

`FFFZSCommand`은 다음 정보를 가진다.

| 필드 | 역할 |
|---|---|
| `CommandId` | 멱등 재시도의 고유 Key |
| `TransactionId` | 여러 외부 작업을 연관시키는 상위 Transaction ID |
| `ExpectedWorldRevision` | 낙관적 동시성 조건. -1이면 최신 Revision 허용 |
| `Type` | Install, Move, Remove, Open, Close, EmergencyClose |
| `FacilityInstanceId` | 대상 또는 새 시설 GUID |
| `FacilitySpec` | Install의 전체 Spec. Move에서는 기존 Spec 사용 |
| `ZoneId` | Install·Move 대상 Zone |
| `Placement` | Cell 원점과 사분면 회전 |

신규 프로덕션 코드는 `MakeInstallCommandFromDefinition`만 사용한다. 이 함수는 선택된
`UFFZSFacilityDefinition::Spec`으로 새 CommandId, TransactionId, FacilityInstanceId를 만들고 회전을
정규화한다. Spec을 직접 받는 `MakeInstallCommand`는 기존 Blueprint 자산을 위한
`DevelopmentOnly`·deprecated 호환 표면이다. 프로덕션 실행에서는 등록된 Definition을 다시 찾아 정확한
Spec으로 정규화하므로 제2의 시설 데이터 원천으로 사용해서는 안 된다. 다른 명령을 직접 만들 때도
CommandId와 TransactionId가 모두 유효해야 한다.

### 9.2 Phase 규칙

| Phase | Install | Move | Remove | Open / Close / EmergencyClose |
|---|---:|---:|---:|---:|
| InitialSetup | 허용 | 허용 | 허용 | 허용 |
| MorningCheck | 허용 | 허용 | 허용 | 허용 |
| LiveOperation | 거부 | 거부 | 거부 | 허용 |
| EveningSettlement | 거부 | 거부 | 거부 | 허용 |

`SetChangePhase`는 Phase만 바꾼다. 그 자체로 `WorldRevision`을 증가시키거나
`OnWorldChanged`를 방송하지 않는다.

### 9.3 Facility 상태 전이

~~~mermaid
stateDiagram-v2
    [*] --> Validated: Install
    Validated --> Active: Open
    Closed --> Active: Open
    MovePending --> Active: Open
    Planned --> Validated: Move
    Validated --> Validated: Move
    Closed --> Validated: Move
    MovePending --> Validated: Move
    Planned --> Removed: Remove
    Validated --> Removed: Remove
    Closed --> Removed: Remove
    MovePending --> Removed: Remove
~~~

상태 의미:

| 상태 | 의미 |
|---|---|
| Planned | 아직 검증·배치 확정 전인 상태 |
| Validated | 설치 또는 이동이 확정됐지만 영업하지 않는 상태 |
| Active | Queue 참가와 서비스가 가능한 상태 |
| Closed | 영업 중지. Emergency 후 또는 일반 Drain 진행 중일 수 있음 |
| MovePending | 일반 Close 후 Queue가 완전히 비어 이동·제거 가능한 상태 |
| Removed | 논리 제거 후 Actor 파괴 직전 상태 |

`CanMoveOrRemove`는 Queue의 Waiting·ActiveServices·Exiting이 모두 비어 있고 상태가
Planned, Validated, Closed, MovePending 중 하나일 때만 true다. `Open`만 입력 상태를
Validated, Closed, MovePending으로 제한한다. 현재 `Close`와 `EmergencyClose`는 Registry에
존재하는 모든 비-Removed 상태에서 실행된다. 위 그림은 상태 제한이 명확한 Install·Open·Move·
Remove만 표시하며, Close 계열의 Queue 연동 결과는 아래 명령 설명을 따른다.

### 9.4 명령별 원자적 처리

#### Install

1. 배치와 Policy 사전 검증
2. External Policy 임시 적용
3. `AFFZSFacilityActor` Spawn
4. Spec·Zone·Placement로 초기화
5. Registry와 압축 점유 등록
6. Policy Commit
7. Revision 증가, Receipt 저장, 이벤트 방송

Actor 생성·초기화·등록 중 하나라도 실패하면 생성 Actor를 파괴하고 적용된 Policy를 역순 Rollback한다.

#### Move

1. `ValidatePlacement`에서 대상 조회, 기존 Spec 기반 로컬 배치 검증, Policy `PreValidate`
2. `ExecuteMove`에서 Queue 비점유와 Facility 상태 확인
3. Policy `ApplyTentative`
4. 이전 Zone·Placement·Transform·점유 보관
5. 이전 점유 해제
6. 새 배치 적용과 점유 등록
7. 실패하면 이전 상태·점유 복원 후 Policy Rollback
8. 성공하면 Policy Commit과 Revision 증가

#### Remove

Queue가 완전히 비고 이동·제거 가능 상태여야 한다. Policy Transaction이 성공하면 Registry와 점유를
해제하고 Actor를 Removed로 만든 뒤 성공 이벤트 정보를 준비하고 Actor를 파괴한다.

#### Open

Validated, Closed, MovePending에서 Active로 전환하고 Queue를 연다. 다른 상태는
`InvalidFacilityState`다.

#### Close

현재 구현은 Active 전용으로 제한하지 않는다. 등록된 비-Removed 시설을 Closed로 만들고 신규 참가를
막은 뒤 기존 Waiting, Service, Exiting을 계속 진행하는 Drain을 시작한다. Queue가 이미 비어 있으면
같은 호출에서 바로 MovePending이 되고, 점유가 있으면 모두 해제된 뒤 MovePending이 된다.

#### EmergencyClose

현재 구현은 Active 전용으로 제한하지 않는다. Facility를 Closed로 만들고 Waiting과 Service 항목을
즉시 취소한다. 실제 QueueAgent Actor가 있으면 Exit 이동을 요청하고,
Actor가 없는 논리 항목은 즉시 해제한다. 물리 Agent가 Exit에 도착하기 전에는 Facility가 Closed여도
Queue 소유권이 남아 있어 이동·제거할 수 없다.

---

## 10. 멱등성, Revision, 이벤트

### 10.1 Command 영수증

플러그인은 `CommandId`를 Key로 전체 의미 Payload 지문과 결과를 저장한다.

- 같은 CommandId + 같은 Payload: 저장 결과를 반환하고 `bWasDuplicateReplay=true`
- 같은 CommandId + 다른 Payload: `DuplicatePayloadMismatch`
- Replay는 새 Revision, 새 Actor, 새 비용, 새 이벤트를 만들지 않음
- 성공뿐 아니라 정식 실행 경계에서 확정된 실패 결과도 Receipt로 저장

따라서 데이터나 상황을 수정한 뒤 다시 시도할 때는 새 CommandId를 사용해야 한다.

지문에는 TransactionId, ExpectedWorldRevision, Type, Facility ID, Zone, Placement, Spec의 의미 데이터가
들어간다. CommandId 자체는 Lookup Key이므로 지문에서 제외된다.

### 10.2 낙관적 동시성

`ExpectedWorldRevision >= 0`이면 현재 Revision과 정확히 같아야 한다. 다르면
`StaleWorldRevision`이다. `-1`은 실행 시점 최신 상태를 허용한다.

`WorldRevision`은 일반 실행 중 성공한 다음 명령에서만 1 증가한다. `RestoreWorldSnapshot`은 예외로
Snapshot의 저장값으로 교체하므로 값이 낮아질 수도 있다.

- Install
- Move
- Remove
- Open
- Close
- EmergencyClose

다음 변경은 Revision을 증가시키지 않는다.

- Preview·Validate
- Phase 변경
- Zone·Protected Area 등록/해제
- Population·Density 갱신
- Queue 내부 진행과 참가·해제
- Policy 등록/해제

따라서 WorldRevision은 “월드의 모든 변화 버전”이 아니라 “성공한 시설 Command 버전”이다.

### 10.3 World 이벤트

`OnWorldChanged`는 성공 Command에 대해 다음 정보를 전달한다.

- EventId
- CommandId
- TransactionId
- Command Type
- FacilityInstanceId
- PreviousZoneId
- NewZoneId
- 확정 WorldRevision

이 이벤트는 로컬 변경, Policy Commit, Receipt 준비가 끝난 뒤 방송된다.

`OnPlacementRejected`는 정상 Command 실행 경계에서 최종 실패했을 때 원본 Command와 Result를
전달한다. 다만 Game Thread 위반, 잘못된 필수 ID, 복원/Policy 재진입, 기존 Receipt Replay나
Payload Mismatch처럼 함수 초입에서 즉시 반환하는 경우에는 이 Delegate가 방송되지 않는다.

이벤트 Callback 동안에도 현재 Command Guard가 유지된다. 구독자가 동기적으로 새 Command를 실행하면
거부되므로 다음 Frame이나 별도 안전한 진입점으로 예약해야 한다.

---

## 11. External Policy Transaction

플러그인은 실제 Economy, Simulation Time, 운영 UI, Safety 공식 등을 구현하지 않는다.
대신 `IFFZSExternalPolicy`로 각 외부 시스템을 같은 Command Transaction에 참여시킨다.

### 11.1 Policy Domain

- Navigation
- Economy
- SimulationTime
- UserInterface
- Safety
- Experience
- Custom

Policy 이름은 전체 Registry에서 고유해야 한다. Custom이 아닌 Domain은 Domain당 하나만 등록할 수 있다.
Custom은 이름이 다르면 여러 개 등록할 수 있다.

실행 순서는 등록 호출 순서가 아니라 `Domain enum 순서 -> PolicyName 사전순`으로 정렬된다.

### 11.2 Transaction 순서

아래 그림은 정책 단계의 개념 순서다. 로컬 상태 검사의 위치는 명령별로 다르며, 특히 Move는
Placement와 Policy `PreValidate` 뒤에 Queue 점유·Facility 상태를 확인한다.

~~~mermaid
sequenceDiagram
    participant C as Caller
    participant W as WorldSubsystem
    participant P as External Policies
    participant L as Local Facility State

    C->>W: ExecuteCommand
    W->>W: Local validation
    W->>P: PreValidate (read only)
    P-->>W: Allow / Reject
    W->>P: ApplyTentative
    P-->>W: Reserved / Reject
    alt 로컬 적용 성공
        W->>L: Install / Move / Remove / Lifecycle
        W->>P: Commit (no-fail)
        W-->>C: Success + Revision
    else 임시 적용 또는 로컬 적용 실패
        W->>P: Rollback in reverse order
        W-->>C: Failure
    end
~~~

계약:

- `PreValidate`는 외부 상태를 변경하지 않는다.
- `ApplyTentative`에서 실패 가능한 예약을 모두 수행한다.
- ApplyTentative가 실패한 Policy도 부분 적용했을 수 있으므로 Rollback 대상이다.
- `Commit`은 실패하지 않아야 한다.
- `Rollback`은 역순으로 호출되며 여러 번 호출해도 안전한 멱등 구현이어야 한다.
- Callback은 짧고 동기적으로 끝내며, 내부에서 새 FFZS Command나 Registry 변경을 시작하지 않는다.

`bRequireProductionPolicyDomains=true`이면 Custom을 제외한 6개 Domain이 모두 준비되지 않은 Command를
`ExternalPolicyRejected`로 차단한다. 기본값은 독립 Prototype을 위해 false다.

Policy Registry는 약한 참조를 사용하므로 Host 프로젝트가 Provider 객체의 수명을 별도로 유지해야 한다.

---

## 12. 결정론적 FIFO Queue

`UFFZSFacilityQueueComponent`는 시설 하나의 Waiting, Service, Exit 흐름을 소유한다.
Facility와 WorldSubsystem을 제외한 코드는 내부 배열을 직접 바꿀 수 없다.

### 12.1 Queue 규칙

| 필드 | 기본값 | 의미 |
|---|---:|---|
| `MaxWaitingAgents` | 12 | Waiting 배열 최대 인원 |
| `SlotSpacingCm` | 115 cm | Queue Spline 위 Slot 간격 |
| `ServiceCapacity` | 1 | 동시 Service Slot 수 |
| `ServiceDurationSeconds` | 4초 | 실제 Service 도착 후 처리 시간 |
| `AbandonAfterSeconds` | 30초 | Waiting 포기 시간. 0이면 자동 포기 없음 |
| `SlotArrivalToleranceCm` | 25 cm | Queue·Service·Exit 도착 XY 허용 오차 |
| `NavigationProjectionExtentCm` | 30 cm | Queue 이동 목표 Nav 투영 범위 |
| `BlockedTimeoutSeconds` | 5초 | Queue 전체 재경로 요청 간격 |
| `MaxRepathAttempts` | 2 | Queue/Agent별 최대 재경로 횟수 |

Waiting 정원은 Active Service와 Exiting 인원을 포함하지 않는다. 전역 소유권은 세 단계 모두에서 유지된다.

### 12.2 Queue 상태

| 상태 | 신규 참가 | 기존 흐름 | Tick |
|---|---:|---:|---:|
| Open | 허용 | 진행 | 켜짐 |
| Full | 거부 | 진행 | 켜짐 |
| Draining | 거부 | 자연 종료 | 켜짐 |
| Blocked | 거부 | 신규 Service 배정 정지. 기존 Service 완료·Waiting 포기·재경로 시간은 진행 | 켜짐 |
| Closed | 거부 | 없음 | 꺼짐 |

`SetBlocked(true)`는 이전 Open, Full 또는 Draining 상태를 보존한다. Blocked를 해제하면 Drain 의도가
있으면 Draining/Closed로, 없으면 Open/Full로 돌아간다.

### 12.3 Agent 흐름

~~~mermaid
stateDiagram-v2
    [*] --> Waiting: Join
    Waiting --> Waiting: Queue Slot 이동·도착
    Waiting --> ServiceAssigned: FIFO 선두 + 빈 Service
    ServiceAssigned --> InService: Service Anchor 도착 보고
    InService --> Exiting: Service 시간 완료
    Waiting --> Exiting: 포기·이탈·취소
    ServiceAssigned --> Exiting: 이탈·취소
    Exiting --> Released: Exit Anchor 도착
    Released --> [*]
~~~

세부 규칙:

1. Join 시 새 TicketId와 단조 증가 `JoinSequence`를 발급한다.
2. Waiting 배열은 JoinSequence 순서의 절대 FIFO다.
3. 각 Agent는 현재 QueueIndex Slot에 도착했다고 보고해야 한다.
4. 뒤 Agent가 먼저 도착해도 앞 Agent를 추월할 수 없다.
5. FIFO 선두가 도착했고 Service Slot이 비면 가장 낮은 ServiceIndex를 배정한다.
6. Service 배정만으로 타이머는 흐르지 않는다.
7. Agent가 실제 Service Anchor에 도착했다고 보고한 뒤부터 서비스 시간이 흐른다.
8. 서비스 완료·포기·취소 후 Exit 이동을 요청한다.
9. 실제 Agent Actor가 있으면 Exit 도착 보고 전까지 전역 Queue 소유권을 유지한다.
10. Actor가 없는 서버/테스트 논리 Agent는 Exit 단계를 즉시 최종 해제한다.

도착은 Z가 아니라 XY 거리로 판정한다. 지형 높이 보정과 실제 이동은 프로젝트 AI 계층 책임이다.

### 12.4 전역 Agent 소유권

`UFFZSWorldSubsystem`은 `AgentId -> FacilityInstanceId`를 관리해 한 Agent가 동시에 여러 시설 Queue에
들어가는 것을 막는다. Join Callback의 동기 재진입에 대비해 Component에 Join을 요청하기 전에 소유권을
먼저 예약하고, Join 실패 시 되돌린다.

Join 결과:

| 결과 | 의미 |
|---|---|
| Joined | 성공, 유효 Ticket 발급 |
| InvalidAgent | ID가 없거나 Actor Interface ID가 다름 |
| DuplicateAgent | 이미 어떤 시설 Queue를 소유 |
| Full | Waiting 정원 초과 |
| NotOpen | Facility 또는 Queue가 참가 상태가 아님 |
| FacilityNotFound | Facility ID 없음 |

### 12.5 QueueAgent Interface

실제 NPC Actor는 `IFFZSQueueAgent`를 구현한다.

| Callback | 프로젝트가 할 일 |
|---|---|
| `GetFFZoneSystemAgentId` | 저장·재바인딩 가능한 Stable ID 반환 |
| `OnFFZoneSystemQueueJoined` | 첫 Queue Slot으로 이동 |
| `OnFFZoneSystemQueueIndexChanged` | 앞당겨진 새 Slot으로 이동 |
| `OnFFZoneSystemServiceAssigned` | Service Anchor로 이동 |
| `OnFFZoneSystemServiceStarted` | 서비스 Animation/상태 시작 |
| `OnFFZoneSystemExitRequested` | Exit Anchor로 이동 |
| `OnFFZoneSystemQueueReleased` | Queue 상태 종료 |

실제 목표에 도착하면 다음 WorldSubsystem API로 되돌려 보고한다.

- `ReportQueueSlotArrival`
- `ReportServicePointArrival`
- `ReportExitPointArrival`
- 이동 실패 시 `ReportQueueMovementFailed`

### 12.6 Drain, Emergency, 이동 실패

- 일반 Close: 새 Join을 막고 모든 기존 Agent가 나갈 때까지 Draining
- Emergency Close: 기존 항목을 Cancelled로 전환, 물리 Agent에는 Exit 요청
- Agent 이동 실패: Agent별 재경로 횟수 증가, 한도 안이면 같은 목표 재통지
- 한도 초과: Waiting/Service는 `RemovedBySystem`으로 Exit, Exit 단계는 최종 해제
- Queue 전체 Blocked Timeout: 설정 간격마다 `OnRepathRequested` 방송

Agent별 `MovementRepathCount`와 Queue 전체 `RepathAttemptCount`는 서로 다른 예산이다.

### 12.7 결정론적 시간 진행

Queue Tick은 단순히 매 Frame 전체 Delta를 더하지 않는다. 다음 서비스 완료, Waiting 포기,
Blocked 재경로 중 가장 가까운 사건까지 Step한다. 같은 시각에 사건이 겹치면 서비스 완료를 먼저 처리하고
빈 Service Slot에 FIFO 선두를 올린 다음 포기를 처리한다.

4,096 event-step 한도 안에서는 한 번의 큰 Delta와 여러 번의 작은 Delta가 같은 결과를 내도록
설계됐다. 한 step에서 같은 시각의 여러 Agent 사건을 함께 처리할 수 있다. 한 Advance는 최대
4,096 step까지만 처리하며, 한도에 도달하면 미처리 상태는 후속 Tick에서 계속되지만 남은
`DeltaSeconds` 자체는 별도로 누적하지 않는다. Queue가 Closed일 때만 Component Tick이 꺼진다.

### 12.8 Queue 이벤트와 통계

Dynamic Multicast Delegate:

- `OnAgentJoined`
- `OnServiceAssigned`
- `OnServiceStarted`
- `OnAgentReleased`
- `OnQueueStateChanged`
- `OnRepathRequested`

읽기 통계:

- Waiting 수
- Active Service 수
- Exiting 수
- 완료 수
- 포기 수
- 완료 Agent 평균 대기 시간
- 불변식 검증 결과

---

## 13. Snapshot 저장과 복원

### 13.1 현재 Schema

현재 `FFFZSWorldSnapshot::CurrentSchemaVersion`은 8이다. Schema 3~7의 기존 저장은 지원되는 필드 변환을 거쳐 복원하며, 시설 설치를 허용하던 이전 Road는 Tile로 이관한다.

포함:

- WorldRevision
- CurrentPhase
- FacilityInstanceId, 복사된 FacilitySpec
- ZoneId, GridPlacement, FacilityState, WorldTransform
- NeedsRelocation
- Queue 상태·항목·통계·Drain/Blocked 의도
- 완료된 Command Receipt

포함하지 않음:

- Zone Actor와 BoundarySpline
- Protected Area
- External Policy Provider와 외부 시스템 상태
- Zone Population·Density 누적 상태
- QueueAgent Actor 포인터
- 실제 Save Slot·파일·클라우드 데이터

Host Save 시스템은 Snapshot을 SaveGame 객체에 넣어 직렬화하고, Economy·Time·NPC·UI 등 외부 상태를
같은 상위 저장 Transaction에서 별도로 보존해야 한다.

### 13.2 Capture

`CaptureWorldSnapshot`은 Facility와 Receipt를 ID 순으로 정렬한다. 진행 중 Receipt는 저장하지 않는다.
UObject Agent 포인터는 저장하지 않고 Stable AgentId만 보존한다.

### 13.3 Restore 검증

복원 전에 다음을 전체 검사한다.

- 지원 Schema와 유효 Phase·Revision
- Facility ID, Agent ID, Receipt CommandId의 월드 범위 고유성
- TicketId와 JoinSequence의 개별 Queue 범위 고유성
- FacilitySpec
- 대상 Zone 존재와 AllowedZone
- Transform, 1 cm Rect, 정확한 Zone 소유
- 시설 간 점유 중복
- Protected Area 교차
- Queue 상태·용량·FIFO·Service Index·Drain/Blocked 일관성
- 완료 Receipt의 구조와 참조

복원은 현재 지면 Line Trace, World Collision, Navigation 경로를 다시 검사하지 않는다.

### 13.4 논리적 원자 복원

복원은 기존 Actor를 바로 지우지 않는다.

1. 숨김·Collision Off 상태의 Staging Facility Actor를 만든다.
2. Staging Queue와 별도 Registry·점유·Agent 소유권·Receipt Map을 완성한다.
3. 모든 실패 가능 검증과 생성 단계를 통과한다.
4. 새 Map들을 한 번에 권위 상태로 게시한다.
5. 새 Actor를 보이게 하고 기존 Actor를 파괴한다.

중간에 실패하면 Staging Actor만 제거하고 기존 월드는 유지한다. 이는 Subsystem 상태가 “이전 세트 또는
새 세트”로만 보인다는 논리적 원자성이다. UObject 생성과 파괴가 하나의 CPU 명령이라는 뜻은 아니다.

Restore는 `OnWorldChanged`를 방송하지 않고 Snapshot의 WorldRevision과 Phase를 그대로 복원한다.
외부 시스템은 Restore 성공을 별도 상위 이벤트로 처리해야 한다.

### 13.5 Queue Agent 재바인딩

Snapshot에는 Actor 포인터가 없으므로 NPC를 다시 Spawn한 뒤 Stable AgentId로
`RebindQueueAgentActor`를 호출한다. Actor는 `IFFZSQueueAgent`를 구현하고 같은 ID를 반환해야 한다.
재바인딩되면 현재 Queue·Service·Exit 이동 목표가 다시 통지된다.

### 13.6 Schema 3 마이그레이션

제한된 Schema 3(1 Cell = 100 cm) Snapshot은 메모리에서 Schema 4로 변환할 수 있다.

- Footprint, Clearance, Origin Cell을 100배
- 구 개별 Queue Cell을 100 x 100 cm 압축 Rect로 병합
- Queue Cell이 10,000개를 넘으면 오프라인 마이그레이션 요구
- Queue Spline·Anchor·시간처럼 이미 cm/초인 값은 유지
- QuarterTurns가 정규화되어 있어야 함
- 회전된 구 Cell 경계를 보존하기 위해 방향별 99 cm Origin 보정
- 오버플로·모호한 Spec·부분 변환은 fail-closed

Schema 2는 저장된 CurrentPhase가 없어 직접 복원하지 않는다.

---

## 14. Read Model과 조회 API

UI와 분석 계층은 가능하면 가변 Actor 전체 대신 `BuildReadModel`의 값 복사본을 사용한다.

`FFFZSReadModel`:

- 현재 WorldRevision
- ZoneId 순으로 정렬된 Zone Read Model
- FacilityInstanceId 순으로 정렬된 Facility Read Model

Zone Read Model:

- ZoneId
- NeighborZoneIds
- EntryExitAnchors
- AreaSquareMeters
- SoftCapacity
- ReportedPopulation
- DensityRatio
- DensityState

`NeighborZoneIds`와 `EntryExitAnchors`는 Actor 배열의 값 복사본이며 원소 순서를 바꾸지 않는다.
앵커 Transform은 Zone 로컬 cm 좌표계다. `AreaSquareMeters`는 현재 BoundarySpline의 로컬 XY(cm)
점에 신발끈 공식을 적용한 `GetBoundaryAreaSquareMeters()` 결과다. 즉,
`abs(sum(x_i * y_(i+1) - x_(i+1) * y_i)) * 0.5 / 10000`으로 계산한 실제 경계 면적(m²)이며,
SoftCapacity 산정에만 쓰이는 `UsablePublicAreaSquareMeters` Override는 반영하지 않는다.

Facility Read Model:

- FacilityInstanceId, DefinitionId, ZoneId, Category
- FacilityState, NeedsRelocation
- QueueState
- WaitingCount, ActiveServiceCount
- AverageCompletedWaitSeconds
- WorldLocation: Facility Actor의 대표 월드 지점(cm)
- bHasApproachWorldTransform: 아래 접근점이 유효한지 나타내는 값
- ApproachWorldTransform: FacilityDefinition의 Entry[0] 로컬 앵커를 설치 Actor 기준 월드 좌표로 변환한 고정 접근점

`ApproachWorldTransform`은 NPC가 시설 또는 Queue 가입 지점으로 접근할 때 사용한다. 실제 Queue 가입 뒤에는
Queue가 배정한 동적 Slot Transform이 권위이며, 접근점이나 Facility Actor 중심을 대기 Slot으로 재사용하지 않는다.
Entry[0]은 가입 전 admission/staging 지점이며 Queue tail 자체일 필요는 없다. 대신 Entry에서 모든 동적 Slot까지
이동 가능한 예약 동선이 있어야 하고, Join 성공 직후에는 Approach 목표를 폐기해야 한다.
유효한 프로덕션 `FFFZSFacilitySpec`은 Entry[0]을 필수로 검증하지만, 수동 생성한 Read Model의 기본 Identity
Transform을 월드 원점의 실제 접근점으로 오인하지 않도록 소비자는 `bHasApproachWorldTransform`도 확인한다.
플러그인 검증은 도착 허용 반경끼리 겹치지 않는 일반 계약을 강제한다. 실제 프로젝트 콘텐츠는 여기에 더해
사용 중인 NPC 몸체 지름도 만족해야 하며, FestaFiesta 프로덕션 Definition은 기본 Crowd 반경 34cm 기준으로
모든 Anchor와 Queue Slot의 XY 중심을 최소 68cm 이상 분리한다. Entry와 Queue Slot의 NPC 몸체 원 전체가
`QueueReservedRects` 안에 포함되는지도 자동화 테스트한다.

2026-08-14 이동 목표 분리 규칙 도입 전 생성된 개발용 World Snapshot은 겹친 frozen FacilitySpec 때문에
복원이 거부될 수 있다. 아직 출시 세이브 호환을 보장하는 단계가 아니므로 해당 개발 Snapshot은 재생성한다.
출시 세이브 호환이 필요해지면 FacilitySpec/WorldSnapshot 버전 상승과 명시적 좌표 마이그레이션을 먼저 추가한다.

Read Model 수정은 권위 상태에 영향을 주지 않는다. Queue 참가·Population 갱신처럼 Revision을 올리지 않는
변화도 Read Model 값에는 나타날 수 있으므로, UI 갱신을 Revision 변화에만 의존해서는 안 된다.

---

## 15. Placement Preview

### 15.1 Placement Preview

`AFFZSPlacementPreviewActor`는 UI와 분리된 비권위 Ghost Actor다.

보유 상태:

- `PreviewCommand`
- 마지막 `FFFZSPlacementValidation`
- 충돌 없는 Preview Mesh
- 자동 갱신 여부와 간격
- 압축 Rect Debug 표시 옵션

`RefreshPreview`는 다음만 수행한다.

1. 현재 Command를 `ValidatePlacement`로 검사
2. LastValidation 갱신
3. 계산된 Placement Transform으로 Ghost 이동
4. Soft Visual Mesh 비동기 요청
5. Custom Depth와 Debug Box 갱신

현재 표현 규칙은 유효 Preview Custom Depth 1, 실패 Preview 2이며,
Footprint는 초록, Clearance는 청록, Queue는 노랑, 실패 영역은 빨강으로 그린다.
개별 1 cm Cell이 아니라 Canonical Rect당 Box 하나를 그린다.

자동 갱신은 기본 false이고 간격 기본값은 0.1초다. 짧은 간격으로 켜면 Ground, Collision,
Navigation, Policy PreValidate를 반복하므로 제작 도구에서만 제한적으로 사용한다.
`CommitPreview`는 `ExecuteCommand`를 호출해 Definition 동일성, 최신 WorldRevision, 배치 정책,
지면·충돌·Navigation, 비용을 모두 다시 검증한 뒤 실제 설치를 커밋한다. 성공하면 Facility Actor,
예약 영역, 비용, Command Receipt와 WorldRevision이 원자적으로 갱신되고
`bWorldStateChanged=true`가 된다. 실패하면 부분 설치 없이 원래 월드 상태를 보존한다.

Preview는 비권위 표시지만 Commit은 권위 변경이라는 경계를 혼동하면 안 된다. 외부 Blueprint는
deprecated raw Spec 노드를 새로 사용하지 말고, 카탈로그에서 선택한 Definition으로
`MakeInstallCommandFromDefinition`을 호출하는 경로만 사용한다.

---

## 16. Project Settings

Editor의 `Edit -> Project Settings -> Game -> FF Zone System`에서 설정한다.

| 영역 | 설정 | 기본값 | 적용 의미 |
|---|---|---:|---|
| Units | Placement Cell Size | 1 cm 고정 | 변경 불가 좌표 계약 |
| Performance | Max Enumerated Cells Per Facility | 20,000,000 | 손상 입력 조기 거부 예산 |
| Performance | Ground Sample Spacing | 100 cm | 지면 Trace 간격 |
| Performance | Queue Simulation Tick Interval | 0.1초 | 0이면 매 Frame |
| Selection | Facility Selection Padding | 75 cm/변 | Footprint 기반 선택 Query Box의 XY 사방 여백, 전체 치수는 +150 cm |
| Placement | Max Ground Slope | 5도 | 표본 법선 최대 경사 |
| Placement | Max Ground Height Variance | 20 cm | 표본 최고·최저 Z 차 |
| Placement | Ground Trace Above / Below | 1000 / 1000 cm | Line Trace 범위 |
| Placement | Enable Ground Checks | true | 지면 검증 사용 |
| Placement | Require Every Ground Sample | true | 모든 표본 Hit 요구 |
| Placement | Enable World Collision Checks | true | Visibility Overlap 사용 |
| Navigation | Enable Projection Checks | true | Anchor·Slot Nav 검사 |
| Navigation | Fail When Nav Unavailable | false | 운영에서는 true 검토 |
| Navigation | Projection Extent | 30 cm | Facility별 값과 비교하는 전역 상한, 1 cm 하한 |
| Zone Authoring | Restrict Zone IDs | true | ProjectZoneIds만 허용 |
| Zone Authoring | Project Zone IDs | 5.3절의 기본 12개 ID | 제한 모드에서 허용·필수인 ID 집합 |
| Zone Membership | Hysteresis | 50 cm | 이전 Zone 유지 완충 |
| Integration | Require Production Policy Domains | false | 6개 Domain Gate |
| Protected Routes | General Width | 300 cm | 전역 기본 폭 |
| Protected Routes | Emergency Width | 400 cm | 전역 기본 폭 |
| Protected Routes | Accessibility Width | 150 cm | 접근성·관람 기본 폭 |
| Density | Square Meters Per Person | 4 m² | 자동 SoftCapacity |
| Density | Caution / Warning / Critical | 0.8 / 1.0 / 1.25 | 밀도 비율 |
| Density | Warning / Critical Hold | 5 / 3초 | 상승 유지 시간 |
| Workflow | Initial Phase | InitialSetup | 새 Subsystem의 시작 Phase |

`Config/DefaultFF_ZoneSystem.ini`에는 배포 핵심값과 두 Boolean
(`bRestrictZoneIdsToProjectPresets`, `bRequireProductionPolicyDomains`)을 명시한다. Ground/Nav
검사 Boolean, Trace 범위, ProjectZoneIds, InitialPhase 등은 CDO 또는 생성자 기본값을 사용한다.

설정이 적용되는 시점을 구분해야 한다.

- InitialPhase: 새 WorldSubsystem 초기화 때 읽음
- Queue Tick Interval: QueueComponent 생성 때 읽음
- Facility Selection Padding: Facility 초기화·Snapshot 복원 때 읽음. 기존 Actor에는 즉시 재적용되지 않으므로 설정 변경 후 PIE/실행 재시작
- 배치 검사 수치: 다음 Validate 호출 때 읽음
- Protected Area 폭 0: 다음 교차 검사 때 현재 설정을 동적 상속
- Actor에 `ApplyDefaultWidthForType`으로 복사한 폭: 이후 전역 변경을 상속하지 않음

이 기본값들은 Prototype 기준이며 행사 안전·법률 인증값이 아니다.

---

## 17. Blueprint와 C++ 공개 API

### 17.1 WorldSubsystem

Blueprint에서는 `Get World Subsystem -> FFZSWorldSubsystem`으로 접근한다.

| 영역 | 주요 API |
|---|---|
| Event | `OnWorldChanged`, `OnPlacementRejected` |
| Registry | `RegisterZone`, `UnregisterZone`, `RegisterProtectedArea`, `UnregisterProtectedArea` |
| Lookup | `FindZone`, `FindFacility` |
| Workflow | `SetChangePhase`, `GetChangePhase`, `GetWorldRevision` |
| Placement | `ValidatePlacement`, `ExecuteCommand` |
| Policy | `RegisterExternalPolicy`, `UnregisterExternalPolicy`, `ValidateIntegrationReadiness` |
| Queue | `JoinFacilityQueue`, `JoinFacilityQueueByActor`, `LeaveCurrentFacilityQueue` |
| Queue Report | Slot·Service·Exit Arrival, Movement Failed, Agent Rebind |
| Queue Control | `SetFacilityQueueBlocked`, `FindAgentQueueFacility` |
| Zone | `ResolveZoneAtWorldLocation`, `ReportZonePopulation` |
| Read | `BuildReadModel` |
| Save | `CaptureWorldSnapshot`, `RestoreWorldSnapshot` |
| Maintenance | `ValidateZoneGraph`, `RevalidateFacilitiesInZone` |

`RegisterFacility`와 `UnregisterFacility`는 C++ 내부 수명주기용이며 일반 Gameplay 변경 API가 아니다.

### 17.2 GridLibrary

Blueprint 노드:

- 최대 열거 예산 조회
- QuarterTurns 정규화
- Cell·Rect 회전
- Debug용 Footprint·Clearance·Queue·Reserved Cell 조회
- 기본 12개 Zone ID 조회
- Install Command 생성
- Placement Failure 기본 문구 변환

C++ 전용 고성능 API:

- Rect Canonicalization
- Rect Set Overlap
- 압축 영역 면적
- Footprint·Clearance·Queue·Reserved Rect 생성
- Command Fingerprint

### 17.3 Actor와 Component API

- GameplayZone: 경계, 포함, Hysteresis, 월드/Cell 변환, 카테고리, 면적, 수용량, Population, Read Model
- ProtectedArea: 유효 폭, World Box 교차, 데이터 검증, Editor Preset
- FacilityDefinition: Spec 검증과 Primary Asset ID
- FacilityActor: ID·Zone·Placement·Spec·State·Queue·Relocation·Anchor·Snapshot·Read Model 조회와 실제 SelectionProxy 기반 `SetSelectionIndicatorState`. `Hovered`는 현재 클릭 대상, `Selected`는 열린 정보 패널 대상을 뜻하며 동시에 서로 다른 시설에 표시될 수 있음
- QueueComponent: 상태·수·목표 위치·Snapshot·불변식·규칙·통계 조회와 Delegate
- PlacementPreviewActor: Data Asset Spec 기반 Refresh, Auto Refresh, Commit, Last Validation

FacilityActor와 QueueComponent의 공개 Blueprint API는 대부분 관찰용이다. 변경은 WorldSubsystem을 사용한다.

---

## 18. 기본 사용 절차

### 18.1 플러그인 연결

1. 프로젝트의 `Plugins/FF_ZoneSystem` 아래에 플러그인을 둔다.
2. `.uproject`에서 `FF_ZoneSystem`을 활성화한다.
3. 플러그인 API를 쓰는 게임 모듈의 `Build.cs`에 `FF_ZoneSystem` 의존성을 추가한다.
4. Public 프로젝트 Header가 플러그인 타입을 노출하면 Public Dependency, cpp 내부에서만 쓰면
   Private Dependency로 둔다.
5. Editor Target을 빌드하고 Project Settings를 확인한다.

현재 FestaFiesta 프로젝트는 `FestaFiesta.uproject`에서 플러그인을 활성화하고,
게임 모듈의 Private Dependency로 연결되어 있다.

### 18.2 Zone 저작

1. 레벨에 `AFFZSGameplayZone`을 배치한다.
2. Stable ZoneId를 입력하거나 `ApplyAcceptedZonePreset`을 사용한다.
3. BoundarySpline을 닫힌 Linear 단순 폴리곤으로 편집한다.
4. Scale 1, Pitch/Roll 0, Yaw 90도 단위, XY 원점 정수 cm를 유지한다.
5. Allowed Category, Neighbor, 수용량 데이터를 입력한다.
6. `ValidateZoneData`와 `ValidateZoneGraph`의 오류를 모두 해결한다.

Zone Actor는 기본 생성 시 100 m x 100 m 사각형 Spline을 제공하지만, 실제 맵 경계에 맞게 수정해야 한다.

### 18.3 Protected Area 저작

1. 레벨에 `AFFZSProtectedArea`를 배치한다.
2. 고유 ProtectedAreaId를 지정한다.
3. 모든 Zone 적용이면 LinkedZoneId를 비우고, Zone 전용이면 유효한 ID를 지정한다.
4. AreaType과 Box Extent를 설정한다.
5. 기본 폭 상속은 RequiredClearWidthCm=0으로 유지한다.
6. Scale 1과 Yaw-only Transform을 유지한다.
7. `ValidateProtectedAreaData`로 확인한다.

존재하지 않는 LinkedZoneId를 가진 영역 하나가 있으면 배치 검증이 DataInvalid로 실패할 수 있으므로,
월드 전체 Graph 검증을 선행한다.

### 18.4 Facility Definition 저작

1. Content Browser에서 `UFFZSFacilityDefinition` Data Asset을 만든다.
2. SchemaVersion 2와 Stable FacilityDefinitionId를 입력한다.
3. 실제 cm 크기를 그대로 Footprint Cell 수에 적는다.
4. Clearance와 반열림 QueueReservedRects를 작성한다.
5. Queue Spline을 Service에서 Tail 방향으로 작성한다.
6. Queue 가입 전 접근점 Entry[0], 퇴장점 Exit[0], Service[0..Capacity-1] Anchor를 만든다. Entry를 Slot 0과 같은 위치에 두지 않고, 모든 Anchor와 Queue Slot의 XY 간격이 도착 허용 지름과 프로젝트 NPC 몸체 지름을 모두 만족하게 한다.
7. 외부 Cost·Revenue·Experience Key와 Soft Mesh를 설정한다.
8. VisualSizingMode는 `MatchPlacementBounds`로 유지한다. 그러면 Footprint와 PlacementObstacleHeight를 바꿀 때 메시 표시 크기도 함께 바뀐다.
9. 이미 작성한 시설의 크기를 바꿀 때는 Footprint 필드를 직접 고치지 말고 `Facility Authoring Tools`의 `New Width (X)`·`New Depth (Y)`·`New Height (Z)`에 목표 값을 넣고 `Resize Facility (Keep Queue & Anchors)`를 누른다. 높이는 XY 배치에 영향을 주지 않으므로 보정 없이 값만 교체되며 최소 10cm다. QueueReservedRects는 건물 좌하단 기준이고 Queue Spline Point와 Anchor는 건물 중심 기준이라, 같은 크기 변경에도 필요한 이동량이 서로 다르다. 버튼은 중심보다 바깥쪽 예약 경계를 크기 변화량만큼, 경로 점과 Anchor를 변화량의 절반만큼 옮겨 각자 붙어 있던 건물 변까지의 거리를 유지하며, 결과 Spec이 전체 검증을 통과할 때만 적용한다. 구버전 `QueueReservedCells`가 남아 있으면 자동 보정을 제공하지 않으므로 먼저 QueueReservedRects로 변환한다.
10. 대기열이 차지하는 공간을 줄일 때는 세 값이 검증으로 묶여 있으므로 `Queue Rules` → `Queue Spline Points` → `Queue Reserved Rects` 순서로 손댄다. 필요 경로 길이는 `(Max Waiting Agents - 1) × Slot Spacing`이므로 인원이나 간격을 줄이면 길이가 줄고, 정원을 유지한 채 면적만 줄이려면 Spline에 굴절점을 넣어 L자·ㄷ자로 접는다. 길이를 바꾼 뒤에는 `Queue Tail <- Max Capacity`로 경로를 맞추고, `Queue Margin (cm)`에 통행 여백을 넣고 `Queue Rects <- Queue Path`를 누르면 예약 영역이 경로를 따라가는 회랑으로 다시 만들어진다. 접힌 경로는 구간마다 사각형이 생겨 외접 박스보다 훨씬 작은 면적만 확보하고, 최대 정원이 쓰지 않는 꼬리 구간과 건물 본체를 침범하는 부분은 자동으로 제외된다.
11. Details의 `Facility Authoring Tools`에서 `Show Korean Help`, `Validate Facility Definition`, `Footprint <- Loaded Mesh`, `Resize Facility (Keep Queue & Anchors)`, `Queue Rects <- Queue Path`, `Queue Tail <- Max Capacity`, `Max Capacity <- Queue Length`, `Fit Mesh to Placement Bounds` 버튼을 사용할 수 있다. 변경 버튼은 Undo/Redo transaction을 지원한다.
12. 카테고리·필드·열거형·배열 요소·버튼처럼 항상 보이는 UI 이름은 영어를 사용한다. `Show Korean Help`를 누르면 핵심 사용법이 한국어 대화상자로 열리고, 개별 항목에 마우스를 올리면 단위, 좌표 원점, 포함/미포함 경계, 설정된 시설 면적 안전 예산, 활성 조건을 한국어 툴팁으로 확인할 수 있다. 버튼 실행 뒤 결과 알림과 검증 대화상자도 한국어로 표시한다.
13. Content Browser의 `Validate Assets`로 Spec 내부·중복 ID·경로·메시 bounds까지 검증한다.
14. Asset Manager Scan과 Shipping Cook 포함 여부를 Host 프로젝트에서 검증한다.

### 18.5 C++ Install 예시

~~~cpp
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"

bool TryInstallSelectedDefinition(
    UWorld* World,
    const UFFZSFacilityDefinition* SelectedDefinition)
{
    if (!IsValid(World) || !IsValid(SelectedDefinition))
    {
        return false;
    }

    UFFZSWorldSubsystem* ZoneSystem =
        World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!IsValid(ZoneSystem))
    {
        return false;
    }

    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(1200, 800);
    Placement.QuarterTurns = 1;

    FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommandFromDefinition(
        SelectedDefinition,
        TEXT("GZ_DadaPocha"),
        Placement,
        ZoneSystem->GetWorldRevision());

    const FFFZSPlacementValidation Preview =
        ZoneSystem->ValidatePlacement(Command);
    if (!Preview.bIsValid)
    {
        return false;
    }

    const FFFZSCommandResult Result =
        ZoneSystem->ExecuteCommand(Command);
    return Result.bSucceeded;
}
~~~

실제 코드는 Null World, Null Subsystem, Result 실패를 처리하고 `FailureReason`으로 UI를 분기해야 한다.

### 18.6 Lifecycle Command 예시

~~~cpp
FFFZSCommand OpenCommand;
OpenCommand.CommandId = FGuid::NewGuid();
OpenCommand.TransactionId = FGuid::NewGuid();
OpenCommand.ExpectedWorldRevision = ZoneSystem->GetWorldRevision();
OpenCommand.Type = EFFZSCommandType::Open;
OpenCommand.FacilityInstanceId = FacilityId;

const FFFZSCommandResult OpenResult =
    ZoneSystem->ExecuteCommand(OpenCommand);
~~~

Close, EmergencyClose, Remove도 매 요청마다 새 CommandId를 사용한다.

### 18.7 Blueprint 권장 흐름

~~~text
UI 입력
→ Make Install Command From Definition
→ PlacementPreviewActor.PreviewCommand 설정
→ Refresh Preview
→ Validation.Reasons와 Summary 표시
→ 사용자 확인
→ Commit Preview
→ CommandResult 확인
→ 성공: OnWorldChanged 기반 UI 갱신
→ 실패: FailureReason 기반 해결 행동 표시
~~~

실패 처리는 Summary 문자열 비교가 아니라 `EFFZSPlacementFailure` Enum을 사용한다.

### 18.8 Queue 연결

1. NPC Actor에 `IFFZSQueueAgent`를 구현한다.
2. 저장 후에도 같은 Stable AgentId를 반환한다.
3. `JoinFacilityQueueByActor`로 참가한다.
4. Callback에서 실제 이동을 시작한다.
5. 도착 시 WorldSubsystem Report API를 호출한다.
6. Save 복원 후 NPC Spawn이 끝나면 `RebindQueueAgentActor`를 호출한다.

### 18.9 프로젝트 플레이테스트

현재 프로젝트에는 다음 QA Slice가 준비되어 있다.

- 통합 수동 QA Map: `/Game/FestaFiesta/Maps/Test_Main`
- Lifecycle Panel: `/Game/FestaFiesta/Building/FF_WBP_Building/Debug/WBP_FF_ZoneLifecycleTestPanel`
- Definition·배치 에디터 픽스처: `/Game/FestaFiesta/Zone/Maps/FacilityDefinitionTestMap`
- Lifecycle QA 코드: `Source/FestaFiesta/Private/Building/Testing/FF_ZoneLifecycleTestPanel*`

Lifecycle 패널은 Editor와 Shipping 패키지의 `Test_Main` Game·PIE 월드에서만 숨김 상태로 생성되며, 게임
뷰포트에 포커스를 둔 뒤 `Ctrl+G`로 표시·숨김을 전환한다. Festival 단계 변경과
Open·Close·Emergency 권위 API를 관찰·수동 실행한다. Install·Move·Remove는 같은 맵의 프로덕션
건축 모드 경로로 확인하고, Definition과 배치 픽스처는 Editor 검증에 사용한다. 이 QA Slice는
Queue 또는 실제 NPC 이동 하네스가 아니므로 해당 통합 검증은 별도로 수행한다.

---

## 19. 배치·명령 실패 코드

배치 검증과 Facility Command의 안정 실패 코드는 `EFFZSPlacementFailure`를 사용한다. 성공이면
`None`이고, 실패이면 `ValidatePlacement`의 `FFFZSPlacementValidation::Reasons`에 하나 이상의
코드와 `Summary`가
누적된다. `ExecuteCommand`는 `FFFZSCommandResult::FailureReason`과 `Message`로 대표 실패를
반환한다. UI는 `Summary`나 `Message`를 로직 분기 값으로 고정하기보다 enum을 사용자용 번역
키로 매핑하고, 텍스트는 표시·개발 진단 정보로 사용하는 편이 안전하다.

| 분류 | 코드 | 의미와 일반적인 대응 |
|---|---|---|
| 성공 | `None` | 검증 또는 명령이 성공했다. |
| 식별자 | `ZoneNotFound` | 지정한 Zone이 Registry에 없다. Zone 등록 시점과 ID 오타를 확인한다. |
| 식별자 | `FacilityNotFound` | 대상 Facility가 없거나 이미 철거되었다. |
| 식별자 | `DuplicateZoneId` | Zone ID 중복을 나타내기 위한 코드다. 현재 Zone 등록 경로는 `false`와 로그로 중복을 알리며, 일반 Command 실행에서 이 코드를 직접 반환하지는 않는다. |
| 식별자 | `DuplicateFacilityId` | 새 시설 ID가 기존 시설과 겹친다. 자동 ID를 쓰거나 호출자가 유일한 ID를 보장해야 한다. |
| Zone 규칙 | `CategoryNotAllowed` | Zone의 허용 Category에 시설 Category가 포함되지 않는다. |
| Zone 규칙 | `OutsideZone` | Footprint·Clearance·Queue 전체 Reserved 영역이 한 Zone에 완전 포함되지 않거나 여러 Zone과 면적으로 겹친다. |
| 지면 | `InvalidGround` | 지면 Trace가 유효한 지면을 찾지 못했다. Trace Channel과 지형 Collision을 확인한다. |
| 지면 | `ExcessSlope` | 지면 경사가 허용 최대값보다 크다. |
| 지면 | `HeightVarianceExceeded` | 여러 지면 표본의 높이 편차가 허용값을 넘었다. |
| 점유 | `PlacementCollision` | 시설 배치 Footprint가 기존 시설 또는 차단 월드 Geometry와 충돌한다. |
| 점유 | `InsufficientClearance` | Clearance 영역이 다른 점유 또는 장애물과 겹친다. |
| 점유 | `QueueFootprintBlocked` | 대기열 Footprint를 확보할 수 없다. |
| 보호 구역 | `ProtectedRouteBlocked` | 보호된 도로·출입구·비상 동선 등과 배치 영역이 충돌한다. |
| Navigation | `RequiredRouteDisconnected` | 필수 Entry/Service/Exit 또는 Queue 목적지 사이의 경로가 연결되지 않는다. |
| 보호 구역 | `AccessibilityRouteBlocked` | Reserved 영역이 `AccessibilityRoute` 또는 `ViewingArea` Protected Area와 교차한다. |
| 단계 | `WrongStage` | 현재 Phase에서 허용하지 않는 명령이다. 배치는 `InitialSetup`·`MorningCheck`에서만 허용된다. |
| 상태 | `FacilityOccupied` | 사용 중인 시설을 비활성화·철거하려 했다. Queue와 이용자를 먼저 비워야 한다. |
| 상태 | `InvalidFacilityState` | 요청한 수명주기 전이가 현재 상태에서 유효하지 않다. |
| 동시성 | `StaleWorldRevision` | `ExpectedWorldRevision`이 현재 Revision과 다르다. 최신 Read Model을 다시 읽고 명령을 재구성한다. |
| 멱등성 | `DuplicatePayloadMismatch` | 같은 `CommandId`를 다른 Payload에 재사용했다. 새 CommandId를 발급해야 한다. |
| Navigation | `NavigationUnavailable` | NavSystem이 없거나 투영·경로 계산을 수행할 수 없고, 설정상 이를 실패로 처리한다. |
| Navigation | `NavRebuildTimeout` | Nav 재빌드 시간 초과를 표현하기 위해 예약된 코드다. 현재 동기 검증 경로에서는 직접 발생시키지 않는다. |
| 데이터 | `DataInvalid` | Facility Definition, Spec, Rect, Anchor 등 입력 데이터의 불변식이 깨졌다. |
| 외부 정책 | `ExternalPolicyRejected` | 필수 Production Domain 준비가 안 됐거나 `PreValidate`·`ApplyTentative`가 거부했다. `FFFZSPolicyDecision`의 코드·Message를 확인한다. |
| 내부 | `InternalError` | Game Thread·재진입 Guard, Actor Spawn·등록, 로컬 적용 또는 Preview Subsystem 등 내부 실행 조건이 실패했다. Policy Commit은 no-fail 계약이다. |

`ExecuteCommand`가 실행 Guard를 세우기 전에 끝나는 Game Thread 위반, Restore·Policy Callback 중
호출, 무효 CommandId/TransactionId, Receipt Replay·Payload 불일치, In-flight 재진입은
`OnPlacementRejected`를 방송하지 않으므로 반환값을 직접 확인해야 한다. Guard 이후의 정상
Validation에서 발생하는 Zone·Facility ID 오류 등은 거부 이벤트가 방송될 수 있다.

---

## 20. 자동화 테스트와 QA Gate

2026-08-19 검증 실행에서 `FF_ZoneSystem.*` Unreal Automation Test는 **37개**가 발견되었다.
개수는 테스트가 추가·제거되면 달라질 수 있으므로, 출하 Gate의 권위 기준은
`Automation RunTests FF_ZoneSystem`이 발견한 전체 목록이 **0 failure, Queue Empty, Exit 0**로 완료되는지이다.

### 20.1 Integration 테스트

별도 `FF_ZoneSystemTests` 모듈이 실제 `UWorld`와 Actor를 구성해 다음 흐름을 검증한다.

- `FF_ZoneSystem.Integration.Policy.AtomicCommitAndReverseRollback`
- `FF_ZoneSystem.Integration.World.CommandLifecycleQueueAndSnapshot`
- `FF_ZoneSystem.Integration.World.ChangePhasePermissions`
- `FF_ZoneSystem.Integration.World.FacilityLookupAtWorldPoint`
- `FF_ZoneSystem.Integration.World.ZoneExclusivityAndProtectedArea`
- `FF_ZoneSystem.Integration.World.InvalidFacilitySpecStopsBeforeNavigation`
- `FF_ZoneSystem.Integration.World.PortableZoneGraphAndPrimaryAssetType`
- `FF_ZoneSystem.Integration.World.SnapshotPhaseReentrancyAndDrainIntent`
- `FF_ZoneSystem.Integration.World.ZoneReadModelSpatialMetadata`
- `FestaFiesta.ZoneIntegration.Queue.FacadeLifecycleE2E`

주요 대상은 외부 정책의 원자성, Command 수명주기, Phase별 월드 변경 권한, Queue와 Snapshot,
Zone 독점성, Protected Area, 잘못된 Definition의 조기 차단, Zone Graph,
Phase·재진입·Drain 복원, Zone Read Model의 공간 메타데이터 값 복사다. 프로젝트 통합 테스트인
`FacadeLifecycleE2E`는 Queue façade 가입, 점유 중 Snapshot 복원 거절, Blocked fail-close,
non-Live Close의 scoped cancellation을 실제 Game test world에서 검증한다.

### 20.2 Unit 테스트와 소속

테스트는 Runtime, Editor 테스트, Editor 저작 모듈로 나뉜다. `Automation RunTests FF_ZoneSystem`은
세 모듈의 아래 Unit·Integration·Editor 테스트를 모두 포함한다.

- `FF_ZoneSystem.Unit.SaveGame.WorldSnapshotByteRoundTrip`
- `FF_ZoneSystem.Unit.Queue.FIFOArrivalAndCapacity`
- `FF_ZoneSystem.Unit.Queue.DrainAndEmergencyCancel`
- `FF_ZoneSystem.Unit.Queue.SaveRestoreMovementState`
- `FF_ZoneSystem.Unit.Queue.SaveRestoreTickAndDrain`
- `FF_ZoneSystem.Unit.Queue.EventStepTieAndDeltaIndependence`
- `FF_ZoneSystem.Unit.Queue.BlockedAndMovementFailure`
- `FF_ZoneSystem.Unit.Queue.NonFiniteArrivalAndRepathBoundary`
- `FF_ZoneSystem.Unit.Queue.SnapshotSafetyLimitsAndSequenceExhaustion`
- `FF_ZoneSystem.Unit.Units.OnePlacementCellEqualsOneCentimeter`
- `FF_ZoneSystem.Unit.Grid.OneCentimeterCellAndRectQuarterTurns`
- `FF_ZoneSystem.Unit.Grid.OneCentimeterCompressedRegions`
- `FF_ZoneSystem.Unit.Commands.OneCentimeterCompressedRegionPayloadIdentity`
- `FF_ZoneSystem.Unit.Definitions.OneCentimeterTestFacilityFixtures`
- `FF_ZoneSystem.Unit.Definitions.OneCentimeterCompressedRegionValidation`
- `FF_ZoneSystem.Unit.Definitions.OneCentimeterEditorDataValidation`
- `FF_ZoneSystem.Unit.Definitions.CardinalitySafetyLimitsFailClosed`
- `FF_ZoneSystem.Unit.Definitions.MovementTargetSeparation`
- `FF_ZoneSystem.Unit.QueueGeometry.ExactTail`
- `FF_ZoneSystem.Unit.QueueGeometry.MultiSegmentSlots`
- `FF_ZoneSystem.Unit.QueueGeometry.NearDegenerateSegmentsAreConsistent`
- `FF_ZoneSystem.Unit.QueueGeometry.ReservationCoordinates`
- `FF_ZoneSystem.Unit.QueueGeometry.SegmentTangentAtBends`
- `FF_ZoneSystem.Unit.QueueGeometry.ZeroLengthSegment`
- `FF_ZoneSystem.Unit.VisualGeometry.AuthoritativeSizingModes`
- `FF_ZoneSystem.Editor.FacilityAuthoringHelpers`
- `FF_ZoneSystem.Editor.FacilityDefinitionCrossAssetValidation`
- `FF_ZoneSystem.Editor.FacilityKoreanTooltips`

### 20.3 명령행 실행 예시

Unreal Engine과 프로젝트 경로를 환경에 맞게 지정한 뒤 다음 형태로 실행할 수 있다.

~~~powershell
& "$UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PROJECT_ROOT\FestaFiesta.uproject" `
  -unattended -nop4 -nullrhi -nosplash -NoSound `
  -ExecCmds='Automation RunTests FF_ZoneSystem; Quit' `
  -TestExit='Automation Test Queue Empty'
~~~

`Test_Main`의 건축·Lifecycle 수동 QA와 `FacilityDefinitionTestMap` 에디터 픽스처는 자동화 테스트를
대체하지 않는다. 특히 실제 NPC가 `IFFZSQueueAgent`를 구현한 상태의 이동, 복제 환경, 대규모
NavMesh 재빌드 비용은 별도 통합 시험이 필요하다.

### 20.4 QA Gate

`Config/DefaultFF_ZoneSystem.ini`가 참조하는 출하 전 승인 체크리스트다. 아래 항목은 현재 통과
상태를 주장하는 목록이 아니라, 대상 빌드마다 로그·리포트·캡처로 증거를 남겨야 하는 Gate다.

#### 설정·정적 데이터

- [ ] Editor와 Cook에서 실제 CDO 설정값, ProjectZoneIds, InitialPhase, Ground/Nav Flag를 확인한다.
- [ ] `ValidateZoneGraph`와 모든 `UFFZSFacilityDefinition` Validate Assets 결과가 오류 0건이다.
- [ ] Production은 `ValidateIntegrationReadiness(Errors, true)`가 오류 0건이고, Custom을 제외한
  6개 Policy Domain에 각각 정확히 한 Provider가 등록된다.
- [ ] 실패 enum과 프로젝트 UI 현지화·해결 행동 매핑이 일치한다.

#### Build·자동화·패키징

- [ ] Editor Development, Game Development, Game Shipping 구성이 모두 빌드된다.
- [ ] `Automation RunTests FF_ZoneSystem`이 발견한 전체 테스트가 0 실패, Queue Empty, 프로세스 Exit 0이다. 2026-08-19 기준 기대치는 37개 통과(0 실패)이다.
- [ ] 독립 Plugin 패키징과 프로젝트 Cook에서 Soft Mesh·Definition·Asset Manager Scan을 검증한다.
- [ ] Clean PC의 Shipping 패키지에서 Plugin Mount, Map Load, Save/Restore Smoke를 통과한다.

#### 실제 Gameplay 통합

- [ ] `Test_Main` PIE·Standalone에서 Install·Move·Remove를 실제 건축 모드로, Open·Close·Emergency를
  Lifecycle 패널로 확인하고 실제 대상 Map에서도 같은 권위 경로를 검증한다.
- [ ] 유효 Command는 Actor·점유·Revision·외부 예약이 한 번만 반영되고, 실패 Command는 전부 원복된다.
- [ ] 실제 NPC `IFFZSQueueAgent`가 Stable ID, 이동 도착·실패 보고, Save 후 Rebind를 검증한다.
  Lifecycle 패널과 Definition·배치 픽스처는 이 항목의 대체 증거가 아니다.
- [ ] 일반 Drain, Emergency Exit, Blocked/Repath, FIFO 비추월을 시각적으로 확인한다.

#### 성능·안전·네트워크

- [ ] 실제 Agent가 연결된 뒤 동일 Map·Seed·빌드에서 20명, 100명, 300명 규모의 FPS, 1% Low,
  Game/Render, AI/Nav, Queue 비용을 기록한다.
- [ ] 경로 폭·밀도·경사 등 Prototype 수치를 Safety Owner가 실제 행사·법률 기준과 대조 승인한다.
- [ ] 네트워크 기능은 미제공으로 명시한다. 멀티플레이를 추가한 프로젝트는 서버 권한, 복제,
  재접속, 중복 Command를 별도 시험한다.

---

## 21. 성능, 스레드, 네트워크 경계

### 21.1 성능 특성

- 권위 점유·저장·압축 영역 교차는 1 cm 셀을 개별 객체로 만들지 않으므로 면적보다
  **Rect 개수**에 더 크게 좌우된다. 전체 배치 검증에는 별도로 Ground 표본 수와
  Navigation Anchor·Queue Slot 수도 영향을 준다.
- 배치 점유 검사는 같은 Zone의 기존 시설과 각 Compressed Region을 비교한다. 시설 수와
  Rect 수가 지나치게 늘어나면 공간 인덱스가 없으므로 비용도 함께 증가한다.
- Protected Area 충돌 검사는 등록된 보호 영역을 순회한다.
- `ResolveZoneAtWorldLocation`은 결정론을 위해 Zone ID를 정렬한 뒤 Zone을 순회한다.
  현재 별도의 Quadtree나 Spatial Hash는 없다.
- Navigation 검증은 첫 Entry Anchor에서 필수 Anchor와 Queue Slot까지 동기식 투영·경로
  검사를 수행한다. 대량 배치는 Game Thread Hitch를 피하도록 작업량을 분산해야 한다.
- Queue 시간 진행은 사건 단위이며 한 Advance에서 최대 4,096 event-step을 처리한다. 같은
  시각의 여러 Agent 사건은 한 step에서 함께 처리될 수 있다. 한도 뒤 미처리 상태는 후속
  Tick에서 계속되지만, 그 호출의 남은 `DeltaSeconds`는 별도로 누적하지 않는다.
- Zone, Protected Area, FacilityActor는 기본적으로 상시 Tick하지 않는다. Queue는
  `Closed`가 아닐 때, Preview Actor는 자동 갱신을 켰을 때만 Tick한다.
- Debug Cell 전개는 최대 10,000개, 압축 영역의 이론적 안전 예산은 20,000,000셀이다.
  이 값들은 정상 Gameplay 규모를 보장하는 벤치마크가 아니라 폭발적 입력을 막는 안전선이다.

### 21.2 스레드와 재진입

World 상태를 읽고 바꾸는 공개 흐름은 **Game Thread 사용을 전제로** 한다. 명령 실행은
Game Thread 검사를 하고, 처리 중 다시 Command를 실행하는 재진입을 거부한다. 외부 정책
Callback과 Event Listener에서 즉시 새 명령을 호출하기보다 다음 Tick 또는 별도 Queue에
예약해야 한다.

일부 단순 조회 함수는 코드상 모든 호출 지점에서 명시적인 Thread Guard를 두지 않는다.
따라서 `const` 함수라는 이유만으로 Worker Thread에서 안전하다고 가정하면 안 된다.

### 21.3 네트워크

이 플러그인은 현재 다음을 제공하지 않는다.

- Server RPC 또는 Client RPC
- `Replicated` Property와 `GetLifetimeReplicatedProps` 구현
- `HasAuthority()` 기반의 Server 권한 검사
- Command·Queue·Snapshot의 자동 네트워크 동기화
- Client Prediction 또는 Reconciliation

따라서 `UFFZSWorldSubsystem`의 “월드 논리 권한”은 Unreal Networking의 Server Authority와
같은 뜻이 아니다. 멀티플레이 프로젝트에서는 서버만 변경 API를 호출하게 만들고,
필요한 Read Model과 Event를 프로젝트 계층에서 복제해야 한다.

---

## 22. 현재 구현 범위와 주의사항

아래 항목은 버그 단정이 아니라, 현재 코드가 의도적으로 제공하지 않거나 호출자가 보완해야
하는 경계다.

### 22.1 등록과 Zone

- 서로 다른 Zone의 Geometry가 겹치는 것 자체를 전역적으로 금지하지 않는다.
  위치 해석 시에는 정렬된 Zone ID 순서와 포함 규칙이 결과에 영향을 준다.
- `NeighborZoneIds`와 `EntryExitAnchors`는 그래프·메타데이터다. 플러그인이 Zone 간
  최단 경로 탐색이나 NPC 이동을 자동 수행하지는 않는다.
- Zone을 해제해도 그 Zone의 시설을 자동 철거하지 않는다. 호출자가 시설 수명주기와
  등록 해제 순서를 관리해야 한다.
- 초기화 시 기존 Actor를 한 번 스캔하고 이후에는 Actor의 `BeginPlay` 등록에 의존한다.
  Private `RefreshWorldActors()`는 현재 비어 있다.
- Registry는 Actor를 Weak Pointer로 보관한다. Actor 생성·파괴가 비정상 순서로 일어나면
  로그와 반환값을 확인해야 한다.

### 22.2 배치와 Navigation

- Zone 재검증은 현재 Phase 규칙을 통과하는 Placement Validator를 다시 사용한다.
  `LiveOperation`, `EveningSettlement`처럼 배치를 허용하지 않는 Phase에서 호출하면
  공간 문제가 없어도 `WrongStage`로 Invalid 처리될 수 있다. 현 구현에서는
  `InitialSetup` 또는 `MorningCheck`에서 호출한다.
- Navigation 안전 검사는 Settings로 비활성화할 수 있고, NavSystem 부재를 실패로 볼지도
  설정에 따라 달라진다. 안전·접근성·법규 준수의 최종 보증 수단으로 단독 사용하면 안 된다.
- `NavRebuildTimeout`은 enum에 있으나 현재 비동기 Nav 재빌드 대기 Pipeline은 없다.
- Density 경고 임계값의 상호 순서를 Runtime에서 강하게 교정하지 않는다. Settings에서
  `DensityCautionRatio <= DensityWarningRatio <= DensityCriticalRatio` 순서를 유지해야 한다.

### 22.3 Snapshot과 복원

Snapshot은 Zone·Protected Area·정책·외부 시스템 상태·실제 Agent Actor를 저장하지 않는다.
복원 전에 Snapshot의 `ZoneId`와 같은 Gameplay Zone이 등록되어 있어야 한다. 전체
`FFFZSFacilitySpec`은 Snapshot에 들어 있으므로 원본 `UFFZSFacilityDefinition` Asset은 필요하지 않다.

복원은 논리 상태를 Stage한 뒤 교체하지만 다음 작업은 다시 수행하지 않는다.

- 현재 지면 Trace
- 월드 Geometry Collision 검사
- Nav 경로 검사 또는 NavMesh 재빌드
- 외부 정책 PreValidate / ApplyTentative / Commit
- `OnWorldChanged` Broadcast

시설 Actor 복원은 기본 `AFFZSFacilityActor`를 생성한다. 프로젝트별 Subclass, 외형, 추가
Component 또는 외부 저장 데이터는 별도 Factory/복원 계층에서 다시 연결해야 한다.

Schema 3에서 Schema 4로의 변환은 100 cm 단위를 1 cm 단위로 바꾸는 제한적 이행 경로다.
임의의 더 오래된 Schema나 미래 Schema를 자동 수용하지 않는다.

### 22.4 Queue와 AI

플러그인은 Queue의 논리적 순서와 상태를 관리하지만, 실제 Character 이동·충돌 회피·애니메이션·
Spawn/Despawn을 구현하지 않는다. 프로젝트 NPC가 `IFFZSQueueAgent` Callback을 받아 이동하고,
도착 또는 실패를 반드시 Report해야 한다.

현재 FestaFiesta QA Harness는 Null Agent Actor와 수동 도착 보고를 사용한다. 따라서
“Queue 상태 머신이 동작한다”는 검증과 “실제 AI가 목적지까지 이동한다”는 검증을 구분해야 한다.

### 22.5 외부 연동과 콘텐츠

- 프로젝트 소스에는 현재 Production `IFFZSExternalPolicy` 구현이 없다. 기본 설정
  `bRequireProductionPolicyDomains=false`에서는 정책 없이 내부 검증만 적용된다. true이면
  6개 필수 Domain 부재로 Command가 `ExternalPolicyRejected`된다.
- `CanContainContent=true`지만 플러그인 자체 Content 폴더에는 운영용 Facility Asset 묶음이
  없다. `UFFZSFacilityDefinition` Asset 생성과 Asset Manager 정책은 호스트 프로젝트 책임이다.
- 운영용 UMG, 사용자 메시지 번역, 분석·Telemetry, 저장 슬롯 관리는 포함하지 않는다.
- `WorldRevision`은 성공한 Facility Command에 대해서만 증가한다. Queue 진행, Population,
  Phase, Registry 변화까지 포괄하는 전역 변경 번호가 아니다.

---

## 23. 소스 탐색 지도

| 관심 영역 | 먼저 볼 파일 |
|---|---|
| 공통 enum, Command, Result, Snapshot 구조체 | `Source/FF_ZoneSystem/Public/FF_FFZSTypes.h`, `Source/FF_ZoneSystem/Private/FF_FFZSTypes.cpp` |
| Registry, Placement, Command, Policy, Snapshot, Read Model | `Source/FF_ZoneSystem/Public/FF_FFZSWorldSubsystem.h`, `Source/FF_ZoneSystem/Private/FF_FFZSWorldSubsystem.cpp` |
| Zone 경계·규칙·그래프 | `Source/FF_ZoneSystem/Public/FF_FFZSZoneActor.h`, `Source/FF_ZoneSystem/Private/FF_FFZSZoneActor.cpp` |
| Protected Area | `Source/FF_ZoneSystem/Public/FF_FFZSProtectedArea.h`, `Source/FF_ZoneSystem/Private/FF_FFZSProtectedArea.cpp` |
| Facility Definition 검증·Asset ID | `Source/FF_ZoneSystem/Public/FF_FFZSFacilityDefinition.h`, `Source/FF_ZoneSystem/Private/FF_FFZSFacilityDefinition.cpp` |
| Runtime Facility와 Anchor | `Source/FF_ZoneSystem/Public/FF_FFZSFacilityActor.h`, `Source/FF_ZoneSystem/Public/FF_FFZSFacilityAnchorComponent.h` |
| Queue 상태 머신 | `Source/FF_ZoneSystem/Public/FF_FFZSQueueComponent.h`, `Source/FF_ZoneSystem/Private/FF_FFZSQueueComponent.cpp` |
| Queue Agent 계약 | `Source/FF_ZoneSystem/Public/FF_FFZSQueueAgent.h`, `Source/FF_ZoneSystem/Private/FF_FFZSQueueAgent.cpp` |
| 1 cm 격자 연산·명령 지문 | `Source/FF_ZoneSystem/Public/FF_FFZSGridLibrary.h`, `Source/FF_ZoneSystem/Private/FF_FFZSGridLibrary.cpp` |
| 외부 정책 계약 | `Source/FF_ZoneSystem/Public/FF_FFZSExternalPolicy.h`, `Source/FF_ZoneSystem/Private/FF_FFZSExternalPolicy.cpp` |
| Preview | `Source/FF_ZoneSystem/Public/FF_FFZSPlacementPreviewActor.h`, `Source/FF_ZoneSystem/Private/FF_FFZSPlacementPreviewActor.cpp` |
| Settings | `Source/FF_ZoneSystem/Public/FF_FFZSDeveloperSettings.h`, `Source/FF_ZoneSystem/Private/FF_FFZSDeveloperSettings.cpp` |
| Runtime Unit Test | `Source/FF_ZoneSystem/Private/Tests/` |
| 자동화 전용 시설 Fixture | `Source/FF_ZoneSystem/Public/Tests/FF_FFZSTestFixtures.h`, `Source/FF_ZoneSystem/Private/Tests/FF_FFZSTestFixtures.cpp` |
| Editor Integration·SaveGame Test | `Source/FF_ZoneSystemTests/Private/` |

이 문서는 현재 소스 구현을 기준으로 작성되었다. API, Snapshot Schema, 단위 계약 또는
Phase 규칙을 바꾸면 관련 자동화 테스트와 이 문서를 같은 변경에서 함께 갱신해야 한다.
