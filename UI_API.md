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


**요청 2건 (백종태 → 강민서, 2026-08-20) — UI 흐름 서브시스템 작업 중 발견**

1. 🔴 **축제를 Day 1 부터 시작하는 API 가 필요합니다.**

   `Tick()` 이 GameInstance 시작부터 돌고 `CurrentStage` 기본값이 `Setup` 이라,
   **타이틀 화면에 앉아 있는 동안 축제 시계가 이미 진행됩니다.** `DT_DaySequence` 에 Setup 시간이
   있으니 `RemainingTime` 이 0 이 되면 `RequestTransition(NextStage)` 로 넘어가고,
   플레이어가 지구본에서 축제를 고르기 전에 Day 2·3 까지 갈 수 있습니다.

   `RequestTransition(Setup)` 은 제가 부를 수 있는데 **`CurrentDay` 를 되돌릴 방법이 없습니다.**
   `BlueprintReadOnly` 이고 리셋 함수가 없습니다. UI 가 남의 상태를 직접 쓰지 않는 원칙이라
   지금은 `CurrentDay != 1` 이면 경고만 남기고 진행합니다.

   `StartNewFestival()` 같은 함수 하나로 `CurrentDay = 1` · `CurrentStage = Setup` ·
   `ElapsedLiveSeconds = 0` · `bFestivalEndedBroadcasted = false` 를 함께 초기화해 주시면
   제 쪽은 `OpenLevel` 직후 그 함수만 부르면 됩니다.

   함께 검토 부탁드리는 것 — **축제가 시작되기 전에는 Tick 이 돌지 않는 편이 맞지 않을까요?**
   프론트엔드에서 시계가 도는 것 자체가 의도된 동작이 아니라면, 시작 전에는 멈춰 있는 쪽이
   저장·불러오기와 반복 플레이에서도 예측하기 쉽습니다.

2. **`SunsetTransition`(노을 전환) 단계의 UI 요구사항을 알려주세요.**

   `EFF_FestivalStage` 는 코드에 **8종**인데 `docs/design/module-ownership.md` 3절은 7종으로 적고
   있고, 참조는 `FF_StageEconomyBridgeTests.cpp` 한 곳뿐입니다. UI 이식 계획에도 이 단계가
   빠져 있어서 화면 매핑에서 누락되기 쉬웠습니다.

   알고 싶은 것 세 개입니다 — **지속 시간이 얼마인가 / 연출 단계라 Skip 이 필요한가 /
   플레이어 입력을 받는 단계인가.**

   지금은 `연출 자리 + Skip` 스텁으로 두고 `to_verify` 로 표시해 두었습니다.

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

### 환경(날씨·야간) — `UFF_FestivalStageSubsystem` -강민서

- **헤더**: `Public/Time/FF_FestivalStageSubsystem.h`
- **접근**: `UFF_FestivalStageSubsystem` (GameInstanceSubsystem)
- **상태**: `구현완료`

| 멤버                                     | 형                            | 설명                                  |
| --------------------------------------- | ---------------------------- | ------------------------------------ |
| `EFF_WeatherType`                       | `enum`                       | `Clear` / `Rain` / `Windy` 3종        |
| `SelectedWeather`                       | `EFF_WeatherType`            | 현재 선택된 날씨                        |
| `SetSelectedWeather(EFF_WeatherType)`   |                               | 날씨 선택 요청 (Setup/MorningCheck에서만 허용) |
| `GetSelectedWeather()`                  | `EFF_WeatherType`            | 현재 선택된 날씨 즉시 조회                |
| `GetIsNightActive()`                    | `bool`                        | 지금 밤(야간 조명 활성) 상태인지 즉시 조회  |
| `OnEnvironmentStateChanged`             | `FOnEnvironmentStateChanged` | `(Weather, bIsNight)` — 환경 상태 변경 시 |

### `OnEnvironmentStateChanged`

- **헤더**: `Public/Time/FF_FestivalStageSubsystem.h`
- **접근**: `UFF_FestivalStageSubsystem` (GameInstanceSubsystem)
- **시그니처**: `FOnEnvironmentStateChanged(EFF_WeatherType Weather, bool bIsNight)`
- **갱신 주기**: 이벤트 — 다음 3개 시점에 발동
  1. `SetSelectedWeather()` 호출 시 (Setup/MorningCheck에서 선택 즉시, 미리보기 목적)
  2. Live 진입 시 (그날 확정된 날씨로)
  3. 밤 조명 트리거 시점 (야간 상태만 true로 바뀌어 재발동)
- **상태**: `구현완료`
- **비고**: `EFF_WeatherType`은 세기(Intensity) 개념 없음. 날씨 값의 만족도·이동 영향 해석은 시간 시스템 책임이 아니며, 수신 측(이동·시설·만족도 시스템)이 자체 판단. 구독 직후 현재값이 필요하면 `GetSelectedWeather()`/`GetIsNightActive()` 사용.

**구독 방법**
```cpp
GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>()
    ->OnEnvironmentStateChanged.AddDynamic(this, &YourClass::YourHandler);
```

**시각 연출 참고 (민서 자체 구현, UI 무관)**: `BP_WeatherManager`가 이 델리게이트를 구독해 Fog Density·VolumetricCloud·`NS_Rain` 파티클·빗소리 사운드를 날씨별로 조절함. 다른 시스템이 참고할 필요는 없음




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

### 시설 선택 · 월드 지점 조회 (2026-08-18 추가)

- **헤더**: `Plugins/FF_ZoneSystem/.../Public/FF_FFZSWorldSubsystem.h`,
  `FF_FFZSFacilityActor.h`, `FF_FFZSCollision.h`
- **접근**: `UFFZSWorldSubsystem` / 설치된 `AFFZSFacilityActor`
- **시그니처**: `AFFZSFacilityActor* FindFacilityAtWorldPoint(const FVector& WorldPoint, FName ZoneId = NAME_None) const`
- **상태**: `구현완료`

`FindFacilityAtWorldPoint()`는 `BlueprintPure`다. 현재 권위 레지스트리에 등록되어 있고 `WorldPoint`가
회전된 footprint의 XY와 시설 높이 범위(Z 허용 오차 포함)에 들어가는 시설만 반환한다. `ZoneId`가 비어
있으면 월드 지점에서 등록 Zone을 먼저 해석하며, Zone이나 시설을 찾지 못하면 `nullptr`를 반환한다.
footprint가 겹치면 Actor 원점까지 3차원 거리가 가장 가까운 시설을, 거리도 같으면
`FacilityInstanceId` 문자열 오름차순의 시설을 선택한다.

`AFFZSFacilityActor::SelectionProxy`는 `BlueprintReadOnly`인 선택 전용 Box다. 유효한 Spec을 적용한 뒤
시설 footprint의 XY 각 변에 기본 75cm를 더하고 장애물 높이에 맞춰지며, `QueryOnly`·`WorldDynamic`으로 프로젝트 Trace Channel
`FacilitySelection`만 Block한다. 물리·Navigation·Overlap에는 참여하지 않으며 `VisualMesh`는 계속
`NoCollision`이고 Navigation에도 영향을 주지 않는다. 유효한 Spec 적용 전 Proxy는 크기 0·`NoCollision`이라
선택되지 않는다. 프로젝트 채널의 기본 응답은 `Ignore`이므로 일반 `BlockAll` 컴포넌트는 전용 Trace를 막지 않는다.

`SetSelectionIndicatorState(Hidden/Hovered/Selected)`는 이 실제 클릭 Box의 렌더 표시만 바꾼다.
Hover는 Live UI Accent `#14F2E0` 3px, 선택된 정보 패널 대상은 같은 색 5px이며 Extent·Transform·Collision은
변경하지 않는다. `Hovered`는 **현재 클릭하면 선택될 대상**, `Selected`는 **현재 정보 패널 대상**을 뜻한다.
두 상태는 서로 다른 시설에 동시에 표시될 수 있다. 기본 상태는 `Hidden`이고 에디터 Actor 선택 여부와
무관하게 런타임에서 표시할 수 있다.

- C++ Trace는 채널 슬롯을 직접 쓰지 않고 `FFZSCollision::FacilitySelectionTraceChannel`을 사용한다.
- Blueprint Trace는 이름이 `FacilitySelection`인 프로젝트 채널을 사용한다.
- `FacilitySelection` Trace가 아무 것도 맞히지 않은 경우에만 호환용 지면 footprint fallback을 시도한다.
  첫 `Visibility` hit가 `ImpactNormal.Z >= 0.45`인 지면일 때만 그 지점을 조회 API에 전달한다.
- 앞쪽 전용 채널 후보가 비등록·stale 상태이거나 외부 `Visibility` 장애물에 가려지면 그 Actor를 제외하고
  최대 32개 후보까지 다음 등록 Proxy를 찾는다. 하나라도 전용 Proxy를 맞힌 뒤 유효 후보를 못 찾으면
  지면 fallback 없이 선택을 거부한다. `Visibility`는 시설 ID 선택 채널이 아니다.
- 가림 Trace 끝점은 Proxy 표면보다 2cm 앞에서 멈춰 Chaos 접촉 오차와 지형 경계가 정확한 클릭을
  거짓 차단하지 않게 한다. 그보다 앞의 실제 벽·오브젝트는 계속 클릭을 차단한다.

### 브리지 2종 (2026-08-06 추가)

