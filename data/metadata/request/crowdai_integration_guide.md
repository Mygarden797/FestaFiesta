# crowdAI ↔ Utility AI 연동 안내

이정원님께 전달할 요청과 변경 사항입니다. 갱신: 2026-09-08.

## 승인된 작업 — Q2·Q3·Q4 반영

정원님 승인 범위에서 예약 NPC 흐름을 연결했습니다.

- 자유활동 → 예약 시설 복귀 → 대기 → 참여 → 자유활동 순서입니다.
- 돌아올 시간이 부족한 후보는 제외합니다. 통과 후보가 없어도 복귀 마감 전에는 배회·재탐색하며 `no_choice`를 누적하지 않습니다. 고정 스텝에서 직접 이동시간 기준 복귀 마감을 검사하고, 마감에 도달하면 후보 유무와 무관하게 복귀합니다. `ProgramReturnBufferMinutes` 기본 10축제분을 복귀 마감과 후보 귀환 판정에 공통 적용합니다. 도착 후에는 대기합니다.
- 시설까지의 이동시간을 기준으로, 일반 시설로 이동·대기 중이어도 복귀 시점을 확인합니다.
- 대기·참여 중 일반 재결정은 막고, 예약 종료 전 개인 퇴장은 차단합니다. 행사 전체 퇴장은 차단하지 않습니다.
- 참여 중에도 Need는 증가하며 체류 예산만 멈춥니다. 종료 시 체류 유예를 1회 적용합니다(`ProgramExitGraceMinutes`, 임시 기본값 10축제분).
- 예약 시각은 Stage의 `TryGetLiveElapsedMinutes`로 읽습니다. 이동과 Queue 대기시간은 `StageSecondsToFestivalMinutes`로 같은 축제분 단위에 맞춥니다.

### 예약자 전용 시설

사용자 확정: 프로그램이 편성된 시설은 그날 예약자 전용입니다. 프로그램 종료 후에도 일반 시설로 개방하지 않습니다.

- 일반 목적지 후보에서 제외하고, 이미 향하던 일반 파티는 Queue 가입 시 차단합니다.
- 예약자는 일반 서비스 Queue에 가입하지 않습니다. 시설에 저작된 Queue 슬롯 좌표만 대기 위치로 사용합니다.
- 현재는 예약 파티 ID 순서로 리더의 슬롯을 배정하고, 일행은 기존 그룹 대형을 유지합니다. 사람별 일렬 대기나 FIFO 서비스는 아닙니다.
- 슬롯이 부족하면 시설 접근점으로 대체합니다. 저작 범위 밖으로 줄을 임의 연장하지 않습니다.
- 실제 에셋 확인: SandArt 공방은 12슬롯·90cm 간격, ExperienceStage(DanceBattle)는 12슬롯·115cm 간격입니다. 기본 프로그램의 예약 그룹 수(공방 2, 요가 3)보다 많습니다.
- 참여 중 메쉬 안으로 이동하거나 입구에서 숨기는 연출은 미정입니다. 현재는 대기 위치에서 참여 상태를 유지하고 종료 후 시설 출구로 이동합니다.

### 변경 접점

| 파일 | 변경 |
|---|---|
| `FF_FestivalCrowdTypes.h` | 예약 컨텍스트, 파티별 예약 상태. 복귀 의도 enum은 제거 |
| `FF_CrowdDecisionSubsystem.cpp` | 예약자 전용 후보 제외, 귀환 가능 후보 필터 |
| `FF_CrowdSimulationSubsystem.*` | 복귀·대기·참여·종료 전환, 퇴장 차단, 종료 유예 |
| `FF_ZoneSystemQueueBridgeSubsystem.*` | 대기 좌표 읽기, 예약 시설 일반 Queue 가입 차단 |
| `FF_ProgramReservationSubsystem.*` | 편성 시설 조회(`IsProgramFacility`) |

