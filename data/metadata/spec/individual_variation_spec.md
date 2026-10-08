# 고정 Seed 기반 개체 변주 명세 (v0)

- 작성일: 2026-08-04 (**구현 현황 갱신 2026-09-03**)
- 상태: multiplier 7종 + 공유 성격축 1종(총 8행)의 범위는 `draft_assumption`, 구조(해시·분포·상관 방식)는 확정

> **8개 knob를 읽어 multiplier 7종과 LeisurelinessAxis를 계산한다. 현재 Crowd는 속도·체류 배수 2종을 직접 소비한다.**
> 나머지 multiplier 5종(인내심·혼잡내성·선호강도·재평가주기·신규성)은 결정 컨텍스트에 전달되지 않는다. 취향 강도와 신규성 배수는 Decision에서 1.0을 쓴다. `ResolvePersonalThresholds`와 `ShouldReevaluate`는 프로덕션 C++ 호출이 없다.
> （2026-10-09 코드 기준: Crowd 스폰이 `ResolvePersonalThresholds` 를 부르고, 주기 재평가·줄 인내 이탈이 `ShouldReevaluate` 를 부른다. 취향 강도·신규성 배수도 결정 컨텍스트로 전달된다. 시설로 걸어가는 중이면 재평가 주기 ×3(`EnRouteReevaluateIntervalScale`).）
> （LeisurelinessAxis는 Resolver 안에서 상관에 쓰이므로 “미사용 5종”에 넣지 않는다. 판단도 전원이 같지는 않다: 아키타입·일행 크기·Need·위치·방문 이력·결정 시드가 다르다.）
> （Zone density 비용은 인구 보고 미연결, `Relocate`는 현재 밀도 0/개인 임계 1 고정이라는 별도 한계가 있다. 개인 임계 전달만으로 Zone 보고가 채워지지는 않는다.）

- 목적: 아키타입(`FFF_ArchetypeRow`)·동반유형(`FFF_CompanionModifierRow`)은 집단 평균이다. 같은 Row를 쓰는 NPC들이 복제처럼 보이지 않도록 개체별로 조금씩 다른 행동양식(보행속도·인내심·혼잡내성·선호강도·체류시간·재평가주기·신규성선호)을 주면서도, 같은 Seed·같은 Agent ID에는 항상 같은 값이 나오게 해서 디버깅과 실험 재현이 가능하게 한다.

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
            MakeStableNameHash(RandomPurpose)),      ← FName은 내용 해시(아래 경고 참조)
        MakeStableNameHash(TraitKey)),
    GetTypeHash(SpawnGeneration))
