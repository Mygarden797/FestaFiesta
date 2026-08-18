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
| `IsReconfigureAllowed()`               | `bool`              | 배치 변경 허용 여부 (Setup/PromotionPreview/MorningCheck) |
| `SetTimeScale(float)`                  |                     | 배속 변경                             |
| `IsMainProgramLocked()`                | `bool`              | 메인 공연 배속 잠금 구간 여부                 |
| `AdvanceToNextDay()`                   |                     | Report에서 다음 날 진입                  |
| `OnRemainingSecondsChanged`            | `int32 NewSeconds`  | 남은 시간이 초 단위로 바뀔 때 (2026-08-11 전 단계로 확장) |
| `OnFestivalEnded`                      | (인자 없음)             | Day3 Report 종료 시 1회                |
| `SkipCurrentStage()`                   |                     | 스킵 가능한 단계를 즉시 넘김 (2026-08-11 추가)   |
| `IsLiveStage()` / `IsReportStage()`    | `bool`              | 단계 판정 (2026-08-11 추가)              |
| `IsFinalDay()`                         | `bool`              | 마지막 날 여부. **UI가 총 일수를 하드코딩하지 않게 해줍니다** (2026-08-11 추가) |
| `OnMainProgramStarted`                 | `FString EventLabel` | 메인 공연 관련 알림 (2026-08-11 추가). **아래 주의 3건 참고** |
| `OnMainProgramEnded`                   | `FString EventLabel` | 메인 공연 잠금 구간 종료 (2026-08-11 추가) |
| `GetFacilityServiceRatePerMinute()`     | `float`             | (Queue) 시설 실측 처리율. 미등록 시설은 0 — **나누지 말 것** |

**`OnMainProgramStarted` 사용 시 주의 3건 (백종태 확인, 2026-08-12)** — 토스트 알림 만들 때 다시 읽습니다.

1. **이 이벤트는 공연 "시작"이 아니라 "배속 잠금 시작" 시점입니다.**
   잠금은 `TriggerSeconds - MainProgramLockLeadSeconds`부터 시작하고 리드타임 기본값이 10초라,
   **실제 공연보다 10초 먼저** 옵니다. 문구를 `다대불꽃쇼 시작!`으로 쓰면 틀립니다 — `곧 시작`으로 씁니다.
   리드타임이 60초로 바뀌면 오차가 그만큼 커집니다.
2. **메인 공연이 그날의 마지막 이벤트면 종료 알림이 잘못 발생합니다.**
   `IsMainProgramLocked()`의 `LockEnd`가 *다음* 이벤트 시각인데 다음이 없으면 `LockEnd = TriggerSeconds`가 되어,
   잠금 구간이 `[시작-10초, 시작]`이 됩니다. 그러면 **공연이 시작하는 순간 `OnMainProgramEnded`가
   빈 라벨(`""`)로 발생**합니다. `DT_ProgramSchedule`에 메인 공연 뒤 이벤트가 하나라도 있으면 문제없습니다.
3. **`EventLabel`이 `FString`이라 현지화 대상이 아닙니다.**
   전 세계 축제로 확장하는 제품이라 프로그램 이름은 `FText`가 맞습니다.
   UI가 `FText::FromString`으로 감싸면 화면에는 뜨지만 번역에서 빠집니다.
   근본 해결은 `FFF_LiveTriggerEvent::EventLabel`을 `FText`로 바꾸는 것입니다 — 강민서 확인 필요.

**삭제된 것**: `CurrentSlot` (하루 4타임 구조 없어짐, Live는 하루에 한 번)
**UI 사용처**: 상단 바(Stage/Day/시계), Morning Check 진입 판정, 배속 버튼 활성/비활성(IsMainProgramLocked 참조),
Report의 `다음 날` 버튼(`IsReportStage` + `IsFinalDay`로 게이팅)

**요청** — `RemainingTime`이 매 틱 바뀌므로 UI가 폴링해야 합니다.
초 단위로 바뀔 때만 쏘는 델리게이트가 있으면 좋겠습니다. 없으면 UI 쪽에서 0.1초 폴링 + 변화 감지로 처리하겠습니다.