| 클래스                                  | 역할                                                                             |
| ------------------------------------ | ------------------------------------------------------------------------------ |
| `UFF_ZoneSystemQueueBridgeSubsystem` | Zone 시설 `FGuid` ↔ Queue `int32` ID 매핑. `TryJoinZoneFacility`가 Party 가입의 단일 진입점 |
| `UFF_ZoneSystemStageBridgeSubsystem` | `UFF_FestivalStageSubsystem`을 구독해 Zone 변경 단계를 맞춤                               |

**위젯은 시설과 대기열을 직접 결합하지 않습니다.** 양쪽을 각각 읽고 위젯에서 합치면 소유권이 흐려집니다.
Overview 등 공통 지표는 `UFF_FestivalViewModelSubsystem`이, 설치 시설 정보 패널은
`UFF_FacilityInfoSubsystem`이 `FFF_FacilityInfoViewData`로 평탄화한 값을 제공합니다.

### 개발용 Zone·시설 QA 도구 (참고)

현재 수동 통합 QA는 `Content/FestaFiesta/Maps/Test_Main`에서 프로덕션
`UFF_BuildModeSubsystem`과 개발용 `UFF_ZoneLifecycleTestPanelSubsystem`을 함께 사용합니다.
비 Shipping의 `Test_Main`에서는 `WBP_FF_ZoneLifecycleTestPanel`이 자동으로 표시되어 Festival 단계와
시설 Open·Close·Emergency 권위 API를 관찰·실행하고, 설치·이동·철거는 실제 건축 모드 경로로 확인합니다.

시설 Definition과 배치용 에디터 픽스처는
`Content/FestaFiesta/Zone/Maps/FacilityDefinitionTestMap`입니다. 이 맵은 패키징 대상이 아닙니다.
**Lifecycle 패널과 검증 맵은 플레이어용 UI가 아니라 개발 도구입니다.** Morning Check UI를 만들 때
동일한 시설 명령 진입점과 결과 처리 방식을 참고합니다.

### 완성형 시설 건축 카탈로그 (2026-08-25 갱신)

- **헤더**: `Public/Building/FF_BuildCatalogSubsystem.h`, `Public/Building/FF_BuildModeSubsystem.h`
- **접근**: `GameInstanceSubsystem` 카탈로그 + `TickableWorldSubsystem` 건축 모드
- **상태**: `구현완료` (스킨 적용 데이터는 계획)

`UFF_BuildCatalogSubsystem`은 Asset Manager의 `FFZSFacilityDefinition` Primary Asset을 비동기 로드하고
`대분류 > 소분류 > 고유이름` 읽기 모델을 만든다. 위젯은 에셋 레지스트리를 직접 조회하거나 ID를 직접
파싱하지 않고 `GetEntries()`/`GetCategoryGroups()`와 `OnCatalogChanged`를 사용한다.

`UFF_BuildModeSubsystem`은 `Closed → BuildMode ↔ CatalogOpen → Placing`과
`BuildMode → Moving → BuildMode` 상태를 소유한다. 건축 세션은
`UFF_FestivalStageSubsystem::IsReconfigureAllowed()`와 FFZS `ChangePhase`
(`InitialSetup`/`MorningCheck`)가 모두 허용할 때만 진입할 수 있다. 활성 세션 중 어느 한 정책이
닫히면 카탈로그·프리뷰·시설 선택·입력 상태를 정리하고 `Closed`로 자동 전환한다.

신규 설치는 선택한 `UFFZSFacilityDefinition::Spec`을, 이동은 기존 액터의 권위 Spec·GUID·현재 회전을
`AFFZSPlacementPreviewActor`와 `ExecuteCommand()` 경로에 전달한다. 따라서 Zone 경계·충돌·여유 공간·
Queue 예약·보호구역·지면·내비게이션·운영 단계 검증을 우회하지 않는다.

배치 회전의 권위 표현은 `FFFZSGridPlacement::YawCentidegrees`(0.01도 단위)다. 기존
`RotatePlacement(int32 QuarterTurnDelta)`는 Blueprint 호환을 위해 90도 증감 래퍼로 유지하고, 새 UI는
`RotatePlacementDegrees(float)`·`SetPlacementYawDegrees(float)`·`GetPlacementYawDegrees()`를 사용한다.
`Q/E` 스냅 간격, `Shift+Q/E` 자유 회전 속도, 위치 스냅 간격은 각각
`UFFZSDeveloperSettings::PlacementRotationSnapDegrees`,
`FreePlacementRotationSpeedDegreesPerSecond`, `PlacementPositionSnapCm`에서 읽는다. 유효하지 않은 config의
안전값은 45도, 초당 90도, 1cm다. 자유 회전은 매 프레임 최신 yaw만 기록하고 전체 배치 검증은 기존
0.08초 주기를 유지하며, 확정 직전에는 최신 커서·각도·revision을 강제 재검증한다.

활성 건축 세션에서는 설치 시설의 footprint(초록)·clearance(청록)·queue 예약 범위(노랑)를 표시한다.
카탈로그가 닫힌 `BuildMode`에서 시설을 좌클릭하면 청록색으로 강조되고, 공용 우측 시설 정보 패널이
`Build` 컨텍스트로 열린다. 패널은 시설 정보와 `이동`·`철거` 액션 및 닫기를 제공하며, 패널 위 좌클릭은
월드 시설 선택 입력으로 전달되지 않는다. 패널 버튼에서 발생한 이동·철거·닫기 요청은 패널의 시설 GUID가
현재 건축 선택과 일치할 때만 처리한다. 이동을 시작하면 권위 액터는 기존 위치에 그대로 두고 ghost만 커서를
따라간다. 따라서 우클릭 취소나 검증 실패 때 원위치 복구 명령이 필요 없다. 이동 성공 시 같은 시설 GUID를
유지한 채 Zone·배치·회전만 원자적으로 갱신한다. 대상이 외부 명령이나 스냅샷 복원으로 바뀌면 열린 작업과
이동 프리뷰는 자동 취소된다.

카탈로그를 열거나 이동을 시작하거나 닫기 요청·철거 성공·건축 모드 종료가 발생하면 `Build` 선택과 패널을
함께 닫는다. 재시도 가능한 이동·철거 실패는 선택을 유지하고 같은 패널의 `FeedbackText`와
`bFeedbackIsError`로 사유를 표시한다.

| 입력 | 동작 |
| --- | --- |
| `B` | 재배치 허용 단계에서 첫 입력은 `BuildMode` 진입, 이후 카탈로그 열기/닫기. 불허 단계에서는 진입하지 않음 |
| `M` 또는 `이동` 버튼 | 선택 시설의 이동 프리뷰 시작 |
| `Q`, `E` 탭 | 설치/이동 프리뷰를 설정된 스냅 간격만큼 진행 방향의 다음 각도로 회전. 기본 45도 |
| `Shift` + `Q`, `E` 홀드 | 설치/이동 프리뷰를 설정 속도로 연속 자유 회전. 첫 edge의 스냅 회전과 중복 실행하지 않음 |
| `좌클릭` | `BuildMode`에서는 시설 선택, `Placing`에서는 설치, `Moving`에서는 이동 확정 |
| `Enter` | 공용 시설 정보 패널의 건축 선택에서는 철거, `Placing`/`Moving`에서는 현재 프리뷰 확정 |
| `우클릭` | 시설 선택 취소, 설치는 카탈로그 복귀, 이동은 원본을 건드리지 않고 `BuildMode` 복귀 |
| `Esc` | 어느 활성 건축 상태에서든 건축 모드 전체 종료 |

카탈로그가 CommonUI 키보드 포커스를 가진 동안에는 화면의 preview-key 어댑터가 `B`/`Esc` 의도를
서브시스템에 전달한다. UI와 PlayerController가 같은 `B`를 모두 관찰한 프레임에는 키 해제 래치가 raw
입력을 한 번 차단하므로 `CatalogOpen → BuildMode → CatalogOpen` 이중 전환이 발생하지 않는다.

- 건축 세션 진입은 Festival Stage의 `IsReconfigureAllowed()`와 FFZS Change Phase의
  `InitialSetup`/`MorningCheck` 조건을 모두 만족해야 한다.
- 활성 세션 중 둘 중 하나라도 재배치를 금지하면 건축 모드가 자동 종료된다.
- `Install`·`Move`·`Remove` 명령도 FFZS에서 `InitialSetup`/`MorningCheck` 정책을 다시 검증한다.
- `Active` 시설 또는 FFZS/프로젝트 Queue에 이용객이 남은 시설은 이동·철거할 수 없다. 자동 폐장이나
  강제 배출은 하지 않으며, 운영 종료 후 Queue가 비워진 것을 확인해야 한다.
- 같은 위치·같은 방향 이동은 revision과 상태를 불필요하게 바꾸지 않도록 UI에서 차단한다.
- 이동 성공 뒤 시설 상태는 `Validated`가 되므로 운영을 재개하려면 기존 개장 절차를 다시 수행한다.
- 상단 UI는 `건축`/`스킨` 2탭이지만, 현재 `스킨` 탭은 데이터 모델이 없어 준비중 상태다.
- 현재 등록된 실제 시설 정의가 1개뿐이므로, 나머지 카드는 규칙에 맞는 Data Asset을 추가할 때 자동 생성된다.

### 설치 시설 공용 정보 패널 (2026-08-18 추가)

- **헤더**: `Public/UI/Facility/FF_FacilityInfoTypes.h`, `FF_FacilityInfoSubsystem.h`,
  `FF_FacilityInfoPanel.h`
