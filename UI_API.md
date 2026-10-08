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
| `StartNewFestival()`                   |                     | 축제를 Day1·Setup으로 초기화하고 시계 시작 (2026-08-20 추가) — OpenLevel 직후 1회 호출 필요 |
| `GetLiveDurationForCurrentDay()`       | `float`             | 오늘 Live 총 길이(초). Day1=240초, Day2·3=300초 |
| `OnNightLightingActivated`             | (인자 없음)             | 밤 조명 켤 시간이 됐을 때 (환경 시스템용) |
| `OnDayEnded`                           | `FOnDayEnded`       | (EndedDay, TodayLiveDuration) — 하루 종료 시 이월 정보 전달 (2026-08-19 추가, 이미 별도 항목으로 아래에 있음) |
| `GetLiveClockStringFromMinutes(int32)` | `FString`           | 임의의 LiveMinutes를 "17:30" 형식 문자열로 변환 (2026-08-26 추가) |
| `GetCurrentLiveClockString()`          | `FString`           | 지금 시각을 "17:30" 형식으로 반환 |
| `GetCurrentStageDisplayName()`         | `FText`             | 현재 Stage의 한글 표시 이름. switch 기반으로 패키징 메타데이터에 의존하지 않음 |
| `IsCurrentStageSkippable()`            | `bool`              | 지금 단계가 스킵 가능한지 (2026-08-13 public 전환됨). **⚠️ 2026-09-08 판정이 하나 늘었습니다 — 이 절 말미 「필수 설치 시설 게이트로 계약이 바뀐 것 2건」 참조** |
| `CanTransitionTo(NewStage, OutBlockingReason)` | `bool`     | 필수 설치 시설 조건으로 전환이 허용되는지 + 막힌 사유 문구 (2026-09-08 추가, 김시리) |
| `GetStageAdvanceBlockingReason()`      | `FText`             | 현재 단계의 **다음** 단계가 막혔을 때의 안내 문구. 막히지 않았으면 빈 `FText` (2026-09-08 추가, 김시리) |
| `OnFestivalStarted`                    | (인자 없음)          | 축제가 새로 시작됐음을 알리는 신호 (StartNewFestival() 호출 시 발동|
| `SetMinigameInProgress(bool)`          |                      | 미니게임 시작/종료 시 호출. true면 배속을 무조건 1배속으로 고정 
| `IsMinigameInProgress()`               | `bool`               | 지금 미니게임 진행 중인지 즉시 조회 (2026-08-28 추가)
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

**답변 (강민서 → 백종태, 관련 요청 2건)**

### `StartNewFestival()`

```cpp
UFUNCTION(BlueprintCallable, Category = "Festival Stage")
void StartNewFestival();
```

- **상태**: `구현완료`

호출 시 아래가 한 번에 초기화됩니다:
- CurrentDay = 1
- CurrentStage = Setup
- ElapsedLiveSeconds = 0
- bFestivalEndedBroadcasted = false
- bNightLightingActivated = false (추가로 넣었습니다 — 안 하면 재시작해도 밤 조명이 켜진 채로 남는 버그가 생길 수 있어서요)

🔴 **`OpenLevel` 직후 이 함수를 한 번 불러 주세요.** `bFestivalStarted` 가 false 인 동안에는
`Tick()` 이 시간 진행 로직 전체를 건너뛰므로, 부르기 전에는 GameInstance 가 살아 있어도
**시계가 아예 안 흐릅니다.**

### `SunsetTransition` 단계 — 순수 연출 구간

`DT_DaySequence` 기준으로 Day1·2·3 모두 같습니다.

- **지속 시간**: **20초 고정** (Min=Max=20)
- **스킵**: 불필요. `bSkippable = False` — 스킵 버튼을 만들지 않으셔도 됩니다
- **플레이어 입력**: 받지 않습니다. 이 구간엔 태양 Rotator 를 Lerp 로 회전시키는
  연출(`BP_EnvironmentManager`)만 돌아갑니다

⚠️ **Stage 는 7종이 아니라 8종입니다.** `SunsetTransition` 을 포함한 `EFF_FestivalStage` enum
기준이 맞습니다(`module-ownership.md` 3절이 7종으로 적혀 있어 어긋납니다).

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

> **Live 길이는 날마다 다릅니다** (표 덤프 실측) — Day1 240초 / Day2·3 300초.
> 밤 조명 트리거는 세 날 모두 90초입니다. 실행 중 조회는 `GetLiveDurationForCurrentDay()`.
>
> ⚠️ **`ElapsedLiveSeconds` 는 "현재 단계 경과"입니다.** 단계가 바뀔 때마다 0 으로 리셋되고
> Setup·Closing 에서도 누적됩니다. Live 경과로 쓰시려면 `IsLiveStage()` 로 먼저 거르세요.
>
> ✅ **2026-08-26 완료: `FString GetLiveClockStringFromMinutes(int32 LiveMinutes) const`**
> — 프로그램 카드의 임의 Live 경과분을 `17:30` 형식으로 바꿉니다. UI는 `StartHour = 17`을
> 복제하지 않고 이 함수를 사용합니다.

### `GetLiveClockStringFromMinutes` — Live 경과분 → `17:30`

```cpp
UFUNCTION(BlueprintPure, Category = "Festival Stage")
FString GetLiveClockStringFromMinutes(int32 LiveMinutes) const;
```

- **상태**: `구현완료`
- ⛔ **UI·경제 어느 쪽도 `StartHour = 17` 을 복제하지 않습니다.** 개장 시각의 원본은 이 함수 하나입니다

> 🔧 **`GetCurrentLiveClockString()` 리팩터링 — 변환 필요 없습니다** (김시리 답변).
> 걱정하셨던 초/분 불일치가 실제로는 없습니다. 지금 그 함수가
> `StartHour * 60 + FMath::FloorToInt(ElapsedLiveSeconds)` 로 **경과 초를 이미 게임 분으로
> 1:1 취급**하고 있어서(`FF_FestivalStageSubsystem.cpp:512`, 바로 아래 주석에도 그렇게 적혀
> 있습니다) 두 함수가 같은 규약을 씁니다. 그래서 `/ 60` 없이 한 줄이면 됩니다:
>
> ```cpp
> return GetLiveClockStringFromMinutes(FMath::FloorToInt(ElapsedLiveSeconds));
> ```
>
> 급하진 않지만 하시면 `StartHour = 17` 중복(`:509`·`:524`)이 한 곳으로 줄어듭니다.

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

### ⚠️ 필수 설치 시설 게이트로 계약이 바뀐 것 2건 (2026-09-08, 김시리)

`UFF_FestivalStageSubsystem` 은 강민서 님 소유인데 필수 설치 시설 판정을 얹었습니다. **UI 코드를 수정하지 않아도 표시가 달라지는 변경 2건**만 여기 적습니다.

**1. `IsCurrentStageSkippable()` 이 필수 시설 조건도 봅니다.**

```
전: return Range.bSkippable;
후: return Range.bSkippable && CanTransitionTo(Range.NextStage, Reason);
```

`ViewModel::CanSkipCurrentStage()` 가 이 값을 그대로 중계하므로(아래 「Command」 절 계약), **메인 무대가 없으면 스킵 버튼이 비활성됩니다.**

**2. `RemainingTime == 0` 이 오래 지속될 수 있습니다.**

값의 범위는 그대로입니다(예전에도 0 으로 클램프). 달라진 것은 **0 에서 머무는 시간**입니다 — 조건을 못 채우면 시간이 만료된 채 그 단계에 머무릅니다. `0` 을 "전환 임박"으로 가정하고 연출을 트리거하거나 `OnRemainingSecondsChanged(0)` 을 단계 종료 신호로 쓰고 있으면 확인이 필요합니다. `GetStageAdvanceBlockingReason()` 이 비어 있지 않은 동안이 "대기 중"입니다.

> 막힌 사유는 **Setup HUD 의 CTA 버튼 왼쪽에 이미 표시됩니다**(종태님이 만들어 두신 `Text_CtaReason` 자리). 자세한 내용과 백로그 3건은 「Utility AI · 경제 · 진행 — 김시리」 절의 `UFF_RequiredFacilitiesSubsystem` 아래에 있습니다.
>
> **강민서 님께**: `IsCurrentStageSkippable()` 의 의미가 넓어졌습니다. `RequestTransition` 은 게이트에 막힐 때 **사유가 바뀔 때만 한 번** 로그를 남깁니다 — 자동 시간 만료로도 매 Tick 불리는 함수라 매번 남기면 초당 수십 줄이 되고, 반대로 완전히 조용하면 "왜 안 넘어가는가"의 단서가 사라져 그 중간을 택했습니다.

#### `GetStageAdvanceBlockingReason()`

- **헤더**: `Public/Time/FF_FestivalStageSubsystem.h`
- **접근**: `GameInstanceSubsystem` · `BlueprintPure`
- **시그니처**: `FText GetStageAdvanceBlockingReason()`
- **갱신 주기**: 요청 시
- **상태**: `구현완료`
- **비고**: 막히지 않았으면 **빈 `FText`** 입니다 — `IsEmpty()` 로 판정하세요. 필수 시설이 여러 종류면 사유가 `\n` 으로 이어진 **여러 줄**이 옵니다. 문구 예: `메인 무대: 1개 추가 설치가 필요합니다. (설치 0/1)`

#### `CanTransitionTo(NewStage, OutBlockingReason)`

- **접근**: `BlueprintPure`
- **시그니처**: `bool CanTransitionTo(EFF_FestivalStage NewStage, FText& OutBlockingReason) const`
- **상태**: `구현완료`
- **비고**: 특정 단계로의 전환 가능 여부. 단계 표의 `bSkippable` 과는 별개 판정입니다.

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

### 시설 Aurora / Clarity UI 갱신 (2026-09-20)

- 실제 건축 카탈로그는 `UFF_PrepPanelBuild`입니다. 카드 클릭은 `ActivateCatalogFacility`로 바로 배치
  미리보기를 엽니다. Gold 차감은 권위 설치 명령 성공 시에만 일어납니다. 방향키 및
  `RequestFacilityFocus`는 포커스만 이동합니다. 좌우 드래그·휠은 탐색만 하고 카드를 활성화하지 않습니다.
- 필터 없는 전체 건축 목록은 미설치 필수→설치 완료 필수→일반 시설 순으로 표시합니다.
  카테고리·검색·즐겨찾기 선택 결과는 기존 순서와 필터 의미를 유지합니다.
- 배치 위젯은 좌측 시설 요약과 그 아래 조작 안내, 우측 하단의 목록·확정으로 구성됩니다.
  `도구 R` 버튼·원형/정밀 패널·R 키 진입은 제거했습니다. 기존 `ToggleRadialMenu` 호출은
  메뉴를 열지 않는 호환용 API이며 Q/E, Shift 회전, Ctrl/Alt 간격, G 격자, T 직접 조정은 유지합니다.
  조작 안내는 배치 진입 시 기본으로 열립니다. `안내 닫기`/`조작 안내`로 전환하고,
  같은 배치 중에는 닫힘을 유지하며 다음 배치 진입에서 다시 기본 표시합니다.
  기존 시설을 옮길 때의 서비스 시간은 `GetSelectedFacilityQueueRules()`가 강화된 실제 Queue 규칙에서 읽습니다.
- 시설 정보의 `QueueCapacityValueText`와 `ServiceCapacityValueText`는 대기 정원과 동시 이용 정원을 분리합니다.
  실시간 이용 현황·처리 속도는 기존 `FFF_FacilityInfoViewData`를 통해 표시합니다.
- 조명 지원 시설은 공용 패널의 `조명 편집` 탭을 사용합니다. HSV/RGB/HEX·밝기는 기존 조명 명령으로
  즉시 미리보기되며, `설정 적용` 전 취소·패널 닫기·시설 선택 변경은 편집 시작 당시 시설 GUID에 원본을 복원합니다.
  적용 후에는 새 값이 기준이 됩니다. 밝기는 0~500%, 슬라이더 간격은 5%입니다.
- 조명 패널은 이동·접기가 가능하며 텍스트 입력, 색상 선택, 밝기 조절, 제목 드래그 동안 카메라 입력을 차단합니다.
  조명·강화 지원 여부와 건축/운영 단계 판정은 기존 권위 규칙을 유지합니다.
- 시설 정보는 직접 탭으로 전환하고, 조합은 이전/다음 버튼으로 확인합니다. 조명은 480×620 영역에서
  색상 휠·프리셋과 HSV/RGB 정밀 입력을 전환합니다. 유료 프로그램은 목록/상세를 전환하지만
  구매는 비용이 적힌 별도 확정 버튼에서만 요청합니다.
- 매출 순위는 오늘/축제 누적과 해당 기간의 비중을 표시합니다. 합계가 0이면 비중은 `—`이며,
  수익 시설 없음과 아직 결제 없음 상태를 구분합니다. 도감은 발견 이력과 현재 적용 여부를 구분합니다.
- `/Game/FestaFiesta/UI/Facility/Aurora`의 UI 재질이 초록·민트·연두 광택을 만듭니다.
  저장된 WBP의 브러시가 재질을 직접 참조하므로 실행 중 위젯 트리를 다시 만들지 않습니다.
- 건축 화면은 `/Game/FestaFiesta/UI/Building/Clarity`의 세이지 표면과 얇은 간섭광을 사용하며,
  프로그램 시설은 별도의 라벤더·청색 펄 재질과 배지로 구분합니다. 도감은 중간 크기의 중앙 창에서
  6열을 유지하고, 매출 순위는 시설 수에 맞춰 높이를 조절합니다.
  1일차 필수 시설은 붉은 펄 카드와 썸네일 왼쪽 아래의 필수 배지로 표시하며, 프로그램 배지와 겹치지 않습니다.
- 마이그레이션·조작·검증 절차는 `Tools/BuildModeClarity/README.md`를 참고합니다.

배치 보조 확장(2026-09-21): `UFF_BuildModeSubsystem::GetPlacementGuidance(Index)`는 기존 검증의
원인 제목·해결 안내·실제 방해 대상과 원인 페이지 수를 제공합니다. `ToggleDirectPlacementAdjustment()`는
프리뷰를 고정하고 `AFF_BuildPlacementGizmo`의 월드 XY/회전 핸들을 사용합니다. `T`로 다시 커서 배치에
돌아가며, 직접 조정 중에는 Enter/확정 버튼만 설치합니다. `FindNearbyPlacement()`는 현재 방향을 보존한
6m 이내 후보를 제한된 프레임 예산으로 검증하고 프리뷰만 이동합니다. `IsFindingNearbyPlacement()`로
진행 상태를 표시하며 자금·한도·단계 문제에는 이 액션을 제공하지 않습니다. 검색·조정은 비용과
시설 상태를 변경하지 않으며, 확정은 기존 `CommitPlacement()`의 재검증 경로를 사용합니다.
`FFFZSPlacementValidation::Obstructions`는 최대 12개의 표시 전용 진단입니다. 기존 충돌 쿼리에서
얻은 대상 이름·회전 경계·실패 영역을 제공하며 별도의 권위 판정이나 저장 데이터로 사용하지 않습니다.
카탈로그 분류 아이콘은 `Building/Clarity/MI_UI_CategoryClarity_*` SDF 픽토그램을 사용합니다.
- 명시적인 1회 적용 및 재실행 보존 검사는 `Tools/FacilityAurora_20260920/apply_and_verify.py`에 있습니다.
  일반 실행이나 WBP 열기에서 마이그레이션을 자동 실행하지 않습니다. 작업 전 에셋 사본은
  `Saved/FacilityAurora_20260920/before_assets`에 보관했습니다.

### 개발용 Zone·시설 QA 도구 (참고)

현재 수동 통합 QA는 `Content/FestaFiesta/Maps/Test_Main`에서 프로덕션
`UFF_BuildModeSubsystem`과 개발용 `UFF_ZoneLifecycleTestPanelSubsystem`을 함께 사용합니다.
Editor와 Shipping 패키지의 `Test_Main`에서는 `WBP_FF_ZoneLifecycleTestPanel`이 숨김 상태로 생성되고
`Ctrl+G`로 표시됩니다. Festival 단계와 시설 Open·Close·Emergency 권위 API를 관찰·실행하고,
설치·이동·철거는 실제 건축 모드 경로로 확인합니다.

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

`UFF_BuildModeSubsystem`은 `Closed → CatalogOpen ↔ BuildMode`, `CatalogOpen → Placing`과
`BuildMode → Moving → BuildMode` 상태를 소유한다. 건축 세션은
`UFF_FestivalStageSubsystem::IsReconfigureAllowed()`와 FFZS `ChangePhase`
(`InitialSetup`/`MorningCheck`)가 모두 허용할 때만 진입할 수 있다. 활성 세션 중 어느 한 정책이
닫히면 카탈로그·프리뷰·시설 선택·입력 상태를 정리하고 `Closed`로 자동 전환한다.

신규 설치는 선택한 `UFFZSFacilityDefinition::Spec`을, 이동은 기존 액터의 권위 Spec·GUID·현재 회전을
`AFFZSPlacementPreviewActor`와 `ExecuteCommand()` 경로에 전달한다. 따라서 Zone 경계·충돌·관람 공간·
Queue 예약·보호구역·지면·내비게이션·운영 단계 검증을 우회하지 않는다.

배치 회전의 권위 표현은 `FFFZSGridPlacement::YawCentidegrees`(0.01도 단위)다. 기존
`RotatePlacement(int32 QuarterTurnDelta)`는 Blueprint 호환을 위해 90도 증감 래퍼로 유지하고, 새 UI는
`RotatePlacementDegrees(float)`·`SetPlacementYawDegrees(float)`·`GetPlacementYawDegrees()`를 사용한다.
`Q/E` 스냅 간격, `Shift+Q/E` 자유 회전 속도, 위치 스냅 간격은 각각
`UFFZSDeveloperSettings::PlacementRotationSnapDegrees`,
`FreePlacementRotationSpeedDegreesPerSecond`, `PlacementPositionSnapCm`에서 읽는다. 유효하지 않은 config의
안전값은 45도, 초당 90도, 1cm다. 자유 회전은 매 프레임 최신 yaw만 기록하고 전체 배치 검증은 기존
0.08초 주기를 유지하며, 확정 직전에는 최신 커서·각도·revision을 강제 재검증한다.

활성 건축 세션에서는 설치 시설의 footprint(파랑)·queue 예약 범위(청록)와 명시적 관람 공간을 표시한다.
시설 둘레의 안전 여유 거리와 해당 수치·범례는 표시하지 않으며, 기존 `ClearanceCells` 값은 배치 판정에 적용하지 않는다.
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
| `B` | 재배치 허용 단계에서 첫 입력은 배치 카탈로그를 즉시 열고, 다음 입력은 `BuildMode`로 전환. 이후 두 상태를 오가며 불허 단계에서는 진입하지 않음 |
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
- 카드는 Asset Manager에 등록된 실제 FacilityDefinition 목록으로 생성된다. 시설 수를 고정값으로 가정하지 않는다.

### 설치 시설 공용 정보 패널 (2026-08-29 갱신)

- **헤더**: `Public/UI/Facility/FF_FacilityInfoTypes.h`, `FF_FacilityInfoSubsystem.h`,
  `FF_FacilityInfoPanel.h`, `FF_FacilityPaidProgramTypes.h`,
  `FF_FacilityPaidProgramPicker.h`, `FF_FacilityPaidProgramCard.h`
- **접근**: `UFF_FacilityInfoSubsystem` (`TickableWorldSubsystem`)
- **기본 WBP**: `/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel.WBP_FF_FacilityInfoPanel_C`
- **유료 프로그램 WBP**:
  `/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramPicker.WBP_FF_FacilityPaidProgramPicker_C`,
  `/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramCard.WBP_FF_FacilityPaidProgramCard_C`
- **갱신 주기**: 선택 즉시, 선택 시설의 non-Remove World Change 다음 Tick, 선택 중 0.25초 간격.
  Remove·World Snapshot 복원은 즉시 선택 해제
- **상태**: `구현완료`

건축 모드와 Live가 같은 비모달 우측 패널을 사용한다. 서브시스템이 Zone·Queue·Economy·Stage·Catalog를
읽어 불변 표시 스냅샷 `FFF_FacilityInfoViewData`로 평탄화하며, WBP는 게임플레이 서브시스템을 직접
조회하거나 값을 다시 계산하지 않는다. 0.25초 샘플링은 이 coordinator 내부에만 한정되며 위젯은 폴링하지 않는다.

유료 프로그램 Picker도 같은 선택 시설을 보여 주는 **두 번째 우측 표현**이다. `유료 프로그램 편성`을 누르면
시설 정보 패널을 먼저 접고 Picker 하나만 표시하며, Picker를 닫거나 편성에 성공하면 선택이 유효한 경우 시설
정보 패널로 돌아온다. 두 WBP를 동시에 보이게 하지 않는 상호 배타 규칙은
`UFF_FacilityInfoSubsystem::SetPaidProgramPickerOpen()`이 소유한다. Blueprint에서 별도 visibility를 덧씌워
두 창을 함께 띄우지 않는다.

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
시설 전체 순회는 없으며, 커서가 패널 위에 있거나 건축·모달·ActorView에서 피킹이 막히면
Hover를 즉시 정리한다. 패널이 열린 시설은 `Selected` 표시가 우선하며, Live 이탈·시설 제거·World Snapshot
복원·Subsystem 종료 때 Hover와 Selected 표시를 모두 해제한다.
캐릭터 그라운드 모드(`CharacterControl`)에서는 관찰 카메라 이동만 막고 시설 피킹은 유지하므로,
같은 좌클릭 경로로 시설 정보 패널과 미니게임 진입 동작을 사용할 수 있다.

제품 건축 경로의 `Build` 선택은 `UFF_BuildModeSubsystem`이 소유한다. 활성 건축 세션은 Festival Stage와
FFZS Phase의 교집합 게이트를 유지하며, 이동·철거 액션 경계에서는 시설 GUID·권위 레지스트리·FFZS Phase·
시설 상태와 점유 여부를 다시 검증한다. Stage가 재배치를 금지하면 `OnStageChanged` 또는 다음 Tick에서
세션 자체를 종료한다. 단, 공개 `SelectFacility(..., Build)` 함수 자체는 Stage/Phase 진입을 막지 않으므로
제품 건축 흐름을 우회하는 진입점으로 사용하지 않는다.

**`FFF_FacilityInfoViewData` (`BlueprintType`, 전 필드 `BlueprintReadOnly`)**

| 묶음 | 필드 |
| --- | --- |
| 식별·Context | `FacilityInstanceId`, `Context`, `ContextTitle` |
| 시설 표시 | `DisplayName`, `CategoryText`, `ZoneText`, `FacilityStateText`, `FootprintText` |
| 가격·정적 사양 | `InstallCostText`, `RecoveryCostText`, `QueueCapacityText`, `ServiceDurationText`, `Thumbnail`, `ThumbnailPlaceholderText` |
| 건축 한도 | `CurrentInstancesInWorld`, `MaxInstancesPerWorld`, `InstanceLimitText`, `bInstanceLimitReached` |
| 운영·Queue | `OperationStateText`, `WaitingPartiesText`, `OccupancyText`, `RemainingServiceText`, `ExpectedWaitText`, `ThroughputText` |
| 유료 프로그램 | `bShowProgramSection`, `bHasScheduledProgram`, `ProgramStatusText`, `ScheduledProgramSummaryText`, `bShowProgramScheduleAction`, `bCanScheduleProgram`, `ProgramScheduleActionText`, `ProgramScheduleDisabledReasonText` |
| 액션·피드백 | `bNeedsRelocation`, `bCanMove`, `bCanRemove`, `bHasMinigame`, `bCanStartMinigame`, `MinigameActionText`, `FeedbackText`, `bFeedbackIsError` |
| 시설 수익 (2026-09-17 추가) | `bShowRevenueSection`, `bFacilityGeneratesRevenue`, `TodayRevenueGold`, `TotalRevenueGold`, `TodayPaymentCount`, `TotalPaymentCount`, `TodayRevenueSharePercent`, `TodayRevenueText`, `TotalRevenueText`, `RevenuePaymentCountText`, `RevenueShareText`, `RevenueStatusText`, `RevenueSummaryText` — **이 설치 시설 한 곳**이 실제로 번 돈. 원본은 `UFF_FestivalProgressionSubsystem::GetFacilityInstanceRevenue(FGuid)`이며 종류별 합계(`GetRevenueByFacilityRowsForDay`)와 다르다. 조명·바닥은 `bShowRevenueSection=false` |

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
- 루트는 `CanvasPanel` + `SelfHitTestInvisible`, 실제 카드는 우측 상단 `(-24, 72)`에 고정된
  `388 × 500`이다. 카드 밖 클릭은 월드로 통과하고 카드 좌클릭과 버튼 입력은 소비한다. 우클릭은 건축
  선택 취소로 전달될 수 있다.
- 다음 핵심 27개 `BindWidget` 이름과, 아래 가산 계약 이름을 유지한다.

```text
Border_Panel, Button_Close, Button_Move, Button_Remove, Image_Thumbnail,
Text_ThumbnailPlaceholder, Panel_BuildSection, Panel_LiveSection, Text_Context,
Text_FacilityName, Text_Category, Text_Zone, Text_FacilityState, Text_RelocationWarning,
Text_Footprint, Text_Clearance, Text_InstallCost, Text_RecoveryCost, Text_QueueCapacity,
Text_ServiceDuration, Text_OperationState, Text_WaitingParties, Text_Occupancy,
Text_RemainingService, Text_ExpectedWait, Text_Throughput, Text_Feedback
```

`Text_Clearance`와 `ClearanceText`는 기존 WBP·Blueprint 호환용으로만 유지한다. 안전 여유 값은 비우고 위젯을 접어 화면에 표시하지 않는다.

```text
Text_InstanceLimit, Button_StartMinigame, Text_StartMinigame,
Panel_ProgramSection, Text_ProgramStatus, Text_ScheduledProgramSummary,
Text_ProgramScheduleReason, Button_ScheduleProgram, Text_ScheduleProgram,
Panel_Actions, Panel_BuildActions, Panel_LiveActions,
Panel_RevenueSection, Text_FacilityTodayRevenue, Text_FacilityTotalRevenue,
Text_FacilityRevenueCount, Text_FacilityRevenueShare, Text_FacilityRevenueStatus,
Text_FacilityRevenueSummary
```

**유료 프로그램 Picker·Card WBP 계약**

- Picker는 종태님 UI와 같은 `LightTeal` 팔레트와
  `/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard`를 사용한다. 루트는 `SelfHitTestInvisible`, 실제 Picker는
  우측 상단 기준 너비 `520`, 우측 여백 `24`, 상단 여백 `72`로 도킹한다.
- Picker의 하단은 Build UI를 위해 **400px를 예약**한다. 이 값은 Build Host `320px` + 상태 바·여백
  `70px` + 안전 간격 `10px`의 합이며, 세로 stretch를 사용해 해상도가 달라도 하단 Build 패널 위에서
  끝나게 한다.
- Picker named widget은 `Border_Picker`, `Button_Close`, `Text_FacilityName`, `Text_Empty`,
  `ScrollBox_Programs`, `Text_PreviewName`, `Text_PreviewDescription`, `Text_PreviewDetails`,
  `Text_PreviewAvailability`, `Text_NoRefundNotice`, `Text_Feedback`, `Button_Confirm`, `Text_Confirm`이다.
- 카드 named widget은 `Button_Select`, `Text_ProgramName`, `Text_ProgramMeta`,
  `Text_ProgramAvailability`이다. 카드 선택은 미리보기만 바꾸고, 명시적 확인 뒤에만
  `UFF_FacilityInfoSubsystem`이 시설 GUID와 편성 가능 여부를 다시 검증해 권위 명령을 실행한다.

에디터 명령 `FF.BuildFacilityInfoUMG`는 시설 정보·유료 Picker·유료 Card WBP가 없을 때 만든다. 기존 시설
정보 패널에는 지원되는 가산 계약만 비파괴 마이그레이션하고, 기존 Picker/Card는 계약을 검증해 재사용한다.
불일치한 기존 Picker/Card는 덮어쓰지 않고 오류로 보고한다. 승인된 스타일을 다시 적용할 때만
유료 WBP만 갱신할 때는 `FF.RefreshFacilityPaidProgramUMG`를 사용한다. 시설 정보 패널까지 함께 갱신할 때는
`FF.RefreshFacilityInfoUMG`를 사용한다. 이 명령은 **시설 정보 패널뿐 아니라 유료 Picker와 유료 Card까지
세 WidgetTree를 모두 LightTeal/Pretendard 템플릿으로 재생성하고 Compile/Save**한다. 실행 전 변경분을
커밋하거나 백업한다.

**시설 수익 카드 (2026-09-17 추가)** — `FF.MigrateFacilityRevenueSection`이 기존 패널의 `Panel_Content`에서
강화 블록 바로 뒤에 `Panel_RevenueSection`(제목 `시설 수익`, 오늘/누적 매출·결제 건수·오늘 매출 비중 stat 카드,
안내 문구 `Text_FacilityRevenueStatus`)을 **한 번만** 추가한다. 이후 색·글꼴·간격·라벨은 Designer에서 수정하고
재실행은 아무것도 바꾸지 않는다. 카드는 `시설 정보`·`운영` 두 페이지에서 보이며 조명·바닥 시설에서는 접힌다.
오늘 매출 숫자의 색은 WBP Class Defaults `Appearance|Revenue`의 두 색(오늘 수익 있음/없음)으로 바꾼다.
`Text_FacilityRevenueSummary`(한 줄 요약)는 선택 사항이라 좁은 레이아웃에서 stat 카드 대신 쓸 수 있다.
`FF.BuildFacilityInfoUMG`도 같은 마이그레이션을 포함하며, 새로 만드는 WBP에는 카드가 기본 포함된다.
갱신은 선택 중 0.25초 샘플과 별개로, 이 시설의 결제가 원장에 들어오면(`OnFacilityRevenueChanged`) 다음 Tick에 앞당겨진다.

**C++ 전용 어댑터 계약**

- `UFF_FacilityInfoPanel`의 `FSimpleMulticastDelegate OnMoveRequested`·`OnRemoveRequested`·
  `OnProgramScheduleRequested`·`OnMinigameRequested`·`OnCloseRequested`는 인자가 없다. 이동·철거·편성은
  현재 ViewData의 Context와 각 `bCan*`를 통과할 때만 요청하며, 닫기는 항상 방송한다.
- `UFF_FacilityInfoSubsystem`의 `FFF_OnFacilityInfoActionRequested(FGuid)` 5종은 패널 이벤트를 현재 선택
  GUID로 변환한다. 유료 프로그램의 실제 Picker 생성·확인·권위 편성은 서브시스템 내부에서 계속 소유한다.
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

**현재 조회 API:** `UFF_QueueSubsystem::GetQueueState(int32 FacilityId)`와
`UFF_ZoneSystemQueueBridgeSubsystem::GetZoneFacilityQueueState(FGuid)`가 있다.
`GetFacilityServiceRatePerMinute(int32 FacilityId)`도 Stage가 아니라 Queue의 API다.
위젯은 두 시스템을 직접 합치지 않고 시설 정보 coordinator의 표시 스냅샷을 사용한다.

**예약 프로그램은 일반 Queue 통계와 별개다.** 예약자는 줄 좌표만 재사용하므로 일반 대기·점유가 0이어도
예약자가 없다는 뜻은 아니다. 프로그램이 편성된 시설은 그날 일반 가입을 받지 않는다.

---

## Crowd · 밀집 — 이정원

### `FFF_CrowdReadModel` — ✅ 실값이 들어옵니다 (2026-08-27 갱신)

- **헤더**: `Public/NPC/Integration/FF_CrowdReadModel.h`
- **읽는 법**: `UFF_CrowdSimulationSubsystem::GetCrowdReadModel()` — const 참조
- **갱신 주기**: **고정 30스텝 = 1초** (`SnapshotIntervalSteps = 30`, `FixedStepSeconds = 1/30`)
- **상태**: ✅ **`PublishCrowdReadModel()` 이 채웁니다.** 스텁 아닙니다

```cpp
FFF_CrowdReadModel {
    float   CellSizeMeters;   // 셀 한 변(m). 지금 2.0
    FVector GridOriginUU;     // 격자 원점(UU) — 셀 → 월드 환산의 기준
    uint32  SnapshotFrame;    // 이 스냅샷을 만든 프레임
    float   SimTimeMinutes;   // Live 경과(축제 분)
    TArray<FFF_CrowdCellSample> OccupiedCells;
}

FFF_CrowdCellSample {
    int32 CellX, CellY;
    int32 Occupancy;        // 현재 인원
    int32 Throughput;       // 직전 구간 통과 인원
    float DensityPerSqm;    // 인/㎡ — Fruin 직접 비교 가능
}
```

**UI 사용처**: **Heat Map 색상 스케일** (2 주의 / 4 위험 / 5 심각), 밀집 지표, Report 드릴다운

> 🔴 **셀은 5m 가 아니라 2m 입니다.** UI 초안이 5m×5m 를 가정하고 있었는데, 실제 값은
> `FFF_CrowdMetrics::AnalysisCellSizeUU = 200UU`(2m, 4㎡)로 확정됐습니다. **숫자를 UI 에
> 다시 적지 말고 `CellSizeMeters` 를 읽으세요** — 격자 크기는 밀도 계산의 분모라
> 값이 갈리면 Heat Map 색이 통째로 틀립니다.

> ⚠️ **`OccupiedCells` 는 사람이 있는 셀만 담습니다.** 인원 0 · 통과 0 인 셀은 배열에서
> 빠집니다. 없는 셀을 "데이터 없음"이 아니라 **0 으로 칠하시면 됩니다** — 격자 전체를
> 채워 오지 않는 것은 의도입니다.

> `FestaFiesta.Crowd.World.ReadModelIsPublishedOnInterval`은 2026-09-02 회귀 테스트에서 통과했다.
> 과거의 원점·발행 주기 실패를 현재 장애로 취급하지 않는다. 실제 Heat Map 배치는
> `GridOriginUU`와 `CellSizeMeters`로 확인하며, 별도의 `Probe.NavMeshBakeStatus` 지형 연결 실패는 남아 있다.

### 기타

`FF_BaseCrowd`, `FF_BaseNPC`, `FF_SpawnActor`, `FF_CrowdMovementComponent`,
`FF_CrowdVisualComponent` — UI 노출 API는 아직 미정.

`UFF_CrowdSimulationSubsystem` 은 위 `GetCrowdReadModel()` 하나만 UI 접점으로 보시면 됩니다.

---

## Utility AI · 경제 · 진행 — 김시리

### `UFF_RequiredFacilitiesSubsystem` — Setup 필수 설치 시설 (2026-09-08 추가)

- **헤더**: `Public/Festival/FF_RequiredFacilitiesSubsystem.h`
- **접근**: `WorldSubsystem`
- **갱신 주기**: 이벤트 (시설 설치·철거 / 축제 정의 교체 / 스태프 배정·해제·고용)
- **상태**: `구현완료`

| 멤버 | 형 | 설명 |
|---|---|---|
| `GetRequiredFacilitiesStatus()` | `FFF_RequiredFacilitiesStatus` | 전체 요구사항 + `bCanProceed` + `BlockingReason` |
| `HasMissingRequiredFacilitiesInCategory(Category)` | `bool` | 카테고리 탭에 느낌표를 붙일지. **설치 부족만** 봅니다(배정 부족은 탭 문제가 아님) |
| `TryGetRequiredFacilityRow(FacilityDefinitionId, OutRow)` | `bool` | 카탈로그 **카드 1장**용 조회 |
| `OnRequiredFacilitiesChanged` | (인자 없음) | 시설 설치·철거·축제 정의 교체·스태프 배정/해제 시 |

`FFF_RequiredFacilityRow` 한 행 — `FacilityDefinitionId` · `DisplayName` · `Category` · `MinimumCount` · `InstalledCount` · `MissingCount` · `RequiredStaffCount` · `AssignedStaffCount` · `MissingStaffCount` · `bDefinitionValid` · `Guidance`.

> **기본 필수 시설은 2종입니다 (2026-10-07, 한광훈).** 선셋 메인공연장 1개(설치만) + **메인안내소 1개(설치 후 스태프 1명 이상 배정)**. 축제 정의의 `Required Setup Facilities` 행마다 `Minimum Assigned Staff`(0이면 설치만)를 둘 수 있습니다. 배정은 자동으로 되지 않으며, 설치만 하고 배정이 남으면 `MissingCount == 0 && MissingStaffCount > 0` 이고 `Guidance` 에 "`+ 배정`" 안내가 들어옵니다. 설치 안내 범례처럼 '설치'만 묻는 곳은 `FFF_RequiredFacilitiesStatus::HasMissingInstallations()` 를 쓰세요.

> **`bDefinitionValid == false` 면 `Category` 도 믿지 마세요.** 저작 오류가 있는 행이라 카테고리가 미확정입니다 — 그 행으로 느낌표를 붙이면 엉뚱한 탭이 빨개집니다. 그 경우 `Guidance` 에 저작 오류 안내가 들어옵니다.

> `Guidance` · `BlockingReason` 은 `FF_RequiredFacilitiesSubsystem.cpp` 의 `LOCTEXT` 입니다. **UI 에서 같은 문장을 다시 만들지 마세요** — 요구사항이 바뀔 때 두 곳이 갈립니다. 문구 수정이 필요하면 말씀 주세요.

#### 종태님께 — 노출은 이미 되고 있습니다. 먼저 확인만 부탁드립니다

**"막힌 이유를 플레이어가 모른다"는 문제는 이미 해결돼 있습니다.** 종태님이 `FF_PrepHudWbpBuilder.cpp` 에 만들어 두신 `Text_CtaReason` 자리에 제가 값만 흘려 넣었습니다.

```cpp
// !! 비활성 흐려짐만으로는 왜 못 누르는지 알 수 없다 !! (AT-011)
// 이유 문구 자리를 버튼 왼쪽에 반드시 둔다. 값이 없으면 코드가 접는다.
```

`Box_Cta` 안에서 **CTA 버튼 바로 왼쪽**이라 버튼을 보면 사유가 같이 보입니다. AT-011 대비를 미리 해두신 덕에 추가 작업이 필요 없었습니다.

| 위치 | 내용 |
|---|---|
| CTA 사유 줄 | 막힌 사유를 넣습니다. **카운트다운 안내보다 우선** — 시간이 지나도 해결되지 않는 조건이라서 |
| 조건 변화 구독 | `OnRequiredFacilitiesChanged` 를 구독해 시설을 놓거나 철거하면 즉시 `RefreshCta()` 가 돕니다 |

**부탁드리는 것은 하나입니다** — PIE 에서 무대를 안 지은 채로 그 줄이 실제로 보이는지 확인해 주세요. 보이면 이 기능의 필수 작업은 끝입니다.

#### 백로그 3건 — 급하지 않습니다

아래는 "이유를 알린다"가 아니라 **"어디서 지어야 하는지 더 빨리 찾게 한다"** 입니다. 사유 문구가 이미 동작하므로 없어도 기능은 성립합니다. 여유 있을 때 판단해 주세요.

| 우선 | 항목 | 성격 | 호출 |
|---|---|---|---|
| 중 | 건축 카탈로그 카테고리 탭 느낌표 | 탐색 단축. 사유 문구는 "메인 무대 1개 필요"라고만 하고 **어느 탭인지는 말하지 않습니다** | `HasMissingRequiredFacilitiesInCategory(Category)` |
| 중 | `00:00` 멈춤 상태 표시 | 오해 방지. 타이머가 0 에 멈춰 있으면 플레이어는 게임이 멈춘 줄 압니다. `GetStageAdvanceBlockingReason()` 이 비어 있지 않은 동안이 "대기 중"입니다 | Stage 절 계약 변경 2번 |
| 하 | 시설 카드 「필수」 뱃지 + `설치 0/1` | 탐색 단축. `Guidance` 는 그대로 써도 되는 완성 문구입니다 | `TryGetRequiredFacilityRow(Id, Out)` |

#### 제가 어디까지 손댔는지

`FF_PrepHudBar.cpp/h` 를 수정했습니다. 기준은 이렇게 잡았습니다.

- **했음** — 이미 있는 위젯에 값 흘리기, 델리게이트 구독
- **안 했음** — 새 시각 요소 추가, 레이아웃·표현 결정

위 백로그 3건이 후자에 해당해서 남겼습니다. 이 선이 불편하시면 조정하겠습니다.

### `UFF_FestivalProgressionSubsystem::OnFestivalDefinitionChanged` (2026-09-08 추가)

- **접근**: `BlueprintAssignable`, 인자 없음
- **상태**: `구현완료`
- **비고**: `StartFestival` 완료 후 발행합니다. **같은 자금으로 다른 축제를 시작해도** 요구사항이 바뀌므로, 금액 델리게이트만 구독하고 있으면 조건 표시가 갱신되지 않습니다.

### `UFF_NpcNeedMetricsSubsystem` — Need 집계

- **헤더**: `Public/NPC/Metrics/FF_NpcNeedMetricsSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **갱신 주기**: **1초(고정 30스텝)마다.** 값이 바뀐 Need만 발행
- **상태**: ✅ **`구현완료`** — 채우는 쪽이 붙어서 실제 값이 나옵니다
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

**집계는 `Live` 단계에서만 돕니다.** 준비·결산 단계에서는 마지막 확정값이 그대로 멈춰 있습니다 —
0으로 떨어지지 않으니 "Live 가 끝나면 지표가 사라진다"고 가정하지 말아 주세요.

**표본은 파티가 아니라 개인 단위입니다.** 4인 가족이면 4표입니다 — 지표 정의가
"이 Need 가 미해소인 **방문객** 비율"이라서입니다.

지금 실제로 움직이는 것은 **`Hunger`·`Restroom`·`Stimulation`** 셋입니다.
`Thirst`·`Fatigue` 는 아직 쌓이지 않게 꺼 두어서 항상 0 입니다(계수만 켜면 되고 코드 변경은 없습니다).

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

### 유료 프로그램 예약 — 읽기 API

- **헤더**: `Public/NPC/Party/FF_ProgramReservationSubsystem.h`
- **접근**: `GameInstanceSubsystem`

| 하는 일 | API |
|---|---|
| 목록 | `GetReservationIds()` + `FindReservation(Id, Out)`. **시작 시각 오름차순 정렬** — 순서가 실행마다 바뀌지 않습니다 |
| 예약 상태 | `FFF_ProgramReservation` 의 `Capacity`·`AssignedGroupIds`·`AssignedHeadcount`. 남은 자리는 `GetRemainingSeats(Id)` |
| 갱신 통지 | `OnReservationChanged(FGuid ReservationId)` — 바뀐 예약의 핸들이 옵니다 |

편성은 아래 "시설 기반 유료 프로그램 편성 UI" 절을 따릅니다. 플레이어용 취소 API는 없습니다.

- 시간 필드(`StartMinutes`·`EndMinutes`)는 **Live 경과분**입니다. 절대 시각으로 표시하시려면
  아래 "프로그램 시각 표시" 절을 봐주세요 — **17 을 직접 더하지 말아 주세요.**
- `AssignedHeadcount`는 **스폰 후 확정한 인원**이지 실제 대기·참여 인원이 아닙니다. 스폰 전에는
  `UnfilledSeats`가 남아 있을 수 있습니다. 판매 카드의 계획 인원은 `Capacity`를 표시합니다.
- `OnReservationChanged`는 예약 등록·배정·삭제 통지입니다. NPC의 복귀·대기·참여 전환 이벤트가 아닙니다.
- 현재 Crowd는 `자유활동 → 복귀 → 대기 → 참여 → 자유활동`으로 전환합니다. 종료 전 개인 퇴장은
  막고 행사 전체 퇴장은 허용합니다. 프로그램 편성 시설은 **그날 예약자 전용**이며 종료 후에도 일반 가입을 받지 않습니다.
- 실제 참여 상태를 UI에 제공하는 Getter·델리게이트는 아직 없습니다. `ProgramStatusText`는
  현재 `오늘 편성됨` 표시일 뿐입니다. 시작·종료 시각만으로 실제 참가 완료를 표시하지 않습니다.
- 참여 중 메쉬 내부 입장·숨김 연출은 미정입니다. 현재는 대기 좌표에서 참여 상태를 유지합니다.

### 시설 기반 유료 프로그램 편성 UI (2026-08-29 구현완료)

Setup·MorningCheck 에서 설치 시설을 선택하고 **그 시설에서 그날 열 프로그램을 고르는 화면**이다.
경제·예약의 원본 상태는 기존 시스템이 소유하고, 시설 정보 coordinator가 화면용 스냅샷과 권위 명령 경계를
연결한다.

실제 진입은 `BuildMode`에서 시설 선택 → 우측 시설 정보 → `유료 프로그램 편성`이다. Picker는
`UFF_FacilityInfoSubsystem`이 `FFF_FacilityPaidProgramPickerViewData`와
`FFF_FacilityPaidProgramCardViewData`로 평탄화한 값만 렌더링한다. 프로그램 카드는
`CanScheduleProgramAtFacility()` 결과로 시설 호환성과 편성 가능 여부를 함께 표시하고, 확인 시
`TryScheduleProgramAtFacility()`로 다시 검증한다. PrepHUD의 전역 `Program` 탭과는 별도인 시설 단위
흐름이다.

PrepHUD에도 `UFF_PrepPanelProgram`이 구현돼 있다. 카드와 호환 시설을 선택한 뒤
ViewModel의 `RequestScheduleProgram`으로 요청한다. 전역 탭이 빈 자리뿐이라는 이전 안내는 더 이상 유효하지 않다.

**한 줄 요약** — 아침에 Gold 를 걸면 **그 프로그램을 좋아하는 손님이 정원만큼 축제에 옵니다.**
프로그램은 "유료 광고"에 가깝습니다 — 체험비도 벌지만, 데려온 손님이 부스에서도 쓰기 때문입니다.

> ⚠️ **2026-09-21 변경 — 프로그램은 이제 인원을 쓰지 않습니다.** 걸리는 자원은 Gold 하나뿐입니다.
> Staff 는 사건 대응 전담 축이 되어 편성과 완전히 분리됐습니다. 아래 표와 화면 예시에서
> `필요 인원`·`가용 인원` 칸을 **전부 빼 주세요.** (자세한 배경은 `### Staff 고용` 절)

> 🔴 **순익은 범위가 아니라 숫자 하나입니다.** 정원이 반드시 채워지므로 "정원을 못 채우면 손해"
> 같은 상황이 없습니다.
>
> 예약 명단 추가와 스폰 후 `ConfirmReservedParty` 연결은 완료됐습니다. 계획 정원·스폰 확정 인원·실제 참여 인원은 서로 다른 값입니다. 화면에서 `AssignedHeadcount`를 실제 참여 인원으로 표시하지 않습니다.

**용어 — 구조체가 셋이라 헷갈립니다**

| | 뭔가 | 수명 |
|---|---|---|
| `FFF_ProgramCard` | **메뉴판 한 줄.** 저작 데이터라 안 변합니다. 지금 두 장(공방·요가) | 축제 내내 고정 |
| `FFF_ScheduledProgram` | **오늘 그 카드를 산 기록.** 카드 id + 예약 핸들 | **하루** |
| `FFF_ProgramReservation` | 그 편성이 만든 **좌석표.** 정원·시각·확정 인원 | 하루 |

"프로그램을 산다"는 것은 **카드를 오늘 하루치로 편성하는 것**입니다. 같은 카드를 하루에 두 번은
못 사고(`AlreadyScheduled`), 다음 날 아침이면 편성이 비워져서 **또 살 수 있습니다.**

⚠️ **플레이어가 시각을 정하는 게 아닙니다.** `StartMinutes` 가 카드에 박혀 있어서 요가를 낮으로
끌어 옮길 수 없습니다 — **"이 시간대의 이 프로그램"을 고르는 화면**입니다.

- **헤더**: `Public/Festival/FF_FestivalProgressionSubsystem.h`
- **접근**: `GameInstanceSubsystem` (다른 경제 값과 같은 곳입니다)

| 하는 일 | API |
|---|---|
| **화면 표시 여부** | `UFF_StageEconomyBridgeSubsystem::CanScheduleProgramNow()` — 🔴 **아래 주의** |
| 목록 | `GetProgramCatalog()` → `TArray<FFF_ProgramCard>` |
| 오늘 편성한 것 | `GetScheduledPrograms()` → `TArray<FFF_ScheduledProgram>`(카드 id + 예약 핸들) |
| 버튼 활성화 | `CanScheduleProgram(ProgramId)` — **상태를 안 바꿉니다.** 매 프레임 부르셔도 됩니다 |
| 편성 | `TryScheduleProgram(ProgramId, FacilityInstanceId)` → `EFF_ProgramPurchaseResult` |
| 갱신 통지 | `OnScheduledProgramsChanged`(인자 없음) — 편성 목록만 다시 그리시면 됩니다. **인원은 안 바뀝니다** |

> 🔴 **단계 판정은 `CanScheduleProgramNow()` 로 해주세요.** `TryScheduleProgram` 은
> **단계를 안 봅니다** — Live 중에 불러도 편성이 됩니다. 피날레의 `CanPurchaseFinaleNow()` 와
> 같은 성격이고, 같은 이유로 **표시용이지 편성을 막지는 않습니다**(돈 규칙과 단계 규칙을 한
> 함수에 섞으면 경제 테스트가 Stage 서브시스템을 세워야 해서요).
>
> **왜 막아야 하나** — Live 30분에 공방(30~60분) 카드를 사면 예약 슬롯은 생기는데 손님이 올
> 시점이 이미 지났습니다. 그러면 **비용만 빠지고 체험비 0** 으로 정산된 뒤 정원 미달 경고가
> 뜹니다. 크래시가 없어서 조용히 손해만 보는 함정입니다.
>
> 허용 단계는 **Setup · MorningCheck** 입니다. ⚠️ 시설 배치(`IsReconfigureAllowed()`)는
> `PromotionPreview` 도 허용하지만 **편성은 안 됩니다** — 두 규칙이 다릅니다. 배치 화면과
> 편성 화면을 같은 조건으로 묶으시면 어긋납니다.

`FFF_ProgramCard` 가 화면에 필요한 값을 다 갖고 있습니다:

| 필드 | 화면에서 |
|---|---|
| `DisplayName` | 카드 이름. **`FText` 라 그대로 쓰시면 됩니다**(enum 이 아니라 패키징 함정 없음) |
| `CostGold` | 비용. **편성에 드는 유일한 자원입니다** |
| `Capacity` · `FeePerPerson` | 정원 · 1인당 체험비 |
| `CalcMaxRevenue()` | 체험비 매출 = `Capacity × FeePerPerson`. **이름은 `Max` 지만 정원이 보장되므로 확정값으로 쓰시면 됩니다** (이름은 배선이 끝날 때 정리하겠습니다) |
| `StartMinutes` · `EndMinutes` | **Live 경과분**입니다. 표시 방법은 아래 "프로그램 시각 표시" 절 |

**화면 예시** — 리스트 한 줄에 이 정도면 판단이 됩니다:

```
바다향기 공방   30~60분   정원 12 · 체험비 30   비용 200   [편성]
   순익 +160      손님 12명이 축제에 옵니다

선셋 요가       90~120분  정원 16 · 체험비 25   비용 250   [편성]
   순익 +150      손님 16명이 축제에 옵니다
```

순익은 `CalcMaxRevenue() - CostGold` 하나입니다.

**"손님 N명이 옵니다" 줄이 이 화면의 핵심입니다.** 체험비 순익만 보면 두 카드가 +160/+150 로
비슷해 보이는데, 실제 값어치는 **데려오는 인원**에서 갈립니다 — 그 손님들이 부스에서도 쓰고
그 부스 매출이 따로 들어오기 때문입니다. `Capacity` 를 그대로 쓰시면 됩니다.

**실패 이유는 한글로 드립니다** —
`UFF_EconomyDisplayTextLibrary::GetProgramPurchaseResultText(Result)`.
`없는 프로그램` / `오늘 이미 편성했다` / `Gold 부족` /
`축제가 시작되지 않았다`. ⚠️ enum 의 `DisplayName` 을 직접 읽으면 패키징 빌드에서 영문이 뜹니다.

**⚠️ 시설 핸들이 필요합니다.** `TryScheduleProgram` 의 두 번째 인자는 ZoneSystem 의
`FacilityInstanceId` 입니다. 프로그램이 어느 시설에서 열리는지가 정해져야 NPC 가
거기로 이동하고, 잠금·퇴장 금지·도착 판정이 그 시설 기준으로 걸립니다.
플레이어가 시설을 고르는 흐름이 필요하고, **무효한 핸들을 넘기면 편성이 거절됩니다**
(Gold 는 안 빠집니다).

**취소 버튼은 만들지 않았습니다.** 편성은 하루짜리라 다음 날 아침이면 자동으로 비워집니다.
되돌리기를 넣으면 "샀다 취소했다"로 인원 점유만 흔들리고 환불 규칙이 또 필요해집니다.
필요하시면 말씀해 주세요 — 규칙(전액/부분 환불)을 정해야 합니다.

#### 프로그램 시각 표시 — Stage 변환 함수를 쓰세요

카드의 `StartMinutes`·`EndMinutes` 는 **Live 경과분**입니다(NPC 의 욕구·체류·잠금 판정이
쓰는 것과 같은 축이라, 플레이어가 보는 시각과 NPC 가 느끼는 시각이 어긋나지 않습니다).

**게임 시계는 이미 있습니다** — `1 Live초 = 1 게임분`, **개장 17:00**. Live HUD 의
`GetLiveClockText()` 가 그걸 그려서 화면에 `17:30` 이 이미 떠 있습니다. 그래서 카드 값도
같은 시각으로 보여야 합니다:

| 카드 | Live 경과분 | 보여야 하는 시각 |
|---|---|---|
| 바다향기 공방 | 30~60 | **17:30 ~ 18:00** |
| 선셋 요가 | 90~120 | **18:30 ~ 19:00** |

`GetLiveClockStringFromMinutes(int32 LiveMinutes)`(강민서)에 `StartMinutes`·`EndMinutes` 를
각각 넘기면 절대 시각이 나옵니다.

- ⛔ UI에 `17`을 직접 더하지 않습니다. 개장 시각의 원본은 Stage 한 곳에만 둡니다
- 카드 값은 `float`지만 현재 저작값은 정수 분입니다. 호출 시 정수 변환 정책은 UI 한 곳에서 통일합니다

> 참고로 Live 총 길이가 날마다 달라서 **폐장 시각도 다릅니다** — Day1 은 21:00,
> Day2·3 은 22:00 입니다. 카드 시각은 세 날 모두 같습니다(가장 짧은 날 안에 들어가는 것을
> 테스트로 고정해 뒀습니다).

#### 인원 표시는 `GetTotalStaffCount()` 하나면 됩니다 (2026-09-21 정정)

> ⚠️ **2026-08-25 에 드린 안내를 뒤집습니다.** 그때는 "프로그램이 인원을 묶으니
> `GetAvailableStaffCount()` 를 띄워 달라"고 했는데, **프로그램이 인원을 안 쓰게 바뀌었습니다.**
> 그 함수와 `GetStaffAssignedToPrograms()` 는 **없어졌습니다.**

| 값 | 뜻 |
|---|---|
| `GetTotalStaffCount()` | 가진 전체 인원 (기본 + 고용) |
| `GetBaseStaffCount()` · `GetHiredStaffCount()` | 내역이 필요하면 |

**전체 인원이 곧 사건에 쓸 수 있는 인원**입니다. `6명 중 3명 사용 가능` 은 `6명` 으로
줄여 주세요 — 나눠서 보여줄 근거가 없어졌습니다.
갱신은 `OnTotalStaffCountChanged` 하나만 받으시면 됩니다. `OnScheduledProgramsChanged` 는
**인원과 무관**하므로 인원 갱신용으로 구독하지 마세요(아무것도 안 바뀐 화면을 다시 그립니다).

#### 체험비는 프로그램이 **열리는 시각**에 들어옵니다

카드마다 열리는 시각이 다릅니다(공방 30분 · 요가 90분). 그 시각이 되면 그 카드 몫의 체험비가
**그때** 원장에 들어갑니다 — 부스 매출과 같은 리듬입니다. Closing 이 아닙니다.

`GetEarnRowsForDay(Day)` 의 **`유료 프로그램 체험비`** 줄이 그것입니다. 줄의 내용은 그대로이고
**값이 채워지는 시점만** 다릅니다 — Live 중에 두 번 올라갑니다(공방 열릴 때, 요가 열릴 때).

- **마감 화면만 보시는 거면 하실 일이 없습니다.** 마감 시점의 합계는 같습니다
- **Live 중 갱신을 반영하실 거면** `OnGoldChanged` / `OnTodayRevenueChanged` 를 구독하세요
- 마감 화면에는 **`유료 프로그램 체험비` 줄 하나면 됩니다.** `공방 8/12명 (빈자리 4)` 같은
  빈자리 표시는 정원이 보장되므로 생길 상황이 없습니다

### 경제 값 4종 — ViewModel 연결 (2026-09-21 갱신)

- **헤더**: `Public/Festival/FF_FestivalProgressionSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **상태**: 원본 API 전부 `구현완료`. 잔액·인원은 연결됐고 매출 2종은 미연결

| 값 | Getter | 델리게이트 | 단위·범위 | 연결 |
|---|---|---|---|---|
| 잔액 | `GetGold()` | `OnGoldChanged` | Gold, 0 이상·상한 없음 | ✅ |
| **누적 매출** | `GetTotalRevenue()` | `OnTotalRevenueChanged` | Gold, **줄어들지 않음** | 🔴 **아직 더미** — ViewModel 이 값을 들고만 있고 원본을 안 구독합니다 |
| **오늘 매출** | `GetTodayRevenue()` | `OnTodayRevenueChanged` | Gold, 매일 아침 0 | 🔴 **아직 더미** — 위와 같음 |
| **전체 인원** | `GetTotalStaffCount()` | `OnTotalStaffCountChanged` | 명, 상한은 Definition 기준 | ✅ ViewModel 중계, PrepHUD·LiveHUD 구독 |

> ⚠️ **2026-09-21 — 없어진 값 2종**
>
> | 없어진 것 | 대신 |
> |---|---|
> | `GetTotalArrears()` · `OnTotalArrearsChanged` (밀린 임대료) | **없습니다.** 임대료가 사라져 미납이라는 개념 자체가 없어졌습니다 |
> | `GetAvailableStaffCount()` · `OnAvailableStaffCountChanged` | **`GetTotalStaffCount()`** 하나만 쓰시면 됩니다 |
>
> `RefreshAvailableStaffCount()` 도 없어졌습니다. 프로그램이 인원을 묶지 않으므로
> **전체 인원이 곧 쓸 수 있는 인원**이고, 두 숫자를 나눠 보여줄 이유가 없습니다.
> `전체 6명 중 4명 사용 가능` 같은 표시는 `6명` 으로 줄여 주세요.

원본이 준비되지 않은 ViewModel 인원 값은 `INDEX_NONE`으로 구분합니다.

**남은 연결 작업 — 매출 2종**

Gold 와 **같은 모양**이라 `HandleOwnerGoldChanged` 를 복사하시면 됩니다.

1. `Initialize` 에서 `OnTotalRevenueChanged`·`OnTodayRevenueChanged` 를 `AddUniqueDynamic`,
   `Deinitialize` 에서 `RemoveDynamic`.
2. 바인딩 직후 Getter 로 초기값 1회 동기화 — 위젯이 늦게 생겨도 초기값을 놓치지 않게.
3. `ApplyIntMetric` 을 그대로 쓰시면 **값이 실제로 바뀔 때만** 발행됩니다.
4. 붙이고 나면 매출 2종도 0.1초 폴링 대상에서 빼실 수 있습니다.

구매 버튼은 `CanAfford(int32 CostGold)` 를 그대로 중계해 주세요 — 위젯이 잔액과 가격을
직접 비교하지 않습니다.

> 연결 여부는 현재 호출부 기준입니다. 헤더의 오래된 더미 주석보다 실제 구독·갱신 경로를 우선합니다.

- 인원은 **기본 팀(`GetBaseStaffCount()`, 무페이·해고 불가) + 고용한 인원(`GetHiredStaffCount()`)** 로
  쪼개서 보여주실 수 있습니다. 두 숫자 다 데이터 애셋 값이라 밸런싱으로 바뀝니다.
  **고용한 인원은 축제가 끝날 때까지 남습니다** — 하루 경계에서 줄지 않습니다
- **오늘 매출을 누적 매출에서 빼서 만들지 마세요.** 따로 들고 있는 값입니다 — 하루 경계에서 오늘 매출만 0이 되고
  누적은 남기 때문에 빼서 만들면 날짜가 바뀔 때마다 어긋납니다

### 🔴 화면 만들 때 알아야 하는 것 3가지

> ⚠️ **2026-09-21 — 5가지에서 3가지로 줄었습니다.** 임대료와 미납이 없어지면서
> 4·5번(밀린 임대료 표시, 매출이 빚부터 갚는 규칙)이 통째로 사라졌습니다.
> **미납 관련 UI 는 전부 빼 주세요** — 띄울 값이 없습니다.

**1. "다음 날" 버튼은 이제 돈을 걷지 않습니다.**
`RequestAdvanceToNextDay()`가 하는 일은 **오늘 매출을 0으로 되돌리고 원장을 넘기는 것**뿐입니다.
임대료가 없어졌고 고용은 1회 구매라 **하루 경계에서 잔액이 줄어드는 경로가 없습니다.**
인원도 안 줄어듭니다 — 고용한 사람은 축제가 끝날 때까지 남습니다.

**2. 잔액을 UI가 직접 계산하지 말아 주세요.** 델리게이트가 주는 값을 그리시면 항상 맞습니다.

> **Gold 는 음수가 되지 않습니다.** 잔액이 모자라면 그 지출은 **거부되고 상태가 그대로**입니다 —
> 부분 결제나 외상이 없습니다. 화면에서는 **버튼이 막히는 것**으로 나타나면 됩니다.
> 거절할 수 없는 지출이 하나도 없어서 빚이 생길 자리가 없습니다.

**3. 🔴 Day1 셋업 예산은 초기 자금 그대로입니다.**

`DT_DaySequence` 에 `Day1_MorningCheck` 행이 없습니다 — Day1만 개장 준비를 건너뜁니다(팀 의도).
예전에는 이것 때문에 Day1 임대료를 축제 시작과 함께 걷었지만, **임대료 자체가 없어져서 그 특례도
사라졌습니다.** 단계 흐름만 기억하시면 됩니다:

| Day | 단계 흐름 |
|---|---|
| **Day1** | `Setup`(420초) → `PromotionPreview`(60초) → `SunsetTransition`(20초) → `Live` → `Closing` → `Report` |
| Day2 | `MorningCheck`(120초) → `SunsetTransition` → `Live` → `Closing` → `Report` |
| Day3 | `MorningCheck` → `SunsetTransition` → `Live` → `Finale` → `Closing` → `Report` |

`Live` 앞에 `SunsetTransition`(20초)이 있는 것도 표 기준이니 프로토타입 단계 순서도 같이
확인해 주세요.

**Day1 고용은 `Setup` 화면에서 하시면 됩니다** — Day1 에는 `MorningCheck` 가 없습니다.

⚠️ 헤더에 `BeginNewDay()`가 보이는데 **UI에서 부르지 말아 주세요.** 날짜를 넘기는 쪽이 둘이 되면
원장 스냅샷이 한 칸 어긋나 결산 표가 엉뚱한 날 값을 그립니다. 부르는 곳은 경제 쪽에서 한 곳으로
잡아 두었습니다.

### 셋업 예산 — 초기 자금이 그대로 예산입니다

> ⚠️ **2026-09-21 — 이 절이 통째로 단순해졌습니다.** 예전에는 `초기 자금 10,000 −
> 장소 임대료 2,400 = 사용 가능 7,600` 세 줄짜리 표가 필요했는데, **임대료가 없어져서
> 초기 자금이 곧 사용 가능 금액**입니다. 세 줄 표는 빼 주세요.

```
사용 가능        10,000
```

`GetGold()` 하나면 되고 이미 연결돼 있습니다. `GetDefinition()->InitialGold` 와 같은 값으로
시작합니다.

> 예전에 안내드린 "Day1 은 `OnGoldChanged` 가 안 온다"는 제약도 없어졌습니다 —
> `StartFestival` 안에서 걷어가는 것이 이제 없기 때문입니다.

### 🔴 피날레 구매 화면이 필요합니다 (담당 백종태)

Day3 아침(MorningCheck)에 피날레 등급을 사는 화면입니다. 경제 쪽은 다 됐습니다.

> **2026-08-27 — 자리는 이미 잡혀 있습니다.** `EFF_PrepHudTab::Finale`("피날레") 탭이 있고,
> `FFF_PrepHudRules::IsTabVisible` 이 **마지막 날 개장 준비에서만** 보이도록 이미 막고
> 있습니다(`bIsFinalDay`). 탭 노출 조건과 `CanPurchaseFinaleNow()` 가 같은 판정이라
> 패널 쪽에서 다시 판정하실 필요 없습니다 — **패널 본체만 없습니다.**

- **버튼 활성화**: `UFF_StageEconomyBridgeSubsystem::CanPurchaseFinaleNow()` — Day3 MorningCheck이고
  아직 안 샀을 때만 `true`. **표시용이고 구매를 막지는 않습니다**(`CanSkipCurrentStage`와 같은 성격)
- **목록**: 등급·비용·보너스는 `GetDefinition()->FinaleTiers`에 있습니다.
  C/B/A/S = 2,000/4,000/6,000/8,000 Gold, 만족도 +3/+6/+10/+15(2026-10-09 변경, 전 +5/+12/+20/+30). 숫자는 밸런싱으로 또 바뀌니 UI 에 복제하지 말고 `GetFinaleTiers()` 로 읽어 주세요
- **구매**: `TryPurchaseFinale(EFF_FinaleGrade)` → `EFF_FinalePurchaseResult`.
  실패 이유 한글은 **`UFF_EconomyDisplayTextLibrary::GetFinalePurchaseResultText(Result)`** 로
  뽑아 주세요 (`Gold 부족` / `이미 구매했다` 등). ⚠️ enum 의 `DisplayName` 을 직접 읽으면
  패키징 빌드에서 영문이 뜹니다 — 아래 절 참고
- **구매 결과**: `GetPurchasedFinaleGrade()` + `OnFinaleGradeChanged(EFF_FinaleGrade)`. 안 샀으면 `None`

⚠️ **구매는 축제당 1회이고 되돌릴 수 없습니다.** 등급 변경·취소·환불이 없으니 **확인창을 넣어 주세요.**
두 번째 시도는 `이미 구매했다`로 거부됩니다.

**만족도 보너스는 Gold가 아닙니다.** `GetFinaleSatisfactionBonus()`가 0~100 척도의 가산점(A면 10)을
돌려주는데, 매출에도 잔액에도 섞이지 않습니다. 최종 평가 점수(`ScoreSatisfaction` = 관객 만족도 + 이 보너스)의
합산은 관객 만족도가 아직 없어서 붙이지 않았습니다 — 지금은 "몇 점짜리를 샀는가"까지만 보여드릴 수 있습니다.

잔액이 모자라면 자연히 못 삽니다. 이때 실패 이유는 `Gold 부족`으로 옵니다.

### Staff 고용 — 영구 고용입니다 (2026-09-21 개편)

> ⚠️ **일당 알바가 아닙니다.** 한 번 고용하면 **축제가 끝날 때까지 남습니다.**
> 하루 경계에서 줄어들지 않고, 매일 다시 뽑을 필요가 없습니다.
> 화면 문구를 `알바` → `고용` 으로 바꿔 주세요.

**Staff 는 무엇에 쓰나** — 앞으로 붙을 **사건 대응 전담**입니다.

| 축 | 담당 |
|---|---|
| **버는 축** | 시설 강화 (회전율·객단가를 올립니다) |
| **지키는 축** | Staff (사건을 처리합니다) |

- **유료 프로그램은 Staff 를 쓰지 않습니다.** 편성에 걸리는 자원은 Gold 하나뿐입니다
- **시설 성능에도 관여하지 않습니다.** 회전율·객단가는 시설 강화가 올립니다
- 사건 시스템(`UFF_FestivalEventSubsystem`, 담당 이정원)이 오기 전까지 **Staff 는 실제 소비처가
  없습니다.** 인원 숫자는 정확하지만 아직 게임에 영향을 주지 않는 상태입니다

표시 단계는 **Day1 `Setup` 과 Day2·3 `MorningCheck`** 입니다. **Live 중에는 고용할 수 없습니다** —
버튼을 두지 마시고 대신 언제 뽑을 수 있는지 문구로 적어 주세요.

| 필요한 것 | 함수 | 무엇을 보는가 |
|---|---|---|
| 고용 실행 | `TryHireStaff(int32 인원)` | — |
| 스텝퍼 최대치·버튼 판정 | `GetAffordableHireCount()` | 상한 **과** 잔액 |
| 상한까지 남은 인원 | `GetRemainingHireCap()` | 상한만 (잔액 무시) |
| 전체 인원 | `GetTotalStaffCount()` | 기본 + 고용 |
| 내역 | `GetBaseStaffCount()` · `GetHiredStaffCount()` | 무페이 기본 팀 / 고용한 인원 |
| 1명당 단가 | `GetStaffHireCostGold()` | **1회 구매 비용**입니다. `/일` 이 아닙니다 |
| 오늘 지불한 고용비 | `ViewModel.GetTodayStaffHireCostGold()` | 현재 일자의 원장에 기록된 `StaffHire` 지출입니다. 점검 패널의 `오늘 견적`에 사용하며 전날 고용비를 다시 포함하지 않습니다. 단가 변경으로 과거 지출을 재계산하지 않으며, 축제 시작 전에는 `INDEX_NONE`입니다 |

조회 함수는 상태를 바꾸지 않습니다. `TryHireStaff` 만 비용·인원을 바꾸는 Command입니다.

⚠️ **이름이 바뀐 API 3종** — 옛 이름은 더 이상 없습니다.

| 예전 | 지금 |
|---|---|
| `TryHireTemporaryStaff` | `TryHireStaff` |
| `GetAffordableTemporaryStaffCount` | `GetAffordableHireCount` |
| `GetTemporaryStaffCount` | `GetHiredStaffCount` |

⚠️ **부분 고용은 없습니다** — 5명 요청에 3명분 돈만 있으면 **전부 거부**되고 아무것도 바뀌지 않습니다.

**상한은 축제 전체 총원 상한입니다.** `MaxHiredStaffCount`(기본 6) 는 **하루 상한이 아닙니다** —
축제 내내 고용할 수 있는 총 인원입니다. 하루 상한으로 읽으시면 "내일 또 뽑을 수 있다"는 안내가
나가는데 사실이 아닙니다.

> 화면의 `최대 N명` 은 **기본 팀 + 고용 상한**으로 보여 주세요. 상한(6)만 쓰면 `최대 6명` 이
> 되어 이미 있는 기본 팀이 화면에서 사라집니다. → `최대 7명 (기본 1 + 고용 6)`
>
> **2026-10-09 — 기본 팀이 6명에서 1명이 됐습니다**(`InitialStaffCount` 1). 메인안내소 필수 배정 1명만 채우고,
> 사건 대응 인력은 고용(450 Gold)으로 확보합니다. 6명이면 고용 없이도 사건이 다 풀려 고용이 선택지가 되지 않았습니다.

**"왜 더 못 뽑는가"를 문구로 가르는 법** — 거부 사유가 `bool` 하나뿐이라 두 값을 비교하는 것이
유일한 구분 수단입니다:

```
GetRemainingHireCap() == 0                        →  "고용 상한에 도달했습니다"
GetAffordableHireCount() < GetRemainingHireCap()  →  "Gold 가 부족합니다"
```

두 문구는 플레이어가 할 행동이 다릅니다. 앞은 **이번 축제에서는 무엇을 해도 안 되고**, 뒤는
돈을 벌거나 다른 지출을 줄이면 됩니다. 버튼을 흐리게만 두면 구분이 안 됩니다 (AT-011).

⚠️ **Definition 이 아직 안 붙은 상태에서 두 창구의 값이 갈립니다.**

| 부르는 곳 | 그때 값 |
|---|---|
| 경제 직접 (`GetRemainingHireCap()`) | `0` |
| ViewModel (`GetRemainingHireCap()`) | `INDEX_NONE` = 모름 |

이 화면에서 `0` 은 이미 **"상한 도달"** 이라는 뜻을 갖고 있어서 겹칩니다.
**ViewModel 쪽을 쓰시는 편이 안전합니다.**

> ✅ **2026-08-26 백종태 님 요청 해결** — `GetRemainingHireCap()` 을 `public` +
> `BlueprintPure` 로 열었습니다. UI 에서 같은 식을 다시 쓰실 필요 없습니다.

### 스태프 강화(승급) — 규칙 확정, API 완성 (2026-09-22 🆕)

> 설계 전체는 `staff_event_design.md`(Git 제외, 로컬 전용) 참고. 여기는 **UI 가 바로 쓸 수
> 있는 것**만 적습니다. 사건 시스템(이정원)이 아직 없어서 **승급 버튼을 눌러도 게임 안에서
> 체감되는 효과는 아직 없습니다** — API 는 끝났지만 "왜 눌러야 하는가"의 답은 다음 커밋에
> 옵니다. 지금 화면을 만드셔도 나중에 API 가 바뀌어서 다시 갈아엎을 일은 없습니다.

**규칙 요약** — Staff 는 개인별로 Lv1~3 을 갖습니다. 고용 직후엔 항상 Lv1이고, **고용한
다음 날 아침부터** 승급할 수 있습니다(당일은 불가). 한 아침에 한 사람당 한 단계만 오릅니다.
→ Day1 에 고용하면 최대 Lv3 까지, Day3 에 고용하면 Lv1 에 머뭅니다 — 이게 "일찍 고용할수록
이득"이라는 서사를 만듭니다.

**개인별 명단 조회**

```cpp
UFUNCTION(BlueprintPure) TArray<FFF_StaffMember> GetStaffRoster() const;
```

`FFF_StaffMember` 필드: `StaffId`(int32) · `TrainingLevel`(1~3) · `HiredOnDay` ·
`LastPromotedDay`(승급한 적 없으면 `INDEX_NONE`) · `bIsBaseStaff`(무페이 기본 팀 여부).
기본 팀(현재 1명)도 이 배열에 같이 들어 있습니다 — 기본 팀도 승급 가능합니다(Day1 부터).

**승급 실행**

```cpp
UFUNCTION(BlueprintCallable) bool TryPromoteStaff(int32 StaffId);
```

거부되면(고용 당일 / 오늘 이미 승급함 / 이미 Lv3) `false`만 돌아오고 상태는 안 바뀝니다.
세분화된 거부 사유 enum은 없습니다 — 필요하시면 요청해 주세요.

**갱신 알림**

```cpp
UPROPERTY(BlueprintAssignable) FFF_OnStaffRosterChanged OnStaffRosterChanged;
```

고용·승급 둘 다 이 델리게이트 하나로 옵니다. 기존 `OnTotalStaffCountChanged`는 **총원 수만**
바뀔 때 오므로(승급은 인원수가 안 바뀝니다), 개인별 목록 화면은 반드시 이 델리게이트를
따로 구독해야 화면이 안 낡습니다.

**화면 만드실 때 참고**

- `필요 인원 3명` 같은 옛 프로그램 편성 칸과 헷갈리지 않게, 이건 "누구를 몇 레벨로 키울지"
  고르는 화면입니다 — 최대 7행(기본 1 + 고용 6), 각 행에 레벨·승급 버튼.
- `오늘 고용됨 — 내일부터 승급 가능` 문구는 `CurrentDay <= HiredOnDay`로 직접 판정하시거나,
  `TryPromoteStaff` 호출 결과(`false`)로 버튼만 비활성화하는 두 방식 다 가능합니다. 서버
  쪽에 "왜 안 되는지" API가 따로 없어서, 세분화된 문구가 필요하면 요청해 주세요.
- 사건 트레이(어느 사건에 누가 배치됐는가)는 **이번 범위 밖**입니다 — 아래 B표·§6 참고.

**시설 철거 환급은 매출이 아닙니다.** 설치비 50%가 돌아오면서 `OnGoldChanged`는 오지만
`OnTotalRevenueChanged`는 오지 않습니다. "잔액 상승 = 매출 상승"으로 가정한 코드가 있으면 확인해 주세요.

아직 없는 경제 값은 **일차 결과(등급 산출)와 저장 API**입니다.

### 결산 표 4종

- **헤더**: `Public/Festival/FF_FestivalProgressionSubsystem.h`
- **접근**: `GameInstanceSubsystem`. 전부 `BlueprintPure`

```cpp
bool                            HasLedgerForDay(int32 Day) const;
TArray<FFF_LedgerRow>           GetEarnRowsForDay(int32 Day) const;
TArray<FFF_LedgerRow>           GetSpendRowsForDay(int32 Day) const;
TArray<FFF_FacilityRevenueRow>  GetRevenueByFacilityRowsForDay(int32 Day) const;
int32                           GetTotalRefundForDay(int32 Day) const;
```

그리려는 날짜를 넘기시면 그 날 표가 옵니다.

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

- 원인별 표는 `GetEarnRowsForDay()` → `GetSpendRowsForDay()` 순으로 붙이시면 됩니다
- 값 0 인 사유는 줄이 없습니다. 순서는 enum 선언 순서로 고정입니다
- 부스별은 `FacilityId` 오름차순으로 드립니다
- 환급은 `GetTotalRefundForDay()` 로 분리했습니다. 매출 표에 안 섞입니다
- 합계 줄은 따로 안 드립니다 — 0 인 줄이 빠져 있어서 `Amount` 를 더하면 정확합니다

**부스별만 구조체가 다릅니다.**

```cpp
struct FFF_FacilityRevenueRow { FName FacilityId; int32 Amount; };   // DisplayName 없음
```

경제는 시설의 한글 이름을 모릅니다. **표시 이름은 Building 카탈로그에서 해석해 주세요.**

부스별 표는 **실제 값으로 찹니다.** NPC 파티가 부스 이용을 마치면(`EFF_QueueExit::Served`)
부스 매출이 원장에 들어갑니다 — 화면에 `잠정`·`더미` 배지가 남아 있으면 떼셔도 됩니다.

**`FacilityId` 는 `FacilityDefinitionId`(`Booth_FoodStall_Grill` 형태)입니다.** Building
카탈로그의 키와 같으므로 표시 이름을 그대로 해석하실 수 있습니다.

⚠️ **같은 정의의 부스를 여러 개 지으면 한 줄로 합쳐집니다.** "1번 음식 부스 vs 2번 음식 부스"
비교는 안 됩니다 — 인스턴스별 구분은 v1 입니다. 지금 표가 답하는 질문은
"어떤 **종류**가 잘 버는가" 입니다.

> ✅ **2026-09-17: 인스턴스별 값은 시설 정보 패널용으로 따로 생겼습니다.** 이 표(종류별 합계)는 그대로이고,
> 설치 시설 한 곳의 오늘/누적 매출·결제 건수는 `GetFacilityInstanceRevenue(FGuid)` 로 읽습니다 — 바로 아래 항목.

- 무료 시설(휴식·포토존·화장실·공연)은 애초에 줄이 생기지 않습니다. 공연은 직접 수익이 없고
  사람을 모아 주변 부스를 먹여살리는 집객 장치입니다(`economy_spec.md` §2.4)
- 대기 포기·취소로 빠진 파티는 매출을 만들지 않습니다

### 설치 시설별 매출 — `GetFacilityInstanceRevenue` (2026-09-17 추가, 한광훈)

- **헤더**: `Public/Festival/FF_FestivalProgressionSubsystem.h`, `Public/Economy/FF_FestivalEconomyTypes.h`
- **접근**: `GameInstanceSubsystem`
- **시그니처**: `bool GetFacilityInstanceRevenue(FGuid FacilityInstanceId, FFF_FacilityInstanceRevenue& OutRevenue) const` (`BlueprintPure`)
- **갱신 주기**: 이벤트 — `OnFacilityRevenueChanged(FGuid FacilityInstanceId)`. 결제가 원장에 들어갈 때마다 해당 시설 GUID 로 발행
- **상태**: `구현완료`
- **비고**: 부스별 표가 **종류**(`FacilityDefinitionId`) 합계인 것과 달리 **설치 시설 한 곳**(Zone 인스턴스 GUID)의 값이다.
  `FFF_FacilityInstanceRevenue { TodayGold, TotalGold, TodayPaymentCount, TotalPaymentCount }`.
  기록이 없으면 `false` + 전부 0 ("아직 결제 없음"). 오늘 값은 하루 경계에서 0, 누적은 새 축제에서만 0.
  부스 매출(`Served` 파티 1회 = 1건)과 유료 프로그램 체험비(프로그램 1회 = 1건)가 들어가고, 입장료·보너스처럼
  시설 핸들이 없는 매출은 들어가지 않는다. 자금·원장과 **같은 호출**(`AddFacilityRevenue(Amount, Reason, FacilityId, FacilityInstanceId)`)에서
  쌓이므로 결산 표와 합이 어긋나지 않는다. 경제 쪽 변경은 가산뿐이다 — `AddRevenue` 시그니처·기존 표 4종은 그대로다
  (김시리 님 확인 부탁드립니다). 사용처: 설치 시설 공용 정보 패널의 `시설 수익` 카드.

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

경제 쪽 표시(피날레 실패 이유·등급·결산 표 사유)는 **전부 이 함수로 나갑니다.** 그대로 쓰시면 됩니다.

#### 종료된 것

- **강민서 (2026-08-27)** — `UFF_FestivalStageSubsystem::GetCurrentStageDisplayName()` 이
  `GetDisplayNameTextByValue()` 를 쓰고 있던 것을 하드코딩 switch 로 교체, 빌드 확인.
  8/26 에 "이 함수는 패키징 안전"이라고 안내한 것을 정정한 결과입니다.
  → **이제 Stage 이름의 안전한 원본은 이 함수 하나입니다.**

#### 표시 문자열 연결 상태 (2026-09-02 확인)

- `FF_FestivalViewModelSubsystem::GetCurrentStageDisplayText`, Live HUD 탭, PrepHUD 탭은
  `UFF_UIDisplayTextLibrary`를 사용합니다. 해당 3곳의 런타임 메타데이터 조회는 해소됐습니다.
- 아직 `UEnum::GetDisplayValueAsText`를 쓰는 곳은 `UI/Frontend/FF_OptionsScreen.cpp`의
  카테고리 표시와 `UI/Core/FF_UIFlowStubScreen.cpp`의 개발용 화면 제목입니다.
- `EFF_PromotionPurchaseResult` 전용 표시 함수는 아직 없습니다. 현재 ViewModel 구매 응답은
  `bool`이며 실패 enum을 위젯으로 중계하지 않습니다. 상세 실패 문구 연결은 별도 작업입니다.

패키징용 문자열은 enum 메타데이터가 아니라 표시 라이브러리나 현지화 데이터에서 가져옵니다.

---

## UI Read Model — 백종태

### `UFF_FestivalViewModelSubsystem`

- **헤더**: `Public/UI/ViewModel/FF_FestivalViewModelSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **상태**: **부분 연동**. Stage·시간·Gold·스태프·프로그램·홍보 구매 상태는 연결됐습니다. 만족도·안전·매출은 아래 표 참조
- **역할**: 위 모든 소스를 UI가 쓰기 좋은 형태로 합쳐서 제공

Overview HUD·Report·공통 지표 위젯은 **이 서브시스템만 참조**하고 원본 시스템을 직접 보지 않습니다.
각 모듈이 바뀌어도 위젯을 안 고치게 하기 위해서입니다. 설치 시설 정보 WBP는 별도 예외 경로로,
`UFF_FacilityInfoSubsystem`이 주입한 `FFF_FacilityInfoViewData`만 렌더링하며 원본 시스템은 직접 조회하지 않습니다.

| 값                 | Getter                                | 델리게이트                        | 현재        |
| ----------------- | ------------------------------------- | ---------------------------- | --------- |
| SimSatisfaction   | `GetSimSatisfaction()`                | `OnSimSatisfactionChanged`   | **원본 연결 완료** — 대기·밀도·취향매칭·Need 충족도 4항의 2단 계층 산출. 첫 확정 전엔 중립값 50 |
| SimSatisfaction 분해 4종 | `GetSimSatisfactionWaitScore()` / `GetSimSatisfactionDensityScore()` / `GetSimSatisfactionPreferenceScore()` / `GetSimSatisfactionNeedFulfillmentScore()` | `OnSimSatisfactionWaitScoreChanged` 등 (이름은 각 Getter + `Changed`) | **원본 연결 완료**. SimSatisfaction과 같은 순간(1초 주기)에 같이 갱신됩니다. "지금 만족도가 왜 이 숫자인지" 분해해 보여주는 용도라 필수는 아니고 화면에 넣으면 원인 진단에 도움이 됩니다 |
| ScoreSatisfaction | `GetScoreSatisfaction()`              | `OnScoreSatisfactionChanged` | SimSatisfaction + FinaleBonus 합산(0~100, 구현됨) |
| Safety            | `GetSafety()`                         | `OnSafetyChanged`            | 더미 88     |
| Gold              | `GetGold()`                           | `OnGoldChanged`              | **원본 연결 완료 · ViewModel 구독 중** |
| TotalRevenue      | `GetTotalRevenue()`                   | `OnTotalRevenueChanged`      | **원본 API 완료 · ViewModel 연결 요청** |
| TodayRevenue      | `GetTodayRevenue()`                   | `OnTodayRevenueChanged`      | **원본 API 완료 · ViewModel 연결 요청** |
| Stage / Day       | `GetCurrentStage()` `GetCurrentDay()` | `OnStageViewChanged`         | **진짜 값**  |
| 남은 시간             | `GetRemainingSeconds()`               | `OnRemainingSecondsChanged`  | **진짜 값 · 구독 중계** |
| 전체 스태프 | `GetTotalStaffCount()` | `OnTotalStaffCountChanged` | 원본 연결, 모르면 `INDEX_NONE`. 가용/전체 구분은 없어졌습니다 |
| 구매 홍보 | `GetPurchasedPromotion()` | `OnPurchasedPromotionChanged` | 원본 등급 미러링. 재시작 통지는 아래 주의 |
| 예상 방문객 | 아직 ViewModel API 없음 | 아직 중계 없음 | 경제 원본 `GetExpectedVisitors()`는 구현됨 |

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
int32 SimSatisfaction;                    // 0~100, 높을수록 좋음. 아래 네 입력의 2단 가중합
int32 SimSatisfactionWaitScore;           // 0~100, 대기 항 하위 점수(큐 성공률 보정 포함)
int32 SimSatisfactionDensityScore;        // 0~100, 밀도 항 하위 점수
int32 SimSatisfactionPreferenceScore;     // 0~100, 취향매칭 항 하위 점수
int32 SimSatisfactionNeedFulfillmentScore; // 0~100, Need 충족도 하위 점수
int32 ScoreSatisfaction;  // 0~100, 높을수록 좋음. SimSatisfaction + FinaleBonus (구현됨, 연출 보정은 없음)
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

**갱신 방식:** Stage·Gold·스태프·편성·홍보 구매는 소유자 이벤트를 ViewModel이 중계합니다.
미연결 지표는 이 구독과 별개입니다. 모든 지표가 0.1초 폴링으로 갱신된다고 가정하지 않습니다.

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
| Report              | **둘 다, 분해식으로** — `관객 만족도 62` `+ 피날레 10` `= 평가 점수 72` | 두 지표의 관계가 화면 자체로 설명됩니다                                                                           |
| Finale 구매           | `ScoreSatisfaction` **예상치** — "S 구매 시 → 77"          | 지표를 나눈 기획 의도가 그대로 구매 판단 근거가 됩니다                                                                  |

화면 라벨은 내부 이름을 그대로 쓰지 않습니다 — `SimSatisfaction` → **관객 만족도**,
`ScoreSatisfaction` → **평가 점수**. Report 드릴다운(AT-005)은 왼쪽 항인 관객 만족도에 걸립니다.

#### 분해값 4종

`SimSatisfaction` 하위 점수 4개(대기·밀도·취향매칭·Need 충족도)가 ViewModel에 있습니다.
Getter + 델리게이트 쌍, 0~100, SimSatisfaction과 같은 주기(1초)로 갱신됩니다. 화면에
넣을지·어디에 넣을지는 UI 판단입니다 — 최종 숫자 하나로는 원인을 못 짚을 때 쓰시면 됩니다.

산식은 2단 계층입니다: 환경 축(대기+밀도)과 개인 부합도 축(취향+Need)을 각각 합친 뒤
그 둘을 다시 합칩니다. 축 가중치(2026-10-09부터 환경 0.4 / 개인 부합 0.6, 개인 부합 안에서 취향 0.35 / 욕구 충족 0.65. 전엔 전부 0.5)는 조정 대상이지만
API 계약(Getter·델리게이트·0~100 범위)은 안 바뀝니다.

### Slot을 노출하지 않습니다

하루 4 Slot 구조와 `CurrentSlot`은 제거됐습니다. UI는 Stage·Day·Live 경과 시각을 사용합니다.

### 게임 시계는 Live에서만 흐릅니다

2026-08-06 16:59 시간 회의 결정입니다. UI 표시에 직접 영향이 있습니다.

| Stage              | 시간의 의미                                       |
| ------------------ | -------------------------------------------- |
| `Setup`            | 게임 시각과 분리된 **UI 제한시간**                       |
| `PromotionPreview` | 게임 시각 진행 안 함                                 |
| `Live`             | **여기서만** 게임 시계가 흐름. 축제별 `LiveStartTime`에서 시작 |

현재 개장 시각 17:00은 Stage 변환 함수 안에 있습니다. `LiveStartTime` 데이터 API가 있다고 가정하거나 공통 UI에 17을 복제하지 않습니다.

### `UFF_MetricsHudWidget`

- **헤더**: `Public/UI/LiveHUD/FF_MetricsHudWidget.h`
- **상태**: `구현완료` (Phase 0 산출물)

지표를 숫자로만 표시합니다. 좋고 나쁨 구간이 정해지기 전이라 **색을 칠하지 않았습니다.**
임의로 정하면 나중에 전부 바꿔야 하고, 색만으로 구분하면 AT-011 위반입니다.

### 이미 만든 UI 베이스 (참고용)

| 클래스                         | 헤더                                     | 용도                                                   |
| --------------------------- | -------------------------------------- | ---------------------------------------------------- |
| `UFF_UIRootLayout`          | `Public/UI/Core/FF_UIRootLayout.h`          | HUD·Screen·Panel·Toast 레이어. `PushScreen`/`PopScreen` |
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

# 아직 안 맞는 것 — 2026-09-02 코드 대조 기준

**A. 화면에 틀린 값이 뜨는 자리** (API 는 있는데 안 붙었거나 다른 값이 붙음)

| 무엇 | 담당 | 지금 무슨 일이 벌어지나 | 어떻게 |
|---|---|---|---|
| 런타임 enum 메타데이터 조회 **2개 파일** | 백종태 | 옵션 카테고리·개발용 스텁 제목에 남음 | 위 `표시 문자열 연결 상태` 절 |
| `TotalRevenue` · `TodayRevenue` | 백종태 | 상단 바 누적 매출이 안 움직입니다 (ViewModel 이 더미를 들고 있음) | Gold 와 같은 모양. 위 `경제 값 4종` 절 |
| Heat Map 셀 크기 | 백종태 | 5m 가정이면 밀도 분모가 틀려 색 구간이 통째로 어긋납니다 | `CellSizeMeters`(=2m) 를 읽기. 위 `Crowd · 밀집` 절 |
| `CauseId` 롤업 | 김시리 · 백종태 | Need 집계는 연결됨. 지표에서 Report 원인으로 묶는 표시는 별도 | `GetNeedUnmetRatio`·`GetSampleCount` 사용 |
| 예상 방문객 UI·스폰 연결 | 백종태 · 이정원 | 경제의 하루 단일 예상치는 구현됨. HUD 범위 표시와 실제 유입 수는 미연결 | 아래 홍보 절 |
| 재시작 홍보 표시 | 김시리 · 백종태 | 경제 등급은 초기화되지만 기존 ViewModel 캐시에 이전 구매 표시가 남을 수 있음 | 재시작 시 변경 통지 또는 초기 동기화 보완 필요 |

**B. 미구현 API·추가 계약** (기획서 6.3 Overview 필수 항목 + 프로토타입 요구)

| 필요한 것 | 소유자 | UI에서 쓰는 곳 |
| --- | --- | --- |
| ~~**Satisfaction** (`SimSatisfaction` 0~100)~~ | 김시리 · 이정원 | ✅ **연결 완료.** 대기·밀도·취향매칭·Need 충족도 4항을 2단 계층(환경 축/개인 부합도 축)으로 산출 + 분해값 4종. 위 「값 규격」·「분해값 4종」 절 참조. `ScoreSatisfaction` 합산(SimSatisfaction + FinaleBonus)도 구현됐습니다(10/9 확인) |
| **Safety** | 이정원 | Overview 지표, 경고 |
| **예상 방문객 범위·가정값 미리보기** | 김시리 | 현재 구매 상태의 하루 단일값은 구현됨. `Min`/`Max`와 구매 전 등급별 미리보기 API는 없음. 단일값을 임의 범위로 늘리지 않음 |
| **`UFF_FestivalEventSubsystem`** — 사건 목록·심각도·필요 인력·남은 시간·`CauseId` | 이정원 | Live 우측 상단 상시 사건 트레이 (`FF_FestivalDefinition.h:146`) |
| **스태프 배치 Command + 결과 이벤트** | 이정원 · 김시리 | 사건 트레이의 `스태프 N명 배치`. 🟡 **2026-09-22**: 개인별 명단 조회·승급(`GetStaffRoster`/`TryPromoteStaff`)은 완성됨(위 「스태프 강화(승급)」 절) — 남은 것은 "어느 사건에 누굴 보낼지"를 정하는 배치 Command 자체이고, 이건 사건 시스템이 먼저 있어야 함 |
| **뒷정리 등급 효과 계수** | 김시리 | Day 마감 시트. 가격 3개만 있음(`FF_FestivalDefinition.h:110` TODO) — 지금은 `효과 계수 미확정` 배지 |
| **최종 점수 산식·등급 가중치** | 김시리 | 최종 결산 — 지금은 `가중치 잠정` 문구 |
| **저장 슬롯 목록 + 저장·불러오기 Command** | 김시리 | ESC 수동 저장, 플레이 허브 |
| ~~**SNS 반응 Read Model** + `MessageTemplateId` → 현지화 문장 매핑~~ | 김시리 (+ 백종태 UI 연결) | 🟡 **값은 준비 완료, UI 연결만 남음(2026-09-16).** `UFF_CrowdInsightsSubsystem`이 3~10초마다 화제·문장을 결정론적으로 골라 글을 만듭니다. `UFF_FestivalViewModelSubsystem::GetRecentSnsPosts()`(`TArray<FFF_LiveSocialPost>`, 최신이 `[0]`)와 `OnSnsPostAdded`(`FFF_LiveSocialPost`)가 이미 있습니다 — Gold·SimSatisfaction과 같은 Bind+Sync 패턴이라 `FF_LivePanelSNS`의 `Debug_SetPosts` 자리를 이걸로 바꾸시면 됩니다. `SentimentScore`는 화제가 그 순간 얼마나 두드러졌는지(0~100)를 그대로 부호만 붙여 보냅니다. 시설 표기(`{시설}` 문장)는 가리킬 시설이 분명한 화제(1·7·10·14)에서 실제 시설 이름으로 치환되어 나옵니다. 나머지 화제의 `{시설}` 문장은 후보에서 빠집니다 |
| **준비 단계 관객 수** | 이정원 | 하단 바 `관객` 칸. 준비 단계에 관객이 없는 것은 정상이고, **값이 없으면 칸을 숨깁니다** — `0` 을 띄우면 틀린 사실이 됩니다 |

PrepHUD의 예상 방문객 범위와 관객 수는 아직 `Debug_SetPlaceholders()`로만 채웁니다.
경제의 하루 단일 예상치 API는 있지만 이 범위 표시에는 연결되지 않았습니다.
범위 계약과 ViewModel 중계를 연결하기 전까지 값이 없는 칸은 숨깁니다.

**요청하지 않는 것 2개** (다시 논의하지 않기 위해 적어 둡니다)

- **시설 수** — `UFFZSWorldSubsystem::BuildReadModel()` 의 `Facilities` 를 세고 `OnWorldChanged`
  로 갱신합니다(한광훈 님 API 그대로). 개장 CTA 활성 판정에 씁니다
- **시설 유지비** — 기획서에도 경제 스펙에도 없는 개념이라 `EFF_SpendReason` 에 사유를
  추가하지 않습니다. **마감 화면 원장에 유지비 줄이 있으면 빼 주세요** — `FacilityInstall` 로
  묶이는 것이 아니라 지출 자체가 발생하지 않습니다

### 🆕 라이브·결산 HUD 를 만들며 나온 요청 8건 (2026-08-29 · 09-02, 백종태)

라이브 탭 7종과 마감·리포트 HUD 를 붙이면서 막힌 것들입니다. **화면은 지금도 뜹니다** —
없는 값은 `모름` 으로 두고 그 사실을 화면에 적어 뒀습니다. 아래가 채워지면 그 문구를 지웁니다.

우선순위는 **화면 몇 개에서 동시에 보이는가**로 매겼습니다.

| # | 필요한 것 | 소유자 | 없으면 화면이 어떻게 되나 |
| --- | --- | --- | --- |
| 1 | **누적 방문객 수** — 오늘 축제장에 들어온 총 인원 | 이정원 | 운영 패널 `일일 목표 500명` 진행이 통째로 `아직 알 수 없습니다` 입니다. **아래 ⚠️ 를 꼭 봐 주세요** |
| 2 | **시설 표시 이름** (`FText`) | 한광훈 | 시설·대기열·프로그램 편성 **3개 화면**에서 `Booth_FoodStall_A` 같은 정의 id 를 그대로 띄웁니다. 현지화도 불가 |
| 3 | **`CompletedCount` 노출** | 한광훈 | 평균 대기의 `0.0` 이 `표본 없음` 인지 `모두 0초 대기` 인지 구분 못 합니다 |
| 4 | **큐 id ↔ 시설 id 다리** | 장원재 · 한광훈 | 대기열 패널에 **예상 대기 시간**을 못 붙입니다 |
| 5 | **줄을 포기한 손님 수** | 장원재 · 이정원 | 줄이 길어서 잃은 손님이 안 보입니다. 대기 인원보다 이게 진짜 손해입니다 |
| 6 | **시설별 스태프 배치** | 김시리 | 스태프 패널이 "프로그램에 N명" 까지만 말합니다 |
| 7 | **입력 모드 정책** | 카메라 담당 | UI 마우스 캡처가 깨집니다. 우회는 했지만 원인은 남아 있습니다 |
| 8 | **마감·준비 단계 배속** | 강민서 | 마감·리포트·셋업·개장 준비에서 **배속 버튼을 못 답니다.** 눌러도 안 되는 버튼이 되기 때문입니다 |

---

#### 1. 누적 방문객 수 — ⚠️ **동시 인원과 다른 값입니다**

`UFF_CrowdSimulationSubsystem::GetActiveAgentCount()` 는 **이미 있고 잘 동작합니다.**
그래서 라이브 상태 바의 `관객` 칸은 2026-08-29 에 그것으로 붙였습니다 (`GetConcurrentVisitorCount()`).

**그런데 그 값을 일일 목표에 쓰면 안 됩니다.**

```
GetActiveAgentCount()   지금 월드에 있는 사람 수      (동시)
DailyVisitorGoal = 500  오늘 들어온 총 인원           (누적)
```

동시 80명을 500명 목표의 분자로 넣으면 **진행률이 16% 에서 영원히 안 올라갑니다.**
숫자가 그럴듯해서 눈으로는 안 걸리는 종류라, 지금은 아예 `모름` 으로 두고 있습니다.

- 필요한 것: **하루 동안 입장한 누적 인원** + 값이 바뀔 때의 델리게이트
- 하루 경계에서 0 으로 리셋되어야 합니다 (`OnDayEnded` 시점)
- 못 읽을 때는 `0` 이 아니라 **`INDEX_NONE`** 으로 주세요. `0` 은 "아무도 안 왔다" 라는
  확정 사실이라 화면이 그대로 그립니다

#### 2. 시설 표시 이름

`UFFZSFacilityDefinition` 에 사람이 읽을 이름 필드가 없습니다. `meta = (DisplayName=...)` 은
에디터 메타데이터라 런타임에 못 씁니다 (패키징 빌드에서 사라집니다).

지금은 정의 id 를 `UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId` 로 쪼개서
고유 토큰만 띄우고 있습니다. 이름 규칙을 어긴 시설은 id 전체가 그대로 나옵니다.

- 필요한 것: `UFFZSFacilityDefinition` 에 `FText DisplayName` 한 줄
- `FFFZSFacilityReadModel` 로도 실려 오면 UI 가 정의를 따로 안 열어도 됩니다

#### 3. `CompletedCount` 노출

`UFFZSFacilityQueueComponent::GetAverageCompletedWaitSeconds()` 는
`CompletedCount > 0 ? 평균 : 0.0f` 입니다. 즉 **`0.0` 이 두 가지 뜻**입니다 —
아직 아무도 못 끝냈거나, 모두 0초에 처리됐거나.

지금 UI 는 `> 0` 이면 표본이 있다고 **추정**하고 있습니다. 실제로는 거의 안 틀리지만
추정인 것은 사실입니다. `CompletedCount` 가 `FFFZSFacilityReadModel` 에 한 줄 실리면
추정이 사라집니다.

이게 중요한 이유: 대기열 패널의 전체 평균은 **표본 없는 시설을 빼고** 냅니다.
0 으로 세면 평균이 실제보다 낮아져서, 줄이 길게 늘어서 있는데 `평균 20초` 가 뜹니다.

#### 4. 큐 id ↔ 시설 id 다리

`UFF_QueueSubsystem` 은 `int32 FacilityId`, ZoneSystem 은 `FGuid FacilityInstanceId` 입니다.
그래서 `GetExpectedWaitSeconds()` · `GetFacilityServiceRatePerMinute()` 를 **시설 이름 옆에
못 붙입니다.**

`UFF_ZoneSystemQueueBridgeSubsystem` 이 이미 둘을 잇고 있는 것으로 보이는데,
UI 가 쓸 수 있는 변환 함수 하나면 됩니다 — `FGuid → int32` 또는 그 반대 한 방향.

#### 5. 줄을 포기한 손님 수

대기 인원은 "지금 기다리는 사람" 이라 **이미 잃은 손님은 안 보입니다.** 줄이 길어서
돌아선 사람 수가 운영 판단의 실제 근거인데 지금은 화면에 못 올립니다.

- 시설별 / 오늘 누적 둘 다 있으면 좋습니다
- `FFF_QueueLog` 에 조인 기록은 있는데 포기 기록이 있는지 확인 부탁드립니다

#### 6. 사건별 스태프 배치

> 2026-09-22 갱신: **개인별 명단은 이제 읽을 수 있습니다** — 위 「스태프 강화(승급)」 절의
> `GetStaffRoster()`. 다만 "어느 사건에 누가 붙어 있는가"(배치 상태)는 **여전히 없습니다** —
> 그 배치 상태를 들고 있는 사건 시스템 자체가 아직 없어서입니다. 그래서 스태프 패널이
> "레벨별 인원 구성"까지는 말할 수 있어도 "어디에 몇 명"은 여전히 못 말합니다.

> 2026-09-21 정정: 예전에 여기 적혀 있던 `GetStaffAssignedToPrograms()` 는 **없어졌습니다.**
> 프로그램이 인원을 안 쓰게 되면서 "묶인 인원" 자체가 0 이 됐습니다. 배치라는 개념은
> 사건 시스템이 와야 처음 생깁니다.

사건 대응(`스태프 N명 배치`)이 들어오면 이 값이 반드시 필요해집니다 — 배치 Command 는
이미 B 표에 요청해 둔 것과 같은 건입니다. Live 인력 패널에 자리를 비워 뒀습니다
(`Box_Assignments` — 지금은 `지금 대응 중인 사건이 없습니다` 로 접혀 있습니다).

#### 7. 입력 모드 정책 — **UI 전반에 영향**

`AFF_FestivalPlayerController` 가 `SetInputMode` 를 **한 번도 부르지 않습니다.**
그래서 기본값 `FInputModeGameOnly` 로 도는데, 그 모드는 `bHideCursorDuringCapture = true` 입니다.

슬레이트가 마우스를 캡처하는 순간 뷰포트가 커서를 숨기고 가둡니다. 패널을 끌면 —
커서가 사라지고, 좌표가 안 변해 안 움직이고, 뗀 이벤트가 위젯에 안 와서 손을 뗀 뒤에도
패널이 커서를 따라다녔습니다.

**UI 쪽에서 캡처를 안 쓰는 것으로 우회했습니다** (2026-08-28). 입력 모드를 UI 가 바꾸면
건축 모드·미니게임이 저장·복원하는 값과 서로 덮어쓰기 때문에 손대지 않았습니다.

건축 모드 쪽 주석에 "이 게임의 기본 GameOnly 정책" 이라고 적혀 있어서 **의도한 결정일 수도
있습니다.** 그렇다면 그대로 두고, 아니라면 컨트롤러에서 `FInputModeGameAndUI` +
`SetHideCursorDuringCapture(false)` 로 잡는 것이 UI 입장에서는 정상입니다.

---

#### 8. 마감·준비 단계 배속 (2026-09-02 추가) · **강민서님께**

`SetTimeScale()` 로 저장한 `TimeScale` 이 **Live 분기에서만 쓰입니다.**

```cpp
// Live 분기 — 배속이 걸린다
const float EffectiveScale = bIsLockedNow ? 1.f : TimeScale;
ElapsedLiveSeconds += DeltaTime * EffectiveScale;

// 비-Live 분기 (Setup / PromotionPreview / MorningCheck / Closing / Report)
ElapsedLiveSeconds += DeltaTime;   // ← 항상 1배속. 주석에도 그렇게 적혀 있습니다
```

그래서 이 단계들에는 **배속 버튼을 안 달았습니다.** 달면 눌러도 아무 일도 안 일어나는
버튼이 되고, 그건 없는 것보다 나쁩니다.

**왜 필요한가** — 이 단계들도 시간이 흐르고 다 되면 자동으로 넘어갑니다. 할 일을 끝낸
플레이어는 남은 시간을 그냥 기다려야 합니다. 특히 마감·리포트는 읽고 나면 할 일이 없습니다.

- 비-Live 분기에도 `EffectiveScale` 을 적용해 주시면 UI 는 **버튼만 열면 됩니다** —
  판정(`FFF_LiveHudRules::ResolveSpeedState`)과 위젯이 이미 있습니다
- 단계별로 허용/금지를 나누고 싶으시면 그 판정도 소유자 쪽에 두시고, UI 는 결과만 읽겠습니다

⚠️ **개장 준비(MorningCheck)는 기획상 시간이 멈추는 단계**로 적혀 있습니다
(`AGENTS.md` 규칙 1). 실제 코드는 시계가 흐르고 자동 진행됩니다 — **어느 쪽이 맞는지
확인 부탁드립니다.** 멈추는 것이 맞다면 배속도 필요 없고, 지금 화면에 띄우는
`{N}초 뒤 자동으로 진행됩니다` 도 지워야 합니다.

#### 9. 🔴 **입장료가 한 번도 걷히지 않습니다** (2026-09-03) · **김시리님께**

실기에서 "사람은 261명인데 오늘 매출이 24" 로 발견했습니다. 코드를 훑어보니 —

```
EFF_EarnReason::AdmissionFee 를 쓰는 곳
  FF_EconomyDisplayTextLibrary.cpp : 1   ← 문구만
  Economy/Tests/*.cpp              : 30  ← 테스트만
  프로덕션 호출부                    : 0
```

실제로 매출이 들어오는 경로는 **시설 수수료**(`FF_QueueEconomyBridgeSubsystem.cpp` 한 곳)와
미니게임 보상·프로그램비·일일 목표 보너스뿐입니다. 입장료는 표(`AdmissionFeeGold = 5`)에만
있고 아무 데서도 `AddRevenue(..., EFF_EarnReason::AdmissionFee)` 를 부르지 않습니다.

⚠️ **UI 는 이미 그 돈을 약속하고 있습니다.** 점검 패널이 `예상 입장료 수입` 을
`예상 방문객 × 입장료` 로 계산해 보여 주는데 (`FF_PrepPanelCheck.cpp:74`), 실제로는 안
들어옵니다. 플레이어 입장에서는 **화면이 거짓말을 하는 셈**입니다.

필요한 것: 라이브 중 관객이 입장할 때 그 이유로 매출을 올리는 호출부.
누가 입장을 판정하는지(Crowd? Queue? Zone?) 몰라서 UI 쪽에서 채울 수 없습니다.

> UI 는 이 값을 새로 읽을 필요가 없습니다. 지금도 `OnTodayRevenueChanged` 를 구독하고
> 있어서, 매출이 실제로 오르면 화면은 그대로 따라갑니다.

---

#### 10. 단계에 시간 제한이 있는지 읽을 표면 (2026-09-03) · **강민서님께**

마감을 무제한(`MaxSeconds = 0`)으로 바꾸면서 생긴 것입니다.

`Tick` 은 `MaxSeconds <= 0` 이면 곧바로 return 하므로 **`RemainingTime` 이 갱신되지 않고
이전 단계 값이 그대로 남습니다.** 라이브가 0 으로 끝나니 마감 내내 하단 바에 —

```
17:00 · 남은 0초
```

제한이 없는데 `남은 0초` 는 **시간이 다 된 것처럼** 읽힙니다.

`남은 N초` 부분만 접으면 되는데, 지금은 UI 가 "이 단계에 제한이 있나" 를 알 방법이
없습니다. `GetPhaseTimeRange()` 가 `const` 가 아니라 ViewModel 의 const 경로에서 못 씁니다.

원하는 것 (둘 중 아무거나):
```cpp
UFUNCTION(BlueprintPure) bool CurrentStageHasTimeLimit() const;
// 또는
FFF_PhaseTimeRange GetPhaseTimeRange(EFF_FestivalStage Stage) const;   // const 추가
```

임시로는 UI 가 `GetRemainingSeconds() <= 0 && 라이브가 아님` 으로 짐작할 수 있지만,
**진짜로 0초 남은 순간과 구분이 안 되므로** 하지 않았습니다.

---

#### 11. Heat Map — 제품용 렌더링과 등급 경계값 (2026-09-03) · **이정원님께**

밀집 탭(토글 + 범례 + 요약)을 만들려고 코드를 읽다가 정리한 것입니다.
**결론부터: 밀도 원본은 이미 충분합니다.** 막히는 것은 두 가지뿐입니다.

##### 이미 있어서 그대로 쓸 것 (감사합니다)

```cpp
UFF_CrowdSimulationSubsystem::GetMaxDensity()            // 가장 붐비는 셀 (명/㎡)
UFF_CrowdSimulationSubsystem::GetCongestedCellCount()    // 혼잡 셀 수
UFF_CrowdSimulationSubsystem::GetLOSAtLocation(FVector)
FCrowdDensityGrid::DensityToLOS(float)                   // static, 공개
ECrowdLOS                                                // A~F, 공개 헤더
```

`FFF_CrowdReadModel` 의 `Occupancy`·`Throughput`·`DensityPerSqm` 도 확인했습니다.
**UI 는 밀도를 다시 계산하지 않습니다** — 위 값을 그대로 표시합니다 (C-009 규칙).

##### 🔴 (1) LOS 경계 6개를 헤더로 올려 주세요

```cpp
// 지금: FF_CrowdDensityGrid.cpp 9행, 익명 네임스페이스라 UI 가 못 읽습니다
namespace CrowdDensity
{
    static constexpr float LOS_A_Max = 0.31f;
    ...
}
```

**범례에 수치를 적어야 합니다.** AT-011 상 색만으로 상태를 구분할 수 없어서
`초록 = 한산` 이 아니라 `A · 0.31 미만 · 한산` 으로 써야 합니다.

UI 가 `0.31` 을 코드에 베껴 두면 그쪽이 값을 바꿨을 때 **범례만 조용히 틀려집니다.**
헤더로 올려 주시면(또는 `GetLOSRange(ECrowdLOS)` 같은 함수로) 그럴 일이 없습니다.

> ⚠️ 이것 때문에 **C-029 는 사실상 정리됐습니다.** UI 문서의 3구간(`<2 / 2~4 / >=4`)은
> Fruin 기준으로 **2.17 이 이미 최악(F)** 이라 `빨강 4 이상` 에 도달조차 못 합니다.
> 구현하신 6등급을 제품 기준으로 삼고 UI 문서를 맞추겠습니다. 이견 있으면 알려 주세요.

##### 🔴 (2) 제품용 렌더링과 토글

지금 화면에 보이는 히트맵은 `FF.Crowd.Debug 1` → `FCrowdDensityGrid::DrawDebug()` →
`DrawDebugSolidBox` 입니다. **`DrawDebug*` 는 셰이핑 빌드에서 컴파일 아웃되므로
패키징에는 안 나옵니다.**

필요한 것:
- 제품 빌드에서도 그려지는 밀도 표시 (데칼·머티리얼·인스턴스 메시 등 — 방식은 그쪽 판단)
- UI 가 부를 토글 — 콘솔 변수 말고 함수 하나면 됩니다

```cpp
UFUNCTION(BlueprintCallable) void SetDensityOverlayVisible(bool bVisible);
UFUNCTION(BlueprintPure)     bool IsDensityOverlayVisible() const;
```

##### 그때까지 UI 가 할 일

**밀집 탭을 `에디터 전용` 전제로 먼저 만들어 두겠습니다.** 토글이 `FF.Crowd.Debug` 를
호출하고, 위 API 가 생기면 **그 한 줄만 갈아끼웁니다.** 범례·요약은 그대로 씁니다.

패널에 담을 것 —

| 구역 | 내용 |
| --- | --- |
| 토글 | `히트맵 보기` 켜기/끄기 |
| 범례 | A~F · 색 + **명/㎡ 수치** + 기호 (색 단독 금지) |
| 요약 | `가장 붐비는 곳 N.N명/㎡ (등급 D)` · `혼잡 셀 N개` |
| 못 읽을 때 | `밀도를 아직 읽지 못했습니다` — **0 으로 쓰지 않습니다** |

##### 확인 부탁

등급 이름을 `A 한산 / B 여유 / C 보통 / D 붐빔 / E 혼잡 / F 위험` 으로 붙일 생각인데,
Fruin 원문은 보행로 흐름 기준이라 축제장 표현으로 맞는지 봐 주세요.
아니면 문자(A~F)와 수치만 쓰고 이름은 빼겠습니다.

---

#### ✅ 되돌린 것 — 마지막 날 배속 잠금 (2026-09-02)

UI 가 3일차에 배속을 막고 있었는데, **소유자 코드에는 그런 잠금이 없었습니다.**
웹 프로토타입(`js/state.js:299`)의 `마지막 날은 배속을 쓸 수 없습니다` 를 옮긴 것이
원인이었습니다. UI 쪽 규칙을 지웠습니다 — 요청 사항이 아니라 알려 드리는 것입니다.

---

**요청하지 않는 것** (다시 논의하지 않기 위해 적어 둡니다)

- **라이브 동시 관객 수** — `GetActiveAgentCount()` 로 붙였습니다. 위 1 번과 다른 값입니다
- **스태프 개인 상태**(피로도·위치) — 지금 화면에 쓸 자리가 없습니다. Event 나 스태프
  개별 조작이 들어올 때 다시 이야기하겠습니다

### 🆕 홍보 4등급 · 입장료 · 화제성(Buzz) · 축제 평가 — UI 작업 안내 (2026-09-23, 김시리 → 백종태)

> **아래 두 절("홍보 구매·예상 방문객 API", "🔴 홍보 — 웹 프로토타입과 코드가 다릅니다")의 기획 결정이 났습니다.**
> 웹 프로토타입 방향(홍보 없음 = 손해, 입장료를 플레이어가 정함, 시작 버즈)을 **수치만 조정해서** 채택했습니다.
> 경제 쪽 API 는 전부 구현·테스트 완료(`FestaFiesta.Economy.Forecast.*`, `Economy.Evaluation.Rules`)이고,
> 남은 것은 화면입니다. 이 절이 최신 계약이고, 아래 두 절은 과거 기록입니다.

#### 필요한 화면 5가지 (우선순위 순)

| # | 화면 | 어디서 | 무엇을 그리나 |
|---|---|---|---|
| 1 | **입장료 슬라이더** | 준비 화면(Setup·MorningCheck), **매일 아침** | 0~15 Gold 슬라이더 + 미리보기("예상 방문객 420명 · 입장료로 −80명 · 예상 입장료 수입 4,200") |
| 2 | **홍보 카드 4장** | 홍보 단계(기존 패널) | 홍보 없음 / 지역 SNS / 전국 홍보 / 인플루언서. 카드마다 비용·방문객 배수·화제성 +N·예상 방문객 |
| 3 | **화제성 게이지** | Live HUD (+ 준비 화면) | 0~100 막대, 50 이 중립. "오늘 +4.2" 처럼 아침 대비 변화 |
| 4 | **축제 평가 화면** | 마지막 날 Report 이후(`FinalReportScreenClass` 가 지금 비어 있음) | 등급 S~D, 총점, 항목 4개 점수, 한 줄 평, 보완 한 줄, 날짜별 만족도 |
| 5 | 일일 목표 진행도 | Live 패널(운영) | "오늘 방문객 312 / 900 · 달성 시 +300 Gold"(목표는 날마다 900/1000/1100). 지금 `Debug_SetVisitorCount` 로만 채워짐 |

SNS 입장료 글(화제 15·16)은 **기존 SNS 피드에 그대로 섞여 나오므로 추가 UI 가 필요 없습니다.**

#### API — 전부 `UFF_FestivalProgressionSubsystem` (GameInstance) 에 있습니다

| 용도 | API | 비고 |
|---|---|---|
| 현재 예보 | `FFF_VisitorForecast GetVisitorForecast()` | 구매한 홍보 · 현재 입장료 · 현재 화제성 기준 |
| **미리보기** | `FFF_VisitorForecast PreviewVisitorForecast(EFF_PromotionGrade, int32 Fee)` | **상태를 안 바꿉니다.** 슬라이더를 끄는 동안·홍보 카드 호버 때 부르세요. 안 산 등급을 넣으면 그 등급의 화제성 시작값까지 얹어서 계산합니다 |
| 예상 방문객 한 숫자 | `int32 GetExpectedVisitors()` + `OnExpectedVisitorsChanged(int32)` | = `GetVisitorForecast().ExpectedVisitors`. 입장료·홍보·화제성 어느 게 바뀌어도 발행 |
| 입장료 현재값 | `int32 GetAdmissionFee()` + `OnAdmissionFeeChanged(int32)` | 축제 전이면 `INDEX_NONE` |
| 입장료 범위 | `GetAdmissionFeeRange(int32& Min, int32& Max, int32& Reference)` | 지금 0 / 15 / 3(기준값 2026-10-09 에 5 → 3). **숫자를 UI 에 복제하지 마세요** — 밸런싱으로 바뀝니다 |
| 입장료 변경 | `EFF_AdmissionFeeResult TrySetAdmissionFee(int32)` | `Success / FestivalNotStarted / OutOfRange / LockedDuringLive`. **Live 중에는 거부**합니다 |
| 홍보 티어 | `FFF_PromotionTier GetPromotionTier(EFF_PromotionGrade)` | `CostGold`·`VisitorMultiplier`·`StartingBuzz`. `None` 도 부를 수 있고 배수 0.85 가 옵니다 |
| 홍보 구매 | `TryPurchasePromotion(EFF_PromotionGrade)` (기존) | `EFF_PromotionGrade::Influencer` 가 추가됐습니다 |
| 일일 목표 | `GetAdmittedVisitorsToday()` · `GetDailyVisitorGoal()` · `IsDailyGoalBonusPaid()` + `OnAdmittedVisitorsTodayChanged(int32)` | 목표 없음은 `INDEX_NONE`. 달성 여부는 이벤트를 받은 뒤 `IsDailyGoalBonusPaid()` 로 읽으세요. `GetDailyVisitorGoal()` 은 **오늘 목표**입니다(2026-10-09 부터 Day N = 900 + 100 × (N−1) → 900/1000/1100). 날이 바뀌면 다시 읽으세요 |
| 축제 평가 | `FFF_FestivalEvaluation GetFestivalEvaluation()` | 축제 도중에 불러도 됩니다(진행 중 평가). 마지막 날은 현재 만족도를 그날 값으로 씁니다 |
| 날짜별 만족도 | `TArray<int32> GetDailySatisfactionHistory()` | 끝난 날만. 평가 화면 그래프용 |

화제성은 별도 서브시스템입니다: **`UFF_FestivalBuzzSubsystem`** (GameInstance)

| 용도 | API |
|---|---|
| 현재값 | `float GetBuzz()` (0~100, 50 중립) + `OnBuzzChanged(float NewBuzz, float PreviousBuzz)` |
| 오늘 아침 값 | `float GetBuzzAtDayStart()` — "오늘 +N" 은 `GetBuzz() - GetBuzzAtDayStart()` |

`OnBuzzChanged` 는 SNS 글이 올라올 때마다(수 초 간격) 나갑니다. 숫자를 매번 애니메이션하면 산만하니
게이지는 보간해서 따라가게 하시는 걸 권합니다.

#### ✅ ViewModel 경로 — 위젯은 이쪽을 쓰세요 (2026-09-28)

위 두 표의 API 를 **`UFF_FestivalViewModelSubsystem` 에 전부 중계해 두었습니다.** 다른 값과 같은 원칙입니다 —
현재값 getter + 변경 델리게이트 쌍, 명령은 `Request*`, 성공해도 낙관적 갱신 없이 결과 이벤트로 다시 그립니다.

| 화면 | ViewModel API |
|---|---|
| 입장료 슬라이더 | `GetAdmissionFeeGold()` + `OnAdmissionFeeChanged` · `GetAdmissionFeeRange(Min, Max, Reference)` · `CanSetAdmissionFeeNow()`(Live 중 false — 슬라이더 비활성) · **`RequestSetAdmissionFee(int32)` → `EFF_AdmissionFeeResult`** |
| 예보·미리보기 | `GetVisitorForecast()` · `PreviewVisitorForecast(EFF_PromotionChoice, int32 Fee)`(상태 안 바뀜) · `GetExpectedVisitors()` + `OnExpectedVisitorsChanged` |
| 화제성 게이지 | `GetBuzz()` · `GetBuzzAtDayStart()` · `GetBuzzChangeToday()`("오늘 +N" 그대로) + `OnBuzzChanged(NewBuzz, PreviousBuzz)`. 하루가 바뀌어 아침 기준만 바뀌어도 옵니다 |
| 일일 목표 진행도 | `GetAdmittedVisitorsToday()` + `OnAdmittedVisitorsTodayChanged` · `GetDailyVisitorGoal()` · `GetDailyGoalBonusGold()` · `IsDailyGoalBonusPaid()` · `GetTotalAdmittedVisitors()`(축제 누적). `Debug_SetVisitorCount` 자리를 이걸로 바꾸시면 됩니다. `GetDailyVisitorGoal()` 은 오늘 목표(900/1000/1100)라 날이 바뀌면 다시 읽어야 합니다 |
| 축제 평가 화면 | `GetFestivalEvaluation()` · `GetDailySatisfactionHistory()` |
| 스태프 승급 | `GetStaffRoster()` · `CanPromoteStaff(StaffId)`(행별 버튼 활성) · `RequestPromoteStaff(StaffId)` + `OnStaffRosterChanged` |

**표시 문자열** — `UFF_EconomyDisplayTextLibrary` 에 추가했습니다. enum `DisplayName` 을 읽지 마세요(패키징에서 영문이 뜹니다).

| 함수 | 용도 |
|---|---|
| `GetAdmissionFeeResultText(EFF_AdmissionFeeResult)` | 입장료 변경 실패 문구 |
| `GetPromotionGradeText(EFF_PromotionGrade)` | 홍보 등급 이름 |
| `GetFestivalEvaluationGradeText(EFF_FestivalEvaluationGrade)` | 평가 등급 S~D (피날레 등급 문자열과 따로) |
| `GetFestivalEvaluationAxisText(EFF_FestivalEvaluationAxis)` | 평가 항목 이름(만족도·방문객·재무·화제성) |

**최종 결산 화면 자리** — 축제가 끝나면 `UFF_UIFlowSubsystem` 이 `Project Settings > FestaFiesta UI Flow > Final Report Screen Class`
를 띄웁니다. 지금 `WBP_FF_FlowStub` 이 들어 있으니, 평가 화면 위젯을 만드시면 그 칸만 바꿔 주세요.

#### 구조체

`FFF_VisitorForecast` (`Economy/FF_FestivalEconomyTypes.h`)

| 필드 | 뜻 |
|---|---|
| `ExpectedVisitors` | 최종 예상 방문객 |
| `VisitorsAtReferenceFee` | 입장료가 기준값(지금 3)이었다면 올 인원 |
| `VisitorDeltaFromFee` | 위 둘의 차. **비싸면 음수, 싸면 양수** — "입장료로 −80명" 에 그대로 쓰세요 |
| `ExpectedAdmissionRevenue` | `ExpectedVisitors × AdmissionFeeGold` |
| `AdmissionFeeGold` | 이 예보의 입장료 |
| `PromotionMultiplier` / `AdmissionFeeMultiplier` / `BuzzMultiplier` | 세 배수. 툴팁 분해용 |

`FFF_FestivalEvaluation` (`Economy/Rules/FF_FestivalEvaluationRules.h`)

| 필드 | 뜻 |
|---|---|
| `Grade` | `EFF_FestivalEvaluationGrade::{D, C, B, A, S}` — **피날레 구매 등급(`EFF_FinaleGrade`)과 다른 것**입니다. 화면 이름도 "축제 평가" 처럼 구분해 주세요 |
| `TotalScore` | 0~100 |
| `SatisfactionScore` / `VisitorScore` / `FinanceScore` / `BuzzScore` | 항목별 0~100 |
| `StrongestAxis` / `WeakestAxis` | `EFF_FestivalEvaluationAxis::{Satisfaction, Visitors, Finance, Buzz}` |
| `Headline` | 등급별 한 줄 평 (예: "모두가 다시 오고 싶어 하는 축제였다") |
| `WeakestAxisComment` | 가장 못한 항목 한 줄 (예: "생각보다 손님이 적었다"). S 면 비어 있음 |

#### 규칙 (화면 문구에 필요한 만큼만)

- **방문객 = 기본 1000(`BaseDailyVisitors`) × 홍보 × 입장료 × 화제성 × 평판(전날 만족도, ×0.75~1.25).**
- 홍보 4등급 — 홍보 없음 ×0.85 · 0 Gold / 지역 SNS ×1.00 · 600 · 화제성 +6 / 전국 홍보 ×1.12 · 1,400 · +14 / 인플루언서 ×1.25 · 2,600 · +26.
  **안 사면 손해**입니다. 카드 문구도 그 방향으로 부탁드립니다.
- 입장료 — 기준 3 Gold(2026-10-09 전 5) 에서 1 Gold 비쌀 때마다 방문객 −5%(최대 ×0.5), 쌀 때마다 +5%(최대 ×1.15).
  비싸면 입장료 수입은 늘지만 손님이 줄어 **부스 매출이 줄고 SNS 부정 글이 늘어 다음 날 화제성이 떨어집니다.**
- 화제성 — 홍보 구매·SNS 긍정 글·유료 프로그램 개최로 오르고, SNS 부정 글로 내려갑니다. SNS 글 1건은 최대 0.3 움직이고, 하루가 바뀌면 중립 쪽으로 40% 식습니다(2026-10-09 전: 0.6 · 25%).
- 축제 평가 — 만족도 40% · 방문객 목표 달성률 25% · 재무 20% · 화제성 15%. S ≥ 85, A ≥ 70, B ≥ 55, C ≥ 40.
  방문객 달성률의 분모는 날짜별 목표의 합(`VisitorGoalTotal`)입니다.
  재무 점수 = 100 × (최종 Gold − 시작 Gold) / `FinanceTargetProfitGold`(축제 정의, 지금 15,000 · 잠정), 0~100 으로 자릅니다. 본전·손해면 0점입니다
  (2026-10-09 전: 50 + 50 × 이익 / 시작 자금).

#### 유료 프로그램 — "하루 한 번" 이 시설 단위로 바뀌었습니다 (2026-09-23)

공방 4종(모래 공예·부채·조개 팔찌·레진 키링)이 모두 공방 카드 한 장만 열 수 있어서, 전역 "하루 한 번" 이면
두 번째 공방부터 편성할 카드가 없었습니다. 이제 **같은 카드라도 다른 시설이면 따로 편성**됩니다.

| API | 변경 |
|---|---|
| `Progression::CanScheduleProgramAtFacility(ProgramId, FacilityId)` 🆕 | 이 시설 기준 판정. 시설을 알면 이것을 쓰세요 |
| `Progression::GetScheduledProgramIdsAtFacility(FacilityId)` 🆕 | 이 시설에 오늘 편성한 카드 |
| `ViewModel::CanScheduleProgramAtFacility` / `GetScheduledProgramIdsAtFacility` 🆕 | 위 둘의 중계 |
| `CanScheduleProgram(ProgramId)` | 시설을 모르므로 **중복을 안 봅니다**(카드·Gold 만) |
| `FFF_ScheduledProgram::FacilityInstanceId` 🆕 | 편성한 시설 |

Prep 프로그램 패널의 "✓ 편성함" 표시와 견적 판정은 **선택한 시설 기준**으로 바꿔 두었습니다.

#### ⚠️ 주의 4가지

1. **ViewModel 쪽은 김시리가 먼저 맞춰 두었습니다(2026-09-23).** `EFF_PromotionChoice::Influencer` 추가, 변환 함수 2개,
   `GetPromotionOptions()` 가 4등급을 경제의 `GetPromotionTier` 로 채움(홍보 없음 ×0.85 가 그대로 나옴),
   홍보 패널 라벨(홍보 없음 / 지역 SNS / 전국 홍보 / 인플루언서), `GetAdmissionFeeGold()` 가 **오늘 입장료**를 반환,
   `OnAdmissionFeeChanged` 중계 + 준비 화면(Check 패널) 구독까지. 카드 위젯은 동적으로 만들어져 4장째가 자동으로 붙습니다 —
   레이아웃(가로 폭)만 확인해 주세요. 값(숫자)은 그대로라 저장은 안 깨집니다.
2. **enum 표시명을 `GetDisplayNameText` 로 읽지 마세요**(위 "🔴 enum 의 DisplayName" 절). 패키징 빌드에서 영문으로 나옵니다.
   등급·항목 이름은 UI 쪽 문자열 표로 매핑해 주세요.
3. **예상 방문객은 "올 사람" 이지 "들어온 사람" 이 아닙니다.** 군중 동시 인원 상한(Agent 풀 500)에 막히면 실제 입장은 더 적을 수 있습니다.
   입장료 수입도 실제 입장 인원 기준으로 들어옵니다. 미리보기에 "최대" 같은 단정 표현은 피해 주세요.
4. **입장료는 Live 중 변경 불가**입니다. 슬라이더는 `LockedDuringLive` 를 받으면 되돌리고, Live 동안은 비활성으로 두는 걸 권합니다.
   값이 바뀌면 `OnAdmissionFeeChanged` 가 오므로 낙관적 갱신 없이 이벤트를 받아 그리면 됩니다(원칙 2).

---

### 홍보 구매·예상 방문객 API (2026-09-02 확인)

> ⚠️ 2026-09-23 이후 수치·등급은 바로 위 절이 최신입니다(3등급 → 4등급, 미구매 ×1.00 → ×0.85).

`2ed2679`에서 경제가 구매 판정·차감·등급 보관·하루 예상 방문객 계산을 소유하도록 연결했습니다.
UI는 별도로 `TrySpend(..., Promotion)`을 호출하지 않습니다.

| 역할 | API / 현재 계약 |
|---|---|
| 경제 원본 | `UFF_FestivalProgressionSubsystem` (`Public/Festival/FF_FestivalProgressionSubsystem.h`) |
| 구매 | `TryPurchasePromotion(EFF_PromotionGrade)` → `EFF_PromotionPurchaseResult` |
| 등급·현재값 | `EFF_PromotionGrade::{None, Basic, Focused}`, `GetPurchasedPromotionGrade()` |
| 구매 통지 | `OnPromotionGradeChanged(EFF_PromotionGrade)` |
| 하루 예상 방문객 | `int32 GetExpectedVisitors() const` |
| 예상치 변경 | `OnExpectedVisitorsChanged(int32 NewValue)`, 구매 후 값이 실제로 바뀔 때 |
| 위젯 구매 경로 | `ViewModel::RequestPurchasePromotion(EFF_PromotionChoice)` → `bool` |
| 위젯 현재값·통지 | `GetPurchasedPromotion()` + `OnPurchasedPromotionChanged`. 경제 enum과 UI enum을 변환해 중계 |
| 구매 단계 표시 | `ViewModel::CanPurchasePromotionNow()`: 축제 시작 후 Setup인지 판정. 중복 구매·잔액은 구매 결과로 판정 |

- 구매는 축제당 1회이며 변경·취소는 없습니다. `None`은 미구매 상태라 구매할 수 없습니다.
- 경제 결과는 `Success / FestivalNotStarted / AlreadyPurchased / InvalidGrade / InsufficientGold`입니다.
  실패 시 Gold·원장·등급·예상치는 유지됩니다. 원본과 ViewModel의 구매 Command 자체는 Stage를 검사하지 않습니다.
- 예상치는 `floor(BaseDailyVisitors × max(VisitorMultiplier, 1))`입니다.
  기본값 500명일 때 미구매 500, Basic 540, Focused 590명입니다. 비용·배수는 Definition의
  `PromotionBasic`·`PromotionFocused`를 읽으며 숫자를 UI에 복제하지 않습니다.
- 반환값은 **하루 단일 예상치**이며 3일 합계·범위·최대 동시 인원·실제 입장 인원이 아닙니다.
  예약 프로그램 추가 인원은 이 계산에 포함하지 않습니다. Definition이 없으면 `INDEX_NONE`이며 0명과 구분합니다.
- `OnExpectedVisitorsChanged`의 ViewModel 중계와 PrepHUD 연결은 아직 없습니다.
  실제 유입도 `AFF_SpawnActor::UnitCount`를 사용하므로 홍보 배수가 스폰에 적용되지는 않습니다.
- 홍보 패널의 현재 문구 `방문객 수에 반영되지 않습니다`는 실제 유입에 관해서는 유효하지만,
  예상치 계산까지 미구현이라는 뜻은 아닙니다. 권장 안내: `예상치에는 반영되며, 실제 유입 연동은 준비 중입니다.`
- 재시작 시 경제의 구매 등급은 `None`으로 초기화됩니다. 다만 `StartFestival`은
  `OnPromotionGradeChanged`·`OnExpectedVisitorsChanged`를 발행하지 않습니다.
  **기존 ViewModel의 재시작 동기화는 미완료**이며 헤더의 “재시작도 이벤트로 통지” 주석은 현재 구현과 다릅니다.
- 경제 재시작 자체는 `OnFestivalStarted → StageEconomyBridge → RestartFestival`로 연결돼 있습니다.
  과거의 “경제 전체가 재시작되지 않는다”와 현재의 “홍보 UI 캐시가 갱신되지 않는다”는 다른 문제입니다.
- 저장·불러오기, 브리지의 홍보 단계 판정, 상세 실패 enum의 위젯 전달, 예상 범위·구매 전 미리보기는 별도 작업입니다.

### ✅ 홍보 — 웹 프로토타입과 코드가 다릅니다 (2026-08-27, 백종태) · **2026-09-23 결정됨**

> ✅ **2026-09-23 결정(김시리).** 프로토타입 방향으로 갑니다 — 4등급, 홍보 없음 = 손해, 플레이어가 매일 입장료를 정함,
> 시작 버즈 있음. 배율은 Agent 풀 때문에 줄였습니다(×0.78~1.46 → ×0.85~1.25). 3일 합계·최대 동시 인원·혼잡 위험 표시는
> 이번 범위에서 뺐습니다. 계약은 위 "🆕 홍보 4등급 · 입장료 · 화제성 · 축제 평가" 절을 보세요.

> ⚠️ **2026-09-18 갱신(김시리).** Buzz(아래 "시작 버즈" 행)는 `FF_FestivalViewModelSubsystem.h`
> 주석의 "Utility AI 점수 항"·`economy_spec.md`의 "공연 부가효과"와 셋이 같은 값을 가리키는지도
> 아직 미확인입니다.

예전에 웹으로 만든 `홍보 프리뷰` 프로토타입이 지금 코드와 **정면으로 다릅니다.**
어느 쪽이 맞는지 정해 주셔야 UI 를 그쪽에 맞춥니다. UI 가 밸런스 값을 지어낼 수는 없습니다.

| 항목 | 웹 프로토타입 | 지금 코드 |
| --- | --- | --- |
| 등급 수 | **4개** | 3개 (`안 함` 포함) |
| 등급·비용·배율 | 홍보 없음 `0` **×0.78** · 지역 SNS `600` ×1.00 · 전국 홍보 `1,400` ×1.22 · 인플루언서 `2,600` ×1.46 | 안 함 `0` ×1.00 · 기본 `500` ×1.08 · 집중 `1,200` ×1.18 |
| 시작 버즈 | 등급마다 `+0 / +6 / +14 / +26` | ❌ Buzz 개념 자체가 없음 (9월 투입 예정) |
| 입장료 | **플레이어가 슬라이더로 정한다** | `AdmissionFeeGold = 5` **고정값**. setter 없음 |
| 예상 총 방문객 (3일 합계) | 표시함 | 하루 단일 예상치는 구현됨. 3일 합계 API·표시는 없음 |
| 예상 입장료 수입 | 표시함 | ❌ 없음 |
| 예상 최대 동시 인원 + 혼잡 위험 | 표시함 (`334`, `혼잡 위험 높음`) | ❌ 없음 |

**특히 갈리는 것 두 가지입니다.**

1. 🔴 **`홍보 없음` 이 ×0.78 입니다.** 프로토타입에서는 홍보를 **안 하면 손해**고 지역 SNS 가
   기준선(×1.00)입니다. 지금 코드는 안 함이 ×1.00 기준이고 홍보가 보너스입니다.
   같은 화면이라도 플레이어가 받는 인상이 완전히 다릅니다 — "안 사면 손해" vs "사면 이득".
2. 🔴 **입장료를 플레이어가 정하는가.** 지금 헤더 주석에 `고정값이라 화면에 슬라이더를
   두지 않는다` 고 박혀 있습니다. 플레이어가 정하는 것으로 가면
   **`SetAdmissionFee(int32)` Command + 변경 델리게이트**가 필요하고,
   프로토타입 문구대로 `입장료가 높으면 수익은 오르지만 방문객이 줄어듭니다` 라는
   **수요 곡선**도 누군가 소유해야 합니다.

**결정이 프로토타입 쪽이면 필요한 것** (위 `홍보 구매·예상 방문객 API`에 더해서):

| 필요한 것 | 비고 |
| --- | --- |
| `FFF_PromotionTier` 에 등급 4종 + `StartingBuzz` | 배율도 프로토타입 값으로 |
| `SetAdmissionFee(int32)` + `OnAdmissionFeeChanged` | 범위(min·max)도 함께 주세요 |
| 예상 방문객 — **입장료와 홍보 등급을 넣으면 나오는 함수** | 위 `예상 방문객 (범위)` 요청과 같은 것. 다만 **현재값이 아니라 가정값으로 물을 수 있어야** 합니다 — 슬라이더를 움직이는 동안 미리보기를 그려야 하므로 |
| 예상 최대 동시 인원 + 혼잡 위험 구간 | 판정 기준을 함께 주세요. Zone `SoftCapacity` 대비인지 Fruin LOS 인지 |

**UI 쪽 준비 상태** — 홍보 패널은 이미 있고 등급 카드·견적·확인창까지 동작합니다.
위 값들이 오면 `UFF_FestivalViewModelSubsystem` 안쪽과 카드 필드만 늘리면 됩니다.
슬라이더와 예상치 3칸은 자리를 비워 두겠습니다.

**소유자분들께** — 만드실 때 아래 두 가지만 지켜 주시면 UI가 바로 붙습니다.

1. 현재값 Getter + 변경 델리게이트를 **쌍으로** 제공
2. 값의 **단위와 범위**를 헤더 주석에 명시 (0~100인지 0~1인지, 초인지 분인지)

---

## 축제 종료·최종 결산 — UI 가 해야 할 일 (2026-10-03 코드 대조)

> 작성: 김시리. 산출(경제·평가)과 종료 이벤트는 코드에서 닫았고, 아래는 **화면이 읽거나 불러야 하는 것**입니다.
> 이 절이 최신입니다. 앞 절의 "마지막 날 Report 이후" 표현은 이 절로 대체됩니다.

### 3일차 종료 흐름 (바뀐 것)

`DT_DaySequence` 에 `Day3_Report` 행이 **없어서** 예전에는 `OnFestivalEnded` 가 한 번도 울리지 않았습니다
(최종 결산 화면·클리어 기록·마지막 SNS 글이 전부 불발). 마지막 날은 **마감이 마지막 화면**이라는 UI 규칙
(`FF_LiveHudRules`)에 맞춰, 종료를 "축제 마치기" 버튼이 일으키게 했습니다.

```text
3일차 Closing ─ "축제 마치기" ─▶ UFF_UIFlowSubsystem::RequestFinishFestival()
   └ UFF_FestivalStageSubsystem::FinishFestival()   (3일차 Closing/Report 에서만 통하고 한 번만)
        └ OnFestivalEnded  → FinalReportScreenClass 올림 / 클리어 기록 저장 / 마지막 SNS 글
   └ 최종 결산 화면이 올라갔으면 거기서 대기, 안 올라갔으면(클래스 미배선) 곧장 나라 지도로
최종 결산 화면의 "로비로" ─▶ UFF_UIFlowSubsystem::RequestFinishFestival()  (이미 끝났으므로 나라 지도로 복귀)
```

| 항목 | 위치 | 비고 |
|---|---|---|
| `FinishFestival()` | `UFF_FestivalStageSubsystem` | BlueprintCallable. 성공하면 true |
| `HasFestivalEnded()` | `UFF_FestivalStageSubsystem` | BlueprintPure. 최종 결산이 떠 있는 동안 true |
| `RequestFinishFestival()` | `UFF_UIFlowSubsystem` | BlueprintCallable. **최종 결산 화면의 `로비로` 는 이것을 부르면 됩니다** |

- `FinalReportScreenClass` 는 지금 `WBP_FF_FlowStub`(임시)입니다. 실제 화면으로 교체하면 임시 분기
  (`FF_UIFlowStubScreen` 의 `bFinalReport`)는 지워도 됩니다.
- 3일차 Report 단계는 쓰지 않아도 됩니다. Report 행이 생기더라도 같은 `FinishFestival()` 로 한 번만 울립니다.
- 새 축제를 시작하면(`OnFestivalStarted`) 만족도·욕구 집계·SNS 피드·배속·날씨·미니게임 상태가 **초기화**됩니다.
  화면은 `OnFestivalStarted` 이후 다시 읽으면 됩니다.

### 최종 결산 화면이 읽을 값 (산출은 전부 구현됨 — 호출만 하면 됩니다)

| 보여줄 것 | 읽는 곳 | 비고 |
|---|---|---|
| 등급 S~D, 총점, 4축 점수, 한 줄 평 | `ViewModel::GetFestivalEvaluation()` | **실시간 계산**입니다. 화면을 열 때 한 번 읽어 고정하세요 |
| 날짜별 만족도 | `ViewModel::GetDailySatisfactionHistory()` | |
| 누적 방문객 | `ViewModel::GetTotalAdmittedVisitors()` | |
| 누적 매출, Gold | `ViewModel::GetTotalRevenue()`, `GetGold()` | |
| 원인별 매출·지출, 부스별 매출, 환급 | `Progression::GetEarnRowsForDay / GetSpendRowsForDay / GetRevenueByFacilityRowsForDay / GetTotalRefundForDay (Day)` | 원장은 이제 끝난 날을 모두 보관합니다. 3일 합계는 아래 `ForFestival` 표 |

### 10/3 추가 — 최종 결산에서 쓸 수 있게 된 것

| 항목 | 위치 | 비고 |
|---|---|---|
| 축제 전체(3일 합계) 결산 표 | `Progression::GetEarnRowsForFestival / GetSpendRowsForFestival / GetRevenueByFacilityRowsForFestival / GetTotalRefundForFestival` | `ForDay` 표와 같은 모양. 끝난 날 원장을 **전부** 보관하게 바꿨으므로 Day1 도 `GetEarnRowsForDay(1)` 로 읽힌다(이전엔 2일치만) |
| 평가가 굳는다 | `GetFestivalEvaluation()` / `IsFestivalEvaluationFrozen()` | `OnFestivalEnded` 이후엔 그 순간 값으로 **고정**. 새 축제에서 풀림. 화면에서 따로 캐시할 필요 없음 |
| 날짜별 만족도 3칸 | `GetDailySatisfactionHistory()` | 종료 시 마지막 날 값이 들어가 3칸이 된다 |
| 예보의 평판 배수 | `FFF_VisitorForecast.ReputationMultiplier` | 어제 만족도 → ×0.75~1.25(2026-10-09 전 ×0.88~1.08. 50 에서 1.0, 첫날 1.0). 예상 방문객에 이미 반영됨. 미리보기 문구에 "어제 만족도 영향" 줄을 넣을 수 있다 |
| 클리어 기록에 평가 | `UFF_SaveSubsystem::GetRecord(FestivalId)` → `BestScore`, `BestGrade`, `BestSatisfaction`(평가의 날짜별 평균) | `HasClearedFestival(FestivalId)` 는 국가 지도의 해금/뱃지 표시에 쓸 수 있다. 지금은 호출하는 UI 가 없다 |
| 이어하기 후 결산 | — | 끝난 날 원장·시설별 누적 매출이 저장에 담긴다. 이어하기 직후 "어제 결산"이 비지 않는다 |

### 아직 화면이 없는 것 — 호출·구독하는 UI 가 0 인 항목

| # | 화면 | 쓰는 API | 영향 |
|---|---|---|---|
| 1 | **피날레 구매 패널** (3일차 MorningCheck) | `RequestPurchaseFinale`, `GetFinaleTiers`, `CanPurchaseFinaleNow` | 없으면 피날레 보너스·불꽃이 영구히 꺼져 있습니다. Finale 탭은 "만드는 중" |
| 2 | **입장료 슬라이더 + 예상 방문객 미리보기** | `RequestSetAdmissionFee`, `CanSetAdmissionFeeNow`, `GetAdmissionFeeRange`, `PreviewVisitorForecast`, `GetVisitorForecast` | 없으면 입장료가 기준값(지금 3) 에 고정되어 평가의 재무·방문객 축이 플레이어 입력과 무관해집니다 |
| 3 | **화제성 게이지** | `GetBuzz`, `GetBuzzChangeToday`, `OnBuzzChanged` | |
| 4 | **일일 목표 진행도** | `GetAdmittedVisitorsToday`, `IsDailyGoalBonusPaid`, `OnAdmittedVisitorsTodayChanged` | 지금은 `Debug_SetVisitorCount` 로만 채워져 늘 "아직 알 수 없습니다" |
| 5 | **마감 리포트 보강** | 결산 표 5종(위), `GetDayLedger` | 지금 패널은 매출·Gold·만족도·스태프 수만 보여줍니다. 지출·순이익·방문객·목표 달성이 없습니다 |
| 6 | **스태프 승급** | `RequestPromoteStaff`, `CanPromoteStaff`, `GetStaffRoster`, `OnStaffRosterChanged` | 승급은 Gold 를 빼지 않습니다(비용 미정, `economy_todo` E1) |
| 7 | 준비 바 "예상 N~M명" | `GetExpectedVisitors` | `Debug_SetPlaceholders` 로만 채워져 항상 숨겨짐 |

### 화면에 나가는 잘못된 값·문구 — 고쳐야 합니다

| 위치 | 문제 |
|---|---|
| `FF_LivePanelReport.cpp:20,173` | "원인 집계가 붙으면 여기에 들어갑니다" 가 그대로 노출됨. "피날레·연출 보정"의 연출 보정은 정의가 없음 |
| `FF_LivePanelFinale.cpp:18` | "피날레는 셋업에서만 고를 수 있다" 는 틀렸음. 실제 구매 시점은 **3일차 MorningCheck** |
| `ViewModel` `Safety` | 소유자가 없어 **88 고정**. 평가에도 안 들어갑니다. 실값처럼 보이지 않게 숨기거나 문구로 알려 주세요 |
| `FF_LiveHudStatusBar` | `bUsePlaceholderResourceValues=true` 기본값(관객 284 등 가짜값). 지금은 참조하는 WBP 가 없어 노출되지 않음 |
| `UFF_ReportScreen` | 사용하는 WBP 가 없는 죽은 코드. 최종 결산 화면을 새로 만들 때 흡수하거나 지우세요 |

### 10/9 추가 — 피날레 SNS 후기 · 오늘 목표 · 재무 점수 · 1차 밸런싱 (김시리)

#### `UFF_FestivalViewModelSubsystem::OnFinaleSnsPostAdded`

- **헤더**: `Public/UI/ViewModel/FF_FestivalViewModelSubsystem.h`
- **접근**: ViewModel 델리게이트 (`BlueprintAssignable`)
- **시그니처**: `FFF_OnSnsSocialPostChanged OnFinaleSnsPostAdded` — 인자 `FFF_LiveSocialPost NewPost` (`OnSnsPostAdded` 와 같은 타입)
- **갱신 주기**: 이벤트 — 피날레 단계 동안 글이 하나 생길 때마다. 평균 간격은 구매 등급에 따라 미구매 1.6 / C 1.2 / B 0.8 / A 0.55 / S 0.35초(±40%)
- **상태**: `구현완료`
- **비고**: 처음 6초는 기대감 글(화제 17), 그 뒤는 구매 등급 평가(18 미구매 · 19 C · 20 B · 21 A · 22 S). 미구매·C 는 부정, B 이상은 긍정입니다.
  흘려보내는 용도라 **ViewModel 은 목록을 들고 있지 않습니다** — 지난 글이 필요하면 아래 `GetFinalePosts()` 를 읽으세요.
  피날레 글은 5건에 1건만 일반 SNS 피드(`OnSnsPostAdded`)에도 들어가고, **화제성에는 반영되지 않습니다.** 불꽃이 끝나고 10초(연출이 없으면 20초) 뒤 멈춥니다.

#### `UFF_CrowdInsightsSubsystem::GetFinalePosts()`

- **헤더**: `Public/NPC/Insights/FF_CrowdInsightsSubsystem.h`
- **접근**: `GameInstanceSubsystem`
- **시그니처**: `const TArray<FFF_SnsPostRow>& GetFinalePosts() const` (`BlueprintPure`). 원본 델리게이트는 `OnFinalePostAdded`
- **갱신 주기**: 요청 시 (최신이 `[0]`, 최대 400건)
- **상태**: `구현완료`
- **비고**: 새 축제를 시작할 때 비웁니다. 위젯은 원칙대로 ViewModel 의 `OnFinaleSnsPostAdded` 를 구독하고, 결산 화면 등에서 지난 피날레 글을 다시 보여 줄 때만 이걸 읽으세요.

#### `UFF_FinaleSnsOverlay` — 이미 화면에 붙어 있습니다

- **헤더**: `Public/UI/LiveHUD/FF_FinaleSnsOverlay.h`
- **상태**: `구현완료` — **`UFF_LiveHudScreen` 이 만들어 뷰포트(ZOrder 15)에 올립니다.** WBP 없이 C++ 로 트리를 만듭니다.
- **비고**: 화면 왼쪽 아래 라이브 채팅 줄(반투명 띠, 작성자 11px + 본문 13px, 왼쪽 2px 반응 색 띠, 폭 340, 최대 6줄). `OnFinaleSnsPostAdded` 를 직접 구독하고,
  피날레 단계를 벗어나면 비우고 숨깁니다. 전체가 `HitTestInvisible` 이라 월드 클릭을 막지 않습니다. 모양은 `MaxEntries` · `ListWidth` · `BodyFontSize` 등 `EditDefaultsOnly` 값으로 조정합니다.
  확인은 콘솔 `FF.SNS.Finale <None|C|B|A|S>`.

#### 바뀐 값·규칙 — 화면에 영향 있는 것만

| 항목 | 지금 | 화면에서 할 일 |
|---|---|---|
| `GetDailyVisitorGoal()` (ViewModel·Progression) | **오늘 목표**. Day N = 900 + 100 × (N−1) → 900 / 1000 / 1100 (`DailyVisitorGoalGrowthPerDay`, `GetDailyVisitorGoalForDay`) | 날이 바뀌면 다시 읽기. 고정값으로 캐시하지 않기 |
| 재무 점수 (`FFF_FestivalEvaluation::FinanceScore`) | 100 × (최종 Gold − 시작 Gold) / `FinanceTargetProfitGold`(15,000, 잠정), 0~100. 본전·손해면 0 | 평가 화면 설명 문구가 "원금 대비"라면 "목표 이익 대비"로 |
| 방문객 점수 | 분모가 날짜별 목표의 합(`VisitorGoalTotal`) | — |
| 피날레 만족도 보너스 C/B/A/S | +3 / +6 / +10 / +15 (가격 2,000/4,000/6,000/8,000 그대로) | 숫자를 복제하지 말고 `GetFinaleTiers()` 로 읽기. 2차 조정에서 가격도 바뀔 예정 |
| 부스 수익 | `BoothCommissionRate` 0.12 → **1.0**. 부스 매출 전액이 축제 몫이고, `DT_FacilityRevenues` 모든 행이 행별 덮어쓰기 꺼짐 | 화면 문구 **"수수료" → "부스 매출"**. 남은 곳: `GetEarnReasonText(FacilityCommission)`("부스 수수료"), 준비 화면 예상 안내("부스 수수료와 미니게임 보상은…"), 시설 정보 예상 수익 툴팁("현재 수수료율…"), 매출 탭 빈 상태("…수수료가 들어옵니다"), 조합 보너스 표기("수수료 +N%") |
| 기본 스태프 | `InitialStaffCount` 6 → **1** (메인안내소 필수 배정만, 사건 대응은 고용 450) | `최대 N명` 은 `기본 1 + 고용 상한` |
| 입장료 기준값 | 5 → **3** (범위 0~15 그대로) | `GetAdmissionFeeRange` 로 읽기 |
| 평판 배수 | ×0.75~1.25 | — |
| 화제성 | SNS 글 1건 최대 0.3, 하루 감쇠 40% | — |
| 대기열 | `UFF_QueueSubsystem` 이 축제 배속을 따름. 공연 휴지 구간은 줄 인내 판정에서 빠짐 | 예상 대기시간·처리량 표시는 배속과 무관하게 Stage 시간 기준 |


---

## 변경 이력

당시 작업 기록입니다. 현재 연결 상태와 남은 요청은 위 본문을 기준으로 확인합니다.

| 날짜         | 작성자 | 내용                                                                                             |
| ---------- | --- | ---------------------------------------------------------------------------------------------- |
| 2026-10-09 | 김시리 | 🆕 피날레 SNS 후기 계약(`ViewModel::OnFinaleSnsPostAdded`, `CrowdInsights::GetFinalePosts`, LiveHUD 가 만드는 `UFF_FinaleSnsOverlay`)과 1차 밸런싱 반영("10/9 추가" 절). `GetDailyVisitorGoal()` 은 오늘 목표(900/1000/1100), 재무 점수 = 100 × 이익 / `FinanceTargetProfitGold`, 피날레 보너스 +3/+6/+10/+15, 부스 수익 100%(화면 문구 "수수료" → "부스 매출"), 기본 스태프 1명, 입장료 기준 3, 평판 ×0.75~1.25, 화제성 감쇠 40% |
| 2026-09-28 | 김시리 | 입장료·예보·화제성·일일 목표·축제 평가·스태프 승급을 **ViewModel 에 전부 중계**("✅ ViewModel 경로" 절). 입장료 변경 명령 `RequestSetAdmissionFee` 신설, 표시 문자열 4종 추가. SNS `{시설}` 문장은 화제 1·7·10·14 에서 실제 시설 이름으로 나옵니다(아래 9/16 기록의 전부 제외 서술은 지난 것) |
| 2026-09-23 | 김시리 | 홍보 기획 결정 반영(4등급·홍보 없음 ×0.85). 매일 입장료 설정·예보 미리보기·화제성(Buzz)·축제 평가·일일 목표 진행도 API 와 필요한 화면 5가지를 "🆕 홍보 4등급 · 입장료 · 화제성 · 축제 평가" 절에 정리 |
| 2026-09-02 | Codex | 홍보 구매·예상 방문객 원본과 UI/스폰 미연동 경계를 반영. 가용 스태프·프로그램 패널·표시 문자열·Crowd 테스트 상태 정정. 예약자 전용 및 실제 참여 인원 미노출, 재시작 홍보 캐시 주의사항 추가 |
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
| 2026-08-19 | 백종태 | 전체 사이클 UI 프로토타입(타이틀→피날레→최종결산)을 한 바퀴 돌려 보고 **없어서 더미로 채운 것 8건**을 "UI가 필요한데 아직 없는 것"에 추가. `FFF_DayLedger` 노출 형태 질문에 회신(원인별 표·부스별 표 **둘 다** 필요, 환급은 분리, 하루 경계 스냅샷 필요). 시설 유지비에 대응하는 `EFF_SpendReason` 부재 보고 |
| 2026-08-19 | 김시리 | **결산 표 4종 구현 완료** — `GetEarnRowsForDay`/`GetSpendRowsForDay`/`GetRevenueByFacilityRowsForDay`/`GetTotalRefundForDay` + `HasLedgerForDay`. 요청하신 형태에 **날짜 인자**가 붙었습니다(진행 중인 하루와 방금 끝난 하루를 안쪽에서 고릅니다). 부스별만 `FFF_FacilityRevenueRow` 로 분리 — 경제가 시설의 한글 이름을 모릅니다 |
| 2026-08-19 | 김시리 | 🔴 **정정: enum 의 `DisplayName` 을 직접 읽으면 패키징 빌드에서 영문이 뜹니다.** 8/18 에 "피날레 실패 이유가 한글로 온다"고 안내한 것이 틀렸습니다. `UFF_EconomyDisplayTextLibrary` 신설 — 피날레 실패 이유·등급 표시 두 군데를 이 함수로 바꿔 주세요. Stage 단계 이름도 같은 문제입니다(강민서) |
| 2026-08-24 | Codex | 타깃 슈팅 레퍼런스 UX 반영 — 정밀도·반응속도·골든 타깃 점수, 즉시 명중 피드백, 커스텀 조준점, TPS/명중률/평균 반응시간 HUD와 결과 요약, Rules/Visual Style 분리 튜닝 가이드 추가 |
| 2026-08-26 | Codex | 타깃 슈팅을 물총 Aim Trainer 방식으로 교체 — 실제 WBP·DataAsset 재생성, 1인칭 물총/반동/총구 섬광, 물방울 탄환 비행, 중앙 집중형 HUD, 저장 에셋 자동화 검사 추가 |
| 2026-08-26 | Codex | 타깃 슈팅 3D 사격장 전환 — UMG 실시간 3D viewport, 원근 카메라·조명·사격장, 이동식 금속 과녁판, 3D 물 탄환, 전용 lit 재질과 2D 자동 폴백 추가 |
| 2026-08-27 | Codex | 타깃 슈팅 하이브리드 정리 — 3D는 배경 전용, 판정과 정확히 겹치는 2D 과녁·탄환/수명 드레인, 발사 중 과녁 예약, 놓친 과녁 지표, Data Asset 한국어 편집명·툴팁·미니게임별 클리어 골드 추가 |
| 2026-08-23 | 김시리 | ✅ **부스별 매출 표가 실제 값으로 찹니다.** NPC 파티가 부스 이용을 마치면(`Served`) 수수료가 원장에 들어갑니다. `FacilityId` 는 `FacilityDefinitionId` 라 Building 카탈로그 키와 같습니다 — 표시 이름을 그대로 해석하실 수 있습니다. **같은 정의의 부스 여러 개는 한 줄로 합쳐집니다**(인스턴스별 구분은 v1). 화면의 `잠정`·`더미` 배지를 떼셔도 됩니다 |
| 2026-08-23 | 김시리 | ✅ **E-26 시설 유지비 종료 — 기획서에 없는 개념입니다.** `EFF_SpendReason` 에 사유를 추가하지 않습니다. 마감 화면 원장에서 **유지비 줄을 빼 주세요**. `FacilityInstall` 로 묶이는 것이 아니라 지출 자체가 발생하지 않습니다 |
| 2026-08-23 | 김시리 | 🔴 **표시 문자열 정정이 아직 안 붙었습니다** — `FF_FestivalViewModelSubsystem.cpp:354`·`FF_LiveHudMenuBar.cpp:168` 두 곳입니다. 패키징 빌드에서만 드러나니 시연 전에 정리 필요 |
| 2026-08-23 | 김시리 | 🔧 전체 자동화 테스트가 크래시로 중단되던 것을 고쳤습니다(피날레 콘솔 명령 수명 문제, 장원재 님 파일·별도 통지) |
| 2026-08-24 | 김시리 | 🔴 **셋업 예산 표시 요청 — A(예산 내역) + B(상단바 고정비·미납) 두 개면 닫힙니다.** 둘 다 **경제 쪽 추가 작업 0건**입니다. **B가 A보다 급합니다** — 미납은 동작하는 기능인데 화면에 나오는 곳이 0곳이라 플레이어가 조용히 파산합니다 |
| 2026-08-25 | 김시리 | 🆕 **유료 프로그램 편성이 구현됐습니다 — 화면이 필요합니다**(담당 백종태). Setup·MorningCheck 에서 그날 열 프로그램을 고르는 리스트 + 버튼입니다. 아침에 Gold 와 인원을 걸면 **그 프로그램을 좋아하는 손님이 정원만큼 축제에 옵니다** — 체험비보다 **모객**이 본체라 카드에 "손님 N명이 옵니다"를 꼭 넣어 주세요. 순익은 범위가 아니라 **숫자 하나**입니다. 예약 4건 요청 중 마지막 ③이 이걸로 닫힙니다. ⚠️ `TryScheduleProgram` 이 **시설 핸들**을 받습니다 — 플레이어가 시설을 고르는 흐름이 필요합니다. 취소 버튼은 안 만들었습니다(하루짜리라 다음 날 자동으로 비워집니다) |
| 2026-08-25 | 김시리 | 🔴 **인원 표시는 `GetTotalStaffCount()` 가 아니라 `GetAvailableStaffCount()` 입니다.** 프로그램이 인원을 묶어서, 전체 인원만 띄우면 플레이어가 Event 에 6명을 다 쓸 수 있다고 착각합니다. 갱신은 `OnScheduledProgramsChanged` 도 같이 받아야 합니다 |
| 2026-08-26 | 김시리 | 🔴 **체험비를 걷는 시점을 바꿨습니다 — Closing 이 아니라 프로그램이 열리는 시각입니다.** 8/24 안내를 정정합니다. 카드마다 시각이 달라 Live 중에 나눠 들어옵니다. **마감 화면만 보시면 하실 일 없습니다**(마감 합계는 같습니다). Live 중 갱신을 반영하시려면 `OnGoldChanged`/`OnTodayRevenueChanged` 구독 |
| 2026-08-26 | 김시리 | 🆕 **`CanScheduleProgramNow()` 추가 — 프로그램 편성 화면의 단계 판정입니다.** `TryScheduleProgram` 은 단계를 안 봐서 Live 중에도 편성이 됩니다(그러면 비용만 빠지고 체험비 0). 피날레의 `CanPurchaseFinaleNow()` 와 같은 모양입니다. ⚠️ 허용 단계는 **Setup·MorningCheck** 이고 시설 배치(`IsReconfigureAllowed()`)와 달리 `PromotionPreview` 는 포함하지 않습니다 |
| 2026-08-26 | 김시리 | 🔴 **정정 — 프로그램 카드 시각을 UI 에서 변환하지 말아 주세요.** 앞서 "변환 지점을 한 곳으로 두시라"고 안내했는데, 그러면 개장 시각 `17` 이 UI 에 복제됩니다(Stage 안에 "표로 빼기" todo 가 붙은 값입니다). **지금은 상대 표기(`개장 +30분`)로 해주시고**, 강민서 님께 요청한 `GetLiveClockStringAt()` 이 오면 `17:30` 으로 바꾸시면 됩니다. 참고로 공방은 17:30~18:00, 요가는 18:30~19:00 입니다 |
| 2026-08-26 | 김시리 | 문서 정리 — 종료된 협의 경위와 뒤집힌 초안 서술을 걷어내고 **지금 필요한 것만** 남겼습니다. 셋업 예산 제안의 C·D 안은 문서에서 뺐습니다(연출이 필요해지면 말씀 주세요). 프로그램 편성 절에 **용어 표**(카드 / 편성 / 예약)를 넣었습니다 — 구조체가 셋이라 헷갈리는 지점입니다 |
| 2026-08-26 | 백종태 | ✅ **8/24 제안의 B(상단바 고정비·미납)를 구현했습니다** — U4 준비 단계 하단 HUD(`UFF_PrepHudBar`)에 `축제 · 금화 · 밀린 임대료 · 관객 · 스태프 · 일차/단계 · 예상 방문객 · CTA` 를 넣었습니다. 미납은 **잔액 바로 옆**이고 0이면 칸을 숨깁니다. 미납·스태프 2종을 ViewModel 에 중계 연결했습니다(경제 쪽 추가 작업 0건). 새로 필요한 것은 **예상 방문객(범위)** 하나입니다 |
| 2026-08-26 | 백종태 | 🔴 **문서 내부 모순 정정 — 날씨는 이미 읽고 쓸 수 있습니다.** `필요한데 아직 없는 것` 표의 `날씨 Read Model ❌ 없음` 행이 낡았습니다(같은 문서 `환경(날씨·야간)` 절에 API가 이미 있음). 지웠던 테스트용 날씨 버튼(`BP_StageTest`)이 부르던 것이 바로 `SetSelectedWeather` 였고, **날씨 선택은 테스트 기능이 아니라 준비 단계의 플레이어 결정**이라 U5 점검 패널로 정식 이관합니다 |
| 2026-08-26 | 백종태 | ✅ **U5 인력 패널을 실물로 붙였습니다 — 더미 0건.** `GetAffordableTemporaryStaffCount` · `TryHireTemporaryStaff` · `MaxHiredStaffPerDay` · `DailyStaffWageGold` · `DailyVenueRentGold` 를 ViewModel 로 중계했습니다. **`TryHireTemporaryStaff` 가 부분 고용이 없어서** 화면은 낙관적 갱신을 하지 않고 결과 이벤트만 봅니다. 거부 사유가 bool 하나라 화면에서 `상한 도달`·`잔액 부족`을 구분하려고 `GetRemainingTemporaryStaffCap()` 을 따로 만들었습니다 — 경제 쪽 `GetRemainingHireCap()` 이 private 이라 같은 식을 UI 에서 한 번 더 씁니다. **public 으로 열어 주시면 그 중복을 지웁니다** |
| 2026-08-26 | 백종태 | ✅ **U5 점검 패널(개장 견적서)을 붙였습니다.** 날씨 선택이 정식 자리를 얻었습니다 — `SetSelectedWeather` 가 `IsReconfigureAllowed()` 로 준비 단계에만 열려 있어서 준비 단계 UI 가 맞는 자리입니다. 🔴 **`OnEnvironmentStateChanged` 가 야간 전환에도 울립니다** — `(Weather, bIsNight)` 를 함께 쏘기 때문입니다. ViewModel 에서 날씨가 실제로 바뀔 때만 중계하도록 걸렀습니다. 강민서님 쪽 수정은 필요 없습니다. 남은 요청은 여전히 **예상 방문객(범위)** 하나이고, 없으면 견적표의 수입·보너스 칸이 `아직 알 수 없습니다` 로 나옵니다 |
| 2026-08-26 | 강민서 | ✅ **날씨 Read Model 표 정정 확인했습니다.** `OnEnvironmentStateChanged`가 야간 전환에도 함께 울리는 부분, ViewModel에서 필터링해주셔서 제 쪽 추가 수정 없습니다. U5 점검 패널 연동 감사합니다 |
| 2026-08-26 | 강민서 | ✅ **`GetLiveClockStringAt(float LiveMinutes)` 요청 구현 완료 — `GetLiveClockStringFromMinutes(int32 LiveMinutes)`로 만들었습니다.** 요청과 시그니처가 달라 확인 필요합니다: 파라미터가 `float`가 아닌 `int32`이고, 함수명도 다릅니다. 통일 필요하시면 말씀 주세요. `GetCurrentLiveClockString()`을 이 함수 호출 한 줄로 리팩터링하는 것도 확정되면 같이 진행하겠습니다 |
| 2026-08-26 | 강민서 | 🔴 **`FF_FestivalViewModelSubsystem.cpp:354` · `FF_LiveHudMenuBar.cpp:168` 표시 문자열 건 — 제 담당 함수는 이미 있습니다.** `EFF_FestivalStage` 표시 이름은 `GetCurrentStageDisplayName()`(UMETA DisplayName 기반, 패키징 빌드 안전)이 원본입니다. 해당 두 파일이 이 함수를 안 쓰고 자체 문자열을 쓰는 것으로 보여, 제가 직접 두 파일을 고치기보다 각 파일에서 이 함수를 호출하도록 바꾸시길 제안드립니다. enum에 없는 특수 케이스가 필요하면 알려주시면 추가하겠습니다 |
| 2026-08-26 | 김시리 | ✅ `GetLiveClockStringFromMinutes(int32)` Dev 병합 확인. 프로그램 카드 시각은 상대 표기 임시안 대신 이 API로 `HH:MM` 표시합니다 |
| 2026-08-26 | 김시리 | 최신 UI 코드 확인 — Gold는 ViewModel이 `OnGoldChanged`를 구독해 연결돼 있습니다. 아직 남은 것은 누적/오늘 매출 연결, 인원 칸을 `GetAvailableStaffCount()` 로 바꾸는 것, 프로그램 편성·피날레 구매 화면입니다. Day1 Setup에서도 알바 고용 가능으로 확정됐습니다 |
| 2026-08-27 | 강민서 | 🔴 **8/26 답변 정정 — `GetCurrentStageDisplayName()`이 실제로 UMETA 직접 읽기 문제였습니다.** 하드코딩 switch문으로 교체 완료, 빌드 확인함 |
| 2026-08-28 | 김시리 | ✅ **Need 지표가 이제 실제로 움직입니다** — `UFF_NpcNeedMetricsSubsystem` 에 표본을 넣는 코드를 붙였습니다(`AdvanceNeeds()` 순회에 얹음). `GetNeedUnmetRatio()`·`GetNeedAverage()` 가 더 이상 항상 0 이 아닙니다. **갱신은 1초(30스텝)마다**이고 `OnNeedUnmetRatioChanged` 는 값이 바뀐 Need 에 대해서만 옵니다. ⚠️ **`GetSampleCount() == 0` 은 여전히 "데이터 없음"이고 0% 와 다릅니다** — Live 단계 밖에서는 집계가 안 돌아 마지막 값이 멈춰 있습니다. 표본은 파티가 아니라 **개인** 단위입니다 |
| 2026-08-28 | 김시리 | 🔴 **2회차 축제를 시작하면 경제가 리셋되지 않습니다.** 경제의 `StartFestival(Definition)` 이 **`Initialize()` 에서 한 번만** 불려서, GameInstance 가 살아 있는 한 다시 안 돕니다 — Gold·원장·미납·인원·예약이 1회차 마지막 값으로 남습니다. **제 쪽에서 닫겠습니다**(리셋 함수는 이미 완성돼 있고 부를 자리만 없었습니다). ❓ **여쭙고 싶은 것**: `UFF_UIFlowSubsystem` 에서 **프론트엔드 복귀 후 축제를 다시 시작하는 흐름이 지금 실제로 도는지**입니다. 도는 상태면 발표 전에, 아니면 발표 뒤에 닫겠습니다 |
| 2026-08-28 | 김시리 | ✅ **메인 무대가 공연 시간에만 후보로 열립니다.** Live 전반에는 `Performance_MainStage_*` 가 NPC 목적지 후보에서 빠져 있다가 공연 시작 시각에 열리고, 그 순간 유휴 파티가 무대로 몰립니다. **버스킹·체험 무대는 상시 열려 있습니다.** 화면 쪽 작업은 없습니다 — 다만 "무대에 아무도 없다"가 버그가 아니라 의도라는 점만 알아 두시면 됩니다 |
| 2026-08-27 | 김시리 | **코드 전수 대조 + 문서 정리.** 안 맞는 것을 문서 끝 `아직 안 맞는 것` 표 하나로 모으고, 이미 닫힌 항목의 경위 서술을 걷어냈습니다. 실제로 바뀐 사실 4개: ① **표시 문자열은 5개 파일에 남아 있습니다**(지목했던 2곳은 줄 번호가 낡았고, 그중 4건은 Stage enum 이 아니라 탭·옵션 enum 이라 강민서 님 수정과 무관합니다) ② **`FFF_CrowdReadModel` 은 스텁이 아니라 실값이고 셀이 2m 입니다**(5m 가정이면 Heat Map 색이 틀립니다) ③ 인원 칸에 **가용이 아니라 보유**가 붙어 있습니다 ④ 프로그램·피날레는 `EFF_PrepHudTab` 에 탭 자리가 이미 있고 패널 본체만 없습니다. **경제 쪽 추가 작업은 0건이고, 제 숙제는 `AddSample` 호출부입니다** |
| 2026-08-28 | 강민서 | ✅ **김시리님 질문 관련 — `OnFestivalStarted` 델리게이트로 대신 구독하시는 걸 제안합니다.** `StartNewFestival()` 호출 시 Day/Stage/시계 초기화 후 마지막에 `OnFestivalStarted.Broadcast()`가 발동하도록 만들어 뒀습니다. 경제 쪽 리셋 함수를 `UFF_UIFlowSubsystem`이 직접 호출하는 방식 대신, 여기 구독해서 자체 리셋을 걸어두시면 "부를 자리가 없다"는 문제가 UI Flow 흐름 여부와 무관하게 해결될 것 같습니다. 구독 방법: `GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>()->OnFestivalStarted.AddDynamic(this, &YourClass::YourResetHandler);` 이 방식이 맞을지 확인 부탁드립니다. |
| 2026-08-28 | 강민서 | 🆕 **미니게임 진행 중 1배속 고정 API 추가 — `SetMinigameInProgress(bool)` / `IsMinigameInProgress()`.** 팀 결정대로 미니게임 플레이 중에는 배속 버튼을 눌러도 무조건 1배속으로 고정되도록 `Tick()`의 배속 계산에 반영했습니다. 미니게임 시작 시점에 `true`, 종료 시점에 `false`로 호출해주시면 됩니다. 다만 **이 함수를 실제로 호출하는 연결부는 아직 없습니다** — 원재/광훈 쪽 미니게임(사격 게임 등) 시작/종료 지점에 이 호출을 걸어주시는 작업이 필요합니다. |
| 2026-08-29 | 백종태 | 🆕 **라이브 HUD 요청 7건.** 문서 끝 `라이브 HUD 를 만들며 나온 요청 7건` 절에 적었습니다 — 누적 방문객(이정원)·시설 표시 이름(한광훈)·`CompletedCount`(한광훈)·큐 id 다리(장원재)·줄 포기 손님(장원재)·시설별 스태프 배치(김시리)·입력 모드 정책(카메라). ⚠️ **동시 관객 수는 요청 대상이 아닙니다** — `GetActiveAgentCount()` 가 이미 있어서 상태 바에 붙였고, 누적 방문객과는 **다른 값**입니다 (동시 인원을 일일 목표 분자로 쓰면 진행률이 안 올라갑니다) |
| 2026-09-02 | 백종태 | 🆕 **요청 8번 추가 — 마감·준비 단계 배속(강민서).**
| 2026-09-08 | 김시리 | ⚠️ **필수 설치 시설 게이트 — 계약 변경 2건.** `IsCurrentStageSkippable()` 이 필수 시설 조건도 보게 되어 **무대가 없으면 스킵 버튼이 비활성됩니다**(`CanSkipCurrentStage` 가 그대로 중계). **`RemainingTime == 0` 이 오래 지속되는 상태가 생겼습니다**(값 범위는 그대로, 0 에서 머무는 시간이 길어진 것). 신규 API: `CanTransitionTo` · `GetStageAdvanceBlockingReason`(Stage 절) / `UFF_RequiredFacilitiesSubsystem` · `OnFestivalDefinitionChanged`(김시리 절). 막힌 사유는 종태님이 만들어 두신 `Text_CtaReason`(CTA 버튼 왼쪽, AT-011 대비) 자리에 값을 흘려 넣어 **이미 표시됩니다** — 부탁드리는 것은 PIE 확인 하나입니다. 탐색을 더 빠르게 하는 백로그 3건(카탈로그 느낌표·`00:00` 상태 표시·카드 진행도)은 급하지 않으며 김시리 절에 정리했습니다. Stage 서브시스템은 강민서 님 소유이므로 함께 확인 부탁드립니다 `TimeScale` 이 `Tick` 의 Live 분기에서만 쓰여서, 마감·리포트·셋업·개장 준비에는 **배속 버튼을 못 답니다**(눌러도 안 되는 버튼이 됩니다). 비-Live 분기에도 적용해 주시면 UI 는 버튼만 열면 됩니다. ❓ **개장 준비는 기획상 시간이 멈추는 단계인데 코드는 흐릅니다** — 어느 쪽이 맞는지 확인 부탁드립니다. ✅ 그리고 **마지막 날 배속 잠금은 UI 쪽 착오였습니다** — 웹 프로토타입 규칙을 옮긴 것이고 소유자 코드에는 없어서 지웠습니다 |
| 2026-09-15 | 김시리 | ✅ **`SimSatisfaction` 더미 해제.** 대기·밀도·취향매칭·Need 충족도 4항을 2단 계층으로 산출하는 실값입니다. 분해값 4종(`GetSimSatisfactionWaitScore/DensityScore/PreferenceScore/NeedFulfillmentScore` + 각 `OnXxxChanged`)도 ViewModel에 있습니다. `ScoreSatisfaction` 합산(FinaleBonus 더하기)은 아직 더미입니다 |

| 2026-09-17 | 한광훈 | 🆕 **시설 정보 패널에 `시설 수익` 카드 추가.** 설치 시설 **한 곳**의 오늘/누적 매출·결제 건수·오늘 매출 비중을 실값으로 보여 줍니다. 원본은 경제에 가산한 `GetFacilityInstanceRevenue(FGuid)` + `OnFacilityRevenueChanged` (김시리 절에 항목 추가, 기존 `AddRevenue`·결산 표 4종은 그대로). 부스 수수료·프로그램 체험비가 `AddFacilityRevenue` 로 시설 GUID 까지 함께 기록됩니다. WBP 는 `FF.MigrateFacilityRevenueSection` 으로 기존 디자이너 에셋에 카드만 비파괴 추가하며, 이후 스타일·라벨은 Designer 에서 수정합니다 |

| 2026-09-16 | 김시리 | 🆕 **SNS Read Model 값 쪽 완성 — UI 연결만 남았습니다.** 위 B표 항목 참조. `UFF_CrowdInsightsSubsystem`이 3~10초마다 화제·문장을 결정론적으로 골라 `FFF_SnsPostRow`를 만들고, `FF_FestivalViewModelSubsystem::GetRecentSnsPosts()`/`OnSnsPostAdded`가 Gold·SimSatisfaction과 같은 방식으로 `FFF_LiveSocialPost`를 중계합니다. **부탁드리는 것 두 가지**: ① `FF_LivePanelSNS`가 `Debug_SetPosts` 대신 이 두 API를 구독하도록 바꿔주세요(구조·감정 표현은 그대로 `FFF_LiveSocialPost`라 위젯 트리는 안 건드리셔도 될 것 같습니다) ② 화면에 몇 건까지 보여줄지·스크롤 정책은 UI 쪽 판단으로 정해주세요. `{시설}` 문장은 `FFFZSFacilityReadModel`에 시설 표시 이름이 실리기 전까지 후보에서 뺐습니다(한광훈 님 쪽 스레딩 필요, 요청 #2와 같은 건) |


| 2026-09-21 | 김시리 | 🔴 **임대료·미납 제거 + Staff 영구 고용 + 프로그램에서 Staff 분리.** 화면에서 **빼셔야 하는 것**: ① 미납 관련 전부(`GetTotalArrears`·`OnTotalArrearsChanged` **삭제**) ② `가용 인원` 개념 전부(`GetAvailableStaffCount`·`GetStaffAssignedToPrograms`·`OnAvailableStaffCountChanged` **삭제**) — 프로그램이 인원을 안 쓰므로 `GetTotalStaffCount()` 하나면 됩니다 ③ 프로그램 카드·패널·시설 피커의 `필요 인력` 칸(`FFF_ProgramCard::RequiredStaff` **필드째 삭제**). **이름이 바뀐 것**: `TryHireTemporaryStaff`→`TryHireStaff`, `GetAffordableTemporaryStaffCount`→`GetAffordableHireCount`, `GetTemporaryStaffCount`→`GetHiredStaffCount`, `DailyStaffWageGold`→`StaffHireCostGold`(150/일 → 450/1회), `MaxHiredStaffPerDay`→`MaxHiredStaffCount`(**하루 상한이 아니라 축제 총원 상한**). `EFF_SpendReason::StaffWages` 와 결과 enum 3종의 `NotEnoughStaff` 도 삭제됐고 뒤 값이 재번호됐습니다. ✅ 8/26 요청하신 `GetRemainingHireCap()` public 전환 완료 — UI 쪽 중복 식 지우셔도 됩니다. ⚠️ **WBP 자산 9종은 재생성했습니다**(`FF.RebuildPrepHudStaffUI`·`FF.RebuildLiveHudStaffUI`·`FF.RefreshFacilityPaidProgramUMG`) — 죽은 칸이 자산에만 남아 `밀린 임대료 미납 2,400` 이 화면에 나오던 문제입니다. 스태프 강화(승급)는 **세부 미정이라 이번에 안 넣었습니다** |
| 2026-09-22 | 김시리 | 🆕 **스태프 강화(승급) 규칙 확정 + API 완성.** 위 「스태프 강화(승급)」 절 새로 씀. 핵심 3개: `GetStaffRoster()`(개인별 명단, `FFF_StaffMember{StaffId, TrainingLevel, HiredOnDay, LastPromotedDay, bIsBaseStaff}`) · `TryPromoteStaff(int32 StaffId)`(승급 실행) · `OnStaffRosterChanged`(고용·승급 둘 다 이걸로 옴 — 승급은 `OnTotalStaffCountChanged` 로 안 옵니다, 인원수가 안 바뀌니까요). 규칙: 고용 다음 날 아침부터 승급 가능(당일 불가), 한 아침에 1인당 1단계만, 최대 Lv3. **⚠️ 아직 체감 효과는 없습니다** — 승급해도 사건 시스템(이정원)이 없어서 게임 안에서 달라지는 게 없습니다. 그래도 화면은 지금 만드셔도 나중에 API가 안 바뀝니다. 사건별 배치(`스태프 N명 배치`)는 여전히 이번 범위 밖 — §6 참고 |
| 2026-10-03 | 김시리 | 🔴 **3일차 종료 이벤트 복구 + 최종 결산 UI 계약.** `DT_DaySequence` 에 `Day3_Report` 가 없어 `OnFestivalEnded` 가 안 울리던 문제를 `FinishFestival()`/`RequestFinishFestival()` 로 해결(「축제 종료·최종 결산」 절). 최종 결산 화면의 `로비로` 는 `RequestFinishFestival()`. 새 축제 시작 시 만족도·욕구·SNS·배속·날씨 초기화. 화면이 아직 없는 7건과 잘못 나가는 문구 목록 수록 |
| 2026-10-07 | 한광훈 | 🆕 **필수 시설에 메인안내소(스태프 배정 시설) 1곳 추가 + 배정까지 필수.** `UFF_FestivalDefinition` 기본 `RequiredSetupFacilities` 에 `Info_GeneralInfo_General` 1개(`MinimumAssignedStaff = 1`)를 더했습니다 — 설치하고 스태프를 1명 이상 `+ 배정`해야 Day 1 시작·개장이 열립니다. `FFF_RequiredFacilityRow` 에 `RequiredStaffCount`·`AssignedStaffCount`·`MissingStaffCount` 추가, `OnRequiredFacilitiesChanged` 가 스태프 배정·해제에도 울립니다. 화면 반영: 하단 바·점검 패널 사유(`+ 배정` 안내), 점검 패널 필수 시설 줄(`! 스태프 배정 필요 (배정 0/1명)`)과 인력 줄(`· 배정 N명`), 카탈로그 카드 툴팁(설치 후 배정 필요), 시설 정보 스태프 영역(`! 필수 배정` 줄). 카탈로그 `필수 · 1일차 시작 전 설치` 범례는 이제 **설치가 남았을 때만** 보입니다 |