**답변** 1, 2번 반영 완료했습니다.

문구는 {EventLabel} 곧 시작으로 수정했습니다. 지적해주신 대로 이 이벤트는 "잠금 시작" 시점이라 실제 공연 시작과 리드타임만큼 오차가 있다는 점 인지했고, 나중에 리드타임 값이 바뀌면 문구 재검토가 필요하다는 것도 기억해두겠습니다.
IsMainProgramLocked()의 LockEnd 계산을 수정했습니다. 다음 이벤트가 없을 때 LockEnd = TriggerSeconds(자기 자신) 대신 GetLiveDurationForCurrentDay()(그날 Live 전체 길이)로 대체되도록 바꿔서, 메인 공연이 그날 마지막 이벤트여도 시작하자마자 빈 라벨로 종료 알림이 발생하는 문제를 해결했습니다.

3번은 데이터 재입력 (20개 행), 델리게이트 2개 + 변수 1개 타입 변경, 코드 여러 곳 호출부 수정, 블루프린트 노드 재연결 (2곳) 이 달라집니다 그래서 지금 작업하기에는 서로 꼬일 수가 있어서 마무리 후에 시간이 된다면 작업해도 될지 의견을 말해봅니다!



FOnRemainingSecondsChanged (int32 NewSeconds)

구독 방법:
GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>()
->OnRemainingSecondsChanged.AddDynamic(this, &YourClass::YourHandler);

이거 구독하시면 폴링 없이 초 단위로 바뀔 때만 UI 갱신하시면 됩니다.

**✅ 처리 완료 (2026-08-11, 백종태)** — `UFF_FestivalViewModelSubsystem`이 구독으로 전환하고
Tick 폴링을 걷어냈습니다. `IsFinalDay()`도 `CanAdvanceToNextDay()`에 바로 물렸습니다. 감사합니다.

**요청 2건 (백종태 → 강민서) — ✅ 2026-08-13 둘 다 반영됨**

1. **`IsCurrentStageSkippable()`을 `public` + `BlueprintPure`로 열어주세요.**
   지금 `private`이라 **블루프린트는 부를 수 있는데 C++에서는 못 부릅니다.**
   그래서 ViewModel의 `CanSkipCurrentStage()`가 "Live도 Report도 아니면 true"라는 근사값을 쓰고 있습니다.
   DaySequence 표에서 `bSkippable=false`인 단계도 버튼이 켜져 보일 수 있습니다.
   열어주시면 ViewModel 본문만 교체하고 위젯은 손대지 않습니다.

2. **`OnRemainingSecondsChanged`가 Stage 전환 직후 값을 놓칠 수 있습니다.**
   `LastBroadcastSeconds`가 전환 시 초기화되지 않아서, 새 단계의 첫 정수 초가 이전 값과 같으면
   방송이 오지 않습니다. `RequestTransition`에서 `LastBroadcastSeconds = -1`로 리셋하면 해결됩니다.
   UI 쪽은 전환 시 직접 현재값을 읽어 맞추는 방어를 넣어 뒀으니 급하지는 않습니다.

**답변**

1, 2번 둘 다 반영 완료했습니다.

1. `IsCurrentStageSkippable()`을 `private`에서 `public` + `BlueprintPure`로 옮겼습니다. C++에서 바로 호출 가능하니, ViewModel의 `CanSkipCurrentStage()`에서 이 함수를 직접 쓰시면 됩니다. `bSkippable=false`인 단계에서 근사값 대신 정확한 값을 받으실 수 있습니다

2. `RequestTransition()`에 `LastBroadcastSeconds = -1;` 리셋 추가했습니다. Stage 전환 시 새 단계의 첫 초도 놓치지 않고 방송되도록 처리했습니다. UI 쪽 방어 로직은 그대로 유지하셔도 되고, 걷어내셔도 상관없을 것 같습니다.

두 건 다 컴파일 확인까지 마쳤습니다


### `OnDayEnded`