```

> 🔴 **`GetTypeHash(FName)`을 쓰면 안 된다 (2026-08-10 버그 수정).**
> 엔진의 `GetTypeHash(FName)`은 `FName::GetComparisonIndex()`를 해시하고, 그 인덱스는 `NameTypes.h` 주석에 **"process specific unstable int"**로 명시돼 있다 — 전역 이름 테이블 **등록 순서**에 따라 값이 달라진다. **에셋 로딩 순서가 조금 달라지거나 프로젝트에 새 이름이 하나 추가되기만 해도 기존 NPC 전원의 시드가 바뀐다.**
>
> 실제로 이 문서가 아래 "금지 입력"에 *Actor 스폰 순서*를 적어놓고 정작 이름 등록 순서에 의존하고 있었다. 증상은 `VisitorDistribution.Deterministic` 테스트가 **같은 바이너리에서 실행마다 다른 결과**를 낸 것이었다.
>
> `MakeStableNameHash`는 FName의 **문자열 내용**을 FNV-1a로 해시한다 — 힙 할당 없고, 동일 구현에서는 이름 등록 순서와 무관하다（다른 엔진 버전·플랫폼까지 동일하다는 검증 범위는 별도다）. FName 비교가 대소문자를 무시하므로 소문자화 후 해시한다(안 하면 FName으로는 같은 두 이름이 다른 NPC를 만든다). **TraitKey는 ASCII snake_case로만 쓴다.**
>
> 회귀 방지: `DeterministicRandom.NameHashIsContentBased` / `SeedsAreStableAcrossProcesses`가 **골든 값 상수**로 고정한다. 그 테스트가 깨지면 상수를 맞추기 전에 "모든 NPC의 특성값이 바뀐다"는 영향을 먼저 확인할 것.

**목적지 선택 시드는 따로다** — `MakeDecisionSeed(GlobalSimulationSeed, StableAgentId, ChoiceRoundIndex)`. 회차를 정수로 접는다. 예전에는 `FName(*FString::Printf(TEXT("destination_choice_round_%d"), Round))`로 문자열에 접어넣었는데, 그러면 **매 목적지 선택마다** FString 할당과 전역 이름 테이블 조회가 발생했다(8만 명 × 회차 수). FName은 해제되지도 않는다.

- `GlobalSimulationSeed` (int32): 시뮬레이션 실험 전체 Seed. 같은 시나리오 재현에 사용.
- `StableAgentId` (int32): 실행 순서·메모리 주소가 아니라 저장·복원 가능한 NPC 고유 ID.
- `RandomPurpose` (FName): 난수 목적. **2026-09-15 실사용 5종** — `Generation`(개체 변주·방문객 분포) / `Decision`(목적지 softmax·배회 좌표) / `NeedInitial`(초기 Need 분포) / `Gate`(입장 게이트 선택, `entry_gate_choice`, 이정원 분할 스폰) / `SnsContent`(SNS 피드 극성·화제·문장·표시 핸들·간격 선택, `FFF_SnsFeedGenerator`, `utility_ai_todo.md` §10). `RuntimeNoise`는 3절에 이름만 있고 아직 사용처가 없다.
  - 이 목록은 **닫힌 열거가 아니다.** `MakeTraitSeed`는 도메인을 모르는 순수 함수라 누구든 새 purpose를 넣어 재사용할 수 있고, 실제로 `Gate`가 그렇게 추가됐다. 새 purpose를 쓰면 이 줄에 적을 것 — 안 적으면 "어떤 난수가 어디서 갈라지는지"를 추적할 방법이 없다.
- `TraitKey` (FName): `walk_speed_multiplier` 등 특성별 고정 문자열.
- `SpawnGeneration` (int32): 같은 ID를 재사용할 때만 쓰는 세대 번호. 필요 없으면 0.
- 금지 입력: 현재 시각, 포인터 주소, Actor 스폰 순서, 프레임 번호 — 실행마다 달라질 수 있는 값은 절대 넣지 않는다.

`TraitSeed`를 `FRandomStream`에 넣어 `GetFraction()`을 한 번 호출하면 `[0,1)` 균등난수 하나를 얻는다(`UniformFraction`). 특성마다 `TraitKey`가 다르므로 각 특성은 독립된 난수 스트림을 갖는다 — 하나의 스트림을 순서대로 소비하지 않으므로, 나중에 특성을 추가해도 기존 특성값이 바뀌지 않는다.

구현: `Source/FestaFiesta/Public/NPC/IndividualVariation/FF_DeterministicRandom.h`의 `FFF_DeterministicRandom::MakeTraitSeed` / `MakeUniformFraction`.

## 3. 난수 3종 분리

`RandomPurpose`를 해시 재료에 포함시키는 것만으로 분리한다. 별도 클래스나 자료구조가 필요 없다 — 같은 `TraitKey`라도 `RandomPurpose`가 다르면 완전히 다른 `TraitSeed`가 나온다.

| Purpose | 용도 | v0 사용 범위 |
|---|---|---|
| `Generation` | NPC 생성 시 한 번 정해지고 유지되는 성격·신체 특성 | 이 문서의 7종 multiplier + `leisureliness_axis` 전부 |
| `Decision` | softmax 목적지 선택 샘플링. 선택 회차 인덱스를 `SpawnGeneration` 또는 별도 인자로 Seed에 포함해 재현 | `Choose`의 `MakeDecisionSeed`에서 사용 |
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

## 5. v0 변주 7종 — 대상 필드와 범위

각 multiplier가 **정확히 어떤 기존 필드에 곱해지는지**를 여기서 확정한다. 이 매핑이 없으면 숫자를 정해도 의미가 없다.

| TraitKey | 곱해지는 대상 | Min | Mode | Max | 상태 |
|---|---|---|---|---|---|
| `walk_speed_multiplier` | Crowd `FUnitData`의 기본 속도 150cm/s（Character MovementComponent 경로가 아님）. 2026-10-09 변경: 손님은 여기에 `GuestWalkSpeedScale` 1.6 을 먼저 곱한다（150 × 1.6 = 2.4m/s） | 0.85 | 1.00 | 1.15 | draft_assumption |
| `patience_multiplier` | `wait_threshold_minutes` (재평가 트리거, 기본 15분) | 0.70 | 1.00 | 1.30 | draft_assumption |
| `crowd_tolerance_multiplier` | `density_threshold_ratio` (재평가 트리거, 기본 0.85) | 0.70 | 1.00 | 1.30 | draft_assumption |
| `preference_strength_multiplier` | `preference_vector` (아키타입+동반유형 delta 합산값 — `lambda_affinity` 자체는 건드리지 않음) | 0.60 | 1.00 | 1.40 | draft_assumption |
| `stay_duration_multiplier` | 체류시간 기준값 (6절) | 0.70 | 0.90 | 1.60 | draft_assumption |
| `reevaluate_interval_multiplier` | `reevaluate_interval_seconds` (기본 120초) | 0.80 | 1.00 | 1.20 | draft_assumption |
| `novelty_seeking_multiplier` | Utility 식의 `revisit_penalty` 항 (2026-08-10 추가) | 0.40 | 1.00 | 1.60 | draft_assumption |
| `leisureliness_axis` | 직접 곱하는 대상 없음 — 7절 상관관계용 공유 성격축 | 0.00 | 0.50 | 1.00 | draft_assumption |

**`novelty_seeking_multiplier`만 성질이 다르다 — 페널티에 곱한다.** 다른 multiplier는 값이 클수록 그 특성이 강해지는데, 이건 **값이 클수록 이미 가본 곳의 효용이 더 내려간다**. 1.6이면 가본 곳을 강하게 피하고, 0.4면 익숙한 곳을 다시 가는 데 거리낌이 없고, 0이면 방문 이력을 아예 신경 쓰지 않는다.

**폭(0.4~1.6)이 다른 knob(0.85~1.15)보다 넓은 것은 의도다.** 이 축은 "화면에서 보이는 행동 다양성"을 만드는 것이 목적이라 개체차가 커야 의미가 있다. 이동속도처럼 미세한 차이로는 "저 사람은 여기저기 돌아다니고 저 사람은 한 곳에 머문다"가 관찰되지 않는다.

`leisureliness_axis`와 상관을 걸지 않았다 — **"느긋한 사람이 새로운 걸 더 찾는다"는 근거가 없다.** 7절의 독립 목록에 포함된다.

**중요한 구분**: `weight_wait`, `weight_density`, `lambda_affinity`는 "게임 전체가 대기·혼잡·취향을 얼마나 중요하게 보는가"를 정하는 전역 밸런스 노브다. 위 multiplier들은 "이 개인이 그 전역 기준에서 얼마나 벗어나는가"를 정하는 개인차이며, 전역 가중치 자체를 개인화하지 않는다. 두 개념을 같은 계수에 합치면 밸런스 문제와 개체차 문제를 구분해 말할 수 없게 된다.

CSV 원본: `data/derived/individual_variation_knobs_v0.csv` (8행). Unreal DataTable: `data/unreal/individual_variation_knobs_v0.csv` → `FFF_IndividualVariationRow`.

## 6. 체류시간 앵커 — "Live 단계 길이의 몇 %"

（2026-09-03 코드 대조: 아래 240분은 초기 설계 앵커다. 실제 유입 진행도는 Stage의 날짜별 Live 길이를 읽지만 체류 초기 예산은 Crowd 임시 240을 사용한다. 예약은 Stage Live 축제분을 읽고, Need·방문 이력은 Crowd GetLiveMinutes를 사용한다. 배속 수신은 연결됐으나 모든 시간 경로가 하나로 통합된 것은 아니다.）

체류시간 분포는 공공데이터로 확보할 수 없는 값이라 기획 판단이 필요하다(`utility_ai_spec.md` 8절 "체류시간 분포 — 노브"). 다만 절대 분(分) 숫자를 감으로 정하면 축제 운영시간이 바뀔 때마다 재조정해야 하므로, 게임 안의 다른 구조적 단위에 앵커를 건다.

```text
BaseStayDurationMinutes = live_stage_duration_minutes * stay_duration_share_of_live_stage_mean
FinalStayDurationMinutes(NPC) = BaseStayDurationMinutes * stay_duration_multiplier(NPC)
```

- `live_stage_duration_minutes`: `EFF_FestivalStage::Live` 단계의 실제 길이. **단위는 게임(시뮬레이션) 시간 분이다 — 배속(x1/x2/x4)이나 플레이어 실제 체감 시간과 무관하다.** 값의 출처는 실제 축제의 실제 운영시간(현실 시간)을 그대로 게임 시계에 옮긴 것이다(2026-08-05, 강민서 작업 중인 실제 프로그램 시간표 기준: 17:00~21:00, 4시간 = 240분). 4시간(240분)이라는 숫자 자체는 바뀌지 않았다 — 처음엔 근거 없는 임시값이었는데 실제 시간표와 우연히 일치했다. **배속 수신과 Stage 시간 API는 구현됐다. 다만 Crowd 초기 체류 예산은 임시 240분 기준이므로 날짜별 통합 점검은 남아 있다.** `status: confirmed`(실제 운영시간 기준값 확정) — （실행 시 Stage 길이와 Crowd 예산 필드를 각각 확인해야 함.）
- `stay_duration_share_of_live_stage_mean`: 평균 NPC가 `Live` 단계 길이 중 머무는 비율의 기준값(초기값 0.5). `status: assumption`.
- 두 값 모두 `data/derived/utility_coefficients_v0.csv`에 `individual_variation_anchor` term으로 추가했다 (전역 스칼라 노브라 기존 Utility 계수 표와 같은 자리에 둔다 — `total_visitor_count` 등 Utility 식의 항이 아닌 전역 노브도 이미 이 표에 있는 선례를 따른다).

`remaining_stay`는 Crowd의 `PartyRemainingStayMinutes`에 저장되고 결정 컨텍스트로 전달된다. 현재 초기 예산은 `Temp_LiveStageMinutes(240) × stay_duration_share_of_live_stage_mean(0.5) × Traits.StayDurationMultiplier`이다（CSV의 `live_stage_duration_minutes`를 직접 소비하지 않음. 날짜별 Stage Live 길이에 맞춘 예산 통합은 미완료）.

## 7. 특성 간 상관관계 — 공유 성격축 1개

（2026-09-03 코드 대조: 현재 Crowd는 CSV `correlation_strength_leisureliness` 대신 SimulationSettings.VariationCorrelationStrength(기본 0.3)를 Resolver에 전달한다. 그룹은 개인별 Patience를 유지한 채 StayDuration만 GroupId 기반 결과로 덮으므로 개인 Patience와 공유 StayDuration의 상관은 아래 solo 식과 동일하다고 단정할 수 없다.）

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
- `walk_speed_multiplier`, `crowd_tolerance_multiplier`, `preference_strength_multiplier`, `reevaluate_interval_multiplier`, `novelty_seeking_multiplier`는 v0에서 상관 없이 완전 독립으로 둔다. 이는 `independence_assumption`이며, 실제로는 상호작용이 있을 수 있으나 관측 자료가 없다.

구현: `FFF_IndividualVariationResolver::Resolve` (`Source/FestaFiesta/Public/NPC/IndividualVariation/FF_DeterministicRandom.h`).

## 8. 완료 조건과 테스트

- 같은 `GlobalSimulationSeed + StableAgentId`는 몇 번을 다시 생성해도 모든 변주값이 동일하다.
- **실행(프로세스)을 바꿔도 같은 값이 나온다** — 2절의 FName 내용 해시가 이걸 보장한다. 전체 테스트 스위트를 3회 연속 돌려 같은 결과를 확인했다(2026-08-10).
- `GlobalSimulationSeed` 또는 `StableAgentId`를 바꾸면 변주값이 달라진다.
- 새 TraitKey를 추가해도 기존 TraitKey의 결과가 바뀌지 않는다(독립 스트림이므로 구조적으로 보장됨).
- 모든 값은 정의된 `Min`/`Max` 범위 안에 있다.
- 특성을 계산하는 순서를 바꿔도 각 특성의 최종값은 같다.
- `correlation_strength = 0`일 때 `patience_multiplier`/`stay_duration_multiplier`가 완전 독립 계산과 동일한 값을 낸다.

테스트 구현: `Source/FestaFiesta/Private/NPC/IndividualVariation/FF_DeterministicRandomTests.cpp` (Unreal Automation Test).

## 8-A. 그룹 단위로 공유되는 특성 (2026-08-04 추가)

`population_group_generation_spec.md` 1.6절 결정: 그룹(동반유형 `pair`/`group_3_plus`)으로 묶인 NPC는 `walk_speed_multiplier`와 `stay_duration_multiplier`만 개체별 독립을 깨고 **그룹 전체가 같은 값을 공유**한다 — 그룹 리더의 `StableAgentId` 대신 그룹의 `GroupId`를 시드 재료로 넣어서 `Resolve()`를 한 번 더 호출하고, 그 결과에서 이 두 필드만 가져다 쓴다. `Resolve()` 시그니처 자체는 바뀌지 않는다 — 호출부가 두 번 불러서 합치는 방식이다.

**2026-08-06 구현 완료** — `FFF_IndividualVariationResolver::ResolveWithGroupSharing(VariationTable, Seed, StableAgentId, GroupId, SpawnGeneration, CorrelationStrength)`. `GroupId == INDEX_NONE`이면 solo로 취급해 `Resolve()` 한 번과 완전히 같은 결과를 낸다(테스트로 고정).

> ✅ **2026-08-19 — 개체 변주가 실제로 걸린다.** crowdAI가 스폰 경로에서 결선했다.
> `Unit.Speed = BaseSpeed × Traits.WalkSpeedMultiplier × MemberBoost` 로 보행 속도가 개체마다 다르고,
> `ArchetypeID`도 개체군 분포로 배정된다(표가 없을 때만 폴백 + Error 로그).

나머지 5개(`patience_multiplier`, `crowd_tolerance_multiplier`, `preference_strength_multiplier`, `reevaluate_interval_multiplier`, `novelty_seeking_multiplier`)는 그룹이어도 계속 개체별 독립 — 취향·인내심까지 그룹으로 묶으면 개체 변주의 원래 목적(복제처럼 안 보이게 하기)이 무의미해진다.

## 9. 남은 의존성

1. ~~`live_stage_duration_minutes`(240분, 임시값) — `Live` 단계 실제 길이 확정 후 교체~~ → **2026-08-05 확정.** 실제 축제 프로그램 시간표(17:00~21:00) 기준 240분 그대로 유지. Stage 배속 수신은 연결됐으나 다른 시간 단위·초기 체류 예산과 최종 재점검이 필요하다.
2. 7종 변주 범위(5절)는 실측이 아니라 초안이다. 실제 시뮬레이션을 돌려본 뒤 편차가 너무 크거나 작으면 DataTable 값만 조정한다(코드 수정 불필요).
3. `correlation_strength_leisureliness`(0.3, 초안) — 두 특성이 부자연스럽게 조합되는지 플레이테스트로 확인 후 조정.
4. ~~`remaining_stay`를 실제로 채우는 NPC 런타임 로직~~ → **2026-08-19 결선.** crowdAI가
   `FFF_NeedRules::ConsumeStay`를 돌리고 `RemainingStayMinutes`를 결정 컨텍스트로 넘긴다.
5. ~~`Decision` purpose는 훅만 마련했고 실제 사용처가 없다~~ → **2026-08-06 연결 완료.** `FFF_UtilityDestinationSelector::Choose`가 softmax 샘플링 시드에 `RandomPurpose = Decision`을 쓴다. 현재는 `MakeDecisionSeed(GlobalSimulationSeed, StableAgentId, ChoiceRoundIndex)`에 회차를 정수로 넣는다（문자열 `destination_choice_round_%d` 방식은 2026-08-10 제거된 과거 구현）.
6. `RuntimeNoise` purpose는 여전히 사용처가 없다 — 애니메이션·시각적 흔들림 담당이 정해지면 연결한다. 훅만 있어도 나중에 붙을 때 기존 특성값이 안 바뀌므로 급하지 않다(2절 `TraitKey`별 독립 스트림).

## 코드 대조 근거 (2026-09-03)

- [FF_DeterministicRandom.cpp](../../../Source/FestaFiesta/Private/NPC/IndividualVariation/FF_DeterministicRandom.cpp)
- [FF_SimulationSettings.h](../../../Source/FestaFiesta/Public/CrowdSystem/Types/FF_SimulationSettings.h)
- [FF_CrowdSimulationSubsystem.cpp](../../../Source/FestaFiesta/Private/CrowdSystem/Systems/FF_CrowdSimulationSubsystem.cpp)

（현재 작업 트리 기준. 데이터/계수 수는 저장소 CSV, 설정은 C++ 기본값과 Config를 구분해 기술했다. 설계 제안·과거 실측은 코드만으로 재검증할 수 없어 해당 설명을 보존했다.）
