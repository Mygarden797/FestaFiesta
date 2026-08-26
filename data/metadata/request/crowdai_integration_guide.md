# crowdAI ↔ Utility AI 연동 가이드 — 이 문서 하나로 시작하시면 됩니다

- 대상: **이정원** (crowdAI — `Source/FestaFiesta/*/CrowdSystem/`)
- 작성: 김시리 (Utility AI · 경제) / 갱신일 **2026-08-26**
- 🔴 **급한 순서로 §5 → §12 → §11 입니다.** §5 는 Utility AI 가 아예 안 도는 원인 4개,
  §12 는 유료 프로그램 배선(실제 작업 요청은 두 줄), §11 은 답을 기다리는 확인 사항입니다
- 목적: Utility AI 쪽 문서가 8개로 흩어져 있어서 "뭘 봐야 하는지"부터 막히는 문제를 없앤다.
  **이 문서는 코드 지도다** — 어떤 파일이 무엇을 하고, 그중 정원님이 실제로 손댈 것은 어디인지만 담는다.
- **이 문서 하나로 연동에 필요한 건 다 담았다.** 본문(§1~§12)은 읽는 것, **부록 A~E**는 코드 쓸 때
  찾아보는 참조표(필드 목록·의사코드)다. 설계 근거("왜 이 비율인가")가 궁금할 때만 §13의 원본 spec 을 본다

---

## 1. 세 줄 요약

1. **Utility AI 쪽은 함수가 다 있고 부르는 쪽이 없다.** 목적지 선택·욕구·개체 변주·예약 배정까지
   구현과 테스트가 끝났는데, NPC 하나하나에 적용하는 층(아래 **L3**)이 비어 있어서
   **실제로는 아무 NPC도 이 로직을 쓰고 있지 않다.**
2. 그 L3 가 crowdAI 데이터(`FUnitData`) 와 붙어야 해서, 정원님께 부탁드릴 것이 **4개(§5)** 다.
3. Utility AI 쪽 코드는 **전부 `static` 순수 함수 + DataTable** 이다. 상태를 안 들고 있고,
   같은 입력이면 항상 같은 출력이 나온다. 그래서 정원님이 언제 어디서 불러도 안전하다.

---

## 2. 담당 경계

```
김시리 (Utility AI)                     이정원 (crowdAI)

스폰  방문객 태그 명단 ───────────────▶ 그룹·리더 지정·스폰
      DistributionBuilder::Build

      개체 특성값 ───────────────────▶ 스폰 시 캐싱
      ResolveWithGroupSharing            (WalkSpeed 등)

입장  시간대별 입장 비율 ─────────────▶ 실제 입장 타이밍
      DT_VisitorArrivalCurve

운영  목적지 결정 ───────────────────▶ 이동·경로·도착
      UtilityDestinationSelector         MoveCommand
                       ◀─────────── 도착/실패

      욕구·체류·퇴장 판정 ◀────────── 시설 이용 완료

              ❌ 사이를 잇는 L3 층이 없음 (§5 A1)
```

**한 줄로**: 저는 **"누가 어디로 가야 하는가"를 계산**하고, 정원님은 **"실제로 그렇게 움직이게"** 합니다.
경계에 있는 게 `FFF_MoveCommand` 하나입니다.

---

## 3. 🔴 정원님이 꼭 봐야 할 코드 — 5개

경로는 전부 `Source/FestaFiesta/Public/` 기준. **위에서부터 급한 순서**입니다.

| #     | 파일                                                 | 무엇을 하는 것                                                                     | 정원님이 할 일                                                                                                            |
| ----- | -------------------------------------------------- | ---------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| **1** | `NPC/Integration/FF_MoveCommand.h`                 | 목적지를 넘기는 **유일한 접점**. `FFF_MoveCommand`(제→정원님) / `FFF_MoveResultEvent`(정원님→제) | 이 구조체를 받아 이동시키고, 도착·실패를 이벤트로 돌려주기. **양쪽 다 아직 스텁** — 발행하는 쪽도 수신하는 쪽도 없다                                              |
| **2** | `NPC/IndividualVariation/FF_DeterministicRandom.h` | 개체마다 다른 보행속도·인내심을 결정적으로 만든다                                                  | **스폰 시 `FFF_IndividualVariationResolver::ResolveWithGroupSharing()` 한 번 호출** 후 결과 캐싱. 안 부르면 모든 NPC가 똑같이 걷는다 (§5 A2) |
| **3** | `NPC/Party/FF_VisitorDistributionBuilder.h`        | 방문객 N명에게 아키타입·그룹 크기·아동 슬롯 태그를 붙인 명단을 만든다                                     | `Build()` 를 불러 나온 명단으로 그룹을 묶는다. 묶는 키는 `(TargetGroupSize, TargetGroupChildCount, ArchetypeId)`                       |
| **4** | `NPC/Integration/FF_CrowdReadModel.h`              | 5m×5m 격자 밀집도 스냅샷 구조체 (`FFF_CrowdCellSample`)                                 | 이 형태로 밀집도를 채워주면 됩니다. 셀 크기·좌표 원점은 같이 정해야 함                                                                           |
| **5** | `NPC/Party/FF_NeedState.h`                         | 배고픔·갈증·화장실·피로·자극 5종 상태 구조체                                                   | **직접 만지지 않습니다.** NPC 하나당 이걸 어디에 보관할지만 같이 정하면 됩니다 (§5 A1)                                                            |