- **헤더**: `Public/Time/FF_FestivalStageSubsystem.h`
- **접근**: `UFF_FestivalStageSubsystem` (GameInstanceSubsystem)
- **시그니처**: `FOnDayEnded(int32 EndedDay, float TodayLiveDuration)`
- **갱신 주기**: 이벤트 (AdvanceToNextDay 호출 시, 하루 종료 시점 1회)
- **상태**: `구현완료`
- **비고**: EndedDay는 방금 끝난 날짜(1~3), TodayLiveDuration은 그날 Live 총 진행 시간(초). 정비/청결 등 Day Modifier 이월 계산은 이 신호를 받는 쪽(시설/경제)이 담당 — 시간 시스템은 기준 파라미터 전달까지만 책임진다.

| 멤버                                     | 형                   | 설명                                |
| -------------------------------------- | ------------------- | --------------------------------- |
| `OnDayEnded`                           | `FOnDayEnded`       |(EndedDay, TodayLiveDuration) — 하루 종료 시 이월 정보 전달     |


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
`WorldLocation`(시설 Actor 대표 월드 지점, cm),
`bHasApproachWorldTransform`, `ApproachWorldTransform`(FacilityDefinition `Entry[0]` 기반 NPC 고정 접근점, 월드 좌표)

NPC는 시설 이동 전에 `bHasApproachWorldTransform`을 확인하고 `ApproachWorldTransform`까지 이동한다.
Queue 가입 성공 후에는 이 고정 접근점이 아니라 `FFF_OnQueueSlotTargetsUpdated`가 제공하는 동적 Slot 위치를 사용한다.
`Entry[0]`은 가입 전 admission/staging 지점이며 Queue tail로 해석하지 않는다.
`Entry[0]`을 Slot 0과 겹쳐 저작하면 안 된다. FFZS는 이동 목표의 XY 중심을
`max(1cm, 2 × SlotArrivalToleranceCm)` 이상 분리하고, FestaFiesta 프로덕션 콘텐츠는 기본 Crowd 반경
34cm를 반영해 Anchor와 Queue Slot 사이에 최소 68cm를 확보한다. Entry와 Queue Slot의 NPC 몸체 원은
`QueueReservedRects` 안에 완전히 포함되어야 한다.

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
⚠️ **지금 값이 들어오는 항은 `AttractionTerm`·`DistanceCostTerm`·`DensityCostTerm` 셋뿐입니다** (2026-08-18 확인).
취향·Need·대기시간은 개체별 상태를 붙이는 작업이 남아서 **0으로 나옵니다** — 화면 버그가 아닙니다.

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

### 유료 프로그램 예약 (2026-08-11 신설)

- **헤더**: `Public/NPC/Party/FF_ProgramReservation.h`, `FF_ProgramReservationReturnTrip.h`
- **스펙**: `data/metadata/spec/reservation_program_spec.md`
- **상태**: 규칙·자료구조 `구현완료`. **UI가 읽을 창구는 아직 없습니다**

`FFF_ProgramReservation`·`FFF_ReservationCandidateGroup`이 `BlueprintType` + `BlueprintReadOnly`라 값은 볼 수
있지만 **예약 목록을 돌려주는 Getter가 없습니다.** 지금 공개된 것은 정적 규칙 클래스(`FFF_ProgramReservationRules`)뿐입니다.

**Setup·MorningCheck에 유료 프로그램 구매 화면이 필요하고 담당은 백종태입니다** (2026-08-12 확인).
경제 쪽 구매 처리는 하루 경계(2026-08-18 완료) 다음 순서라 이번 주에 붙습니다.

**요청 (백종태 → 김시리)** — 화면을 만들려면 다음이 필요합니다. 형태는 맞춰 드립니다.

1. 구매 가능한 프로그램 목록 (이름·가격·정원·시각) — `Setup`·`MorningCheck`에서 요청 시
2. 현재 예약 상태 (슬롯 수, 배정된 그룹 수, 남은 자리)
3. 구매·취소 **Command** (UI가 상태를 직접 쓰지 않습니다. 실패 시 이유를 결과로 받습니다)
4. 예약 변경 시 알려주는 델리게이트

시간 필드 기준이 **Live 경과분**이라고 헤더에 적혀 있습니다. 화면에 표시할 때
게임 시각으로 바꿀 필요가 있으면 변환 지점을 한 곳으로 두겠습니다.