일반 Queue의 서비스·만석·포기 규칙과 시설 에셋은 변경하지 않았습니다.

### 검증

- 예약·Queue 관련 9개 테스트와 프로젝트 데이터 검증 통과.
- 전체 회귀: FestaFiesta 540/541, FF_ZoneSystem 48/48 통과. 기존 `Probe.NavMeshBakeStatus`의 Test_Main 지형 연결 실패 1건은 유지됩니다.
- 실제 플레이에서 대형·참여 연출은 아직 확인하지 않았습니다.

## 개체 변주 소비 결선 (2026-09-08)

개체 변주 8종 중 실제로 쓰이던 것이 2종뿐이었습니다. 남은 소비 지점을 연결했습니다.

| 특성 | 소비 지점 | 상태 |
|---|---|---|
| `walk_speed` | `FUnitData::Speed`/`MaxSpeed`/`CruiseSpeed` | 기존 |
| `stay_duration` | `PartyRemainingStayMinutes` | 기존 |
| `patience` | 줄 이탈 임계 (`AdvancePartyQueueOnArrival`) | **신규** |
| `reevaluate_interval` | 주기 재평가 (`StepSimulation` 리더 분기) | **신규** |
| `crowd_tolerance` | Relocate 임계 (`ResolveFallbackAction`) | **신규 — 단 Q5 대기, 아래 참조** |
| `novelty_seeking` | revisit 페널티 (`Choose`) | **신규** |
| `preference_strength` | 취향 벡터 (`GetScaledPreferenceVector`) | **신규** |
| `leisureliness_axis` | 직접 소비처 없음 | 설계상 정상. `patience`·`stay_duration`이 균등난수 단계에서 공유하는 축이라 그 두 값에 이미 반영돼 있습니다(knobs CSV의 `affects_field = none`) |

특성값은 스폰 시 한 번 Resolve 해서 `AgentTraits`·`AgentPersonalThresholds`(둘 다 **AgentIndex 인덱싱**)에 보관합니다. 결정론이 보장되니 매번 다시 뽑아도 값은 같지만, 그러면 결정 한 번에 변주 DataTable 조회가 8회 붙습니다.

### Crowd 쪽에 새로 생긴 행동 두 가지

**줄 이탈(reneging).** `AtSlot` 상태에서 개인 인내심 한계를 넘기면 줄을 떠나고 목적지를 다시 고릅니다. 기준점은 `QueueEnteredSimTime`(줄 가입 시각)이라 슬롯까지 걸어간 시간이 포함됩니다. 임계는 `wait_threshold_minutes` 15 × `patience_multiplier` 0.7~1.3이고, 현재 `Temp_FestivalMinutesPerRealSecond = 1.0`이라 실제로는 **10.5~19.5초**에 발화합니다.

> ⚠️ 이탈은 `ClearPartyQueueState(.., bLeaveQueue=true)`를 **먼저** 호출해 Queue에 실제 탈퇴를 알린 뒤 재결정을 겁니다. 순서를 바꾸면 안 됩니다 — `RequestDecision`이 내부에서 부르는 것은 `bLeaveQueue=false`라 로컬 상태만 지우고, Queue는 그 파티를 계속 들고 있어 자리가 영영 비지 않습니다(I-9와 같은 문제).

**주기 재평가(jockeying).** 걷는 도중 개인 주기가 차면 목적지를 다시 고릅니다. `reevaluate_interval_seconds` 120 × 0.8~1.2 = 96~144초. 큐·프로그램 구간과 이미 결정 대기 중인 파티는 제외합니다.

둘 다 `FF.Crowd.Variation [인내심 0|1] [주기 0|1]`로 런타임에 끄고 켤 수 있습니다(재스폰 불필요). 아무 일이 없어도 결정을 만들어내는 경로라, 결정 큐가 넘칠 때 원인을 가르려면 꺼서 대조할 수 있어야 한다고 판단했습니다.

