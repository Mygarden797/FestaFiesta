# 유료 프로그램 예약 시스템

- 갱신: 2026-09-21 — PR #217의 Staff 분리와 현재 하루 편성·예약 수명 반영（Crowd 실행 경로 대조는 2026-09-03 기준）
- 관련: `../request/facility_naming_convention.md` 5.4, `utility_ai_spec.md` §2·§9, [UI_API.md](../../../UI_API.md)의 시설 기반 유료 프로그램 편성·Staff 고용 절
- 배경: 시설 태그에 `예약 필요`가 있는데 Utility AI 가 이 태그를 모르고 NPC 가 그냥 들어간다

## 1. 확정된 규칙

**현재 구매 계약(2026-09-21)**: 설치 시설에 **오늘 하루치** 프로그램을 편성하며, 소모 자원은 **Gold뿐**이다. Staff는 사건 대응 전담으로 프로그램의 구매 조건·배정 자원이 아니다. 다음 날에는 편성 목록과 해당 예약 슬롯을 함께 제거하므로 같은 카드를 다시 구매할 수 있다. 고용한 Staff는 하루 경계에서 유지된다.

（2026-09-03 코드 대조: 표의 “사면 즉시 찬다/반드시 시작 전 입장”은 기획 의도다. 코드는 구매 때 슬롯 등록·차감, 예약 명단 생성·그룹 스폰 시 배정 확정으로 나뉜다. 풀 부족·경로 실패가 있을 수 있어 구매 성공만으로 물리적 정시 도착을 보장하지 않는다.）

| #   | 규칙                                             | 왜                                                |
| --- | ---------------------------------------------- | ------------------------------------------------ |
| 1   | 플레이어가 **Setup·MorningCheck 에 설치 시설을 골라 오늘 하루치 프로그램을 산다** | 현재 시설 기반 편성 계약                                         |
| 2   | **사면 정원이 즉시 찬다.** 플레이어가 개별 NPC 를 배정하지 않는다      | 구매는 "그날 방문할 NPC 를 확보하는 것"이다                      |
| 3   | **예약 NPC 외에는 그 시설에 못 들어간다**                    | `예약 필요` 태그의 뜻                                    |
| 4   | **배정 단위는 그룹**이다. 1인도 그룹이라 예외가 없다               | 그룹은 통째로 움직이는 것이 전제(스폰·이동·퇴장 전부)                  |
| 5   | **정원을 그룹 크기 조합으로 정확히 채운다** (10명 = 5+2+2+1)     | 덜 차면 플레이어가 산 것보다 적게 받는다. 경제가 매출을 예측하려면 확정값이어야 한다 |
| 6   | **초과는 허용하지 않는다**                               | 시설 정원은 안전 제약이기도 하다                               |
| 7   | **배정된 그룹은 시작 시각 전에 반드시 장내에 있어야 한다**            | 정원이 비는 상황을 만들지 않는다                               |
| 8   | **확정 뒤로는 프로그램이 끝날 때까지 퇴장하지 않는다**               | 사용자 확정. §3                                       |
| 9   | **프로그램 중에도 Need 는 그대로 쌓인다.** 체류 예산 드레인만 멈춘다    | §3                                               |
| 10  | 시작 전에는 **다른 NPC 와 똑같이 자유 행동**한다                | 강제 대기는 부자연스럽다                                    |

### 기각한 안 (다시 꺼내지 않기 위해)

| 안                              | 기각 이유                                |
| ------------------------------ | ------------------------------------ |
| 정원 이하로 채우고 남는 자리는 비운다 (8/6 초안) | 5번과 충돌. 매출이 비결정적이 된다                 |
| 정원을 개인 단위로 쪼개 정확히 맞춘다          | "그룹은 통째로 움직인다" 전제를 깬다                |
| 프로그램 중 체류시간이 끝나면 중간 퇴장         | 그룹 단위라 최대 6명이 한꺼번에 빠져 정원이 빈다         |
| 예약 확정 시 체류 예산을 미리 늘려둔다         | 퇴장 금지(§3)가 이미 같은 규칙을 보장해 중복이었다       |
| 늦은 그룹을 프로그램에서 빼고 정원을 다시 채운다    | 정원이 확정값이어야 매출을 계산할 수 있다. 늦으면 늦게 참여한다 |

## 2. 자료구조와 순수 함수 (구현 완료)

`FF_ProgramReservation.h` — 상태를 갖지 않고, 같은 입력이면 항상 같은 결과다.
예약 목록·파티 역색인은 `UFF_ProgramReservationSubsystem`이 보관하고 구매·편성·경제 처리는 `UFF_FestivalProgressionSubsystem`이 담당한다(§5).