### 3.1 그리고 한광훈 님 플러그인에서 하나

| 파일                                                  | 함수                                                       | 정원님이 할 일                                                                    |
| --------------------------------------------------- | -------------------------------------------------------- | --------------------------------------------------------------------------- |
| `Plugins/FF_ZoneSystem/.../FF_FFZSWorldSubsystem.h` | `ReportZonePopulation(ZoneId, Population, DeltaSeconds)` | **몇 초 간격으로 호출.** 프로젝트 전체에서 호출부가 **0곳**이라 NPC가 혼잡을 피하는 행동이 아예 안 나옵니다 (§5 A3) |

---

## 4. ✋ 안 봐도 되는 코드 — 이쪽은 제 안쪽입니다

정원님 파트와 안 닿습니다. 궁금해서 열어보는 건 자유지만 **읽지 않아도 연동에 지장 없습니다.**

| 폴더 / 파일                                 | 무엇                               | 왜 안 봐도 되나                                     |
| --------------------------------------- | -------------------------------- | --------------------------------------------- |
| `NPC/Destination/` 3개 파일                | Utility 식 6항 계산, 취향 벡터 합성, 계수 조회 | 제 내부 계산. 정원님은 **결과(`FFF_MoveCommand`)만** 받습니다 |
| `NPC/Party/FF_NeedRules.*`              | 욕구가 오르고 내리는 규칙                   | 제가 굴립니다. 정원님은 "시설 이용 완료" 신호만 주시면 됩니다 (§5 A4)  |
| `NPC/Party/FF_VisitHistory.*`           | 최근 방문 5곳 (같은 부스 재방문 페널티)         | 위와 같음. 완료 신호 하나로 욕구 해소 + 방문 기록이 같이 처리됩니다      |
| `NPC/Party/FF_ProgramReservation*.*`    | 유료 프로그램 예약 배정                    | 배정은 제 결정적 함수가 합니다. 정원님께는 §10 D2 제약 하나만 걸립니다   |
| `NPC/Integration/FF_ZoneSystemBridge.*` | ZoneSystem(한광훈) 자료형 → 제 자료형 변환   | 한광훈 님과의 경계입니다. 단위 환산(cm↔m, 초↔분)을 여기서만 합니다     |
| `NPC/Metrics/`                          | 욕구 미해소율 집계 → UI                  | 백종태 님과의 경계입니다                                 |
| `*Tests.cpp` 전부                         | 자동화 테스트                          | 제 쪽 회귀 방지용                                    |

> **핵심**: 정원님이 실제로 코드를 부르는 지점은 **`ResolveWithGroupSharing` · `Build` ·
> `ReportZonePopulation` 세 함수**와 **`FFF_MoveCommand` 수신** 뿐입니다. 나머지는 다 제 안쪽입니다.

---

## 5. 🔴 지금 막혀 있는 것 4개 — 이게 안 되면 Utility AI가 아예 안 돕니다

| #      | 부탁드릴 것                                                               | 안 되면 지금 무슨 일이 벌어지나                             | 어디         |
| ------ | -------------------------------------------------------------------- | ---------------------------------------------- | ---------- |
| **A1** | **NPC 하나당 상태를 어디에 둘지 같이 정하기** — `FUnitData` 에 넣을지 별도 배열로 둘지 (목록은 §6) | 목적지 선택이 **한 번도 실행되지 않는다.** 가장 큰 공백             | 협의         |
| **A2** | **스폰 시 `ResolveWithGroupSharing()` 1회 호출 + 결과 캐싱**                   | **모든 NPC가 똑같은 속도로 걷고 똑같이 참는다.** 개체 변주가 코드로만 존재 | 봐야 할 코드 #2 |
| **A3** | **`ReportZonePopulation()` 호출** (몇 초 간격, `DeltaSeconds`에 실제 보고 간격)   | **NPC가 혼잡을 피하는 행동이 안 나온다** (밀집 항이 항상 0)        | §3.1       |
| **A4** | **"시설 이용 완료" 신호** — Queue 의 `ServiceCompleted` 를 쓸 수 있으면 가장 깔끔       | 배고픔·화장실이 **영원히 안 풀린다**                         | 협의         |

> ⚠️ **A4는 "도착"이 아니라 "완료" 시점이어야 합니다.** 줄만 서고 포기한 NPC의 욕구가 풀리면
> 대기열에서 이탈하는 게 이득이 되어버립니다.

### 5.1 A2 는 이 코드 한 덩어리입니다

```cpp
#include "NPC/IndividualVariation/FF_DeterministicRandom.h"

// 스폰 시 1회. 매 Tick 다시 부를 필요 없습니다 — 같은 입력이면 항상 같은 값입니다
Unit.ResolvedTraits = FFF_IndividualVariationResolver::ResolveWithGroupSharing(
    VariationTable,          // DT_IndividualVariationKnobs 애셋
    GlobalSimulationSeed,
    Unit.StableAgentId,
    Unit.GroupID,            // solo 면 INDEX_NONE → 그룹 공유 없이 개인값만
    /*SpawnGeneration=*/0,   // 같은 ID를 재사용하지 않으면 0
    CorrelationStrength);    // DT_UtilityCoefficients 의 correlation_strength_leisureliness (기본 0.3)
```