### 변경 접점

| 파일 | 변경 |
|---|---|
| `FF_FestivalCrowdTypes.h` | `EFF_DecisionReason`에 `Reevaluated`·`QueuePatienceExpired` 추가. `FFF_PartyDecisionContext`에 개체 변주 3개 필드 추가 |
| `FF_CrowdSimulationSubsystem.*` | 특성값 보관 배열 2개, 줄 이탈, 주기 재평가, 스위치·계수기 2종 |
| `FF_CrowdDecisionSubsystem.*` | Zone 밀집도 조회, `GetScaledPreferenceVector`, novelty 전달, `QueuePatienceExpired` 감점 |
| `FF_CrowdDebugCommands.cpp` | `FF.Crowd.Variation` 추가. `FF.Crowd.Stats`·`FF.Crowd.Queue`에 계수기 표시 |

일반 Queue의 서비스·만석·포기 규칙, 이동·조향, 시설 에셋은 변경하지 않았습니다.

### 검증

- 전체 회귀: FestaFiesta **514/559**, FF_ZoneSystem 48/48 통과.
- 실패 5건 중 3건(`AllDefinitionsAreProjectable`, `InTransitCountsAccumulateWithinBatch`, `Probe.NavMeshBakeStatus`)은 이 작업 전 HEAD에서도 실패하던 에셋·NavMesh 환경 문제입니다. 나머지 2건은 같은 워킹트리에 있던 필수시설 게이트 작업 쪽이며 이 변경과 무관합니다.
- 취향강도 결선 위치를 고정하는 테스트 1개를 추가했습니다(`PreferenceStrengthScalesBeforeClamp`).
- 인게임 확인은 아직입니다.

### crowd_tolerance는 배선만 됐고 아직 발화하지 않습니다

`ResolveFallbackAction`이 이제 하드코딩 0 대신 `FFFZSZoneReadModel::DensityRatio`를 읽고 개인 임계와 비교합니다. 그런데 **`DensityRatio`는 항상 0입니다** — `UFFZSWorldSubsystem::ReportZonePopulation`을 부르는 곳이 프로젝트 전체에 없어서(`DensityRatio = ReportedPopulation / SoftCapacity`) 분자가 채워지지 않습니다.

즉 Q6의 "파티별 보관·컨텍스트 전달" 요청은 해결됐고, 남은 것은 **Q5(존 인구 보고 주기)** 하나입니다. 그것이 붙는 순간 별도 작업 없이 동작합니다.

덧붙여 `EFF_PartyFallbackAction::Relocate`는 Simulation에 전용 분기가 없어 현재 배회와 동일하게 처리됩니다. 인접 Zone 이동을 실제로 구현할지는 Q5 연결 후 `GetRelocateRequestedCount()` 수치를 보고 정하는 것이 좋겠습니다.

## 인게임 측정 결과 — 원인은 시설 밀도였습니다 (2026-09-08)

개체 변주 결선을 끝내고 Test_Main 에서 실측했습니다. 처음에는 결정의 96%가 배회(`no_choice`)로 끝나 계수 문제로 의심했는데, **시설을 더 촘촘하게 설치하니 계수를 하나도 바꾸지 않고 해결됐습니다.**

| | 시설 4개 (듬성) | 시설 촘촘 |
|---|---|---|
| 시설 좌표 지급률 | 3.7% | **73%** |
| no_choice 비율 | 96.3% | **27%** |
| 퇴장: 체류 소진 (정상) | 0 | **2** |
| 퇴장: 인내 한계 (비정상) | 35 | **0** |

두 번째 열이 `distance_ref_meters=400`·`in_transit_ref_count=12`(둘 다 기본값) 그대로 낸 결과입니다. 즉 계수가 틀린 것이 아니라 **배치가 그 계수의 전제를 벗어나 있었습니다.**

### 배치 담당자께 — 이게 실질적인 배치 제약입니다

