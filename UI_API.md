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

| 멤버 | 형 | 설명 |
| --- | --- | --- |
| `CurrentStage` | `EFF_FestivalStage` | 현재 Stage |
| `RemainingTime` | `float` | 남은 시간(초) |
| `CurrentDay` | `int32` | 1~3일차 |
| `CurrentSlot` | `int32` | 1~4 타임슬롯 |
| `OnStageChanged` | `FOnStageChanged` | `(PreviousStage, NewStage)` 2인자 |
| `RequestTransition(EFF_FestivalStage)` | | Stage 전환 요청 |
| `IsReconfigureAllowed()` | `bool` | 배치 변경 허용 여부 |

**UI 사용처**: 상단 바(Stage/Day/Slot/시계), Morning Check 진입 판정, 배속 제한 구간

**요청** — `RemainingTime`이 매 틱 바뀌므로 UI가 폴링해야 합니다.
초 단위로 바뀔 때만 쏘는 델리게이트가 있으면 좋겠습니다. 없으면 UI 쪽에서 0.1초 폴링 + 변화 감지로 처리하겠습니다.

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
`State`, `bNeedsRelocation`, `QueueState`, `WaitingCount`, `ActiveServiceCount`, `AverageCompletedWaitSeconds`

**`FFFZSZoneReadModel`** — `ZoneId`, `SoftCapacity`, `ReportedPopulation`, `DensityRatio`, `DensityState`

> ⚠️ **`DensityRatio`는 Heat Map 색상에 쓰지 않는다.**
> `ReportedPopulation / SoftCapacity`이고 SoftCapacity는 기획자가 정한 정원이라 면적 기반이 아니다.
> Fruin Level of Service(인/㎡)와 직접 비교할 수 없다. Heat Map은 Crowd 쪽 `DensityPerSqm`을 쓴다.

### 델리게이트

| 이름 | 인자 | 용도 |
| --- | --- | --- |
| `FFFZSWorldChangedSignature` | `FFFZSWorldChangeEvent` | 월드 변경 알림 |
| `FFFZSPlacementRejectedSignature` | `FFFZSCommand`, `FFFZSCommandResult` | 배치 거부 — **UI가 실패 사유를 표시** |

**UI 사용처**: Overview 시설 상태, 대기열 길이, Morning Check 배치, 실패 사유 토스트

---

## Queue — 장원재

### `UFF_QueueSubsystem`

- **헤더**: `Public/Queue/FF_QueueSubsystem.h`
- **상태**: `구현완료` (P0)

| 델리게이트 | 인자 수 |
| --- | --- |
| `FFF_OnQueuePartyJoined` | 4 |
| `FFF_OnQueuePartyRejected` | 3 |
| `FFF_OnQueueServiceStarted` | 3 |
| `FFF_OnQueuePartyExited` | 4 |
| `FFF_OnQueueSlotTargetsUpdated` | 2 |

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

### 표시 명칭 편집 창구

- **경로**: `data/ui/archetype_label_overrides.csv`, `companion_label_overrides.csv`
- **반영**: `node Tools/data/build_unreal_datatables.mjs` → UE에서 DataTable Reimport
- **상태**: `구현완료`

UI 담당이 `ui_override_label` 열만 채운다. 비우면 엔지니어링 가칭이 그대로 쓰인다.

### 아직 없는 것

`UFF_FestivalProgressionSubsystem`(경제·Gold·일차 결과·저장)이 아직 없습니다.
아래 "UI가 필요한데 없는 것" 표를 봐 주세요.

---

## UI Read Model — 백종태

### `UFF_FestivalViewModelSubsystem`

- **상태**: `계획` — 8/8까지 스텁 커밋 예정
- **역할**: 위 모든 소스를 UI가 쓰기 좋은 형태로 합쳐서 제공

UI 위젯은 **이 서브시스템만 참조**하고 다른 시스템을 직접 보지 않습니다.
각 모듈이 바뀌어도 위젯을 안 고치게 하기 위해서입니다.

### 이미 만든 UI 베이스 (참고용)

| 클래스 | 헤더 | 용도 |
| --- | --- | --- |
| `UFF_UIRootLayout` | `Public/UI/FF_UIRootLayout.h` | HUD·Screen·Panel·Toast 레이어. `PushScreen`/`PopScreen` |
| `UFF_ActivatableScreenBase` | `Public/UI/FF_ActivatableScreenBase.h` | 화면 베이스. 입력 모드·초기 포커스 |
| `UFF_CommonButtonBase` | `Public/UI/FF_CommonButtonBase.h` | 버튼 베이스 |
| `LogFestaFiestaUI` | `Public/UI/FF_UILog.h` | UI 로그 카테고리 |

다른 분이 UI를 만들 일이 있으면 이 클래스들을 부모로 쓰시면 됩니다.
**시간 정지 권한은 UI에 없습니다** — 화면을 열어도 시뮬레이션은 계속 흐릅니다.

---

# UI가 필요한데 아직 없는 것

기획서 6.3의 Overview 필수 표시 항목입니다.

| 필요한 것 | 예상 소유자 | 상태 | UI에서 쓰는 곳 |
| --- | --- | --- | --- |
| **Satisfaction** (`SimSatisfaction` 0~100) | 김시리 · 이정원 | ❌ 없음 | 상단 지표, Report, SNS |
| **Revenue / Gold 잔액** | 김시리 | ❌ 없음 | 상단 지표, 구매 UI 가능 여부 |
| **Safety** | 이정원 | ❌ 없음 | 상단 지표, 경고 |
| Staff 보유·가용 인원 | 김시리 | ❌ 없음 | Event 배치 UI |
| `CauseId` 롤업 | 김시리 | ❌ 없음 | Report 드릴다운 (AT-005) |
| 저장 슬롯 목록 | 김시리 | ❌ 없음 | 플레이 허브 |

**UI는 위 항목들을 하드코딩 더미로 먼저 만들어 8/8에 커밋합니다.**
실제 값이 생기면 `UFF_FestivalViewModelSubsystem` 안쪽만 교체하고 위젯은 그대로 둡니다.

**소유자분들께** — 만드실 때 아래 두 가지만 지켜 주시면 UI가 바로 붙습니다.

1. 현재값 Getter + 변경 델리게이트를 **쌍으로** 제공
2. 값의 **단위와 범위**를 헤더 주석에 명시 (0~100인지 0~1인지, 초인지 분인지)

---

## 변경 이력

| 날짜 | 작성자 | 내용 |
| --- | --- | --- |
| 2026-08-06 | 백종태 | 문서 신설. 현재 리포에서 확인한 API를 초기 채움 |