- **접근**: `UFF_FacilityInfoSubsystem` (`TickableWorldSubsystem`)
- **기본 WBP**: `/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel.WBP_FF_FacilityInfoPanel_C`
- **갱신 주기**: 선택 즉시, 선택 시설의 non-Remove World Change 다음 Tick, 선택 중 0.25초 간격.
  Remove·World Snapshot 복원은 즉시 선택 해제
- **상태**: `구현완료`

건축 모드와 Live가 같은 비모달 우측 패널을 사용한다. 서브시스템이 Zone·Queue·Economy·Stage·Catalog를
읽어 불변 표시 스냅샷 `FFF_FacilityInfoViewData`로 평탄화하며, WBP는 게임플레이 서브시스템을 직접
조회하거나 값을 다시 계산하지 않는다. 0.25초 샘플링은 이 coordinator 내부에만 한정되며 위젯은 폴링하지 않는다.

**`EFF_FacilityInfoContext`**

| 값 | 의미 |
| --- | --- |
| `Build` | 건축 선택. 이동·철거 액션을 노출할 수 있음 |
| `Live` | Live 운영 정보. `CurrentStage == Live`일 때만 선택 가능 |
| `None` | 선택 없음. Blueprint enum 목록에서는 숨김 |

**`UFF_FacilityInfoSubsystem` Blueprint API**

| 시그니처 | 계약 |
| --- | --- |
| `bool PrimePanel(APlayerController* OwningController)` | LocalPlayer용 숨김 WBP를 미리 로드·생성. PlayerController 자동 바인딩 때 호출되며 필요하면 명시적으로 먼저 호출할 수 있음. 실패하면 `false` |
| `bool SelectFacility(APlayerController* OwningController, AFFZSFacilityActor* Facility, EFF_FacilityInfoContext Context)` | 현재 Zone 레지스트리에 등록된 유효 시설만 선택하고 패널을 엶 |
| `bool SelectFacilityAtWorldPoint(APlayerController* OwningController, const FVector& WorldPoint, EFF_FacilityInfoContext Context, FName ZoneId = NAME_None)` | `FindFacilityAtWorldPoint()`로 찾은 뒤 선택 |
| `void ClearSelection()` | 선택·피드백·썸네일 세션을 비우고 패널을 접음 |
| `AFFZSFacilityActor* GetSelectedFacility() const` | 현재도 등록된 선택 Actor. 없거나 무효면 `nullptr` |
| `FGuid GetSelectedFacilityId() const` | 현재 선택 GUID |
| `EFF_FacilityInfoContext GetSelectionContext() const` | 현재 `Build`/`Live`/`None` 컨텍스트 |
| `bool HasSelection() const` | 유효한 선택 GUID 보유 여부 |
| `bool IsPointerOverPanel() const` | 보이는 카드 위에 포인터가 있는지. 월드 좌클릭 관통 방지용 |
| `OnSelectionChanged(FGuid FacilityInstanceId, EFF_FacilityInfoContext Context)` | 실제 ID/Context 변경 또는 선택 해제 때만 발행하는 `BlueprintAssignable` 이벤트 |

`OnSelectionChanged`를 구독한 직후에는 `HasSelection()`·`GetSelectedFacilityId()`·
`GetSelectionContext()`로 현재값을 한 번 읽는다. 같은 시설과 같은 Context를 다시 선택하면 화면은 새로 고치지만
이벤트를 중복 발행하지 않으며 기존 피드백을 비우고 0.25초 타이머를 다시 시작한다. 선택 해제 이벤트는
무효 GUID와 `None`을 전달한다. 선택 시설 철거,
World Snapshot 복원, Context 무효화 때도 선택이 자동 해제된다.

`Live` 선택은 `AFF_FestivalPlayerController::OnActorPicked`를 받은 뒤 전용 `FacilitySelection` Trace로
해석한다. 전용 Trace가 아무 것도 맞히지 않고 첫 `Visibility` hit가 걸을 수 있는 지면일 때만 footprint
fallback을 시도한다. 앞쪽 비등록·stale·가림 후보는 건너뛰되, 전용 후보를 맞히고도 유효 시설이 없으면
fallback 없이 거부한다. 빈 곳을 클릭하면 기존 Live 선택을 닫는다.

기본 좌클릭은 `IA_Pick`의 `Started`를 사용한다. CommonUI 또는 런타임 Mapping Context 때문에 그 Action이
같은 PlayerTick에 실행되지 않은 경우 PlayerController가 raw `LeftMouseButton` edge를 한 번만 보조 처리한다.
따라서 `OnActorPicked`는 누락되지 않으며 Action과 fallback이 함께 들어와도 중복 방송하지 않는다.

Live에서는 같은 resolver를 0.05초 간격으로 재사용해 커서 아래 실제 클릭 Box를 Hover 표시한다. 클릭 순간에는
현재 Hovered Proxy가 새 ray와 실제로 교차하고 등록·채널·가림 검증을 통과하는지 동기 재확인해 그 시설을
우선 선택한다. stale Hover가 ray를 벗어났으면 실제 교차 후보로 즉시 표시와 결과를 바꾼다. 별도 Actor Tick이나
시설 전체 순회는 없으며, 커서가 패널 위에 있거나 건축·모달·ActorView·CharacterView에서 피킹이 막히면
Hover를 즉시 정리한다. 패널이 열린 시설은 `Selected` 표시가 우선하며, Live 이탈·시설 제거·World Snapshot
복원·Subsystem 종료 때 Hover와 Selected 표시를 모두 해제한다.

제품 건축 경로의 `Build` 선택은 `UFF_BuildModeSubsystem`이 소유한다. 활성 건축 세션은 Festival Stage와
FFZS Phase의 교집합 게이트를 유지하며, 이동·철거 액션 경계에서는 시설 GUID·권위 레지스트리·FFZS Phase·
시설 상태와 점유 여부를 다시 검증한다. Stage가 재배치를 금지하면 `OnStageChanged` 또는 다음 Tick에서
세션 자체를 종료한다. 단, 공개 `SelectFacility(..., Build)` 함수 자체는 Stage/Phase 진입을 막지 않으므로
제품 건축 흐름을 우회하는 진입점으로 사용하지 않는다.

**`FFF_FacilityInfoViewData` (`BlueprintType`, 전 필드 `BlueprintReadOnly`)**

| 묶음 | 필드 |
| --- | --- |
| 식별·Context | `FacilityInstanceId`, `Context`, `ContextTitle` |
| 시설 표시 | `DisplayName`, `CategoryText`, `ZoneText`, `FacilityStateText`, `FootprintText`, `ClearanceText` |
| 가격·정적 사양 | `InstallCostText`, `RecoveryCostText`, `QueueCapacityText`, `ServiceDurationText`, `Thumbnail`, `ThumbnailPlaceholderText` |
| 운영·Queue | `OperationStateText`, `WaitingPartiesText`, `OccupancyText`, `RemainingServiceText`, `ExpectedWaitText`, `ThroughputText` |
| 액션·피드백 | `bNeedsRelocation`, `bCanMove`, `bCanRemove`, `FeedbackText`, `bFeedbackIsError` |

문자열 필드는 모두 표시용 `FText`다. 포맷터가 만든 수치에는 단위와 지역화된 숫자 포맷이 적용되지만,
`ZoneText`·`DisplayName`·`FeedbackText`의 현지화 여부는 원본 데이터에 따른다. 예상 대기는 1인 진입
기준이며 0초는 `바로 이용 가능`, 60초 미만은 `1분 미만`, 그 이상은 분 단위 올림, 도달 불가·비정상 값은
`계산 불가`로 표시한다. 처리율은 `분당 x.x명`이다. Queue 미등록은 `준비 중`, Queue 시스템 부재는
`사용 불가`로 구분한다.

`bCanMove`와 `bCanRemove`는 같은 판정을 쓴다: `Build` Context이고, FFZS Phase가
`InitialSetup`/`MorningCheck`이며, Queue가 연결되지 않았거나 `OccupiedMemberCount <= 0`이고,
Actor의 `CanMoveOrRemove()`가 참이어야 한다. 버튼 상태는 안내용이며 실제 명령 경계가 다시 검증한다.

**`UFF_FacilityInfoPanel` WBP 계약**

- Blueprint 함수는 `ApplyViewData(const FFF_FacilityInfoViewData&)`, `SetPanelOpen(bool)`,
  `IsPointerOverInteractivePanel()` 3종이다. 일반 호출자는 패널을 직접 만들지 않고 서브시스템을 사용한다.
- 루트는 `CanvasPanel` + `SelfHitTestInvisible`, 실제 카드는 우측 중앙 + AutoSize다. 카드 밖 클릭은 월드로
  통과하고 카드 좌클릭과 버튼 입력은 소비한다. 우클릭은 건축 선택 취소로 전달될 수 있다.
- 다음 27개 `BindWidget` 이름을 유지한다.

```text
Border_Panel, Button_Close, Button_Move, Button_Remove, Image_Thumbnail,
Text_ThumbnailPlaceholder, Panel_BuildSection, Panel_LiveSection, Text_Context,
Text_FacilityName, Text_Category, Text_Zone, Text_FacilityState, Text_RelocationWarning,
Text_Footprint, Text_Clearance, Text_InstallCost, Text_RecoveryCost, Text_QueueCapacity,
Text_ServiceDuration, Text_OperationState, Text_WaitingParties, Text_Occupancy,
Text_RemainingService, Text_ExpectedWait, Text_Throughput, Text_Feedback
```

