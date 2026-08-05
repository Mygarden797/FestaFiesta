# 고정 Seed 기반 개체 변주 명세 (v0)

- 작성일: 2026-08-04
- 상태: 초안 — 6종 변주 범위는 `draft_assumption`, 나머지 구조(해시·분포·상관 방식)는 확정
- 목적: 아키타입(`FFF_ArchetypeRow`)·동반유형(`FFF_CompanionModifierRow`)은 집단 평균이다. 같은 Row를 쓰는 NPC들이 복제처럼 보이지 않도록 개체별로 조금씩 다른 행동양식(보행속도·인내심·혼잡내성·선호강도·체류시간·재평가주기)을 주면서도, 같은 Seed·같은 Agent ID에는 항상 같은 값이 나오게 해서 디버깅과 실험 재현이 가능하게 한다.

## 1. 최종 개체값 구성

```text
최종 개체값 = 아키타입 기준값 + 동반유형 보정 + Seed 기반 개체 변주(이 문서)
```

이 문서는 세 번째 층(개체 변주)만 다룬다. 앞 두 층은 `utility_ai_spec.md`, `dependent_child_npc_spec.md` 참조.

## 2. TraitSeed 구성 규칙

```text
TraitSeed = HashCombine(
    HashCombine(
        HashCombine(
            HashCombine(GetTypeHash(GlobalSimulationSeed), GetTypeHash(StableAgentId)),
            GetTypeHash(RandomPurpose)),
        GetTypeHash(TraitKey)),
    GetTypeHash(SpawnGeneration))
```

- `GlobalSimulationSeed` (int32): 시뮬레이션 실험 전체 Seed. 같은 시나리오 재현에 사용.
- `StableAgentId` (int32): 실행 순서·메모리 주소가 아니라 저장·복원 가능한 NPC 고유 ID.
- `RandomPurpose` (FName): `Generation` / `Decision` / `RuntimeNoise` 중 하나 (3절).
- `TraitKey` (FName): `walk_speed_multiplier` 등 특성별 고정 문자열.
- `SpawnGeneration` (int32): 같은 ID를 재사용할 때만 쓰는 세대 번호. 필요 없으면 0.
- 금지 입력: 현재 시각, 포인터 주소, Actor 스폰 순서, 프레임 번호 — 실행마다 달라질 수 있는 값은 절대 넣지 않는다.

`TraitSeed`를 `FRandomStream`에 넣어 `GetFraction()`을 한 번 호출하면 `[0,1)` 균등난수 하나를 얻는다(`UniformFraction`). 특성마다 `TraitKey`가 다르므로 각 특성은 독립된 난수 스트림을 갖는다 — 하나의 스트림을 순서대로 소비하지 않으므로, 나중에 특성을 추가해도 기존 특성값이 바뀌지 않는다.

구현: `Source/FestaFiesta/Public/NPC/FF_DeterministicRandom.h`의 `FFF_DeterministicRandom::MakeTraitSeed` / `MakeUniformFraction`.

## 3. 난수 3종 분리

`RandomPurpose`를 해시 재료에 포함시키는 것만으로 분리한다. 별도 클래스나 자료구조가 필요 없다 — 같은 `TraitKey`라도 `RandomPurpose`가 다르면 완전히 다른 `TraitSeed`가 나온다.

| Purpose | 용도 | v0 사용 범위 |
|---|---|---|
| `Generation` | NPC 생성 시 한 번 정해지고 유지되는 성격·신체 특성 | 이 문서의 6종 multiplier + `leisureliness_axis` 전부 |
| `Decision` | softmax 목적지 선택 샘플링. 선택 회차 인덱스를 `SpawnGeneration` 또는 별도 인자로 Seed에 포함해 재현 | 목표 4(목적지 선택기 v0)에서 사용 — 이 문서에서는 훅만 마련 |
| `RuntimeNoise` | 애니메이션·시각적 흔들림 등 의사결정에 영향 없는 표현용 난수 | 아직 사용처 없음 |

## 4. 분포 함수 — 삼각분포로 통일

v0의 모든 특성은 `(Min, Mode, Max)` 세 값으로 정의되는 삼각분포를 쓴다. 정규분포는 이론상 범위 제한이 없어 `Max`를 넘는 값이 나오면 clamp해야 하는데, clamp는 경계값에 확률이 비정상적으로 쌓이는 부작용(clamp bias)이 있다. 삼각분포는 처음부터 `[Min, Max]` 밖으로 나가지 않으므로 이 문제가 없고, 균등난수 하나에서 닫힌 공식으로 값을 얻어 계산도 단순하다.

균등난수 `U ∈ [0,1)`을 삼각분포 값으로 바꾸는 역누적분포함수:

```text
C = (Mode - Min) / (Max - Min)

U < C 이면:  X = Min + sqrt(U * (Max - Min) * (Mode - Min))
U ≥ C 이면:  X = Max - sqrt((1 - U) * (Max - Min) * (Max - Mode))
```

구현: `FFF_DeterministicRandom::SampleTriangularFromFraction`.

