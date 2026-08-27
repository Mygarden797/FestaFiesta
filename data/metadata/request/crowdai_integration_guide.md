# crowdAI ↔ Utility AI 연동 가이드 — 이 문서 하나로 시작하시면 됩니다

- 대상: **이정원** (crowdAI — `Source/FestaFiesta/*/CrowdSystem/`)
- 작성: 김시리 (Utility AI · 경제) / 갱신일 **2026-08-27**
- 🔴 **처음 여시는 거면 바로 아래 "전달드리는 이슈 7건"만 보시면 됩니다.** 나머지는 그 이슈의
  근거·배경이고, 급하지 않습니다
- 목적: Utility AI 쪽 문서가 8개로 흩어져 있어서 "뭘 봐야 하는지"부터 막히는 문제를 없앤다.
  **이 문서는 코드 지도다** — 어떤 파일이 무엇을 하고, 그중 정원님이 실제로 손댈 것은 어디인지만 담는다.
- **이 문서 하나로 연동에 필요한 건 다 담았다.** 본문(§1~§8)은 읽는 것, **부록 A~E**는 코드 쓸 때
  찾아보는 참조표(필드 목록·의사코드)다. 설계 근거("왜 이 비율인가")가 궁금할 때만 §9 의 원본 spec 을 본다

---

# 🔴 이정원님께 — 전달드리는 이슈 7건 (2026-08-27)

정원님, `origin/Dev`(`396319a`)를 빌드해서 코드를 읽고 전체 테스트를 돌린 결과입니다.
**코드 작업은 3건(I-2·I-3·I-4), 결정만 주시면 되는 것 2건, 회귀 확인 1건, 확인 1건입니다.**

| # | 이슈 | 종류 | 지금 무슨 일이 벌어지나 | 상세 |
|---|---|---|---|---|
| **I-1** | Crowd 테스트 10건이 새로 깨졌습니다 | 🔴 회귀 확인 | 스폰 부족·이동 0·밀도 0 이 한 덩어리로 나옵니다 | 아래 I-1 |
| **I-2** | `BeginArrivalSchedule` 에 인자 하나 | 🔴 코드 | 유료 프로그램이 **손님을 못 데려옵니다** | §8.2 · §8.3 |
| **I-3** | 스폰된 그룹 `PartyID` 통지 | 🔴 코드 | 예약 잠금·퇴장 금지를 걸 수 없습니다 | §8.2 · §8.3 |
| **I-4** | `ReportZonePopulation()` 호출 | 🔴 코드 | **혼잡 회피 행동이 한 번도 안 나옵니다** | §4 |
| **I-5** | `AgentPoolSize` 를 올릴지 | ⚪ 결정 | 예약 손님이 들어올 자리가 0입니다 | §8.2 |
| **I-6** | 체류 예산의 `240` 상수 | ⚪ 결정 | Day2·3 은 **전원이 20% 짧게 머뭅니다** | §7 |
| **I-7** | 확인만 3가지 (높이 격자 Z · 멤버 속도 · 출구 반경) | ⚪ 확인 | — | 아래 I-7 |

> **순서 제안** — I-1 을 먼저 봐 주시면 좋겠습니다. **나머지가 그 위에 얹히기 때문**입니다.
> 지금 헤드리스에서 5명 요청에 2명만 스폰되는 상태라, I-2·I-3 을 넣어도 **예약 손님이
> 제대로 들어왔는지 테스트로 확인할 방법이 없습니다.** 다만 **에디터 실플레이에서도 그런지는
> 제가 확인 못 했습니다** — 아래에 그 판단에 필요한 것만 정리했습니다.

## I-1 · 🔴 Crowd 테스트 10건 신규 실패 — 실플레이 영향 여부를 봐 주세요

**실측**: `396319a` 빌드(성공) 후 전체 스위트 → **456 통과 / 15 실패**, 크래시 없음.
경제·NPC·UI 는 전부 초록불입니다. 8/26 기준선은 5 실패였고, **늘어난 10건이 전부
`FF_CrowdSystemTests.cpp`** 입니다. 제 변경은 없습니다.

```
Crowd.Avoidance.CounterFlowPathOpens                 Crowd.World.ActorShortageDoesNotAffectMetrics
Crowd.Avoidance.MillingCrowdDoesNotGridlock          Crowd.World.ReadModelIsPublishedOnInterval
Crowd.Decision.ArrivalCooldownThrottlesRechoice      Crowd.World.ReleaseReturnsActorAndHidesIt
Crowd.Decision.InTransitCountsAccumulateWithinBatch  Crowd.World.TickMovesAgentsAndTheirActors
Crowd.Decision.StuckPartyGivesUpDestination
Crowd.Queue.SlotArrivalRadiusIsSmallerThanSlotSpacing
```

**증상 — 다섯 갈래로 보이지만 한 뿌리 같습니다** (원인은 제가 단정하지 않았습니다):

| 무엇 | 실측값 |
|---|---|
| 스폰이 모자란다 | `요청한 5명` → **2명** · `37명` → 35명 · `20명` → 19명 |
| Agent 가 안 움직인다 | `Agent가 실제로 이동했다` false · `평균 전진량이 양수다 (0UU)` |
| 밀도가 0 | `밀도 집계는 12명 전원` → **0** · `밀도가 0보다 크다` false |
| 격자 원점이 0 | `스냅샷 원점 = 격자 원점` 이 `-5000` 기대에 **0** |
| 도착 반경 | `시설 ID가 없는 목적지는 배회 반경을 쓴다` 가 `400` 기대에 **150** |

**판단에 도움이 될 것 같은 사실 3개**

1. **`Crowd.HeightField` 8건은 전부 통과합니다.** 높이 격자 로직 자체는 초록불입니다
2. 로그에 `높이 격자: 지면을 찾지 못했다(트레이스 1681회) — 개체 Z는 스폰 값 그대로 유지된다`
   가 **29번** 찍힙니다. 테스트가 쓰는 빈 월드에는 지면이 없으니 당연한데,
   **그 상태에서 스폰·이동까지 막히는 것이 의도인지**가 핵심인 것 같습니다
3. 시점상 오늘 병합된 높이 격자 커밋(`5c46c39` · `a2f90db`)과 겹칩니다.
   그전 커밋으로 되돌려 비교해 보진 않았습니다

**그래서 여쭙는 것은 하나입니다** — **에디터 실플레이(지면 있는 레벨)에서도 스폰이 모자라나요?**
- **아니라면** 테스트 하네스만 손보면 되는 일이고, I-2~I-4 는 그대로 진행하셔도 됩니다
- **맞다면** 이게 최우선입니다. 스폰이 모자라면 예약 손님도 같이 잘립니다

