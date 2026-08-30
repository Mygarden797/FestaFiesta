# FESTA FIESTA 3D 타깃 슈팅 개발·운영 가이드

## 1. 완성 범위

`TargetShooting`은 3D Aim Trainer 구조의 30초 물총 사격 미니게임이다.

- OS 마우스 커서는 커스텀 조준점으로 대체하며 두 좌표는 항상 동일하다.
- 실제 마우스 화면 좌표로 총기 조준 방향과 커스텀 조준점을 함께 움직인다.
- 단일 배경 이미지는 조준 중에도 확대·이동하지 않는다.
- 좌클릭은 방아쇠 입력만 담당하고, 회전한 총기의 총구에서 조준 방향으로 물 탄환을 발사한다.
- 탄환이 도착한 시점에 런타임이 명중·정밀 명중·정중앙·빗나감을 판정한다.
- 한 번에 과녁 1개와 탄환 1개만 활성화해 프레임 비용과 중복 입력을 제한한다.
- 게임 규칙, UI 스타일, 3D 아트 교체 지점을 서로 분리했다.

## 2. 실행 흐름

```text
시설 정보의 미니게임 시작
  → UFF_MinigameSubsystem
  → DA_Minigame_TargetShooting 로드
  → UFF_TargetShootingRuntime 생성
  → WBP_FF_TargetShooting 표시
  → UViewport에 AFF_TargetShootingArena3D 1회 생성
  → 마우스 화면 좌표를 Canvas 로컬 좌표로 변환
  → 정규화한 좌표로 CameraAimPointNormalized 갱신
  → 총기 회전과 커스텀 조준점을 같은 조준 상태로 갱신
  → 좌클릭으로 FireProjectileFromMuzzle(SessionId, Muzzle, AimPoint, TargetSerial)
  → 탄환 도착 후 런타임 단일 판정
  → 점수·콤보·정확도·TPS·반응 시간 HUD 반영
  → 제한 시간 종료 후 보상 결과 1회 확정
```

핵심 원칙은 화면이 점수를 예측하지 않는 것이다. UI는 포인터를 변환한 총기 조준 좌표,
현재 총구 좌표와 `TargetSerial`만 전달한다. 과녁 위젯 클릭 여부는 전달하지 않는다.
명중 판정, 점수, 콤보, 과녁 교체와 보상은 모두
`UFF_TargetShootingRuntime`이 소유한다.

## 3. 주요 소스 위치

| 책임 | 파일 |
| --- | --- |
| 기획 데이터 구조 | `Source/FestaFiesta/Public/Minigame/FF_MinigameTypes.h` |
| 순수 판정·점수 규칙 | `Source/FestaFiesta/Private/Minigame/TargetShooting/FF_TargetShootingRules.cpp` |
| 세션·과녁·탄환 런타임 | `Source/FestaFiesta/Private/Minigame/FF_TargetShootingRuntime.cpp` |
| FPS 총기 입력·HUD·3D 투영 | `Source/FestaFiesta/Private/UI/Minigame/FF_TargetShootingScreen.cpp` |
| 기본 3D 사격장 | `Source/FestaFiesta/Private/UI/Minigame/FF_TargetShootingArena3D.cpp` |
| 에디터 WBP 생성기 | `Source/FestaFiesta/Private/UI/Minigame/FF_TargetShootingWbpBuilder.cpp` |
| 런타임 자동화 | `Source/FestaFiesta/Private/Minigame/Tests/FF_TargetShootingRuntimeTests.cpp` |
| WBP·에셋 계약 자동화 | `Source/FestaFiesta/Private/UI/Minigame/FF_TargetShootingAssetTests.cpp` |

## 4. 기획자 데이터 수정

Unreal Editor에서 다음 에셋을 연다.

```text
/Game/FestaFiesta/Minigame/Data/DA_Minigame_TargetShooting
```

코드 수정 없이 다음 항목을 조절할 수 있다.

