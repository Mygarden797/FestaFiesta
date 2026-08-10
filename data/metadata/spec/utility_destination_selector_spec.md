# Utility AI 목적지 선택기 v0 명세

- 작성일: 2026-08-04 (최종 갱신 2026-08-06)
- 상태: 선택기 알고리즘·글루 코드는 구현·테스트 완료. **실제 NPC Pawn/AIController에 연결된 적은 없음** (그런 클래스가 아직 없음).
- 관련 문서: `utility_ai_spec.md`(Utility 식·필드), `individual_variation_spec.md`(개체 변주 6종), `dependent_child_npc_spec.md`(종속 아동 처리)
- **2026-08-06 변경**: Utility 식에 `need` 항 추가(6항), `no_choice` 행동 3종 분해 + 연속 횟수 감쇠, 개체 변주 그룹 공유 wrapper. `Choose()`에 인자 2개가 추가됐다(기본값 있음).

## 1. 이 문서가 정하는 것

`Source/FestaFiesta/Public/NPC/Destination/FF_UtilityDestinationSelector.h` 및 그 주변 글루 코드(`FF_PreferenceVectorBuilder`, `FFF_IndividualVariationResolver::ResolvePersonalThresholds`, `FF_UtilityCoefficientHelpers`)가 실제로 어떻게 조립되는지, 그리고 아직 안 된 것을 정리한다.

## 2. 전체 호출 순서 (한 NPC의 목적지 선택 1회)

```text
1. FFF_IndividualVariationResolver::ResolveWithGroupSharing(...)   ← 8/6: Resolve()가 아니라 이것
     → FFF_ResolvedIndividualTraits (6개 multiplier + leisureliness_axis)
     그룹 멤버면 walk_speed/stay_duration을 GroupId 시드 값으로 덮는다 (solo면 GroupId=INDEX_NONE)

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
       GlobalSimulationSeed, StableAgentId, ChoiceRoundIndex,
       CurrentNeeds, ConsecutiveNoChoiceCount)                    ← 8/6: 마지막 2개 추가(기본값 있음)
     → FFF_DestinationDecision (선택 결과 + 항별 디버그 breakdown, 자동으로 Verbose 로그 출력)

4-1. (bIsNoChoice == true 일 때만) FFF_UtilityDestinationSelector::ResolveNoChoiceAction(
       RemainingStayMinutes, ConsecutiveNoChoiceCount, NoChoiceLeavePatienceRounds,
       CurrentZoneDensityRatio, Thresholds.DensityThresholdRatio)
     → EFF_NoChoiceAction (Wander / Relocate / Leave)

5. (재평가 시점마다) FFF_UtilityDestinationSelector::ShouldReevaluate(
       CurrentWaitMinutes, Thresholds.WaitThresholdMinutes,
       CurrentDensityRatio, Thresholds.DensityThresholdRatio,
       SecondsSinceLastEvaluation, Thresholds.ReevaluateIntervalSeconds,
       bOpenChanged, bLiveEventChanged)
     → bool. true면 4번부터 다시.
```

1~3번은 NPC가 스폰될 때(또는 아키타입/동반유형이 바뀔 때) 한 번만 계산해서 캐싱해두면 된다 — 매 선택마다 다시 계산할 필요 없다(개체 변주는 `GlobalSimulationSeed`+`StableAgentId`가 같으면 항상 같은 값이므로).