**재현** (경고를 에러로 취급하므로 빌드 통과 자체가 1차 검증입니다):

```
Build.bat FestaFiestaEditor Win64 Development -Project=<uproject> -WaitMutex
UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests FestaFiesta;Quit" \
  -unattended -nopause -nosplash -NullRHI -NoSound -stdout -FullStdOutLogOutput > full.log 2>&1
```
판정은 `full.log` 의 `Result={Success}` / `Result={Fail}` 개수로 합니다(종료 코드는 무시).
⚠️ `-ABSLOG` 는 상위 디렉터리가 없으면 **로그가 아예 안 생기는데 종료 코드는 같습니다** —
저는 이걸로 두 번 헛돌렸습니다. stdout 리다이렉트가 확실합니다.

> ⚠️ **`ReadModelIsPublishedOnInterval` 은 백종태 님께도 걸립니다.** 그게 Heat Map 의 유일한
> 원본이라 제가 `UI_API.md` 에 "실값이 나온다"고 적어 보냈는데, **격자 원점이 0 이면
> 셀 → 월드 환산이 통째로 어긋나서 Heat Map 이 엉뚱한 자리에 그려집니다.**
> 종태님께는 "이 테스트가 초록불로 돌아온 뒤에 붙이시라"고 caveat 을 달아 뒀습니다.

## I-7 · 확인만 부탁드리는 것 3가지

작업 요청이 아니라, 제가 코드를 읽고 **이렇게 이해했는데 맞는지**만 알려주시면 됩니다.

1. **높이 격자와 Z 소유권** — 제 쪽은 목적지를 XY 로만 넘기고 Z 는 안 봅니다.
   **`FFF_PartyDecisionResult::Destination` 의 Z 를 그쪽에서 격자로 덮으시는 것**으로
   이해했는데 맞나요? 제가 Z 를 맞춰 드려야 하면 알려주세요
2. **파티 멤버가 리더보다 빠릅니다** — `PartyMemberSpeedBoost`(1.15) ·
   `PartyMemberSpeedMultiplier`(1.3) 가 비리더에만 곱해집니다(`:375~378`).
   **대형 유지용 따라붙기**로 이해했습니다. 맞으면 제 계수 문서에 그렇게 적겠습니다
3. **`Temp_ExitArrivalRadiusUU` 가 400 입니다.** 출구 앞에서 뭉치던 것을 푸느라 키우신 것으로
   이해했습니다 — 맞는지만 알려주세요. 퇴장 판정은 제 쪽이라 이 값과 맞물립니다

---

## 근거 — 코드 어디를 보고 판단했는지

`origin/Dev`(`396319a`) 기준입니다. 제 판단이 틀렸으면 이 줄들을 반박해 주시면 됩니다.

| 이슈 | 근거 (파일:줄) |
|---|---|
| **I-2** | `BeginArrivalSchedule` 이 3인자(`FF_CrowdSimulationSubsystem.h:629`), `BuildVisitorDistribution`(`:2204`) 이 `Build` 를 6인자로만 부름 |
| **I-3** | Crowd 가 발행하는 델리게이트 0개. `ConfirmReservedParty` 호출부도 테스트에만 존재 |
| **I-4** | 함수는 있는데(`FF_FFZSWorldSubsystem.h:199`) **저장소 전체 호출부 0곳** → `FF_CrowdDecisionSubsystem.cpp:395` 의 `CurrentZoneDensityRatio = 0.f` 하드코딩 |
| **I-5** | `AFF_SpawnActor` 기본값 `UnitCount = 500` · `AgentPoolSize = 500` (`FF_SpawnActor.h:39,47`) |
| **I-6** | `FF_CrowdSimulationSubsystem.cpp:385` 가 `Temp_LiveStageMinutes`(240) 로 체류 예산 계산. 예약 예외 없음 |

**제 쪽 몫이라 요청에서 뺀 것 하나** — 개체 변주 7종 중 `WalkSpeedMultiplier`·
`StayDurationMultiplier` 2종만 스폰 때 소비되고, 나머지 5종(인내심·혼잡내성·취향강도·
재평가간격·여유도)은 `FFF_PartyDecisionContext` 에 안 실려서 버려집니다.
제가 채우겠습니다. 다만 **I-4 와 같이 봐야 의미가 있습니다** — 혼잡도가 들어와도
개인 임계(`PersonalDensityThresholdRatio`)가 1.0 고정이면 개체차 없는 일괄 회피가 됩니다.

---

## 1. 담당 경계

```
김시리 (Utility AI)                     이정원 (crowdAI)

스폰  방문객 태그 명단 ───────────────▶ 그룹·리더 지정·스폰
      DistributionBuilder::Build

      개체 특성값 ───────────────────▶ 스폰 시 소비
      ResolveWithGroupSharing            (WalkSpeed 등)

입장  시간대별 입장 비율 ─────────────▶ 실제 입장 타이밍
      DT_VisitorArrivalCurve

운영  목적지 결정 ───────────────────▶ 이동·경로·도착
      UtilityDestinationSelector         OnPartyNeedsDecision
                       ◀─────────── 도착/거절/포기 (EFF_DecisionReason)

      욕구·체류·퇴장 판정 ◀────────── 시설 이용 완료 (OnPartyExited)
```

**한 줄로**: 저는 **"누가 어디로 가야 하는가"를 계산**하고, 정원님은 **"실제로 그렇게 움직이게"** 합니다.

---

## 2. 접점 — 함수 네 개가 전부입니다

경로는 `Source/FestaFiesta/Public/` 기준.

**① 목적지 결정** — 정원님이 부르는 게 아니라, 제가 그쪽 델리게이트에 물려 있습니다.

```cpp
// UFF_CrowdSimulationSubsystem 가 소유. FF_CrowdDecisionSubsystem.cpp:43 에서 제가 BindUObject
Crowd->OnPartyNeedsDecision  →  bool(const FFF_PartyDecisionContext&, FFF_PartyDecisionResult&)
```

파티 스냅샷을 넘기시면 목적지를 채워 돌려드리고, **false 면 배회로 폴백**하시면 됩니다.
구조체 둘은 `CrowdSystem/Types/FF_FestivalCrowdTypes.h` 에 있습니다.

**② ~ ④ 정원님이 직접 부르는 함수**

