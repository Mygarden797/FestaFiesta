# BuildMode 팀 소유 연동 기록 — 적용 완료

## 상태

이 문서는 실제 적용한 최소 연동 지점과 회귀 안전장치를 기록한다. 팀 작업 보호 원칙에 따라 `FF_BuildModeSubsystem.h/.cpp`의 기존 기능을 삭제·이동하지 않고, 신규 PrepHUD 표현에 필요한 API와 분기만 가산적으로 추가했다.

신규 `WBP_FF_PrepPanelBuild`는 이름 규칙으로 기존 PrepHUD에 결합된다. `OpenCatalog()`가 기존 전체 화면을 강제로 만드는 중복만 피하기 위해 아래 두 확장 지점을 구현했다. 기본값에서는 기존 동작이 유지된다.

## 적용 1 — 외부 카탈로그 표현 opt-in

적용 API:

```cpp
void SetExternalCatalogPresentationActive(bool bActive);
bool IsExternalCatalogPresentationActive() const;
```

기본값은 반드시 `false`다.

| 상황 | `false` — 기존 경로 | `true` — 신규 PrepHUD 표현 |
|---|---|---|
| `OpenCatalog()` | 기존 CatalogScreen 생성·표시 | 기존 화면 생성 생략, 세션·입력·상태·썸네일 수명은 동일하게 유지 |
| `StartPlacement()` | 기존 동작 | 동일한 기존 배치 로직 사용 |
| `ReturnToCatalog()` | 기존 전체 화면 복귀 | 상태만 CatalogOpen으로 복귀하고 PrepHUD 패널이 목록 상태 표시 |
| `ExitBuildMode()` | 기존 정리 | 외부 표현 플래그와 무관하게 동일하게 정리 |

플래그는 표현 선택만 담당하며 비용, 검증, 배치, 저장 규칙을 분기하지 않는다.

## 적용 2 — 상태 변경 이벤트

적용 API:

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FFF_OnBuildModeStateChanged,
    EFF_BuildModeState, PreviousState,
    EFF_BuildModeState, NewState);

UPROPERTY(BlueprintAssignable)
FFF_OnBuildModeStateChanged OnBuildModeStateChanged;
```

상태 대입을 하나의 내부 `SetState(NewState)` helper로 모으고, 실제로 값이 달라졌을 때만 이벤트를 한 번 보낸다. 신규 패널은 이 이벤트를 구독해 다음과 같이 표시한다.

| BuildMode 상태 | 패널 표시 |
|---|---|
| Closed / Inactive | 패널 닫힘 |
| CatalogOpen | 전체 카탈로그 목록 |
| Placing | 배치 조작 축약 바 |
| Moving | 이동 조작 축약 바 |

이벤트가 없으면 UI가 매 프레임 상태를 조회하거나 입력 결과를 추측해야 한다. 두 방식 모두 상태 불일치와 불필요한 Tick을 만들기 때문에 권장하지 않는다.

## 실제 적용 순서

1. 기존 BuildMode Automation Test와 수동 흐름을 먼저 기록한다.
2. 플래그와 getter를 추가하고 기본값을 `false`로 둔다.
3. 기존 상태 대입을 동작 변화 없이 `SetState()`로 치환한다.
4. 상태 변경 이벤트와 기본 opt-in 계약을 자동화 테스트에 포함한다.
5. `OpenCatalog()`와 `ReturnToCatalog()`의 표현 생성 부분만 플래그로 분기한다.
6. 플래그 `false`가 클래스 기본값임을 자동화 테스트로 고정한다.
7. 플래그 `true`에서 신규 패널의 카드 선택→배치→목록 복귀를 연결한다.
8. 패널 생명주기에서 delegate와 썸네일 preload를 대칭 해제한다.

## 검증 결과

- PASS — 외부 표현 opt-in 기본값은 `false`이며 자동화 계약으로 고정했다.
- PASS — 외부 표현 모드에서는 레거시 CatalogScreen 생성만 생략한다.
- PASS — 세션·입력·카탈로그 갱신·썸네일 preload 및 배치 도메인 로직은 기존 경로를 사용한다.
- PASS — 모든 상태 쓰기는 `SetState()`를 통과하고 실제 변경 때만 이벤트를 보낸다.
- PASS — 신규 패널은 이벤트를 구독하며 닫힐 때 delegate와 preload를 해제한다.
- PASS — Editor 빌드 및 전체 7종 카테고리 계약을 포함한 `FestaFiesta.UI.PrepBuild` 자동화 7/7 성공.
- 수동 확인 — 실제 맵의 B/Esc·게임패드 포커스·패키지 실행 체감은 통합 QA에서 확인한다.

## 채택하지 않는 우회 방식

신규 카드 클릭 시 `OpenCatalog()`와 `StartPlacement()`를 연속 호출하면 기존 화면이 렌더되기 전에 사라지는 것처럼 보일 수 있다. 하지만 불필요한 위젯 생성, 입력 모드 순간 전환, B/Esc 복귀 시 기존 화면 재등장, 테스트 어려움이 남는다. 데모성 임시 확인 외에는 사용하지 않는다.

## 변경 범위

- 수정된 기존 파일: `Public/Building/FF_BuildModeSubsystem.h`, `Private/Building/FF_BuildModeSubsystem.cpp`
- 신규 패널·카드·모델·테스트·Cook label: 모두 별도 신규 경로
- 삭제·rename·move: 0건
- 기존 WBP·이미지·설정 파일 수정: 0건
- 리뷰 포인트: API 이름, Blueprint 노출 범위, 실제 맵 입력 회귀