에디터 명령 `FF.BuildFacilityInfoUMG`는 에셋이 없을 때만 기본 WBP를 생성한다. 기존 디자이너 에셋은
덮어쓰지 않고 위 상속·배치·BindWidget 계약만 검증한다.

**C++ 전용 어댑터 계약**

- `UFF_FacilityInfoPanel`의 `FSimpleMulticastDelegate OnMoveRequested`·`OnRemoveRequested`·
  `OnCloseRequested`는 인자가 없다. 이동·철거는 현재 ViewData가 `Build`이고 해당 `bCan*`가 참일 때만,
  닫기는 항상 방송한다.
- `UFF_FacilityInfoSubsystem`의 `FFF_OnFacilityInfoActionRequested(FGuid)` 3종은 패널 이벤트를 선택 GUID로
  변환한다. 이동·철거·닫기 모두 `Build` 선택에서만 외부로 방송하며, 닫기는 방송 후 같은 선택을 해제한다.
- `SetFeedback(const FText&, bool)`은 선택이 없으면 no-op이고, `ClearSelectionIfMatches(FGuid, Context)`는
  호출자 소유 선택만 닫는다. `static FSoftClassPath GetDefaultPanelClassPath()`,
  `const FFF_FacilityInfoViewData& GetCurrentViewData() const`, `FFFacilityInfoModel` 포맷 함수도 C++ 전용이다.

위 항목은 BlueprintAssignable API로 간주하지 않는다. Soft Class로 로드하는 WBP가 Shipping에서도
포함되도록 `/Game/FestaFiesta/UI/Facility`은 `DirectoriesToAlwaysCook`에 등록되어 있다.

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

### `UFF_NpcNeedMetricsSubsystem` — Need 집계

- **헤더**: `Public/NPC/Metrics/FF_NpcNeedMetricsSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **갱신 주기**: 이벤트 (집계 주기마다, 초안 30틱). 값이 바뀐 Need만 발행
- **상태**: 🔴 **읽는 쪽은 완성, 채우는 쪽이 아직 없어 값이 전부 0입니다.** 제 다음 작업 대상입니다
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
⚠️ **여섯 항 중 넷만 값이 움직입니다.** 나머지 둘이 0으로 나오는 것은 화면 버그가 아닙니다.

| 항 | 상태 |
|---|---|
| `PreferenceTerm` · `NeedTerm` · `DistanceCostTerm` · `WaitCostTerm` | ✅ 값이 들어옵니다 |
| `AttractionTerm` | ❌ 항상 0 — 시설 기본 매력도의 정규화 설계가 남아 있습니다(제 작업) |
| `DensityCostTerm` | ❌ 항상 0 — Zone 혼잡도를 보고하는 코드가 없습니다(이정원 님 대기) |

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

### 유료 프로그램 예약 — 요청 4건 중 3건 완료

- **헤더**: `Public/NPC/Party/FF_ProgramReservationSubsystem.h`
- **접근**: `GameInstanceSubsystem`

| 요청 | 상태 | API |
|---|---|---|
| ① 프로그램 목록 | ✅ | `GetReservationIds()` + `FindReservation(Id, Out)`. **시작 시각 오름차순 정렬** — 순서가 실행마다 바뀌지 않습니다 |
| ② 예약 상태 | ✅ | `FFF_ProgramReservation` 의 `Capacity`·`AssignedGroupIds`·`AssignedHeadcount`·`UnfilledSeats`. 남은 자리는 `GetRemainingSeats(Id)` |
| ③ **구매·취소 Command** | 🔴 **다음 작업** | Gold 를 쓰는 일이라 경제 S8 과 같이 붙입니다. 실패 이유는 피날레와 같은 방식으로 **열거형**을 드립니다 |
| ④ 변경 델리게이트 | ✅ | `OnReservationChanged(FGuid ReservationId)` — 바뀐 예약의 핸들이 옵니다 |

- 시간 필드(`StartMinutes`·`EndMinutes`)는 **Live 경과분**입니다. 화면에 시각으로 표시하시려면
  변환 지점을 한 곳으로 두시는 게 좋습니다.
- `UnfilledSeats > 0` 은 **정원을 다 못 채운 것**이고 오류가 아닙니다 — 정원을 그룹 크기 조합으로
  정확히 채우는 규칙이라(4명 정원에 3인 그룹만 남으면 1자리) 남을 수 있습니다. "남은 자리"로
  그대로 보여주시면 됩니다.
- ⚠️ **구매 화면이 없어서 실제 예약이 0건입니다.** 목록이 비어 보이는 것이 지금은 정상이고,
  ③이 붙어야 찹니다.

### 경제 값 5종 — ViewModel 연결 요청

- **헤더**: `Public/Festival/FF_FestivalProgressionSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **상태**: 원본 API 전부 `구현완료`. **ViewModel 한 곳만 붙이면 화면에 뜹니다**

| 값 | Getter | 델리게이트 | 단위·범위 | 연결 |
|---|---|---|---|---|
| 잔액 | `GetGold()` | `OnGoldChanged` | Gold, 0 이상·상한 없음 | ✅ 연결됨 |
| 누적 매출 | `GetTotalRevenue()` | `OnTotalRevenueChanged` | Gold, **줄어들지 않음** | ⬜ |
| 오늘 매출 | `GetTodayRevenue()` | `OnTodayRevenueChanged` | Gold, 매일 아침 0 | ⬜ |
| **밀린 임대료** | `GetTotalArrears()` | `OnTotalArrearsChanged` | Gold, 0 이상 | 🔴 **UI 참조 0곳** — 상단 바 자금난 경고가 이것에 달려 있습니다 |
| **오늘 일하는 인원** | `GetTotalStaffCount()` | `OnTotalStaffCountChanged` | 명, 6~12 | 🔴 **UI 참조 0곳** — Event 배치·아침 고용 화면 |

(2026-08-23 코드 확인 기준입니다. 이미 붙이셨는데 표가 틀렸으면 알려주세요.)

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

### 🔴 화면 만들 때 알아야 하는 것 5가지

**1. "다음 날" 버튼이 이제 돈을 걷습니다. 종태님이 고치실 것은 없습니다.** 이미 만들어 두신
`RequestAdvanceToNextDay()`가 하루치 임대료 2,400을 걷고, 오늘 매출을 0으로 되돌리고, 어제 쓴 알바를 퇴근시킵니다.
값 4개(잔액·밀린 임대료·오늘 매출·인원)가 한꺼번에 바뀌고 전부 델리게이트가 옵니다.

**2. 잔액을 UI가 직접 계산하지 말아 주세요.** 버튼 직후에 `잔액 − 2,400`을 계산하면 어긋납니다 — 잔액이 부족하면
실제로 빠지는 금액은 2,400이 아니라 "있는 만큼"입니다. 델리게이트가 주는 값을 그리시면 항상 맞습니다.

**3. 🔴 Day1 셋업 예산은 7,600입니다. 프로토타입을 10,000으로 만들어 두셨다면 바꿔 주셔야 합니다.**

> 2026-08-19에 *"1일차만 아침 단계를 건너뛰는 것이 맞다면 알려 주세요"* 라고 물어봐 주셨습니다.
> **네, 그게 맞습니다.** Day1만 개장 준비(MorningCheck)를 건너뜁니다.

`DT_DaySequence` 에 **`Day1_MorningCheck` 행이 없습니다.** Day1은 준비의 날이라 아침 점검이 없고,
팀 의도로 확정됐습니다. 그래서 Day1 임대료 2,400은 **축제 시작과 함께** 걷힙니다 —
Day2·3처럼 아침에 걷을 자리가 없기 때문입니다. 표에 있는 각 날의 흐름입니다:

| Day | 단계 흐름 |
|---|---|
| **Day1** | `Setup`(420초) → `PromotionPreview`(60초) → `SunsetTransition`(20초) → `Live` → `Closing` → `Report` |
| Day2 | `MorningCheck`(120초) → `SunsetTransition` → `Live` → `Closing` → `Report` |
| Day3 | `MorningCheck` → `SunsetTransition` → `Live` → `Finale` → `Closing` → `Report` |

흐름에 `MorningCheck` 대신 `SunsetTransition`(20초)이 `Live` 앞에 있는 것도 표 기준이니
프로토타입 단계 순서도 같이 확인해 주세요.

🔴 **그래서 플레이어가 셋업에서 시설을 고를 때 보는 예산은 7,600입니다.** 10,000이 아닙니다.
프로토타입이 10,000 기준이면 **셋업·홍보 구간 표시를 7,600으로 바꿔 주셔야 합니다.**

숫자를 옮기는 쪽(축제 시작 → 개장 준비)도 검토했는데, Day1에 개장 준비가 없으니 옮기면
**Day1 임대료가 아예 안 걷힙니다**(3일 합계 7,200 → 4,800, 피날레 예산이 무너집니다).
그래서 징수 시점은 그대로 두는 게 맞습니다.

**Day1에 개장 준비를 새로 추가하는 방법도 있는데, 현재 경제 입장에서는 필요하지 않습니다.**

⚠️ **나중에 아침에만 일어나는 일**(날씨 공개 · Event 브리핑 · 알바 고용)이 늘어나서
Day1에도 그게 필요해지면 **추가하실 때 저에게 알려 주세요** — `Day1_MorningCheck` 행만
생기면 경제가 저절로 따라가지는 않습니다.

