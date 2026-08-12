# UI 연동 API 계약

UI가 **호출하거나 구독하는** 함수·델리게이트·구조체를 한곳에 모은 문서다.

## 왜 이 문서가 필요한가

UI는 7개 서브시스템 거의 전부에서 데이터를 읽는다. 각 모듈의 헤더를 매번 뒤지면
어떤 게 완성이고 어떤 게 스텁인지, 언제 갱신되는지 알 수 없다.

**모듈 담당자는 UI에 노출할 것이 생기면 자기 섹션에 한 항목 추가한다.**
헤더 주석에만 적으면 UI 쪽에서 존재를 모른다.

---

## 지켜야 할 원칙 (합의사항)

1. **UI는 월드·경제 상태를 직접 수정하지 않는다.** 읽기는 Read Model, 쓰기는 Command로 요청한다.
2. **낙관적 갱신을 하지 않는다.** World Change는 실패 시 롤백되므로, 결과 이벤트를 받은 뒤에 화면을 바꾼다.
3. **폴링하지 않는다.** UI가 매 틱 시스템 내부를 들여다보는 코드를 만들지 않는다.
   값이 바뀌면 알려주는 델리게이트를 우선한다. 델리게이트가 없으면 이 문서에 요청으로 남긴다.
4. **구독한 직후 현재값을 한 번 읽는다.** 이벤트는 미래만 알려주므로, 위젯이 늦게 생성되면 초기 상태를 놓친다.
   → 델리게이트를 만들 때 **현재값을 돌려주는 Getter도 같이** 제공해 주면 UI가 이 처리를 할 수 있다.

---

## 항목 작성 템플릿

```markdown
### 이름

- **헤더**: `Public/경로/파일.h`
- **접근**: (서브시스템 이름 / 정적 함수 / 구조체)
- **시그니처**: `반환형 함수명(인자)`
- **갱신 주기**: (이벤트 / N틱 / 요청 시)
- **상태**: `구현완료` / `스텁` / `계획`
- **비고**: 단위, 주의사항, 미합의 항목
```

`상태`를 반드시 적어 주세요. **스텁을 구현완료로 착각하면 UI가 빈 화면을 그리고 원인을 못 찾습니다.**

---

# 모듈별 API

## Stage · 시간 — 강민서

### `UFF_FestivalStageSubsystem`

