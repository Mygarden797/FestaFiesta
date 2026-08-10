# Utility AI 목적지 선택 v0 — 필드·계수 스펙

- 작성일: 2026-08-03 (최종 갱신 2026-08-04)
- 상태: 아키타입 개수·주축 구조는 2026-08-04 확정. 표시 명칭은 가칭(UI 담당자 논의 필요), 계수 값 자체는 여전히 assumption.
- 계수 표: `data/derived/utility_coefficients_v0.csv` (원본) / `data/unreal/utility_coefficients_v0.csv` (UE DataTable용)

## 1. 이 문서가 정하는 것과 정하지 않는 것

정하는 것

- Utility 식의 항 구성과 각 항이 어떤 데이터 필드에서 오는지
- 각 항의 정규화 방법 (모두 0~1 또는 -1~1로 맞춘다)
- 계수의 기본값·범위·상태 표기
- 목적지를 다시 고르는 시점
- MVP 아키타입 개수·주축 구조 (2026-08-04 확정, 3절 참고)

정하지 않는 것

- 아키타입 표시 명칭의 최종본 (가칭 상태, UI 담당자 논의 필요)
- 계수의 "옳은" 값 — 실측이 없으므로 확정할 수 없다
- 축제장 부스 배치와 실제 거리 행렬 (레이아웃 확정 후)
- 활동→축제 부스 카테고리 매핑의 최종본 (실제 부스 유형 확정 필요)

## 2. Utility 식

```text
Utility(i, j, t) =
      weight_attraction  * attraction_norm(j, t)
    + lambda_affinity    * preference_norm(i, category(j))
    + weight_need        * need_urgency_norm(i, j)
    - weight_distance    * distance_norm(i, j)
    - weight_wait        * wait_norm(j, t)
    - weight_density     * density_norm(j, t)
```

- `i` = NPC 한 명, `j` = 목적지 한 곳, `t` = 시각
- 모든 `*_norm` 항은 0~1 (preference만 -1~1)
- 계수는 전부 `data/unreal/utility_coefficients_v0.csv` 에서 읽는다. 코드에 숫자를 박지 않는다.

### need 항 (2026-08-06 추가)

```text
need_urgency_norm(i, j) = max{ Need_i[n] * need_priority[n] : n 은 j 가 해소할 수 있는 Need 성분 }
```

`j`가 해소할 수 있는 Need 성분은 `FFF_NeedReliefFlags`(`NPC/Party/FF_NeedState.h`)가 담고, 판정은 `FFF_ZoneSystemBridge::GetNeedReliefFlags`가 한다(`FF_NeedState.h` 상단 매핑 표).

**합이 아니라 최대값이다.** 합을 쓰면 3가지를 해소하는 시설(예: Rest = Fatigue + Thirst)이 각 성분이 아무리 안 급해도 1가지만 해소하는 시설을 항상 이긴다. `FFF_NeedState::MakeAggregated`가 그룹 집계에 성분별 최대를 쓰는 것과 같은 논리다 — **가장 급한 것이 행동을 결정한다.**

#### 성분별 우선순위 `need_priority[n]` (2026-08-06 추가)

이게 없으면 화장실 Need 0.9와 배고픔 0.9가 동시에 급할 때 둘을 똑같이 취급해서, 행동이 Need가 아니라 시설의 거리·매력도로 결정됐다.

| 성분 | 가중치 | 비고 |
|---|---|---|
| `Restroom` | **1.00** | 가장 급함. **이 1.0이 `weight_need` 계산의 전제다** — 낮추면 `weight_need`도 다시 잡아야 한다 |
| `Hunger` | 0.85 | |
| `Thirst` | 0.75 | 기온 공공데이터를 주입하면 동적으로 올릴 후보 |
| `Fatigue` | 0.60 | 참을 수 있는 욕구 |
| `Stimulation` | 0.50 | 생리적 욕구가 아니라 가장 낮다. 단 **0으로 두면 Live 후반 무대 앞 집중(AT-016)이 사라진다** |

전부 0~1이라 `need_urgency_norm`도 0~1을 벗어나지 않는다 — 스케일은 `weight_need` 하나가 담당하고 여기서는 성분 간 순서만 정한다.

