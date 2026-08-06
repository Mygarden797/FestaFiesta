# Utility AI 목적지 선택기 v0 명세

- 작성일: 2026-08-04
- 상태: 선택기 알고리즘·글루 코드는 구현·테스트 완료. **실제 NPC Pawn/AIController에 연결된 적은 없음** (그런 클래스가 아직 없음).
- 관련 문서: `utility_ai_spec.md`(Utility 식·필드), `individual_variation_spec.md`(개체 변주 6종), `dependent_child_npc_spec.md`(종속 아동 처리)

## 1. 이 문서가 정하는 것

`Source/FestaFiesta/Public/NPC/Destination/FF_UtilityDestinationSelector.h` 및 그 주변 글루 코드(`FF_PreferenceVectorBuilder`, `FFF_IndividualVariationResolver::ResolvePersonalThresholds`, `FF_UtilityCoefficientHelpers`)가 실제로 어떻게 조립되는지, 그리고 아직 안 된 것을 정리한다.

## 2. 전체 호출 순서 (한 NPC의 목적지 선택 1회)

```text
1. FFF_IndividualVariationResolver::Resolve(...)
     → FFF_ResolvedIndividualTraits (6개 multiplier + leisureliness_axis)

2. FFF_PreferenceVectorBuilder::Build(
       ArchetypeId, CompanionType,
       Traits.PreferenceStrengthMultiplier,
       ArchetypeTable, CompanionModifierTable)
     → TMap<FName, float> PreferenceVectorByCategory
     (아키타입 delta + 동반유형 delta를 카테고리별로 더하고 개체 변주를 곱함 — "3층 합성")

3. FFF_IndividualVariationResolver::ResolvePersonalThresholds(Traits, UtilityCoefficientTable)
     → FFF_PersonalReevaluationThresholds
     (wait_threshold_minutes 등 전역 계수 * 개체 변주 multiplier)

4. FFF_UtilityDestinationSelector::Choose(
       Candidates, PreferenceVectorByCategory, UtilityCoefficientTable,
       GlobalSimulationSeed, StableAgentId, ChoiceRoundIndex)
     → FFF_DestinationDecision (선택 결과 + 항별 디버그 breakdown, 자동으로 Verbose 로그 출력)

5. (재평가 시점마다) FFF_UtilityDestinationSelector::ShouldReevaluate(
       CurrentWaitMinutes, Thresholds.WaitThresholdMinutes,
       CurrentDensityRatio, Thresholds.DensityThresholdRatio,
       SecondsSinceLastEvaluation, Thresholds.ReevaluateIntervalSeconds,
       bOpenChanged, bLiveEventChanged)
     → bool. true면 4번부터 다시.
```

1~3번은 NPC가 스폰될 때(또는 아키타입/동반유형이 바뀔 때) 한 번만 계산해서 캐싱해두면 된다 — 매 선택마다 다시 계산할 필요 없다(개체 변주는 `GlobalSimulationSeed`+`StableAgentId`가 같으면 항상 같은 값이므로).

## 3. 3층 합성 (`FFF_PreferenceVectorBuilder`)

`Source/FestaFiesta/Public/NPC/Destination/FF_PreferenceVectorBuilder.h`. 카테고리 10종(`ambience_photo_zone` 등, `build_unreal_datatables.mjs`의 `CATEGORY_FIELD`와 동일한 키)에 대해:

```text
PreferenceVectorByCategory[c] =
    (ArchetypeRow[ArchetypeId][c] + CompanionRow[CompanionType][c]) * PreferenceStrengthMultiplier
```

`ArchetypeRow`/`CompanionRow`를 못 찾으면(테이블이 비었거나 ID가 없으면) 해당 값을 0으로 취급하고 크래시하지 않는다.

## 4. 개인화 임계값 (`ResolvePersonalThresholds`)

`Source/FestaFiesta/Public/NPC/IndividualVariation/FF_DeterministicRandom.h`의 `FFF_IndividualVariationResolver::ResolvePersonalThresholds`. individual_variation_spec.md 5절에서 정한 매핑을 그대로 코드로 옮긴 것뿐이다:

| Personal 필드 | 전역 계수 | 개체 변주 |
|---|---|---|
| `WaitThresholdMinutes` | `wait_threshold_minutes` | `PatienceMultiplier` |
| `DensityThresholdRatio` | `density_threshold_ratio` | `CrowdToleranceMultiplier` |
| `ReevaluateIntervalSeconds` | `reevaluate_interval_seconds` | `ReevaluateIntervalMultiplier` |

`weight_wait`/`weight_density`(Utility 식 안의 전역 비용 가중치)는 건드리지 않는다 — 이 둘은 계속 전역 값 그대로 쓴다 (5절 개체 변주 매핑 표의 "중요한 구분" 참고).

## 5. Utility 계수 조회 (`FFF_UtilityCoefficientHelpers`)

`utility_coefficients_v0` DataTable에서 계수 하나를 Row 이름(=`CoefficientId`)으로 찾는 헬퍼. `FFF_UtilityDestinationSelector::Choose`와 `ResolvePersonalThresholds`가 공유한다 — 계수 조회 로직을 두 번 짜지 않기 위해 분리했다.

## 6. 디버그 로그

`LogFestaFiestaNpc` 카테고리(`FF_UtilityDestinationSelector.h`에 선언). `Choose()`가 끝날 때마다 최종 선택 + 후보별 항별 점수를 `Verbose` 레벨로 출력한다.

- **기본적으로 아무것도 안 찍힌다** — 방문객 규모가 최대 8만 명(`total_visitor_count`)까지 갈 수 있어서, 기본 노출하면 로그가 감당 안 된다.
- 켜려면 콘솔에 `Log LogFestaFiestaNpc Verbose` 입력.
- 출력 예시:
  ```text
  [목적지 선택] Agent=7 Round=2 선택=food_stall_01 (no_choice=0, U=0.4213)
    - food_stall_01: Attraction=0.720 Preference=0.310 Distance=-0.120 Wait=-0.050 Density=-0.080 Total=0.780 Prob=0.612
    - no_choice: Attraction=0.000 Preference=0.000 Distance=0.000 Wait=0.000 Density=0.000 Total=0.150 Prob=0.388
  ```

## 7. 아직 안 된 것 (v0 범위 밖으로 명시적으로 남긴 것)

1. **NPC 상태 보관·Tick 루프 없음** — 위 2절의 1~5단계를 실제로 누가·언제 호출하는지는 이 문서의 범위가 아니다. 실제 NPC Pawn/AIController가 생기면 그쪽에서 `FFF_ResolvedIndividualTraits`/`FFF_PersonalReevaluationThresholds`를 캐싱하고, 매 Tick 또는 이벤트마다 `ShouldReevaluate`를 호출해야 한다.
2. **`attraction_norm`(부스 기본 매력도 정규화) 설계 미정** — `FFF_DestinationCandidate::BaseAttraction`을 0~1로 누가 어떻게 정할지는 부스 설정 쪽 설계 대기 (`utility_ai_spec.md` 8절).
3. **거리는 직선거리 근사** — 축제장 레이아웃·내비메시 확정 후 실제 보행거리(장애물 포함)로 교체.
4. **종속 아동 분기는 selector 밖에서 처리** — `bIsDependent == true`인 개체는 이 selector를 호출하지 않고 부모의 `ChosenDestinationId`를 그대로 복사한다 (`dependent_child_npc_spec.md`).
5. **그룹 배정(누가 누구와) — 담당이 crowdAI로 넘어갔다 (8/6 역할 재조정)**. Utility AI 쪽 몫인 **개체 태그 분포 생성은 완료**됐다(`FFF_VisitorDistributionBuilder::Build`, `NPC/Party/FF_VisitorDistributionBuilder.h`) — 개체마다 `ArchetypeId`/`CompanionSizeTag`/`TargetGroupSize`/`TargetGroupChildCount`/`bIsDependentChildSlot`을 붙여 목록으로 돌려준다. 그 태그를 보고 실제로 묶고 `GroupID`를 부여하는 것은 crowdAI다(`crowdai_handoff_note.md` §3). 즉 이 selector 입장에서 남은 미지수는 **"내가 지금 보고 있는 이 개체가 그룹 리더인지"를 어떻게 아는가** 하나이며, 그 조회 경로는 아직 미정이다(같은 노트 §6 열린 질문).
6. **만족도(Experience) 계산 미착수** — 사용자 요청으로 홀드 중.