- **헤더**: `Public/Time/FF_FestivalStageSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **상태**: `구현완료`

| 멤버                                     | 형                   | 설명                                |
| -------------------------------------- | ------------------- | --------------------------------- |
| `CurrentStage`                         | `EFF_FestivalStage` | 현재 Stage (Reconfigure 삭제됨)        |
| `RemainingTime`                        | `float`             | 남은 시간(초)                          |
| `CurrentDay`                           | `int32`             | 1~3일차                             |
| `ElapsedLiveSeconds`                   | `float`             | 오늘 Live 시작 후 경과 시간                |
| `TimeScale`                            | `float`             | 배속값 (1/2/4)                       |
| `OnStageChanged`                       | `FOnStageChanged`   | (PreviousStage, NewStage)         |
| `RequestTransition(EFF_FestivalStage)` |                     | Stage 전환 요청                       |
| `IsReconfigureAllowed()`               | `bool`              | 배치 변경 허용 여부 (Setup/MorningCheck만) |
| `SetTimeScale(float)`                  |                     | 배속 변경                             |
| `IsMainProgramLocked()`                | `bool`              | 메인 공연 배속 잠금 구간 여부                 |
| `AdvanceToNextDay()`                   |                     | Report에서 다음 날 진입                  |

**삭제된 것**: `CurrentSlot` (하루 4타임 구조 없어짐, Live는 하루에 한 번)
**UI 사용처**: 상단 바(Stage/Day/시계), Morning Check 진입 판정, 배속 버튼 활성/비활성(IsMainProgramLocked 참조)

**요청** — `RemainingTime`이 매 틱 바뀌므로 UI가 폴링해야 합니다.
초 단위로 바뀔 때만 쏘는 델리게이트가 있으면 좋겠습니다. 없으면 UI 쪽에서 0.1초 폴링 + 변화 감지로 처리하겠습니다.

**답변** -

FOnRemainingSecondsChanged (int32 NewSeconds)

구독 방법:
GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>()
->OnRemainingSecondsChanged.AddDynamic(this, &YourClass::YourHandler);

이거 구독하시면 폴링 없이 초 단위로 바뀔 때만 UI 갱신하시면 됩니다.

---

## Zone · 시설 — 한광훈

### `FFFZSReadModel`

- **헤더**: `Plugins/FF_ZoneSystem/.../Public/FF_FFZSTypes.h`
- **접근**: `UFFZSWorldSubsystem::BuildReadModel()`
- **갱신 주기**: 요청 시 수집 (`WorldRevision`으로 버전 확인)
- **상태**: `구현완료`

```cpp
FFFZSReadModel {
    int64 WorldRevision;
    TArray<FFFZSZoneReadModel> Zones;
    TArray<FFFZSFacilityReadModel> Facilities;
}
```

**`FFFZSFacilityReadModel`** — `FacilityInstanceId`, `FacilityDefinitionId`, `ZoneId`, `Category`,
`State`, `bNeedsRelocation`, `QueueState`, `WaitingCount`, `ActiveServiceCount`, `AverageCompletedWaitSeconds`,
`WorldLocation`(시설 Actor 대표 월드 지점, cm)

**`FFFZSZoneReadModel`** — `ZoneId`, `SoftCapacity`, `ReportedPopulation`, `DensityRatio`, `DensityState`

> ⚠️ **`DensityRatio`는 Heat Map 색상에 쓰지 않는다.**
> `ReportedPopulation / SoftCapacity`이고 SoftCapacity는 기획자가 정한 정원이라 면적 기반이 아니다.
> Fruin Level of Service(인/㎡)와 직접 비교할 수 없다. Heat Map은 Crowd 쪽 `DensityPerSqm`을 쓴다.

### 델리게이트

| 이름                                | 인자                                   | 용도                        |
| --------------------------------- | ------------------------------------ | ------------------------- |
| `FFFZSWorldChangedSignature`      | `FFFZSWorldChangeEvent`              | 월드 변경 알림                  |
| `FFFZSPlacementRejectedSignature` | `FFFZSCommand`, `FFFZSCommandResult` | 배치 거부 — **UI가 실패 사유를 표시** |

**UI 사용처**: Overview 시설 상태, 대기열 길이, Morning Check 배치, 실패 사유 토스트

### 브리지 2종 (2026-08-06 추가)

| 클래스                                  | 역할                                                                             |
| ------------------------------------ | ------------------------------------------------------------------------------ |
| `UFF_ZoneSystemQueueBridgeSubsystem` | Zone 시설 `FGuid` ↔ Queue `int32` ID 매핑. `TryJoinZoneFacility`가 Party 가입의 단일 진입점 |
| `UFF_ZoneSystemStageBridgeSubsystem` | `UFF_FestivalStageSubsystem`을 구독해 Zone 변경 단계를 맞춤                               |

**UI는 시설과 대기열을 직접 결합하지 않습니다.** 양쪽을 각각 읽고 UI에서 합치면
소유권이 흐려집니다. `UFF_FestivalViewModelSubsystem`에서 읽습니다.

### 개발용 플레이테스트 도구 (참고)

`UFF_ZoneSystemPlaytestSubsystem` + `WBP_ZoneSystemPlaytestHUD`가 있습니다.
`Content/FestaFiesta/Zone/Maps/ZoneTestMap`에서 키보드로 시설 설치·이동·제거·개폐·큐 조작을 시험할 수 있습니다.

**플레이어용 UI가 아니라 개발 도구입니다.** CommonUI·스타일 에셋을 쓰지 않는 것은 의도된 것입니다.
Morning Check UI를 만들 때 시설 명령 호출 방식의 참고 자료로 씁니다.

### 완성형 시설 건축 카탈로그 (2026-08-12 갱신)

- **헤더**: `Public/Building/FF_BuildCatalogSubsystem.h`, `Public/Building/FF_BuildModeSubsystem.h`
- **접근**: `GameInstanceSubsystem` 카탈로그 + `TickableWorldSubsystem` 건축 모드
- **상태**: `구현완료` (스킨 적용 데이터는 계획)

`UFF_BuildCatalogSubsystem`은 Asset Manager의 `FFZSFacilityDefinition` Primary Asset을 비동기 로드하고
`대분류 > 소분류 > 고유이름` 읽기 모델을 만든다. 위젯은 에셋 레지스트리를 직접 조회하거나 ID를 직접
파싱하지 않고 `GetEntries()`/`GetCategoryGroups()`와 `OnCatalogChanged`를 사용한다.

`UFF_BuildModeSubsystem`은 `Closed → BuildMode ↔ CatalogOpen → Placing`과
`BuildMode → Moving → BuildMode` 상태를 소유한다. 신규 설치는 선택한
`UFFZSFacilityDefinition::Spec`을, 이동은 기존 액터의 권위 Spec·GUID·현재 회전을
`AFFZSPlacementPreviewActor`와 `ExecuteCommand()` 경로에 전달한다. 따라서 Zone 경계·충돌·여유 공간·
Queue 예약·보호구역·지면·내비게이션·운영 단계 검증을 우회하지 않는다.

활성 건축 세션에서는 설치 시설의 footprint(초록)·clearance(청록)·queue 예약 범위(노랑)를 표시한다.
카탈로그가 닫힌 `BuildMode`에서 시설을 좌클릭하면 청록색으로 강조되고, 하단 작업 패널에
`이동`·`철거`·`취소`가 표시된다. 이동을 시작하면 권위 액터는 기존 위치에 그대로 두고 ghost만 커서를
따라간다. 따라서 우클릭 취소나 검증 실패 때 원위치 복구 명령이 필요 없다. 이동 성공 시 같은 시설 GUID를
유지한 채 Zone·배치·회전만 원자적으로 갱신한다. 대상이 외부 명령이나 스냅샷 복원으로 바뀌면 열린 작업과
이동 프리뷰는 자동 취소된다.

| 입력 | 동작 |
| --- | --- |
| `B` | 첫 입력은 `BuildMode` 진입, 이후 카탈로그 열기/닫기 |
| `M` 또는 `이동` 버튼 | 선택 시설의 이동 프리뷰 시작 |
| `Q`, `E` | 설치/이동 프리뷰를 90도 회전 |
| `좌클릭` | `BuildMode`에서는 시설 선택, `Placing`에서는 설치, `Moving`에서는 이동 확정 |
| `Enter` | 선택 작업 패널에서는 철거, `Placing`/`Moving`에서는 현재 프리뷰 확정 |
| `우클릭` | 시설 선택 취소, 설치는 카탈로그 복귀, 이동은 원본을 건드리지 않고 `BuildMode` 복귀 |
| `Esc` | 어느 활성 건축 상태에서든 건축 모드 전체 종료 |

카탈로그가 CommonUI 키보드 포커스를 가진 동안에는 화면의 preview-key 어댑터가 `B`/`Esc` 의도를
서브시스템에 전달한다. UI와 PlayerController가 같은 `B`를 모두 관찰한 프레임에는 키 해제 래치가 raw
입력을 한 번 차단하므로 `CatalogOpen → BuildMode → CatalogOpen` 이중 전환이 발생하지 않는다.

- `Install`·`Move`·`Remove`는 `InitialSetup`/`MorningCheck`에서만 가능하다.
- `Active` 시설 또는 FFZS/프로젝트 Queue에 이용객이 남은 시설은 이동·철거할 수 없다. 자동 폐장이나
  강제 배출은 하지 않으며, 운영 종료 후 Queue가 비워진 것을 확인해야 한다.
- 같은 위치·같은 방향 이동은 revision과 상태를 불필요하게 바꾸지 않도록 UI에서 차단한다.
- 이동 성공 뒤 시설 상태는 `Validated`가 되므로 운영을 재개하려면 기존 개장 절차를 다시 수행한다.
- 상단 UI는 `건축`/`스킨` 2탭이지만, 현재 `스킨` 탭은 데이터 모델이 없어 준비중 상태다.
- 현재 등록된 실제 시설 정의가 1개뿐이므로, 나머지 카드는 규칙에 맞는 Data Asset을 추가할 때 자동 생성된다.

---

## Queue — 장원재

### `UFF_QueueSubsystem`

- **헤더**: `Public/Queue/FF_QueueSubsystem.h`
- **상태**: `구현완료` (P0)

| 델리게이트                           | 인자 수 |
| ------------------------------- | ---- |
| `FFF_OnQueuePartyJoined`        | 4    |
| `FFF_OnQueuePartyRejected`      | 3    |
| `FFF_OnQueueServiceStarted`     | 3    |
| `FFF_OnQueuePartyExited`        | 4    |
| `FFF_OnQueueSlotTargetsUpdated` | 2    |

함수: `RegisterFacility`, `UnregisterFacility`, `IsFacilityRegistered`

**UI 사용처**: Overview 대기열 표시, 시설별 대기 인원, 포기율 집계

**요청** — 시설별 **현재 대기 인원을 한 번에 돌려주는 Getter**가 있으면 좋겠습니다.
델리게이트만으로는 위젯이 늦게 생성됐을 때 현재 상태를 알 수 없습니다.
(`FFFZSFacilityReadModel::WaitingCount`와 중복이면 어느 쪽이 권위인지 알려주세요.)

---

## Crowd · 밀집 — 이정원

### `FFF_CrowdReadModel`

- **헤더**: `Public/NPC/Integration/FF_CrowdReadModel.h`
- **갱신 주기**: N틱 (초안 30틱)
- **상태**: **`스텁`** — 채우는 쪽도 읽는 쪽도 아직 없음. 셀 크기·기록 주기·좌표 원점 미합의

```cpp
FFF_CrowdCellSample {
    int32 CellX, CellY;
    int32 Occupancy;        // 현재 인원
    int32 Throughput;       // 직전 구간 통과 인원
    float DensityPerSqm;    // 인/㎡ — Fruin 직접 비교 가능
}
```

**UI 사용처**: **Heat Map 색상 스케일** (2 주의 / 4 위험 / 5 심각), 밀집 지표, Report 드릴다운

**UI 요청** — 이게 Heat Map의 유일한 원본입니다. 셀 크기와 좌표 원점이 정해지면 알려주세요.
UI는 5m × 5m 격자를 가정하고 스텁을 만들어 두겠습니다.

### 기타

`FF_BaseCrowd`, `FF_BaseNPC`, `FF_SpawnActor`, `FF_CrowdMovementComponent`,
`FF_CrowdVisualComponent`, `FF_CrowdSimulationSubsystem` — UI 노출 API는 아직 미정.

---

## Utility AI · 경제 · 진행 — 김시리

### `UFF_NpcNeedMetricsSubsystem` — Need 집계 (2026-08-06 신설)

- **헤더**: `Public/NPC/Metrics/FF_NpcNeedMetricsSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **갱신 주기**: 이벤트 (집계 주기마다, 초안 30틱). 값이 바뀐 Need만 발행
- **상태**: **`스텁`** — 읽는 쪽은 완성, 채우는 쪽 없음. **지금은 전부 0**
- **UI 사용처**: Report 드릴다운 `CauseId` 롤업(AT-005)

