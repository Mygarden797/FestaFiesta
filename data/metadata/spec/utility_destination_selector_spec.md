# Utility AI 목적지 선택기 v0 명세

- 작성일: 2026-08-04 (코드 대조 갱신 2026-09-03)
- 상태: 선택기 알고리즘·글루 코드 구현·테스트 완료. **2026-08-19 갱신 — 실제 NPC에 연결됐다.**
  `UFF_CrowdDecisionSubsystem`(조정자)이 crowdAI의 결정 델리게이트를 물고 `Choose`를 부른다.
  Pawn/AIController가 아니라 Crowd 유닛 단위로 도는 구조다.
  **시설 효용식은 9항이며 Full 모드에서 7항의 입력 경로가 연결돼 있다.**
  preference·need·distance·wait·revisit·failed_destination·in_transit이 연결됐고 attraction은 0, density는 인구 보고 미연결이다（실제 항값은 상황별로 0일 수 있음）.
- 관련 문서: `utility_ai_spec.md`(Utility 식·필드), `individual_variation_spec.md`(multiplier 7종 + 공유 축 1종), `dependent_child_npc_spec.md`(종속 아동 처리)
- **2026-08-06 변경**: Utility 식에 `need` 항 추가(당시 6항, 현재 9항), `no_choice` 행동 3종 분해 + 연속 횟수 감쇠, 개체 변주 그룹 공유 wrapper. `Choose()`에 인자 2개가 추가됐다(기본값 있음).

## 1. 이 문서가 정하는 것

`Source/FestaFiesta/Public/NPC/Destination/FF_UtilityDestinationSelector.h` 및 그 주변 글루 코드(`FF_PreferenceVectorBuilder`, `FFF_IndividualVariationResolver::ResolvePersonalThresholds`, `FF_UtilityCoefficientHelpers`)가 실제로 어떻게 조립되는지, 그리고 아직 안 된 것을 정리한다.

## 2. 전체 호출 순서 (한 NPC의 목적지 선택 1회)

（2026-09-03 코드 대조: 아래는 순수 API 조립 계약이다. 실제 스폰은 ResolveWithGroupSharing의 속도·체류만 소비한다. ResolvePersonalThresholds는 미연결이고 취향 벡터는 Decision이 아키타입+동반유형별로 배수 1.0을 넣어 캐시한다. 실제 결정은 ShouldReevaluate를 통과하지 않고 Crowd 요청 큐에서 들어온다.）