다만 지적해 주신 문제의식은 그대로 유효합니다 — 화면에 이유가 없으면 7,600은 그냥
"왜 10,000이 아니지"로 읽힙니다. 임대료는 어차피 매일 나가는 돈이니 **감추지 말고 보여주는
쪽**이 맞다고 봅니다.

**제안 — 두 개를 같이 해주시면 이 건이 닫힙니다.** 아래 A·B 입니다. 둘 다 **경제 쪽 추가
작업이 없습니다**(필요한 값이 이미 다 나가 있습니다). C·D 는 하실 필요 없고, 원하실 때
말씀만 주시면 제가 준비합니다.

⚠️ **A·B 는 델리게이트가 아니라 원장 조회로 만들어 주세요** — `HasLedgerForDay(Day)` +
`GetSpendRowsForDay(Day)`. Day1 임대료는 `StartFestival` 안에서 걷혀 그 시점엔 ViewModel이
구독 전이라(`FF_FestivalViewModelSubsystem.cpp:225` 주석의 그 상황) **`OnGoldChanged` 는
Day1에 안 옵니다.** 연출을 붙이고 싶으시면 D 를 봐주세요.

### A · 셋업 예산 내역 — 이것만으로 이 건이 닫힙니다

셋업 화면의 예산 표시를 숫자 하나에서 세 줄로:

```
초기 자금        10,000
장소 임대료      −2,400
────────────────────────
사용 가능         7,600
```

세 값이 이미 다 나가 있습니다 — **제가 추가할 것이 없습니다.**

| 줄 | 어디서 |
|---|---|
| 초기 자금 10,000 | `GetDefinition()->InitialGold` |
| 장소 임대료 −2,400 | `GetSpendRowsForDay(1)` 의 임대료 줄(`DisplayName` 한글 포함). 셋업 중에도 `HasLedgerForDay(1) == true` 라 조회됩니다 |
| 사용 가능 7,600 | `GetGold()` (이미 연결돼 있습니다) |

시설을 고르는 화면에 상시 붙어 있어서 토스트처럼 지나가지 않고, 3일 내내 같은 자리에서
같은 걸 보여줄 수 있습니다.

⚠️ **문구는 잠정으로 두세요.** Day1 임대료가 "오늘의 임대료"인지 **"첫날 선납·계약금"** 인지
기획 확인 중입니다(E-27). 후자면 Day1만 `장소 임대료(첫날 선납)` 로 바뀝니다.

### B · 상단바 고정비·미납 — 같이 해주시면 더 큰 게 같이 풀립니다

`오늘 고정비 2,400 · 미납 0` 형태. 자금난이 오면 같은 자리가 경고로 바뀝니다.
새 요청이 아니라 아래 "경제 값 5종" 표에서 **UI 참조가 0곳**인 두 값(밀린 임대료 · 오늘 일하는
인원)을 붙이는 것과 같은 작업입니다. 아침 고용 화면이 어차피 이 자리를 쓰니 같이 하시면 됩니다.

🔴 **이게 A보다 급하다고 봅니다.** 미납은 이미 동작하는 기능인데 **화면에 나오는 곳이
0곳**입니다. 지금은 잔액이 0이 되고 빚이 쌓여도 플레이어가 알 방법이 없고, 매출이 들어와도
빚을 먼저 갚아서 **"매출은 오르는데 잔액이 안 늘어나는" 구간**이 설명 없이 생깁니다
(위 5번 항목). A는 숫자 하나를 설명하는 일이고, B는 **기능 하나를 화면에 처음 올리는 일**입니다.

### C · 임대 계약 카드 (선택 — 여력 되실 때)

셋업에 처음 들어올 때 한 번만 뜨는 카드입니다.

```
        축제장 임대 계약
      다다포차 광장 · 3일

  일 2,400 G × 3일 = 7,200 G
  첫날분 2,400 G 납부 완료

        잔액 7,600 G
```

A가 "지금 왜 7,600인가"를 답한다면, 이건 **"앞으로도 매일 2,400이 나간다"를 첫날에
가르칩니다.** Day2 아침에 또 줄었을 때 놀라지 않고, 시설을 몇 개까지 지을지 판단할 때
남은 이틀치 4,800을 감안하게 됩니다.

- 값은 A와 같은 곳에서 나옵니다 + 3일치는 `GetDefinition()->DailyVenueRentGold × 3`
- **매일 띄우지 마세요.** 첫날 1회면 충분하고, 반복되면 3일차엔 성가십니다
  (매일 필요한 정보는 B가 상단바에서 상시로 답합니다)
- "돈이 빠졌다"가 아니라 "계약을 했다"로 읽히는 쪽이 낫다고 봅니다 — 손실이 아니라
  축제를 여는 첫 단계로 프레이밍됩니다

### D · 연출을 붙이고 싶으시면 — 말씀만 주시면 제가 만듭니다

A~C 는 정적 표시입니다. `10,000 → 7,600` 카운트다운이나 코인 이펙트 같은 **연출**을 원하시면
Day1은 `OnGoldChanged` 로 못 받으니, "아직 안 보여준 정산"을 경제가 들고 있게 하면 됩니다:

```
bool HasPendingSettlementReveal() const;
bool GetPendingSettlementReveal(FFF_DaySettlement& Out) const;   // 날짜·임대료·차감 전후 잔액
void ConsumeSettlementReveal();
```

화면이 뜨는 **아무 시점에** 물어보시고, 있으면 연출을 처음부터 재생하시면 됩니다. 연출이 실제
차감 프레임에 붙어 있을 필요가 없고, 세 날 모두 같은 방식으로 동작합니다.
`ConsumeSettlementReveal()` 을 언제 부를지는(연출 시작 / 끝) UI 쪽 판단입니다.

- 원장만으로는 **"이걸 아직 안 보여줬다"를 알 수 없습니다.** UI가 그 상태를 직접 들면
  화면 재진입·저장 불러오기에서 두 번 뜨거나 안 뜹니다. 그래서 경제 쪽에 두는 게 안전합니다
- 쓰실 계획이 생기시면 그때 만들어 드리겠습니다.

### 안 하는 쪽으로 정한 것 — 징수를 늦춰 셋업을 10,000으로 만들기

"홍보 진입에서 걷으면 셋업이 10,000이 된다"는 방법이 있습니다. 표를 안 건드리고 제 쪽에서만
가능하고 실시간 연출도 됩니다. **그런데 안 하는 게 맞다고 봅니다.**

- 플레이어가 **10,000 기준으로 예산을 세운 뒤 2,400을 빼앗깁니다.** 그러면 "사업 비용"이 아니라
  **"페널티"** 로 읽힙니다. 처음부터 7,600으로 시작하면서 내역을 보여주는 쪽이 낫습니다
- 셋업에서 10,000을 다 쓰면 홍보 진입에서 **즉시 미납**입니다. 그런데 미납은 지금 화면에
  0곳이라(B 참고) **플레이어가 조용히 파산합니다.** B 없이는 버그처럼 보입니다
- 3일 수지 검산이 셋업 가용액 7,600을 전제로 돼 있고, 임대료 2,400 자체가 아직 팀 확인
  전입니다(E-23). 확정 안 된 숫자 위에 밸런스 변경을 더 쌓을 시점이 아닙니다

### 나중에 다시 볼 것 — Day1 전용 "임대 계약" 스테이지

Day1에 계약 전용 단계를 만들면 연출 자유도가 가장 크고, 경제 쪽 정산 경로도 하나로 통일됩니다
(지금은 Day1이 `StartFestival`, Day2·3이 브리지로 경로가 둘입니다).

**다만 지금은 권하지 않습니다.** 연출은 D로 되니 단계 추가의 고유 이점이 아니고, 단계 enum·
기본 단계·`DT_DaySequence`를 같이 건드려야 하고(강민서 님 파일), 단계 표시명 버그 대상이
3곳에서 4곳으로 늘어납니다. 중간 발표 뒤에, 그리고 **Day1 임대료가 "계약금" 성격으로
정해지면**(E-27) 그때 다시 꺼내는 게 맞다고 봅니다.

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

### 🔴 피날레 구매 화면이 필요합니다 (담당 백종태)

Day3 아침(MorningCheck)에 피날레 등급을 사는 화면입니다. 경제 쪽은 다 됐습니다.

- **버튼 활성화**: `UFF_StageEconomyBridgeSubsystem::CanPurchaseFinaleNow()` — Day3 MorningCheck이고
  아직 안 샀을 때만 `true`. **표시용이고 구매를 막지는 않습니다**(`CanSkipCurrentStage`와 같은 성격)
- **목록**: 등급·비용·보너스는 `GetDefinition()->FinaleTiers`에 있습니다.
  C/B/A/S = 2,000/4,000/6,000/8,000 Gold, 만족도 +5/+12/+20/+30. UI가 쓰기 편한 Getter가 필요하시면 만들겠습니다
- **구매**: `TryPurchaseFinale(EFF_FinaleGrade)` → `EFF_FinalePurchaseResult`.
  실패 이유 한글은 **`UFF_EconomyDisplayTextLibrary::GetFinalePurchaseResultText(Result)`** 로
  뽑아 주세요 (`Gold 부족` / `이미 구매했다` 등). ⚠️ enum 의 `DisplayName` 을 직접 읽으면
  패키징 빌드에서 영문이 뜹니다 — 아래 절 참고
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