| 멤버                                | 형                             | 단위·범위                                       |
| --------------------------------- | ----------------------------- | ------------------------------------------- |
| `GetNeedUnmetRatio(EFF_NeedType)` | `float`                       | **0~100 (퍼센트)**. 0~1 아님                     |
| `GetNeedAverage(EFF_NeedType)`    | `float`                       | 0.0~1.0                                     |
| `GetSampleCount()`                | `int32`                       | 직전 집계 표본 수                                  |
| `GetUnmetThreshold()`             | `float`                       | 0.0~1.0, 기본 0.7                             |
| `OnNeedUnmetRatioChanged`         | `FFF_OnNeedUnmetRatioChanged` | `(EFF_NeedType, float NewRatioPercent)` 2인자 |

`EFF_NeedType` 5종: `Hunger` `Thirst` `Restroom` `Fatigue` `Stimulation`

> ⚠️ **`GetSampleCount() == 0`은 "데이터 없음"입니다.** 표본이 없어도 미해소율은 0을 돌려줍니다.
> 구분하지 않으면 "아무도 배고프지 않다"로 잘못 표시됩니다.

### `FFF_DestinationDecision` — NPC 1명의 이동 원인

- **헤더**: `Public/NPC/Destination/FF_UtilityDestinationSelector.h`
- **접근**: 구조체 (선택 함수 반환값)
- **상태**: `구현완료` — **다만 UI가 받을 경로가 아직 없습니다** (아래 요청)
- **UI 사용처**: NPC 클릭 시 "왜 저기로 갔는가" 표시