**왜 crowdAI가 부르는 게 맞는가**: 스폰 시점을 소유하는 쪽이 crowdAI이고, 결과 중
`WalkSpeedMultiplier` 를 실제로 쓰는 쪽도 crowdAI(이동 담당)입니다.

**이 함수가 그룹 동행을 보장합니다.** 내부에서 두 번 계산합니다 — `StableAgentId` 시드로 개인 특성 4종,
`GroupID` 시드로 `WalkSpeedMultiplier`/`StayDurationMultiplier` 2종. 그래서 **같은 그룹은 같은 속도로
걷고 같은 시간 머뭅니다.** 인내심·혼잡내성 등 4종까지 공유하면 개체 변주의 목적(복제처럼 안 보이게
하기)이 무의미해지므로 그건 개체별 독립입니다.

| `FFF_ResolvedIndividualTraits` 필드                                                                                                         | 그룹 공유  | 쓰는 쪽             |
| ----------------------------------------------------------------------------------------------------------------------------------------- | ------ | ---------------- |
| `WalkSpeedMultiplier`                                                                                                                     | **공유** | **crowdAI (이동)** |
| `StayDurationMultiplier`                                                                                                                  | **공유** | Utility AI       |
| `PatienceMultiplier` · `CrowdToleranceMultiplier` · `PreferenceStrengthMultiplier` · `ReevaluateIntervalMultiplier` · `LeisurelinessAxis` | 개체별    | Utility AI       |

### 5.2 `FUnitData` 필드 — 2개는 됐고 3개 남았습니다

| 필드                                          | 상태   | 용도                           |
| ------------------------------------------- | ---- | ---------------------------- |
| `ArchetypeID` · `StableAgentID` · `PartyID` | ✅ 있음 | —                            |
| `CompanionSizeTag` (FName)                  | ❌    | `solo`/`pair`/`group_3_plus` |
| `bIsDependentChildSlot` (bool)              | ❌    | 아동은 자체 목적지 계산 없이 성인에게 상속     |
| `bIsGroupLeader` (bool 또는 `LeaderAgentId`)  | ❌    | 목적지는 리더만 계산, 나머지 상속          |
| `FFF_ResolvedIndividualTraits` 캐싱 필드        | ❌    | A2 결과 보관                     |

---

## 6. NPC 하나당 보관해야 하는 상태 (A1 상세)

목적지 선택·욕구·신규성 선호가 실제로 돌려면 NPC(또는 그룹)마다 아래를 들고 있어야 합니다.
**이걸 어디에 둘지가 A1 이고, 지금 유일하게 막힌 지점입니다.**

| 값                                         | 언제 바뀌나           | 누가 계산하나                              |
| ----------------------------------------- | ---------------- | ------------------------------------ |
| `FFF_NeedState` (5개 float)                | 시간 경과 + 시설 이용 완료 | `FFF_NeedRules::Advance` / `Relieve` |
| `FFF_VisitHistory` (FName 5 + float 5)    | 시설 이용 완료         | `FFF_VisitHistory::RecordVisit`      |
| `ConsecutiveNoChoiceCount`                | 재평가 때마다          | 선택기 결과                               |
| `RemainingStayMinutes`                    | 시간 경과            | `FFF_NeedRules::ConsumeStay`         |
| `LastEvaluationTime` / `ChoiceRoundIndex` | 재평가 때마다          | —                                    |
| `FFF_ResolvedIndividualTraits`            | **스폰 시 1회 캐싱**   | `ResolveWithGroupSharing` (§5.1)     |

앞 5개는 매 틱 변하고 마지막 하나는 스폰 후 불변이라, **보관 위치를 다르게 두는 게 나을 수 있습니다** —
매 틱 도는 배열에 불변값을 섞으면 캐시가 낭비돼서요.

**알아두시면 좋은 것 2개**

- **줄에서 기다리는 동안에도 욕구는 계속 쌓입니다.** 의도한 것입니다 — 줄이 길면 배고픔이 더 쌓여
  다음 재평가에서 다른 선택이 나오는 게 자연스럽다고 봤습니다.
  **"대기 중엔 건너뛰기" 최적화를 넣으면 동작이 바뀝니다.**
- **`RemainingStayMinutes` 는 시계가 아니라 예산입니다.** 해소 못 한 욕구가 쌓이면 시계보다 빨리
  줄어듭니다("줄이 길어서 축제를 떠났다"를 표현). 그래서 이 값으로 "몇 분 뒤 나가겠구나"를 읽으면 틀립니다.

---

## 7. 호출 순서 — 두 종류뿐입니다

### 7.1 스폰할 때 (1회, 결과를 캐싱)

```
1. FFF_VisitorDistributionBuilder::Build(...)      → 태그 명단 + 그룹 레시피   [제 함수, 정원님이 호출]
2. (정원님) 명단을 묶어 GroupID·리더·StableAgentId 부여
3. ResolveWithGroupSharing(...)                    → FFF_ResolvedIndividualTraits  [캐싱] ⬅ A2
```

