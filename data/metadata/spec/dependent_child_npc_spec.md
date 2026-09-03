# 종속 아동 NPC 메커닉 명세 (초안)

- 작성일: 2026-08-04
- 상태: 생성·리더 상속은 구현됨. 아동 전용 행동 3종은 초안（2026-09-03 코드 대조, 값·실행 분기 미구현）
- 결정 배경: 2026-08-04 사용자가 생애주기 5종 + 동반유형 오버레이 + 아동 종속 NPC 구조를 확정함
- **2026-08-04 갱신 (이중 계산 문제 발견·수정)**: 이 문서의 3절(아래)이 원래 설명하던 "확률 0.4로 아동 추가" 메커닉은, 일반 인구 샘플러가 `age_10_or_under`(0~19세 통합)를 이미 `life_stage_teen`으로 독립 샘플링하고 있는 것과 겹쳐서 **아동이 이중으로 계산되는 문제**가 있었다. 해결책과 정확한 생성 순서는 `population_group_generation_spec.md` 2절로 옮겼다 — 당시 0.4·1.5를 파생값으로 재정의했으나 2026-08-06에 두 노브 자체를 폐기했다（현재는 그룹 크기별 아동 수 분포가 생성 입력이고 인구 목표 비율은 검산용）. 이 문서는 계속 "종속 아동이 만들어진 뒤 어떻게 행동하는가"(2절, 4절)만 다룬다.

## 1. 왜 독립 아키타입으로 만들지 않는가

인구 데이터(`data/cleaned/population/`)의 `age_10_or_under` 세그먼트는 0~19세를 통째로 묶는다. 반면 행동 데이터(Tour_Stat 2018)는 **15~19세부터** 시작한다. 0~14세 아동의 독립적인 여행지 선택·활동 성향을 관측한 자료는 국민여행조사·데이터랩 어느 쪽에도 없다.

`data/derived/npc_archetypes_v0.csv`의 `life_stage_teen` 행은 15~19세 응답만을 대리값으로 쓰고 있으며, 그 메타데이터(`npc_archetypes_v0.json` `prohibited_uses`)에 "0~14세 아동에게 그대로 적용하지 않는다"고 명시했다. 이 명세가 그 공백을 메우는 설계다.

아동을 독립 아키타입으로 만들면 존재하지 않는 데이터를 있는 것처럼 다루게 된다. 대신 **성인 아키타입에 종속되는 개체**로 설계한다.

## 2. 종속 관계

> **2026-08-06 정정**: 아래 트리의 "동반유형 group_3_plus 일 때만 1 이상"은 틀렸다 — 실측상 `pair`(2인) 그룹도 아동을 포함한다(0.38%). 그리고 부모 아키타입이 `life_stage_30_40s`로 고정되지도 않는다. 3절 참조.

```text
성인 NPC (life_stage_30_40s 아키타입, 주로)
  └─ dependent_child_count: 0~N (크기 2~6 그룹 전부 가능 — 3절 정정 참조)
       └─ 아동 NPC 1인당
            ├─ destination_choice: 부모의 선택을 그대로 상속 (자체 Utility 계산 없음)
            ├─ movement_speed_multiplier: 부모보다 낮추는 설계 (값·아동 전용 분기 미구현)
            ├─ attention_span_minutes: 짧게 두는 설계 (부모 재평가를 촉발하는 분기 미구현)
            └─ wait_tolerance_multiplier: 부모보다 낮추는 설계 (값·실행 분기 미구현)
```

아동이 부모와 다른 목적지를 원하는 상황(짜증, 이탈 시도)은 v0 범위 밖이다. v1 이후 확장 후보로 남긴다.

**2026-08-04 갱신 — 일반 규칙으로 확장됨**: "자체 Utility 계산 없이 부모 목적지 상속"은 `population_group_generation_spec.md` 1.5절에서 **그룹 전체의 목적지 결정 방식**(그룹 리더 1명만 Utility 계산, 나머지 전원 상속)으로 일반화됐다. 아동은 이제 그 일반 규칙의 특수 케이스일 뿐이다 — 아동을 특별하게 만드는 건 `destination_choice` 상속이 아니라 위 표의 나머지 3개(이동속도·주의집중·대기 인내 노브)라는 설계뿐이다（현재 실행에서는 아직 차이를 만들지 않음）. 성인 그룹원(리더 아님)도 목적지는 상속받지만 이 3개 노브는 그대로 성인 기본값을 쓴다.