`AllCandidateScores`에 후보별 항별 점수가 다 들어 있어 그대로 표시하면 됩니다.

| 필드                                                      | 뜻             |
| ------------------------------------------------------- | ------------- |
| `AttractionTerm`                                        | 시설 기본 매력도     |
| `PreferenceTerm`                                        | 취향 일치 (음수 가능) |
| `NeedTerm`                                              | 욕구 급함         |
| `DistanceCostTerm` / `WaitCostTerm` / `DensityCostTerm` | 비용 (항상 0 이하)  |
| `TotalUtility` / `ChoiceProbability`                    | 합계 / 선택 확률    |

`DestinationId`가 `NAME_None`이면 **`no_choice`**(아무 데도 안 감)이고, 그때 행동은
`EFF_NoChoiceAction` 3종 — `Wander`(배회) / `Relocate`(구역 이동) / `Leave`(퇴장).

**요청** — 이 구조체를 UI에 넘기는 경로를 정해야 합니다. 원하는 형태 알려주시면 맞추겠습니다
(예: "선택된 NPC 1명의 마지막 결정"을 돌려주는 Getter).

### 아직 없는 것

`UFF_FestivalProgressionSubsystem`(경제·Gold·일차 결과·저장) — 아래 "UI가 필요한데 없는 것" 표 참고.