`Build()` 가 개체마다 `TargetGroupSize`/`TargetGroupChildCount`/`bIsDependentChildSlot` 을 미리
채워서 돌려주므로, 정원님이 실제로 하는 건 **2번(리더 지정·ID 부여·누구와 누구를 묶을지)** 뿐입니다.

> ⚠️ `CompanionSizeTag` 는 `solo`/`pair`/`group_3_plus` 3종뿐이라 3~6인이 전부 뭉칩니다.
> **묶을 때는 정확한 인원수(1~6)를 담은 `TargetGroupSize` 를 보셔야 합니다.**

### 7.2 재평가 시점마다 (제 쪽에서 돕니다)

```
4. ShouldReevaluate(...)  → false 면 끝
5. BuildCandidates(...)   → 후보 8~12개 (ZoneSystem 스냅샷에서)
6. Choose(...)            → FFF_DestinationDecision
7-a. 목적지 있음 → FFF_MoveCommand 로 정원님께 전달   ⬅ 여기가 접점
7-b. 목적지 없음 → Wander / Relocate / Leave 중 하나
```

**아동(`bIsDependentChildSlot`)과 그룹 비리더는 4~6을 건너뜁니다** — 리더의 결정을 상속합니다.
즉 **`Choose` 는 그룹당 1회**만 돕니다.

---

## 8. 데이터 — DataTable 5장만 보시면 됩니다

**CSV 는 제 원본·재생성용입니다.** 정원님은 `Content/FestaFiesta/Data/` 의 애셋만 참조하세요.

| 애셋                                | 구조체                            | 무엇                            | 언제                                       |
| --------------------------------- | ------------------------------ | ----------------------------- | ---------------------------------------- |
| `DT_VisitorGroupSizeDistribution` | `FFF_GroupSizeDistributionRow` | 그룹 크기 1~6 분포                  | 그룹 묶을 때 (`ScopeId="tourism_daytrip"` 행만) |
| `DT_GroupChildComposition`        | `FFF_GroupChildCompositionRow` | 크기별 아동 수 분포                   | 같음                                       |
| `DT_GroupCompositionRules`        | `FFF_GroupCompositionRuleRow`  | 나머지 규칙 11개 (리더 수·상속 범위·공유 특성) | 같음 — 값 전체는 **부록 B**                      |
| `DT_VisitorArrivalCurve`          | `FFF_ArrivalCurveRow`          | 시간대별 유입 **비율(%)**, 시나리오 3종    | 입장 타이밍 정할 때                              |
| `DT_IndividualVariationKnobs`     | `FFF_IndividualVariationRow`   | 변주 7종의 (Min, Mode, Max)       | A2 호출 인자                                 |

**필드별 의미는 부록 B, 반환값 필드와 실전 의사코드는 부록 C, 유입 곡선 필드는 부록 D** 에 있습니다.

### 8.0 그룹 하나 만드는 순서

```text
1. DT_VisitorGroupSizeDistribution 에서 GroupSize 를 뽑는다
   (ScopeId="tourism_daytrip" 행만 사용 — 다른 값과 섞으면 비율 합계가 깨진다)
2. DT_GroupChildComposition 에서 그 GroupSize 의 ChildCount 를 뽑는다
3. ChildCount 명에게 bIsDependentChildSlot=true, 나머지는 성인
4. 성인 중 1명을 bIsGroupLeader 로 지정 (아동은 리더 불가)
5. 전원에게 같은 GroupID, StableAgentId 부여 (§9)
```

**1~3단계는 `FFF_VisitorDistributionBuilder::Build` 가 이미 대신 해줍니다** — 개체마다
`TargetGroupSize`/`TargetGroupChildCount`/`bIsDependentChildSlot` 이 미리 채워져서 나옵니다.
정원님이 실제로 하는 건 **4~5단계와 "누구를 누구와 묶을지"** 뿐입니다.

### 8.1 유입 곡선에서 헷갈리기 쉬운 것 3개

예시로 (총 유입 N=950, 어느 버킷이 `ArrivalSharePct=5.2`, `CumulativeSharePct=48.3`, `ProgressStart=0.4167`):

1. **비율(%)이지 인원이 아닙니다** — `5.2` 는 "전체 N의 5.2%". `5.2% × 950 ≈ 49명`처럼 **정원님이 정한 N**을
   곱해야 실제 인원이 나옵니다. N은 성능 실측으로 정원님이 정하는 값이라 제 쪽에서 알 수 없습니다.
2. **계산은 `ProgressStart`(0~1 진행도) 기준입니다** — `MinutesStartAtDefault`(=100분) 는 Live 가 지금
   240분이라서 그런 것뿐이고, **Live 길이가 바뀌면 틀린 값이 됩니다.**
   실제 코드는 `현재 경과분 / live_stage_duration_minutes` 를 `ProgressStart` 와 비교해야 합니다.
