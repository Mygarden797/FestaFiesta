# 스태프 ↔ 사건 시스템 연동 안내

정원님께 전달할 요청과 변경 사항입니다. 갱신: 2026-09-22.

> 오늘 스태프를 "사건 대응 전담 축"으로 확정하면서(설계는 `staff_event_design.md`),
> 사건 시스템 착수 전에 **인터페이스부터 맞춰두면 서로 안 기다려도 된다**고 판단해
> 데이터 계약과 판정 순수 함수를 먼저 만들었습니다. 사건의 발생·저작·배치 우선순위는
> 전혀 건드리지 않았습니다 — 그건 전부 정원님 몫입니다. 여기는 "투입된 인원으로
> 무엇이 나오는가"만 계산합니다.

## 확정된 스태프 규칙 (요약)

- 영구 고용. 1회 구매(450골드/명), 축제 끝까지 유지. 시설·매출엔 안 관여합니다.
- 상한: 기본 6명(무료) + 고용 최대 6명 = 총원 12명.
- 사건 투입 모델(하한/정원): 사건마다 `최소 인원`(미달이면 착수 불가) ~ `최소 인원+2`(정원).
  정원 안은 능력치 그대로, 초과분은 절반만 인정.
- 개인별 숙련도(1~3). 고용 다음 날 아침부터 승급 가능, 한 아침에 1인당 1단계만.
  → Day1 고용은 Lv3까지, Day2는 Lv2까지, Day3는 Lv1에 머뭅니다.
- 배치는 수동 기본 + 자동은 방치된 사건만 줍는 안전망(심각도→남은시간→발생순, 하한만 채움) —
  이 부분은 전적으로 정원님 설계 영역입니다. 저는 판정 함수만 제공합니다.
- 시간 초과 페널티는 진행률 비례(`최대치 × (1−진행률)`), 초과 순간 1회만 계산.
- Gold 직접 차감 페널티는 없습니다(팀 결정) — 페널티는 만족도 쪽으로 갑니다.

## 오늘 새로 만든 것 — 사건 시스템이 바로 쓸 수 있는 재료

### 1. `FFF_FestivalEventCard` — 데이터 계약

`Source/FestaFiesta/Public/Economy/FF_FestivalEventTypes.h`

```cpp
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FestivalEventCard
{
    FGuid EventId;
    FName EventType;    // 화재·미아·쓰레기 등 — 값 목록·저작은 정원님 몫
    FName ZoneId;        // 발생 구역. ZoneSystem 의 존 식별자와 타입이 맞는지 확인 부탁드립니다
    int32 Severity = 1;
    int32 MinStaff = 1;  // 착수 하한
    int32 MaxStaff = 3;  // 정원. 기본 MinStaff+2, 종류별로 다르게 저작 가능
    float WorkAmount = 0.f;        // ⚠️ 아래 참고 — 초가 아니라 "사람-분" 단위입니다
    float TimeLimitSeconds = 0.f;
    int32 MaxOverduePenalty = 0;   // 시간 초과 시 페널티 최대치
};
```

사건 발생·수명주기 로직은 여기 없습니다. 이 구조체를 채워서 아래 순수 함수에 넘기면
능력치 합·소요시간·페널티가 나옵니다.

### 2. `FFF_StaffMember` — 명단 한 명

`Source/FestaFiesta/Public/Economy/Rules/FF_StaffPool.h`

```cpp
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_StaffMember
{
    int32 StaffId = INDEX_NONE;
    int32 TrainingLevel = 1;        // 1~3
    int32 HiredOnDay = 0;           // 기본 팀은 0
    int32 LastPromotedDay = INDEX_NONE;
    bool bIsBaseStaff = false;
};
```

### 3. `FFF_StaffEventRules` — 판정 순수 함수 4개

`Source/FestaFiesta/Public/Economy/Rules/FF_StaffEventRules.h`. 월드 없이 검증됩니다
(기존 `FFF_*Rules` 방식 그대로) — 사건이 실제로 안 터져도 단위 테스트로 돌아갑니다.

```cpp
static float CalcAbilitySum(const TArray<FFF_StaffMember>& Assigned, int32 MinStaff);
static float CalcResolveSeconds(const FFF_FestivalEventCard& Card, float AbilitySum);
static bool  CanPromote(const FFF_StaffMember& Member, int32 CurrentDay);
static int32 CalcOverduePenalty(const FFF_FestivalEventCard& Card, float Progress01);
```

- `CalcAbilitySum` — 내부에서 레벨 내림차순으로 정렬한 뒤 계산합니다. **호출부가 순서를
  신경 쓸 필요가 없습니다** — 어떤 순서로 배열을 넘겨도 결과가 같습니다(결정론은 이 함수가
  보장합니다).
- `CalcResolveSeconds` — **⚠️ `WorkAmount` 는 "사람-분" 단위입니다, 초가 아닙니다.**
  `(작업량 ÷ 능력치 합) × 60`으로 계산합니다. 처음에 이 ×60을 빼먹은 채로 짰다가 전체
  자동화 스위트를 돌려서 잡았습니다 — 사건 카드를 저작하실 때 `WorkAmount=12`처럼 "사람이
  느끼는 분량"으로 넣으시면 됩니다(예시: 대형 화재 작업량 12 ÷ 능력 3.0 = 4분).