**호출부가 보관해야 하는 상태 (8/6 추가)**: 4번 호출에 `CurrentNeeds`(`FFF_NeedState`)와 `ConsecutiveNoChoiceCount`가 필요하다. selector는 상태를 안 들고 있으므로 이 둘은 NPC를 들고 있는 쪽이 보관·갱신해야 한다 — `no_choice`가 나오면 `ConsecutiveNoChoiceCount`를 1 올리고, 목적지를 골랐으면 0으로 되돌린다. 둘 다 기본값이 있어서 안 넘겨도 컴파일은 되지만, 안 넘기면 Need 항이 0이 되고 무한 배회 방지도 동작하지 않는다.

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

   **(8/6) 보관해야 하는 상태가 늘었다.** `Choose()`가 `FFF_NeedState`와 `ConsecutiveNoChoiceCount`를 받고, `ResolveNoChoiceAction()`이 `RemainingStayMinutes`를 받는다. 둘 다 기본값이 있어서 안 넘겨도 컴파일은 되지만 — 안 넘기면 need 항이 0이 되고 무한 배회 방지도 동작하지 않는다.

1-1. ~~**`FFF_ZoneSystemBridge::BuildCandidates`가 스텁**~~ — **2026-08-10 구현 완료.** 현재+인접 Zone으로 좁히고, 닫힌 시설·Info·Safety를 빼고, (같은 Zone → 가까운 순 → ID 순)으로 상한을 자른다. 테스트 10개.

   **남은 구멍 3개** (전부 다른 파트 대기, 값이 없어도 크래시는 없다):
   - `BaseAttraction` = 0 — `attraction_norm` 설계 대기(아래 2번). 모든 후보가 0이면 항이 무력화될 뿐 특정 시설이 유리해지지 않는다
   - `DistanceMeters` = 0 — **시설 월드 위치가 `FFFZSFacilityReadModel`에 없다.** `CaptureInstanceSnapshot()`은 `WorldTransform`을 담는데 `GetReadModel()`은 안 담는다. 지금은 `BuildCandidates`가 위치 맵을 파라미터로 받아 우회하고, 맵이 비면 거리 항이 무력화된다
   - 밀집도 — 시설 단위 점유값이 없어 Zone 밀집도(`ReportedPopulation`/`SoftCapacity`)로 근사(`assumption`). 같은 Zone 시설들의 density_cost가 전부 같아진다

2. **`attraction_norm`(부스 기본 매력도 정규화) 설계 미정** — `FFF_DestinationCandidate::BaseAttraction`을 0~1로 누가 어떻게 정할지는 부스 설정 쪽 설계 대기 (`utility_ai_spec.md` 8절).
3. **거리는 직선거리 근사** — 축제장 레이아웃·내비메시 확정 후 실제 보행거리(장애물 포함)로 교체.
4. **종속 아동 분기는 selector 밖에서 처리** — `bIsDependent == true`인 개체는 이 selector를 호출하지 않고 부모의 `ChosenDestinationId`를 그대로 복사한다 (`dependent_child_npc_spec.md`).
5. **그룹 배정(누가 누구와) — 담당이 crowdAI로 넘어갔다 (8/6 역할 재조정)**. Utility AI 쪽 몫인 **개체 태그 분포 생성은 완료**됐다(`FFF_VisitorDistributionBuilder::Build`, `NPC/Party/FF_VisitorDistributionBuilder.h`) — 개체마다 `ArchetypeId`/`CompanionSizeTag`/`TargetGroupSize`/`TargetGroupChildCount`/`bIsDependentChildSlot`을 붙여 목록으로 돌려준다. 그 태그를 보고 실제로 묶고 `GroupID`를 부여하는 것은 crowdAI다(`crowdai_handoff_note.md` §3). 즉 이 selector 입장에서 남은 미지수는 **"지금 평가 중인 개체가 그룹 리더인지"를 어떻게 아는가** 하나이며, 그 조회 경로는 아직 미정이다(같은 노트 §6 열린 질문).
6. **만족도(Experience) 계산 미착수** — 사용자 요청으로 홀드 중.
7. ~~`no_choice` 행동의 그룹 전파 미정~~ → **2026-08-06 결정: 리더가 퇴장하면 그룹 전원이 함께 퇴장한다.** 행동 3종(`Wander`/`Relocate`/`Leave`) 전부 목적지와 똑같이 리더가 결정하고 그룹원이 상속한다 — 그룹원이 각자 다른 행동을 하는 경우는 없다. `spawn_group_together`와 대칭으로 통째로 들어오고 통째로 나가며, 덕분에 아동만 장내에 남는 상태가 원리적으로 발생하지 않는다.
8. **후보 필터가 결과를 좌우하는데 계수로 안 나와 있다** (8/6 신설) — `BuildCandidates`의 "전역 인기 상위 K"가 부익부 피드백을 만든다(인기 있으면 후보에 더 자주 들어가서 더 인기 있어진다). K와 "인기"의 정의를 계수로 빼지 않으면 병목이 실제 선호 때문인지 후보 필터 때문인지 구분할 수 없다.