```text
1. FFF_IndividualVariationResolver::ResolveWithGroupSharing(...)   ← 8/6: Resolve()가 아니라 이것
     → FFF_ResolvedIndividualTraits (7개 multiplier + leisureliness_axis)
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
       CurrentNeeds, ConsecutiveNoChoiceCount, NoveltySeekingMultiplier=1.f)
     （현재 Crowd 호출은 마지막 개인 배수를 생략한다）
     → FFF_DestinationDecision (선택 결과 + 항별 디버그 breakdown, 자동으로 Verbose 로그 출력)

4-1. (bIsNoChoice == true 일 때만) FFF_UtilityDestinationSelector::ResolveNoChoiceAction(
       RemainingStayMinutes, ConsecutiveNoChoiceCount, NoChoiceLeavePatienceRounds,
       CurrentZoneDensityRatio, Thresholds.DensityThresholdRatio)
     → EFF_NoChoiceAction (Wander / Relocate / Leave)

5. (재평가 감시 — 2026-10-09 기준 Crowd 주기 재평가·줄 인내 이탈에서 호출됨) FFF_UtilityDestinationSelector::ShouldReevaluate(
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

### 실제 후보 조립과 모드

`BuildCandidates` → 당일 예약 전용 시설 제외 → 직전 실패 후보 표시 → Full에서 방문 이력·Queue 대기·이동 중 인원 반영 → 활성 예약의 귀환 가능 후보 필터 → `Choose` → 시설 `Entry[0]` 접근점을 결과로 전달한다.

일반 후보는 현재·직접 인접 Zone에서 최대 12개다. 공연 중 `Performance_MainStage_SunsetMainStage`만 모든 등록 Zone에서 상한 밖에 중복 없이 추가한다. StreetStage·DanceBattle·LocalArtist는 메인 공연 시간과 무관한 지역 후보다（예약 프로그램 편성 시에는 당일 전용 정책 적용）.

어떤 Zone 에도 속하지 않은 파티는 200m 안 가장 가까운 Zone(액터 원점 기준, `MaxOutsideZoneFallbackMeters`)을 현재 Zone 으로 보고 후보를 만든다（2026-10-09 변경. 전엔 `OutsideZone` 으로 결정 포기. `FF.Decision.Stats` 의 `OutsideZoneUsedNearest`）.

- `Random(0)`: 시설 선택 없이 폴백으로 간다（체류 소진이면 Leave도 가능하므로 무조건 배회는 아님）.
- `Greedy(1)`: 취향·Need·대기·혼잡·방문 이력·이동 중 인원을 끈 입력으로 **같은 softmax**를 호출한다（엄밀한 argmax가 아님. 실패 목적지 감점과 no_choice는 남음）.
- `Full(2)`: 9항 계산 경로. Attraction 입력 0, Zone density 보고 미연결, 개인 신규성 배수 1.0이라는 한계가 남는다（콘솔의 “6항 전부” 문구는 낡은 표시）.

## 6. 디버그 로그

`LogFestaFiestaNpc` 카테고리(`FF_UtilityDestinationSelector.h`에 선언). `Choose()`가 끝날 때마다 최종 선택 + 후보별 항별 점수를 `Verbose` 레벨로 출력한다.

- **기본적으로 아무것도 안 찍힌다** — 방문객 규모가 최대 8만 명(`total_visitor_count`)까지 갈 수 있어서, 기본 노출하면 로그가 감당 안 된다.
- 켜려면 콘솔에 `Log LogFestaFiestaNpc Verbose` 입력.
- 실제 후보 로그는 `Attraction / Preference / Distance / Wait / Density / Need / Revisit / Failed / InTransit / Total / Prob`를 출력한다. `U`는 샘플링 난수이고 Total 효용과 다르다.
- `FF.Decision.Stats`는 결정·실패·대기 입력·이동 중 인원·거리 행렬 진단, `FF.Crowd.StageFlow [leader index]`는 구역/게이트/예약/Queue/NavMesh/시뮬레이션 경계 진단이다.
- `FF.Crowd.Reevaluate 0`은 적격 리더 전체 재평가 요청이다（공연 시각은 바꾸지 않음. 프레임당 64개 기본 처리）.

## 7. 구현 상태와 남은 것 (초기 미완료 목록 보존)

1. **NPC 상태와 실행 루프는 Crowd에 연결됨.** `FFF_PartyDecisionContext`로 파티 Need·방문 이력·연속 no_choice·예약·속도·이동 중 원장을 넘긴다. `ShouldReevaluate` 주기 감시와 개인 임계/취향 강도/신규성 배수 전달은 미연결이다（Pawn/AIController를 추가하는 설계가 아님）. （2026-10-09 기준: 주기 감시는 Crowd 리더 분기에서 연결됨. 시설로 걸어가는 중이면 주기 ×3 `EnRouteReevaluateIntervalScale`.）

1-1. ~~**`FFF_ZoneSystemBridge::BuildCandidates`가 스텁**~~ — **2026-08-10 구현 완료.** 현재+직접 인접 Zone으로 좁히되 공연 중 Sunset은 전역으로 추가하고, 닫힌 시설·Info·Safety를 빼고, (같은 Zone → 가까운 순 → ID 순)으로 상한을 자른다. 테스트 10개.

   `DistanceMeters`는 `FFFZSFacilityReadModel::WorldLocation`과 Party 월드 위치의 직선거리를
   cm→m로 환산해 채운다. **남은 구멍 2개** (전부 다른 파트 대기, 값이 없어도 크래시는 없다):
   - `BaseAttraction` = 0 — `attraction_norm` 설계 대기(아래 2번). 모든 후보가 0이면 항이 무력화될 뿐 특정 시설이 유리해지지 않는다
   - 밀집도 — 시설 단위 점유값이 없어 Zone 밀집도(`ReportedPopulation`/`SoftCapacity`)로 근사(`assumption`). 같은 Zone 시설들의 density_cost가 전부 같아진다

2. **`attraction_norm`(부스 기본 매력도 정규화) 설계 미정** — `FFF_DestinationCandidate::BaseAttraction`을 0~1로 누가 어떻게 정할지는 부스 설정 쪽 설계 대기 (`utility_ai_spec.md` 8절).
3. **거리 행렬은 선택적으로 사용 가능.** 기본은 직선거리이며 `FF.Decision.WalkDistance 1`로 Zone 앵커 간 NavMesh 행렬을 켠다. WorldRevision 변경 시 재구축하고 NavMesh 빌드 중이면 미룬다（양 끝은 직선 근사, 같은 Zone·행렬 실패 시 직선 폴백. 접근점까지 완전 경로를 보장하는 필터는 아님）.
4. **종속 아동 분기는 selector 밖에서 처리** — 실제 `bIsDependentChild` 유닛은 리더의 목적지를 상속하며, Crowd가 `LeaderAgentIndex`로 비리더 전체의 독립 결정을 막는다 (`dependent_child_npc_spec.md`).
5. **그룹 배정(누가 누구와) — 담당이 crowdAI로 넘어갔다 (8/6 역할 재조정)**. Utility AI 쪽 몫인 **개체 태그 분포 생성은 완료**됐다(`FFF_VisitorDistributionBuilder::Build`, `NPC/Party/FF_VisitorDistributionBuilder.h`) — 개체마다 `ArchetypeId`/`CompanionSizeTag`/`TargetGroupSize`/`TargetGroupChildCount`/`bIsDependentChildSlot`을 붙여 목록으로 돌려준다. 그 태그를 보고 실제로 묶고 `GroupID`를 부여하는 것은 crowdAI다(`request/crowdai_integration_guide.md` 부록 C). 즉 이 selector 입장에서 남은 미지수는 **"지금 평가 중인 개체가 그룹 리더인지"를 어떻게 아는가** 하나이며, 현재 `FUnitData::LeaderAgentIndex`와 `PartyID`를 이용해 Crowd가 리더만 요청 큐에 넣는 경로가 구현돼 있다.
6. **만족도(Experience) 계산 미착수** — 사용자 요청으로 홀드 중.（10/14 기준: 만족도는 구현됐다 — `UFF_SatisfactionMetricsSubsystem`. selector 점수식에는 들어가지 않고, 어제 만족도가 다음 날 방문객 배수(평판)와 축제 평가에 쓰인다）
7. ~~`no_choice` 행동의 그룹 전파 미정~~ → **2026-08-06 결정: 리더가 퇴장하면 그룹 전원이 함께 퇴장한다.** 행동 3종(`Wander`/`Relocate`/`Leave`) 전부 목적지와 똑같이 리더가 결정하고 그룹원이 상속한다 — 그룹원이 각자 다른 행동을 하는 경우는 없다. `spawn_group_together`와 대칭으로 통째로 들어오고 통째로 나가며, 덕분에 아동만 장내에 남는 상태가 원리적으로 발생하지 않는다.
8. **후보 필터가 결과를 좌우하는데 계수로 안 나와 있다** (8/6 신설) — 미구현 설계인 "전역 인기 상위 K"는 구현 시 부익부 피드백을 만들 수 있다（현재 전역 예외는 공연 중 Sunset뿐）(인기 있으면 후보에 더 자주 들어가서 더 인기 있어진다). K와 "인기"의 정의를 계수로 빼지 않으면 병목이 실제 선호 때문인지 후보 필터 때문인지 구분할 수 없다.

9. **예약 보호는 연결됐고 일부 추정은 남아 있다.** 프로그램 편성 시설은 당일 일반 후보·Queue에서 제외한다. 귀환 후보 필터는 실제 Queue 대기를 반영한다（`CandidateUseMinutes`는 0, 시설→예약 시설 거리는 직선）. 복귀 마감은 Simulation 고정 스텝이 별도로 감시한다.

## 8. Utility 식·계수 변경 이력

| 날짜 | 변경 | 이유 |
|---|---|---|
| 2026-10-09 | `distance_ref_meters` 400 → 150, 새 계수 `distance_cost_cap` 3（거리 비용 = 0.8 × min(거리/150m, 3)）. Zone 밖 파티는 200m 안 가장 가까운 Zone 으로 결정. 이동 중 재평가 주기 ×3. 굿즈·공방·게임 부스가 `Stimulation` 해소 | 전엔 거리 비용이 400m 에서 1로 포화했다. 입구·길 위 파티가 결정을 포기했고, 걷는 도중 돌아서 줄에 서지 못했다. 세 부스는 Need 항이 없어 손님이 거의 오지 않았다 |
| 2026-09-03 코드 확인 | 실패 목적지·이동 중 인원 감점 포함 **9항**, 방문 이력 Full 연결 | 실패 반복·이동 중 수요를 별도로 반영（신규 계수 3행은 CSV에 없어 코드 폴백 사용） |
| 2026-09-03 | 시간 게이트·전역 후보를 SunsetMainStage ID 하나로 한정 | 소규모 공연 시설은 주변 후보 유지. 적격 리더 전체 요청·64개/프레임 처리 |
| 2026-08-04 | `preference_match` 항을 `preference_norm` 하나로 확정, `affinity_norm` v0 미사용 | 아키타입 구조 확정으로 정의가 겹쳤다 |
| 2026-08-06 | `need` 항 추가 (`weight_need` 1.8) | Need가 이동 원인이 되게 함 + 재방문 문제 해결 |
| 2026-08-06 | `no_choice_utility_decay_per_round` 0.03 추가 | 무한 배회 루프 방지 |
| 2026-08-06 | `no_choice_leave_patience_rounds` 5 추가 | 퇴장 판정. 체류시간 게이트가 우선 |
| 2026-08-10 | **`revisit_penalty` 항 추가** (`weight_revisit` 0.5 / `revisit_memory_minutes` 60 / 개체 변주 `novelty_seeking_multiplier`) | `preference_vector`가 카테고리 단위라 "이 부스는 가봤으니 옆 부스로"가 안 나왔다. 추가 당시 `attraction`·거리·밀집 입력이 모두 같아 같은 카테고리 부스들의 점수가 완전히 동일했다. **경제와 직결** — 같은 부스만 가면 매출이 한 곳에 몰린다 |
| 2026-08-10 | `weight_need` **코드 기본값 1.2 → 1.8** (CSV와 불일치 수정) | CSV의 1.8 근거가 `weight_need > weight_attraction + lambda_affinity(1.6)` 부등식인데 1.2는 그걸 깬다 — DataTable 없는 경로에서 급한 욕구가 취향을 못 이겼다. 부등식을 테스트로 고정 |
| 2026-08-10 | `EstimatedWaitMinutes`를 브릿지가 채우지 않도록 변경 | 소유자를 `FFF_QueueWaitAdapter` 한 곳으로. 폴백을 만들었다가 "Queue가 모르는 시설이 없다"는 것을 확인해 제거 |

### `revisit_penalty` 항의 입력 경로

```text
revisit_penalty_norm = max(0, 1 − MinutesSinceLastVisit / revisit_memory_minutes)
```

`FFF_DestinationCandidate::MinutesSinceLastVisit`로 들어온다. **음수 = 안 가본 곳**이고 그게 기본값이다 — `0`은 "방금 다녀왔다"라서 뜻이 완전히 다르다.

채우는 것은 `FFF_VisitHistory::ApplyToCandidates`이고, `FFF_QueueWaitAdapter::ApplyToCandidates`와 같은 "후보 채우기" 형태다 — 선택기가 이력 구조를 몰라도 되게 하려는 것이다. **안 채우면 항 전체가 0이 되어 신규성 선호가 꺼진 상태로 동작한다**(크래시 없음).

`FFF_VisitHistory`는 최근 5곳만 기억하며 현재 Crowd의 리더 인덱스별 파티 이력으로 보관하고, 기록 시점은 **"시설 이용 완료"**다(`FFF_NeedRules::Relieve`와 같은 시점). 도착만 하고 포기한 곳을 "가봤다"로 세면 줄만 서다 떠난 시설을 다시 안 가게 된다.

자세한 근거와 계수 표는 `utility_ai_spec.md` §2 "revisit 항".

## 코드 대조 근거 (2026-09-03)

- [FF_UtilityDestinationSelector.cpp](../../../Source/FestaFiesta/Private/NPC/Destination/FF_UtilityDestinationSelector.cpp)
- [FF_CrowdDecisionSubsystem.cpp](../../../Source/FestaFiesta/Private/CrowdSystem/Systems/FF_CrowdDecisionSubsystem.cpp)
- [FF_CrowdSimulationSubsystem.cpp](../../../Source/FestaFiesta/Private/CrowdSystem/Systems/FF_CrowdSimulationSubsystem.cpp)

（현재 작업 트리 기준. 데이터/계수 수는 저장소 CSV, 설정은 C++ 기본값과 Config를 구분해 기술했다. 설계 제안·과거 실측은 코드만으로 재검증할 수 없어 해당 설명을 보존했다.）