`Attraction` 항이 아직 0이라(`attraction_norm` 미도입) 시설 효용의 플러스 항은 `Preference`(±0.3)와 `Need`(최대 1.8×급함)뿐입니다. 반면 거리 감점은 `distance_ref_meters`(400m)에서 -0.8 까지 갑니다.

**따라서 군중 위치에서 400m 를 넘는 곳에 시설을 두면 배회(`no_choice` 효용 0.15)가 이깁니다.** 시설이 듬성하면 NPC 가 아무 데도 안 가고, 그 상태가 길어지면 연속 `no_choice` 한계로 퇴장해 인원 보존까지 깨집니다(위 표의 35건).

`attraction_norm` 이 붙으면 이 제약이 완화됩니다. 그때까지는 **시설 간 간격을 400m 이내로** 유지해 주세요.

### 정원님 확인 부탁드리는 것 3건

배치를 고쳐 배회 문제는 풀렸지만, 아래 3건은 그와 별개로 남아 있습니다.

### 1. 대기열 서비스가 완료되지 않습니다 — 가장 급합니다

```
가입 시도        : 8
  수락           : 8
슬롯 목표 적용   : 0   <- 수락은 됐는데 이게 0이면 슬롯이 이동으로 안 넘어간다
해소 적용        : 0회 <- 서비스를 마친 파티가 있는데 0이면 배선이 끊긴 것이다
```

가입은 수락되는데 **슬롯 좌표가 Crowd 로 넘어오지 않고, 서비스 완료도 한 번도 없습니다.** 위 두 줄은 `FF.Crowd.Queue` 덤프 자신이 출력하는 진단 문구입니다.

⚠️ 이것 때문에 저희 쪽 신규 기능인 **인내심 이탈(reneging) 수치를 밸런스 근거로 쓸 수 없습니다.** 줄이 정상적으로 길어서 떠난 것이 아니라 서비스가 끝나지 않아 떠난 것일 수 있습니다. 이 배선이 먼저 살아나야 대기시간 임계(15축제분)가 적절한지 판단할 수 있습니다.

### 2. Q5 — 존 인구 보고가 여전히 미연결입니다

`UFFZSWorldSubsystem::ReportZonePopulation` 을 부르는 곳이 프로젝트 전체에 없어서 `FFFZSZoneReadModel::DensityRatio` 가 항상 0입니다(`DensityRatio = ReportedPopulation / SoftCapacity`).

결과로 `Relocate 요청` 이 계속 0이고, `crowd_tolerance_multiplier` 는 배선을 마쳤는데도 발화하지 못합니다. **호출 주기만 정해 주시면 저희 쪽 추가 작업 없이 동작합니다.**

### 3. NavMesh 가 구워져 있지 않습니다

`FestaFiesta.Probe.NavMeshBakeStatus` 가 상시 실패합니다. 경로 쿼리가 실패한 개체는 직선으로 걸어 건물에 붙으므로, 이 상태에서는 동선·혼잡 수치를 신뢰할 수 없습니다. `FF.Crowd.Stats` 의 `쿼리 / 실패` 비율을 함께 봐 주세요.

### 계수는 건드리지 않았습니다

듬성한 배치에서 `in_transit_ref_count` 를 12 → 60 으로 올려 보니 지급률이 3.7% → 12.7% 로 3.4배 올랐습니다. 그래서 처음에는 이 계수를 고칠 후보로 봤는데, **시설을 촘촘히 놓으니 12 그대로 73% 가 나왔습니다.**

시설이 늘면 목적지가 분산되어 한 곳당 이동 중 인원이 줄고, 그러면 `in_transit` 정규화가 포화되지 않습니다. 계수를 올려 감점을 약화시키는 것은 증상을 가리는 쪽이었고, 실제로는 배치가 원인이었습니다. **두 계수 모두 기본값(400 / 12)으로 커밋합니다.**

## 정원님 확인·협의 요청 — 이번 작업에서 미변경