```text
FFF_ProgramReservation          시설·시작·종료·정원 + 배정 결과(AssignedGroupIds·인원·빈자리)
FFF_ReservationCandidateGroup   GroupId · MemberCount · PreferenceScore

Assign(InOut, Candidates)       정원에 그룹을 배정한다
IsLockedAt(Reservation, Now)    목적지 선택을 건너뛰어야 하는가
IsExitBlockedAt(Reservation, Now)  퇴장이 금지되는가
ApplyExitGrace(Current, Grace)  종료 시 체류 예산 유예
```

**배정 결과를 예약 안에 넣은 이유**: 처음에는 별 구조체로 뺐는데, 그러면 런타임이
**"이 그룹이 지금 예약 있나?"를 물을 방법이 없다.** 테스트만 통과하고 실제로는 못 쓰는 API 였다.

**시각은 전부 Live 경과분**이다. 절대 시각으로 두면 Live 길이가 바뀔 때 틀린 값이 된다
(`visitor_arrival_curve_spec.md` 와 같은 규칙).

### 2.1 정원 채우기는 greedy 가 아니라 유계 배낭 DP

greedy 는 **정원을 정확히 채우지 못한다.** 정원 8 에 후보가 5·4·4 뿐이면 5 를 먼저 담고
4 가 안 들어가 3 자리를 남기지만 정답은 4+4 다. 규칙 5가 확정이므로 이 실패를 받아들일 수 없다.

그룹 크기(1~6)별로 묶으면 상태 공간이 `정원 × 6` 으로 작아서 후보 수에 지수로 늘지 않는다.
개체 단위 subset-sum 이 비싼 것이고 **크기별로 묶으면 싸진다.**

- 목표는 취향 합 최대화. 각 크기 안에서는 취향 내림차순으로 앞에서부터 쓴다
- 같은 점수면 먼저 계산된 조합을 유지해 결정성을 보장한다(갱신 조건이 `>`)
- 정확히 채우는 조합이 없으면 가장 많이 채우고 `UnfilledSeats` 가 0 보다 커진다.
  호출부가 로그를 남긴다 — 조용히 넘기면 플레이어가 산 정원보다 적게 받은 것을 아무도 모른다

⚠️ **`GroupId` 가 실행마다 같아야 한다.** 정렬 타이브레이커라서, 스폰 순서처럼 프로세스마다
달라지면 **같은 시드인데 예약자가 바뀐다**(8/10 `MakeTraitSeed` 버그와 같은 종류).
v0 은 `PreferenceScore` 가 전부 0 이라 **순서가 전적으로 이 값으로 결정된다.**

## 3. 잠금과 퇴장 금지는 구간이 다르다

**이 둘을 같은 것으로 취급하면 버그다.** 초안이 그래서 자유 행동 구간의 퇴장을 못 막았다.

| 구간                            | 재평가        | 퇴장     | 체류 예산 드레인 | Need 증가 |
| ----------------------------- | ---------- | ------ | --------- | ------- |
| 시작 전 자유활동 | 한다 | **금지** | 돈다 | 쌓인다 |
| 시작 전 복귀·대기 | 일반 재평가 안 함 | **금지** | 돈다 | 쌓인다 |
| 참여 중(도착 후 시작 ~ 종료) | 안 한다 | 금지 | **멈춘다** | 쌓인다 |
| `EndMinutes` 이후               | 한다         | 가능     | 돈다        | 쌓인다     |

**왜 자유 행동 구간이 위험한가** — 그 구간에는 드레인이 계속 돌아서 체류 예산이 시작 전에
마를 수 있다:

```text
예약 확정 10분, 프로그램 100~160분, 체류 예산 150분
자유 행동 90분 동안 배수 2.0 으로 한계 Need 유지 → 180분 소모
  → 시각 85분에 예산 0 → 퇴장 금지가 없으면 시작 전에 나가고 정원이 빈다
```

막는 대상은 **퇴장 판정 두 경로 전부**다 — 체류 예산 소진과 연속 `no_choice`.
예산은 0 에서 멈춘다(음수면 종료 직후 즉시 사라진다). 종료 후 남는 시간은 `ApplyExitGrace` 가
만든다 — **그래서 이탈유예가 실질적 의미를 갖는다.** 기본값 0 이면 예산이 마른 예약자는
종료 시각에 곧바로 퇴장한다.

