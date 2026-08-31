# FestaFiesta 서핑 미니게임 개발·운영 가이드

## 1. 결과 요약

서핑은 30초 동안 바다를 자동 전진하며 장애물을 피하고 코인·하트·부스트를
수집하는 짧은 프리라이드 미니게임이다. 게임 규칙은 고정 스텝 런타임,
코스는 DataTable, 세션과 보상은 MinigameSubsystem, 화면은 교체 가능한 WBP와
생성 아트 기반 2.5D 표시 레이어로 분리했다.

- 플레이: 자동 전진, 2축 조향, 부스트, 충돌 무적 시간, 점수·등급·골드 보상
- 환경: 밝은 부산 연안 바다, 반복되는 무한 코스 세그먼트
- 입력: `WASD`/방향키/D-pad/왼쪽 스틱, 마우스 클릭 방향 조향,
  `Space`/게임패드 아래쪽 페이스 버튼 부스트
- 데이터: `DA_Minigame_Surfing`, `DT_SurfingCourse`
- 화면: `WBP_FF_Surfing_FreeRide`, 실패 시 동일 아트를 쓰는 네이티브 WBP 폴백
- 기본 이미지: `T_MG_Surfing_Backdrop_Final`, `T_MG_Surfing_Surfer_Final`,
  장애물·픽업 RGBA 6종
- 선택 3D: 완성된 메시·머티리얼을 별도 지정한 경우에만
  `bEnableSceneCapture3D`로 활성화

## 2. 참고 게임에서 적용한 원칙

특정 맵·수치·아트를 복제하지 않고 움직임의 설계 원칙만 가져왔다.