**하드 우선순위가 아니다.** 우선순위가 낮아도 값이 훨씬 크면 이긴다(배고픔 0.95 > 화장실 0.3). 하드 우선순위로 하면 확률적 선택이 깨져 "모든 NPC가 동시에 화장실로" 같은 쏠림이 생긴다. 두 성질 모두 테스트로 고정했다(`NeedPriorityBreaksTies`).

순서 자체는 생리적 급함의 상식적 순서이고 **실측 근거가 없다**(`draft_assumption`).

`weight_need` 기본값 **1.8**은 임의값이 아니라 다른 계수와의 상대 관계에서 나왔다.

- `weight_attraction`(1.0) + `lambda_affinity`(0.6) = **1.6**보다 크게 잡았다 → 한계에 가까운 Need(0.95)가 "가장 매력적이고 취향에도 맞는 대안"까지 이긴다. 급한 사람은 취향을 포기한다.
- 반대쪽 경계도 지켜진다: 가벼운 Need(0.3 → 0.54)는 취향을 뒤집지 못한다. 두 경계 모두 자동화 테스트로 고정했다(`UrgentNeedRaisesRelievingCandidate`).
- 실측 근거는 없다(`draft_assumption`). 상대 관계만 근거다.

**이 항이 재방문 문제를 같이 해결한다.** 이전에는 배고픔을 푼 직후에도 식음료 부스가 최고 효용이면 또 갔다 — `Choose()`가 방문 이력을 안 보기 때문이다. Need를 연결하면 `Hunger`가 0이 되어 그 부스의 효용이 저절로 내려간다. 별도 `recency_penalty` 노브를 만들지 않은 이유다(만들면 Need와 이중계산이 된다).

**2026-08-04 결정 (개체 변주 작업 중 확정)**: `preference_match` 항은 `preference_norm(i, category(j))` 하나로 확정했다. 원래 이 자리는 세그먼트(성·연령) 기반 `affinity_norm(segment(i), category(j))`(데이터랩 관광지 관측치)이었는데, 같은 날 아키타입 구조가 확정되면서 `preference_vector`(아키타입+동반유형 delta, 3.3절)와 같은 자리를 두고 정의가 겹쳤다. v0에서는 축제 부스 선택에 더 직접적인 `preference_vector`(Tour_Stat 활동 데이터 기반, 축제 카테고리 단위)를 채택하고, **`affinity_norm`은 v0 목적지 선택에서 사용하지 않는다** — 세그먼트 단위 보정이 필요해지면 그때 별도 항으로 추가한다.

```text
preference_norm(i, c) = clamp(preference_vector(i)[c] / preference_vector_scale_ref, -1, 1)
```

`preference_vector(i)[c]`는 3.3절 합성식(아키타입 delta + 동반유형 delta)에 개체 변주 `preference_strength_multiplier`(`individual_variation_spec.md` 5절)를 곱한 값이다. `preference_vector_scale_ref`(17.0)는 `affinity_scale_ref`와 같은 방식 — 산출물 관측 최대 절대값(16.80pp, `life_stage_teen`×`group_3_plus`×`ambience_photo_zone`)에서 정했다.

### no-choice 대안

부스만 후보로 두면 대기시간이 아무리 길어도 NPC는 어딘가로 반드시 간다. 대기시간을 인위적으로 클램프해서 막는 대신, **아무 부스도 고르지 않는 선택지를 후보 집합에 넣는다.**

```text
Utility(i, no_choice, t) = max(0, no_choice_utility - no_choice_utility_decay_per_round * ConsecutiveNoChoiceCount)
```

이것은 대기행렬 이론의 **balking**(줄을 보고 들어가지 않음)이다. 세 이탈 현상이 각각 다른 코드에 매핑되어 있으니 섞지 말 것:

| 현상 | 뜻 | 담당 |
|---|---|---|
| **balking** | 줄을 보고 안 들어감 | `no_choice` 후보 |
| **reneging** | 줄에 서 있다가 이탈 | `ShouldReevaluate` |
| **jockeying** | 다른 줄로 옮김 | 재평가 → 재선택 |

#### 감쇠가 필요한 이유 (2026-08-06 추가)