**잠금 중 드레인을 멈추는 대가**(사용자 확정): 1~2시간 화장실을 못 간 개체가 종료 시점에
Need 한계인데도 예산이 멀쩡해서 한참 더 머문다. 규칙 9(Need 는 계속 쌓임)와 합쳐지면
**종료 직후 화장실·식음료 쏠림이 커진다** — 관측하고 판단하기로 한 사항이다.

**잠금 상태는 selector 밖에 둔다.** `Choose()` 를 부르기 전에 호출부가 거른다 —
selector 에 예약 개념을 넣으면 순수 함수성이 깨진다(지금 selector 가 `bIsOpen` 하나로
폐쇄 이유를 모르는 것과 같은 원칙).

## 4. 자유활동·복귀·대기·참여

`자유활동 → 복귀 → 대기 → 참여 → 자유활동`을 Crowd의 파티별 상태로 관리한다.

- 자유활동: 시작 전 귀환 가능한 일반 시설을 선택한다. 프로그램 종료 전 개인 퇴장은 금지한다.
- 자유활동: 이동·Queue 대기 후 돌아올 시간이 부족한 후보를 제외한다（판정 함수는 이용시간도 받지만 현재 `CandidateUseMinutes` 입력은 생략돼 0이다）. 통과 후보가 없어도 복귀 마감 전에는 배회 후 재탐색한다. 예약 중에는 `no_choice` 횟수를 누적하지 않고 개인 퇴장도 막는다.
- 복귀: Simulation의 고정 스텝에서 `현재 축제분 + 직접 이동 예상분 >= 시작 축제분 - ProgramReturnBufferMinutes`를 검사한다. 여유 기본값은 10축제분이며 음수는 0으로 보정한다. 일반 시설 후보의 귀환 가능 판정에도 같은 값을 적용한다. 마감 도달 시 후보 유무·배회·일반 시설 방문 여부와 무관하게 복귀하며, 일반 Queue에서는 탈퇴한다. Decision의 별도 복귀 의도는 사용하지 않는다.
- 대기: 시설의 저작된 Queue 슬롯을 위치로만 사용한다. 일반 Queue 가입·서비스·포기 타이머는 사용하지 않는다. 대기 중 일반 목적지 재선택은 없다.
- 참여: 도착했고 시작 시각이 지났으면 참여한다. Need는 계속 증가하고 체류 예산 소모만 멈춘다. 늦게 도착하면 늦게 참여한다.
- 종료: 예약 상태를 해제하고 시설 출구로 이동한 뒤 자유활동을 재개한다. 체류 유예는 종료 시 1회 적용한다(`ProgramExitGraceMinutes`, 임시 기본 10축제분).
- 행사 전체 종료는 예약과 무관하게 전원 퇴장시킨다.

시각은 `UFF_FestivalStageSubsystem::TryGetLiveElapsedMinutes`의 Live 경과분이다. `StageSecondsToFestivalMinutes`로 환산하며 현재 1 Stage초 = 1축제분이다.
이동시간에는 리더의 실제 속도를 쓰고 Queue의 실제 대기분도 축제분으로 변환한다.
귀환 거리는 직선거리 근사이므로 장애물·혼잡에 의한 지각까지 보장하지는 않는다. 순간이동은 하지 않는다.

프로그램이 편성된 시설은 그날 예약자 전용이며, 프로그램이 끝나도 일반 후보나 일반 Queue에 개방하지 않는다. 이 판정은 `IsProgramFacility`의 편성 기록이며 시설 카테고리 전체를 막는 게이트가 아니다（DanceBattle 등도 미편성 시 일반 지역 후보. 메인 시간·전역 안내는 SunsetMainStage ID에만 적용）.
대기 슬롯은 예약 파티 ID 순서로 리더에 배정하고 일행은 기존 대형을 유지한다.
슬롯 부족 시 시설 접근점으로 대체한다. 2026-09-02 에셋 확인 기록에서 SandArt·ExperienceStage(DanceBattle)는 각각 12슬롯이었다（이번 갱신은 C++ 기준이며 에셋 인스턴스 값은 재덤프하지 않음）.
슬롯 간격은 각각 90cm·115cm이며, 실제 그룹 대형 간 간격은 플레이 화면에서 확인해야 한다.

참여 중 시설 메쉬 내부로 이동할지, 입구에서 숨길지는 미정이다.
현재 구현은 대기 위치에서 참여 상태를 유지한다.

## 5. 만든 것과 안 만든 것

### 5.0 ✅ 런타임 소유자 — `UFF_ProgramReservationSubsystem` (2026-08-19 신설)