| 그룹 | 주요 값 | 영향 |
| --- | --- | --- |
| 기본 정보 | 한 판 플레이 시간 | 세션 제한 시간 |
| 보상 | 최대 보상 달성 점수, 클리어 최대 획득 골드 | 점수→골드 선형 매핑 |
| 과녁 노출 시간 | 초반/후반 과녁 유지 시간 | 진행도에 따른 반응 난도 |
| 과녁 크기·위치 | 초반/후반 반지름, 안전 여백 | 과녁 크기와 HUD 충돌 방지 |
| 기본 점수 | 명중 점수, 콤보, 빗나감 감점 | 기본 득점 곡선 |
| 정밀 명중 | 정밀/정중앙 비율과 추가 점수 | 조준 정확도 보상 |
| 반응 보너스 | 최고/종료 반응 시간, 최대 점수 | 빠른 조준 보상 |
| 보너스 과녁 | 확률, 점수 배율, 수명 배율 | 변주와 고점 구간 |
| 고급 연출 | 탄환 시작점, 속도, 최소 비행 시간 | 물 탄환 체감과 발사 간격 |

저장 시 `IsDataValid`가 잘못된 범위를 차단한다. 대표적인 실패 조건은 후반 과녁이
초반보다 커지는 경우, 안전 여백 안에 과녁이 들어가지 않는 경우, 정중앙 반지름이
정밀 영역보다 큰 경우다.

### 권장 밸런싱 순서

1. `DurationSeconds`와 보상 상한을 먼저 고정한다.
2. 과녁 수명과 크기로 평균 명중 수를 맞춘다.
3. 기본 점수와 콤보로 중간 실력 구간을 맞춘다.
4. 반응 보너스와 정밀 점수로 상위 실력 구간을 분리한다.
5. 마지막에 보너스 과녁 확률을 조절한다.

같은 `RandomSeed`와 입력 순서는 항상 같은 과녁 순서를 만든다. 밸런스 비교 시 같은
시드를 사용하면 변경 전후 결과를 재현할 수 있다.

## 5. 디자이너 에셋 교체

### 5.1 UI·텍스처 교체

프로덕션 화면의 외형 원본은 다음 WBP 에셋이다.

```text
/Game/FestaFiesta/UI/Minigame/WBP_FF_TargetShooting
```

- Widget Designer의 `Image_Backdrop > Brush`: 기본 단일 사격 배경
- Widget Designer의 `Image_Target > Brush`: 미니멀 원형 과녁
- Widget Designer의 `Image_WaterBlaster > Brush`: 전경 물총
- 각 Border·Text·ProgressBar·Button의 Brush, Font와 Color: HUD 전체 스타일
- Class Defaults의 `Visual Style`: 이동량, 크기, 총구·회전축, 반동처럼 연출에 필요한 값

디자이너 WBP를 사용하면 런타임은 저장된 Brush, Tint, Font와 Color를 덮어쓰지 않는다.
C++은 텍스트 값, 위치, 표시 여부, 진행률, 반동과 판정만 갱신한다. 코드에 남은 색상 토큰과
소프트 텍스처 경로는 WBP가 없을 때 생성되는 네이티브 비상 폴백 전용이다.

화면 전체를 다른 에셋으로 교체할 때는 `WBP_FF_TargetShooting`을 복제해 같은 필수 위젯
계약을 유지한 뒤 `DA_Minigame_TargetShooting > Screen Class`에 새 WBP 클래스를 지정한다.
이 방식으로 C++ 수정 없이 슈팅 화면 전체 스킨을 교체할 수 있다.

`FF.RebuildTargetShootingUI`는 초기 레이아웃을 다시 만드는 명시적 개발 명령이므로 디자이너가
WBP 스타일을 수정한 이후에는 실행하지 않는다. 실행하면 현재 WBP 레이아웃을 기본 스캐폴드로
교체한다.