- `CanPromote` — 승급 가능 여부만 봅니다. 사건과는 무관하고, Progression 쪽 승급 API가
  내부에서 이걸 씁니다.
- `CalcOverduePenalty` — 진행률(0~1)을 넘기면 클램프합니다. 초과 순간에 **한 번만** 불러야
  합니다 — 여러 번 부르면 그때마다 다시 계산되어 이중으로 물립니다.

### 4. `UFF_FestivalProgressionSubsystem` 의 새 API

```cpp
UFUNCTION(BlueprintPure) TArray<FFF_StaffMember> GetStaffRoster() const;
UFUNCTION(BlueprintCallable) bool TryPromoteStaff(int32 StaffId);
UPROPERTY(BlueprintAssignable) FFF_OnStaffRosterChanged OnStaffRosterChanged;
```

`GetStaffRoster()`가 지금 배치 가능한 전체 인원(기본+고용)을 돌려줍니다. 사건 시스템이
"이 사건에 누구를 투입할지" 배정하고 나면, 배정된 부분집합을 `CalcAbilitySum`에 넘기면
됩니다 — Progression은 "누가 있는가"만 알고 "누가 어느 사건에 배정됐는가"는 모릅니다.
그 배정 상태는 사건 시스템 쪽에서 들고 계시면 될 것 같습니다.

## 변경 접점

| 파일 | 변경 |
|---|---|
| `FF_FestivalEventTypes.h` | 신규. `FFF_FestivalEventCard` |
| `FF_StaffPool.h/.cpp` | 인원수 하나 → `FFF_StaffMember` 배열. 기존 집계 API(`GetBaseStaffCount` 등) 시그니처는 그대로 유지 |
| `FF_StaffEventRules.h/.cpp` | 신규. 판정 순수 함수 4개 |
| `FF_FestivalProgressionSubsystem.h/.cpp` | `GetStaffRoster()`·`TryPromoteStaff()`·`OnStaffRosterChanged` 추가. `TryHireStaff`가 고용일(`HiredOnDay`)을 남기도록 변경 |

시설·매출·기존 고용 로직(`TryHireStaff`, 총원 상한 판정)은 동작을 바꾸지 않았습니다.
기존 호출부는 전부 그대로 동작합니다.

## 정원님께 여쭙는 것

1. **`ZoneId`(FName) 가 ZoneSystem 쪽 존 식별자 타입과 맞는지 확인 부탁드립니다.** 제가
   임의로 FName으로 잡았는데, 다른 타입(예: FGuid, 정수 인덱스)이 표준이면 맞추겠습니다.
2. **사건 발생/완료를 Economy 쪽에 알릴 방법.** 스태프 쪽에서 직접 알 필요는 없지만,
   나중에 만족도 페널티(§5, ⓐ다음날 방문객 배수 + ⓑ3일 평균)를 걸 때 "사건이 미해결로
   닫혔다"는 신호가 필요합니다. 기존 `UFF_CrowdEconomyBridgeSubsystem` 같은 브리지
   서브시스템 패턴을 재사용하면 될 것 같은데, 발행 시점(사건 종료 즉시 vs 하루 경계)은
   설계에 맞춰 정하겠습니다.
3. **`Safety` 지표의 소유권.** `FF_FestivalViewModelSubsystem.h:240`에 소유자가 이미
   "이정원"으로 적혀 있는 걸 확인했습니다(지금은 하드코딩 88). 사건 처리율 같은 걸로
   실값을 채우실 계획이면 제 쪽(경제)은 손 안 대고 기다리겠습니다.
4. `AgentPoolSize` 500 고정 — 기존에 전달드린 건 그대로 확인 부탁드립니다
   (`FF_SpawnActor.h:48`).

## 검증

전체 자동화 스위트 717 통과 / 65 경고 포함 통과 / 2 실패. 실패 1건은 위 단위 버그였고
수정 후 재확인했습니다. 남은 1건(`Crowd.Arrival.RuntimeTablesSurviveFestivalGarbageCollection`,
Agent 풀 소진)은 기존부터 있던 것이고 이번 작업과 무관합니다. `FestaFiesta.Economy` 카테고리
전체(오늘 변경분 포함) 161/161 통과.

새로 추가한 테스트
- `Economy.StaffEventRules.*` — 능력치 합·소요시간·승급 가능 여부·페널티, 문서 §4
  "대형 화재" 예시 표 그대로 고정
- `Economy.DayBoundary.PromotionRespectsHireDayAndPersists` — 고용 당일 승급 불가,
  하루 1회, 저장·복원 후 레벨 유지

## 참고 자료

- [스태프↔이벤트 설계 정리](../SiriKim_Docs/staff_event_design.md) — 하한/정원 모델·개인별
  승급·만족도 페널티 루프 전체 설계 (⚠️ 이 파일은 Git 제외 폴더라 로컬에만 있습니다.
  필요하시면 내용을 별도로 전달드리겠습니다)
- `Source/FestaFiesta/Public/Economy/FF_FestivalEventTypes.h`
- `Source/FestaFiesta/Public/Economy/Rules/FF_StaffEventRules.h`
- `Source/FestaFiesta/Public/Economy/Rules/FF_StaffPool.h`