3. **버킷별 오차는 정상이고, 맞춰야 하는 건 `CumulativeSharePct`(누적)입니다** — 그룹이 1~6명 단위로
   들어와서 버킷당 49명을 딱 맞추는 건 원리적으로 불가능합니다. 누적을 비교해 크게 어긋나면
   다음 그룹을 더 빨리/천천히 꺼내는 식으로 보정하면 됩니다.

**곡선 모양 수치는 전부 가정값**이라 특정 모양을 전제하지 않는 구현을 부탁드립니다.
구현·테스트 중엔 `flat_v0`(균등, 가장 단순) 권장.

---

## 9. 반드시 지켜야 할 것 2개

1. **그룹은 항상 통째로 움직입니다.** 통째로 입장하고 통째로 퇴장합니다 — 일부만 먼저 들어오거나
   그룹원이 각자 다른 행동을 하는 경우는 없습니다. 구현 방법은 자유이고, 요구사항은 "쪼개지지 않는다"는
   결과뿐입니다.
2. **배정·타이밍은 항상 결정적이어야 합니다.** 같은 Seed 면 항상 같은 결과 — **`FMath::Rand()` 금지.**
   특히 `PartyID` 가 실행마다 같은 값이어야 합니다(예약 배정 순서를 정하는 값이라, 스폰 순서처럼
   프로세스마다 달라지면 같은 Seed 인데 예약자가 바뀝니다).

`StableAgentId` 는 이렇게 만들면 결정적입니다:

```cpp
int32 ComputeStableAgentId(int32 GroupID, int32 OrdinalInGroup)
{
    return static_cast<int32>(HashCombine(GetTypeHash(GroupID), GetTypeHash(OrdinalInGroup)));
}
```

> ⚠️ `FFF_DeterministicRandom::MakeTraitSeed` 를 그대로 쓰면 안 됩니다 — 그 함수의 두 번째 인자가
> 이미 `StableAgentId` 라서, `StableAgentId` 자체를 만드는 함수가 아닙니다.
> 
> ⚠️ **`GetTypeHash(FName)` 은 Seed 재료로 쓰면 안 됩니다.** FName 의 전역 표 칸 번호를 해시하는데
> 엔진이 그 값을 "process specific unstable int" 라고 명시합니다 — 에셋 로딩 순서가 바뀌면 값이
> 바뀝니다. 제 쪽에서 2026-08-10 에 같은 버그를 겪어서 `MakeStableNameHash`(내용 기반 FNV-1a)로
> 고쳤습니다. `int32` 해시는 안전합니다(위 코드처럼).

---

## 10. 그 밖에 부탁드릴 것 2개

| #      | 요청                               | 왜                                                                                                                                                   |
| ------ | -------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| **D1** | **퇴장을 출구 이동으로 처리**               | 퇴장 판정(체류시간 소진 / 연속 no_choice)은 제 쪽입니다. 즉시 사라지면 부자연스러워서, 그룹원 전원이 출구로 이동한 뒤 소멸했으면 합니다. **출구 지점·이동·소멸 시점이 정원님 소관**이고, 제 쪽은 판정과 이동 중 재평가 잠금입니다          |
| **D2** | **예약 그룹은 프로그램 시작 시각 전에 반드시 장내에** | 유료 프로그램을 사면 정원만큼의 손님이 **추가로** 방문합니다. 그 손님은 배열 앞쪽에 놓여 개장 직후 도착하므로 지금 설계로는 여유가 충분하지만, **예약석이 앞쪽에 몰리면 초반 버킷이 곡선보다 커집니다**(§11 오차 허용 폭). 상세는 §12 |

