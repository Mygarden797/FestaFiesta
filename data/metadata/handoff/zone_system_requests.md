# 한광훈 님께 — 요청 1건 + 확인 3건

김시리 (Utility AI), 2026-08-10 · `NPC/Integration/FF_ZoneSystemBridge.*`
(네이밍 규칙은 별도 문서: `facility_naming_convention.md`)

ZoneSystem 읽기 모델을 NPC 목적지 후보로 바꾸는 부분을 구현했습니다.
**"1번 ⓐ"** 처럼만 답 주셔도 됩니다.

## 처리 결과 — 2026-08-10

- **1번 ⓐ 완료** — `FFFZSFacilityReadModel::WorldLocation`(시설 Actor 대표 월드 지점, cm)을 추가하고 NPC 브리지가 이 값으로 직선거리를 계산하도록 연결 완료했습니다.
- **2번 ⓐ 확인** — 미입력은 기본 12이며, 0 이하는 Spec 검증에서 거부되고 Queue에서도 최소 1로 보정된다.
- **3번 ⓐ 확인** — Bridge에 전달되는 시설은 권위 등록 전에 Spec 검증을 통과하며 Queue에도 양수 시간 보정이 있다.
- **4번 ⓐ 확인** — `FindQueueFacilityId`는 GUID→int32 `TMap` 단일 조회이므로 소비자 캐시는 두지 않는다.

---
## 김시리님께
1번 ⓐ. FFFZSFacilityReadModel에 FVector WorldLocation을 추가하고 GetReadModel()에서 GetActorLocation()을 담겠습니다. 단위는 cm이며 Queue 슬롯이 아닌 시설 Actor 대표점입니다. 현재 용도에는 회전·스케일이 필요 없으므로 FTransform 전체보다 FVector가 적절합니다.
2번 ⓐ. 정확히는 미입력 시 기본값이 12이고, 0은 유효한 사양으로 취급하지 않습니다. Spec.IsValid()에서 설치가 거부되고 Queue 구성 시에도 최소 1로 보정되므로, 빈 줄이 항상 Full인 시설은 정상 경로로 만들어지지 않습니다.
3번 ⓐ. RegisterOrRefreshFacility() 자체에서는 중복 검사를 하지 않지만, 여기에 전달되는 시설은 설치 명령·Actor 초기화·Registry 등록·Snapshot 복원 과정에서 이미 Spec.IsValid()를 통과한 객체뿐입니다. 서비스 시간 0은 그 전에 거부되며 Queue 변환과 Queue 코어에도 양수 보정이 있어 처리율 0 → 24시간 경로로 가지 않습니다. NPC 쪽 별도 차단은 필요 없습니다.
4번 ⓐ. 내부 구현이 GUID→int32 TMap 단일 조회라 후보 12개를 재평가마다 조회해도 부담 없습니다. 별도 캐싱 없이 그대로 호출해 주세요.

ReportZonePopulation() 호출부가 0곳인 것도 확인했습니다. Zone별 인원을 crowdAI가 소유한다면 그쪽에서 정기 보고하는 방향이 적절합니다. 몇 초 간격으로 호출한다면 DeltaSeconds에는 실제 보고 간격을 전달해 주세요.

읽기 모델에는 현재 위치가 없고, GetReadModel()도 위치를 복사하지 않습니다. 반면 Actor 배치 위치는 footprint 대표 중심으로 계산됩니다: [FF_FFZSTypes.h (line 752)](C:/GitHub/FestaTeam/8th-Team13-Final-Project/Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Public/FF_FFZSTypes.h:752), [FF_FFZSFacilityActor.cpp (line 320)](C:/GitHub/FestaTeam/8th-Team13-Final-Project/Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSFacilityActor.cpp:320)

MaxWaitingAgents는 기본 12, 에디터 최소 1이며, IsValid()가 0 이하를 거부합니다. Queue에서도 다시 최소 1로 보정합니다: [FF_FFZSTypes.cpp (line 123)](C:/GitHub/FestaTeam/8th-Team13-Final-Project/Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSTypes.cpp:123), [FF_FFZSQueueComponent.cpp (line 36)](C:/GitHub/FestaTeam/8th-Team13-Final-Project/Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSQueueComponent.cpp:36)

시설 사양은 설치 검증뿐 아니라 Actor 초기화와 Registry 등록에서도 재검사됩니다: [FF_FFZSWorldSubsystem.cpp (line 897)](C:/GitHub/FestaTeam/8th-Team13-Final-Project/Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSWorldSubsystem.cpp:897), [FF_FFZSFacilityActor.cpp (line 121)](C:/GitHub/FestaTeam/8th-Team13-Final-Project/Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSFacilityActor.cpp:121)