`no_choice_utility`가 고정값이면 **무한 배회 루프**가 생긴다: 혼잡해서 배회 → 다음 재평가에 같은 후보 집합 → 또 배회 → 영원히. NPC가 통로를 돌아다니며 밀집도만 올리고 아무것도 하지 않는다.

`no_choice_utility_decay_per_round`(기본 0.03)로 연속 횟수만큼 깎는다 = **인내심 소모**. "계속 못 고르면 결국 양보한다"는 뜻이다. 0 아래로는 내려가지 않게 클램프한다 — 음수가 되면 `no_choice`가 사실상 사라져 대기시간 클램프와 같은 상태가 되고, 이 대안을 둔 이유 자체가 무의미해진다.

#### no_choice 행동 3종 (2026-08-06 확정)

`FFF_UtilityDestinationSelector::ResolveNoChoiceAction`이 정한다. **softmax가 아니라 우선순위 캐스케이드**다 — 근거 없는 효용값 3개를 새로 만들지 않기 위한 선택이고, 확률화하면 "왜 퇴장했는지"를 설명할 수 없게 된다.

| 순위 | 행동 | 조건 | 지속 | 관찰 지표 |
|---|---|---|---|---|
| 1 | `Leave` | `remaining_stay <= 0` | 영구 | **이탈률** |
| 2 | `Leave` | `ConsecutiveNoChoiceCount >= no_choice_leave_patience_rounds`(기본 5) | 영구 | 이탈률 |
| 3 | `Relocate` | 현재 Zone 밀집 > 개인 밀집 임계값 | 인접 Zone 도착까지 | Zone 간 유량 |
| 4 | `Wander` | 그 외(기본값) | 재평가 주기 1회분 | 통로 밀집 기여 |

**`Rest`(휴식)를 의도적으로 넣지 않았다.** `Rest` 카테고리 시설(오아시스 존)이 이미 후보 집합에 들어 있으므로 여기에 또 두면 같은 행동이 두 경로로 발생하고, "Rest 시설이 부족해서 못 쉬었다"는 진단이 불가능해진다. 시설 없이 아무 데나 앉는 행동은 v1 후보다.

**`Leave`가 체류시간 게이트를 먼저 보는 것이 중요하다.** 혼잡만으로 퇴장하게 두면 "혼잡 → 대량 퇴장 → 혼잡 해소 → 남은 사람 만족"이 반복되어 총 방문객 수가 유지되지 않고, 7절의 **인원 보존 검증이 깨진다.** 퇴장의 주 원인은 체류시간 소진이고 혼잡은 보조 원인이다. `no_choice_leave_patience_rounds`를 0 이하로 두면 2번 조건을 끌 수 있다.

### 확률적 선택

최고 효용을 항상 고르면 모든 NPC가 같은 부스로 몰린다. `utility_noise_scale`(σ)로 확률화한다.

```text
P(i가 j를 고를 확률) = exp(U_ij / σ) / Σ_k exp(U_ik / σ)
```

`k`에는 `no_choice`도 포함한다.

## 3. 항별 데이터 연결

| 항 | 정규화 | 데이터 출처 | 상태 |
|---|---|---|---|
| `attraction_norm` | `base_attraction` 을 0~1로 | 부스 설정 (설계 필요) | 미정 |
| `preference_norm` | `preference_vector(i)[category(j)] / preference_vector_scale_ref`, [-1,1] 클램프 | `npc_archetypes_v0.csv` + `companion_type_modifier_v0.csv` + 개체 변주 | draft_assumption |
| `affinity_norm` (v0 미사용) | `centered_log_share_ratio / affinity_scale_ref`, [-1,1] 클램프 | `data/derived/affinity_prior_equal_site_v0.csv` | assumption, 보류 |
| `distance_norm` | `min(직선거리 / distance_ref_meters, 1)` — v0는 실제 보행거리 아닌 직선거리 근사, 레이아웃·내비메시 확정 후 교체 | NPC/목적지 월드 위치 | 근사값 |
| `wait_norm` | `min(estimated_wait / wait_ref_minutes, 1)` | 런타임 동적 상태 | 계산값 |
| `density_norm` | `min((현재인원/정원) / density_ref_ratio, 1)` | 런타임 동적 상태 | 계산값 |