| 항목 | 요청 |
|---|---|
| Q1 / I-5 · 풀 크기 | 일반 방문객 500명 + 예약 최대 28명에 맞춰 `AgentPoolSize`를 530으로 조정해도 될까요? 배치 액터의 저장값도 확인이 필요합니다. |
| Q5 / I-4 · 존 인구 | `ReportZonePopulation(ZoneId, Population, DeltaSeconds)`를 어느 주기에 호출할지 협의 부탁드립니다. 실제 보고 간격을 전달해야 합니다. |
| Q6 · 개체별 혼잡 내성 | **보관·전달은 완료했습니다**(2026-09-08). `AgentPersonalThresholds`에 보관하고 `FFF_PartyDecisionContext::PersonalDensityThresholdRatio`로 전달합니다. 남은 것은 Q5뿐입니다 — `ReportZonePopulation` 호출부가 없어 `DensityRatio`가 0이라 아직 발화하지 않습니다. |
| I-6 · 체류 예산 | `Temp_LiveStageMinutes=240`을 날짜별 Live 길이로 바꾸는 작업이 필요합니다. Day2·3은 300초입니다. 예약 조기 퇴장은 별도 가드로 차단했습니다. |
| I-9 · 일반 Queue 재결정 | Queue 소유 중 재결정 시 실제 탈퇴 없이 로컬 상태만 지우는 경로를 확인 부탁드립니다. 예약 복귀와 **인내심 이탈**(2026-09-08 추가)에서는 실제 탈퇴를 먼저 호출합니다. `RequestDecision` 자체의 경로는 그대로 두었으므로 이 항목은 계속 열려 있습니다 — 줄에 선 파티에 `RequestDecision`을 직접 거는 새 호출부가 생기면 같은 함정을 다시 밟습니다. |
| I-1 · 격자 원점 | 관련 테스트는 통과합니다. Heat Map 문서의 주의 문구를 해제하기 전에 원점 계산 수정 여부만 확인 부탁드립니다. |
| I-7 · 이동 계약 | 목적지 Z를 Crowd 높이 격자가 결정하는지, 비리더 속도 배수가 대형 유지용인지, 출구 반경 400UU가 의도한 값인지 확인 부탁드립니다. |
| C3·C4 · 그룹 | 리더 조회 경로와 `bEnablePartyGrouping`을 실제 플레이에서 끌 수 있는지 확인 부탁드립니다. |
| D2 · 유입 곡선 | 예약자를 앞쪽에 배치할 때 허용할 초반 누적 유입 오차 기준이 필요합니다. |

슬롯 좌표 검증은 통과했습니다. 그룹 대형끼리의 간격과 참여 연출은 플레이 화면에서 별도 확인이 필요합니다.
위 미승인 항목은 사용자 확인 후 수정하고 이 문서에 알리겠습니다.

## 예약 복귀 예측 개선 — 확인·협의 요청

현재 귀환 필터는 `CandidateUseMinutes`를 별도로 채우지 않고, 후보 시설에서 예약 시설까지의 거리와 직접 복귀 마감은 직선거리로 계산합니다. 시설 접근점이나 판정 정보가 없으면 해당 후보를 유지합니다. 따라서 현재 판정은 정시 도착 보장이 아니라 지각이 예상되는 후보를 제외하는 수준입니다.

| 담당 영역 | 확인·요청 |
|---|---|
| 시설·Queue | 현재 예상 대기시간에 파티의 서비스 완료까지 포함되는지 확인 부탁드립니다. 포함돼 있다면 이용시간을 다시 더하지 않고, 빠진 경우에만 예상 이용시간을 제공해야 합니다. |
| Crowd 이동·Nav | 기존 `FFF_WalkDistanceMatrix`가 제공하는 거리 범위와 시설 접근점 간 조회 지원 여부를 확인 부탁드립니다. 직접 복귀와 후보→예약 시설 이동에 사용할 경로 거리 또는 예상 이동시간 조회가 필요합니다. |
| 예약·Decision | 제공받은 이동·대기·이용시간을 축제분으로 맞춰 시작 시각·복귀 여유와 비교합니다. 판정 정보가 없는 후보는 예약자에 한해 제외하는 방향을 제안합니다. 후보가 없어도 복귀 마감 전에는 배회·재탐색합니다. |