| 파일 | 함수 | 언제 |
| --- | --- | --- |
| `NPC/Party/FF_VisitorDistributionBuilder.h` | `Build(...)` | 유입 시작할 때. 태그 붙은 명단이 나옵니다 (상세: 부록 C) |
| `NPC/IndividualVariation/FF_DeterministicRandom.h` | `ResolveWithGroupSharing(...)` | 스폰 시 1회 |
| `Plugins/FF_ZoneSystem/.../FF_FFZSWorldSubsystem.h:199` | `ReportZonePopulation(...)` | 🔴 **I-4 — 아직 호출부 0곳** |

**제가 채우고 정원님이 읽기만 하는 것**

- `NPC/Party/FF_NeedState.h` — 욕구 5종. `FUnitData::Needs` 에 들어 있습니다
- `NPC/Integration/FF_CrowdReadModel.h` — 밀집 스냅샷. `PublishCrowdReadModel()` 이 30스텝(1초)마다 채웁니다.
  **셀 한 변은 2m**(`FFF_CrowdMetrics::AnalysisCellSizeUU` = 200UU), 원점은 `GridOriginUU`

> ⚠️ **`NPC/Integration/FF_MoveCommand.h` 는 열지 마세요.** 쓰이지 않는 죽은 타입이라
> 제가 지우겠습니다. 목적지 전달은 위 ① 입니다.

---

## 3. ✋ 안 봐도 되는 코드 — 이쪽은 제 안쪽입니다

정원님 파트와 안 닿습니다. 궁금해서 열어보는 건 자유지만 **읽지 않아도 연동에 지장 없습니다.**

| 폴더 / 파일                                 | 무엇                               | 왜 안 봐도 되나                                     |
| --------------------------------------- | -------------------------------- | --------------------------------------------- |
| `NPC/Destination/` 3개 파일                | Utility 식 6항 계산, 취향 벡터 합성, 계수 조회 | 제 내부 계산. 정원님은 **결과(`FFF_PartyDecisionResult`)만** 받습니다 |
| `NPC/Party/FF_NeedRules.*`              | 욕구가 오르고 내리는 규칙                   | 제가 굴립니다. 완료 신호는 `OnPartyExited` 로 이미 연결돼 있습니다  |
| `NPC/Party/FF_VisitHistory.*`           | 최근 방문 5곳 (같은 부스 재방문 페널티)         | 위와 같음. 완료 신호 하나로 욕구 해소 + 방문 기록이 같이 처리됩니다      |
| `NPC/Party/FF_ProgramReservation*.*`    | 유료 프로그램 예약 배정                    | 배정은 제 결정적 함수가 합니다. 정원님께는 §7 D2 제약 하나만 걸립니다   |
| `NPC/Integration/FF_ZoneSystemBridge.*` | ZoneSystem(한광훈) 자료형 → 제 자료형 변환   | 한광훈 님과의 경계입니다. 단위 환산(cm↔m, 초↔분)을 여기서만 합니다     |
| `NPC/Metrics/`                          | 욕구 미해소율 집계 → UI                  | 백종태 님과의 경계입니다                                 |
| `*Tests.cpp` 전부                         | 자동화 테스트                          | 제 쪽 회귀 방지용                                    |

> **핵심**: 정원님이 실제로 코드를 부르는 지점은 **`ResolveWithGroupSharing` · `Build` ·
> `ReportZonePopulation` 세 함수**와 **`OnPartyNeedsDecision` 응답**뿐입니다. 나머지는 다 제 안쪽입니다.

---

## 4. I-4 상세 — `ReportZonePopulation()` 이 없으면 무엇이 0이 되는가

호출부가 0곳이라 결정 계층이 혼잡도를 **아예 못 받습니다.** 그래서 지금 이렇게 박혀 있습니다.

```cpp
// FF_CrowdDecisionSubsystem.cpp:395
const float CurrentZoneDensityRatio      = 0.f;   // ← ReportZonePopulation 이 없어서
const float PersonalDensityThresholdRatio = 1.f;  // ← 개체 변주가 안 실려와서 (제 쪽 몫)
```

두 값이 상수라서 **"사람이 많아 다른 데로 간다"가 한 번도 발화하지 않습니다.**
`Relocate` 는 나오지만 밀집 때문이 아니라 no_choice 누적 때문에만 나옵니다.

보고 주기는 몇 초여도 됩니다 — 정확도보다 **`DeltaSeconds` 에 실제 보고 간격을 넣는 것**이
중요합니다. ZoneSystem 이 그 값으로 시간 평균을 내서(`DensityWarningHoldSeconds` 등)
순간 스파이크로 경고가 뜨는 것을 막습니다.

---

## 5. 동작 계약 — 바꾸시면 시뮬레이션 결과가 달라지는 것들

코드로는 안 보이는 전제라 적어 둡니다. **작업 요청이 아니라 "건드리실 때 알고 계셔야 하는 것"** 입니다.

- **그룹은 항상 통째로 움직입니다.** 통째로 입장하고 통째로 퇴장합니다. 구현 방법은 자유이고,
  요구사항은 "쪼개지지 않는다"는 결과뿐입니다
- **줄에서 기다리는 동안에도 욕구는 계속 쌓입니다.** 의도한 것입니다 — 줄이 길면 배고픔이 더 쌓여
  다음 재평가에서 다른 선택이 나옵니다. **"대기 중엔 건너뛰기" 최적화를 넣으면 동작이 바뀝니다**
- **`RemainingStayMinutes` 는 시계가 아니라 예산입니다.** 해소 못 한 욕구가 쌓이면 시계보다 빨리
  줄어듭니다("줄이 길어서 떠났다"를 표현). 이 값으로 "몇 분 뒤 나가겠구나"를 읽으면 틀립니다
- **`Choose` 는 그룹당 1회**입니다. 아동과 비리더는 리더의 결정을 상속합니다
- **배정·타이밍은 결정적이어야 합니다.** 같은 Seed 면 항상 같은 결과 — **`FMath::Rand()` 금지.**
  특히 `PartyID` 가 실행마다 같아야 합니다(예약 배정 순서를 정하는 값이라, 스폰 순서처럼
  프로세스마다 달라지면 같은 Seed 인데 예약자가 바뀝니다)

`StableAgentId` 는 이렇게 만들면 결정적입니다:

```cpp
int32 ComputeStableAgentId(int32 GroupID, int32 OrdinalInGroup)
{
    return static_cast<int32>(HashCombine(GetTypeHash(GroupID), GetTypeHash(OrdinalInGroup)));
}
```