## 3. 발생 조건 — **(2026-08-06 전면 교체)** 노브 2개 폐기, 실측 분포로 대체

### 폐기된 것

| 노브 | 기존 값 | 상태 |
|---|---|---|
| ~~`child_attach_probability_given_group3plus`~~ | 0.4000 | **폐기** |
| ~~`child_count_mean_given_attached`~~ | 1.5000 | **폐기** |

두 노브 모두 실측 근거가 없었고, "목표 아동 수에 맞게 재조정되는 파생값"이라는 어정쩡한 위치에 있었다. 2025 국민여행조사 문항 `A6A`("일행 중 만 15세 미만은 몇 명이었습니까?")를 `WT_DOM`으로 가중 집계해 **그룹 크기별 아동 수 분포를 직접 얻었으므로 두 노브가 모두 불필요해졌다.**

### 대체 — `group_child_composition_v0.csv`

```text
1. 그룹 크기를 뽑는다        ← visitor_group_size_distribution_v0.csv
2. 그 크기에서 아동 수를 뽑는다 ← group_child_composition_v0.csv  (이 문서가 다루던 부분)
3. 그 수만큼 bIsDependentChildSlot 을 세운다
```

구현: `FFF_VisitorDistributionBuilder::Build` (`Source/FestaFiesta/Public/NPC/Party/FF_VisitorDistributionBuilder.h`).

**바뀐 사실 2개**:

1. **`group_3_plus` 전용이 아니다.** 아래 §2의 "동반유형 group_3_plus 일 때만"은 틀렸다 — 실측상 `pair`(2인) 그룹도 0.38%가 아동을 포함한다(보호자+아동 2인 조합). 크기 2~6 전부에 아동이 들어갈 수 있다.
2. **아동 수는 목표치가 아니라 파생값이다.** `dependent_child_population_target_v0.csv`(7.9193% / 3.8925%)는 **생성 입력이 아니라 검산용으로 격하**됐다(2026-08-06 결정, `population_group_generation_spec.md` 2.4절). 실제 생성 결과는 **6.4167%**이며 원자료 실측 6.4506%와 0.034pp 차이다(차이는 리더 성인 불변식으로 "전원 아동" 칸을 제외·재정규화한 몫).

### 부모 아키타입은 어느 것인가

**부모 역할을 별도로 지정하는 정책은 없다.** 원래 `life_stage_30_40s`를 기본 대상으로 하려 했으나, 현재 빌더는 그룹의 성인 아키타입을 인구비중대로 뽑고 아동 수는 그와 **독립적으로** 뽑는다 — 즉 60대 성인 그룹에도 아동이 붙을 수 있다(조부모+손자녀는 실제로 존재하므로 비현실적이지는 않다. `companion_type_response_v0.csv`에서 5인 그룹의 "손자/손녀" 응답률이 11.3%로 관측된다).

연령×아동 교차를 실제로 반영하려면 `A6A`를 응답자 연령(`BAGE`)과 교차집계해야 한다 — v0 범위 밖으로 남긴다. 기존 정성적 근거("40대의 가족/친지 방문 응답률 13.2%")는 방문지 선택 이유가 아니라 활동 항목이며 가족 동반을 직접 증명하지 않으므로 그대로 참고 수준이다.

## 4. Unreal 구조 제안

（2026-09-03 코드 대조: 실제 구조는 FUnitData.bIsDependentChild / PartyID / LeaderAgentIndex / PartySlotIndex다. 아래 bIsDependent·DependentOfNpcId·bIsGroupLeader는 제안 명칭이며 해당 이름의 런타임 필드는 없다. 리더 전용 결정과 일행 Need 최댓값 집계는 구현돼 있고 아동 전용 3개 knob는 미구현이다.）