**예약을 들고 도는 곳이 없어 예약이 실제로 존재하지 않던 문제를 풀었다.** GameInstance 스코프이고,
보관·수명주기·조회만 한다(배정 계산과 잠금 판정은 계속 `FFF_ProgramReservationRules` 가 갖는다).

| API | 비고 |
|---|---|
| `RegisterProgram` / `RemoveReservation` | 슬롯 등록·삭제. 저작 오류(정원 0·종료<시작·무효 시설)는 무효 핸들로 거부 |
| `ClearAssignments` | **배정만** 비운다. 슬롯은 남으므로 하루 편성을 종료하는 API가 아니다 |
| `AssignGroups` | 규칙 함수 호출 전에 **다른 예약에 이미 배정된 그룹을 걸러낸다** |
| `GetReservationIds` / `FindReservation` / `GetRemainingSeats` | 목록은 **시작 시각 오름차순** |
| `FindActiveReservationForParty` / `IsPartyLockedAt` / `IsPartyExitBlockedAt` | 결정 계층·퇴장 판정이 쓰는 창구 |
| `OnReservationChanged` | UI 갱신 |

**구매한 프로그램은 하루짜리다.** 슬롯은 구매할 때 만들고, 배정은 이후 예약 명단 생성·그룹 스폰 때 확정한다.
`UFF_FestivalProgressionSubsystem::BeginNewDay`가 전날 편성의 `ReservationId`마다 `RemoveReservation`을 호출한 뒤 편성 목록을 비운다.
슬롯과 배정을 함께 제거하므로 전날 파티의 잠금·퇴장 금지가 다음 날로 넘어가지 않는다. `ClearAssignments`만 호출해 슬롯을 유지하는 기간권 계약은 현재 사용하지 않는다.

> 초안의 `IsGroupLockedAt`·`IsGroupExitBlockedAt`는 이 서브시스템의 `IsPartyLockedAt`·`IsPartyExitBlockedAt`로 실현됐다.
> 이름이 Group→Party로 바뀐 것은 crowdAI의 `PartyID`와 같은 값이기 때문이다. 새 날을 시작할 때 예약 제거는 `RemoveReservation`을 사용한다.

### 5.1 배선 — `UFF_FestivalProgressionSubsystem` (경제 S8)

구매가 Gold 차감이고 기획서 §7 이 경제·진행을 한 소유자로 두므로,
**슬롯 등록과 비용 차감은 함께 성공해야 한다.** 실제 `TryScheduleProgram`은 슬롯 등록 뒤 차감하고 차감 실패 시 슬롯을 되돌린다（그룹 배정·스폰은 이후 단계이며 구매와 같은 트랜잭션이 아님）.

| 현재 API                                                              | 쓰는 쪽                   |
| ------------------------------------------------------------------- | ---------------------- |
| `TryScheduleProgram(ProgramId, FacilityInstanceId)` | 시설 편성 명령이 최종 구매에 사용（초안 `TryPurchaseProgram`을 대체） |
| `GetReservationIds` / `FindReservation` / `OnReservationChanged` | 예약 현황 UI — §5.0의 예약 서브시스템이 제공 |
| `IsPartyLockedAt` / `IsPartyExitBlockedAt` / `FindActiveReservationForParty` | Crowd의 잠금·퇴장·목적지 판정 |
| `RemoveReservation` | `BeginNewDay`에서 전날 구매한 슬롯·배정 제거 |

**실제 구매 순서**: 시설 편성 명령(`UFF_ProgramSchedulingSubsystem`)이 허용 단계·시설 상태·호환 타입·중복·잔액·예약 시간 충돌을 검증한다. 이어 경제의 `TryScheduleProgram`이 카탈로그·중복·잔액·시설 타입을 확인하고 `RegisterProgram` → `TrySpend` → 편성 목록 추가·통지를 수행한다. 차감 실패는 예약 슬롯을 제거한다. Staff 검증이나 차감은 없다.

（초안의 “배정 0명이면 차감 안 함”은 현재 구매 계약과 다르다. 구매 시 그룹 후보를 받지 않고, 이후 예약 전용 명단 생성·스폰 배정으로 정원을 채운다. 풀 부족·배정 실패 처리와 환불 보장은 별도 미완료 사항이다.）

`EFF_ProgramPurchaseResult`는 축제 미시작·알 수 없는 카드·중복·잔액 부족·시설 타입 불일치를 구분한다. `NotEnoughStaff`는 제거됐다. 허용 단계·시설 상태·시간 충돌은 시설 편성 명령의 `EFF_PaidProgramScheduleResult`에서 구분하며, 순수 구매 규칙은 단계나 Staff를 검사하지 않는다.