- [Planet Surf 공식 Steam 페이지](https://store.steampowered.com/app/2904670/Planet_Surf/)의
  “항상 움직임”, 경사면을 타고 속도를 유지하는 흐름에서 자동 전진과
  속도감을 채택했다.
- [Vivid Surf 공식 Steam 커뮤니티 설명](https://steamcommunity.com/app/1288580)의
  마찰감이 낮은 이동, 높은 숙련도 상한, 진입 장벽을 낮추는 선택적 보조 철학에서
  즉시 이해되는 2축 입력과 부드러운 시각 보간을 채택했다.
- FestaFiesta는 긴 정밀 플랫폼 게임이 아니므로 30초 세션, 세 가지 장애물,
  세 가지 픽업, 제한된 부스트로 범위를 줄였다.

## 3. 런타임 구조

```text
Facility interaction
  -> UFF_MinigameSubsystem
    -> UFF_SurfingDefinition (기획 데이터)
      -> UFF_SurfingRuntime (60 Hz 고정 스텝 판정)
        -> FFF_SurfingSnapshot (불변 표시 데이터)
          -> UFF_SurfingScreen (입력/HUD/기본 2.5D 표시)
            -> ImageGen 배경 + RGBA 서퍼/오브젝트
            -> AFF_SurfingSceneActor (선택형 3D 표시)
              -> AFF_SurfingCharacterActor (교체 가능한 3D 캐릭터)
```

중요 원칙:

- 점수·충돌·코스 진행은 UI나 월드 프레임률에 의존하지 않는다.
- 런타임은 `1/60s` 고정 스텝으로 판정하여 30/60/120 FPS에서 동일한 결과를 낸다.
- 키 입력 의도와 방향은 즉시 스냅샷에 공개하되 실제 위치·충돌은 다음 `1/60s`
  고정 스텝에서만 계산한다. 반대 키가 겹치면 마지막으로 누른 방향이 우선한다.
- UI는 `FFF_SurfingSnapshot`만 읽고 게임 규칙을 직접 수정하지 않는다.
- 코스 오브젝트는 세그먼트 풀을 재사용하므로 플레이 시간이 늘어도 개수가
  계속 증가하지 않는다.
- 서퍼 위치·방향은 표시 레이어에서 지수 보간하고, 뱅킹·보빙·배경 드리프트를
  추가한다. 방향 반전 시에는 `bResetPresentationLagOnDirectionChange`로 이전 방향의
  누적 보간 지연만 제거하여 즉각 반응하고, 이후 이동은 다시 부드럽게 보간한다.
- 표시 보간은 매 프레임 `DeltaTime`과 `1 - exp(-ResponsePerSecond * DeltaTime)`을
  사용한다. 따라서 30/60/120 FPS에서 같은 실제 시간이 지나면 같은 응답률에 도달한다.
- 장애물과 픽업은 진행 거리로 크기와 Z 순서를 바꿔 2D 이미지에서도 원근을 만든다.
- 배경은 단일 바다 이미지로 유지한다. 별도 `WakeGuide`, `ArenaGlass`, `ArenaShadow`,
  전체 화면 vignette를 겹치지 않아 수평선·회색 막처럼 보이는 UI 아티팩트를 방지한다.

## 4. 기획자 데이터 수정

### `DA_Minigame_Surfing`

콘텐츠 브라우저 경로:
`/Game/FestaFiesta/Minigame/Data/DA_Minigame_Surfing`

주요 항목:

| 영역 | 값 | 의미 |
| --- | --- | --- |
| 공통 | `DurationSeconds` | 플레이 시간 |
| 공통 | `ScoreForMaxReward` | 최대 보상 기준 점수 |
| 공통 | `RewardGoldAtMaxScore` | 최대 골드, `-1`은 축제 공통값 |
| Movement | `CharacterSpeedNormalizedPerSecond` | 기본 자동 전진 속도 |
| Movement | `SteeringStrength` | 좌우 조향 세기 |
| Movement | `ForwardInputSpeedInfluence` | W/S 속도 보정 폭 |
| Course | `CourseSegmentLengthNormalized` | 반복 세그먼트 길이 |
| Course | `CourseSegmentsBehind/Ahead` | 유지할 세그먼트 풀 범위 |
| Collision | `PlayerCollisionRadiusNormalized` | 플레이어 판정 반경 |
| Collision | `HitInvincibilitySeconds` | 피격 후 무적 시간 |
| Boost | `BoostDurationSeconds` | 부스트 지속 시간 |
| Boost | `BoostSpeedMultiplier` | 부스트 속도 배율 |
| Player | `MaxLives`, `InitialLives` | 목숨 제한과 시작값 |
| Player | `bEndRunWhenLivesDepleted` | 마지막 목숨 손실 즉시 `WIPEOUT`으로 종료 |

`bEndRunWhenLivesDepleted`가 켜진 기본 모드에서는 보호받지 않은 장애물 충돌로
`Lives`가 0이 되는 즉시 세션을 `Completed / LivesDepleted`로 확정한다. 마지막 충돌의
횟수와 점수 패널티를 먼저 반영한 뒤 종료하며, 같은 고정 스텝에 뒤따르던 픽업은
처리하지 않는다. 따라서 플레이어가 이미 획득한 점수 기반 골드 보상은 유지되고
결과 화면 제목은 `WIPEOUT`으로 표시된다. 옵션을 끄면 HP 0에서도 계속 진행하는
특수 모드를 데이터만으로 구성할 수 있다.
| Score | `DistanceScorePerMeter` | 이동 거리 점수 |
| Score | `CoinScore` | 코인 점수 |
| Score | `ObstacleDestroyScore` | 부스트 파괴 점수 |
| Score | `CollisionPenalty` | 충돌 감점 |

저장 시 C++ Data Validation이 비정상 범위, 빈 코스, 잘못된 클래스와 시설 연결을
거부한다. 한 번에 여러 변수를 바꾸기보다 목표 난이도를 정하고 속도, 조향,
판정 반경 순으로 한 항목씩 조정한다.

### `DT_SurfingCourse`

콘텐츠 브라우저 경로:
`/Game/FestaFiesta/Minigame/Data/DT_SurfingCourse`

각 행은 다음 데이터만 가진다.

- `Type`: Rock, Log, Buoy, Coin, Heart, BoostPickup
- `PositionNormalized`: 세그먼트 내부 위치, X/Y 모두 `-1..1`
- `CollisionRadiusNormalized`: 표시와 판정에 사용하는 반경
- `ScoreValueOverride`: `0`이면 Rules 기본 점수, 양수면 해당 행만 재정의
- `bEnabled`: 행을 삭제하지 않고 빠르게 제외

최소 한 개의 장애물과 한 개의 픽업이 활성화되어야 한다. 같은 진행선에 큰
장애물을 겹치지 말고, 플레이어 반경과 오브젝트 반경의 합만큼 탈출 공간을 남긴다.

## 5. 디자이너 에셋 교체

### 기본 2.5D 플레이 아트

`WBP_FF_Surfing_FreeRide`의 Class Defaults에서 `VisualStyle > Artwork`를 수정한다.

- `BackdropTexture`
- `SurferTexture`, `SurferTextureSize`
- `RockTexture`, `LogTexture`, `BuoyTexture`
- `CoinTexture`, `HeartTexture`, `BoostPickupTexture`
- `EntityTextureSize`

배경은 16:9, 텍스트·UI·플레이 오브젝트가 없는 이미지로 제작한다. 스프라이트는
정사각형 RGBA와 실제 투명 배경을 사용한다. 소스 원본은 `SourceAssets`에 보존하고
게임에서는 `/Game/FestaFiesta/UI/Minigame` 아래 임포트 자산을 참조한다.

최종 ImageGen 백드롭과 서퍼 임포트:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecutePythonScript=Tools/ui/import_surfing_final_assets.py' `
  -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache
```

생성 프롬프트와 출처는
`Design/UXUI/FestaFiesta_Minigame_Set/PROMPTS.md`에 기록되어 있다.

임포트 후 `Tools/ui/verify_surfing_final_assets.py`를 같은 방식으로 실행하면
크기, UI 압축, sRGB, mip, 스트리밍, FreeRide CDO의 실제 참조를 함께 검사한다.

### 선택형 3D 경로

기본값은 `VisualStyle > Presentation > bEnableSceneCapture3D = false`다. 현재
네이티브 SceneCapture의 BasicShapes는 디버그 폴백이므로 최종 플레이에 켜지 않는다.
다음 조건을 모두 만족할 때만 활성화한다.

1. 바다 머티리얼, 세 장애물 메시, 픽업 메시가 프로덕션 에셋으로 교체돼 있다.
2. `AFF_SurfingCharacterActor` 파생 Blueprint가 최종 캐릭터를 표시한다.
3. 독립 캡처에서 노출·알파·그림자·성능을 검증했다.

3D 캐릭터를 연결하는 절차:

1. `AFF_SurfingCharacterActor` 파생 Blueprint를 만든다.
2. `InitializeCharacter`, `ApplyPresentationState`, `AdvancePresentation` 계약을 구현한다.
3. 캐릭터가 별도 Actor를 사용하면 `GetPresentationActors`에서 함께 반환한다.
4. `WBP_FF_Surfing_FreeRide > VisualStyle > 3D Character > SurfingCharacterClass`에 지정한다.

3D 초기화에 실패하면 최종 2.5D 화면으로 안전하게 복귀한다. 게임플레이 런타임은
표시 경로 교체의 영향을 받지 않는다.

### 움직임 연출 튜닝

`VisualStyle > 3D Scene > Motion`에서 코드를 고치지 않고 다음을 조정한다.
플레이어 위치·회전 값과 뱅킹 값은 기본 2.5D 표시에도 사용한다.

- 플레이어 위치·회전 반응 속도
- 카메라 위치·회전 반응 속도
- 코스 오브젝트 보간 속도
- 서퍼 상하 바운스 크기
- 회전 뱅킹 배율과 최대 각도

값이 높으면 즉각적이고 낮으면 부드럽지만 느리다. 먼저 플레이어 위치 `8..14`,
회전 `6..12`, 카메라 위치 `3..7` 범위에서 조정한다.

## 6. 성능 설계

- 기본 2.5D 경로는 별도 SceneCapture·월드 Actor·동적 머티리얼을 만들지 않는다.
- UI 텍스처는 NoMipmaps/NeverStream으로 임포트하며 런타임에서 캐시된 자산을 사용한다.
- 오브젝트 위젯은 ID별로 재사용하여 매 프레임 생성·삭제하지 않는다.
- 선택형 SceneCapture는 1280×720 기본값이며 디자이너가 낮출 수 있다.
- 선택형 SceneCapture는 전용 ShowOnly 목록, 고정 노출, 비활성 Motion Blur/DOF/SSR을 사용한다.
- 자동 프레임 캡처와 수동 지연 캡처를 중복 호출하지 않는다.
- 캐릭터 ShowOnly 목록은 매 프레임 재구성하지 않고 0.5초 간격으로 변경 여부만 확인한다.
- 동적 코스 메시의 충돌·내비게이션·캡처 속성은 생성 시 한 번만 설정한다.
- 코스와 표시 오브젝트 모두 풀 범위가 제한되어 장시간 플레이에도 메모리가 선형 증가하지 않는다.
- 3D 초기화 실패 시 게임을 중단하지 않고 최종 2.5D 화면으로 전환한다.

기본 2.5D 경로에서는 RenderTarget 비용이 없다. 선택형 3D 저사양 프리셋이 필요하면
`RenderTargetWidth/Height`를 960×540으로 낮춘다. 게임 규칙이나 판정에는 영향이 없다.

## 7. 실행과 검증

에디터에서 서핑 시설을 배치한 뒤 운영 단계에서 시설 정보의 미니게임 버튼으로
진입한다. 확인 순서는 다음과 같다.

1. 30초 동안 자동 전진이 멈추지 않는지 확인한다.
2. 키보드, 마우스, 게임패드 입력이 모두 동작하는지 확인한다.
3. 빠르게 좌우를 바꿀 때 캐릭터와 카메라가 튀지 않는지 확인한다.
4. 장애물 피격 후 무적 시간 동안 중복 감점되지 않는지 확인한다.
5. 부스트 중 장애물이 파괴되고 충돌 감점이 없는지 확인한다.
6. 하트와 부스트가 최대치 이상 증가하지 않는지 확인한다.
7. 시간이 끝나면 점수, 등급, 골드가 한 번만 지급되는지 확인한다.
8. 16:9와 울트라와이드에서 HUD가 잘리지 않는지 확인한다.
9. `stat unit`, `stat gpu`로 프레임 타임을 기록한다. 선택형 3D를 켠 경우에만
   SceneCapture 비용을 별도 기록한다.

디자이너 WBP의 실제 렌더 프레임 캡처:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe' `
  'FestaFiesta.uproject' -game -windowed -ResX=1920 -ResY=1080 `
  '-ExecCmds=FF.PreviewMinigameUI Surf Surfing_Final.png AutoExit' `
  -nop4 -nosplash
```

결과는 `Saved/Screenshots/MinigameUI/Surfing_Final.png`에 저장된다. 이 캡처에서
최종 백드롭, RGBA 서퍼, 여섯 오브젝트, HUD가 모두 보이지 않으면 완료로 판정하지 않는다.

자동화 테스트 필터:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecCmds=Automation RunTests FestaFiesta.Minigame.Surfing;Quit' `
  -unattended -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache -TestExit='Automation Test Queue Empty'
```

빌드가 통과해도 최종 조작감과 난이도는 실제 플레이 테스트에서 확정한다.

## 8. 주요 파일

- `Source/FestaFiesta/Public/Minigame/Surfing/FF_SurfingTypes.h`
- `Source/FestaFiesta/Private/Minigame/FF_SurfingRuntime.cpp`
- `Source/FestaFiesta/Public/UI/Minigame/FF_SurfingScreen.h`
- `Source/FestaFiesta/Private/UI/Minigame/FF_SurfingScreen.cpp`
- `Source/FestaFiesta/Private/UI/Minigame/FF_SurfingSceneActor.cpp`
- `Source/FestaFiesta/Public/UI/Minigame/FF_SurfingCharacterActor.h`
- `Tools/ui/import_surfing_final_assets.py`
- `Tools/ui/verify_surfing_final_assets.py`
- `SourceAssets/UI/Minigame/Final/T_MG_Surfing_Backdrop_Final.png`
- `SourceAssets/UI/Minigame/Final/T_MG_Surfing_Surfer_Final.png`

변경 시 규칙, 표시, 에셋 임포트 중 어느 계층인지 먼저 구분한다. 게임 수치 변경은
Data Asset/DataTable, 시각 변경은 WBP/Character Blueprint, 판정 계약 변경만 C++에서 한다.