기본값은 `단일 배경 모드 = true`다. 이 모드에서는 검은 화면을 만들 수 있는
`Viewport_Arena3D`를 생성·표시하지 않고 천장, 기둥, 벽, 난간, 바닥 가이드도 모두 제외한다.
배경은 이미지 한 장을 확대나 이동 없이 고정해서 사용하며 과녁, 탄환과 물총만 UMG
레이어로 움직인다. 과녁에는 서보 암·볼트가 붙지 않는다. OS 커서 모양은 숨기되 실제
포인터 화면 좌표를 `CameraAimPointNormalized`로 변환하며, 커스텀 조준점과 물총 회전이
같은 조준 상태를 사용한다. 과녁 위치는 런타임 좌표만 사용하므로 배경과 함께 밀리지 않는다. 완성된 3D
환경을 사용할 때만 단일 배경 모드를 끄고 Arena 클래스를 쓴다.

### 5.2 전체 3D 아트 교체

1. `FF Target Shooting 3D Arena`의 Blueprint 자식 클래스를 만든다.
2. 원하는 Static Mesh, Niagara, Light, Material 컴포넌트를 구성한다.
3. 아래 Blueprint Native Event를 구현한다.

   - `Set Target Presentation`
   - `Set Projectile Presentation`
   - `Set Weapon Presentation`

4. `WBP_FF_TargetShooting > Visual Style > 3D 사격장 클래스`에 새 Blueprint를 지정한다.

이벤트가 받는 위치와 크기는 정규화된 권위 좌표다. 디자이너 Blueprint는 점수나
판정을 수정하지 말고 표시만 담당해야 한다. 새 클래스가 없으면 엔진 기본 도형으로
구성된 네이티브 3D 사격장이 자동 폴백한다.

### 5.3 WBP 위젯 이름 계약

다음 위젯은 이름과 타입을 유지한다.

- `Border_PlayArea`, `Canvas_PlayArea`, `Border_Target`, `Image_Target`
- `Image_WaterBlaster`, `Text_MuzzleFlash`, `Text_TargetCue`
- `Text_RemainingTime`, `Text_Score`, `Text_HitCount`, `Text_MissCount`, `Text_Streak`
- `Panel_Result`, `Text_Result`, `Button_Close`

누락되거나 계층이 잘못되면 네이티브 폴백 화면을 사용하며 에디터 자동화 테스트가
계약 오류를 보고한다.

`Border_Target`의 콘텐츠는 `Overlay_TargetVisual`이며 프로덕션 WBP에서는
`Image_Target`이 외형 전체를 담당한다. 네이티브 폴백 전용
`TargetVisual_AuthoritativeRings`는 디자이너 WBP 사용 시 표시하지 않는다.
디자이너는 Widget Designer의 `Image_Target > Brush`를 교체한다.
`Image_Target`의 이름·부모·Hit Test 설정은 유지해야 한다. 이미지 교체는 표시만 바꾸며
판정 반지름, 점수와 과녁 이동 규칙에는 영향을 주지 않는다.

## 6. FPS 총기 조준과 판정 일치 원리

`CameraAimPointNormalized`는 플레이 Canvas의 좌상단 `(0,0)`, 우하단 `(1,1)` 좌표다.

1. `FPointerEvent` 이동 이벤트와 매 프레임의 Slate 포인터에서 실제 화면 좌표를 읽는다.
2. `FGeometry::AbsoluteToLocal`로 DPI가 반영된 Canvas 로컬 좌표로 변환한다.
3. 로컬 좌표를 Canvas 크기로 나누고 `[0,1]`로 제한해 `CameraAimPointNormalized`에 직접 저장한다.
4. 발사 버튼을 누른 순간에도 같은 변환을 다시 실행해 이벤트·프레임 사이의 시차를 제거한다.
5. 단일 배경은 항상 고정한다.
6. 총기, 커스텀 조준점과 발사 목적지가 동일한 `CameraAimPointNormalized`를 사용한다.
7. 과녁은 런타임이 제공하는 별도 정규화 좌표로 이동한다.
8. 좌클릭은 방아쇠로만 처리하고, 총기 에셋의 `총구 위치`에서 현재 조준 좌표까지 탄환을 발사한다.

### 6.1 총열-조준점 직선 정렬 알고리즘

단일 배경 모드는 선형 화면 보정값으로 총기 각도를 추정하지 않는다. 매 프레임 다음 순서로
실제 총열 광선을 계산한다.