> ⚠️ **`GetTypeHash(FName)` 은 Seed 재료로 쓰면 안 됩니다.** FName 의 전역 표 칸 번호를 해시하는데
> 엔진이 그 값을 "process specific unstable int" 라고 명시합니다 — 에셋 로딩 순서가 바뀌면 값이
> 바뀝니다. 제 쪽에서 같은 버그를 겪어 `MakeStableNameHash`(내용 기반 FNV-1a)로 고쳤습니다.
> `int32` 해시는 안전합니다(위 코드처럼).
>
> ⚠️ `FFF_DeterministicRandom::MakeTraitSeed` 를 그대로 쓰면 안 됩니다 — 그 함수의 두 번째 인자가
> 이미 `StableAgentId` 라서, `StableAgentId` 자체를 만드는 함수가 아닙니다.

---

## 6. 데이터 — DataTable 5장만 보시면 됩니다

**CSV 는 제 원본·재생성용입니다.** 정원님은 `Content/FestaFiesta/Data/` 의 애셋만 참조하세요.

| 애셋                                | 구조체                            | 무엇                            | 언제                                       |
| --------------------------------- | ------------------------------ | ----------------------------- | ---------------------------------------- |
| `DT_VisitorGroupSizeDistribution` | `FFF_GroupSizeDistributionRow` | 그룹 크기 1~6 분포                  | 그룹 묶을 때 (`ScopeId="tourism_daytrip"` 행만) |
| `DT_GroupChildComposition`        | `FFF_GroupChildCompositionRow` | 크기별 아동 수 분포                   | 같음                                       |
| `DT_GroupCompositionRules`        | `FFF_GroupCompositionRuleRow`  | 나머지 규칙 11개 (리더 수·상속 범위·공유 특성) | 같음 — 값 전체는 **부록 B**                      |
| `DT_VisitorArrivalCurve`          | `FFF_ArrivalCurveRow`          | 시간대별 유입 **비율(%)**, 시나리오 3종    | 입장 타이밍 정할 때                              |
| `DT_IndividualVariationKnobs`     | `FFF_IndividualVariationRow`   | 변주 7종의 (Min, Mode, Max)       | ResolveWithGroupSharing 인자                                 |

**필드별 의미는 부록 B, 반환값 필드와 실전 의사코드는 부록 C, 유입 곡선 필드는 부록 D** 에 있습니다.

### 6.1 그룹 하나 만드는 순서

```text
1. DT_VisitorGroupSizeDistribution 에서 GroupSize 를 뽑는다
   (ScopeId="tourism_daytrip" 행만 사용 — 다른 값과 섞으면 비율 합계가 깨진다)
2. DT_GroupChildComposition 에서 그 GroupSize 의 ChildCount 를 뽑는다
3. ChildCount 명에게 bIsDependentChildSlot=true, 나머지는 성인
4. 성인 중 1명을 bIsGroupLeader 로 지정 (아동은 리더 불가)
5. 전원에게 같은 GroupID, StableAgentId 부여 (§5)
```

**1~3단계는 `FFF_VisitorDistributionBuilder::Build` 가 이미 대신 해줍니다** — 개체마다
`TargetGroupSize`/`TargetGroupChildCount`/`bIsDependentChildSlot` 이 미리 채워져서 나옵니다.
정원님이 실제로 하는 건 **4~5단계와 "누구를 누구와 묶을지"** 뿐입니다.

### 6.2 유입 곡선에서 헷갈리기 쉬운 것 3개

예시로 (총 유입 N=950, 어느 버킷이 `ArrivalSharePct=5.2`, `CumulativeSharePct=48.3`, `ProgressStart=0.4167`):

1. **비율(%)이지 인원이 아닙니다** — `5.2` 는 "전체 N의 5.2%". `5.2% × 950 ≈ 49명`처럼 **정원님이 정한 N**을
   곱해야 실제 인원이 나옵니다. N은 성능 실측으로 정원님이 정하는 값이라 제 쪽에서 알 수 없습니다.
2. **계산은 `ProgressStart`(0~1 진행도) 기준입니다** — `MinutesStartAtDefault`(=100분) 는 Day1
   기준으로 적힌 표시용 값이고, **Day2·3 에서는 틀린 값이 됩니다**(Live 가 240초가 아니라 300초).
   지금 코드는 `ElapsedLiveSeconds / GetLiveDurationForCurrentDay()` 로 맞게 되어 있습니다.
   **이 열을 계산에 끌어 쓰지만 마세요.**
3. **버킷별 오차는 정상이고, 맞춰야 하는 건 `CumulativeSharePct`(누적)입니다** — 그룹이 1~6명 단위로
   들어와서 버킷당 49명을 딱 맞추는 건 원리적으로 불가능합니다. 누적을 비교해 크게 어긋나면
   다음 그룹을 더 빨리/천천히 꺼내는 식으로 보정하면 됩니다.

**곡선 모양 수치는 전부 가정값**이라 특정 모양을 전제하지 않는 구현을 부탁드립니다.
구현·테스트 중엔 `flat_v0`(균등, 가장 단순) 권장.

---

## 7. I-6 상세 · 그 밖에 답을 기다리는 것

### I-6 — 체류 예산이 아직 `240` 상수입니다

유입 곡선 분모는 날짜별(`GetLiveDurationForCurrentDay()`)로 바뀌었는데,
**체류 예산(`FF_CrowdSimulationSubsystem.cpp:385`)은 아직 `Temp_LiveStageMinutes`(240)** 를 씁니다.

| Day | 실제 Live 길이 | 체류 예산 분모 |
|---|---|---|
| Day1 | 240초 | 240 — 일치 |
| Day2·3 | **300초** | 240 — **20% 짧게 뽑힘** |

들어오는 타이밍은 맞는데 **머무는 시간만 짧습니다.** 여기에도
`GetLiveDurationForCurrentDay()` 를 쓰시면 닫힙니다 —
⚠️ **단위 주의**: 그 함수는 **초**, 이 값은 축제 **분**입니다.

**같은 뿌리로 예약 손님이 조기 퇴장합니다.** 체류가 짧게 뽑히면 예약자가 프로그램
시작 전에 나가버립니다. 스폰 시점에 정해지는 값이라 **그쪽에서 예약 예외를 두는 게
자연스러운데, 제가 판정으로 막는 쪽이 나으면 그렇게 하겠습니다.**

### 그 밖에