### 경제 값 5종 — ViewModel 연결 요청 (김시리 → 백종태, 2026-08-18 정리)

- **헤더**: `Public/Festival/FF_FestivalProgressionSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **상태**: 원본 API 전부 `구현완료`. **ViewModel 연결 대기** — 여기 한 곳만 붙이면 화면에 뜹니다

| 값 | Getter | 델리게이트 | 단위·범위 | 화면 |
|---|---|---|---|---|
| 잔액 | `GetGold()` | `OnGoldChanged` | Gold, 0 이상·상한 없음 | 상단 바, 구매 판정 |
| 누적 매출 | `GetTotalRevenue()` | `OnTotalRevenueChanged` | Gold, **줄어들지 않음** | 상단 바 |
| 오늘 매출 | `GetTodayRevenue()` | `OnTodayRevenueChanged` | Gold, 매일 아침 0 | Report |
| 🆕 밀린 임대료 | `GetTotalArrears()` | `OnTotalArrearsChanged` | Gold, 0 이상 | 상단 바 자금난 경고, Report |
| 🆕 오늘 일하는 인원 | `GetTotalStaffCount()` | `OnTotalStaffCountChanged` | 명, 6~12 | Event 배치, 아침 고용 화면 |

**연결 작업 4가지**

1. ViewModel 초기화 때 위 5개 Getter로 초기값을 한 번 동기화해 주세요.
2. 델리게이트 5개를 구독해 ViewModel 값을 갱신하고, `Deinitialize`에서 구독을 해제해 주세요.
   다섯 개 다 같은 모양(Getter + 변경 델리게이트 쌍)이고 **값이 실제로 바뀔 때만 발행됩니다.**
3. 구매 버튼은 `CanAfford(int32 CostGold)`를 그대로 중계해 주세요. 위젯이 잔액과 가격을 직접 비교하지 않습니다.
4. 다섯 개 다 델리게이트가 있으니 0.1초 폴링 대상에서 빼도 됩니다.

- 인원은 **기본 팀(`GetBaseStaffCount()`, 무페이·해고 불가) + 오늘 쓴 알바(`GetTemporaryStaffCount()`)** 로
  쪼개서 보여주실 수 있습니다. 두 숫자 다 데이터 애셋 값이라 밸런싱으로 바뀝니다
- **오늘 매출을 누적 매출에서 빼서 만들지 마세요.** 따로 들고 있는 값입니다 — 하루 경계에서 오늘 매출만 0이 되고
  누적은 남기 때문에 빼서 만들면 날짜가 바뀔 때마다 어긋납니다

### 🔴 화면 만들 때 알아야 하는 것 5가지 (2026-08-18)

**1. "다음 날" 버튼이 이제 돈을 걷습니다. 종태님이 고치실 것은 없습니다.** 이미 만들어 두신
`RequestAdvanceToNextDay()`가 하루치 임대료 2,400을 걷고, 오늘 매출을 0으로 되돌리고, 어제 쓴 알바를 퇴근시킵니다.
값 4개(잔액·밀린 임대료·오늘 매출·인원)가 한꺼번에 바뀌고 전부 델리게이트가 옵니다.

**2. 잔액을 UI가 직접 계산하지 말아 주세요.** 버튼 직후에 `잔액 − 2,400`을 계산하면 어긋납니다 — 잔액이 부족하면
실제로 빠지는 금액은 2,400이 아니라 "있는 만큼"입니다. 델리게이트가 주는 값을 그리시면 항상 맞습니다.

**3. 게임을 켜면 잔액이 이미 7,600입니다.** 1일차는 셋업으로 시작해 아침 단계를 지나지 않으므로 축제 시작 시점에
정산됩니다. 초기 자금 10,000이 화면에 안 보이는 것이 정상입니다(3일 임대료 합계 7,200 = 초기 자금의 72%).

**4. 잔액이 부족하면 "밀린 임대료"가 생깁니다.** 시설은 안 사면 되지만 축제장 임대료는 거절할 수가 없어서,
있는 만큼만 내고 못 낸 금액을 따로 적어 둡니다. 이자는 없습니다.

- **잔액은 음수가 되지 않습니다.** "잔액 −1,400"이 아니라 잔액 0 + 밀린 임대료 1,400입니다
- **`GetGold()`만 보면 자금난이 안 보입니다.** 잔액 0은 "다 썼다"일 수도 "빚까지 있다"일 수도 있어서,
  경고는 **밀린 임대료 > 0** 으로 판정해 주세요
- 날짜가 바뀌어도 **사라지지 않고 쌓입니다.** 매일 아침 0으로 돌아가는 건 오늘 매출과 알바뿐입니다
- 축제가 끝났을 때 남은 미납을 어떻게 처리할지는 **아직 안 정했습니다.** 결산 화면에 필요하면 같이 정하겠습니다

**5. 밀린 임대료가 있는 동안에는 매출이 들어와도 잔액이 안 늘어납니다.** 들어온 돈이 잔액에 쌓이기 전에 빚을 먼저
갚기 때문입니다. 예: 밀린 임대료 1,400인 상태에서 매출 500이 들어오면 매출 2종과 `OnTotalArrearsChanged`(1,400 →
900)는 발행되지만 **`OnGoldChanged`는 발행되지 않습니다**(잔액 0 → 0).

- 값이 바뀔 때만 발행하는 규칙 때문에 **"매출은 올랐는데 잔액 이벤트가 안 오는" 구간이 정상적으로 존재합니다.**
  누락이 아닙니다
- 플레이어에게는 "매출이 늘었는데 돈이 안 늘었다"로 보입니다. **밀린 임대료를 잔액 옆에 상시 노출**하시면 숫자가
  줄어드는 게 보여서 설명이 됩니다(0일 때는 숨기셔도 됩니다)
- `OnTotalArrearsChanged`는 하루 경계뿐 아니라 **매출·철거 환급이 들어오는 아무 시점에나** 옵니다
- ⚠️ 같은 이유로 **Report의 임대료 지출이 2,400을 넘을 수 있습니다.** 어제 못 낸 몫을 오늘 갚으면 오늘 임대료에
  합산됩니다(늦게 나간 임대료라 별도 항목을 만들지 않았습니다). 하루 임대료를 2,400으로 박아두지 마세요

⚠️ 헤더에 `BeginNewDay()`가 보이는데 **UI에서 부르지 말아 주세요.** 날짜를 넘기는 쪽이 둘이 되면 하루에 임대료가
두 번 빠집니다. 부르는 곳은 경제 쪽에서 한 곳으로 잡아 두었습니다.

### 🆕 피날레 구매 화면이 필요합니다 (2026-08-18, 담당 백종태)

Day3 아침(MorningCheck)에 피날레 등급을 사는 화면입니다. 경제 쪽은 다 됐습니다.

- **버튼 활성화**: `UFF_StageEconomyBridgeSubsystem::CanPurchaseFinaleNow()` — Day3 MorningCheck이고
  아직 안 샀을 때만 `true`. **표시용이고 구매를 막지는 않습니다**(`CanSkipCurrentStage`와 같은 성격)
- **목록**: 등급·비용·보너스는 `GetDefinition()->FinaleTiers`에 있습니다.
  C/B/A/S = 2,000/4,000/6,000/8,000 Gold, 만족도 +5/+12/+20/+30. UI가 쓰기 편한 Getter가 필요하시면 만들겠습니다
- **구매**: `TryPurchaseFinale(EFF_FinaleGrade)` → `EFF_FinalePurchaseResult`.
  실패 이유가 한글 `DisplayName`으로 들어 있어서 그대로 화면에 쓰실 수 있습니다
  (`Gold 부족` / `이미 구매했다` / `구매할 수 없는 등급` 등)
- **구매 결과**: `GetPurchasedFinaleGrade()` + `OnFinaleGradeChanged(EFF_FinaleGrade)`. 안 샀으면 `None`

⚠️ **구매는 축제당 1회이고 되돌릴 수 없습니다.** 등급 변경·취소·환불이 없으니 **확인창을 넣어 주세요.**
두 번째 시도는 `이미 구매했다`로 거부됩니다.

**만족도 보너스는 Gold가 아닙니다.** `GetFinaleSatisfactionBonus()`가 0~100 척도의 가산점(A면 20)을
돌려주는데, 매출에도 잔액에도 섞이지 않습니다. 최종 평가 점수(`ScoreSatisfaction` = 관객 만족도 + 이 보너스)의
합산은 관객 만족도가 아직 없어서 붙이지 않았습니다 — 지금은 "몇 점짜리를 샀는가"까지만 보여드릴 수 있습니다.

밀린 임대료가 있으면 잔액이 0이라 자연히 못 삽니다. 이때 실패 이유는 `Gold 부족`으로 옵니다.

### 아직 만들 것 — 형태만 정해 주세요

**아침 고용 화면**은 판정이 이미 있습니다. `GetAffordableTemporaryStaffCount()`(지금 잔액으로 가능한 최대 인원,
값을 바꾸지 않으니 슬라이더 최대치·버튼 판정에 그대로) + `TryHireTemporaryStaff(int32 인원)`.
**부분 고용은 없습니다** — 5명 요청에 3명분 돈만 있으면 전부 거부되고 아무것도 바뀌지 않습니다. 실패 이유
(돈 부족 / 하루 상한 초과)를 화면에 띄우시려면 말씀만 주세요, 지금은 `true`/`false`만 돌려줍니다.

**Report 드릴다운**은 지출·매출이 원인별(`EFF_SpendReason`/`EFF_EarnReason`, 한글 `DisplayName` 있음), 매출은
부스별로도 쌓이고 있습니다. UI Getter를 아직 안 만들었으니 어떤 표로 필요하신지 알려주시면 그 모양대로 만들겠습니다.

**시설 철거 환급은 매출이 아닙니다** (2026-08-13). 설치비 50%가 돌아오면서 `OnGoldChanged`는 오지만
`OnTotalRevenueChanged`는 오지 않습니다. "잔액 상승 = 매출 상승"으로 가정한 코드가 있으면 확인해 주세요.

아직 없는 경제 값은 **일차 결과(등급 산출)와 저장 API**입니다.

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
| Gold              | `GetGold()`                           | `OnGoldChanged`              | **원본 API 완료 · ViewModel 연결 요청** |
| TotalRevenue      | `GetTotalRevenue()`                   | `OnTotalRevenueChanged`      | **원본 API 완료 · ViewModel 연결 요청** |
| TodayRevenue      | `GetTodayRevenue()`                   | `OnTodayRevenueChanged`      | **원본 API 완료 · ViewModel 연결 요청** |
| Stage / Day       | `GetCurrentStage()` `GetCurrentDay()` | `OnStageViewChanged`         | **진짜 값**  |
| 남은 시간             | `GetRemainingSeconds()`               | `OnRemainingSecondsChanged`  | **진짜 값 · 구독 중계** |

### Command (2026-08-11 신설)

**위젯은 다른 시스템의 함수를 직접 부르지 않습니다.** 쓰기 요청은 전부 여기를 거칩니다(원칙 1).
낙관적 갱신을 하지 않으므로, 호출한 쪽이 화면을 직접 바꾸지 말고 결과 이벤트를 기다립니다.

| Command                     | 활성 조건 Getter          | 위임 대상                                 |
| --------------------------- | -------------------- | ------------------------------------- |
| `RequestAdvanceToNextDay()` | `CanAdvanceToNextDay()` | `UFF_FestivalStageSubsystem::AdvanceToNextDay()` |
| `RequestSkipCurrentStage()` | `CanSkipCurrentStage()` | `UFF_FestivalStageSubsystem::SkipCurrentStage()` |

- `CanAdvanceToNextDay()` = `IsReportStage() && !IsFinalDay()`. 총 일수를 UI가 모릅니다.
- `CanSkipCurrentStage()`는 `IsCurrentStageSkippable()`을 그대로 중계합니다 (2026-08-13, 근사값 제거).
  **버튼 표시용입니다.** `RequestSkipCurrentStage()`는 이 값으로 막지 않습니다 — 판정 소유자를 두 곳에 두지 않습니다.

### 값 규격 (2026-08-06 UI 결정)

```cpp
int32 SimSatisfaction;    // 0~100, 높을수록 좋음. 대기·밀도·취향 매칭에서 나오는 트윈 산출물
int32 ScoreSatisfaction;  // 0~100, 높을수록 좋음. SimSatisfaction + FinaleBonus + 연출 보정
int32 Safety;             // 0~100, 높을수록 안전 (2026-08-06 확정)
int32 Gold;               // 잔액. 지출로 줄어들고, 매출·시설 철거 환급으로 늘어난다. 초기 10,000 (밸런스 시드 v0)
int32 TotalRevenue;       // 축제 전체 누적 매출. 줄어들지 않는다
int32 TodayRevenue;       // 오늘 하루 매출
```

**Gold와 Revenue는 다른 값입니다.** 대략 `Gold ≒ 초기자금 + TotalRevenue - 누적지출 + 누적환급`이지만
**UI는 이 식을 계산하지 않습니다.** 전부 소유자에게서 그대로 받습니다.
`TodayRevenue`도 UI가 누적값에서 빼서 만들지 않습니다 — Report에 필요하니 따로 주세요.

🆕 **시설 철거가 `OnGoldChanged`를 쏩니다 (2026-08-13).** 설치비의 50%를 돌려받습니다
(`FacilityDemolitionRefundRate`, 기획 조정 가능). **`OnTotalRevenueChanged`는 안 쏩니다** — 환급은
매출이 아니라 이미 쓴 돈이 되돌아온 것입니다. `Gold`만 구독하고 있다면 그대로 잘 작동하지만,
"Gold가 늘었다 = 매출이 늘었다"로 가정하는 코드가 있다면 깨집니다.

**갱신 주기: 일정 틱마다.** ViewModel이 0.1초마다 샘플링하고 **정수값이 실제로 바뀔 때만** 델리게이트를
쏩니다. 소스가 매 틱 흔들려도 위젯 갱신은 지표당 초당 10회로 묶입니다.
소유자 쪽에서 별도 스로틀을 넣지 않으셔도 됩니다.

**좋고 나쁨 구간 (2026-08-06 UI 결정):** 0~100 지표 셋은 방향이 같아서(높을수록 좋음) 구간을 공유합니다.
`좋음 70~100` / `주의 40~69` / `나쁨 0~39`. 값은 `UFF_UIMetricBandSettings`에 있고
Project Settings > Game > FestaFiesta UI Metric Bands 에서 조정합니다. 코드에 하드코딩돼 있지 않습니다.
AT-011에 따라 색 + 라벨 + 아이콘을 함께 씁니다.

`Gold` · `TotalRevenue` · `TodayRevenue`는 상한이 없어 절대 구간이 없습니다.
Gold는 "이걸 살 수 있나"(가격 대비), 매출은 전날 대비 증감으로 표현합니다.

Gold는 음수가 되지 않습니다. 잔액 부족 지출은 0으로 클램프하지 않고 거부되며 기존 잔액을 유지합니다.

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
| **`Gold`** (잔액)                               | 김시리       | ⚠️ 원본 완료·ViewModel 연결 대기 | 공통 상단 바, Finale·설치 구매 가능 판정                                        |
| **`TotalRevenue`** (축제 누적 매출)                 | 김시리       | ⚠️ 원본 완료·ViewModel 연결 대기 | 공통 상단 바                                                            |
| **`TodayRevenue`** (오늘 매출)                    | 김시리       | ⚠️ 원본 완료·ViewModel 연결 대기 | Report — UI가 누적값에서 빼지 않습니다                                         |
| 🆕 지출·매출 **원인별 분해** (`EFF_SpendReason`/`EFF_EarnReason`) | 김시리 | ⚠️ 내부 집계만 있음 | Report 드릴다운. UI Getter는 형태 협의 후 추가 |
| 🆕 **부스별 매출 표** (`RevenueByFacility`)          | 김시리       | ⚠️ 내부 집계만 있음 | Report — 부스별 수익 표 |
| **Safety**                                    | 이정원       | ❌ 없음      | Overview 지표, 경고                                                    |
| Staff 보유·가용 인원                                | 김시리       | ⚠️ 원본 완료·ViewModel 연결 대기 | Event 배치 UI, MorningCheck 고용 화면 |
| 🆕 **임대료 미납 총액** (`GetTotalArrears`)          | 김시리       | ⚠️ 원본 완료·ViewModel 연결 대기 | 상단 바 자금난 경고, Report |
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
| 2026-08-11 | 백종태 | Stage 표에 `SkipCurrentStage`·`IsFinalDay`·`IsLiveStage`·`IsReportStage`·`OnFestivalEnded` 반영. 남은 시간을 폴링→구독으로 전환 완료 표시. ViewModel Command 2종 신설. 강민서에게 남은 요청 2건 기록 |
| 2026-08-12 | 백종태 | `OnMainProgramStarted`·`OnMainProgramEnded` 항목과 사용 주의 3건 추가. 유료 프로그램 예약 절 신설 — UI 담당 확정과 필요 API 4건 요청 |
| 2026-08-12 | 김시리 | `UFF_FestivalProgressionSubsystem`의 Gold Getter·델리게이트·`CanAfford` 구현 완료. ViewModel 초기 동기화·이벤트 중계·구매 가능 판정 연결 요청 |
| 2026-08-13 | 강민서 | 백종태 요청 2건 반영(`IsCurrentStageSkippable()` public+BlueprintPure 전환, `RequestTransition`에 `LastBroadcastSeconds` 리셋 추가) 및 사용 주의 2건 반영(`OnMainProgramStarted` UI 문구 "곧 시작"으로 수정, `IsMainProgramLocked()` 마지막 이벤트 빈 라벨 버그 수정). `EventLabel` FText 전환은 환경 작업 마무리 후 진행 예정 |
| 2026-08-13 | 백종태 | 강민서 반영분 수용 — `CanSkipCurrentStage()`의 근사값을 걷어내고 `IsCurrentStageSkippable()` 직접 중계로 교체. `RequestSkipCurrentStage()`는 여전히 이 값으로 막지 않음 |
| 2026-08-13 | 김시리 | `TotalRevenue`·`TodayRevenue` 원본 API 구현 완료(`GetTotalRevenue`/`GetTodayRevenue` + 델리게이트 쌍) — ViewModel 연결 요청. `FFF_DayLedger` 신설로 지출·매출 원인별 분해와 부스별 매출 집계가 내부적으로 쌓이기 시작 — UI Getter는 필요한 형태를 확인한 뒤 추가 예정 |
| 2026-08-18 | 김시리 | 🆕 **피날레 구매가 구현됐습니다** — Day3 MorningCheck 구매 화면이 필요합니다(담당 백종태). 버튼 활성화는 `CanPurchaseFinaleNow()`, 구매는 `TryPurchaseFinale()`이고 실패 이유가 한글로 옵니다. **1회 구매·변경 불가라 확인창이 필요합니다.** 만족도 보너스는 Gold가 아니고 평가 점수 합산은 관객 만족도가 생길 때 붙입니다 |
| 2026-08-18 | 김시리 | 🆕 **"다음 날" 버튼이 이제 하루치 임대료를 정산합니다** — `RequestAdvanceToNextDay()`를 누르면 잔액·밀린 임대료·오늘 매출·인원 4개 값이 바뀝니다(전부 기존 델리게이트로 통지). 종태님 쪽 수정은 없고, 버튼 직후 UI가 잔액을 직접 계산하지만 않으면 됩니다. 문서에 빠져 있던 밀린 임대료(`GetTotalArrears`)와 인원(`GetTotalStaffCount`) 2종 추가 |
| 2026-08-13 | 김시리 | 🆕 시설 철거 환급 기능 수용(한광훈 구현, Dev 병합) — 철거 시 설치비 50%를 `OnGoldChanged`로 돌려받습니다. `OnTotalRevenueChanged`는 안 쏩니다(환급 ≠ 매출). "Gold 상승 = 매출 상승"으로 가정하는 UI 코드가 있다면 확인 필요 |
| 2026-08-19 | 강민서 | `OnDayEnded` 델리게이트 신설. AdvanceToNextDay() 호출 시 방금 끝난 날짜와 그날 Live 진행 시간을 broadcast. 이월 데이터(정비/청결 등) 계산은 수신 측(광훈/시리) 담당 |