`affinity_scale_ref = 1.27` 은 임의값이 아니라 산출물의 관측 최대 절대값(1.2687)에서 정했다. 따라서 클리핑이 발생하지 않는다.

**주의:** `affinity_norm` 은 실제 선택 효용계수가 아니다. 사하구 관광지에서 어떤 세그먼트가 상대적으로 많이/적게 관측되었는지를 나타내는 사전값이다. `lambda_affinity` 는 그것을 게임 효용으로 환산하는 밸런스 노브이며 추정된 적이 없다.

### 거리와 affinity 이중계산 주의

데이터랩 집중률에는 선호뿐 아니라 **접근성**이 이미 섞여 있다. 어떤 관광지에 특정 세그먼트가 적게 오는 이유가 취향이 아니라 멀거나 오르기 힘들어서일 수 있다. 이 상태에서 `distance_cost` 를 따로 빼면 거리 효과를 두 번 세게 된다.

- v0에서는 이 문제를 해결하지 않는다. 대신 `lambda_affinity` 와 `weight_distance` 를 **함께** 흔드는 민감도 분석으로 영향 범위를 본다.
- 축제장은 단일 부지이고 부스 간 거리 편차가 관광지 간 거리 편차보다 훨씬 작으므로, 실무적 영향은 제한적일 것으로 본다. 이 판단 자체가 `assumption` 이다.

## 4. 필수 필드

### NPC

2026-08-04 아키타입 구조 확정(생애주기 5종 주축 + 동반유형 오버레이 + 아동 종속 NPC) 이후 필드 출처가 다음과 같이 구체화되었다.

| 필드 | 의미 | 출처 |
|---|---|---|
| `npc_id` | 개체 식별자 | 런타임 생성 |
| `archetype_id` | 생애주기 아키타입 (5종 중 하나) | `data/derived/npc_archetypes_v0.csv` |
| `segment_key` | 성·연령 세그먼트 (인구 샘플링용) | `visitor_population_prior_v0.csv` 에서 샘플링, 아키타입 정체성 축으로는 쓰지 않음 |
| `companion_type` | 동반유형 (solo/pair/group_3_plus) | **개체 속성이 아니라 그룹 생성 규칙의 결과값** (`population_group_generation_spec.md` 1절, 2026-08-04 재정의). 개별 NPC에게 독립 샘플링하지 않는다. **(8/6)** 비율 출처는 `visitor_group_size_distribution_v0.csv`(실측)이고, 크기 3~6이 전부 `group_3_plus`로 뭉치므로 그룹을 묶을 때는 `FFF_TaggedVisitor::TargetGroupSize`를 본다 |
| `preference_vector` | 축제 카테고리별 선호 | `archetype_id` 의 `{category}_delta_vs_all_pp` + `companion_type` 의 동일 카테고리 delta를 더한 값. `lambda_affinity` 로 스케일 |
| `GroupId` / `bIsGroupLeader` | 그룹 식별자 / 리더 여부 | `population_group_generation_spec.md` 1.5절. 리더만 Utility 계산, 나머지는 리더의 `current_destination` 상속 |
| `bIsDependent` / `DependentOfNpcId` | 종속 아동 전용 노브 적용 대상 | `dependent_child_npc_spec.md`. **목적지 상속은 이제 위 `GroupId`/`bIsGroupLeader`가 담당** — 이 필드는 아동 전용 3개 노브(이동속도·주의집중·대기인내)를 누구 기준으로 적용할지에만 쓴다 |
| `current_location` | 현재 위치 | 런타임 |
| `arrival_time` | 도착 시각 | **그룹 공유 필드로 재정의됨** (`population_group_generation_spec.md` 1.6절) — 개인별 노브 아님. **(8/6)** 정확한 값은 Utility AI가 지정하지 않는다 — 시간대별 목표 유입 곡선(`visitor_arrival_curve_v0.csv`)만 데이터로 주고 실제 입장 시각은 crowdAI가 정한다(`visitor_arrival_curve_spec.md`) |
| `remaining_stay` | 남은 체류시간 | 체류 분포 (미확보 — 노브). `stay_duration_multiplier`는 그룹 멤버라면 `GroupId` 시드로 그룹 전체가 공유 (`individual_variation_spec.md` 8-A절) |
| `current_destination` | 현재 목적지 | 런타임 |

