# 한광훈 님께 — 요청 1건 + 확인 3건

김시리 (Utility AI), 2026-08-10 · `NPC/Integration/FF_ZoneSystemBridge.*`
(네이밍 규칙은 별도 문서: `facility_naming_convention.md`)

ZoneSystem 읽기 모델을 NPC 목적지 후보로 바꾸는 부분을 구현했습니다.
**"1번 ⓐ"** 처럼만 답 주셔도 됩니다.

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