| # | 질문 |
| --- | --- |
| C3 | **그룹 리더 조회 경로** — 목적지는 리더만 계산하고 나머지가 상속하는데, 리더 지정은 정원님이 하십니다. 이벤트인가요 조회 API 인가요 |
| C4 | **`bEnablePartyGrouping` 은 디버그 스위치인가요, 실제로 끄고 돌릴 수 있는 옵션인가요?** 끄면 전원 1인 그룹이 되어 정원 12 를 12 그룹으로 채우게 되는데, 그래도 되는 상황인지만 알면 됩니다 |
| D2 | **예약 그룹은 프로그램 시작 시각 전에 반드시 장내에 있어야 합니다.** 지금은 배열 앞쪽이라 개장 직후 도착해 여유가 충분한데, **예약석이 앞쪽에 몰리면 초반 버킷이 곡선보다 커집니다** — 누적 오차를 얼마까지 허용할지 기준이 필요합니다 |

**시각의 유일한 출처는 Stage 입니다.** 팀에서 *"각 시스템이 자체 시계를 들지 않고
`UFF_FestivalStageSubsystem` 을 거친다"* 로 정했습니다(시간 담당 강민서). 남아 있는
임시 상수는 `Temp_LiveStageMinutes`(240, 위 I-6) · `Temp_FestivalMinutesPerRealSecond`(1.0) ·
`Temp_LeaveTimeoutSeconds`(180) · `Temp_ExitArrivalRadiusUU`(400) 입니다.

**급하지 않은 것**: 셀 격자 밀도 로그, 판단·이동·렌더링 3층 분리 성능 실측,
`CohesionRadiusMeters` 값(이동 스택 확정 후).
방문객 300·500·700 은 감으로 잡은 체크포인트일 뿐 상한 의도가 아닙니다.

---

## 8. I-2 · I-3 상세 — 유료 프로그램이 방문객을 "데려옵니다"

플레이어가 프로그램을 사면 **그 프로그램을 좋아하는 아키타입으로 정원만큼 손님이 추가로
방문합니다.** 프로그램이 "유료 광고"가 되는 셈이고, 그래서 그 손님은 반드시 그 시설에 갑니다.

- **곡선 총량이 커집니다.** 오는 사람 중에서 고르는 것이 아니라 명단에 더 얹는 것이라,
  조합을 맞춰 고르는 문제가 없습니다 — **"명단에 몇 명 더 있다"** 가 전부입니다
- 예약 손님은 **아동 없이** 성인만, 그룹 상한 6명으로 쪼개집니다(정원 12 = 6+6, 16 = 6+6+4).
  인원 합은 항상 정원과 같습니다
- 프로그램이 둘이면 **손님도 둘 다 옵니다.** 같은 파티가 두 예약에 들어가는 것은 제 쪽에서 막습니다

### 8.1 현재 스폰 흐름

진입점이 둘입니다. **아래 부탁들의 전제가 이 그림입니다.**

```
① AFF_SpawnActor::BeginPlay()                  ← 레벨 BeginPlay, 1회
② HandleStageChanged(→ Live)                   ← 날마다. 저장해 둔 요청값으로 재호출
  └ BeginArrivalSchedule(TotalVisitorCount, SpawnCenter, SpawnRadiusUU)   ← 🔴 I-2 는 여기에 4번째 인자
      ArrivalRequestedVisitorCount = TotalVisitorCount   ← 다음 날 재개가 이 값을 다시 씁니다
      └ BuildVisitorDistribution(TotalVisitorCount)      ← 제 빌더. 순서가 고정된 배열이 나옵니다
  (매 틱) UpdateArrivalSchedule()
      Progress = ElapsedLiveSeconds / GetLiveDurationForCurrentDay()
      Target   = CumulativeSharePct(Progress)/100 × Total
      └ DrainVisitorUntil(Target)               ← 커서를 Target 까지 밉니다
                                                   PartyID 는 여기서 발급됩니다  ← 🔴 I-3 은 여기서 통지
```

> ⚠️ **재개가 쓰는 값은 `ActualVisitorCount` 가 아니라 `ArrivalRequestedVisitorCount` 입니다.**
> 그룹을 통째로만 만드느라 실제 인원이 요청보다 작은데, 그 작아진 값을 다시 넣으면 날마다 인원이
> 깎입니다. 그쪽 주석(`:259`)에 이미 적혀 있고 제 쪽도 그 전제로 계산합니다 — **I-2 인자를 넣으실 때
> 예약 인원을 이 값에 더하지 마세요.** 예약은 매일 새로 편성되는 별개 명단입니다.

여기서 제게 중요한 것은 **곡선이 "언제"만 정하고 "누가"는 배열 순서가 이미 정해두었다**는 점입니다.
그러면 "언제 도착하는가" = "배열 몇 번째인가" 라서, 프로그램 시작 시각을 인덱스로 바꿀 수 있습니다:

```
프로그램이 T분에 시작 → 그 전에 도착해야 함
MaxIndex(T) = floor( CumulativeSharePct(T)/100 × Total ) - 그룹크기
```

**즉 예약 그룹을 배열 어디에 꽂을지는 제가 표만 보고 계산할 수 있습니다.** 런타임 협의가 필요 없습니다.

### 8.2 부탁드리는 것

| # | 부탁 | 왜 |
|---|---|---|
| **I-2** | **`BeginArrivalSchedule` 에 인자 하나만 더 받아주실 수 있나요** | 배열은 제 빌더가 만들어 드립니다. **제 쪽 코드는 이미 다 들어가 있습니다** — 인자를 그대로 `FFF_VisitorDistributionBuilder::Build` 로 넘겨주시면 끝입니다. 커서 미는 방식은 그대로입니다. 정확한 형태는 **아래 8.3 배선도** 참고 |
| **I-3** | **스폰된 그룹의 `PartyID` 를 알려주는 창구** | 예약은 `PartyID` 로 잠금·퇴장 금지를 겁니다. 그런데 `PartyID` 는 스폰 순간에 생기니 제가 알 방법이 없습니다. 크라우드가 발행하는 델리게이트가 0개라 **새로 만들어야 하는 항목**입니다. **받는 쪽 함수는 이미 만들어 뒀습니다** |
| **I-5** | **`AgentPoolSize` 를 올려도 될까요 — 여유가 사실상 0입니다** | `AFF_SpawnActor` 기본값이 `UnitCount = 500` · `AgentPoolSize = 500` 으로 **똑같습니다**(`FF_SpawnActor.h:39,47`). 실측도 `요청 500명 중 498명`이었고, 남는 2자리는 그룹을 통째로만 만들어 생긴 우수리라 **설계상 여유가 아닙니다.** 프로그램 하나(정원 12)만 사도 넘치고, 넘치면 **곡선 뒷부분이 잘립니다**(그 Error 로그는 `:404` 에 이미 넣어두셨더군요). 프로그램은 최대 2개라 **`AgentPoolSize` 를 530 으로** 올리면 닫힙니다. `UnitCount` 를 470 으로 줄이는 쪽도 괜찮습니다 — 어느 쪽이든 정해주시면 됩니다 |

