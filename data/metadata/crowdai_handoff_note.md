# crowdAI 담당자에게 전달할 내용 — 아키타입·동반유형 연동

- 대상: Crowd 시스템(`Source/FestaFiesta/*/CrowdSystem/`, PR #8로 Dev에 병합됨) 담당(이정원)
- 배경: `FUnitData`(`CrowdSystem/Types/FF_FestivalCrowdTypes.h`) 유닛이 이 문서의 아키타입 NPC와 같은 존재다.

## 한눈에 보기

**바뀐 것(8/6)**: 그룹핑(누구를 누구랑 묶을지)과 입장 타이밍 결정권이 Utility AI에서 crowdAI로 넘어갔다. Utility AI는 이제 **개별 방문객 태그 분포**와 **그룹 구성 규칙(데이터)**만 만든다.

**제가 한 일**
- 개체 태그 분포 생성 함수 완성 — `FFF_VisitorDistributionBuilder` (§3)
- 그룹 크기·아동 구성 실측 데이터 확보, DataTable 3장으로 제공 (§2)
- 입장 타이밍용 유입 곡선 확정 — `FFF_ArrivalCurveRow` (§4)

**정원 님이 하실 일**
- [ ] `FUnitData`에 필드 5개 추가 (§1)
- [ ] 태그 기반 그룹핑 구현 (§2, §3)
- [ ] `FCrowdGroupTypes::GroupID` 그대로 재사용 + 초기화 한 줄 추가 (§1)
- [ ] 유입 곡선을 읽어 입장 타이밍 구현 (§4)
- [ ] 셀 격자 밀도 로그 (§5)
- [ ] 판단·이동·렌더링 3개 층 분리 실측 (§5)

**먼저 확인해주세요**: 8/4에 드린 그룹핑 규칙 중 일부가 실측으로 대체돼 폐기됐다. 이미 구현하셨다면 §2 "폐기된 것"을 보고 걷어내달라.

**지켜야 할 것 2가지**
1. 그룹은 항상 통째로 입장한다 (일부만 먼저 들어오면 안 됨) — **어떻게 구현할지는 자유**, 저희가 요구하는 건 "쪼개지지 않는다"는 결과뿐
2. 배정·타이밍은 자유지만 항상 **결정적**이어야 한다 — 같은 Seed면 항상 같은 결과 (`FMath::Rand()` 금지)

**같이 정해야 할 것**: §6

---

## 1. `FUnitData`에 필요한 필드

| 필드 | 타입 | 용도 |
|---|---|---|
| `ArchetypeId` | FName | 5종 아키타입 중 하나. preference_vector 계산과 그룹핑 둘 다에 필요 |
| `CompanionSizeTag` | FName (`solo`/`pair`/`group_3_plus`) | 목표 그룹 크기 분류 (거친 태그 — §2 참조) |
| `bIsDependentChildSlot` | bool | 종속 아동 여부. 자체 목적지 계산 없이 성인에게 상속 |
| `GroupID` (기존 `FCrowdGroupTypes`) | int32 | 그룹 식별자. **crowdAI가 그룹핑하며 직접 부여** — 새 필드 추가 안 함, `FF_FestivalCrowdTypes.h:29`에 초기화(`= INDEX_NONE;`)만 추가해달라 |
| `bIsGroupLeader` (또는 `LeaderAgentId`) | bool/int32 | 목적지는 리더만 계산, 나머지는 상속. **crowdAI가 그룹핑하며 같이 지정** |
| `StableAgentId` | int32 | 개체 안정 ID. **crowdAI가 계산** — 공식은 [부록 C](#부록-c-fff_visitordistributionbuilder-상세) 참조 |

색상·라벨은 crowdAI 쪽 디버그 코드에만 두고 프로덕션 DataTable에는 넣지 않는다. `ArchetypeLabelDraft`(한글 표시명)는 아직 가칭이라 플레이어 노출 UI에는 쓰지 말 것.

## 2. 그룹 만들기

규칙은 마크다운이 아니라 **DataTable 3장**으로 제공한다 — **CSV 파일은 저희 쪽 원본·재생성용일 뿐이고, 정원 님은 아래 에디터 DataTable 애셋만 참조하면 된다**(이미 `Content/FestaFiesta/Data/`에 만들어뒀다. 전체 설계: `population_group_generation_spec.md`).

| DataTable 애셋 (`Content/FestaFiesta/Data/`) | 구조체 | 무엇 |
|---|---|---|
| `DT_VisitorGroupSizeDistribution` | `FFF_GroupSizeDistributionRow` | 그룹 크기 분포 (1~6인) |
| `DT_GroupChildComposition` | `FFF_GroupChildCompositionRow` | 그룹 크기별 아동 수 분포 |
| `DT_GroupCompositionRules` | `FFF_GroupCompositionRuleRow` | 나머지 규칙(리더 수·상속 범위·공유 특성 등) |

**그룹 하나 만드는 순서:**

```text
1. DT_VisitorGroupSizeDistribution 에서 GroupSize 를 뽑는다
   (ScopeId="tourism_daytrip" 행만 사용 — 그룹 분포가 어느 모집단 기준인지 구분하는 값,
    다른 값과 섞으면 비율 합계가 깨진다)
2. DT_GroupChildComposition 에서 그 GroupSize 의 ChildCount 를 뽑는다
3. ChildCount 명에게 bIsDependentChildSlot=true, 나머지는 성인
4. 성인 중 1명을 bIsGroupLeader 로 지정 (아동은 리더 불가)
5. 전원에게 같은 GroupID, StableAgentId 부여 (부록 C)
```

**사실 위 1~3단계는 저희 함수(`FFF_VisitorDistributionBuilder::Build`, §3)가 이미 대신 해준다** — 개체마다 `TargetGroupSize`/`TargetGroupChildCount`/`bIsDependentChildSlot`이 미리 채워져서 나온다. crowdAI가 실제로 하는 건 4~5단계(리더 지정, GroupID·StableAgentId 부여)와 "누구를 누구와 묶을지"뿐이다.

**중요한 구분**: `CompanionSizeTag`는 `solo`/`pair`/`group_3_plus` 3종뿐이라 3~6인이 전부 뭉친다. **그룹을 묶을 때는 정확한 인원수(1~6)를 담은 `TargetGroupSize`(§3)를 봐야 한다.**

**폐기된 것** — 8/4에 드렸던 아래 규칙은 전부 실측 없는 가정이었고 2025 국민여행조사 실측으로 대체됐다. 이미 구현하셨다면 걷어내달라:
- ~~`group_3_plus`는 3명 70% / 4명 30%~~ → 상한 **6인**, 실측 3명 37% / 4명 44% / 5명 9% / 6명 10%
- ~~`pair` couple_or_friends 80%/parent_and_child 20%~~, ~~`group_3_plus` family 50%/friends 50%~~, ~~아동 부착확률 0.4·평균 1.5~~ → **그룹 유형을 뽑는 단계 자체가 없어졌다.** `DT_GroupChildComposition` 한 장으로 대체

DataTable 3장의 필드별 상세와 규칙 11개 실제 값은 [부록 B](#부록-b-datatable-필드-상세) 참조. `CohesionRadiusMeters`는 이동 스택 확정 후 정한다.

## 3. 개체 태그 분포 — `FFF_VisitorDistributionBuilder`

**비유**: 여행사 예약팀(저희)이 손님 명단을 만들어 넘긴다. 명단의 각 사람에게는 "4인 가족 중 한 명, 그중 2명은 어린이, 성인 취향은 30대형" 같은 메모(태그)가 이미 붙어 있고, "몇 인 가족이 몇 팀 있는지" 요약표도 같이 준다. 하지만 실제로 "이 4명은 한 팀, 이분이 조장"이라고 배차하는 건 현지 가이드(crowdAI) 몫이다 — 구체적 예시는 [부록 C](#부록-c-fff_visitordistributionbuilder-상세) 맨 위에 있다.

`NPC/Party/FF_VisitorDistributionBuilder.h`. 순수 함수(같은 입력이면 매번 같은 출력) — 같은 `(Seed, 요청 인원, ScopeId, ArchetypeScenarioId, 입력 테이블)`은 항상 같은 결과를 낸다.

```cpp
FFF_VisitorDistribution Dist = FFF_VisitorDistributionBuilder::Build(
    GroupSizeTable, ChildCompositionTable, ArchetypeTable,
    TEXT("tourism_daytrip"), TEXT("resident_neutral_v0"),
    RequestedVisitorCount, GlobalSimulationSeed);
```

- `GroupSizeTable`/`ChildCompositionTable`/`ArchetypeTable`: §2의 DataTable 애셋(`DT_VisitorGroupSizeDistribution` 등)과 아키타입 애셋(`DT_NpcArchetypes`)을 그대로 꽂으면 된다.
- 네 번째 인자(`resident_neutral_v0`)는 **인구 구성 시나리오**(파라미터 이름 `ArchetypeScenarioId`)다 — "방문객이 사하구 주민 구성과 비슷하다"는 가정. **저희(Utility AI) 쪽 결정 사항이라 신경 쓰지 말고 기본값 그대로 두면 된다.** (**8/6 정정**: 원래 이 파라미터도 `ScenarioId`였는데 §4 유입 곡선의 `FFF_ArrivalCurveRow::ScenarioId`와 이름이 겹쳐서 헷갈릴 수 있어 `ArchetypeScenarioId`로 바꿨다 — 순수 함수 파라미터라 애셋 재임포트 없이 이름만 바뀜)

**GroupID도 그룹 소속 번호도 넣지 않는다** — 누구를 누구와 묶을지는 crowdAI 몫이다. 대신 개체마다 묶는 키 **`(TargetGroupSize, TargetGroupChildCount, ArchetypeId)`**를 붙여둔다. 같은 키의 성인 `TargetGroupSize-TargetGroupChildCount`명 + 같은 `(크기,아동수)`의 아동 `TargetGroupChildCount`명을 모으면 그룹 하나가 된다. `ArchetypeId`를 키에 넣은 덕분에 "같은 그룹 성인은 같은 아키타입"이 저절로 지켜진다.

**개체 수가 정확히 나누어떨어지는 것은 저희가 보장한다** — `Dist.GroupPlan`(예약 요약표에 해당)이 `(크기,아동수,아키타입)→그룹 몇 개`를 알려주고, 자동화 테스트가 검증한다. 나머지가 남거나 모자라는 일은 없다.

필드 전체 목록, 실전 코드 예시, `StableAgentId` 계산 공식은 [부록 C](#부록-c-fff_visitordistributionbuilder-상세) 참조.

## 4. 입장 타이밍

**정확한 도착 시각을 Utility AI가 지정하지 않는다.** 시간대별 목표 유입 곡선(`DT_VisitorArrivalCurve` DataTable 애셋, 구조체 `FFF_ArrivalCurveRow`)만 제공하고, 그 안에서 언제 어떤 그룹을 Pool에서 꺼낼지는 자유다. 제약은 한눈에 보기의 "그룹은 통째로" 하나뿐.

이 표는 Live 단계(축제 라이브 운영 시간대)를 24개 구간으로 나눠 각 구간에 "신규 유입이 몇 % 있어야 하는가"를 담고 있다 — 이 구간 하나하나를 **시간 버킷**이라 부른다. (§2/§3의 "그룹을 묶는 키"도 코드상 이름이 `FFF_VisitorGroupPlanBucket`이라 똑같이 "버킷"이라 불리는데, **완전히 다른 개념**이다. 시간 버킷은 "언제"를 답하고 그룹 버킷은 "누구끼리"를 답한다 — 같은 코드·같은 함수에 같이 나오는 일이 없어서 실제로 헷갈릴 일은 없다.)

**핵심 3가지 — 예시로 (총 유입 N=950, 어느 버킷이 `ArrivalSharePct=5.2`, `CumulativeSharePct=48.3`, `ProgressStart=0.4167`라고 하자):**

1. **비율(%)이지 인원이 아니다** — `5.2`는 인원수가 아니라 "전체 N의 5.2%"라는 뜻. `이 버킷 목표 인원 = 5.2% × 950 ≈ 49명`처럼 crowdAI가 정한 N을 곱해야 실제 인원이 나온다. N을 저희는 모른다(성능 실측으로 그쪽이 정하는 값이라서).
2. **계산은 `ProgressStart`(0~1 진행도) 기준** — `ProgressStart=0.4167`은 "Live 240분 중 41.67% 지점(=100분째)"이라는 뜻. `MinutesStartAtDefault`를 보면 "100분"이라 적혀 있겠지만 이건 Live가 지금 240분이라서 그런 것뿐 — **나중에 Live 길이가 바뀌면 이 숫자는 틀린 값이 된다.** 실제 코드는 `현재 경과분 / live_stage_duration_minutes`를 계산해서 `ProgressStart`와 비교해야 한다.
3. **버킷별 오차는 정상, 맞춰야 하는 건 `CumulativeSharePct`(누적)** — 이 버킷에 정확히 49명을 맞추려고 애쓸 필요 없다. 그룹이 1~6명 단위로 들어와서 딱 맞추는 게 원리적으로 불가능하다. 대신 "지금까지 총 몇 명 들어왔는지"를 `48.3% × 950 ≈ 459명`과 비교해서 크게 어긋나면 다음 그룹을 더 빨리/천천히 꺼내는 식으로 보정하면 된다.

**곡선 시나리오** 3종(`flat_v0`/`single_peak_v0`/`sunset_peak_v0`) 중 하나를 골라 쓴다 — 이 필드(`FFF_ArrivalCurveRow::ScenarioId`)는 §3의 `ArchetypeScenarioId`(인구 구성)와는 이름도 뜻도 다른 별개 값이다. **모양 수치는 전부 가정값**이라 특정 모양을 전제하지 않는 구현을 부탁드린다. 최종 어떤 걸 쓸지는 아직 팀 결정 전이라, 구현·테스트 중엔 `flat_v0`(균등, 가장 단순) 권장.

왜 이런 형태인지(절대 인원이 아닌 이유 등)는 `visitor_arrival_curve_spec.md`, 필드 전체 목록은 [부록 D](#부록-d-유입-곡선-필드-상세) 참조.

## 5. 그 외 요청

- **목적지 이동**: `FF_MoveCommand.h`. 항상 "이미 결정된 목적지 1개"만 전달, 도착/실패는 폴링 대신 `FFF_MoveResultEvent`로. 상세는 [부록 E](#부록-e-목적지-전달-상세).
- **셀 격자 밀도 로그**: `FFF_CrowdCellSample`(`FF_CrowdReadModel.h`) 기준 구조체. 5m×5m 격자, 좌표 원점·셀 크기는 같이 정해야 함.
- **성능 실측**: 300/500/700은 감으로 잡은 체크포인트일 뿐, 상한 의도 아님. 판단(Utility AI)·이동(Crowd)·렌더링 3개 층을 분리해서 각각 몇 개까지 버티는지 실측 요청.

## 6. 같이 정해야 하는 열린 질문

| 질문 | 왜 막혀 있는지 |
|---|---|
| 그룹 **리더가 누구인지** Utility AI가 어떻게 아는가 | 목적지 결정은 리더에 대해서만 해야 하는데, 리더 지정은 crowdAI가 한다. 이벤트? 조회 API? |
| `TargetDestinationId`를 월드 좌표로 바꾸는 조회는 누가 하는가 | 한광훈 님 시설 데이터와도 걸림 |
| `CohesionRadiusMeters` 값 | 이동 스택(Detour Crowd/Mass Entity) 확정 후 |
| 유입 곡선 누적 오차 허용 폭 | 실제로 돌려보고 정하는 게 맞다고 봄 — 정해지면 알려달라 |

---

# 부록 (필요할 때만 펼쳐보세요)

## 부록 A. 데이터 신뢰도 라벨

DataTable의 `Status` 열에 붙는 값. `derived`=실측 가중집계, `confirmed`=검증된 사실, `assumption`/`draft_assumption`=근거 없는 임시값(바뀔 수 있음), `to_verify`=아직 미확인.

## 부록 B. DataTable 필드 상세

`ScopeId` 값이나 `table_ref` 값(예: `visitor_group_size_distribution_v0`)은 저희 쪽 내부 데이터셋 이름(snake_case)이라 `DT_` 애셋 이름과 표기가 다르다 — 대응 관계는 아래 표 참조.

**`FFF_GroupSizeDistributionRow`**

| 필드 | 뜻 | 사용법 |
|---|---|---|
| `ScopeId` | 모집단 구분 | `tourism_daytrip` 행만 사용 |
| `GroupSize` | 그룹 인원수 1~6 | 뽑아서 그대로 그룹 크기로 |
| `CompanionSizeTag` | solo/pair/group_3_plus | `FUnitData::CompanionSizeTag`에 대입 |
| `SharePct` | 뽑힐 확률(%) | 같은 `ScopeId` 안에서 합계 100 |
| `bIsCapBucket` | `GroupSize=6`에서만 true | "6명 이상 전부"가 합쳐진 칸이라는 표시 |
| `Status` | 신뢰도 | 전부 `derived` |

**`FFF_GroupChildCompositionRow`**

| 필드 | 뜻 | 사용법 |
|---|---|---|
| `ScopeId`/`GroupSize` | 위와 동일 | 방금 뽑은 `GroupSize`와 일치하는 행만 필터 |
| `ChildCount` | 그룹 안 아동 수 | 뽑아서 그대로 사용 |
| `ShareWithinGroupSizePct` | 그 `GroupSize` **안에서** 뽑힐 확률(%) | 같은 `(ScopeId,GroupSize)` 안에서 합계 100 |

**`FFF_GroupCompositionRuleRow`** — 현재 11개 규칙:

| `RuleId` | `Value` | `ValueType` | `Status` | 뜻 |
|---|---|---|---|---|
| `group_size_cap` | `6` | int | derived | 그룹 최대 인원 |
| `group_size_distribution_table` | `visitor_group_size_distribution_v0` | table_ref | derived | = `DT_VisitorGroupSizeDistribution` 애셋 |
| `child_composition_table` | `group_child_composition_v0` | table_ref | derived | = `DT_GroupChildComposition` 애셋 |
| `child_age_definition` | `under_15` | enum | derived | 아동 슬롯 연령 기준 |
| `leader_count_per_group` | `1` | int | confirmed | 그룹당 리더 수 |
| `leader_must_be_adult` | `true` | bool | confirmed | 리더는 항상 성인 |
| `destination_decision_scope` | `leader_only` | enum | confirmed | 목적지는 리더만 계산 |
| `non_leader_adult_archetype_rule` | `same_as_leader` | enum | **assumption** | 리더 아닌 성인도 리더와 같은 아키타입. 실측 근거 없음 |
| `spawn_group_together` | `true` | bool | confirmed | 그룹원 전원 한 번에 스폰 |
| `group_shared_traits` | `walk_speed_multiplier;stay_duration_multiplier` | list | confirmed | 그룹이 공유하는 특성 2개 |
| `child_trait_overrides` | `movement_speed;attention_span;wait_tolerance` | list | **assumption** | 아동 전용 노브 3개, 값 미정 |

`ValueType` 파싱: `int`→정수, `bool`→문자열 비교, `enum`→고정 문자열, `list`→세미콜론 split, `table_ref`→다른 DataTable 이름.

## 부록 C. `FFF_VisitorDistributionBuilder` 상세

### 예시 (요청 10명)

| # | ArchetypeId | CompanionSizeTag | TargetGroupSize | TargetGroupChildCount | bIsDependentChildSlot |
|---|---|---|---|---|---|
| 1~2 | life_stage_60plus | solo | 1 | 0 | false |
| 3~6 | life_stage_20s | pair | 2 | 0 | false |
| 7~8 | life_stage_30_40s | group_3_plus | 4 | 2 | false(성인) |
| 9~10 | `NAME_None` | group_3_plus | 4 | 2 | true(아동) |

이때 `GroupPlan`은 `(크기1,아동0,life_stage_60plus)→2개`, `(크기2,아동0,life_stage_20s)→2개`, `(크기4,아동2,life_stage_30_40s)→1개`. crowdAI는 이 레시피를 보고 7~10번을 모아 4인 그룹 하나를 만들면 된다 — 몇 번과 몇 번을 묶을지는 자유(위 번호는 예시일 뿐).

### `FFF_TaggedVisitor` 필드

| 필드 | 용도 |
|---|---|
| `ArchetypeId` | 생애주기 5종. **아동 슬롯은 `NAME_None`** |
| `CompanionSizeTag` | solo/pair/group_3_plus |
| `TargetGroupSize` | 1~6, 그룹핑 시 봐야 하는 값 |
| `TargetGroupChildCount` | 그룹 내 아동 수 |
| `bIsDependentChildSlot` | 아동 여부 |

### `FFF_VisitorDistribution`(반환값) 필드

| 필드 | 뜻 | 사용법 |
|---|---|---|
| `Visitors` | 태그 붙은 개체 목록 | 순회하며 그룹 구성 |
| `GroupPlan` | `(크기,아동수,아키타입)→그룹 몇 개` 레시피 | 검산용으로도 사용 가능 |
| `ActualVisitorCount` | 실제 생성 인원 | 요청보다 최대 5명 적을 수 있음 — **정상 동작** |
| `DependentChildSharePct` | 실제 아동 비율(%) | **목표치 아님**, 결과값. 맞추려고 조정 불필요 |

### `StableAgentId` 계산

`FFF_DeterministicRandom::MakeTraitSeed`를 그대로 호출하면 안 된다 — 그 함수의 두 번째 인자가 이미 `StableAgentId`라서, `StableAgentId` 자체를 만드는 함수가 아니다. "재사용"의 의도는 **기법**(`GetTypeHash`+`HashCombine` 체이닝, 비결정적 값 금지)이었다:

```cpp
int32 ComputeStableAgentId(int32 GroupID, int32 OrdinalInGroup)
{
    return static_cast<int32>(HashCombine(GetTypeHash(GroupID), GetTypeHash(OrdinalInGroup)));
}
```

`OrdinalInGroup`은 그룹 안 몇 번째 멤버인지(0부터, 리더가 항상 0번 등 항상 같은 순서로 배정하면 결정적).

### 실전 사용 흐름 (의사코드)

```cpp
FFF_VisitorDistribution Dist = FFF_VisitorDistributionBuilder::Build(
    GroupSizeTable, ChildCompositionTable, ArchetypeTable,
    TEXT("tourism_daytrip"), TEXT("resident_neutral_v0"),
    RequestedVisitorCount, GlobalSimulationSeed);

// 1. Visitors 를 그룹 키로 분류 (성인/아동 별도 큐)
TMap<FBucketKey, TArray<FFF_TaggedVisitor*>> AdultQueue;   // 키: (Size, ChildCount, ArchetypeId)
TMap<FBucketKey, TArray<FFF_TaggedVisitor*>> ChildQueue;   // 키: (Size, ChildCount)

// 2. GroupPlan 순서대로 그룹 생성
int32 NextGroupId = 0;
for (const FFF_VisitorGroupPlanBucket& Bucket : Dist.GroupPlan)
{
    for (int32 g = 0; g < Bucket.GroupCount; ++g)
    {
        const int32 GroupID = NextGroupId++;
        int32 Ordinal = 0;
        for (int32 i = 0; i < Bucket.GroupSize - Bucket.ChildCount; ++i) // 성인, 0번이 리더
        {
            FFF_TaggedVisitor* M = AdultQueue[Key].Pop();
            M->GroupID = GroupID;
            M->StableAgentId = ComputeStableAgentId(GroupID, Ordinal++);
            M->bIsGroupLeader = (i == 0);
        }
        for (int32 i = 0; i < Bucket.ChildCount; ++i) // 아동, 서로 구별 안 됨
        {
            FFF_TaggedVisitor* M = ChildQueue[ChildKey].Pop();
            M->GroupID = GroupID;
            M->StableAgentId = ComputeStableAgentId(GroupID, Ordinal++);
        }
    }
}
```

큐에서 꺼내는 순서는 자유지만 매 실행 같은 Seed에서 같은 순서가 나와야 한다 — 배열 앞에서부터 순서대로면 충분, `FMath::RandRange`로 무작위 추출 금지.

## 부록 D. 유입 곡선 필드 상세

| 필드 | 타입 | 뜻 |
|---|---|---|
| `ScenarioId` | FName | flat_v0/single_peak_v0/sunset_peak_v0 중 하나만 사용 (§4의 "곡선 시나리오") |
| `BucketIndex`/`BucketCount` | int32 | 버킷 순번/전체 개수(24). **상수 박지 말고 값을 읽을 것** |
| `ProgressStart`/`ProgressEnd` | float | Live 진행도 [0,1). 계산 기준 |
| `ArrivalSharePct` | float | 이 버킷 신규 유입 비율(%) |
| `CumulativeSharePct` | float | 누적 유입 비율(%) — 실제로 맞춰야 하는 지표 |
| `MinutesStartAtDefault`/`ClockStartAtDefault` | float/FString | 표시용, 계산에 쓰지 말 것 |

## 부록 E. 목적지 전달 상세

`FFF_MoveCommand`/`FFF_MoveResultEvent` (`FF_MoveCommand.h`):
- **받는 것**: `PartyId`(=`GroupID`), `TargetDestinationId`(FName, ZoneSystem 시설 `FGuid`를 문자열화한 값), `TargetEntryPointId`, `Intent`, `CommandId`.
- **돌려줄 것**: `PartyId`, `CommandId`, `Result`, `SimTimeMinutes`. 폴링 금지 — 도착/실패 시 이벤트로.
- `TargetEntryPointId`는 퇴장 지점과 반드시 분리 (겹치면 데드락).
- `AFFZSGameplayZone::GetBoundaryAreaSquareMeters()` 결과는 `FFFZSZoneReadModel::AreaSquareMeters`로 노출된다. `FFF_CrowdReadModel::ZoneAreaSqm`이 필요하면 이 값을 ZoneId별로 복사.

## 부록 F. 함께 읽으면 좋을 문서

| 문서 | 언제 |
|---|---|
| `population_group_generation_spec.md` | 그룹핑 규칙 적용 코드 짤 때 — 전체 설계·근거·폐기 이력 |
| `visitor_arrival_curve_spec.md` | 입장 타이밍 구현할 때 — 유입 곡선 설계 결정 이유 |
| `dependent_child_npc_spec.md` | 아동 슬롯 관련 의문 — 왜 아키타입을 안 주는지 등 |
| `individual_variation_spec.md` | 그룹 공유 특성(`walk_speed_multiplier` 등) 구현할 때 |

## 요약 — crowdAI에 보낼 메시지

> 1. 역할을 다시 정리했어요 — 그룹핑과 입장 타이밍은 이제 crowdAI 쪽에서 정하는 걸로 할게요. 저희는 개별 방문객 태그 분포와 그룹 구성 규칙(데이터)만 드릴게요.
> 2. `FCrowdGroupTypes::GroupID`를 그대로 써주시면 돼요 — 새 필드 필요 없어요.
> 3. `FUnitData`에 `ArchetypeId`/`CompanionSizeTag`/`bIsDependentChildSlot`/`bIsGroupLeader`/`StableAgentId` 필드 추가 부탁드려요 (계산 공식은 문서 부록 C).
> 4. 입장 타이밍은 자유롭게 정하셔도 됩니다 — 딱 하나, 그룹은 항상 통째로 같이 들어와야 해요.
> 5. 목적지 이동은 `FF_MoveCommand.h`로 주고받아요. 도착/실패는 이벤트로요. 다만 "그룹 리더가 누구인지"는 같이 정해야 해요.
> 6. 셀 격자 밀도 로그(`FFF_CrowdCellSample` 기준) 부탁드려요.
> 7. 300/500/700 숫자에 큰 의미 두지 마세요 — 판단/이동/렌더링 나눠서 실측 부탁드려요.
> 8. 개체 목록 생성 함수도 다 됐어요 — `FFF_VisitorDistributionBuilder::Build`. 태그만 보고 묶으실 수 있어요 (문서 §3, 부록 C).
> 9. 유입 곡선 형태도 확정했어요 — `FFF_ArrivalCurveRow`. 비율(%)이지 인원이 아니에요 (문서 §4, 부록 D).
> 10. **8/4에 드린 그룹핑 규칙 중 일부가 폐기됐어요** — 이미 구현하셨다면 §2 "폐기된 것" 참고해서 걷어내주세요.