1. WBP 데이터의 `총기 회전축`과 `총구 위치`로 기본 총열 방향 벡터를 만든다.
2. 화면의 `회전축 → 포인터 조준 좌표` 벡터와 기본 총열 벡터의 최소 각도 차이를 `atan2`로 구한다.
3. 동일한 2D 회전 행렬을 총기와 총구에 적용해 총열이 포인터 조준 좌표를 정확히 향하게 한다.
4. 조준점, 명중 가능 표시와 런타임 발사 목적지가 모두 포인터에서 변환한 단일 좌표를 사용한다.
5. 탄환 위치는 총구와 목적지 사이를 시간에 따라 선형 보간하므로 총열 방향으로 직진한다.

따라서 감도 누적, Y축 반전, 회전 한도 같은 별도 상태 때문에 포인터와 조준점이 어긋나지 않는다.

총기 에셋 교체 시 `WBP_FF_TargetShooting > Class Defaults > Visual Style > Weapon`에서
`총기 회전축`과 `총구 위치`를 새 이미지 내부 좌표로 맞춘다. 두 값은 좌상단 `(0,0)`,
우하단 `(1,1)`의 정규화 좌표라서 해상도와 DPI가 바뀌어도 총구 궤적이 유지된다.

단일 배경 모드에서는 과녁과 피드백을 런타임 좌표에 직접 배치한다. 완성된 3D Arena 모드로
전환한 경우에만 카메라 행렬로 화면에 다시 투영한다. 따라서 보이는 과녁, 움직이는 조준점과
런타임 타원 판정이 서로 다른 좌표계를 사용하지 않는다.

OS 커서 이미지만 숨기며 포인터를 강제로 캡처하거나 상대 이동 모드로 전환하지 않는다.
따라서 조준점은 운영체제 포인터와 동일하게 화면 경계에서 멈추고 다시 들어오면 즉시 동기화된다.

## 7. ImageGen 에셋

최신 단일 배경 원본:

```text
SourceAssets/UI/Minigame/TargetShooting/T_MG_TargetShooting_Backdrop_Single_ImageGen_v4.png
```

Unreal 에셋:

```text
/Game/FestaFiesta/UI/Minigame/TargetShooting/T_MG_TargetShooting_Backdrop_Single_ImageGen_v4
```

미니멀 원형 과녁 원본과 Unreal 에셋:

```text
SourceAssets/UI/Minigame/TargetShooting/T_MG_TargetShooting_Target_Simple_ImageGen_v1.png
/Game/FestaFiesta/UI/Minigame/TargetShooting/T_MG_TargetShooting_Target_Simple_ImageGen_v1
```

임포트:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecutePythonScript=C:\GitHub\FestaTeam\8th-Team13-Final-Project\Tools\ui\import_target_shooting_backdrop_imagegen_v4.py' `
  -nop4 -nosplash -nullrhi -unattended -DDC-ForceMemoryCache
```

과녁만 다시 임포트할 때는 다음 스크립트를 실행한다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecutePythonScript=C:\GitHub\FestaTeam\8th-Team13-Final-Project\Tools\ui\import_target_shooting_target_imagegen_v1.py' `
  -nop4 -nosplash -nullrhi -unattended -DDC-ForceMemoryCache