---

## UI Read Model — 백종태

### `UFF_FestivalViewModelSubsystem`

- **헤더**: `Public/UI/FF_FestivalViewModelSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **상태**: **`스텁`** — 지표 4종은 하드코딩 더미. Stage·Day는 진짜 값
- **역할**: 위 모든 소스를 UI가 쓰기 좋은 형태로 합쳐서 제공

UI 위젯은 **이 서브시스템만 참조**하고 다른 시스템을 직접 보지 않습니다.
각 모듈이 바뀌어도 위젯을 안 고치게 하기 위해서입니다.

| 값                 | Getter                                | 델리게이트                        | 현재        |
| ----------------- | ------------------------------------- | ---------------------------- | --------- |
| SimSatisfaction   | `GetSimSatisfaction()`                | `OnSimSatisfactionChanged`   | 더미 72     |
| ScoreSatisfaction | `GetScoreSatisfaction()`              | `OnScoreSatisfactionChanged` | 더미 72     |
| Safety            | `GetSafety()`                         | `OnSafetyChanged`            | 더미 88     |
| Gold              | `GetGold()`                           | `OnGoldChanged`              | 더미 10,000 |
| TotalRevenue      | `GetTotalRevenue()`                   | `OnTotalRevenueChanged`      | 더미 0      |
| TodayRevenue      | `GetTodayRevenue()`                   | `OnTodayRevenueChanged`      | 더미 0      |
| Stage / Day       | `GetCurrentStage()` `GetCurrentDay()` | `OnStageViewChanged`         | **진짜 값**  |
| 남은 시간             | `GetRemainingSeconds()`               | `OnRemainingSecondsChanged`  | **진짜 값**  |

### 값 규격 (2026-08-06 UI 결정)

```cpp
int32 SimSatisfaction;    // 0~100, 높을수록 좋음. 대기·밀도·취향 매칭에서 나오는 트윈 산출물
int32 ScoreSatisfaction;  // 0~100, 높을수록 좋음. SimSatisfaction + FinaleBonus + 연출 보정
int32 Safety;             // 0~100, 높을수록 안전 (2026-08-06 확정)
int32 Gold;               // 잔액. 지출로 줄어든다. 초기 10,000 (밸런스 시드 v0)
int32 TotalRevenue;       // 축제 전체 누적 매출. 줄어들지 않는다
int32 TodayRevenue;       // 오늘 하루 매출
```

**Gold와 Revenue는 다른 값입니다.** 대략 `Gold ≒ 초기자금 + TotalRevenue - 누적지출`이지만
**UI는 이 식을 계산하지 않습니다.** 셋 다 소유자에게서 그대로 받습니다.
`TodayRevenue`도 UI가 누적값에서 빼서 만들지 않습니다 — Report에 필요하니 따로 주세요.

**갱신 주기: 일정 틱마다.** ViewModel이 0.1초마다 샘플링하고 **정수값이 실제로 바뀔 때만** 델리게이트를
쏩니다. 소스가 매 틱 흔들려도 위젯 갱신은 지표당 초당 10회로 묶입니다.
소유자 쪽에서 별도 스로틀을 넣지 않으셔도 됩니다.

**좋고 나쁨 구간 (2026-08-06 UI 결정):** 0~100 지표 셋은 방향이 같아서(높을수록 좋음) 구간을 공유합니다.
`좋음 70~100` / `주의 40~69` / `나쁨 0~39`. 값은 `UFF_UIMetricBandSettings`에 있고
Project Settings > Game > FestaFiesta UI Metric Bands 에서 조정합니다. 코드에 하드코딩돼 있지 않습니다.
AT-011에 따라 색 + 라벨 + 아이콘을 함께 씁니다.

`Gold` · `TotalRevenue` · `TodayRevenue`는 상한이 없어 절대 구간이 없습니다.
Gold는 "이걸 살 수 있나"(가격 대비), 매출은 전날 대비 증감으로 표현합니다.

아직 미확인:

- Gold가 음수가 될 수 있는지 (지금은 클램프하지 않음)

### Satisfaction 이중 지표 — 화면 배치 (2026-08-06 UI 결정)

`digital-twin-pipeline.md` 7절은 "UI에서도 둘을 구분해 표시한다"까지만 정해 놨습니다. 화면은 UI에서 정했습니다.

| 화면                  | 표시                                                   | 이유                                                                                               |
| ------------------- | ---------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| Overview HUD (Live) | `SimSatisfaction` **하나만**                            | Live 중엔 FinaleBonus가 없어 두 값이 같습니다. 같은 숫자를 두 번 띄우지 않습니다. 이때 플레이어가 개선할 수 있는 것도 SimSatisfaction뿐입니다 |
| Report              | **둘 다, 분해식으로** — `관객 만족도 62` `+ 피날레 20` `= 평가 점수 82` | 두 지표의 관계가 화면 자체로 설명됩니다                                                                           |
| Finale 구매           | `ScoreSatisfaction` **예상치** — "S 구매 시 → 92"          | 지표를 나눈 기획 의도가 그대로 구매 판단 근거가 됩니다                                                                  |

화면 라벨은 내부 이름을 그대로 쓰지 않습니다 — `SimSatisfaction` → **관객 만족도**,
`ScoreSatisfaction` → **평가 점수**. Report 드릴다운(AT-005)은 왼쪽 항인 관객 만족도에 걸립니다.

### Slot을 노출하지 않습니다

2026-08-06 12시 스크럼에서 하루 4 Slot 구조를 없애기로 했고 16:57에 `Reconfigure` 삭제도 승인됐습니다.
`UFF_FestivalStageSubsystem::CurrentSlot`이 아직 남아 있지만 ViewModel은 참조하지 않습니다.
**시간 담당이 실제로 제거해도 UI는 깨지지 않습니다.**

### 게임 시계는 Live에서만 흐릅니다

2026-08-06 16:59 시간 회의 결정입니다. UI 표시에 직접 영향이 있습니다.

| Stage              | 시간의 의미                                       |
| ------------------ | -------------------------------------------- |
| `Setup`            | 게임 시각과 분리된 **UI 제한시간**                       |
| `PromotionPreview` | 게임 시각 진행 안 함                                 |
| `Live`             | **여기서만** 게임 시계가 흐름. 축제별 `LiveStartTime`에서 시작 |

`LiveStartTime`은 축제 데이터에서 받습니다. 다대포의 17:00을 공통 UI에 하드코딩하지 않습니다.

### `UFF_MetricsHudWidget`

- **헤더**: `Public/UI/FF_MetricsHudWidget.h`
- **상태**: `구현완료` (Phase 0 산출물)

지표를 숫자로만 표시합니다. 좋고 나쁨 구간이 정해지기 전이라 **색을 칠하지 않았습니다.**
임의로 정하면 나중에 전부 바꿔야 하고, 색만으로 구분하면 AT-011 위반입니다.

### 이미 만든 UI 베이스 (참고용)

| 클래스                         | 헤더                                     | 용도                                                   |
| --------------------------- | -------------------------------------- | ---------------------------------------------------- |
| `UFF_UIRootLayout`          | `Public/UI/FF_UIRootLayout.h`          | HUD·Screen·Panel·Toast 레이어. `PushScreen`/`PopScreen` |
| `UFF_ActivatableScreenBase` | `Public/UI/FF_ActivatableScreenBase.h` | 화면 베이스. 입력 모드·초기 포커스                                 |
| `UFF_CommonButtonBase`      | `Public/UI/FF_CommonButtonBase.h`      | 버튼 베이스                                               |
| `LogFestaFiestaUI`          | `Public/UI/FF_UILog.h`                 | UI 로그 카테고리                                           |

다른 분이 UI를 만들 일이 있으면 이 클래스들을 부모로 쓰시면 됩니다.
**시간 정지 권한은 UI에 없습니다** — 화면을 열어도 시뮬레이션은 계속 흐릅니다.

---

# UI가 필요한데 아직 없는 것

기획서 6.3의 Overview 필수 표시 항목입니다.

| 필요한 것                                         | 예상 소유자    | 상태        | UI에서 쓰는 곳                                                          |
| --------------------------------------------- | --------- | --------- | ------------------------------------------------------------------ |
| **Satisfaction** (`SimSatisfaction` 0~100)    | 김시리 · 이정원 | ❌ 없음      | Overview 지표, Report, SNS                                           |
| **`ScoreSatisfaction`** (= Sim + FinaleBonus) | 김시리       | ❌ 없음      | Report 분해식, Finale 구매 예상치                                          |
| **`Gold`** (잔액)                               | 김시리       | ❌ 없음      | 공통 상단 바, Finale·설치 구매 가능 판정                                        |
| **`TotalRevenue`** (축제 누적 매출)                 | 김시리       | ❌ 없음      | 공통 상단 바                                                            |
| **`TodayRevenue`** (오늘 매출)                    | 김시리       | ❌ 없음      | Report — UI가 누적값에서 빼지 않습니다                                         |
| **Safety**                                    | 이정원       | ❌ 없음      | Overview 지표, 경고                                                    |
| Staff 보유·가용 인원                                | 김시리       | ❌ 없음      | Event 배치 UI                                                        |
| `CauseId` 롤업                                  | 김시리       | ⚠️ 창구만 있음 | Report 드릴다운 (AT-005) — `UFF_NpcNeedMetricsSubsystem`이 원본, 채우는 쪽 대기 |
| 저장 슬롯 목록                                      | 김시리       | ❌ 없음      | 플레이 허브                                                             |

**UI는 위 항목들을 하드코딩 더미로 먼저 만들어 8/8에 커밋합니다.**
실제 값이 생기면 `UFF_FestivalViewModelSubsystem` 안쪽만 교체하고 위젯은 그대로 둡니다.

**소유자분들께** — 만드실 때 아래 두 가지만 지켜 주시면 UI가 바로 붙습니다.

1. 현재값 Getter + 변경 델리게이트를 **쌍으로** 제공
2. 값의 **단위와 범위**를 헤더 주석에 명시 (0~100인지 0~1인지, 초인지 분인지)

---

## 변경 이력

| 날짜         | 작성자 | 내용                                                                                             |
| ---------- | --- | ---------------------------------------------------------------------------------------------- |
| 2026-08-06 | 백종태 | 문서 신설. 현재 리포에서 확인한 API를 초기 채움                                                                  |
| 2026-08-06 | 백종태 | Revenue를 축제 누적으로 확정하고 `TodayRevenue`·`Gold` 분리. 갱신 주기를 일정 틱으로 확정. Satisfaction 이중 지표의 화면 배치 결정 |
| 2026-08-06 | 김시리 | `UFF_NpcNeedMetricsSubsystem`·`FFF_DestinationDecision` 항목 추가                                  |