**시설 철거 환급은 매출이 아닙니다.** 설치비 50%가 돌아오면서 `OnGoldChanged`는 오지만
`OnTotalRevenueChanged`는 오지 않습니다. "잔액 상승 = 매출 상승"으로 가정한 코드가 있으면 확인해 주세요.

아직 없는 경제 값은 **일차 결과(등급 산출)와 저장 API**입니다.

### 결산 표 4종 — 구현완료

- **헤더**: `Public/Festival/FF_FestivalProgressionSubsystem.h`
- **접근**: `GameInstanceSubsystem`. 전부 `BlueprintPure`

```cpp
bool                            HasLedgerForDay(int32 Day) const;
TArray<FFF_LedgerRow>           GetEarnRowsForDay(int32 Day) const;
TArray<FFF_LedgerRow>           GetSpendRowsForDay(int32 Day) const;
TArray<FFF_FacilityRevenueRow>  GetRevenueByFacilityRowsForDay(int32 Day) const;
int32                           GetTotalRefundForDay(int32 Day) const;
```

**요청하신 형태에서 날짜 인자 하나가 붙었습니다.** 그리려는 날짜를 넘기시면 그 날 표가 옵니다.

```cpp
const int32 DayToDraw = Stage->CurrentDay;
if (Progression->HasLedgerForDay(DayToDraw)) { /* 4종 호출 */ }
```

**예 — Day3 Live 가 아직 진행 중일 때**

| 넘긴 값 | 나오는 것 |
|---|---|
| `3` | Day3 의 **지금까지** 쌓인 값. 진행 중이라 계속 늘어납니다 |
| `2` | Day2 의 최종값 |
| `1` | 빈 표 (`HasLedgerForDay(1)` 이 `false`) |

진행 중인 날도 그대로 읽으시면 됩니다 — Live 중 실시간 표시와 Report 결산이 같은 호출입니다.

⚠️ **`HasLedgerForDay()` 를 먼저 확인해 주세요.** 보관하는 것은 **오늘과 어제 이틀뿐**입니다.
없는 날짜는 빈 표가 오는데 **"그 날 매출이 0" 과 모양이 같아서** 이 함수 말고는 구분할 방법이 없습니다.

**표 규칙**

- 원인별 표는 `GetEarnRowsForDay()` → `GetSpendRowsForDay()` 순으로 붙이시면 요청하신 배치가 됩니다
- 값 0 인 사유는 줄이 없습니다. 순서는 enum 선언 순서로 고정입니다
- 부스별은 `FacilityId` 오름차순으로 드립니다
- 환급은 `GetTotalRefundForDay()` 로 분리했습니다. 매출 표에 안 섞입니다
- 임대료 줄은 2,400 을 넘을 수 있습니다(미납 상환 합산). 원장 값을 그대로 그려 주세요
- 합계 줄은 따로 안 드립니다 — 0 인 줄이 빠져 있어서 `Amount` 를 더하면 정확합니다

**부스별만 구조체가 다릅니다.**

```cpp
struct FFF_FacilityRevenueRow { FName FacilityId; int32 Amount; };   // DisplayName 없음
```

경제는 시설의 한글 이름을 모릅니다. **표시 이름은 Building 카탈로그에서 해석해 주세요.**

✅ **부스별 표가 이제 실제 값으로 찹니다 (2026-08-23).** 매출을 넣는 호출부가 붙었습니다 —
NPC 파티가 부스 이용을 마치면(`EFF_QueueExit::Served`) 수수료가 원장에 들어갑니다.
화면에 `잠정`·`더미` 배지가 있다면 떼셔도 됩니다.

**`FacilityId` 는 `FacilityDefinitionId`(`Booth_FoodStall_Grill` 형태)입니다.** Building
카탈로그의 키와 같으므로 표시 이름을 그대로 해석하실 수 있습니다.

⚠️ **같은 정의의 부스를 여러 개 지으면 한 줄로 합쳐집니다.** "1번 음식 부스 vs 2번 음식 부스"
비교는 안 됩니다 — 인스턴스별 구분은 v1 입니다. 지금 표가 답하는 질문은
"어떤 **종류**가 잘 버는가" 입니다.

- 무료 시설(휴식·포토존·화장실·공연)은 애초에 줄이 생기지 않습니다. 공연은 직접 수익이 없고
  사람을 모아 주변 부스를 먹여살리는 집객 장치입니다(`economy_spec.md` §2.4)
- 대기 포기·취소로 빠진 파티는 매출을 만들지 않습니다

### 🔴 enum 의 `DisplayName` 을 직접 읽지 마세요

`UMETA(DisplayName)` 조회는 **에디터 전용**이라 패키징 빌드에서는 `"Gold 부족"` 대신
`"InsufficientGold"` 가 뜹니다. 에디터에서는 정상으로 보이고 자동 테스트로도 안 잡혀서,
시연 빌드를 만들기 전까지 드러나지 않습니다.

문자열 원본을 경제 쪽으로 옮겨 뒀습니다. 아래를 쓰시면 어느 빌드에서든 한글이 나옵니다.

```cpp
// Public/Economy/FF_EconomyDisplayTextLibrary.h — 전부 static BlueprintPure
GetFinalePurchaseResultText(EFF_FinalePurchaseResult)   GetFinaleGradeText(EFF_FinaleGrade)
GetSpendReasonText(EFF_SpendReason)                     GetEarnReasonText(EFF_EarnReason)
```

**고치실 곳은 피날레 실패 이유와 등급 표시 두 군데입니다.** 결산 표의 `DisplayName` 은 이미 이
함수로 채워 드리니 그대로 쓰시면 됩니다.

#### 🔴 종태님 파일 2곳이 아직 그대로입니다 (2026-08-23 코드 확인)

| 파일 | 화면에 뜨는 것 |
|---|---|
| `FF_FestivalViewModelSubsystem.cpp:354` | `GetCurrentStageDisplayText()` — 단계 표시 |
| `FF_LiveHudMenuBar.cpp:168` | Live HUD 탭 라벨 |

둘 다 Stage enum 이라 문자열 원본은 강민서 님이 만드셔야 합니다.
**패키징 빌드에서만 드러나고 자동 테스트로는 안 잡히니** 시연 빌드 전에 정리가 필요합니다.

---

## UI Read Model — 백종태

### `UFF_FestivalViewModelSubsystem`