## 5. v0 변주 6종 — 대상 필드와 범위

각 multiplier가 **정확히 어떤 기존 필드에 곱해지는지**를 여기서 확정한다. 이 매핑이 없으면 숫자를 정해도 의미가 없다.

| TraitKey | 곱해지는 대상 | Min | Mode | Max | 상태 |
|---|---|---|---|---|---|
| `walk_speed_multiplier` | NPC 기본 이동속도 (Character MovementComponent 상수) | 0.85 | 1.00 | 1.15 | draft_assumption |
| `patience_multiplier` | `wait_threshold_minutes` (재평가 트리거, 기본 15분) | 0.70 | 1.00 | 1.30 | draft_assumption |
| `crowd_tolerance_multiplier` | `density_threshold_ratio` (재평가 트리거, 기본 0.85) | 0.70 | 1.00 | 1.30 | draft_assumption |
| `preference_strength_multiplier` | `preference_vector` (아키타입+동반유형 delta 합산값 — `lambda_affinity` 자체는 건드리지 않음) | 0.60 | 1.00 | 1.40 | draft_assumption |
| `stay_duration_multiplier` | 체류시간 기준값 (6절) | 0.70 | 0.90 | 1.60 | draft_assumption |
| `reevaluate_interval_multiplier` | `reevaluate_interval_seconds` (기본 120초) | 0.80 | 1.00 | 1.20 | draft_assumption |
| `leisureliness_axis` | 직접 곱하는 대상 없음 — 7절 상관관계용 공유 성격축 | 0.00 | 0.50 | 1.00 | draft_assumption |

**중요한 구분**: `weight_wait`, `weight_density`, `lambda_affinity`는 "게임 전체가 대기·혼잡·취향을 얼마나 중요하게 보는가"를 정하는 전역 밸런스 노브다. 위 multiplier들은 "이 개인이 그 전역 기준에서 얼마나 벗어나는가"를 정하는 개인차이며, 전역 가중치 자체를 개인화하지 않는다. 두 개념을 같은 계수에 합치면 밸런스 문제와 개체차 문제를 구분해 말할 수 없게 된다.

CSV 원본: `data/derived/individual_variation_knobs_v0.csv` (7행). Unreal DataTable: `data/unreal/individual_variation_knobs_v0.csv` → `FFF_IndividualVariationRow`.

## 6. 체류시간 앵커 — "Live 단계 길이의 몇 %"

체류시간 분포는 공공데이터로 확보할 수 없는 값이라 기획 판단이 필요하다(`utility_ai_spec.md` 8절 "체류시간 분포 — 노브"). 다만 절대 분(分) 숫자를 감으로 정하면 축제 운영시간이 바뀔 때마다 재조정해야 하므로, 게임 안의 다른 구조적 단위에 앵커를 건다.

```text
BaseStayDurationMinutes = live_stage_duration_minutes * stay_duration_share_of_live_stage_mean
FinalStayDurationMinutes(NPC) = BaseStayDurationMinutes * stay_duration_multiplier(NPC)
```

- `live_stage_duration_minutes`: `EFF_FestivalStage::Live` 단계의 실제 길이. **단위는 게임(시뮬레이션) 시간 분이다 — 배속(x1/x2/x4)이나 플레이어 실제 체감 시간과 무관하다.** 값의 출처는 실제 축제의 실제 운영시간(현실 시간)을 그대로 게임 시계에 옮긴 것이다(2026-08-05, 강민서 작업 중인 실제 프로그램 시간표 기준: 17:00~21:00, 4시간 = 240분). 4시간(240분)이라는 숫자 자체는 바뀌지 않았다 — 처음엔 근거 없는 임시값이었는데 실제 시간표와 우연히 일치했다. **다만 강민서 님의 배속·Tick 구현이 아직 진행 중이라 다른 클래스의 시간 단위(초/분, 배율)는 아직 서로 안 맞을 수 있다 — 그쪽 작업이 끝나면 전체 재점검 필요.** `status: confirmed`(실제 운영시간 기준값 확정) — 코드상 실제 값 반영은 `FF_FestivalStageSubsystem` 담당(강민서) 완료 후.
- `stay_duration_share_of_live_stage_mean`: 평균 NPC가 `Live` 단계 길이 중 머무는 비율의 기준값(초기값 0.5). `status: assumption`.
- 두 값 모두 `data/derived/utility_coefficients_v0.csv`에 `individual_variation_anchor` term으로 추가했다 (전역 스칼라 노브라 기존 Utility 계수 표와 같은 자리에 둔다 — `total_visitor_count` 등 Utility 식의 항이 아닌 전역 노브도 이미 이 표에 있는 선례를 따른다).

`remaining_stay` 필드 자체를 채우는 NPC 런타임 로직은 아직 없다(목표 4 목적지 선택기 구현 시 필요). 이 문서는 값을 만드는 공식만 확정한다.

## 7. 특성 간 상관관계 — 공유 성격축 1개