`preference_vector` 합성식(초안):

```text
preference_vector[category] =
    archetype[archetype_id][category + '_delta_vs_all_pp']
  + companion_modifier[companion_type][category + '_delta_vs_all_pp']
```

두 delta를 단순합산하는 것은 **독립가정**이다(나이×동반유형 교차표가 없어 상호작용항을 추정할 수 없음). `festival_category` 는 `festival_activity_category_mapping_v0.csv` 판단 매핑을 따르며 `not_applicable` 항목은 제외된다. `festival_engagement_baseline` 카테고리는 부스 선호가 아니라 해당 개체의 전반적 축제 참여 성향 기준값으로 별도 취급한다.

### Unreal 구조체 (2026-08-04 작성)

| 자료 | CSV | USTRUCT |
|---|---|---|
| 아키타입 | `data/unreal/npc_archetypes_v0.csv` | `FFF_ArchetypeRow` (`Source/FestaFiesta/Public/DataTables/FF_ArchetypeRow.h`) |
| 동반유형 오버레이 | `data/unreal/companion_type_modifier_v0.csv` | `FFF_CompanionModifierRow` (`Source/FestaFiesta/Public/DataTables/FF_CompanionModifierRow.h`) |
| Utility 계수 | `data/unreal/utility_coefficients_v0.csv` | `FFF_UtilityCoefficientRow` (`Source/FestaFiesta/Public/DataTables/FF_UtilityCoefficientRow.h`) |
| 그룹 크기 분포 (8/6 신설) | `data/unreal/visitor_group_size_distribution_v0.csv` | `FFF_GroupSizeDistributionRow` |
| 그룹 아동 구성 (8/6 신설) | `data/unreal/group_child_composition_v0.csv` | `FFF_GroupChildCompositionRow` |
| 그룹 구성 규칙 (8/6 신설) | `data/unreal/group_composition_rules_v0.csv` | `FFF_GroupCompositionRuleRow` |
| 유입 곡선 (8/6 신설) | `data/unreal/visitor_arrival_curve_v0.csv` | `FFF_ArrivalCurveRow` |

> **⚠️ `companion_type_modifier_v0.csv`의 delta 값에 대한 경고 (2026-08-06 발견, 미해결)**
>
> 이 표의 `{category}_delta_vs_all_pp`는 Tour_Stat 2018의 **`가구원수`**(1인/2인/3인 이상 **가구**)별 활동 응답률 차이다 — **여행 동반 인원이 아니다.** 원본 `.xls`의 가로분석변수를 다시 확인해 밝혀졌다(`build_cleaned_tourstat.mjs`의 caution에 이미 적혀 있었으나 `companion_type`이라는 이름이 이를 가렸다).
>
> 즉 지금 `preference_vector`에 더해지는 동반유형 delta는 "혼자 온 사람 vs 3명이 온 사람의 차이"가 아니라 "1인 가구 vs 3인 이상 가구의 차이"다. 그룹 **크기 비율**은 실측으로 교체했지만 이 **delta 값**은 아직 교체하지 않았다.
>
> 대체 후보: 2025 국민여행조사 통계편 표 14-2-x(동반자 유형) 또는 원자료 `A6B_1..12`. `preference_vector` 재계산이 필요해 별도 작업으로 남긴다 — `public_data_decision_log.md` 2026-08-06 항목 참조.

구조체 이름은 프로젝트 태그 `FF`(FestaFiesta)와 Unreal 구조체 필수 접두 `F`를 합쳐 `FFF_`로 시작한다. CSV는 `data/derived/`의 계산 결과를 손대지 않고 헤더만 구조체 프로퍼티명에 맞춰 재배열한 것이며, `Tools/data/build_unreal_datatables.mjs`로 재생성한다.

### 목적지

| 필드 | 의미 | 출처 |
|---|---|---|
| `destination_id` | 부스·공연장 식별자 | 부스 설정 |
| `category` | 카테고리 (affinity 조인 키) | 부스 설정 |
| `base_attraction` | 기본 매력도 | 부스 설정 |
| `capacity` | 동시 수용 인원 | 부스 설정 |
| `service_rate` | 시간당 처리 인원 | 부스 설정 |
| `position` | 좌표 | 축제장 레이아웃 |

### 동적 상태