- **헤더**: `Public/UI/FF_FestivalViewModelSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **상태**: **`스텁`** — 지표 4종은 하드코딩 더미. Stage·Day는 진짜 값
- **역할**: 위 모든 소스를 UI가 쓰기 좋은 형태로 합쳐서 제공

Overview HUD·Report·공통 지표 위젯은 **이 서브시스템만 참조**하고 원본 시스템을 직접 보지 않습니다.
각 모듈이 바뀌어도 위젯을 안 고치게 하기 위해서입니다. 설치 시설 정보 WBP는 별도 예외 경로로,
`UFF_FacilityInfoSubsystem`이 주입한 `FFF_FacilityInfoViewData`만 렌더링하며 원본 시스템은 직접 조회하지 않습니다.

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

**이 ViewModel을 사용하는 위젯은 다른 시스템의 함수를 직접 부르지 않습니다.** 아래 쓰기 요청은 전부
여기를 거칩니다(원칙 1). 시설 정보 패널의 이동·철거는 `UFF_FacilityInfoSubsystem`의 C++ 이벤트를 거쳐
`UFF_BuildModeSubsystem` 명령 경계로 전달됩니다.
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

## 타깃 슈팅 디자인·튜닝 가이드 (2026-08-27)

`Booth_ActivityGame_Shoot` 시설에서 `TargetShooting` 미니게임을 시작합니다. 입력은
`Border_PlayArea` 전체가 받습니다. 화면 픽셀 좌표를 플레이 영역의 0~1 정규화 좌표로 바꿔
Runtime에 발사를 요청하면, 하단 1인칭 물총의 총구에서 해당 좌표까지 물방울 탄환이 이동하고 도착 시 Runtime이 타원
명중 판정을 수행합니다. 비행 중에는 중복 발사를 받지 않으며 점수도 충돌 전까지 바뀌지 않습니다.
`Viewport_Arena3D` 안의 원근 카메라·실내 사격장·조명과 그림자는 분위기용 3D 배경입니다.
실제 과녁(`Border_Target` 안의 `UFF_TargetShootingTargetVisual`), 물 탄환, 조준점, 명중 피드백은 모두
`Canvas_PlayArea`의 같은 2D 좌표계에 합성합니다. 따라서 3D 원근 투영이나 해상도와 무관하게 보이는 과녁 타원과
Runtime 명중 판정이 정확히 일치합니다. 과녁 외곽의 밝은 호는 남은 노출 시간을 시계 방향으로 표시합니다.
모든 표시 요소는 `HitTestInvisible`이므로 WBP를 수정할 때 클릭을 가로채는 위젯을 플레이 영역 위에 올리지 마세요.

화면 구성은 [3D Aim Trainer](https://www.3daimtrainer.com/)의 훈련 화면에서 중앙 조준 공간,
상단의 작은 기록 HUD, 하단 1인칭 무기라는 정보 우선순위만 참고했습니다. 배경·물총·과녁·색상은
FestaFiesta 해안 축제 테마에 맞춰 새로 제작했으며 외부 게임의 이미지나 상표는 사용하지 않습니다.

### 기획 수치 수정

`DA_Minigame_TargetShooting`을 열면 주요 값이 한국어 카테고리로 바로 펼쳐집니다. 각 항목에 마우스를
올리면 단위, 계산 방식, 다른 값과의 제약을 설명하는 한국어 툴팁이 표시됩니다. 유효하지 않은 조합은 Data Validation이
저장을 검증하며, `FF.RebuildTargetShootingUI`는 이 Data Asset의 기획 수치를 덮어쓰지 않습니다.

| 목적 | Data Asset 필드 |
| --- | --- |
| 한 판 진행 시간 | `한 판 플레이 시간` (`DurationSeconds`) |
| 클리어 골드 | `최대 보상 달성 점수` (`ScoreForMaxReward`), `클리어 최대 획득 골드` (`RewardGoldAtMaxScore`) |
| 난이도 곡선 | `TargetLifetimeStartSeconds/EndSeconds`, `TargetRadiusStartNormalized/EndNormalized` |
| HUD·닫기 버튼 회피 | `SpawnSafeMarginNormalized` (표적 전체 외곽 기준) |
| 총알 시작점·비행 속도 | `ProjectileOriginNormalized`, `ProjectileSpeedNormalizedPerSecond`, `ProjectileMinTravelSeconds` |
| 기본 점수·콤보·오발 감점 | `HitBaseScore`, `ComboBonusPerHit`, `MaxComboBonus`, `MissPenalty` |
| 정밀/정중앙 판정 | `PrecisionRadiusRatio`, `BullseyeRadiusRatio`, 각 `ScoreBonus` |
| 반응속도 보너스 | `FastReactionSeconds`, `SlowReactionSeconds`, `MaxReactionScoreBonus` |
| 골든 타깃 | `BonusTargetChance`, `BonusTargetScoreMultiplier`, `BonusTargetLifetimeScale` |

점수는 `기본 + 콤보 + 정밀도 + 반응속도`를 먼저 계산하고 골든 타깃이면 배율을 적용합니다.
시간이 끝나 정상 완료되면 획득 골드는
`round(클리어 최대 획득 골드 × clamp(점수 / 최대 보상 달성 점수, 0, 1))`로 계산합니다.
`RewardGoldAtMaxScore=-1`은 축제 공통 보상값을 사용하고, `0`은 해당 미니게임의 골드 보상을 끕니다.
플레이어가 취소한 세션은 항상 0 Gold입니다.
현재 기본값은 총구 `(0.50, 0.68)`, 속도 `1.8 normalized/s`, 최소 비행시간 `0.16초`, 하단 안전 여백
`0.24`입니다. 탄환 이동을 눈으로 확인할 수 있고 하단 물총과 과녁이 겹치지 않도록 조정한 값입니다.
총알이 명중 지점에 도착해 명중하면 다음 타깃이 나오며, 빗나가면 같은 타깃을 유지해 조준을
보정할 수 있습니다. 승인된 발사는 해당 과녁을 예약하고 탄환이 도착할 때까지 과녁 카운트다운을 멈춥니다.
따라서 연출용 탄환 비행 때문에 제때 쏜 과녁이 사라지지 않습니다. 발사하지 않은 타깃이 시간 안에 맞지 않으면
`시간 초과` 피드백 후 다음 타깃으로 넘어갑니다.

### UI/모션 수정

`WBP_FF_TargetShooting`의 **Class Defaults → Visual Style**만 수정합니다. 색·링 두께·폰트 크기·
스폰/긴급 안내 펄스·물방울 탄환 코어/광원/트레이서·물총 크기/반동/총구 섬광·피드백 상승 거리/시간·플레이 영역 플래시·커스텀
조준점뿐 아니라 3D 배경 카메라 위치·키라이트·스카이라이트도 게임 규칙과 분리되어 있습니다. 2D 표적 외곽·정밀·불스아이 링은 Runtime 판정 비율로 그리며, 표적 본체 크기는
애니메이션으로 확대하지 않습니다. 따라서 보이는 타원 내부와 클릭 판정 범위가 어긋나지 않습니다.
HUD에는 시간, 점수, 명중률, TPS, 평균 반응시간, 빗나감·놓친 과녁, 콤보를 표시하고 결과 화면에도 같은 성과 지표를
요약합니다.

3D 배경 표면은 `M_MG_TargetShooting_Arena3D` 재질을 사용합니다. `Color`, `Roughness`, `Metallic`,
`EmissiveStrength` 파라미터가 있으며 재생성 스크립트는
`Tools/ui/create_target_shooting_3d_material.py`입니다. `Viewport_Arena3D`는 이전 WBP에 없어도 런타임이
`Canvas_PlayArea` 최하위 레이어에 자동 생성합니다.

필수 WBP 이름은 기존 계약에 `Image_WaterBlaster`, `Text_MuzzleFlash`, `Text_WeaponStatus`를 추가했습니다.
`Text_ShotFeedback`과 `Text_Reticle`은 선택 사항이며,
없으면 런타임이 `Canvas_PlayArea` 안에 자동 생성합니다. 따라서 기존 WBP를 깨지 않고 점진적으로
레이아웃을 바꿀 수 있습니다. 두 선택 위젯을 다른 패널로 옮겨도 초기화 시 플레이 캔버스 직속으로
안전하게 다시 연결하며, 연결할 수 없는 경우에는 하드웨어 커서를 숨기지 않습니다.

---

# UI가 필요한데 아직 없는 것

기획서 6.3의 Overview 필수 표시 항목입니다.

| 필요한 것                                         | 예상 소유자    | 상태        | UI에서 쓰는 곳                                                          |
| --------------------------------------------- | --------- | --------- | ------------------------------------------------------------------ |
| **Satisfaction** (`SimSatisfaction` 0~100)    | 김시리 · 이정원 | ❌ 없음      | Overview 지표, Report, SNS                                           |
| **`ScoreSatisfaction`** (= Sim + FinaleBonus) | 김시리       | ⚠️ **FinaleBonus 는 완료**(`GetFinaleSatisfactionBonus`), `SimSatisfaction` 이 없어 합산 불가 | Report 분해식, Finale 구매 예상치 |
| **`Gold`** (잔액)                               | 김시리       | ⚠️ 원본 완료·ViewModel 연결 대기 | 공통 상단 바, Finale·설치 구매 가능 판정                                        |
| **`TotalRevenue`** (축제 누적 매출)                 | 김시리       | ⚠️ 원본 완료·ViewModel 연결 대기 | 공통 상단 바                                                            |
| **`TodayRevenue`** (오늘 매출)                    | 김시리       | ⚠️ 원본 완료·ViewModel 연결 대기 | Report — UI가 누적값에서 빼지 않습니다                                         |
| 지출·매출 **원인별 분해** (`EFF_SpendReason`/`EFF_EarnReason`) | 김시리 | ✅ **2026-08-19 완료** — `GetEarnRowsForDay`/`GetSpendRowsForDay` | Report 드릴다운 |
| **부스별 매출 표** (`RevenueByFacility`)          | 김시리       | ✅ **2026-08-23 완료** — `GetRevenueByFacilityRowsForDay` 가 이제 실제 값을 돌려줍니다(이용 완료 → 수수료 결선) | Report — 부스별 수익 표 |
| **Safety**                                    | 이정원       | ❌ 없음      | Overview 지표, 경고                                                    |
| Staff 보유·가용 인원                                | 김시리       | ⚠️ 원본 완료·ViewModel 연결 대기 | Event 배치 UI, MorningCheck 고용 화면 |
| 🆕 **임대료 미납 총액** (`GetTotalArrears`)          | 김시리       | ⚠️ 원본 완료·ViewModel 연결 대기 | 상단 바 자금난 경고, Report |
| `CauseId` 롤업                                  | 김시리       | ⚠️ 창구만 있음 | Report 드릴다운 (AT-005) — `UFF_NpcNeedMetricsSubsystem`이 원본, 채우는 쪽 대기 |
| 저장 슬롯 목록                                      | 김시리       | ❌ 없음      | 플레이 허브                                                             |

### 🆕 전체 사이클 프로토타입에서 새로 확인된 것 (2026-08-19, 백종태)

타이틀 → 지구본 → 셋업 → 홍보 → Day 1·2·3 → 피날레 → 최종결산까지 **한 바퀴를 실제로 돌려 보고**
없어서 더미로 채운 것들입니다. 화면이 이미 그 모양으로 만들어져 있으니 API 모양만 맞춰 주시면 붙습니다.

| 필요한 것 | 예상 소유자 | 상태 | UI에서 쓰는 곳 |
| --- | --- | --- | --- |
| **`UFF_FestivalEventSubsystem`** — 사건 목록·심각도·필요 인력·남은 시간·`CauseId` | 김시리 · 백종태 (공동) | ❌ 없음 (`FF_FestivalDefinition.h:146`) | Live 우측 상단 상시 사건 트레이 |
| **스태프 배치 Command + 결과 이벤트** | 김시리 | ❌ 없음 | 사건 트레이의 `스태프 N명 배치` |
| **홍보 구매 Command** | 김시리 | ⚠️ 등급 데이터만 있고 `EFF_SpendReason::Promotion` 호출부가 0곳 | 셋업 HUD `홍보` 패널 |
| **뒷정리 등급 효과 계수** | 김시리 | ⚠️ 가격 3개만 있음 (`FF_FestivalDefinition.h:110` TODO) | Day 마감 시트 — 지금은 `효과 계수 미확정` 배지로 표시 중 |
| **최종 점수 산식·등급 가중치** | 김시리 | ❌ 없음 | 최종 결산 — 지금은 `가중치 잠정` 문구를 화면에 띄우고 있음 |
| **저장 슬롯 저장·불러오기 Command** | 김시리 | ❌ 없음 | ESC 수동 저장, 플레이 허브 불러오기 |
| **SNS 반응 Read Model** + `MessageTemplateId` → 현지화 문장 매핑 소유자 | 미정 | ❌ 없음 | Live `SNS` 패널 |
| **날씨 Read Model** | 강민서 | ❌ 없음 | 개장 준비 견적서 — 지금은 `더미` 배지 |

~~**시설 유지비에 대응하는 `EFF_SpendReason` 이 없습니다.**~~ → **2026-08-23 종료 (김시리).**
**기획서에 시설 유지비라는 개념이 없습니다.** 경제 스펙에도 처음부터 없던 항목이라
`EFF_SpendReason` 에 사유를 추가하지 않습니다. **마감 화면 원장에서 유지비 줄을 빼 주세요** —
`FacilityInstall` 로 묶여 들어가는 것이 아니라, 애초에 유지비 지출이 발생하지 않습니다.

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
| 2026-08-18 | 김시리 | 🆕 **예약 요청 4건 중 3건이 열렸습니다** — `UFF_ProgramReservationSubsystem` 신설로 프로그램 목록(시작 시각 순 정렬)·예약 상태·변경 델리게이트가 생겼습니다. 남은 하나(구매·취소 Command)는 Gold를 쓰는 일이라 경제 S8과 같이 붙입니다. 지금은 구매 화면이 없어 **목록이 비어 있는 것이 정상**입니다 |
| 2026-08-18 | 김시리 | 🆕 **피날레 구매가 구현됐습니다** — Day3 MorningCheck 구매 화면이 필요합니다(담당 백종태). 버튼 활성화는 `CanPurchaseFinaleNow()`, 구매는 `TryPurchaseFinale()`이고 실패 이유가 한글로 옵니다. **1회 구매·변경 불가라 확인창이 필요합니다.** 만족도 보너스는 Gold가 아니고 평가 점수 합산은 관객 만족도가 생길 때 붙입니다 |
| 2026-08-18 | 김시리 | 🆕 **"다음 날" 버튼이 이제 하루치 임대료를 정산합니다** — `RequestAdvanceToNextDay()`를 누르면 잔액·밀린 임대료·오늘 매출·인원 4개 값이 바뀝니다(전부 기존 델리게이트로 통지). 종태님 쪽 수정은 없고, 버튼 직후 UI가 잔액을 직접 계산하지만 않으면 됩니다. 문서에 빠져 있던 밀린 임대료(`GetTotalArrears`)와 인원(`GetTotalStaffCount`) 2종 추가 |
| 2026-08-13 | 김시리 | 🆕 시설 철거 환급 기능 수용(한광훈 구현, Dev 병합) — 철거 시 설치비 50%를 `OnGoldChanged`로 돌려받습니다. `OnTotalRevenueChanged`는 안 쏩니다(환급 ≠ 매출). "Gold 상승 = 매출 상승"으로 가정하는 UI 코드가 있다면 확인 필요 |
| 2026-08-19 | 강민서 | `OnDayEnded` 델리게이트 신설. AdvanceToNextDay() 호출 시 방금 끝난 날짜와 그날 Live 진행 시간을 broadcast. 이월 데이터(정비/청결 등) 계산은 수신 측(광훈/시리) 담당 |
| 2026-08-19 | 백종태 | 전체 사이클 UI 프로토타입(타이틀→피날레→최종결산)을 한 바퀴 돌려 보고 **없어서 더미로 채운 것 8건**을 "UI가 필요한데 아직 없는 것"에 추가. `FFF_DayLedger` 노출 형태 질문에 회신(원인별 표·부스별 표 **둘 다** 필요, 환급은 분리, 하루 경계 스냅샷 필요). 시설 유지비에 대응하는 `EFF_SpendReason` 부재 보고 |
| 2026-08-19 | 김시리 | **결산 표 4종 구현 완료** — `GetEarnRowsForDay`/`GetSpendRowsForDay`/`GetRevenueByFacilityRowsForDay`/`GetTotalRefundForDay` + `HasLedgerForDay`. 요청하신 형태에 **날짜 인자**가 붙었습니다(진행 중인 하루와 방금 끝난 하루를 안쪽에서 고릅니다). 부스별만 `FFF_FacilityRevenueRow` 로 분리 — 경제가 시설의 한글 이름을 모릅니다. **부스별 표는 S10 까지 빈 표입니다** |
| 2026-08-19 | 김시리 | 🔴 **정정: enum 의 `DisplayName` 을 직접 읽으면 패키징 빌드에서 영문이 뜹니다.** 8/18 에 "피날레 실패 이유가 한글로 온다"고 안내한 것이 틀렸습니다. `UFF_EconomyDisplayTextLibrary` 신설 — 피날레 실패 이유·등급 표시 두 군데를 이 함수로 바꿔 주세요. Stage 단계 이름도 같은 문제입니다(강민서) |
| 2026-08-24 | Codex | 타깃 슈팅 레퍼런스 UX 반영 — 정밀도·반응속도·골든 타깃 점수, 즉시 명중 피드백, 커스텀 조준점, TPS/명중률/평균 반응시간 HUD와 결과 요약, Rules/Visual Style 분리 튜닝 가이드 추가 |
| 2026-08-26 | Codex | 타깃 슈팅을 물총 Aim Trainer 방식으로 교체 — 실제 WBP·DataAsset 재생성, 1인칭 물총/반동/총구 섬광, 물방울 탄환 비행, 중앙 집중형 HUD, 저장 에셋 자동화 검사 추가 |
| 2026-08-26 | Codex | 타깃 슈팅 3D 사격장 전환 — UMG 실시간 3D viewport, 원근 카메라·조명·사격장, 이동식 금속 과녁판, 3D 물 탄환, 전용 lit 재질과 2D 자동 폴백 추가 |
| 2026-08-27 | Codex | 타깃 슈팅 하이브리드 정리 — 3D는 배경 전용, 판정과 정확히 겹치는 2D 과녁·탄환/수명 드레인, 발사 중 과녁 예약, 놓친 과녁 지표, Data Asset 한국어 편집명·툴팁·미니게임별 클리어 골드 추가 |
| 2026-08-23 | 김시리 | ✅ **부스별 매출 표가 실제 값으로 찹니다.** NPC 파티가 부스 이용을 마치면(`Served`) 수수료가 원장에 들어갑니다. `FacilityId` 는 `FacilityDefinitionId` 라 Building 카탈로그 키와 같습니다 — 표시 이름을 그대로 해석하실 수 있습니다. **같은 정의의 부스 여러 개는 한 줄로 합쳐집니다**(인스턴스별 구분은 v1). 화면의 `잠정`·`더미` 배지를 떼셔도 됩니다 |
| 2026-08-23 | 김시리 | ✅ **E-26 시설 유지비 종료 — 기획서에 없는 개념입니다.** `EFF_SpendReason` 에 사유를 추가하지 않습니다. 마감 화면 원장에서 **유지비 줄을 빼 주세요**. `FacilityInstall` 로 묶이는 것이 아니라 지출 자체가 발생하지 않습니다 |
| 2026-08-23 | 김시리 | 🔴 **표시 문자열 정정이 아직 안 붙었습니다** — `FF_FestivalViewModelSubsystem.cpp:354`·`FF_LiveHudMenuBar.cpp:168` 두 곳입니다. 패키징 빌드에서만 드러나니 시연 전에 정리 필요 |
| 2026-08-23 | 김시리 | 🔴 **8/19 정정 요청에 답변 — "1일차만 아침 단계를 건너뛴다"가 맞습니다.** `DT_DaySequence` 에 `Day1_MorningCheck` 행이 없습니다(팀 의도). 셋업 예산은 **7,600**이므로 프로토타입이 10,000 기준이면 바꿔 주세요 — "화면 만들 때 알아야 하는 것" 3번 |
| 2026-08-23 | 김시리 | 🔧 전체 자동화 테스트가 크래시로 중단되던 것을 고쳤습니다(피날레 콘솔 명령 수명 문제, 장원재 님 파일·별도 통지). 스위트가 55개에서 멈추던 것이 **359 통과 / 4 실패**로 복구됐습니다 |
| 2026-08-23 | 김시리 | 문서 정리 — 종료된 협의 경위와 과거 서술을 걷어내고 **지금 필요한 것만** 남겼습니다. ViewModel 연결 5종에 실제 연결 여부를 표시했습니다 |
| 2026-08-24 | 김시리 | 🔴 **셋업 예산 표시 제안 — A(예산 내역) + B(상단바 고정비·미납) 두 개면 닫힙니다.** 둘 다 **경제 쪽 추가 작업 0건**입니다. **B가 A보다 급하다고 봅니다** — 미납은 동작하는 기능인데 화면에 나오는 곳이 0곳이라 플레이어가 조용히 파산합니다. C(계약 카드)는 선택, D(연출용 래치)는 하시겠다고 하면 제가 만듭니다. 징수를 늦춰 셋업을 10,000으로 만드는 안과 Day1 전용 단계 추가는 **안 하는 쪽으로 정리**했습니다(이유는 3번) |