### 8.3 배선도 — 실제 함수 이름으로

🔴 **표시된 두 줄만 정원님 쪽 작업입니다.** 나머지는 제 쪽에 다 들어가 있습니다.

```
[아침]  Setup · MorningCheck — 플레이어가 프로그램을 산다
  UFF_FestivalProgressionSubsystem::TryScheduleProgram(ProgramId, FacilityInstanceId)
    └ Gold 차감 · Staff 묶기
    └ UFF_ProgramReservationSubsystem::RegisterProgram(...) → ReservationId

[그날 유입 시작]
  TArray<FFF_ReservedPartyRequest> Requests = Progression->GetReservedPartyRequests();
                                                                        ← 김시리 (완료)
  🔴 CrowdSim->BeginArrivalSchedule(UnitCount, Origin, Radius, Requests);   ← I-2
        └ FFF_VisitorDistributionBuilder::Build(..., Requests)
              → Visitors[] **앞쪽**에 예약 손님이 놓인다
                각 개체가 FFF_TaggedVisitor::ReservationId 를 달고 있다

[Live 중]  곡선이 커서를 민다 (지금 하시는 그대로)
  DrainVisitorUntil(Target) → 그룹을 묶고 PartyID 를 발급
  🔴 선두 개체의 ReservationId 가 유효하면:                                  ← I-3
        Reservations->ConfirmReservedParty(ReservationId, PartyID, PartySize);
        → 이 순간부터 아래 두 판정이 true 가 된다

[Live 중]  이동·퇴장 결정에서
  IsPartyLockedAt(PartyId, NowMinutes)      → true 면 목적지를 다시 고르지 않는다
  IsPartyExitBlockedAt(PartyId, NowMinutes) → true 면 퇴장시키지 않는다

[Live 중]  카드의 시작 시각(StartMinutes)이 지나면
  UFF_ProgramEconomyBridgeSubsystem → SettleStartedPrograms(NowMinutes)
    → 확정 인원 × 체험비를 원장에                                    ← 김시리 (완료)

[Closing]  못 걷은 것이 있으면 안전망으로 걷고 Warning              ← 김시리 (완료)
```

> ⚠️ **I-3(`ConfirmReservedParty`)가 프로그램 시작 시각(`StartMinutes`)보다 먼저 와야** 그
> 프로그램의 매출이 잡힙니다 — 체험비를 프로그램이 시작하는 순간에 걷습니다. 예약 손님이
> 개장 직후 도착하고 프로그램은 Live 30분·90분에 시작하므로 지금 설계로는 여유가 충분합니다.

**I-2 의 정확한 형태** — 기본 인자라 기존 호출부는 안 고쳐도 됩니다:

```cpp
// UFF_CrowdSimulationSubsystem
void BeginArrivalSchedule(
    int32 TotalVisitorCount, FVector SpawnCenter, float SpawnRadiusUU,
    const TArray<FFF_ReservedPartyRequest>& ReservedParties = {});
```

받은 값을 `BuildVisitorDistribution` 을 거쳐 `Build` 의 마지막 인자로 넘기시면 됩니다.
빌더 쪽은 이미 그 인자를 받고 있고, **비우면 이 인자가 생기기 전과 한 개체도 다르지 않습니다**
(`FestaFiesta.NPC.VisitorDistribution.EmptyReservedChangesNothing` 이 그것을 고정합니다).

**I-3 은 두 가지 중 편하신 쪽**:
① 스폰 직후 `ConfirmReservedParty` 를 직접 부르신다 (위 배선도)
② `OnPartySpawned(PartyID, LeadReservationId, MemberCount)` 같은 델리게이트만 열어주시면
   제가 구독해서 처리하겠습니다

**헤더**: `NPC/Party/FF_VisitorDistributionBuilder.h`(주문서·태그) ·
`NPC/Party/FF_ProgramReservationSubsystem.h`(확정·판정)

> ⚠️ **미결 하나.** 지금은 예약 손님이 배열 맨 앞이라 **개장 직후 도착**하고,
> 퇴장 금지는 프로그램이 **끝날 때까지**입니다. 선셋 요가(90~120분)면 0분에 와서 120분까지
> 못 나가서 하루의 절반을 묶여 있게 됩니다. 부스는 이용하니 매출에는 도움이 되지만
> 자연스러운지는 돌려봐야 알 것 같습니다. **곡선 오차 허용 폭(§7 D2)이 정해지면 도착을
> 뒤로 미뤄 좁힐 수 있습니다.** 배선을 붙이실 때 같이 보시죠.

---

## 9. 더 깊이 볼 때만 — 원본 문서 지도

**평소엔 안 보셔도 됩니다.** "왜 이렇게 정했나"가 궁금할 때만 여는 곳입니다.
경로는 `data/metadata/` 기준.

| 문서                                         | 언제                                       |
| ------------------------------------------ | ---------------------------------------- |
| `spec/population_group_generation_spec.md` | 그룹 크기·아동 구성 비율의 근거와 폐기 이력                |
| `spec/visitor_arrival_curve_spec.md`       | 유입 곡선이 왜 절대 인원이 아니라 비율인지                 |
| `spec/individual_variation_spec.md`        | 개체 변주 7종·삼각분포·그룹 공유 규칙의 근거               |
| `spec/dependent_child_npc_spec.md`         | 아동 슬롯이 왜 아키타입을 안 받는지                     |
| `spec/reservation_program_spec.md`         | 예약 규칙 전체 (D2 배경)                         |
| `request/facility_naming_convention.md`    | 시설 이름 규칙 `대분류_소분류_고유이름` (한광훈 님 파트지만 참고용) |
| 리포 루트 `UI_API.md`                          | UI(백종태)가 읽는 값                            |

**제 안쪽 설계**(`spec/utility_ai_spec.md`, `spec/utility_destination_selector_spec.md`)는
Utility 식·계수 표라서 연동과 무관합니다.

---

# 부록 (코드 쓰실 때 찾아보는 곳)

본문은 **읽는 것**, 부록은 **찾아보는 것**입니다. 처음부터 읽지 않으셔도 됩니다.

## 부록 A. `Status` 열의 신뢰도 라벨

DataTable 의 `Status` 열에 붙는 값입니다.