```

정확한 프롬프트와 생성 이력은
`Design/UXUI/FestaFiesta_Minigame_Set/PROMPTS.md`에 기록한다. 배경에는 HUD, 과녁,
조준선, 숫자와 문자열을 굽지 않는다. 과녁 텍스처에는 원형 색상 구획만 포함하고 수명,
점수, 보너스 안내와 판정 링은 UMG 런타임 레이어가 표시한다.

## 8. 성능 설계

- 3D 아레나 Actor는 화면 생성 시 1회만 Spawn한다.
- Actor Tick은 비활성화했다.
- 플레이 중 과녁 Actor를 반복 Spawn/Destroy하지 않고 기존 Mesh Transform만 바꾼다.
- 활성 탄환은 최대 1개이며 별도 Actor를 만들지 않는다.
- 충돌·Overlap은 사용하지 않고 순수 수학 타원 판정을 사용한다.
- Dynamic Material Instance는 최초 접근 시 만들고 이후 재사용한다.
- UMG 타깃과 탄환 위젯도 세션 동안 재사용한다.
- 과녁 페인트 값이 바뀌지 않으면 Slate invalidation을 요청하지 않는다.
- Motion Blur, Grid, Selection Outline, On-screen Debug는 3D 뷰포트에서 끈다.
- 소프트 텍스처는 화면 초기화 시 한 번 해석한다.
- 3D 뷰포트 생성 실패 시 ImageGen 배경과 2D 권위 과녁으로 즉시 폴백한다.

추가 아트 제작 시 한 과녁을 여러 Actor로 분리하거나 Tick 가능한 Blueprint를 과녁마다
생성하지 않는다. 새 `Arena3DActorClass` 내부에서도 컴포넌트 재사용 방식을 유지한다.

## 9. 빌드·검증 명령

Editor 빌드:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' `
  FestaFiestaEditor Win64 Development `
  'C:\GitHub\FestaTeam\8th-Team13-Final-Project\FestaFiesta.uproject' `
  -WaitMutex -NoHotReloadFromIDE
```

TargetShooting 자동화 필터:

```text
FestaFiesta.Minigame.TargetShooting
```

주요 자동 검증 항목:

- 동일 시드 과녁 순서 재현
- 정밀/정중앙/보너스/반응 점수
- 탄환 비행 중 중복 발사 차단
- 발사 시점 반응 시간 보존
- 세션 종료 경계에서 단일 보상
- WBP 필수 위젯과 부모 계층
- ImageGen v4 단일 배경과 3D Arena 클래스 기본값
- 3D 뷰포트 실패 시 네이티브 폴백

## 10. 수동 QA 체크리스트

- 게임 시작 시 OS 커서가 보이지 않고 총기 조준점이 보이는가
- 포인터를 화면의 임의 지점으로 옮겼을 때 조준점 중심이 같은 위치에 표시되는가
- 마우스를 오른쪽으로 움직이면 총기가 오른쪽으로 회전하는가
- 좌우 끝까지 조준해도 총열 중심선과 조준점이 일직선으로 유지되는가
- 창 가장자리에서 포인터와 조준점이 같은 좌표로 멈추는가
- 총기 조준점과 과녁이 겹친 상태에서 발사하면 총구에서 탄환이 출발해 명중하는가
- 탄환이 총구에서 조준점까지 곡선이나 좌우 편향 없이 직선으로 이동하는가
- 과녁을 직접 클릭하지 않아도 현재 총기 조준 방향으로 발사되는가
- 빗나간 탄환은 현재 과녁을 유지하고 콤보만 초기화하는가
- 탄환 비행 중 추가 클릭이 중복 점수를 만들지 않는가
- 보너스 과녁의 색·수명·점수 배율이 데이터와 일치하는가
- 해상도와 DPI 변경 후 3D 과녁과 얇은 판정 링이 계속 겹치는가
- 30초 종료 직전 발사한 탄환이 경계에서 한 번만 처리되는가
- 결과 확인 후 기본 커서 상태가 정상 복구되는가
- 3D Arena 클래스를 비운 상태에서도 2D 폴백으로 플레이 가능한가

## 11. 변경 시 금지 사항

- UI에서 점수나 Hit/Miss를 직접 계산하지 않는다.
- `TargetSerial` 검증을 제거하지 않는다.
- 탄환 비행 중 과녁 수명 일시 정지를 제거하지 않는다.
- 보이는 과녁만 확대하고 권위 판정 반지름을 그대로 두지 않는다.
- 배경 이미지에 과녁·HUD·점수·문자열을 굽지 않는다.
- 기획 밸런스 값을 C++ 화면 코드에 새로 하드코딩하지 않는다.
- 기존 v2/AAA 원본을 새 이미지로 덮어쓰지 않는다. 새 버전 파일을 추가한다.
