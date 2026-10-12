# SunsetMainStage 관람 공간 — 팀원 연동 인계

작성일: 2026-09-10 · 구현 범위: 시설 플러그인 / 대상 PDA / 신규 가이드와 시설 도구

[팀 공유 노션 가이드](https://app.notion.com/p/3d796c43a7f78071a5a7ffd485607128)에 시설 작업 순서와 아래 연동 계약을 공유했습니다.

**시설 관람 데이터와 읽기 API는 구현 완료. 전체 NPC 정시 집결 및 1배속 동기화는 연동 대기.** 이번 작업에서는 `Source/FestaFiesta`, 시간표, NPC/Queue/경제 코드, 다른 PDA 및 레벨을 수정하지 않았습니다.

## 1. 시설이 제공하는 계약

설치된 시설의 `FFFZSFacilitySpec.Audience`가 기준입니다. 런타임은 원본 PDA를 다시 읽지 않고 설치된 Spec을 사용합니다. 데이터는 `SaveGame` 태그로 시설 저장 경로에 포함됩니다. 기본값은 비활성이라 기존 시설은 관람 기능이 자동으로 켜지지 않습니다.

추가 편집 기능: 에디터에서 `Audience.Columns`, `Rows`, `AreaLimitM2`를 확정하면 요청 행·열을 보존하며 자동 배치하고, 부족한 관람 면적 한도를 모든 사용 레벨에 필요한 최소 정수 m²로 확대합니다. 보정 원인과 전후 값을 표시한 뒤 `IsDefinitionValid`와 전체 `UEditorValidatorSubsystem` 검증을 실행합니다. 드래그 도중에는 전체 검증하지 않습니다. 파생 값은 같은 Undo 트랜잭션에 포함되고 열린 관람 미리보기도 갱신됩니다. 직접 메모리에 Spec을 대입하는 코드/스크립트에는 UI 동작을 가정하지 말고 명시적으로 검증하세요.

자동 보정으로 해결할 수 없는 오류는 요청 행·열·면적을 에디터 세션의 weak-key map에 보관하고 기존 설치 Spec에는 부분 적용하지 않습니다. 오류를 고친 뒤 `Apply Requested Audience Values`로 현재 geometry에 다시 적용합니다. 재적용도 한 번의 Undo로 복구됩니다. 이 대기 요청은 SaveGame이나 원본 PDA에 저장하는 런타임 데이터가 아닙니다.

`AreaLimitM2`는 관람 영역 전용 한도입니다. 0은 기존 합산 예산, 양수는 해당 시설 관람 영역에 별도 한도를 적용합니다. 최대 50,000m²이며 본체/일반 Queue 예산과 8,192자리 상한은 그대로입니다. 이 필드는 저장 및 명령 fingerprint에 포함되고, 기본 0은 이전 Audience fingerprint를 보존합니다. 업그레이드에서 한도 값은 배율로 늘리지 않습니다.

| API / 타입 | 사용 방법 |
| --- | --- |
| `UFFZSAudienceLibrary::GetFacilityAudienceSnapshot` | WorldContext + 시설 **Instance GUID**로 조회. 운영 중 Active, 재배치 불필요 상태만 성공. 정원·시설 Transform·Layout·입구·출구·Focus·WorldRevision 반환. 실패하면 OutError를 처리합니다. |
| `FFFZSAudienceSnapshot` | 읽기용 값 복사본. 시설 이동·삭제 이후 스스로 갱신되거나 무효화되지 않습니다. NPC 연동자가 수명을 관리합니다. |
| `GetAudienceSlotTransform` | Snapshot + `0..Capacity-1` 자리 번호. 사람 한 명의 **발 위치**와 무대 쪽 Yaw 반환. O(1), 예약과 NavMesh 투영은 하지 않습니다. bool 성공 여부를 확인합니다. |
| `CheckAudienceNavigationBatch` | 명시적 검사 API. 한 호출 1~64자리. 입구/출구/자리를 NavMesh에 투영하고 입구→자리, 자리→출구의 완전한 경로를 검사합니다. 실패 bool, 실패 자리 목록, 오류 텍스트를 함께 확인합니다. |

공개 헤더: `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Public/FF_FFZSAudienceLibrary.h`, `FF_FFZSAudienceLayout.h`.

좌표는 시설 중심 기준 local cm, +X=오른쪽, +Y=무대 앞의 관객 방향입니다. 자리 번호는 앞줄부터 좌→우, 다음 줄 순서입니다. 무대 바라보기는 수평 Yaw이며 애니메이션/상체 시선 처리는 제공하지 않습니다. Actor Scale은 1을 요구하며 시설 크기는 Spec으로 관리합니다.

## 2. NPC 담당자에게 요청할 작업

1. 공연 인스턴스별 집결 상태를 한 곳에서 관리합니다. 일반 Utility 점수만 올리면 전원 집결 조건이 성립하지 않습니다. 집결 대상, 기존 대기열/예약 해제 정책, 종료/취소 시 복귀를 명시합니다.
2. 대상 무대를 Definition ID로 식별한 뒤 **실제 설치된 Instance ID 하나**를 확정합니다. 같은 무대가 여러 개라면 어느 인스턴스로 갈지 명시적으로 선택합니다. 시설 전체 Actor를 매 NPC·매 프레임 검색하지 않습니다.
3. 집결 준비 시 시설 Snapshot을 한 번 받고, `Capacity >= 대상 사람 수`인지 확인합니다. 현재 초기 PDA는 120자리이며 목표 방문객 수에 맞춘 최종 정원은 아닙니다. 정원 초과는 조용히 중복 배정하지 말고 집결 불가/시설 증설 요구로 표시합니다.
4. NPC/파티 시스템에서 **한 사람당 고유 SlotIndex 하나**를 배정·반납합니다. 파티 하나를 한 자리로 세지 않습니다. 파티를 나란히 둘 때 줄 경계와 중앙 통로도 확인합니다. 연속 index라고 모두 같은 줄에서 나란히 붙는 것은 아닙니다.
5. 설정/배치 확정 후 모든 자리를 나눠 경로 검사합니다. 이 검사는 동기식 경로 탐색이므로 한 프레임에 모든 묶음을 몰아 실행하지 않습니다. 공연별 결과를 캐시하고, 시설이나 보행 환경 변경 시 필요한 범위만 다시 검사합니다.
6. 각 구역→관람 입구의 접근 경로는 별도로 검사합니다. 시설 검사 API는 바깥 구역에서 입구까지 오는 길과 군중 병목을 인증하지 않습니다. 실제 Crowd가 사용하는 NavData/필터·HeightField·벽 회피 조건도 확인합니다. 현재 시설 검사 API는 기본 NavData/필터를 사용합니다.
7. 입구를 경유해 배정 자리로 이동시키고 도착 후 Focus 쪽으로 정렬합니다. 반환 좌표는 작성된 발 위치이므로 Crowd의 지면 높이와 투영 규칙에 맞춰 최종 위치를 구합니다. ArrivalTolerance는 시설이 제공하는 기준값이며 이동 판정에 자동 적용되는 것은 아닙니다. 반경과 허용 오차를 초과해 자리를 옮기면 인접 NPC와 간격을 다시 검증해야 합니다.
8. `UFFZSWorldSubsystem::OnWorldChanged`에서 대상 시설 이동·삭제·수명주기 변경 등을 처리하고 `OnWorldSnapshotRestored`에서 저장 복원 이전 Snapshot/자리 소유권을 정리합니다. WorldRevision은 월드 전체의 변경 토큰이므로 모든 NPC가 Tick에서 비교하지 않습니다. 관련 변경에 맞춰 재조회하고, 시작 직전에 시설이 여전히 유효한지 다시 확인합니다.
9. 종료·취소·삭제·재배치 실패 시 배정과 진행 중 경로를 취소하고 자리를 반납합니다. 도착하지 못한 인원은 실패 사유를 집계합니다.

개략 호출 흐름:

```text
시간 담당자의 집결 준비 신호
  → NPC 집결 관리자: 대상 인원 + 공연 + 설치된 시설 Instance ID 확정
  → Get Facility Audience Snapshot (실패 시 진행 중단/오류 표시)
  → 수용 인원 확인 + 전체 자리 경로 검사 묶음 처리
  → 사람별 고유 자리 배정
  → Get Audience Slot Transform → 입구 경유 → 자리 도착 → 무대 방향
  → 공연 종료/취소/시설 변경 → 배정 반납 및 이동 상태 정리
```

새로운 시설 API는 관람 모드의 geometry 계약입니다. 현재 일반 목적지 결정/Queue 흐름을 자동으로 대체하거나 NPC를 생성하지 않습니다.

## 3. 시간 담당자에게 요청할 작업

시설 PDA에 공연 시각이나 별도 타이머를 추가하지 않았습니다. **시간의 기준은 기존 `UFF_FestivalStageSubsystem`과 `DT_ProgramSchedule` 한 곳**으로 유지합니다.

현재 코드에서 확인한 사항:

- `OnMainProgramStarted`는 `TriggerSeconds`가 아니라 **메인 프로그램 배속 잠금 진입 시점**에 방송됩니다. 잠금 진입은 시작 시각에서 `MainProgramLockLeadSeconds`를 뺀 때이며 코드 기본값은 10초입니다. 실제 연결된 시간표의 값은 시간 담당자가 확인해야 합니다.
- `UFF_CrowdDecisionSubsystem::HandleMainProgramStarted`는 무대를 후보로 열고 유휴 파티 재결정을 요청합니다. 기존 대기열·예약·퇴장 상태를 보호하므로 이것만으로 전원을 강제 집결시키지 않습니다.
- Stage는 `GetEffectiveTimeScale()`을 사용하지만 Crowd는 `OnTimeScaleChange`로 받은 `SimulationTimeScale`을 사용합니다. 공연 잠금으로 Stage가 1배속이 되어도 Crowd가 기존 배속을 사용할 수 있으므로 시간 담당자와 NPC 담당자가 같은 실효 배속 계약으로 맞춰야 합니다.
- 고배속·낮은 FPS에서는 Crowd의 고정 스텝 처리 상한과 누적 시간 처리도 점검해야 합니다. 화면의 시계만 1배속으로 표시된다고 군중 진행 시간까지 일치하는 것은 아닙니다.

요청하는 동작 계약:

1. **집결 준비 / 1배속 잠금 / 실제 공연 시작 / 종료·취소**를 서로 구분합니다. 기존 이벤트 의미 변경 시 기존 구독자를 함께 확인합니다.
2. 집결 시작은 `공연 시각 - 필요한 이동 여유 시간`으로 정합니다. 여유 시간은 가장 먼 구역의 실제 이동시간, 입구 처리량, 자리 배정/경로 계산 시간을 측정해 정합니다. 코드 기본 10초를 전원 도착 보장값으로 사용하지 않습니다.
3. 집결 구간에도 1배속이 필요하면 잠금 범위를 그 구간까지 확장하는 결정을 시간 담당자가 적용합니다. 시설 쪽에 `Delay`, `Set Global Time Dilation`, 별도 시계를 넣지 않습니다.
4. 프레임이 경계를 건너뛰어도 신호를 정확히 한 번 발생시키고, 일시정지·재개·저장 복원·날짜 전환 시 중복 집결을 막습니다. 실효 배속의 0(정지) 정책도 같은 기준으로 처리합니다.
5. 정시가 되어도 미도착자가 있는 경우의 정책을 확정합니다. 자연 이동으로 전원 도착을 보장하려면 충분한 선행 시간, 정원, 연결된 길, 대상 확정이 먼저 충족되어야 합니다. 순간이동은 현재 구현에 포함하지 않았습니다.

검토 위치(읽기만 했으며 수정하지 않음): `Source/FestaFiesta/Private/Time/FF_FestivalStageSubsystem.cpp`, `Source/FestaFiesta/Private/CrowdSystem/Systems/FF_CrowdSimulationSubsystem.cpp`, `FF_CrowdDecisionSubsystem.cpp`.

## 4. Queue·경제 담당자와 합의할 내용

- 기존 Queue의 capacity/service time은 유지했습니다. 공연 관람은 사람별 자리 공간이며 FIFO 서비스 정원과 구분합니다.
- 집결 대상이 유료 서비스 중이거나 예약/대기 중일 때 계속 이용할지, 취소할지, 비용/보상을 어떻게 처리할지 해당 담당자가 결정합니다.
- 관람 종료가 일반 부스 서비스 완료로 처리되거나 요금이 중복 발생하지 않도록 별도 관람 상태 전환을 확인합니다.
- '모두'에 이미 퇴장 중인 NPC, 늦게 입장하는 NPC, 직원·공연자까지 포함하는지 명시해야 합니다. 현재 시설 데이터는 대상 집합을 결정하지 않습니다.

## 5. 적용한 구조와 유지보수 이유

| 선택 | 이번 프로젝트에서의 효과와 한계 |
| --- | --- |
| 기존 시설 Spec에 관람 geometry 추가 | 별도 설정 에셋/중복 정원표 없이 에디터·저장·런타임이 동일한 배치를 사용합니다. 기존 설치물 자동 갱신은 하지 않습니다. |
| 자리 좌표를 index에서 계산 | 자리마다 Actor/Component를 만들지 않고 O(1)로 계산합니다. 예산 상한은 8,192자리/한 방향 512이나 실제 면적 제한이 먼저 걸릴 수 있습니다. |
| 기존 압축 사각형/회전 충돌·공간 해시 사용 | cm 단위의 모든 셀을 늘어놓지 않고 관람 외곽을 예약 영역에 합칩니다. 관람 공간은 보행 가능한 명시적 예약 영역이고 본체 NavMesh 장애물은 그대로입니다. 제거된 시설 둘레 안전 여유와 구별됩니다. |
| GUID 조회 + 읽기 Snapshot | 에셋 동기 로드/Actor 이름 검색을 피하고 타 파트에는 작은 읽기 API를 제공합니다. 캐시 수명은 연동자가 관리합니다. |
| 명시적 배치 검사 + 묶음 경로 검사 | 기본 오류를 저장 전에 발견하고 비싼 경로 탐색을 실행 시점과 호출량으로 제한합니다. 경로 검사는 자동 Tick 기능이 아닙니다. |
| 인원 확정 시 자동 보정·전체 검증 + Undo | 요청 행·열을 보존하고 안전 간격·무대/Queue 겹침·입퇴장·부족한 면적을 고치며 이유를 설명합니다. 미해결 오류는 요청을 보관해 수정 후 다시 적용합니다. 다른 에셋은 저장하지 않으며 게임 월드에 디버그 자리 Actor가 생기지 않습니다. |
| 기본값 비활성 + 태그 직렬화 | 기존 비관람 시설 동작과 기본 Spec fingerprint를 보존합니다. 이미 저장된 옛 시설을 새 배치로 강제 이주하지 않습니다. |

이는 현재 프로젝트 구조를 재사용하면서 계산·객체 수·파일 충돌을 줄이는 설계입니다. 대규모 군중에서 더 빠르다는 수치나 FPS 개선율은 측정하지 않았습니다. 엔진 프레임워크를 새로 교체하는 작업은 포함하지 않았습니다.

## 6. 변경 범위와 병합 시 주의

- 시설 런타임: `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem`의 Audience 타입/API/검증/예약 영역/fingerprint/테스트, 테스트용 FacilityActor friend 선언.
- 시설 에디터: `Plugins/FF_ZoneSystem/Source/FF_ZoneSystemEditor`의 Audience 편집 섹션·정렬·도움말·크기 조정·기존 미리보기 확장·테스트.
- 시설 월드 테스트: `Plugins/FF_ZoneSystem/Source/FF_ZoneSystemTests/Private/FF_FFZSAudienceWorldTests.cpp`.
- 콘텐츠: 정확히 `Content/FestaFiesta/Zone/Data/FacilityDefinitions/Performance/PDA_FFZS_Performance_MainStage_SunsetMainStage.uasset` 하나의 Audience 설정. Mesh/조명/오디오/Queue 등 다른 Spec 값이 바뀌지 않았음을 스크립트로 확인했습니다.
- 신규 도구: `Tools/FacilityAudience/configure_sunset_audience.py`, `sunset_audience_preset.json`.
- 신규 문서: `Collaboration/FacilityAudience_*`.

PDA는 바이너리 에셋이므로 팀에서 편집 담당자를 한 명 정하고 공유 전에 저장 상태를 확인하세요. NPC/시간 담당자는 공개 API를 소비하고 각자 소유 파일에서 연동합니다. 실제 이름이 같은 시설 두 개를 동시에 편집/지정하는 혼선을 피하도록 Instance ID를 공유합니다. 기존 사용자 미추적 `.claude/`는 손대지 않았습니다.

## 7. 확인 결과와 팀 통합 검수

UE 5.7.4 Editor Development 빌드 성공. 시설 관람 8개와 관련 기존 회귀/전체 검증기 14개, **총 22개 성공/실패 0**. 최소 저장 복원 테스트 월드에서 Economy의 Queue 없음 경고 1건이 있었고 해당 테스트도 성공했습니다. 실제 PDA 전체 검증, 요청 인원 유지·최소 면적 자동 확대·보정 설명, 최대 지원 정원 geometry, 모든 사용 레벨의 면적 계산, 미해결 요청 보관·재적용 및 Undo, 편집 취소 후 재입력을 확인했습니다. 최초 설정 시 120개 native slot 조회도 성공했습니다.

실제 운영 맵 NavMesh와 Crowd 이동, 전체 집결, 시간 전환, 화면 육안 확인, 대규모 성능 측정은 이번 자동 검증 범위에 포함되지 않습니다.

- [ ] 대상 인원·포함/제외 기준·정원 확정.
- [ ] 모든 구역→입구 및 모든 자리 입퇴장 경로 성공.
- [ ] 사람별 자리 중복 0, 시작 시 미도착 0 또는 합의한 실패 정책 적용.
- [ ] 집결 전 고배속→잠금 1배속→종료 후 복구, 일시정지/재개에서 시계·군중 동기화.
- [ ] 낮은 FPS/프레임 지연에서도 시작·종료 신호 중복/누락 없음.
- [ ] 시설 이동·삭제·폐쇄·저장 복원 시 낡은 Snapshot/자리 소유권 정리.
- [ ] 기존 Queue/예약/경제 처리 회귀 없음.

개발자 재실행 필터:

```text
Automation RunTests FF_ZoneSystem.Audience+FF_ZoneSystem.Unit.Grid+FF_ZoneSystem.Unit.SaveGame+FF_ZoneSystem.Unit.Commands+FF_ZoneSystem.Unit.Upgrade+FF_ZoneSystem.Editor.FacilityAuthoringHelpers+FF_ZoneSystem.Editor.FacilityLevelPreview+FF_ZoneSystem.Editor.FacilityDefinitionCrossAssetValidation
```

보고서: `Saved/FacilityAudience/RepairTests/index.json`. 빌드 로그: `Saved/AudienceRepairBuild.log`. 에셋 초기화 스크립트는 이미 활성화된 관람 값을 보존합니다. 이전 에셋 백업은 최초 적용 PC의 `Saved/FacilityAudience/PDA_SunsetMainStage.before_audience.uasset`에만 있으므로 팀 공유용 버전 관리 백업으로 간주하지 않습니다.