진행 순서는 **Queue 시간 의미 확인 → 판정 불가 후보 처리 합의 → 기존 Nav·거리 행렬 재사용 범위 확인 → 필요한 조회 기능만 보완**입니다. 예약 쪽에 별도 거리 필드를 새로 만들기보다 이동 쪽이 거리·예상시간을 제공하고 예약 쪽이 방문 가능 여부를 판단하는 구조를 제안합니다.

경로 거리를 사용해도 혼잡으로 지연될 수 있으므로 복귀 여유와 이동 중 마감 재검사는 유지합니다. 경로 질의가 반복되지 않도록 기존 캐시 재사용·갱신 기준도 함께 확인 부탁드립니다. 위 내용은 개선 제안이며 아직 코드에는 반영하지 않았습니다.

## 연동 계약

- Utility는 목적지를 선택하고 Crowd는 이동·도착·그룹 유지·퇴장을 처리합니다.
- `OnPartyNeedsDecision(Context, Result)`는 일반 목적지 또는 배회 등 폴백을 전달합니다. 예약 복귀 전환은 Simulation이 맡습니다.
- `BeginArrivalSchedule(..., ReservedParties)`는 예약 명단을 일반 방문객에 더합니다. 다음 날 재개용 요청 인원에 예약자를 중복 합산하지 않습니다.
- 스폰 직후 `ConfirmReservedParty(ReservationId, PartyID, PartySize)`로 확정합니다. 시작 시각보다 먼저 확정돼야 프로그램 매출에 반영됩니다.
- 그룹은 통째로 입장·이동·퇴장합니다. 목적지 선택은 리더가 맡습니다.
- 일반 줄 대기 중에도 Need는 증가합니다. `RemainingStayMinutes`는 시계가 아니라 욕구에 따라 소모되는 체류 예산입니다.
- 재결정 사유(`EFF_DecisionReason`)는 **실패와 비실패를 구분합니다.** `MoveFailed`·`JoinRejected`·`QueuePatienceExpired`는 직전 목적지를 감점하고, `Arrived`·`Reevaluated`는 감점하지 않습니다. 사유를 새로 추가할 때 이 구분을 같이 정해 주세요 — 안 하면 못 갔던 곳을 그대로 다시 고릅니다.
- 개체 변주는 **Crowd가 스폰 때 확정해 Context에 실어 보냅니다.** 결정 계층은 다시 Resolve하지 않습니다. 같은 시드에서 값은 같지만, 결정마다 뽑으면 변주 DataTable 조회가 8회씩 붙습니다.
- 같은 Seed에서는 같은 배정·타이밍을 유지합니다. Seed에 `FMath::Rand()`나 프로세스별 `GetTypeHash(FName)`를 쓰지 않습니다.
- 시설 입구와 출구는 분리합니다. 일반 시설은 도착과 Queue 가입 성공을 별도로 처리합니다.

## 참고 자료

- [예약 프로그램 규칙](../spec/reservation_program_spec.md)
- [방문객 그룹 생성](../spec/population_group_generation_spec.md)
- [유입 곡선](../spec/visitor_arrival_curve_spec.md)
- [개체 변주](../spec/individual_variation_spec.md)
- 데이터 에셋: `Content/FestaFiesta/Data/`의 `DT_VisitorGroupSizeDistribution`, `DT_GroupChildComposition`, `DT_GroupCompositionRules`, `DT_VisitorArrivalCurve`, `DT_IndividualVariationKnobs`.