（2026-09-08부터 예약 귀환·종료 유예는 **CSV가 출처다.** `reservation_return_buffer_minutes`·`reservation_exit_buffer_minutes`(둘 다 10축제분)를 `OnWorldBeginPlay`에서 읽어 `ProgramReturnBufferMinutes`·`ProgramExitGraceMinutes`를 덮는다. 그 전에는 코드 기본값 10만 살아 있고 CSV 행은 0으로 방치돼 아무도 읽지 않았다 — 결선하면서 CSV 값을 10으로 맞춰 동작이 바뀌지 않게 했다. UPROPERTY로 편집한 값은 해당 행이 없을 때만 남는다.

`npc_walk_speed_mps=1.5`는 여전히 이 경로가 읽지 않는다 — 이동 예상은 리더의 실제 속도를 전달한다.）

### 5.2 Crowd 연결

- `FFF_PartyDecisionContext`: 활성 예약과 리더 속도를 전달한다.
- `HandlePartyNeedsDecision`: 예약 전용 시설을 일반 후보에서 제외하고 귀환 후보를 필터링한다.
- `BeginProgramReturn / HandleProgramArrival / UpdateProgramReservationStates / ReleaseProgramParty`: 파티 상태를 전환한다.
- `RequestDecision / ProcessDecisions / AdvanceNeeds`: 상태 잠금, 종료 전 퇴장 차단, 참여 중 체류 예산 정지를 적용한다.
- `TryGetProgramWaitingTransform`: Queue를 점유하지 않고 대기 좌표를 읽는다.

## 6. 합의 필요

| 상대  | 항목                                                                                                      |
| --- | ------------------------------------------------------------------------------------------------------- |
| 이정원 | **유입 곡선에 하드 제약（구현됨: 예약 명단 별도 생성·시작 전 강제 스폰. 용량 부족·도착 시간 보장은 남음）** — "시작 전까지 이 그룹들은 반드시 입장"이 비율 목표 위에 얹힌다. 프로그램이 여러 개면 제약도 여러 개고, 곡선 누적 오차 허용 폭과 충돌할 수 있다 |
| 이정원 | `PartyID`는 예약 배정과 Crowd가 공유한다（같은 시드·명단·생성 순서가 전제이며 실행 전체 결정성 실측은 별도） — §2.1                                                                |
| 한광훈 | `IsProgramFacility`가 편성 기록으로 당일 전용 여부를 판정한다（추상적인 예약 필요 태그의 별도 필드는 사용하지 않음）                                    |
| 한광훈 | **정원의 의미** — 프로그램은 `FFF_ProgramCard::Capacity`, Queue는 대기·서비스 정원, Utility 후보는 Zone SoftCapacity다（세 값을 동일시하지 않음）                                                      |
| 장원재 | 예약자는 일반 Queue 서비스에 가입하지 않는다. 저작된 줄 좌표를 예약 대기로 재사용하며, 시설별 슬롯 수·대형 간격은 확인이 필요하다 |
| 백종태 | 타임테이블 UI — 예약 현황·정원·시간 표시                                                                               |

## 코드 대조 근거 (구매·예약 수명 2026-09-21, Crowd 2026-09-03)

- [FF_FestivalProgressionSubsystem.cpp](../../../Source/FestaFiesta/Private/Festival/FF_FestivalProgressionSubsystem.cpp)
- [FF_ProgramSchedulingSubsystem.cpp](../../../Source/FestaFiesta/Private/Economy/FF_ProgramSchedulingSubsystem.cpp) — 2026-09-21 시설 편성 명령 대조
- [FF_ProgramScheduleRules.cpp](../../../Source/FestaFiesta/Private/Economy/Rules/FF_ProgramScheduleRules.cpp) — 2026-09-21 Staff 없는 구매 규칙 대조
- [FF_ProgramReservationSubsystem.cpp](../../../Source/FestaFiesta/Private/NPC/Party/FF_ProgramReservationSubsystem.cpp)
- [FF_CrowdSimulationSubsystem.cpp](../../../Source/FestaFiesta/Private/CrowdSystem/Systems/FF_CrowdSimulationSubsystem.cpp)
- [FF_CrowdDecisionSubsystem.cpp](../../../Source/FestaFiesta/Private/CrowdSystem/Systems/FF_CrowdDecisionSubsystem.cpp)

（현재 작업 트리 기준. 데이터/계수 수는 저장소 CSV, 설정은 C++ 기본값과 Config를 구분해 기술했다. 설계 제안·과거 실측은 코드만으로 재검증할 수 없어 해당 설명을 보존했다.）