`estimated_wait`, `current_density`, `is_open`, `event_state`, `queue_length`

## 5. 재평가 트리거

- 현재 목적지 도착 또는 활동 종료
- `estimated_wait > wait_threshold_minutes`
- `current_density > density_threshold_ratio`
- 목적지 폐쇄·개장 (`is_open` 변화)
- 라이브 이벤트 시작·종료
- `reevaluate_interval_seconds` 경과
- 경로 실패 또는 접근 불가

## 6. 균형 계산 시 주의 (v1 이후)

v0는 런타임 NPC가 각자 선택하는 것으로 충분하다. 이후 혼잡 균형을 계산할 때는 다음을 지킨다.

**흐름을 평균하고, 그 흐름에서 비용을 다시 계산한다.** 대기시간 자체를 평균하지 않는다.

```text
y(k)    = 현재 비용으로 계산한 새 선택 흐름
x(k+1)  = x(k) + (1/k) * (y(k) - x(k))
wait(k+1) = QueueModel(x(k+1), capacity, service_rate)
```

균형 반복 중에는 개별 NPC를 샘플링하지 말고 **기대흐름**을 누적한다.

```text
arrival_flow[j] += P(i가 j를 고를 확률)
```

매번 샘플링하면 무작위 노이즈 때문에 수렴 판정값이 줄지 않는다. 실제 샘플링은 게임 런타임에서만 한다.

## 7. 결과를 어떻게 말할 것인가

이 모델은 예측 모델이 아니다. 입력 가정에 조건부인 시나리오 생성기다.

- 쓸 수 있는 말: "가정 A에서는 3번 게이트가 병목이 되고, 계수를 ±50% 흔들어도 이 순위는 유지된다"
- 쓸 수 없는 말: "실제 축제에서 3번 게이트가 병목이 된다"
- 쓸 수 없는 말: "입력에 없던 병목이 나왔으므로 모델이 타당하다"

입력에 없던 결과가 나오는 것은 모델이 비선형 상호작용을 계산했다는 뜻일 뿐, 그 결과가 현실과 맞다는 뜻이 아니다. 잘못된 용량 단위, 과도한 대기 패널티, 잘못된 거리 행렬, 큐 함수 발산으로도 그럴듯한 병목은 만들어진다.

검증은 다음 네 가지로 한다.

1. **구조 검증** — 단위, 정규화 범위, 확률 합, 인원 보존
2. **수렴 검증** — 균형 반복이 실제로 수렴하는가
3. **민감도 검증** — 계수를 흔들 때 결론 순위가 유지되는가
4. **외부 범위 대조** — 총 방문객 규모가 알려진 축제 범위와 자릿수가 맞는가 (정확도 주장 아님)

## 8. 미확보 상태로 남은 것

| 항목 | 영향 | 현재 처리 |
|---|---|---|
| 도착 시각 분포 | 시간대별 혼잡 패턴 전체 | **(8/6 해소)** 인터페이스는 확정(`visitor_arrival_curve_v0.csv`), 곡선 **모양** 수치만 여전히 노브(`draft_assumption`, 시나리오 3종) |
| 체류시간 분포 | 동시 체류 인원 | 노브 |
| 관광지별 방문 규모 | affinity 축소추정 불가 | 동일가중 유지 |
| 방문 전환율 | 주민→방문객 변환 | 시나리오 2종으로 대체 |
| 축제장 레이아웃 | 거리 행렬 전체 | 설계 대기 |

## 9. 코드 구현 백로그

### 완료 (2026-08-06)

- ~~**Need를 Utility 점수식에 실제로 연결**~~ → **완료.** §2에 `weight_need * need_urgency_norm` 항 추가, `FFF_DestinationCandidate::NeedRelief` 필드로 입력받는다. 재방문 문제도 같이 해결됐다.
- ~~**`walk_speed_multiplier`/`stay_duration_multiplier`를 GroupId로 시드하는 실제 호출 코드**~~ → **완료.** `FFF_IndividualVariationResolver::ResolveWithGroupSharing`. `Resolve()` 시그니처는 안 바뀌었고 내부에서 두 번 불러 합친다. `GroupId == INDEX_NONE`이면 solo로 취급해 `Resolve()` 한 번과 완전히 같은 결과를 낸다.
- ~~**`no_choice` 행동 미분화**~~ → **완료.** `EFF_NoChoiceAction` 3종 + 감쇠 계수 2개(§2 참고).
- ~~**Booth 네이밍 컨벤션 위반이 조용히 넘어감**~~ → **완료.** `FFF_ZoneSystemBridge`가 시설당 1회 `Warning` 로그를 낸다.
- ~~**Need 집계 지표가 UI에 노출되지 않음**~~ → **읽는 쪽 계약 완료.** `UFF_NpcNeedMetricsSubsystem`(Getter + 델리게이트 쌍). 채우는 쪽은 런타임 층 대기 — 아래 참고.