**급하지 않은 것**: 셀 격자 밀도 로그(§3 #4), 판단·이동·렌더링 3층 분리 성능 실측.
방문객 300·500·700 은 감으로 잡은 체크포인트일 뿐 상한 의도가 아닙니다.

---

## 11. 확인만 해주시면 되는 것

**답을 기다리는 것**

| #   | 질문                                                                                                                                                                                             |
| --- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| C1  | 🔴 **보행 속도 `150` 이 확정값인가요?** `FUnitData::Speed = 150.f`(1.5 m/s)를 제 계수(`npc_walk_speed_mps`)에 그대로 옮겨 적었는데, `Private/CrowdSystem/` 에서 이 값을 **대입하는 코드가 0곳**이라 구조체 기본값 그대로입니다. 같은 종류를 4개 더 찾았습니다 — `Temp_LiveStageMinutes`(240) · `Temp_FestivalMinutesPerRealSecond`(1.0) · `Temp_LeaveTimeoutSeconds`(180) · `Temp_ExitArrivalRadiusUU`(60). **"임시"라고만 알려주셔도** 제 쪽에서 그렇게 다루겠습니다 |
| C3  | **그룹 리더 조회 경로** — 목적지는 리더만 계산하고 나머지가 상속하는데, 리더 지정은 정원님이 하십니다. 이벤트인가요 조회 API 인가요                                                                                                                |
| C4  | **`bEnablePartyGrouping` 은 디버그 스위치인가요, 실제로 끄고 돌릴 수 있는 옵션인가요?** 끄면 전원 1인 그룹이 되어 정원 12 를 12 그룹으로 채우게 되는데, 그래도 되는 상황인지만 알면 됩니다 |

**제가 알려드리는 것 (C2 답)** — 🔴 **시각의 유일한 출처는 Stage 입니다.**
팀에서 *"각 시스템이 자체 시계를 들지 않고 `UFF_FestivalStageSubsystem` 을 거친다"* 로
정했습니다(시간 담당 강민서). 그래서 크라우드도 `SimTimeSeconds`·`Temp_LiveStageMinutes`·
`Temp_FestivalMinutesPerRealSecond` 를 자체 보유하지 않는 방향입니다.
저는 `IsLiveStage()` + `ElapsedLiveSeconds` 로 읽고, 초→분 변환을 제 쪽 한 곳에만 뒀습니다.
**아래 C5·C6 이 그 결정에 걸립니다.**

### 🔴 C5 — 유입 곡선 분모 `240` 은 **Day1 값**입니다

표를 덤프해서 확인한 값입니다(`DT_ProgramSchedule` 의 날짜별 마지막 이벤트 "전체 종료"):

| Day | 실제 Live 총 길이 | 곡선 분모(`Temp_LiveStageMinutes`) |
|---|---|---|
| Day1 | **240초** | 240 — 일치 |
| Day2 | **300초** | 240 — 🔴 **60초(20%) 모자람** |
| Day3 | **300초** | 240 — 🔴 **60초(20%) 모자람** |

`Progress = GetLiveMinutes() / 240` 이 240 에서 1.0 에 도달하고 clamp 되므로,
**Day2·3 의 마지막 60초에는 신규 입장이 한 명도 없습니다.** 크래시도 경고도 없어서
"후반이 좀 한산하네"로만 보입니다.

> 지금은 하루 경계 자체가 없어서(§12.2 의 E1) Day2·3 에 애초에 유입이 0 입니다.
> **하루 경계를 붙이시는 순간 드러날 두 번째 층**이라 같이 보시면 좋겠습니다.

분모는 상수 대신 Stage 의 그날 Live 길이를 쓰는 쪽이 맞을 것 같습니다.
`GetLiveDurationForCurrentDay()` 가 **`public` 으로 열렸습니다**(강민서 님 승인, 반영 완료).

### ⚠️ C6 — 배속을 켜면 두 시계가 영구히 벌어집니다

`DT_TimeSettings` 에 `Scale_1x`·`Scale_2x`·`Scale_4x` 가 저작돼 있는데
`TimeScaleValue` 를 읽는 코드가 아직 0곳입니다. 배선되면 이렇게 됩니다:

```
Stage      ElapsedLiveSeconds += DeltaTime * TimeScale     ← 배속 반영
크라우드    SimTimeSeconds     += FixedStepSeconds          ← 배속 없음
```

4배속이면 4배로 벌어지고, 1 로 되돌려도 **벌어진 채로 남습니다**(누적값이라).
크라우드가 Stage 시각을 읽는 방향으로 가면 이 문제가 통째로 사라집니다.

**같이 정해야 하는 것**: 🔴 **유입 곡선 누적 오차 허용 폭** — *"돌려보고 정하자"* 로 남겨둔
항목인데, 예약석이 배열 앞쪽에 몰려서 초반 버킷이 곡선보다 커집니다. 얼마까지 튀어도 되는지
기준이 필요합니다. 곡선 준수를 검사하는 테스트를 이미 만드셨다면 그게 먼저 깨질 겁니다.
그 밖에 `CohesionRadiusMeters` 값(이동 스택 확정 후) ·
`TargetDestinationId` → 월드 좌표 조회를 누가 하는지(한광훈 님도 걸림).

---

## 12. 🔴 유료 프로그램이 방문객을 "데려옵니다" — 배선 요청

플레이어가 프로그램을 사면 **그 프로그램을 좋아하는 아키타입으로 정원만큼 손님이 추가로
방문합니다.** 프로그램이 "유료 광고"가 되는 셈이고, 그래서 그 손님은 반드시 그 시설에 갑니다.

- **곡선 총량이 커집니다.** 오는 사람 중에서 고르는 것이 아니라 명단에 더 얹는 것이라,
  조합을 맞춰 고르는 문제가 없습니다 — **"명단에 몇 명 더 있다"** 가 전부입니다
- 예약 손님은 **아동 없이** 성인만, 그룹 상한 6명으로 쪼개집니다(정원 12 = 6+6, 16 = 6+6+4).
  인원 합은 항상 정원과 같습니다
- 프로그램이 둘이면 **손님도 둘 다 옵니다.** 같은 파티가 두 예약에 들어가는 것은 제 쪽에서 막습니다

### 12.1 제가 읽은 현재 스폰 흐름 — 맞는지 봐주세요

코드를 읽고 정리한 것입니다. **아직 작업 중이신 부분이면 아래 부탁 4개의 전제가 통째로 틀립니다.**
틀린 곳만 알려주시면 됩니다.

```
AFF_SpawnActor::BeginPlay()                    ← 레벨 BeginPlay, 1회
  └ BeginArrivalSchedule(UnitCount, Origin, Radius)
      └ BuildVisitorDistribution(UnitCount)     ← 제 빌더. 순서가 고정된 배열이 나옵니다
  (매 틱) UpdateArrivalSchedule()
      Progress = GetLiveMinutes() / Temp_LiveStageMinutes
      Target   = CumulativeSharePct(Progress)/100 × Total
      └ DrainVisitorUntil(Target)               ← 커서를 Target 까지 밉니다
                                                   PartyID 는 여기서 발급됩니다
```

여기서 제게 중요한 것은 **곡선이 "언제"만 정하고 "누가"는 배열 순서가 이미 정해두었다**는 점입니다.
그러면 "언제 도착하는가" = "배열 몇 번째인가" 라서, 프로그램 시작 시각을 인덱스로 바꿀 수 있습니다:

```
프로그램이 T분에 시작 → 그 전에 도착해야 함
MaxIndex(T) = floor( CumulativeSharePct(T)/100 × Total ) - 그룹크기
```

**즉 예약 그룹을 배열 어디에 꽂을지는 제가 표만 보고 계산할 수 있습니다.** 런타임 협의가 필요 없습니다.

### 12.2 부탁드릴 것 5개 — 실제 작업 요청은 2개(E2·E3)입니다

| # | 부탁 | 왜 |
|---|---|---|
| **E1** | 🔴 **하루 경계에서 유입을 다시 시작하는 주체가 누구인가요** | `BeginArrivalSchedule` 호출부가 `BeginPlay` 1곳이고, `SimTimeSeconds` 는 리셋이 없고, 크라우드가 Stage 를 구독하지 않는 것으로 읽었습니다. 그러면 Day2·3 에는 새로 입장하는 사람이 없게 되는데 — **아직 안 붙이신 것인지, 다른 방식으로 하실 것인지**를 알아야 예약석을 언제 심을지 정합니다 |
| **E2** | **`BeginArrivalSchedule` 에 인자 하나만 더 받아주실 수 있나요** | 배열은 제 빌더가 만들어 드립니다. **제 쪽 코드는 이미 다 들어가 있습니다** — 인자를 그대로 `FFF_VisitorDistributionBuilder::Build` 로 넘겨주시면 끝입니다. 커서 미는 방식은 그대로입니다. 정확한 형태는 **§12.3 배선도** 참고 |
| **E3** | 🔴 **스폰된 그룹의 `PartyID` 를 알려주는 창구** | 예약은 `PartyID` 로 잠금·퇴장 금지를 겁니다. 그런데 `PartyID` 는 스폰 순간에 생기니 제가 알 방법이 없습니다. 현재 크라우드가 발행하는 델리게이트가 0개라 **새로 만들어야 하는 항목**입니다. **받는 쪽 함수는 이미 만들어 뒀습니다** — `ConfirmReservedParty` 를 부르시거나, 델리게이트만 열어주시면 제가 구독하겠습니다 |
| **E4** | 🔴 **`AgentPoolSize` 를 올려도 될까요 — 지금 여유가 2명뿐입니다** | `Test_Main` 을 헤드리스로 띄워 실측했습니다: `Initialized: 500 actors, 500 agents` · `요청 500명 중 498명`. **풀 500 에 498 명이 이미 차 있어 남는 자리가 2 개**입니다. 프로그램 하나(정원 12)만 사도 넘치고, 넘치면 **곡선 뒷부분이 잘립니다**(그 Error 로그는 이미 넣어두셨더군요). 프로그램은 최대 2개까지라 **+30 정도 여유**면 충분합니다. 올리는 게 맞을지, 아니면 `UnitCount` 를 줄여 자리를 비우는 게 맞을지 정해주세요 |
| **E5** | **예약 그룹의 체류 예산을 늘려주실 수 있나요** | 체류시간이 `Temp_LiveStageMinutes × stay_duration_share × 개체변주` 로 스폰 때 정해지는 것으로 읽었습니다. 짧게 뽑히면 **예약자가 프로그램 시작 전에 퇴장**합니다. 스폰 시점에 정해지는 값이라 그쪽에서 예외를 두는 게 자연스러운데, 제가 판정으로 막는 쪽이 나으면 그렇게 하겠습니다 |

### 12.3 배선도 — 실제 함수 이름으로

🔴 **표시된 두 줄만 정원님 쪽 작업입니다.** 나머지는 제 쪽에 다 들어가 있습니다.

```
[아침]  Setup · MorningCheck — 플레이어가 프로그램을 산다
  UFF_FestivalProgressionSubsystem::TryScheduleProgram(ProgramId, FacilityInstanceId)
    └ Gold 차감 · Staff 묶기
    └ UFF_ProgramReservationSubsystem::RegisterProgram(...) → ReservationId

[그날 유입 시작]
  TArray<FFF_ReservedPartyRequest> Requests = Progression->GetReservedPartyRequests();
                                                                        ← 김시리 (완료)
  🔴 CrowdSim->BeginArrivalSchedule(UnitCount, Origin, Radius, Requests);   ← E2
        └ FFF_VisitorDistributionBuilder::Build(..., Requests)
              → Visitors[] **앞쪽**에 예약 손님이 놓인다
                각 개체가 FFF_TaggedVisitor::ReservationId 를 달고 있다

[Live 중]  곡선이 커서를 민다 (지금 하시는 그대로)
  DrainVisitorUntil(Target) → 그룹을 묶고 PartyID 를 발급
  🔴 선두 개체의 ReservationId 가 유효하면:                                  ← E3
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

> ⚠️ **E3(`ConfirmReservedParty`)가 프로그램 시작 시각(`StartMinutes`)보다 먼저 와야** 그
> 프로그램의 매출이 잡힙니다 — 체험비를 프로그램이 시작하는 순간에 걷습니다. 예약 손님이
> 개장 직후 도착하고 프로그램은 Live 30분·90분에 시작하므로 지금 설계로는 여유가 충분합니다.

**E2 의 정확한 형태** — 기본 인자라 기존 호출부는 안 고쳐도 됩니다:

```cpp
// UFF_CrowdSimulationSubsystem
void BeginArrivalSchedule(
    int32 TotalVisitorCount, FVector SpawnCenter, float SpawnRadiusUU,
    const TArray<FFF_ReservedPartyRequest>& ReservedParties = {});
```

받은 값을 `BuildVisitorDistribution` 을 거쳐 `Build` 의 마지막 인자로 넘기시면 됩니다.
빌더 쪽은 이미 그 인자를 받고 있고, **비우면 이 인자가 생기기 전과 한 개체도 다르지 않습니다**
(`FestaFiesta.NPC.VisitorDistribution.EmptyReservedChangesNothing` 이 그것을 고정합니다).

**E3 는 두 가지 중 편하신 쪽**:
① 스폰 직후 `ConfirmReservedParty` 를 직접 부르신다 (위 배선도)
② `OnPartySpawned(PartyID, LeadReservationId, MemberCount)` 같은 델리게이트만 열어주시면
   제가 구독해서 처리하겠습니다

**헤더**: `NPC/Party/FF_VisitorDistributionBuilder.h`(주문서·태그) ·
`NPC/Party/FF_ProgramReservationSubsystem.h`(확정·판정)

**제가 요청하는 것은 위 🔴 두 줄뿐입니다.** E1·E4·E5 는 "어떻게 하실 계획인지"를
여쭙는 것이지 작업 요청이 아닙니다.

> ⚠️ **한 가지 미결이 남습니다.** 지금은 예약 손님이 배열 맨 앞이라 **개장 직후 도착**하고,
> 퇴장 금지는 프로그램이 **끝날 때까지**입니다. 선셋 요가(90~120분)면 0분에 와서 120분까지
> 못 나가서 하루의 절반을 묶여 있게 됩니다. 부스는 이용하니 매출에는 도움이 되지만
> 자연스러운지는 돌려봐야 알 것 같습니다. **곡선 오차 허용 폭(§11)이 정해지면 도착을
> 뒤로 미뤄 좁힐 수 있습니다.** 배선을 붙이실 때 같이 보시죠.

---

## 13. 더 깊이 볼 때만 — 원본 문서 지도

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
순서대로면 충분하고, `FMath::RandRange` 로 무작위 추출은 금지입니다 (§9).

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
| `MinutesStartAtDefault` / `ClockStartAtDefault` | float / FString | **표시용. 계산에 쓰지 마세요** (§8.1 ②)                             |

## 부록 E. 목적지 전달 상세 (`FF_MoveCommand.h`)

| `FFF_MoveCommand` (제 → 정원님) | 뜻                                        |
| --------------------------- | ---------------------------------------- |
| `PartyId`                   | = `GroupID`                              |
| `TargetDestinationId`       | FName. ZoneSystem 시설 `FGuid` 를 문자열화한 값   |
| `TargetEntryPointId`        | 대기열 입구                                   |
| `Intent`                    | `Visit` / `Wander` / `Exit` / `Evacuate` |
| `CommandId`                 | 결과 이벤트와 짝을 맞추는 식별자. 같은 값 중복 요청은 거부 대상    |

| `FFF_MoveResultEvent` (정원님 → 제) | 뜻                                                    |
| ------------------------------- | ---------------------------------------------------- |
| `PartyId` / `CommandId`         | 어느 명령에 대한 결과인지                                       |
| `Result`                        | `Arrived` / `PathFailed` / `Blocked` / `Interrupted` |
| `SimTimeMinutes`                | 현실 분 기준 시뮬레이션 시각                                     |

- **폴링하지 말고 이벤트로 주세요** — 병렬화 때문입니다
- ⚠️ **`TargetEntryPointId` 는 퇴장 지점과 반드시 분리해야 합니다.** 겹치면 나가는 사람과 들어오는 사람이 서로를 막아서 인위적인 데드락이 생깁니다. 시설 데이터(한광훈 님) 쪽에 EntryPoint / ExitPoint 가 따로 있어야 합니다
- ⚠️ **`Arrived` 는 "입구에 도달했다"일 뿐 "줄에 들어갔다"가 아닙니다.** 걸어가는 동안 줄이 꽉 찰 수 있어서 도착 판정과 가입 판정을 분리합니다. 가입 실패는 재평가 요청으로 이어집니다
- `AFFZSGameplayZone::GetBoundaryAreaSquareMeters()` 결과가
  `FFFZSZoneReadModel::AreaSquareMeters` 로 노출됩니다. `FFF_CrowdReadModel::ZoneAreaSqm` 이
  필요하면 이 값을 ZoneId 별로 복사하시면 됩니다

---