- 아동은 별도 `FFF_ArchetypeRow`를 만들지 않는다.
- **2026-08-04 갱신**: 목적지 상속 자체는 이제 아동 전용 로직이 아니라 `population_group_generation_spec.md` 1.5절의 **그룹 전체 규칙**(`GroupId`+`bIsGroupLeader`, 리더만 Utility 계산·나머지 상속)이 담당한다. `bIsDependent`/`DependentOfNpcId`는 목적지 상속용이 아니라 **아동 전용 3개 노브(이동속도·주의집중·대기인내)를 누구 기준으로 적용할지**만 남긴다 — 보통 `DependentOfNpcId` = 같은 그룹의 리더(부모) `StableAgentId`.
- NPC 스폰 시 `bIsDependent`, `DependentOfNpcId` 필드만 있는 경량 구조로 생성한다.
- Utility AI 목적지 선택 로직은 `bIsGroupLeader == false`인 개체 전체(아동뿐 아니라 성인 그룹원도)에 대해 스킵하고 리더의 `current_destination`을 그대로 복사한다. 그중 `bIsDependent == true`인 개체는 추가로 위 3개 노브를 적용한다.

**2026-08-06 갱신 — 상속해야 할 것이 2개 늘었고, 하나는 아직 미정이다.**

| 상속 대상 | 규칙 |
|---|---|
| `ChosenDestinationId` | 리더 값을 복사 (기존) |
| `FFF_ResolvedIndividualTraits`의 `WalkSpeedMultiplier`·`StayDurationMultiplier` | 상속이 아니라 **공유**다 — `ResolveWithGroupSharing`이 `GroupId` 시드로 뽑아 그룹원 전원에게 같은 값을 준다. 아동도 마찬가지(그래서 아동이 부모보다 느리게 걸어 뒤처지는 일이 없다). 아동 전용 이동속도 노브는 이 위에 곱하는 제안이며 **값·분기 미구현**이다（현재 성인·아동 모두 같은 그룹 공유 배수와 멤버 추격 보정을 사용하므로 순간 속도까지 같다는 뜻은 아님） |
| `FFF_NeedState` | 상속하지 않는다. 개체별로 갖고, 목적지 결정에는 `MakeAggregated`(성분별 최대)로 합친 값을 쓴다 — 한 명이라도 화장실이 급하면 일행 전체가 움직인다. **아동의 Need가 그룹 결정을 끌어당기는 경로가 이것이다** |
| `EFF_NoChoiceAction` (Wander/Relocate/**Leave**) | **리더 값을 상속** (2026-08-06 결정). 리더가 퇴장하면 **그룹 전원이 함께 퇴장한다** — 셋 다 상속 대상이고 그룹원이 각자 다른 행동을 하는 경우는 없다. 덕분에 **아동만 장내에 남는 상태가 원리적으로 발생하지 않는다.** `spawn_group_together = true`와 대칭 — 통째로 들어오고 통째로 나간다 (`population_group_generation_spec.md` 3절 항목 8) |

> ⚠️ 아동 전용 3개 노브(이동속도·주의집중·대기인내)는 여전히 **값이 없다**(`group_composition_rules_v0.csv`의 `child_trait_overrides`가 `assumption`). 지금은 성인과 같게 동작한다.

## 5. v0에서 하지 않는 것

- 아동의 독립적 선호 벡터를 만들지 않는다.
- 아동이 부모와 분리되는 이벤트를 만들지 않는다.
- 부모(성인)의 아키타입과 아동 수를 교차시키지 않는다 — 3절 "부모 아키타입은 어느 것인가" 참조.

## 6. 다음에 검증이 필요한 것

- 부모 연령(`BAGE`) × 아동 수(`A6A`) 교차 — 지금은 독립으로 뽑는다. 3절 참조.
- 아동의 부스 유형별 체류시간 차이 — 별도 관찰조사나 유사 축제 사례가 필요하다.
- 이동속도·주의집중·대기인내 3개 노브의 실제 값 — 여전히 미정(`group_composition_rules_v0`의 `child_trait_overrides`, `assumption`).

## 코드 대조 근거 (2026-09-03)

- [FF_FestivalCrowdTypes.h](../../../Source/FestaFiesta/Public/CrowdSystem/Types/FF_FestivalCrowdTypes.h)
- [FF_VisitorDistributionBuilder.cpp](../../../Source/FestaFiesta/Private/NPC/Party/FF_VisitorDistributionBuilder.cpp)
- [FF_CrowdSimulationSubsystem.cpp](../../../Source/FestaFiesta/Private/CrowdSystem/Systems/FF_CrowdSimulationSubsystem.cpp)

（현재 작업 트리 기준. 데이터/계수 수는 저장소 CSV, 설정은 C++ 기본값과 Config를 구분해 기술했다. 설계 제안·과거 실측은 코드만으로 재검증할 수 없어 해당 설명을 보존했다.）