전체 상관행렬은 v0 범위를 넘는다. 대신 `patience_multiplier`와 `stay_duration_multiplier`(실제로 상관될 법한 두 특성 — 여유 있는 사람이 대기도 잘 참고 오래 머문다)에만, 공유 값 하나(`leisureliness_axis`)를 통해 가벼운 상관을 준다.

```text
u_own      = 그 특성 고유의 균등난수 (독립적으로 뽑음)
u_leisure  = leisureliness_axis의 균등난수 (모든 상관 대상 특성이 공유)
u_blended  = Lerp(u_own, u_leisure, correlation_strength)

최종값 = SampleTriangularFromFraction(Min, Mode, Max, u_blended)
```

- `correlation_strength ∈ [0,1]`: `data/derived/utility_coefficients_v0.csv`에 `correlation_strength_leisureliness` 노브로 추가했다(초기값 0.3).
- `correlation_strength = 0`이면 완전 독립(기존 모델과 동일), `1`이면 두 특성이 같은 축에서 완전히 파생된다.
- 블렌딩은 **균등난수(0~1) 단계에서** 하고, 그 결과를 삼각분포 공식에 한 번만 통과시킨다. 이렇게 하면 특성마다 `Min/Mode/Max`가 달라도 같은 블렌딩 로직을 재사용할 수 있고, 결정성도 그대로 유지된다(모든 입력이 여전히 순수하게 결정적).
- `walk_speed_multiplier`, `crowd_tolerance_multiplier`, `preference_strength_multiplier`, `reevaluate_interval_multiplier`는 v0에서 상관 없이 완전 독립으로 둔다. 이는 `independence_assumption`이며, 실제로는 상호작용이 있을 수 있으나 관측 자료가 없다.

구현: `FFF_IndividualVariationResolver::Resolve` (`Source/FestaFiesta/Public/NPC/FF_DeterministicRandom.h`).

## 8. 완료 조건과 테스트

- 같은 `GlobalSimulationSeed + StableAgentId`는 몇 번을 다시 생성해도 모든 변주값이 동일하다.
- `GlobalSimulationSeed` 또는 `StableAgentId`를 바꾸면 변주값이 달라진다.
- 새 TraitKey를 추가해도 기존 TraitKey의 결과가 바뀌지 않는다(독립 스트림이므로 구조적으로 보장됨).
- 모든 값은 정의된 `Min`/`Max` 범위 안에 있다.
- 특성을 계산하는 순서를 바꿔도 각 특성의 최종값은 같다.
- `correlation_strength = 0`일 때 `patience_multiplier`/`stay_duration_multiplier`가 완전 독립 계산과 동일한 값을 낸다.

테스트 구현: `Source/FestaFiesta/Private/NPC/FF_DeterministicRandomTests.cpp` (Unreal Automation Test).

## 8-A. 그룹 단위로 공유되는 특성 (2026-08-04 추가)

`population_group_generation_spec.md` 1.6절 결정: 그룹(동반유형 `pair`/`group_3_plus`)으로 묶인 NPC는 `walk_speed_multiplier`와 `stay_duration_multiplier`만 개체별 독립을 깨고 **그룹 전체가 같은 값을 공유**한다 — 그룹 리더의 `StableAgentId` 대신 그룹의 `GroupId`를 시드 재료로 넣어서 `Resolve()`를 한 번 더 호출하고, 그 결과에서 이 두 필드만 가져다 쓴다. `Resolve()` 시그니처 자체는 바뀌지 않는다 — 호출부가 두 번 불러서 합치는 방식이다.

나머지 4개(`patience_multiplier`, `crowd_tolerance_multiplier`, `preference_strength_multiplier`, `reevaluate_interval_multiplier`)는 그룹이어도 계속 개체별 독립 — 취향·인내심까지 그룹으로 묶으면 개체 변주의 원래 목적(복제처럼 안 보이게 하기)이 무의미해진다.

## 9. 남은 의존성

1. ~~`live_stage_duration_minutes`(240분, 임시값) — `Live` 단계 실제 길이 확정 후 교체~~ → **2026-08-05 확정.** 실제 축제 프로그램 시간표(17:00~21:00) 기준 240분 그대로 유지. 강민서 님 `FF_FestivalStageSubsystem` 배속 구현이 끝나면 다른 시간 단위 필드들과 함께 최종 재점검.
2. 6종 변주 범위(5절)는 실측이 아니라 초안이다. 실제 시뮬레이션을 돌려본 뒤 편차가 너무 크거나 작으면 DataTable 값만 조정한다(코드 수정 불필요).
3. `correlation_strength_leisureliness`(0.3, 초안) — 두 특성이 부자연스럽게 조합되는지 플레이테스트로 확인 후 조정.
4. `remaining_stay`를 실제로 채우는 NPC 런타임 로직 — 목표 4(목적지 선택기 v0)에서 구현.
5. `Decision`/`RuntimeNoise` purpose는 훅만 마련했고 실제 사용처는 아직 없다 — 목표 4 구현 시 연결.