## 8. Utility 식·계수 변경 이력

| 날짜 | 변경 | 이유 |
|---|---|---|
| 2026-08-04 | `preference_match` 항을 `preference_norm` 하나로 확정, `affinity_norm` v0 미사용 | 아키타입 구조 확정으로 정의가 겹쳤다 |
| 2026-08-06 | `need` 항 추가 (`weight_need` 1.8) | Need가 이동 원인이 되게 함 + 재방문 문제 해결 |
| 2026-08-06 | `no_choice_utility_decay_per_round` 0.03 추가 | 무한 배회 루프 방지 |
| 2026-08-06 | `no_choice_leave_patience_rounds` 5 추가 | 퇴장 판정. 체류시간 게이트가 우선 |
| 2026-08-10 | **`revisit_penalty` 항 추가** (`weight_revisit` 0.5 / `revisit_memory_minutes` 60 / 개체 변주 `novelty_seeking_multiplier`) | `preference_vector`가 카테고리 단위라 "이 부스는 가봤으니 옆 부스로"가 안 나왔다. `attraction`·거리·밀집이 전부 0인 현재 상태에서는 같은 카테고리 부스들의 점수가 완전히 동일해 softmax가 균등 분포가 된다. **경제와 직결** — 같은 부스만 가면 매출이 한 곳에 몰린다 |
| 2026-08-10 | `weight_need` **코드 기본값 1.2 → 1.8** (CSV와 불일치 수정) | CSV의 1.8 근거가 `weight_need > weight_attraction + lambda_affinity(1.6)` 부등식인데 1.2는 그걸 깬다 — DataTable 없는 경로에서 급한 욕구가 취향을 못 이겼다. 부등식을 테스트로 고정 |
| 2026-08-10 | `EstimatedWaitMinutes`를 브릿지가 채우지 않도록 변경 | 소유자를 `FFF_QueueWaitAdapter` 한 곳으로. 폴백을 만들었다가 "Queue가 모르는 시설이 없다"는 것을 확인해 제거 |

### `revisit_penalty` 항의 입력 경로

```text
revisit_penalty_norm = max(0, 1 − MinutesSinceLastVisit / revisit_memory_minutes)
```

`FFF_DestinationCandidate::MinutesSinceLastVisit`로 들어온다. **음수 = 안 가본 곳**이고 그게 기본값이다 — `0`은 "방금 다녀왔다"라서 뜻이 완전히 다르다.

채우는 것은 `FFF_VisitHistory::ApplyToCandidates`이고, `FFF_QueueWaitAdapter::ApplyToCandidates`와 같은 "후보 채우기" 형태다 — 선택기가 이력 구조를 몰라도 되게 하려는 것이다. **안 채우면 항 전체가 0이 되어 신규성 선호가 꺼진 상태로 동작한다**(크래시 없음).

`FFF_VisitHistory`는 개체당 최근 5곳만 기억하고, 기록 시점은 **"시설 이용 완료"**다(`FFF_NeedRules::Relieve`와 같은 시점). 도착만 하고 포기한 곳을 "가봤다"로 세면 줄만 서다 떠난 시설을 다시 안 가게 된다.

자세한 근거와 계수 표는 `utility_ai_spec.md` §2 "revisit 항".