FindQueueFacilityId()는 실제로 TMap::Find() 한 번입니다. 소비자 캐시는 이득이 거의 없고 드문 ID 재할당 시 오히려 stale 위험이 있습니다: [FF_ZoneSystemQueueBridgeSubsystem.cpp (line 144)](C:/GitHub/FestaTeam/8th-Team13-Final-Project/Source/FestaFiesta/Private/Zone/FF_ZoneSystemQueueBridgeSubsystem.cpp:144)

ReportZonePopulation()은 누적 DeltaSeconds로 Warning/Critical 유지 시간을 계산하므로 실제 호출 간격 전달이 중요합니다: [FF_FFZSWorldSubsystem.cpp (line 2119)](C:/GitHub/FestaTeam/8th-Team13-Final-Project/Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSWorldSubsystem.cpp:2119), [FF_FFZSZoneActor.cpp (line 308)](C:/GitHub/FestaTeam/8th-Team13-Final-Project/Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSZoneActor.cpp:308)

---

## 1. (요청) `FFFZSFacilityReadModel`에 시설 위치를 넣어주실 수 있을까요

시설이 어디 있는지를 읽을 수 없어서 **NPC 입장에서 장 반대편 부스와 바로 앞 부스가 똑같습니다.**
지금은 모든 거리를 0으로 두고 있습니다.

값은 이미 갖고 계십니다 — `AFFZSFacilityActor::CaptureInstanceSnapshot()`은 `WorldTransform`을
담는데 `GetReadModel()`은 안 담습니다.

|       | 답                                                                             |
| ----- | ----------------------------------------------------------------------------- |
| **ⓐ** | **`FFFZSFacilityReadModel`에 `FVector WorldLocation` 추가** (또는 `FTransform` 전체) |
| ⓑ     | 다른 조회 경로가 이미 있다 → 알려주시면 그쪽을 쓰겠습니다                                             |

- **단위는 cm 그대로** 주세요 (m 환산은 제 쪽에서만 합니다)
- Queue 슬롯이 아니라 **시설 대표 지점**이면 됩니다

---

## 2. (확인) 대기 인원이 0명인 시설이 만들어질 수 있나요

```cpp
// UFFZSFacilityQueueComponent::RefreshOpenOrFullState()
SetQueueState(Waiting.Num() >= Rules.MaxWaitingAgents ? Full : Open);
```

`MaxWaitingAgents`가 0이면 `0 >= 0`이 참이라 **줄이 비어 있어도 항상 `Full`**입니다.
그 시설은 NPC가 영원히 못 가는데 로그에도 안 남습니다. 내장 프리셋 7종은 전부 값이 있습니다.

|       | 답                                     |
| ----- | ------------------------------------- |
| **ⓐ** | **커스텀 Data Asset에서도 비워둘 수 없다** → 닫습니다 |
| ⓑ     | 비워둘 수 있다 → 저작 주의사항으로 팀에 공유하겠습니다       |

---

## 3. (확인) `RegisterOrRefreshFacility`가 `Spec.IsValid()`를 안 부르는 것 같습니다

`UFF_ZoneSystemQueueBridgeSubsystem`이 시설 사양을 그대로 Queue 설정으로 바꾸는데 검사를
건너뜁니다. **서비스 시간이 0인 시설이 등록되면** 처리율이 0이 되고, 장원재 님 계산식이 그걸
"영원히 안 빠지는 줄"로 보고 **대기시간 24시간**을 돌려줍니다 → 아무도 안 가는데 경고도 없습니다.

|       | 답                                             |
| ----- | --------------------------------------------- |
| **ⓐ** | **검사 안 거친 사양으로 등록되는 경로가 없다** → 닫습니다           |
| ⓑ     | 있을 수 있다 → 등록할 때 `Spec.IsValid()` 검사 추가 부탁드립니다 |
| ⓒ     | 제 쪽에서 막겠습니다 → 처리율 0인 시설을 후보에서 뺍니다             |

---

## 4. (확인) `FindQueueFacilityId` 결과를 캐싱해도 되나요

NPC가 목적지를 고를 때마다 후보(최대 12개)의 `FGuid`를 Queue 쪽 `int32`로 바꿔야 해서
`FindQueueFacilityId`를 재평가마다 12번씩 부르게 됩니다.

|       | 답                                         |
| ----- | ----------------------------------------- |
| **ⓐ** | **매번 조회해도 부담 없다** → 그냥 부르겠습니다             |
| ⓑ     | 캐싱이 낫다 → 시설 추가·제거 시 **무효화 신호**를 주실 수 있을까요 |

---

## 참고 — 광훈님 기능이 하나 안 쓰이고 있습니다

`ReportZonePopulation()`을 부르는 코드가 프로젝트 전체에 **0곳**입니다. 외부에서 인구를
보고해야 Zone 밀도 상태(Caution/Warning/Critical)가 올라가는 설계인데 아무도 안 부릅니다.
Zone별 인원은 crowdAI가 아는 값이라 이정원 님과 정리하겠습니다.