| 값                                 | 뜻                         |
| --------------------------------- | ------------------------- |
| `derived`                         | 실측 자료를 가중집계한 값            |
| `confirmed`                       | 검증된 사실                    |
| `assumption` / `draft_assumption` | **근거 없는 임시값 — 바뀔 수 있습니다** |
| `to_verify`                       | 아직 미확인                    |

## 부록 B. DataTable 필드 상세

`ScopeId` 나 `table_ref` 값(예: `visitor_group_size_distribution_v0`)은 제 쪽 내부 데이터셋
이름(snake_case)이라 `DT_` 애셋 이름과 표기가 다릅니다. 대응은 아래 규칙 표에 있습니다.

**`FFF_GroupSizeDistributionRow`**

| 필드                 | 뜻                      | 사용법                                |
| ------------------ | ---------------------- | ---------------------------------- |
| `ScopeId`          | 모집단 구분                 | **`tourism_daytrip` 행만 사용**        |
| `GroupSize`        | 그룹 인원수 1~6             | 뽑아서 그대로 그룹 크기로                     |
| `CompanionSizeTag` | solo/pair/group_3_plus | `FUnitData::CompanionSizeTag` 에 대입 |
| `SharePct`         | 뽑힐 확률(%)               | 같은 `ScopeId` 안에서 합계 100            |
| `bIsCapBucket`     | `GroupSize=6` 에서만 true | "6명 이상 전부"가 합쳐진 칸이라는 표시            |
| `Status`           | 신뢰도                    | 전부 `derived`                       |

**`FFF_GroupChildCompositionRow`**

| 필드                        | 뜻                              | 사용법                                  |
| ------------------------- | ------------------------------ | ------------------------------------ |
| `ScopeId` / `GroupSize`   | 위와 동일                          | 방금 뽑은 `GroupSize` 와 일치하는 행만 필터       |
| `ChildCount`              | 그룹 안 아동 수                      | 뽑아서 그대로 사용                           |
| `ShareWithinGroupSizePct` | 그 `GroupSize` **안에서** 뽑힐 확률(%) | 같은 `(ScopeId, GroupSize)` 안에서 합계 100 |

**`FFF_GroupCompositionRuleRow`** — 현재 규칙 11개

| `RuleId`                          | `Value`                                          | `ValueType` | `Status`       | 뜻                                          |
| --------------------------------- | ------------------------------------------------ | ----------- | -------------- | ------------------------------------------ |
| `group_size_cap`                  | `6`                                              | int         | derived        | 그룹 최대 인원                                   |
| `group_size_distribution_table`   | `visitor_group_size_distribution_v0`             | table_ref   | derived        | = `DT_VisitorGroupSizeDistribution`        |
| `child_composition_table`         | `group_child_composition_v0`                     | table_ref   | derived        | = `DT_GroupChildComposition`               |
| `child_age_definition`            | `under_15`                                       | enum        | derived        | 아동 슬롯 연령 기준                                |
| `leader_count_per_group`          | `1`                                              | int         | confirmed      | 그룹당 리더 수                                   |
| `leader_must_be_adult`            | `true`                                           | bool        | confirmed      | 리더는 항상 성인                                  |
| `destination_decision_scope`      | `leader_only`                                    | enum        | confirmed      | 목적지는 리더만 계산                                |
| `non_leader_adult_archetype_rule` | `same_as_leader`                                 | enum        | **assumption** | 리더 아닌 성인도 리더와 같은 아키타입. 실측 근거 없음            |
| `spawn_group_together`            | `true`                                           | bool        | confirmed      | 그룹원 전원 한 번에 스폰                             |
| `group_shared_traits`             | `walk_speed_multiplier;stay_duration_multiplier` | list        | confirmed      | 그룹이 공유하는 특성 2개                             |
| `child_trait_overrides`           | `movement_speed;attention_span;wait_tolerance`   | list        | **assumption** | 아동 전용 노브 3개, **값 미정** — 지금은 성인과 같게 두시면 됩니다 |

`ValueType` 파싱: `int`→정수, `bool`→문자열 비교, `enum`→고정 문자열,
`list`→세미콜론 split, `table_ref`→다른 DataTable 이름.

> `CohesionRadiusMeters` 는 이동 스택(Detour Crowd / Mass Entity) 확정 후 정합니다.

**표시 라벨** (백종태 님이 값을 정했습니다): 아키타입 `체험파`/`활동파`/`휴식파`/`탐방파`/`여유파`,
동반유형 `혼자`/`둘이서`/`여럿이서`. 열 이름에 `Draft` 가 남아 있지만 UI 담당이 정한 값이라
그대로 표시하셔도 됩니다. 색상·디버그 라벨은 crowdAI 쪽 디버그 코드에만 두고 프로덕션
DataTable 에는 넣지 않습니다.

## 부록 C. `FFF_VisitorDistributionBuilder` 상세

**비유**: 여행사 예약팀(저)이 손님 명단을 만들어 넘깁니다. 명단의 각 사람에게는
"4인 가족 중 한 명, 그중 2명은 어린이, 성인 취향은 30대형" 같은 메모(태그)가 붙어 있고,
"몇 인 가족이 몇 팀인지" 요약표도 같이 드립니다. 하지만 "이 4명은 한 팀, 이분이 조장"이라고
배차하는 건 현지 가이드(정원님) 몫입니다.

`NPC/Party/FF_VisitorDistributionBuilder.h`. 순수 함수라 같은
`(Seed, 요청 인원, ScopeId, ArchetypeScenarioId, 입력 테이블)` 은 항상 같은 결과를 냅니다.

```cpp
FFF_VisitorDistribution Dist = FFF_VisitorDistributionBuilder::Build(
    GroupSizeTable, ChildCompositionTable, ArchetypeTable,   // §8 의 DT_ 애셋 + DT_NpcArchetypes
    TEXT("tourism_daytrip"),      // ScopeId
    TEXT("resident_neutral_v0"),  // ArchetypeScenarioId — 기본값 그대로 두시면 됩니다
    RequestedVisitorCount, GlobalSimulationSeed);
```

> 네 번째 인자 `ArchetypeScenarioId`(`resident_neutral_v0`)는 **인구 구성 시나리오**입니다 —
> "방문객이 사하구 주민 구성과 비슷하다"는 가정이고 **제 쪽 결정 사항이라 신경 쓰지 않으셔도**
> 됩니다. §8 유입 곡선의 `FFF_ArrivalCurveRow::ScenarioId`(곡선 모양)와는 **이름도 뜻도 다른
> 별개 값**입니다 — 원래 둘 다 `ScenarioId` 였는데 헷갈려서 이쪽을 개명했습니다.