### 미착수

- **NPC 런타임 층(L3)이 없다** — 가장 큰 구조적 공백. 위 함수들을 **누가 언제 호출하는지**가 정해지지 않았다. `UFF_NpcNeedMetricsSubsystem::AddSample`을 부르는 코드도, `ResolveWithGroupSharing`을 부르는 코드도 없다.
- **`FFF_ZoneSystemBridge::BuildCandidates`가 스텁** — 목적지 선택이 실제로 돌지 않는 직접 원인. Zone 인접 그래프(`NeighborZoneIds`)는 이미 노출돼 있다.
- **`companion_type_modifier_v0.csv`의 preference delta가 `가구원수` 기반인 문제** — 그룹 크기 *비율*은 실측으로 교체했지만 이 *delta 값* 자체는 아직 그대로다(§6 경고 박스). 대체 후보는 2025 국민여행조사 통계편 표 14-2-x(동반자 유형) 또는 원자료 `A6B_1..12`.
- **후보 집합 규칙이 계수로 노출되지 않았다** — `BuildCandidates`가 "현재 Zone + 인접 Zone + 전역 인기 상위 K"로 줄이는데, 이 "전역 인기"가 **부익부 피드백**을 만든다(인기 있으면 후보에 더 자주 들어가서 더 인기 있어진다). K와 "인기"의 정의를 계수로 빼서 민감도 분석 대상에 넣어야 한다. 안 그러면 병목이 실제 선호 때문인지 후보 필터 때문인지 구분할 수 없다.

### 합의 필요 (다른 파트와 걸림)

| 항목 | 왜 막혀 있는지 | 상대 |
|---|---|---|
| `EstimatedWait`에 "향하는 중(en-route)" 인원을 넣는가 | 안 넣으면 같은 틱에 N명이 같은 부스를 고른다. softmax가 완화하지만 못 막는다. **v0에서 가장 먼저 터질 구조 문제** | 장원재 (Queue) |
| Queue가 Party를 쪼개는가 | `TryJoinZoneFacility`가 Party 단위 진입점인데 정원 초과 시 일부만 입장하는지 미정 | 장원재 (Queue) |
| 모든 시간 필드의 기준 = "Live 경과분"인가 | `arrival_time`·`remaining_stay`·`SecondsSinceLastEvaluation`이 기준 시각을 요구한다. Setup/PromotionPreview에서 시계가 안 흐르면 어떻게 되는지 미정 | 강민서 (Stage) |
| ~~리더가 `Leave`면 그룹 전체가 퇴장하는가~~ | **8/6 결정: 전원 함께 퇴장.** `no_choice` 행동 3종 전부 리더가 결정하고 그룹원이 상속한다 | 결정 완료 |
| ~~`ActivityGame` 토큰을 쓰는 부스가 있는가~~ | **8/6 해소.** 팀 네이밍 규칙 페이지에 `Booth_ActivityGame_RingToss` 예시가 명시됐다 — `activity_game` 축은 죽은 값이 아니다 | 해소 |
| 인원 보존 카운터의 소유자 | 총 방문객 = 장내 + 퇴장 + 미입장. §7 검증 항목인데 그 상태를 들고 있는 주체가 없다. 경제 파트와도 직결(매출 = 장내 인원 × 소비) | 김시리 (경제) |
| 동시 Need 충돌 우선순위 | `MakeAggregated`가 성분별 최대를 취하지만 Hunger 0.9와 Restroom 0.9가 동시일 때 어디로 갈지는 need 항의 성분별 가중치가 없어 미정 | 김시리 |
