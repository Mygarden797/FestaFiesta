# 카메라 구역 이동·시작 높이 문제 분석과 수정 기록

작성일: 2026-10-10. **사용자가 `FF-Win64-Shipping-20261010-05`에서도 시작 위치와 이동 문제가 남아 있다고 재보고했다. 이전 자동화 성공을 문제 해결로 판단한 결론은 정정한다.** 최종적으로 확인한 근본 원인은 Cook에서 에디터 전용 부모가 제거되면서 런타임 Zone 13개의 월드 위치가 함께 바뀐 것이다. 잘못된 Zone 외곽 경계가 시작 초점을 다른 곳으로 강제 이동시키고 W 방향 이동까지 막았다.

원본 맵의 Zone을 월드 위치를 보존한 채 부모에서 분리하고, 같은 연결 문제를 검출하도록 제작 검사를 보완한 **`FF-Win64-Shipping-20261010-06` 패키징을 완료했다.** Cook된 파일에서도 13개 Zone의 월드 좌표 오차가 0이고 경계 점 89개가 일치한다. 상세 원인·변경·결과와 실행 파일은 [12절: Cook 시 부모 제거 문제와 -06 수정](#cook-parent-06)에 있다. 사용자의 최신 지시에 따라 `-06`의 게임 플레이, 자동화 테스트, 실행 스모크 검사는 생략했다. 컴파일·Cook·패키징, 에셋 유효성 검사와 정적 파일·구성 확인만 완료했다. 에셋 유효성 검사는 저장된 데이터의 규칙을 확인하는 작업으로 게임을 플레이하는 검사가 아니다. 직접 조작 결과는 사용자가 확인한다.

`-05`의 자동화 1,148개 성공, 에셋 검증, 초기 기동과 ZIP 무결성 성공은 당시의 실제 기록으로 보존한다. 이 기록이 Cook된 본편 맵의 카메라 문제 해결을 증명하지 못한 이유를 함께 설명한다.

이번 작업의 목표는 두 가지다. 카메라가 W/S 입력과 화면 가장자리 이동으로 서로 떨어진 구역 사이를 오갈 수 있어야 한다. 그리고 제작자가 지정한 높은 광장 시점을 게임 시작부터 유지해야 한다. 이동을 마친 뒤 입력하지 않으면 카메라는 현재 위치에서 멈춰 있어야 한다.

## 1. 사용자에게 나타난 증상

| ID | 사용자에게 보인 문제 | 이번 수정의 완료 조건 |
|---|---|---|
| C01 | W 또는 화면 위쪽 이동을 해도 다른 구역으로 넘어가지 못한다. | 구역 사이의 빈틈을 작은 연속 입력으로 왕복한다. 전체 맵의 외곽 제한은 유지한다. |
| C02 | 에디터에서 기대한 높은 광장 시점과 달리, 패키지에서는 낮은 모래 쪽 시점으로 시작한다. | 실제 플레이 시작 경로가 지정한 초점 높이와 줌 거리를 보존한다. 첫 프레임, 대기, 이동 후에도 의도하지 않은 급강하가 없어야 한다. |

처음에는 C01의 **이동 제한**과 C02의 **시작 높이**를 각각 조사하고 개선했다. 그러나 `-05` 재보고 이후, 두 증상을 함께 설명하는 **Cook된 Zone의 좌표 이동과 그에 따른 외곽 클램프**를 확인했다. 3·4절은 이전에 개선한 별도 동작의 기록이고, 이번 패키지 문제의 근본 원인은 12절이다.

## 2. 먼저 알아둘 용어

| 용어 | 쉬운 설명 |
|---|---|
| 초점, Focus | 카메라가 바라보는 기준점이다. 카메라 렌즈 자체의 위치와는 다르다. |
| 스프링암, Arm | 초점과 카메라 사이의 거리다. 이 문서의 `4000`은 언리얼 단위로 4,000cm, 즉 40m다. 렌즈의 수직 높이가 40m라는 뜻은 아니다. |
| Zone, 구역 | 시설을 배치하거나 구역 규칙을 적용하는 다각형 영역이다. 구역 사이에는 도로나 좁은 빈 공간이 있을 수 있다. |
| 외곽 경계, Bounds | 여러 구역을 모두 감싸는 큰 사각형이다. 카메라가 맵에서 끝없이 멀어지는 것을 막는다. |
| 지면 추종 | 이동할 때 바닥 높이를 읽어 초점 높이를 조절하는 기능이다. |
| Tick, 프레임 | 게임이 한 번 갱신되는 주기다. 120FPS라면 초당 120번 갱신한다. |
| GameMode | 해당 맵의 시작 규칙을 정한다. 여기서는 사용할 카메라와 최초 시점을 지정한다. |
| Blueprint, BP | 디자이너가 에디터에서 저장하는 클래스와 설정이다. C++ 기본값을 그대로 사용할 수도 있고, 별도 값을 저장할 수도 있다. |
| PIE | 에디터 안에서 실제 게임을 실행하는 Play In Editor다. 에디터의 편집용 화면과 구분한다. |
| 회귀 테스트 | 한 번 고친 문제가 이후 변경으로 다시 생기지 않는지 검사하는 자동화 테스트다. |
| Cook | 에디터의 원본 에셋을 실제 배포용으로 가공하는 과정이다. 에디터에서만 필요한 물체는 이때 제외될 수 있다. |
| 부모 연결, Attachment | 한 물체의 위치를 다른 물체 기준으로 정하는 관계다. 부모 기준 좌표인 상대 위치와 맵 전체 기준인 월드 위치가 다르다. |
| Keep World | 부모 연결을 끊어도 화면상 위치·회전·크기를 그대로 유지하는 방식이다. |
| 클램프, Clamp | 좌표가 허용 범위를 벗어나면 가장 가까운 경계값으로 제한하는 계산이다. |

## 3. 이전 C01 조사 — 개별 구역 제한 개선

이 절의 strict 제한은 실제로 구역 사이 이동을 막을 수 있어 개선했다. 다만 `-05`의 문제가 계속된 근본 원인은 아니었다. 현재 확정 원인은 [Cook된 Zone 좌표](#cook-parent-06)를 참고한다.

### 확인한 제한 동작

기존 `bConstrainFocusToZones` 기본값은 `true`였다. 이 옵션은 카메라 초점이 **어느 개별 Zone에도 포함되지 않는 위치**로 이동하려 하면 마지막으로 허용된 위치로 되돌린다. 여러 구역을 합친 외곽 사각형 안에 있어도, 구역과 구역 사이의 빈틈은 이 규칙에서 허용되지 않는다.

`AFF_OverviewCameraPawn::Tick`은 이동 입력을 처리한 다음 `ConstrainFocusToZones`를 호출한다. 이 함수는 먼저 목표 위치가 구역 안인지 확인한다. 바깥이면 X축과 Y축을 따로 시도하고, 그것도 허용되지 않으면 마지막 유효 위치로 돌아간다. 축 하나만 되돌리는 경우에는 진단 로그의 “차단” 대신 “미끄러짐” 횟수가 늘 수 있다. 차단 횟수가 0이라는 이유만으로 이 문제가 없다고 판단하면 안 된다.

저장된 `Test_Main`의 튜토리얼 구역과 동쪽 임시 주차 구역에는 다음과 같은 실제 간격이 확인됐다.

| 항목 | 값과 의미 |
|---|---|
| 검사할 수평선 | 월드 Y = 35,800cm |
| 튜토리얼 쪽 경계 | X = 75,909.875cm |
| 주차 구역 쪽 경계 | X = 75,884.515cm |
| 두 경계 사이 간격 | 약 25.36cm |
| 빈틈의 대표 점 | X = 75,897cm, Y = 35,800cm. 실제 테스트는 이 점이 모든 Zone 바깥인지 확인한다. |
| 시작 줌 거리에서의 이동 예 | Arm = 4,000cm, 120FPS, 최대 세기 키보드 입력일 때 계산상 약 18.36cm/프레임 |

한 프레임의 이동량이 빈틈보다 작으면 목표점이 빈틈 안에 들어간다. 이전 규칙은 그때마다 초점을 되돌리므로, 키를 계속 눌러도 다음 구역에 닿지 못한다. 큰 입력이나 낮은 프레임률에서는 한 번에 빈틈을 건너뛰어 증상이 달라질 수 있다. `18.36cm`는 해당 설정과 120FPS 조건의 이동량 계산이며, 사용자의 실제 플레이 프레임률을 측정한 값으로 사용하지 않는다.

초기 BP 검사에서도 `BP_Camera`의 유효 값이 `constrain_focus_to_zones=True`였다. 이 항목을 BP가 별도 값으로 덮어쓴 근거는 확인되지 않았다. 따라서 단순한 W키 매핑 누락보다, 입력 뒤 적용되는 구역 제한 규칙이 원인이라는 설명에 부합한다.

### 무엇을 변경했는가

`FFF_OverviewCameraSettings::bConstrainFocusToZones`의 C++ 기본값을 `false`로 변경했다. 카메라가 구역 사이의 도로와 빈틈을 통과할 수 있게 하는 변경이다.

전체 Zone을 감싼 외곽 경계를 사용하는 `bUseZoneBounds=true`와 외곽 여유 `ZoneBoundsPadding=3000cm`는 유지한다. 개별 다각형 안에만 머물러야 하는 특수 맵에서는 `bConstrainFocusToZones=true`를 명시적으로 켤 수 있다. 이 선택 옵션의 기존 제한 동작도 새 테스트로 검사한다.

구역 다각형을 임의로 넓히거나 빈틈을 에셋으로 메우는 방식은 이 수정에 포함하지 않는다. 시설 배치 영역과 관찰 카메라 이동 영역은 각각의 목적에 맞게 사용한다.

### 기존 테스트가 놓친 이유

기존 `FF_OverviewCameraMotionTests.cpp`의 테스트 월드는 `bUseZoneBounds=false`, `bConstrainFocusToZones=false`로 설정했다. 지면 추종, 줌 보간, 입력 해제 후 정지를 분리해서 검사하는 데는 적합하지만, 실제 Zone 다각형 때문에 이동이 취소되는 경로를 실행하지 않는다.

리그의 좌표 계산 테스트만으로도 부족하다. 리그가 올바른 방향으로 이동량을 계산해도, 이후 Pawn에서 목표를 되돌리면 실제 화면은 움직이지 않는다. 새 테스트는 **실제 월드 → 등록된 Zone 두 개 → 실제 Pawn 입력 → Pawn Tick → 최종 초점** 전체 경로를 확인한다.

## 4. 이전 C02 조사 — 지정한 높은 시점 보존

이 절은 사용자가 원하는 높은 시점을 유지하기 위한 개선 기록이다. 높이 보존을 적용한 `-05`에서도 문제가 남았으며, 이후 Cook된 Zone 경계 때문에 시작 **X좌표**가 강제 변경되는 현상을 확인했다. 높이 보존만으로 이번 패키지 문제를 해결할 수는 없었다.

### 저장된 시작 설정과 실행 경로

초기 에셋 검사에서 `Test_Main`은 다음 설정을 사용했다.

| 항목 | 확인한 저장 값 |
|---|---|
| 맵 | `/Game/FestaFiesta/Maps/Test_Main` |
| GameMode | `/Game/FestaFiesta/Camera/BP_FestivalGameMode` |
| 카메라 클래스 | `/Game/FestaFiesta/Camera/BP_Camera` |
| 최초 초점 | X = 78,740cm, Y = 35,800cm, Z = 4,540cm |
| 최초 Arm 길이 | 4,000cm |
| GameMode 최초 구도 적용 | `bApplyInitialFraming=true` |
| 카메라 지면 추종 | `bFollowGroundHeight=true`, `GroundFocusOffset=100cm` |

GameMode는 시작할 때 `InitializeOverview(InitialFocusLocation, InitialArmLength)`를 호출한다. 그런데 기존 Pawn의 첫 Tick은 `UpdateGroundHeight(0, true)`를 실행한 뒤 `Rig.SnapToTarget()`을 실행한다. 첫 지면 표본은 보간 없이 반영되므로, 제작자가 지정한 초점 Z가 “측정된 바닥 + 기본 100cm”로 즉시 바뀔 수 있다.

높은 초점으로 시작하도록 설정해도, 첫 실제 갱신에서 낮은 지면 위로 초점이 내려가는 구조다. Arm 길이가 같아도 초점 높이가 바뀌면 화면 구도가 크게 달라진다. 이 때문에 “카메라 위치가 바뀌었다”거나 “모래 쪽으로 내려갔다”는 모습이 나타날 수 있다.

관련 Git 이력 `4e01975`에는 입력하지 않을 때의 카메라 움직임을 멈추는 변경과 함께 최초 지면 배치 경로가 포함돼 있다. 이 이력은 높은 시점 보존을 개선한 근거다. 이후 Cook된 `-05`의 GameMode에도 최초 초점과 Arm 값은 정상적으로 남아 있는 것을 확인했다. **BP의 초기 카메라 값을 바꾼 문제는 아니지만, Cook이 에디터 전용 부모를 제거하면서 Zone 좌표와 카메라 외곽 경계가 달라지는 문제는 실제로 있었다.**

에디터의 편집용 시점, PlayerStart 위치, GameMode가 게임 시작에 적용하는 초점도 서로 다르다. 에디터 화면과 패키지 화면을 비교할 때는 실제 게임 시작 경로, 선택된 GameMode, 저장 데이터, 해상도와 화면 비율을 함께 확인해야 한다. 저장 에셋을 읽은 결과만으로 사용자가 본 두 화면의 모든 차이를 확정하지 않는다.

### 무엇을 변경했는가

사용자는 높은 시작 시점을 유지하도록 요청했다. 이에 따라 **제작자가 정한 시작 초점과 지면 사이의 높이 차이를 보존하는 선택 기능**을 코드에 반영했다. 카메라 집중 검사와 전체 자동화 검사에서 모두 통과했다.

| 설정·API | 확정된 동작 |
|---|---|
| GameMode `bPreserveInitialCameraHeight=true` | 게임 시작의 지정 구도에 높이 보존을 적용한다. `EditDefaultsOnly` 설정이므로 GameMode 기본값에서 선택할 수 있다. |
| Pawn `InitializeOverview(FocusLocation, SuggestedArmLength, bool bPreserveGroundClearance=false)` | 세 번째 인자로 높이 보존을 선택한다. 기존 두 인자 호출은 기본 `false`이므로 종전의 지면 + 지정 offset 동작을 유지한다. |
| GameMode의 초기화 호출 | `bApplyInitialFraming`이 켜진 시작 경로에서 `bPreserveInitialCameraHeight`를 위 세 번째 인자로 전달한다. |
| Pawn의 초기 높이 저장 | 리그 초기화와 현재 경계 적용을 거친 최초 초점 Z를 기준으로 저장한다. |
| 최초 유효 지면 표본 | `max(초기 초점 Z − 처음 측정한 지면 Z, 원래 GroundFocusOffset)`으로 유지할 높이 차이를 한 번 정한다. |
| 실행 중 설정 | 계산한 값을 실행 중 사용하는 설정 사본에만 적용한다. 원래 `CameraSettings.GroundFocusOffset`과 저장된 BP 설정은 덮어쓰지 않는다. |

예를 들어 회귀 테스트의 평평한 바닥이 Z=450cm이고 지정 초점이 Z=4,540cm라면, 지면과의 높이 차이는 4,090cm다. 이동한 곳의 바닥이 200cm 높아지면 초점도 그 차이를 유지하며 조절된다. 이 숫자의 바닥 높이는 테스트용 조건이며, 실제 맵 전체의 지면 높이라는 뜻은 아니다.

원래 `GroundFocusOffset`은 이 프로젝트 BP에서 100cm다. 최초 초점이 지면에 너무 가깝거나 지면 아래라면 이 최소 간격을 우선한다. 따라서 모든 입력 Z를 무조건 고정하는 기능이 아니다. 지정한 높은 시점을 보존하면서 기존 최소 지면 간격도 지키는 기능이다.

| 상황 | 높이 기준의 처리 |
|---|---|
| 처음에 지면을 찾지 못함 | 현재 지정 높이를 유지한다. 유효 지면 표본을 처음 얻었을 때 저장해 둔 초기 Z를 기준으로 높이 차이를 정한다. |
| 지면을 찾기 전에 낮은 대상에 Focus/Follow 요청 | 대상의 낮은 Z를 새로운 시작 높이로 오인하지 않는다. 최초 기준 Z를 보존한다. |
| `ResetView` | 높이 차이 기준을 유지한다. 최초 XY와 Arm으로 돌아가되, 지면이 바뀌었다면 그 지면 + 유지한 높이 차이를 적용한다. |
| `StopCameraMotion` | 이동·추적을 멈추고 높이 차이 기준은 유지한다. |
| `RefreshZoneBounds` | 외곽 경계를 다시 계산하더라도 실행 중 높이 차이 기준을 유지한다. |
| `InitializeOverview`를 다시 호출 | 이전 높이 차이를 버리고 새 초기 Z와 다음 유효 지면 표본으로 다시 정한다. |
| 높이 보존과 지면 추종이 모두 켜진 Focus/Follow | 명령이나 대상이 XY를 정하고, 지면 추종이 Z를 정한다. 대상의 Z와 지면 높이가 매 프레임 서로 덮어쓰지 않는다. |
| 높이 보존 또는 지면 추종을 끈 Focus/Follow | 기존 명시적 XYZ 좌표 동작을 유지한다. |

입력이 없는 Idle 상태에서 계속 새 지면을 따라 움직이지 않는 기존 정책도 유지한다. 지면을 늦게 찾는 경우에도 Idle 자체가 새로운 탐색을 강제로 시작하지 않으며, 다음 명시적 이동·Focus 명령이나 활성 Follow의 표본 갱신에서 기준을 확정한다.

## 5. 파일별 역할과 변경 연결

행 번호는 코드 수정으로 바뀔 수 있으므로 함수와 테스트 이름으로 찾는다. 아래 경로는 현재 작업 공간 `C:/FFProj` 기준이다.

| 파일 | 역할 | 이번 문제와의 관계 |
|---|---|---|
| [FF_CameraTypes.h](C:/FFProj/Source/FestaFiesta/Public/Camera/FF_CameraTypes.h) | 카메라 설정의 기본값 | C01의 엄격한 개별 Zone 제한을 선택 옵션으로 변경한다. |
| [FF_OverviewCameraPawn.cpp](C:/FFProj/Source/FestaFiesta/Private/Camera/FF_OverviewCameraPawn.cpp) | 입력, 경계, 지면, 정지 상태를 연결 | C01은 `ConstrainFocusToZones`, C02는 `InitializeOverview`와 최초 Tick의 지면 처리 경로를 확인한다. |
| [FF_OverviewCameraPawn.h](C:/FFProj/Source/FestaFiesta/Public/Camera/FF_OverviewCameraPawn.h) | Pawn의 공개 API와 실행 중 상태 선언 | 높이 보존 선택 인자와 실행 중 높이 차이·최초 Z 상태를 선언한다. |
| [FF_OverviewCameraRig.cpp](C:/FFProj/Source/FestaFiesta/Private/Camera/FF_OverviewCameraRig.cpp) | 초점·줌·각도·보간 계산 | 이동량 계산, 외곽 제한, `ApplyGroundHeight`, 입력 해제 후 목표 고정을 담당한다. |
| [FF_FestivalGameMode.h](C:/FFProj/Source/FestaFiesta/Public/Camera/FF_FestivalGameMode.h) | 맵 시작 구도 옵션 | `bPreserveInitialCameraHeight=true`를 기본값으로 제공한다. |
| [FF_FestivalGameMode.cpp](C:/FFProj/Source/FestaFiesta/Private/Camera/FF_FestivalGameMode.cpp) | 실제 게임 시작의 구도 적용 | `bApplyInitialFraming`이 켜지면 저장된 초점·Arm 길이와 높이 보존 선택을 Pawn에 전달한다. |
| [FF_OverviewCameraZoneTests.cpp](C:/FFProj/Source/FestaFiesta/Private/Camera/FF_OverviewCameraZoneTests.cpp) | 새 합성 월드 회귀 검사 | 두 구역의 빈 공간 왕복, 프레임률·줌별 이동, 외곽 제한과 strict 선택 옵션을 검사한다. |
| [FF_OverviewCameraProductionTests.cpp](C:/FFProj/Source/FestaFiesta/Private/Camera/FF_OverviewCameraProductionTests.cpp) | 새 실제 맵 회귀 검사 | 실제 GameMode가 생성한 BP 카메라의 최초 구도와 `Test_Main`의 실제 빈틈 이동을 검사한다. |
| [FF_OverviewCameraMotionTests.cpp](C:/FFProj/Source/FestaFiesta/Private/Camera/FF_OverviewCameraMotionTests.cpp) | 이동·정지와 높이 보존 회귀 검사 | 기존 Idle·줌·지면 검사에 높이 보존용 테스트 3개를 추가했다. |

새 Zone 테스트는 현재 허용된 `GZ_Beach01`, `GZ_Beach02`를 공통 테스트 계약에서 가져온다. 구역 ID 정책을 느슨하게 만들거나, 폐기된 ID를 테스트에 다시 넣지 않는다. 실제 Zone 등록 실패는 설정 오류로 명확하게 보고한다.

## 6. 테스트가 보장해야 하는 동작

### 합성 월드: 실제 Pawn과 서로 떨어진 두 Zone

새 테스트는 구역 두 개 사이에 4,000cm의 빈 공간을 만든다. 실제 Zone 서브시스템에 등록하고, 위치를 직접 순간이동시키는 대신 `AddPanInput` 또는 `AddEdgeScrollInput`을 반복 호출한 뒤 `Tick`으로 이동시킨다. 이동 검사는 C++ 기본 제한값을 `false`로 덮어쓰지 않으므로, 기본값이 다시 잘못 바뀌면 실패한다.

| 정확한 테스트 이름 | 검사 내용 |
|---|---|
| `FestaFiesta.Camera.Zones.KeyboardCrossesDisconnectedZones` | 30/60/144FPS × 최소/최대 Arm에서 W/S에 대응하는 입력으로 빈 공간을 왕복하고 상대 구역 안에 도착한다. |
| `FestaFiesta.Camera.Zones.EdgeScrollCrossesDisconnectedZones` | 같은 조건에서 위·아래 가장자리 이동 입력으로 빈 공간을 왕복한다. |
| `FestaFiesta.Camera.Zones.CombinedOuterBoundsAndIdle` | 키보드와 가장자리 입력으로 외곽 X/Y 양쪽 경계까지 이동한다. 경계에서 계속 눌러도 벗어나지 않고 해제하면 멈춘다. |
| `FestaFiesta.Camera.Zones.StrictZoneContainmentIsOptIn` | strict 옵션을 명시적으로 켜면 시작 Zone 내부 이동은 허용하고 구역 밖 통과는 막는 기존 동작을 유지한다. |

입력 해제 검사는 2초 동안 초점·Arm·Yaw·Pitch와 Pawn 위치가 정확히 고정되는지 확인한다. 외곽 검사는 경계 도달 뒤 1초 동안 입력을 계속해 이탈이 없는지도 확인한다. 수평 경계 문제를 분리하기 위해 이 합성 월드에서만 지면 추종을 끈다. 실제 지면과 높은 시작 구도는 다음 실제 맵 검사 및 별도 높이 회귀 검사에서 다룬다.

### 물리 바닥: 높은 시점과 지면 추종

다음 3개 테스트는 모두 `Source/FestaFiesta/Private/Camera/FF_OverviewCameraMotionTests.cpp`에 추가했다. 실제 충돌 바닥을 만들고 물리 질의로 높이를 확인한다. 세 테스트 모두 카메라 집중 검사와 전체 자동화 검사에서 통과했다.

| 정확한 테스트 이름 | 검사 내용 |
|---|---|
| `FestaFiesta.Camera.Motion.AuthoredHeightSurvivesIdlePanAndEdgeScroll` | 30/60/144FPS에서 최초 Z=4,540cm·Arm=4,000cm를 보존하고, 대기·키보드·가장자리 이동·입력 해제 후에도 평평한 바닥 위의 높이를 유지한다. 원래 `GroundFocusOffset=100cm`가 바뀌지 않는지도 확인한다. |
| `FestaFiesta.Camera.Motion.AuthoredHeightSlopeResetBoundsAndReinitialize` | 실제 경사진 바닥으로 Focus를 이동하고 초기 높이 차이 유지, Reset, 늦은 외곽 적용과 `RefreshZoneBounds`, 재초기화에 따른 새 기준 계산을 검사한다. 일반 모드의 기존 지면 + offset 동작과 너무 낮은 초기 Z에 대한 최소 간격도 검사한다. |
| `FestaFiesta.Camera.Motion.AuthoredHeightWaitsForGroundDuringFocusAndFollow` | 시작 지면이 없을 때 낮은 대상에 Focus/Follow해도 최초 높이를 잃지 않는지 확인한다. 나중에 지면이 생길 때 최초 기준을 한 번 정하고, 이후 지면 변화에 따라 움직이며, 지면 추종을 끄면 명시적 XYZ Focus 동작으로 돌아가는지도 확인한다. |

### 실제 맵: 시작 설정과 작은 빈틈

`FestaFiesta.Camera.Production.InitialFramingAndZoneTraversal`은 저장된 `Test_Main`을 읽고 PIE를 시작한다. 실제 GameMode, Controller, BP 카메라가 연결됐는지 확인하며, 테스트가 카메라 위치를 준비하기 **전에** 최초 초점 X/Y/Z와 Arm을 읽는다. 테스트 준비 코드가 잘못된 실제 시작 높이를 감추지 않게 하기 위한 순서다.

이후 실제 튜토리얼·주차 구역 사이의 빈틈을 작은 입력으로 통과한다. 대표 점이 모든 구역 바깥인지도 따로 확인하므로, 맵 변경으로 빈틈이 사라졌는데 회귀 검사가 계속 의미 없이 통과하는 일을 방지한다. 실제 외곽 경계와 여유 폭, 카메라 소유권 및 화면 대상이 유지되는지도 검사한다.

이 검사는 Pawn 입력 경로를 직접 실행하므로 OS의 키보드 포커스나 창 활성화 상태의 영향을 줄인다. 그만큼 **사용자가 실제 창에 W키를 눌러 조작하는 전체 경로를 대신하지는 않는다.** 최종 패키지에서는 실제 창의 입력과 화면을 별도로 확인해야 한다.

## 7. 운영·재검증 참고 절차

**아래 자동화 명령은 유지보수 참고용이다. 사용자 최신 지시에 따라 이번 `-06` 작업에서는 실행하지 않는다.** 이번에 수행하는 범위와 대기 항목은 12절에 별도로 기록한다. 실제 조작 확인은 사용자가 담당한다.

### 코드 또는 카메라 설정을 바꾼 뒤

먼저 카메라 필터만 실행해 원인을 좁힌다. 아래 명령은 현재 프로젝트의 기존 테스트 실행기를 사용한다. `SkipDataValidation`은 이 빠른 진단에서 전체 에셋 검사를 중복 실행하지 않기 위한 선택이며, 최종 배포 검증을 생략하는 설정으로 사용하지 않는다.

```powershell
& 'C:/FFProj/Tools/Build/Run-AutomationTests.ps1' `
    -EngineRoot 'C:/Unreal/UE_5.7' `
    -ProjectFile 'C:/FFProj/FestaFiesta.uproject' `
    -TestFilter 'FestaFiesta.Camera' `
    -OutputRoot 'C:/FFProj/Artifacts/CameraRegression-20261010/FocusedValidation' `
    -SkipDataValidation
```

실패하면 `Automation.log`의 최초 오류와 `Report/index.json`의 정확한 테스트 이름을 함께 읽는다. 입력에 반응하는데 구역 끝에서 되돌아가면 strict 설정과 Zone 폴리곤을 확인한다. 시작부터 높이가 낮으면 GameMode의 최초 초점, 카메라의 최초 지면 표본 처리, 높이 보존 옵션 적용 여부를 확인한다. 처음부터 입력이 들어오지 않으면 UI 입력 차단, 창 포커스와 실제 Controller 연결을 별도로 확인한다.

기존 디버그 명령 `FF.Camera.Bounds`는 외곽 경계, 소속 Zone, strict 옵션, 마지막 유효 위치, 차단·미끄러짐 횟수를 보여준다. 개발 환경에서 진단용으로 사용한다. Shipping 실행 파일에서 이 개발용 명령이 제공된다고 가정하지 않는다.

일반적인 유지보수에서는 집중 진단과 전체 자동화·에셋 검사·실행 확인을 구분해 결과를 기록한다. 이번 `-06`에서는 사용자 요청에 따라 게임 자동화 테스트와 게임 실행을 수행하지 않고, 컴파일·Cook·패키징, 에셋 유효성 검사와 정적 파일·구성 확인 결과를 기록한다. 기존 테스트는 삭제하거나 통과 기준을 약하게 바꾸지 않는다.

### 새 패키지를 실제로 확인할 때

1. 실행한 파일의 폴더와 빌드 이름을 확인한다. 이전 ZIP을 다른 폴더에 풀어 놓고 새 결과로 착각하지 않도록 한다.
2. 첫 화면을 움직이기 전에 높은 광장 구도가 맞는지 확인한다. 같은 저장 상태와 게임 시작 경로로 비교한다.
3. W/S로 구역 사이를 왕복한다. 화면 가장자리 이동은 프로젝트의 휠 버튼 유지 조건에 맞게 조작한다.
4. 입력을 놓고 화면이 계속 흘러가거나 지면으로 내려가지 않는지 확인한다.
5. 줌을 가장 가까이·멀리 바꿔 같은 경로를 통과하고, 전체 맵의 바깥 경계에서는 제한이 유지되는지 확인한다.
6. 리셋 명령으로 최초 구도가 복원되는지 확인한다. 결과를 영상·화면 크기·실행 파일 경로와 함께 보관한다.

수정 전 비교본이 필요하면 기존 패키지를 별도 폴더에서 그대로 실행한다. 원본 프로젝트에 낡은 BP나 소스 파일을 덮어써 비교하지 않는다. 결과가 좋지 않으면 실패한 실행의 로그를 남기고 코드·설정 변경을 검토한 뒤 다시 빌드한다. 실행 중인 에디터가 에셋을 잡고 있을 때 파일을 강제로 덮어쓰는 복구 방식도 사용하지 않는다.

<a id="validation-status"></a>
## 8. `-05` 검증 이력과 판정 한계

이 절의 성공 수치와 결과는 모두 이전 `-05` 작업의 기록이다. `-06`의 재실행 결과가 아니며, 사용자가 재보고한 카메라 문제가 해결됐다는 판정도 아니다. PIE에는 에디터 전용 부모가 남아 있어 Cook 후 Zone 이동을 발견하지 못했다. 현재 상태는 12절에 기록한다.

| 항목 | 현재 상태 | 증거와 남은 확인 |
|---|---|---|
| 저장된 맵·BP 기본값 검사 | 확인 | 최초 초점 `(78740, 35800, 4540)`, Arm `4000`, 구역 제한 `true`, 지면 추종 `true`를 초기 감사 JSON에서 확인했다. |
| C01 이동 차단 호출 경로 | 확인 | `Tick`이 입력 처리 후 `ConstrainFocusToZones`로 구역 밖 목표를 되돌리는 경로를 확인했다. |
| C01 기본값 변경 | 집중 검사 통과 | 개별 Zone strict 기본값 `false`, 전체 외곽 사용과 3,000cm 여유 유지. 수정 후 BP 기본값 재조회에서도 확인했다. |
| C02 최초 높이 덮어쓰기 경로 | 확인 | 지정한 초점 뒤 첫 지면 갱신과 즉시 반영이 실행되는 코드를 확인했다. |
| C02 높은 시점 보존 선택 기능 | 집중 검사 통과 | GameMode 선택을 Pawn에 전달하고 최초 유효 지면에서 높이 차이를 실행 중 설정 사본에 보존한다. |
| 합성 Zone 테스트 4개 | 통과 | 30/60/144FPS × near/far 조건과 strict·외곽·Idle 검사. |
| 높이 보존 Motion 테스트 3개 | 통과 | 실제 평면·경사 바닥, 늦은 지면, Focus/Follow, Reset·경계 갱신·재초기화, 기존 모드와 최소 간격 검사. |
| PIE `Test_Main` 카메라 테스트 | 당시 통과 | 최초 초점 `(78740,35800,4540)`, Arm `4000`; 틈 왕복 후 X=`75440.506`과 `76399.711`. Cook된 맵이 아니라 에디터 부모가 유지된 PIE 결과다. |
| 기존 카메라 테스트 재실행 | 통과 | 전체 41개 성공: 일반 성공 38개, 경고 포함 성공 3개, 실패·미실행 0개. |
| 전체 자동화·에셋 검증 | 통과 | `-05`의 새 실행에서 1,148개 성공: 일반 성공 1,053개, 경고 포함 성공 95개, 실패·미실행 0. 에셋 검증은 오류 0·경고 0. |
| `-05` Shipping 패키지 | 제작·게시 완료, 문제 재보고 | 최종 등록 2026-10-10 21:51:30 KST. 99개 배포 파일과 필수 에셋 37개·프레젠테이션 런타임 파일 19개·셰이더 라이브러리 4개를 확인했으나, 사용자가 카메라 문제를 재보고했다. |
| `-05` 패키지 초기 기동 | 당시 통과 | NullRHI, DX11/SM5, DX12/SM6, 기본 `FestaFiesta.exe`의 초기화·종료 코드 0. 본편의 Cook된 Zone 좌표와 플레이 조작 성공을 보장하는 검사는 아니었다. |
| ZIP·최종 폴더 무결성 | 통과 | ZIP의 99개 파일을 전부 풀어 SHA-256 대조, 누락·추가·불일치 0. 게시 복구 때 전체 1,084개 파일도 원본과 대조했다. |
| `-05` 패키지의 실제 W키·건축 수동 플레이 | 에이전트 미수행, 사용자 문제 재보고 | 앞선 자동 카메라 검사와 초기 광장 캡처는 PIE 결과다. 최종 Shipping의 조작 성공으로 확장해 해석하지 않는다. |

집중 검사와 전체 검사 모두 최초 초점 `(78740,35800,4540)` 및 Arm `4000`을 확인했다. 실제 구역 틈 왕복 후 X는 `75440.506`과 `76399.711`이었으며, 지면의 작은 높이 차이를 따라 Z는 각각 `4539.404`와 `4539.950`이었다. 경고 포함 성공을 실패 0과 혼동해 경고까지 0이라고 기록하지 않는다. 카메라 집중 검사에 포함된 경고는 경로 별칭, 기존 구역 접근점, 테스트용 잘못된 설정, 종료 시 NavMesh 조회 및 네트워크 조회 등이며, 각 원문은 보고서에 보존했다. 에셋 유효성 검사 자체는 오류·경고 모두 0이다.

## 9. 증거 파일과 진단 중 발견한 제한

| 근거 | 용도와 주의점 |
|---|---|
| [camera-audit.json](C:/FFProj/Artifacts/CameraRegression-20261010/camera-audit.json) | 수정 전 로드한 맵·BP 설정의 감사 기록이다. 여기에 남은 `constrain_focus_to_zones=True`는 초기 상태이며 수정 후 값으로 읽지 않는다. |
| [camera-audit-after.json](C:/FFProj/Artifacts/CameraRegression-20261010/camera-audit-after.json) | 새 빌드에서 다시 읽은 BP: strict=`False`, GameMode 높이 보존=`True`. 초기 Python 속성 조회 오류도 수정했다. |
| [inspect-camera.log](C:/FFProj/Artifacts/CameraRegression-20261010/inspect-camera.log) | 초기 에셋 검사를 수행한 실행 로그다. |
| [inspect_camera.py](C:/FFProj/Artifacts/CameraRegression-20261010/inspect_camera.py) | 초기 검사에 사용한 읽기 스크립트다. |
| [video-contact.png](C:/FFProj/Artifacts/CameraRegression-20261010/video-contact.png) | 화면 비교를 위한 참고 자료다. 좌표나 모든 입력 경로의 자동 검증 결과로 대체하지 않는다. |
| [수정 후 초기 화면](C:/FFProj/Artifacts/CameraRegression-20261010/initial-after.png) | 실제 PIE에서 첫 이동 전에 캡처한 광장 구도다. 에디터 전체 창이며 뷰포트는 789×318이다. 셰이더·텍스처 준비 표시가 남아 있어 최종 렌더링 품질 검증으로 사용하지 않는다. |
| [집중 검사 보고서](C:/FFProj/Artifacts/CameraRegression-20261010/FocusedValidation/FestaFiesta.Camera/Report/index.json), [로그](C:/FFProj/Artifacts/CameraRegression-20261010/FocusedValidation/FestaFiesta.Camera/Automation.log) | 2026-10-10 21:38 KST, 41개 성공·실패 0. 테스트 실행 시간 22.406초이며 빌드 시간은 별도다. |
| [전체 FestaFiesta 검사](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-05/Validation/FestaFiesta/Report/index.json), [ZoneSystem 검사](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-05/Validation/FF_ZoneSystem/Report/index.json) | 각각 988개·160개 성공. 새 소스로 다시 실행한 결과다. |
| [패키지 검증](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-05/Metadata/verification.json), [ZIP 무결성](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-05/Metadata/zip-integrity.json) | 배포 파일·컨테이너·초기 기동·압축 파일의 검증 결과다. |
| [게시 실패 원본](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-05/Metadata/failure-release-promotion-history.json), [복구 기록](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-05/Metadata/release-promotion-recovery.json) | 마지막 폴더 이동 실패를 숨기지 않고, 같은 검증 산출물로 복구한 방법을 남겼다. |

초기 감사 스크립트는 일부 정보를 Python 속성으로 직접 읽으려다 다음 오류도 남겼다. 이것들은 게임 실패 판정과 구분한다.

| 진단 항목 | 실패 내용 | 이후 검사 방법 |
|---|---|---|
| Controller 시점 모드 | `view_mode`라는 Python 속성을 찾지 못했다. | 실제 C++ Controller의 공개 `GetViewMode()` 결과를 검사한다. 이 조회 실패만으로 게임 시점이 잘못됐다고 판단하지 않는다. |
| Zone 폴리곤 | `polygon_points`라는 Python 속성을 찾지 못했다. | 실제 Zone의 `GetBoundaryPolygonLocalCm()`와 `ContainsWorldPoint()`로 경계 및 포함 여부를 검사한다. 초기 JSON만으로 구역 모양을 검증 완료했다고 쓰지 않는다. |

확정된 설정을 다시 읽고 실제 사용 경로로 검사하는 것이 이 문서의 원칙이다. API 조회 실패, 코드 분석, 자동화 실행, 사람이 보는 최종 화면은 서로 다른 종류의 증거이므로 결과를 섞지 않는다.

## 10. C03 — 빌드 완료 뒤 최종 폴더 게시 실패와 복구

### 어떤 실패였나

빌드·Cook·에셋 및 실행 검사·ZIP 생성까지 성공한 뒤, `Package-Win64.ps1`의 `Move-ReleaseDirectory`가 임시 폴더를 최종 폴더로 바꾸는 단계에서 Windows 오류 5, `Access to the path ... is denied`로 실패했다. 기존 재시도 8회도 통과하지 못해 자동 스크립트는 종료 코드 1을 반환했다. 이때 `latest.json`은 이전 `-04`를 유지하여 미완료 게시를 최신 버전으로 노출하지 않았다.

임시 폴더의 `FestaFiesta.exe`(PID 235680)와 Shipping 자식(PID 126280)이 실행 중인 것을 확인했다. 부모는 Explorer(PID 220804)였으며 자동 기동 검사 인수가 없는 별도 실행이었다. 두 게임 프로세스는 이후 종료됐고 에이전트가 강제로 종료하지 않았다. **종료 후에도 폴더 이동 거부가 남았으므로, 남은 잠금의 소유자는 확인하지 못했다.** 특정 백신이나 OneDrive가 원인이라고 추정해 단정하지 않는다.

### 이 실패 때문에 무엇을 변경했나

게임 소스·맵·검사 기준을 다시 바꾸지 않고 마지막 게시 절차를 복구했다. 회수 가능한 원본을 유지하면서 다음 순서로 진행했다.

1. ZIP의 모든 99개 파일을 읽어 `PACKAGE-SHA256SUMS.txt`와 SHA-256을 대조했다. 압축 파일 자체의 SHA-256도 일치했다.
2. 빌드 상태가 `validated`, 품질 예외가 `false`이며, 전체 검사 실패·미실행이 0이고 네 초기 기동 검사가 모두 성공했는지 재확인했다.
3. 원본과 목적지가 `C:/Builds/FestaFiesta` 아래의 의도한 실제 디렉터리인지 검사했다. 기존 목적지는 덮어쓰지 않고, 재분석 지점/링크를 따라 복사하지 않도록 제한했다.
4. 이동 재시도가 계속 거부되어 새 최종 폴더로 복사했다. 전체 1,084개 파일의 SHA-256을 원본과 비교해 모두 일치하는 것을 확인했다.
5. 최종 검증 메타데이터의 경로를 갱신하고, 원래 `failure.json`은 삭제하지 않고 실패 이력 파일로 보존했다.
6. 복구 기록을 저장한 다음 `latest.json`을 원자적으로 교체해 `-05`를 최신 버전으로 등록했다.

복구 스크립트는 [complete_release_promotion.ps1](C:/FFProj/Artifacts/CameraRegression-20261010/complete_release_promotion.ps1), ZIP 검사는 [verify_release_zip.py](C:/FFProj/Artifacts/CameraRegression-20261010/verify_release_zip.py)에 있다. 이번 릴리스 경로에 한정된 복구 기록이며 일반 자동 빌드 실행기를 대체하지 않는다. 검증한 실행 파일이나 ZIP 내용은 변경하지 않았다. 임시 `.staging` 원본은 잠금 조사 근거로 보존했으며, 사용자가 실행할 경로는 아래 최종 폴더다.

## 11. `-05` 실행 파일과 ZIP 보관 기록

아래 파일은 제작·게시된 이전 빌드다. 사용자가 문제가 남았다고 재보고했으므로 수정 완료본으로 안내하지 않는다. `-06`은 아직 준비 중이며, 완료 후 12절에 새 산출물을 기록한다.

- [게임 실행 파일](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-05/Package/Windows/FestaFiesta.exe)
- [배포 ZIP](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-05/FF-Win64-Shipping-20261010-05.zip)
- [빌드 명세](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-05/Metadata/build-manifest.json)

ZIP 크기는 **1,083,016,397 bytes**, 압축 전 배포 파일 합계는 **1,412,083,401 bytes**다. ZIP은 폴더 전체를 풀고 그 안의 `FestaFiesta.exe`로 실행한다. 실행 파일 하나만 다른 장소로 옮기면 필요한 리소스를 찾지 못한다.

ZIP SHA-256:

```text
8876b750aa451d838e4139ba8e6344cb4c43bfd050fd22f3a14d1b78206dcdd5
```

동일 빌드의 코드·검사·파일 검증 기록은 실패 원인 대조를 위해 보관한다. `-05`의 무결성 성공은 배포한 파일이 일치한다는 의미이며 그 안의 Zone 좌표가 올바르다는 의미는 아니다.

<a id="cook-parent-06"></a>
## 12. C04 — Cook에서 에디터 전용 부모가 제거된 문제와 `-06` 수정

### `-05`에서 해결되지 않은 실제 원인

본편의 13개 Zone은 `FFZAZoneLayoutManager_3`라는 관리 액터의 SceneRoot에 연결돼 있었다. 이 관리 액터의 월드 위치는 `(83920, 34650, 450)`cm이며, `IsEditorOnly()`는 `true`를 반환한다. 에디터에서 구역을 관리하는 도구이므로 Cook된 게임에서는 제거되는 액터다.

문제는 **게임에 남아야 하는 Zone의 위치가, 게임에서 제거될 부모를 기준으로 저장돼 있었다는 것**이다. `-05`의 Cook 결과에서는 부모 연결 `AttachParent`가 없어졌지만 Zone의 `RelativeLocation`은 그대로 남았다. 상대 위치가 부모 없는 월드 위치처럼 사용돼 모든 Zone이 `(-83920, -34650, -450)`cm만큼 이동했다.

비전공자 관점에서는 “광장의 동쪽 10m”라고 적은 표지에서 “광장 기준”만 지워진 상황과 같다. 숫자 10m는 남아 있지만 기준점이 달라져 전혀 다른 장소가 된다. 구역의 모양을 만드는 로컬 스플라인 점 **총 89개는 그대로 보존**돼 있었다. 점이 없어지거나 다각형이 찌그러진 것이 아니라, 다각형 전체를 맵 어디에 둘 것인지에 사용하는 좌표 기준이 사라진 것이다.

| 비교 대상 | 정상 원본 맵 | Cook된 `-05` |
|---|---|---|
| 런타임 Zone 수 | 13개 | 13개 |
| 로컬 스플라인 점 | 총 89개 | 점의 내용은 보존됨 |
| 에디터 전용 관리 부모 | 위치 `(83920,34650,450)`, Zone이 연결됨 | 부모가 제거돼 `AttachParent=null` |
| Zone 전체의 월드 좌표 차이 | 기준 상태 | `(-83920,-34650,-450)`cm 이동 |
| 카메라 외곽 최소 XY, 3,000cm 여유 포함 | `(-8285.475873, -2944.807807)` | `(-92205.475873, -37594.807807)` |
| 카메라 외곽 최대 XY, 3,000cm 여유 포함 | `(99055.000735, 96720)` | `(15135.000735, 62070)` |
| GameMode 최초 초점 | `(78740,35800,4540)` | 동일하게 정상 보존 |
| GameMode 최초 Arm | `4000`cm | 동일하게 정상 보존 |

카메라는 모든 Zone을 감싼 외곽 경계를 계산하고 초점을 그 안으로 제한한다. 정상 맵에서는 시작 X=78,740cm가 최대 X=99,055.000735cm 안에 있다. 그러나 `-05`에서 계산된 최대 X는 15,135.000735cm다. 시작 X가 경계 밖으로 판정되므로, 리그의 클램프가 이를 **약 X=15,135cm로 강제로 바꾼다.** 처음부터 다른 장소가 보이는 이유다.

초기 Yaw=0에서 W에 해당하는 화면 위쪽 이동은 월드 +X 방향이다. 잘못된 경계의 최대 X에서 시작한 카메라는 W를 눌러도 더 큰 X로 나갈 수 없다. 따라서 **시작 위치가 달라짐과 W 방향 이동 차단이 하나의 잘못된 외곽 경계로 연결**된다. 이전의 strict 옵션을 꺼도 `bUseZoneBounds=true`인 전체 외곽 제한은 살아 있으므로 이 문제는 남았다. 높이 보존 역시 잘못된 X좌표 경계를 고칠 수 없다.

### 이전 검사에서 놓친 이유와 정정

`-05`의 실제 맵 카메라 검사는 에디터 안의 PIE에서 실행됐다. 그 환경에는 `FFZAZoneLayoutManager_3`가 남아 있어 Zone의 월드 좌표가 정상적으로 계산됐다. 따라서 최초 초점과 구역 이동 검사는 통과했지만, 부모가 제거되는 **Cook 이후 데이터 구조**를 검사하지는 못했다.

초기 기동 스모크도 그래픽 초기화와 프로세스 종료를 확인하는 범위였다. ZIP SHA-256 검사는 만든 파일과 배포한 파일이 같은지 확인하는 범위였다. 이들 결과를 합쳐도 Cook된 본편 맵의 Zone 좌표가 원본과 같은지는 증명되지 않는다. “전체 자동화가 통과했으므로 사용자의 카메라 문제가 해결됐다”는 앞선 판정은 부족했다.

strict 옵션과 높이 보존 개선은 해당 동작의 개선으로 남긴다. 다만 이번 패키지 재보고의 근본 원인 수정으로 기록하지 않는다. 이번 수정의 핵심은 **Cook 후에도 런타임 Zone의 월드 좌표가 바뀌지 않는 콘텐츠 구조**다.

### 무엇을 변경하는가

원본 맵의 13개 Zone을 `Keep World` 방식으로 관리 액터에서 분리한다. 부모 연결을 끊되 현재 월드 위치·회전·크기와 경계 모양을 유지하는 작업이다. 에디터에서 정리할 때는 월드 위치에 영향을 주지 않는 기존 Outliner 폴더를 유지한다.

| 변경 대상 | 역할과 보존 조건 |
|---|---|
| [repair_camera_zone_parents.py](C:/FFProj/Tools/Build/repair_camera_zone_parents.py) | `Test_Main`에만 적용하는 재실행 가능한 이관 도구다. 예상한 13개 ID와 관리 부모를 확인하고 적용 전에 맵을 백업한다. |
| `Content/FestaFiesta/Maps/Test_Main.umap` | 13개 Zone의 부모 연결을 해제한다. 원본의 모든 액터 월드 Transform, Zone의 월드 폴리곤·출입 앵커·Outliner 폴더를 비교해 보존한 상태에서 저장한다. |
| [FFZAAuthoredZone.cpp](C:/FFProj/Plugins/FF_ZoneAuthoring/Source/FF_ZoneAuthoring/Private/FFZAAuthoredZone.cpp) | `CollectAuthoringIssues`에 런타임 Zone의 에디터 전용 조상 검사를 추가한다. 직접 부모 액터뿐 아니라 전체 SceneComponent 부모 체인의 컴포넌트와 소유 액터를 확인한다. |

제작 검사에서는 런타임 Zone이 에디터 전용 조상에 연결돼 있으면, Cook 시 월드 위치가 바뀔 수 있다는 오류와 `Keep World` 분리·Outliner 폴더 사용 안내를 제공한다. 단순히 이번 맵만 고치는 데 그치지 않고 같은 구조를 다시 만들었을 때 발견하기 위한 변경이다. 엔진의 `UWorld::IsDataValid`가 맵 액터 검사를 호출하므로 패키징 전 에셋 검증에도 연결된다. 수정한 맵을 포함한 이번 에셋 검증은 오류 0건·경고 0건으로 완료했다.

### 이관 도구의 저장·복구 규칙

도구는 기본적으로 읽기 검사만 한다. `FF_REPAIR_CAMERA_ZONE_PARENTS=1`일 때만 변경 적용을 허용한다. 이번 문서는 도구 사용 규칙을 기록하는 것이며, 별도의 게임 테스트 실행 지시가 아니다.

1. 프로젝트와 맵을 확인하고 예상한 13개 Zone ID가 모두 존재하는지 검사한다. 다른 부모에 연결됐거나 일부만 이관된 뜻밖의 상태는 자동으로 덮어쓰지 않고 오류로 중단한다.
2. 변경 전 모든 액터의 월드 위치·회전·크기, Zone의 월드 다각형·출입 앵커·폴더를 기록한다. 스냅샷의 좌표는 소수 다섯 자리, 회전 쿼터니언은 소수 일곱 자리로 기록해 비교한다.
3. `Artifacts/CameraZoneParentRepair/<실행시각>/Backups/Content/FestaFiesta/Maps/Test_Main.umap`에 원본을 복사하고 SHA-256이 원본과 같은지 확인한다.
4. 위치·회전·크기 모두 `KEEP_WORLD`로 부모 연결을 해제한다. 저장 전후의 기록을 비교해 월드 데이터 보존을 확인한다.
5. `AssetSaveTransaction`으로 저장을 관리한다. 실패 시 활성 트랜잭션은 무조건 롤백해 원본과 sidecar를 복원하고 결과를 보고서에 남긴다. 원본이 없거나 읽히지 않는 저장 실패에서도 해시 조회가 복구를 가로막지 않게 했다.
6. `report.json`에 적용 모드, 저장 여부, 변경 전후 파일 해시, 보존 여부와 남은 부모 연결 수를 기록한다. 이미 분리된 상태에서는 재저장하지 않으며, 읽기 검사 모드에서는 원본 파일 바이트가 바뀌지 않았는지 확인한다.

맵 파일의 해시는 부모 연결을 수정해 저장하면 달라질 수 있다. 이때 확인할 것은 “파일이 전과 똑같다”가 아니라, **의도한 연결 변경 외에 제작자가 정한 월드 위치와 구역 데이터가 보존됐는가**다. 백업과 전후 기록을 함께 보관해 이 차이를 설명할 수 있게 한다.

### `-06` 진행 상태와 확인 범위

#### C05 — 이관 스크립트 API 오류와 복구

첫 적용은 `zone.get_root_component()`가 UE 5.7의 해당 Python 객체에 노출돼 있지 않아 `AttributeError`로 중단됐다. 첫 연결 해제 전의 오류였고 저장도 수행되지 않았다. 원본 맵 SHA-256은 `87c4fa5247cd4b1a973623ed975b611dc902c4a103e16bed67ba3836d59e134f` 그대로였다. 노출된 속성을 사용하는 `zone.get_editor_property('scene_root')`로 바꿔 다시 실행했다. 실패 기록은 [첫 적용 보고서](C:/FFProj/Artifacts/CameraZoneParentRepair/20261010-130923-948744/report.json)에 보존했다.

재실행은 정상 종료했다. [적용 보고서](C:/FFProj/Artifacts/CameraZoneParentRepair/20261010-131213-347902/report.json)의 `saved=true`, `remaining_parent_count=0`, `authored_world_data_unchanged=true`를 확인했다. 이 비교는 메모리에 로드된 전체 액터의 저장 전후 보존 결과이며, Cook 데이터 비교는 별도로 수행한다. 저장 후 맵 SHA-256은 `7e71d6e18ac7ccef093830a77c3a426504d5f068e12b0648335e7e3cb7d33312`다.

`-05`의 근본 원인 분석은 [정적 분석 스크립트](C:/FFProj/Artifacts/CameraRegression-20261010/audit_cooked_zone_attachments.py)와 [Cook 전후 좌표 보고서](C:/FFProj/Artifacts/CameraRegression-20261010/cooked05-zone-attachment-audit.json)에 남겼다. 재쿠킹으로 증거가 덮어써지지 않도록 이전 맵과 Cook 파일도 별도 보관했다.

최종 기록: 2026-10-10 22:20 KST. 패키징 스크립트와 BuildCookRun 모두 종료 코드 0. 아래 결과는 `-06`에 대한 기록이며 `-05`의 테스트 결과를 재사용하지 않았다.

| 항목 | 현재 상태 | 판정 범위 |
|---|---|---|
| Cook된 `-05` 좌표 원인 | 확인 | 13개 Zone의 부모 제거, 상대 위치 잔존, 공통 좌표 이동과 외곽 경계 변경을 확인했다. |
| Keep World 이관 도구 | 적용 완료 | 예상 Zone 제한, 백업, 전후 월드 데이터 비교, 재실행과 복구 절차를 포함한다. |
| 원본 맵의 부모 연결 수정 | 적용·저장 완료 | 13개 연결 해제, 남은 부모 0개. 메모리의 저장 전후 월드 데이터 보존 확인. |
| 에디터 전용 조상 검사 | 반영·컴파일 완료 | 런타임 Zone의 전체 부모 체인을 검사한다. |
| 컴파일·에셋 유효성 검사 | 완료 | Editor·Shipping 빌드 성공. DataValidation 오류 0·경고 0. 게임을 조작하는 자동화 테스트는 실행하지 않았다. |
| Cook·Stage·Package·Archive | 완료 | BuildCookRun 228.64초, 종료 코드 0. 최종 폴더 게시도 복구 작업 없이 성공했다. |
| 정적 Cook 데이터 확인 | 일치 | 13개 Zone의 월드 좌표 오차 0, 경계 점 89개 일치. 최초 XY는 `(78740,35800)`으로 유지된다. |
| 패키지 구성·ZIP 무결성 | 완료 | 필수 에셋 37개, 영상·CEF 파일 19개, SM5·SM6 셰이더 라이브러리 4개 포함. ZIP 100개 파일의 SHA-256 모두 일치. |
| 게임 자동화·PIE·게임플레이 probe | 이번 작업에서 미수행 | 사용자가 직접 테스트하겠다는 최신 지시에 따른다. 작성 검토 중이던 gameplay probe 파일도 생성하지 않았다. |
| 패키지 launch/smoke 검사 | 이번 작업에서 미수행 | 프로세스를 실행하는 검사는 하지 않는다. `-05`의 기존 통과 수치를 `-06` 결과로 재사용하지 않는다. |
| 직접 카메라 조작·화면 확인 | 사용자 확인 예정 | 새 파일이 완성된 뒤 사용자가 확인한다. 에이전트가 플레이 성공을 주장하지 않는다. |

최종 산출물:

- [새 실행 파일 — FestaFiesta.exe](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-06/Package/Windows/FestaFiesta.exe)
- [전체 패키지 ZIP](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-06/FF-Win64-Shipping-20261010-06.zip)
- [빌드 기록](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-06/Metadata/build-manifest.json), [구성 검사](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-06/Metadata/verification.json), [ZIP 무결성](C:/Builds/FestaFiesta/FF-Win64-Shipping-20261010-06/Metadata/zip-integrity.json)
- [원본 맵 변경 전후 바이너리 비교](C:/FFProj/Artifacts/CameraRegression-20261010/source-zone-detachment-binary-audit.json), [수정 후 Cook 비교](C:/FFProj/Artifacts/CameraRegression-20261010/cooked06-zone-attachment-audit.json), [검사 중 파일 안정성 기록](C:/FFProj/Artifacts/CameraRegression-20261010/cooked06-audit-input-stability.json)

배포 폴더는 100개 파일·1,412,080,358바이트이며 ZIP은 1,083,029,639바이트다. ZIP SHA-256은 `8b9ab18ca1926b38f9fe11147171104fa13cbb63ce47ee85828ce94c1d56ff85`다. ZIP은 전체를 풀어 루트의 `FestaFiesta.exe`를 실행한다. 실행 파일만 다른 폴더로 옮기면 필요한 게임 데이터가 없어 실행되지 않는다.

새 Cook 결과의 카메라 외곽 범위는 원본과 동일하게 X `-8285.475873~99055.000735`, Y `-2944.807807~96720`이다. GameMode의 최초 `(78740,35800,4540)`와 Arm `4000`도 Cook된 기본값에 보존됐다. 이 결과는 잘못된 경계 때문에 시작 X가 이동하던 데이터 원인이 제거됐다는 근거다. 화면·키 입력·건축의 실제 조작 결과를 검사한 것으로 표현하지 않는다.

사용자 요청에 따른 테스트 생략은 매니페스트에 `automationTests=false`, `smokeTest=false`, `qualityOverride=true`와 사유로 기록했다. 따라서 자동 검증된 배포판을 가리키는 기존 `latest.json`은 갱신하지 않았다. 이번 사용자 확인용 빌드는 위의 **`FF-Win64-Shipping-20261010-06` 경로**로 실행하면 된다.