**`GroupID` 도 그룹 소속 번호도 제가 넣지 않습니다** — 누구를 누구와 묶을지는 정원님 몫입니다.
대신 개체마다 묶는 키 **`(TargetGroupSize, TargetGroupChildCount, ArchetypeId)`** 를 붙여둡니다.
같은 키의 성인 `TargetGroupSize - TargetGroupChildCount` 명 + 같은 `(크기, 아동수)` 의 아동
`TargetGroupChildCount` 명을 모으면 그룹 하나입니다. `ArchetypeId` 를 키에 넣은 덕분에
"같은 그룹 성인은 같은 아키타입"이 저절로 지켜집니다.

### 예시 (요청 10명)

| #    | ArchetypeId       | CompanionSizeTag | TargetGroupSize | TargetGroupChildCount | bIsDependentChildSlot |
| ---- | ----------------- | ---------------- | --------------- | --------------------- | --------------------- |
| 1~2  | life_stage_60plus | solo             | 1               | 0                     | false                 |
| 3~6  | life_stage_20s    | pair             | 2               | 0                     | false                 |
| 7~8  | life_stage_30_40s | group_3_plus     | 4               | 2                     | false (성인)            |
| 9~10 | `NAME_None`       | group_3_plus     | 4               | 2                     | true (아동)             |

이때 `GroupPlan` 은 `(크기1, 아동0, life_stage_60plus)→2개`, `(크기2, 아동0, life_stage_20s)→2개`,
`(크기4, 아동2, life_stage_30_40s)→1개`. 7~10번을 모아 4인 그룹 하나를 만들면 됩니다 —
몇 번과 몇 번을 묶을지는 자유입니다(위 번호는 예시일 뿐).

### `FFF_TaggedVisitor` 필드

| 필드                      | 용도                              |
| ----------------------- | ------------------------------- |
| `ArchetypeId`           | 생애주기 5종. **아동 슬롯은 `NAME_None`** |
| `CompanionSizeTag`      | solo / pair / group_3_plus      |
| `TargetGroupSize`       | 1~6. **그룹핑 시 봐야 하는 값**          |
| `TargetGroupChildCount` | 그룹 내 아동 수                       |
| `bIsDependentChildSlot` | 아동 여부                           |

### `FFF_VisitorDistribution` (반환값) 필드

| 필드                       | 뜻                              | 사용법                                  |
| ------------------------ | ------------------------------ | ------------------------------------ |
| `Visitors`               | 태그 붙은 개체 목록                    | 순회하며 그룹 구성                           |
| `GroupPlan`              | `(크기, 아동수, 아키타입) → 그룹 몇 개` 레시피 | 검산용으로도 사용 가능                         |
| `ActualVisitorCount`     | 실제 생성 인원                       | **요청보다 최대 5명 적을 수 있습니다 — 정상 동작**     |
| `DependentChildSharePct` | 실제 아동 비율(%)                    | **목표치가 아니라 결과값.** 맞추려고 조정하지 않으셔도 됩니다 |

**개체 수가 정확히 나누어떨어지는 것은 빌더가 보장합니다** — `GroupPlan` 이
`(크기, 아동수, 아키타입) → 그룹 몇 개` 를 알려주고 자동화 테스트가 검증합니다.
나머지가 남거나 모자라는 일은 없습니다.

### 실전 사용 흐름 (의사코드)

```cpp
FFF_VisitorDistribution Dist = FFF_VisitorDistributionBuilder::Build(/* 위와 같음 */);

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

큐에서 꺼내는 순서는 자유지만 **같은 Seed 에서 같은 순서가 나와야 합니다** — 배열 앞에서부터
순서대로면 충분하고, `FMath::RandRange` 로 무작위 추출은 금지입니다 (§5).

> ⚠️ `FFF_VisitorGroupPlanBucket`(그룹 버킷)과 §8 유입 곡선의 **시간 버킷**은 완전히 다른
> 개념입니다. 시간 버킷은 "언제"를, 그룹 버킷은 "누구끼리"를 답합니다. 같은 함수에 같이
> 나오는 일이 없어서 실제로 헷갈릴 일은 없습니다.

## 부록 D. 유입 곡선 필드 상세 (`FFF_ArrivalCurveRow`)

| 필드                                              | 타입              | 뜻                                                        |
| ----------------------------------------------- | --------------- | -------------------------------------------------------- |
| `ScenarioId`                                    | FName           | `flat_v0` / `single_peak_v0` / `sunset_peak_v0` 중 하나만 사용 |
| `BucketIndex` / `BucketCount`                   | int32           | 버킷 순번 / 전체 개수(24). **상수로 박지 말고 값을 읽으세요**                 |
| `ProgressStart` / `ProgressEnd`                 | float           | Live 진행도 [0, 1). **계산 기준**                               |
| `ArrivalSharePct`                               | float           | 이 버킷 신규 유입 비율(%)                                         |
| `CumulativeSharePct`                            | float           | 누적 유입 비율(%) — **실제로 맞춰야 하는 지표**                          |
| `MinutesStartAtDefault` / `ClockStartAtDefault` | float / FString | **표시용. 계산에 쓰지 마세요** (§6.2 ②)                             |

## 부록 E. 목적지·도착에서 지켜야 하는 제약 2개

접점 자체는 §2 ① 의 `OnPartyNeedsDecision` 이고, 아래 둘은 그 위에서 계속 유효한 제약입니다.

- ⚠️ **대기열 입구와 퇴장 지점은 반드시 분리해야 합니다.** 겹치면 나가는 사람과 들어오는
  사람이 서로를 막아 인위적인 데드락이 생깁니다. 시설 데이터(한광훈 님) 쪽에
  EntryPoint / ExitPoint 가 따로 있어야 합니다
- ⚠️ **"도착"은 "줄에 들어갔다"가 아닙니다.** 걸어가는 동안 줄이 꽉 찰 수 있어서 도착 판정과
  가입 판정을 분리합니다. 가입 실패는 재평가 요청으로 이어집니다 —
  `EFF_DecisionReason::JoinRejected` 로 이미 구현돼 있습니다

`AFFZSGameplayZone::GetBoundaryAreaSquareMeters()` 결과가
`FFFZSZoneReadModel::AreaSquareMeters` 로 노출됩니다. `FFF_CrowdReadModel::ZoneAreaSqm` 이
필요하면 이 값을 ZoneId 별로 복사하시면 됩니다.

---
