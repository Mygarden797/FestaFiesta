# crowdAI 담당자에게 전달할 내용 — 아키타입·동반유형 연동

- 대상: Crowd 시스템(`Source/FestaFiesta/*/CrowdSystem/`, PR #8로 Dev에 병합됨) 담당(이정원)
- 배경: `FUnitData`(`CrowdSystem/Types/FF_FestivalCrowdTypes.h`) 유닛이 이 문서의 아키타입 NPC와 같은 존재다. 현재 필드는 `Position`/`Velocity`/`TargetLocation`/`Speed`/`InstanceIndex`뿐이고 아키타입 식별자는 없다.
- **확인 필요(8/6)**: 같은 파일에 `FCrowdGroupTypes`(`GroupID`/`GroupName`)가 이미 있고 `AFF_BaseCrowd::CrowdGroup`이 이걸 참조한다 — 아래 2절에서 요청하는 `PartyId`와 목적이 겹칠 수 있다. 이미 만들어진 그룹 개념이 있다면 그걸 그대로 쓰고 싶다 — 새로 필드를 추가하기 전에 `GroupID`가 이 문서의 `PartyId`(그룹 크기·리더·아카이브 등을 포함하는 `FFF_Party`)와 같은 역할인지, 아니면 다른 목적(예: 시각적 군집화)인지 먼저 확인 부탁드립니다.

## 1. 역할 분담

"그룹을 어떻게 구성할지 결정"과 "그 결정대로 실제로 스폰·이동시키는 것"은 다른 영역이다 — 목적지 선택기가 "어디로 갈지 결정"과 "실제로 걸어가는 것"을 나누는 것과 같은 구조.

- **결정 로직 (김시리)**: 그룹 크기·유형·연령 조합·아동 배치·목적지를 결정하는 순수 로직/데이터. 그룹 생성 규칙은 `population_group_generation_spec.md`에 있지만 **실제로 그룹을 만들어내는 코드는 아직 없음** — 다음 작업.
- **실행부 (crowdAI)**: 그 결정 결과를 받아서 실제 `AActor`/`FUnitData`를 스폰하고, `TargetLocation`을 따라 움직이게 하는 것.

## 2. `FUnitData`에 필요한 필드 (통합 요청)

| 필드                                    | 타입            | 용도                                                                                                                                   |
| ------------------------------------- | ------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| `ArchetypeId`                         | FName         | 5종 아키타입 중 하나. 배정 비율은 `data/derived/npc_archetypes_v0.csv`의 `population_share_pct`(`resident_neutral_v0` 시나리오 기준으로 시작)                |
| `CompanionType`                       | FName         | 그룹 크기 분류(`solo`/`pair`/`group_3_plus`). 디버그 색상 구분용                                                                                   |
| `PartyId`                             | int32         | 그룹 식별자. `FF_Party.h`의 `FFF_Party::PartyId`와 동일 값 — **단, 위 확인 필요 항목 참고: `FCrowdGroupTypes::GroupID`가 이미 같은 역할이면 새로 안 만들고 그걸 그대로 씀**   |
| `bIsGroupLeader` (또는 `LeaderAgentId`) | bool 또는 int32 | 목적지 결정은 리더만 하고 나머지는 상속(4절)                                                                                                           |
| `StableAgentId`                       | int32         | 개체 안정 ID. **값은 저희가 계산해서 그룹 데이터와 함께 넘긴다** — `InstanceIndex`는 ISM 렌더링용이라 재사용될 수 있어 못 쓴다. 정원 님 쪽에서 새로 만들거나 저장·복원을 관리할 필요 없음, 필드만 추가하면 됨 |

`FFF_Party`(`FF_Party.h`) 전체를 복제할 필요는 없다 — 위 필드면 충분하다. `EFF_PartyComposition`(커플/친구/부모+아동/가족 구분)은 이동·스폰에 영향이 없어 안 넘겨도 된다.

**색상·라벨 표시**: `ArchetypeId` 5종 색상표는 crowdAI 쪽 스폰 테스트 코드에 로컬로 두고, 프로덕션 DataTable(`FFF_ArchetypeRow`)에는 색상 필드를 추가하지 않는다(디버그 전용이라 실제 데이터 스키마를 오염시키지 않기 위함). `ArchetypeLabelDraft`(한글 표시명)는 UI 담당자 논의 전 가칭이라 **플레이어 노출 UI에는 아직 쓰면 안 됨** — 내부 디버그 표시로만 사용.

## 3. 목적지 전달 방식 — 순위·점수는 절대 안 넘어간다

**정원 님 쪽으로는 언제나 "이미 결정된 목적지 1개"만 넘어간다.** `FFF_UtilityDestinationSelector::Choose()`가 후보 평가와 softmax 선택까지 전부 끝내고, 후보별 점수(`AllCandidateScores`)는 디버그·로그용(Ground에서 NPC 클릭 시 "왜 여기 왔는지" 설명)일 뿐이다. Crowd로 넘어가는 건 `FFF_MoveCommand` 하나뿐이다.

목적지에 못 가면 순위 2~3위를 미리 들고 있는 게 아니라 **실패를 이벤트로 돌려받아 다시 결정**한다:

```
Utility AI → FFF_MoveCommand(목적지 1개) → Crowd
Crowd가 실패 → FFF_MoveResultEvent(PathFailed/Blocked 등) → Utility AI
Utility AI가 Choose()를 다시 호출(실패한 후보는 배제) → 새 FFF_MoveCommand
```

이 구조로 정원 님 쪽 코드는 점수·순위·선호도를 전혀 몰라도 된다 — `EFF_MoveResult` 4가지(Arrived/PathFailed/Blocked/Interrupted)만 보고 성공/실패를 알려주면 된다.

### `FFF_MoveCommand`/`FFF_MoveResultEvent` (`FF_MoveCommand.h`)

- **받는 것**: `PartyId`, `TargetDestinationId`(FName), `TargetEntryPointId`(FName), `Intent`(Visit/Wander/Exit/Evacuate), `CommandId`. `TargetDestinationId`는 ZoneSystem 시설 인스턴스 `FGuid`를 문자열로 바꾼 값(`FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId`/`TryParseDestinationId`로 상호 변환 가능). **다만 그 값을 실제 월드 좌표로 바꾸는 조회는 아직 누가 할지 정해지지 않았다** — 정원 님·한광훈 님과 함께 정할 문제.
- **돌려줄 것**: `PartyId`, `CommandId`(받은 그대로), `Result`, `SimTimeMinutes`. **폴링 금지** — 유닛이 `TargetLocation`에 도달하거나 실패가 확정되면 이벤트로 알려준다(델리게이트든 콜백이든 방식은 자유).
- `TargetEntryPointId`는 퇴장 지점과 반드시 분리해야 한다 — 겹치면 데드락이 생긴다. 시설 데이터(한광훈)에 EntryPoint/ExitPoint가 따로 있어야 한다.

## 4. 그룹 생성 세부규칙 — 스폰 코드에 영향 주는 부분

1. **스폰은 그룹 단위로 일괄 처리** — 그룹원을 시차 없이 한 번에 스폰. `AFF_SpawnActor::SpawnUnits`를 그룹 크기만큼 한 배치로 호출.
2. **목적지는 그룹 리더만 결정, 나머지는 상속** — 그룹원(리더 아님)의 `TargetLocation`은 리더의 `TargetLocation`을 그대로 따라간다. 각자 독립적으로 Utility를 계산하지 않는다. ("임시 분리 후 재합류"는 v1 이후 후보.)
3. **도착 시각(`arrival_time`)은 그룹 공유 값** — `FFF_Party::ArrivalTimeMinutes`(단위: 현실 분).
4. **그룹 크기**: `group_3_plus`는 3명 또는 4명(열린 상한 없음). 비율(70%/30%)은 draft_assumption.
5. **이동속도·체류시간 공유**: `walk_speed_multiplier`/`stay_duration_multiplier` 이 두 개만 그룹 단위로 공유하고 나머지 4개(취향·인내심·혼잡내성·재평가주기)는 개체별 독립. `FFF_DeterministicRandom::Resolve()`를 그룹ID로 한 번 더 호출해서 이 두 필드만 가져다 쓰면 된다 — 코드 추가 없이 기존 API 재사용. 범위값은 `data/unreal/individual_variation_knobs_v0.csv`에 있음.

`CohesionRadiusMeters`(그룹이 이 반경 밖으로 벌어지면 재집결)는 값을 아직 안 정했다 — 이동 스택(Detour Crowd냐 Mass Entity냐)에 따라 적당한 값이 달라져서, 스택 선택이 먼저 필요하다.

## 5. 셀 격자 밀도 로그 요청

Heat Map·밀도 지표·AT-016(Fundamental Diagram 검증)에 쓸 원시 로그. `FFF_CrowdCellSample`(`FF_CrowdReadModel.h`)이 기준 구조체: `CellX`, `CellY`, `Occupancy`, `Throughput`, `DensityPerSqm`.

- 5m×5m 격자로 맵을 나누고, 인원 0인 칸은 기록 안 해도 됨(실측 레벨 전역이면 대부분 빈 칸).
- 기록 주기는 `FFF_CrowdReadModel::SnapshotFrame`과 맞추면(초안 30틱) 결정 로그와 조인 가능.
- **정해야 할 것**: 좌표 원점(레벨 원점 vs 특정 앵커), 셀 크기(5m 제안, 성능에 맞춰 조정 가능).
- `FFF_CrowdReadModel::ZoneAreaSqm`은 이미 `AFFZSGameplayZone::GetBoundaryAreaSquareMeters()`(한광훈 플러그인)에 있어 새로 만들 필요 없음 — ZoneReadModel에 노출만 되면 됨.

## 6. 성능 숫자(300/500/700)에 의존하지 말 것

팀 내 대량 크라우드 경험이 없어서 감으로 잡은 체크포인트 숫자다. "피크에도 700을 못 넘긴다" 같은 제약 의도는 없다. 스폰 코드에 상한으로 하드코딩하지 말고 DataTable/설정값으로 빼두면 나중에 숫자만 바꿀 수 있다.

대신 **판단(Utility AI)·이동(Crowd)·렌더링 3개 층을 분리해서 실측** 요청:

1. 판단 없이 존재만 하는 에이전트를 몇 개까지 렌더링할 수 있는가(ISM 기준)
2. 판단 없이 NavMesh로 계속 걸어 다니게만 하면 몇 개까지 버티는가
3. 300/500/700에서 실제로 막히는 지점이 이동인지 판단인지 — Utility AI 판단 비용은 `DecisionBudgetPerTick`으로 상한이 걸려 있어 이론상 병목이 아닐 가능성이 높음(30Hz 틱 기준 처리량 계산 시 인구 115,000명 수준까지 감당 가능).

## 7. 아직 안 된 것 (김시리 쪽)

`population_group_generation_spec.md`에 규칙은 정해졌지만, 그룹 크기·유형·그룹원 아키타입·아동 슬롯 수를 결정해 "이 그룹은 이런 구성이다"를 돌려주는 순수 함수는 아직 없다. crowdAI는 그 결과(데이터)를 받아 위 4절대로 스폰만 하면 된다 — 그룹 배정 로직까지 새로 만들 필요는 없다.

## 요약 — crowdAI에 보낼 메시지

> 1. 스폰 테스트에 `ArchetypeId`(5종, `npc_archetypes_v0.csv`의 population_share_pct 비율로) 배정해서 색상/라벨로 구분해줄 수 있나요? 색상표는 크라우드 쪽 디버그 코드에만 두고 DataTable엔 안 넣는 걸 추천해요.
> 2. `FCrowdGroupTypes`(`GroupID`/`GroupName`)를 이미 만들어두신 걸 봤어요 — 이게 저희가 말하는 그룹(리더 상속·목적지 공유 단위)과 같은 역할인가요? 같으면 그대로 쓰고, 다르면 `FUnitData`에 `PartyId`(int32)/`bIsGroupLeader`/`StableAgentId`(int32)/`ArchetypeId`/`CompanionType` 필드를 추가해주실 수 있을까요? `StableAgentId` 값은 저희가 그룹 데이터를 넘길 때 채워서 드릴게요 — 새로 만들거나 관리하실 필요 없어요.
> 3. 그룹 구성을 결정하는 코드는 저희가 만들 예정이에요 — crowdAI는 그 결과를 받아서 그룹 단위로 일괄 스폰, 그룹원 목적지는 리더 것을 따라가는 구조로 부탁드려요.
> 4. 목적지 이동은 `FF_MoveCommand.h`의 명령·이벤트 구조체로 주고받아요. 항상 "이미 결정된 목적지 1개"만 가고, 도착/실패는 폴링 대신 이벤트(`FFF_MoveResultEvent`)로 알려주시면 돼요.
> 5. 셀 격자 밀도 로그(`FFF_CrowdCellSample` 기준)를 만들어주실 수 있을까요? 좌표 원점·셀 크기만 같이 정하면 될 것 같아요.
> 6. 300/500/700이라는 숫자에 큰 의미를 두지 마세요 — 저희도 감으로 잡은 거예요. 대신 판단/이동/렌더링을 나눠서 각각 몇 개까지 버티는지 실측해볼 수 있을까요?
