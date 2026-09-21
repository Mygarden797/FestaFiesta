# FestaFiesta 인형뽑기 미니게임 개발·운영 가이드

## 1. 구현 결과

`바다 친구 인형뽑기`는 FestaFiesta의 기존 시설 → 미니게임 카탈로그 → 런타임 → 보상 흐름에 연결된 3D 미니게임이다. 플레이어는 제한 시간 75초 안에 3번 집게를 내릴 수 있으며, 집게 정렬과 인형별 난이도에 따라 획득 여부와 점수가 결정된다.

Claw Machine Simulator류 게임에서 중요한 조작 감각인 평면 이동, 하강, 집게 닫힘, 상승, 출구 복귀, 배출의 순서를 참고했으나 특정 게임의 에셋·UI·수치·코드를 복제하지 않았다. FestaFiesta의 밝은 해변 축제 톤, 짧은 플레이 세션, 시설 경제 및 보상 구조에 맞춘 독립 구현이다.

주요 계약은 다음과 같다.

- 시설 ID: `Booth_ActivityGame_ClawMachine`
- 미니게임 ID: `ClawMachine`
- 표시 이름: `바다 친구 인형뽑기`
- 제한 시간: `75초`
- 기본 시도 횟수: `3회`
- 최고 보상 기준: `1,500점`
- 최고 골드 보상: `150 Gold`
- 입력: WASD/방향키/왼쪽 스틱/화면 조이스틱 이동, Space/Enter/게임패드 A 하강, Esc 닫기
- 종료: 3회 배출 완료 시 `AttemptsExhausted`, 제한 시간 만료 시 기존 타임아웃 계약

## 2. 플레이 흐름

런타임만 게임 상태를 변경하고 3D 화면은 런타임 스냅샷을 표현만 한다. 이 분리 덕분에 프레임률이나 아트 교체가 판정 결과를 바꾸지 않는다.

`Aiming → Dropping → Closing → Lifting → Returning → Releasing → Aiming/종료`

1. `Aiming`: 정규화된 X/Y 플레이 영역에서 집게를 이동한다.
2. `Dropping`: 입력 지점을 고정하고 집게를 인형 더미 쪽으로 내린다.
3. `Closing`: 가장 잘 정렬된 유효 인형을 찾고 집기 품질을 계산한다.
4. `Lifting`: 성공 시 인형을 매단 채 올리고, 실패 시 집게만 올라온다.
5. `Returning`: 집게와 획득 인형을 배출구로 보간한다.
6. `Releasing`: 인형을 제거하고 점수/피드백을 한 번만 반영한다.
7. 남은 횟수가 있으면 다시 조작 상태로, 없으면 결과 화면으로 전환한다.

집기 품질은 플레이어 조작값에 인형의 물성을 결합해 계산한다.

```text
BaseQuality = Clamp(
    Alignment × 0.72
  + GripPower × 0.42
  - PrizeDifficulty × 0.38
  + SeededVariance,
  0, 1)

ContactAssist = SurfaceFriction × 0.075 + GripCompliance × 0.055
LoadPenalty  = NormalizedMass × 0.13 + NormalizedCenterOfMassOffset × 0.085
GripQuality  = Clamp(BaseQuality + ContactAssist - LoadPenalty, 0, 1)
```

`GripQuality >= RequiredGripQuality`일 때 성공한다. 정렬이 가장 큰 항이므로 플레이어 조작이 결과를 주도하고, 무거운 인형·마찰이 낮은 인형·무게중심이 치우친 인형은 실제 기계처럼 불리하다. 작은 기계 편차는 세션 시드로 고정되어 같은 시드와 입력에서 재현된다. Chaos 물리의 비결정적 충돌은 판정에 사용하지 않는다.

화면 표현은 판정과 분리된 물리 디지털트윈이다. 갤트리는 감쇠 스프링, 케이블은 수평 가속도를 입력으로 받는 진자, 획득 인형은 감쇠 추종 모델로 계산한다. 120Hz 목표 서브스텝을 한 프레임당 최대 8번까지만 수행해 히치가 있어도 발산과 CPU 스파이크를 막는다.

## 3. 프로젝트 연결 구조

```text
PDA_FFZS_Booth_ActivityGame_ClawMachine
  ├─ VisualMesh → SM_FFZS_Booth_ActivityGame_ClawMachine
  └─ CatalogThumbnail → T_UI_BuildCatalog_Booth_ActivityGame_ClawMachine
          ↓ FacilityDefinitionId
DA_Minigame_ClawMachine
  ├─ RuntimeClass → UFF_ClawMachineRuntime
  ├─ Rules → 기계·판정·물리 설정
  ├─ PrizeCatalogTable → DT_ClawMachinePrizes
  └─ ScreenClass → 디자이너 WBP 또는 네이티브 폴백
          ↓
UFF_ClawMachineScreen
  └─ UViewport → AFF_ClawMachineArena3D
          ↓
기존 UFF_MinigameSubsystem → 결과/보상 1회 처리
```

시설이 설치되고 플레이 가능한 라이프사이클 상태일 때 기존 시설 정보 UI의 미니게임 시작 흐름이 시설 ID로 정의를 찾는다. 미니게임 종료/취소/시설 제거 처리와 보상 지급은 기존 `UFF_MinigameSubsystem` 계약을 그대로 사용한다.

## 4. 기획자 데이터 수정 방법

인형 목록·배치 확률·점수·난이도·물성은 다음 데이터테이블에서 행 단위로 관리한다.

```text
/Game/FestaFiesta/Minigame/Data/DT_ClawMachinePrizes
```

행을 추가/복제/삭제하면 다음 신규 세션부터 인형 구성이 바로 바뀐다. `PrizeId`를 비우면 행 이름을 ID로 사용하며, 명시할 경우 반드시 고유해야 한다. 데이터테이블이 연결되면 `DA_Minigame_ClawMachine.Rules.PrizePool`은 실행에 사용하지 않고 회복용 폴백으로만 남는다.

기계 전체·판정·물리 표현 수치는 다음 에셋에서 관리한다.

```text
/Game/FestaFiesta/Minigame/Data/DA_Minigame_ClawMachine
```

저장 시 정의 검증이 범위 오류, 중복 ID, 빈 카탈로그, 잘못된 행 스트럭처를 차단한다.

### 전체 플레이 감각

| 필드 | 기본값 | 의미 |
|---|---:|---|
| `MovementSpeedNormalizedPerSecond` | 0.54 | 초당 집게 이동량 |
| `MovementMinNormalized` | (0.08, 0.08) | 조작 최소 경계 |
| `MovementMaxNormalized` | (0.92, 0.86) | 조작 최대 경계 |
| `HomePositionNormalized` | (0.14, 0.12) | 시작 및 배출 위치 |
| `GripRadiusNormalized` | 0.12 | 집게 기본 탐색 반경 |
| `GripPower` | 0.62 | 기계의 기본 집게 힘 |
| `DeterministicGripVariance` | 0.08 | 시드 기반 ±편차 |
| `RequiredGripQuality` | 0.62 | 성공 판정선 |
| `MaxAttempts` | 3 | 한 판 시도 횟수 |
| `PerfectAlignmentThreshold` | 0.82 | 퍼펙트 정렬 기준 |
| `PerfectGrabBonusScore` | 120 | 퍼펙트 추가 점수 |
| `PrizeCount` | 18 | 기계 안 표시 인형 수 |
| `PrizeMinSeparationNormalized` | 0.075 | 초기 배치 최소 간격 |

### 기계 애니메이션

| 필드 | 기본값 |
|---|---:|
| `DropDurationSeconds` | 0.72초 |
| `CloseDurationSeconds` | 0.28초 |
| `LiftDurationSeconds` | 0.64초 |
| `ReturnDurationSeconds` | 0.78초 |
| `ReleaseDurationSeconds` | 0.34초 |

시간을 줄이면 템포는 빨라지지만 획득 결과를 인지하기 어려워질 수 있다. 우선 10% 단위로 변경하고, 3회의 전체 플레이 시간이 지나치게 길거나 짧지 않은지 확인한다.

### 물리 디지털트윈 표현

| 필드 | 기본값 | 의미 |
|---|---:|---|
| `GantryResponseFrequencyHz` | 5.8Hz | 상부 레일이 목표 위치를 따라가는 응답성 |
| `GantryDampingRatio` | 0.86 | 1에 가까울수록 잔진동이 적음 |
| `PendulumLengthCm` | 175cm | 케이블 진자의 기준 길이 |
| `PendulumDampingPerSecond` | 2.35 | 케이블 흔들림 감쇠 |
| `MaxPendulumSwingDegrees` | 11° | 급격한 프레임에서도 보장하는 최대 스윙 |
| `HeldPrizeResponseFrequencyHz` | 4.2Hz | 매달린 인형의 느린 관성 추종 |

높은 응답 주파수와 낮은 감쇠비를 동시에 지나치게 조정하면 아케이드 기계보다 가벼운 드론처럼 보일 수 있다. 기본값에서 주파수는 0.5Hz, 감쇠는 0.1 단위로 조정한다.

### 기본 인형 풀

| ID | 표시 이름 | 점수 | 등장 가중치 | 집기 난이도 | 잡힘 반경 |
|---|---|---:|---:|---:|---:|
| `SeaSeal` | 하프물범 바다물개 | 280 | 1.35 | 0.34 | 0.115 |
| `CoralOctopus` | 산호빛 문어 | 420 | 0.92 | 0.49 | 0.100 |
| `BlueWhale` | 파도빛 고래 | 650 | 0.58 | 0.61 | 0.092 |

새 인형은 `DT_ClawMachinePrizes`의 행을 복제한 뒤 다음 순서로 설정한다.

1. `PrizeId`를 영문 고유 ID로 변경한다. 저장/로그/아트 연결 키이므로 출시 후에는 가급적 바꾸지 않는다.
2. `DisplayName`, `ScoreValue`, `SpawnWeight`를 설정한다.
3. `GripDifficulty`와 `CaptureRadiusNormalized`로 난이도를 조정한다.
4. `MassKg`, `SurfaceFriction`, `GripCompliance`, `CenterOfMassOffsetNormalized`로 물성을 설정한다.
5. 디자이너 메시가 준비되면 `PrizeMesh`, `PrizeMaterial`, `VisualScale`을 지정한다.
6. 전체 `SpawnWeight`의 합은 1일 필요가 없다. 각 값의 상대 비율이 등장 확률이다.

`MinigameId`, `CompatibleFacilityDefinitionIds`, `RuntimeClass`는 연결 계약이므로 밸런스 작업 중 변경하지 않는다. 보상 곡선은 `ScoreForMaxReward`와 `RewardGoldAtMaxScore`에서 조정한다.

## 5. 디자이너 에셋 교체 방법

### 인형 3D 에셋

각 `DT_ClawMachinePrizes` 행에 다음 Soft Reference가 있다.

- `PrizeMesh`: 원하는 Static Mesh. 비워 두면 가벼운 네이티브 봉제인형 형태를 사용한다.
- `PrizeMaterial`: 원하는 Material Interface. 비워 두면 `FallbackColor`로 동적 재질을 만든다.
- `VisualScale`: 인형별 표시 배율.

권장 제작 규칙은 피벗을 메시의 실제 무게중심 근처에 두고, 앞 방향과 업 축을 프로젝트 Static Mesh 규칙에 맞추며, 단일 LOD0 기준 약 2,000 triangles 이하와 1~2개 재질 슬롯을 목표로 하는 것이다. 결과 판정과 흔들림은 경량 디지털트윈이 담당하므로 복잡한 Physics Asset은 필요 없다.

### 조작 반응 카메라

카메라는 기본적으로 캐비닛 전면 880cm 지점의 `근접 플레이 구도`를 사용한다. 전체 기계를 멀리서 전시하는 구도가 아니라 전면 유리, 집게, 인형이 화면의 중심을 크게 채우는 실제 조작자 시점이다. 이 프리셋은 기존 WBP가 저장한 구형 카메라 값을 변경하지 않고 별도 버전 필드로 제공하므로 기존 에셋을 손상시키지 않는다.

`Aiming`에서는 이 기준 정면 구도로 강제 고정된다. WASD·방향키·아날로그 스틱은 갤트리만 이동하며 카메라 위치·회전·롤에 영향을 주지 않는다. 따라서 인형과 집게의 정렬을 일관된 화면에서 판독할 수 있다.

하강이 시작되면 입력을 확정한 `DropPositionNormalized`을 영상 타겟으로 사용한다. 카메라는 5차 `Smootherstep` 곡선으로 집기 위치를 확대하며, 하강 완료 지점의 집게 발·인형 접촉부를 바라본다. `Closing`에서는 최대 근접 구도를 유지하고, `Lifting` 초반에는 인형이 실제로 매달렸는지 확인한 뒤 속도·가속도가 튀지 않게 정면 카메라로 복귀한다.

`WBP_FF_ClawMachine` Class Defaults의 `VisualStyle > 3D Arena > Gameplay Camera`에서 근접 구도 사용 여부, 위치, 회전, 캐비닛 안전 이격거리를 조정한다. `Operator Camera`에서는 집기 위치 보정, 확대 돌리 거리, 최대 회전, 하강 확대 완료 비율, 상승 근접 유지 비율, 추종 속도를 조정한다. 기본 돌리 거리는 310cm이며 좌우 10°, 상하 12° 범위 안에서 접촉부를 따라간다. 확대 위치는 캐비닛 전면 평면과 기본 160cm 안전 이격거리로 제한되어 큰 돌리 값을 넣어도 유리·조작 패널을 관통하지 않는다.

### 조이스틱 조작 패널

Claw Machine Simulator 레퍼런스의 하단 기계 조작부 구조를 참고하되, 원본 로고·문구·에셋은 사용하지 않았다. FestaFiesta의 골드 제어판, 네이비 스틱 베이스, 산호색 노브로 재구성했다.

- `WBP_FF_ClawMachineJoystick`: 판·베이스·노브·문구를 디자이너가 별도로 교체하는 독립 WBP다.
- 키보드·게임패드 입력을 정규화한 후 화면 노브와 3D 기계 조이스틱에 동시 표시한다.
- 마우스 또는 터치로 노브를 드래그하면 같은 `[-1, 1]` 입력 축으로 집게를 조작한다.
- 대각선 입력은 반경 1인 원으로 클램프해 축 이동보다 빨라지지 않는다.
- 하강 이후에는 조이스틱을 중립으로 복귀하고 입력을 잠겨 연출과 판정을 보호한다.

`WBP_FF_ClawMachine` Class Defaults의 `VisualStyle > HUD > Joystick > 조이스틱 WBP`를 다른 자식 클래스로 바꾸면 런타임 코드 수정 없이 조작부 전체를 교체할 수 있다. 누락되거나 로드에 실패하면 같은 조작 계약을 갖는 네이티브 폴백이 생성된다.

### 3D 기계 전체 교체

`AFF_ClawMachineArena3D`의 Blueprint 자식을 만들면 기계 외형 전체를 디자이너가 소유할 수 있다.

- `ConfigureMachine(Definition, InitialSnapshot)`: 런타임 바인딩 직후 한 번 호출된다.
- `ApplyMachineSnapshot(Snapshot)`: 현재 집게/인형/단계 표현 갱신에 사용한다.
- 기본 구현을 유지하면서 개별 부품만 바꾸거나, 두 이벤트를 오버라이드해 전용 구성으로 교체할 수 있다.

네이티브 폴백은 단일 `UViewport` 월드 안에 기계, 집게, 최대 18개 인형을 구성한다. 투명 유리의 오버드로우와 다수 Actor Tick을 피하기 위해 불투명 프레임과 경량 Primitive Component를 사용한다.

화면 구성은 8px 기준 그리드에 맞춘 비대칭 아케이드 레이아웃이다. 왼쪽 70%는 3D 기계에만 사용하고, 오른쪽 상태 레일에 시간·점수·기회·기계 단계·주요 행동을 위에서 아래 순서로 고정했다. 색상은 네이비 배경, 청록 구조색, 산호 보조색, 골드 행동색의 네 역할로 제한했다. 상태값이나 조작 버튼을 3D 화면 위에 겹치지 않으므로 인형과 집게의 시인성을 유지한다.

### UMG 전체 교체

실제 디자이너 소유 화면은 `/Game/FestaFiesta/UI/Minigame/WBP_FF_ClawMachine`, 조이스틱은 `/Game/FestaFiesta/UI/Minigame/WBP_FF_ClawMachineJoystick`에 나뉘어 있다. 각각의 부모는 `UFF_ClawMachineScreen`과 `UFF_ClawMachineJoystickWidget`이며, 디자이너는 패널·브러시·폰트·여백·애니메이션을 직접 변경한다. 생성 명령을 다시 실행해도 기존 WBP를 덮어쓰지 않는다. 다음 이름은 메인 화면의 런타임 자동 바인딩 계약이다.

```text
Viewport_ClawMachine3D
Text_RemainingTime
Text_Score
Text_Attempts
Text_Status
Text_Feedback
Progress_Phase
Button_Drop
Button_Close
Panel_Result
Text_Result
```

WBP가 없거나 로드에 실패해도 `UFF_MinigameSubsystem`이 완성된 네이티브 화면으로 폴백하므로 기능이 중단되지 않는다. 3D 기계 전체는 WBP Class Defaults의 `VisualStyle.ArenaActorClass`, 기본/내부 배경 재질은 Arena Blueprint Class Defaults, 인형은 `DA_Minigame_ClawMachine.Rules.PrizePool`에서 교체한다.

### ImageGen 및 시설 에셋

임시 이미지는 사용하지 않았다. 두 이미지는 Codex 내장 ImageGen으로 생성했다.

| 용도 | 프로젝트 경로 |
|---|---|
| 시설 카탈로그 이미지 | `/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Images/T_UI_BuildCatalog_Booth_ActivityGame_ClawMachine` |
| 기계 내부 배경 텍스처 | `/Game/FestaFiesta/UI/Minigame/ClawMachine/T_MG_ClawMachine_Interior_ImageGen` |
| 내부 배경 재질 | `/Game/FestaFiesta/UI/Minigame/ClawMachine/M_MG_ClawMachine_Backdrop` |
| 시설 Static Mesh | `/Game/FestaFiesta/Zone/Test_Asset/Asset/SM_FFZS_Booth_ActivityGame_ClawMachine` |

원본은 다음 위치에 보관한다.

```text
Content/FestaFiesta/Building/FF_WBP_Building/Catalog/Images/T_UI_BuildCatalog_Booth_ActivityGame_ClawMachine.png
SourceAssets/UI/Minigame/ClawMachine/T_MG_ClawMachine_Interior_ImageGen.png
SourceAssets/Zone/FacilityMeshes/SM_FFZS_Booth_ActivityGame_ClawMachine.fbx
```

생성 프롬프트 기록:

```text
Use case: stylized-concept. Asset type: square Unreal Engine facility catalog thumbnail for FestaFiesta. Image 1 is the claw-machine edit target and Image 2 is the existing Shoot/Surfing/SandArt thumbnail style reference. Preserve the compact sea-animal claw machine, visible three-prong claw, prize chamber, joystick, button and chute. Restyle it as a production-ready handcrafted 3D facility render with a centered 3/4 isometric camera, generous dark margin, near-black navy studio void, subtle teal floor glow, warm amber practical lights, cool teal rim light, deep teal/weathered coral/antique brass palette. Match the object scale and cinematic contrast of the existing facility cards. No UI, labels, text, logos, people, border or watermark.
```

```text
Use case: stylized-concept. Asset type: replaceable Unreal 3D claw-machine rear panel. Cheerful underwater festival scenery with waves, bubbles, shells, coral and confetti; no characters or prizes; polished stylized game texture; landscape 3:2 orthographic panel; pale aqua, turquoise, cream, coral and gold palette; no text, logos, UI, watermark, claw machine, people, or plush toys.
```

시설 3D 원본은 `Tools/facilities/build_claw_machine_test_asset.py`로 결정론적으로 다시 만들 수 있다. 9개 프로젝트 공용 재질 슬롯, 지면 중앙 피벗, +Y 전면, 단순 충돌 계약을 따른다.

## 6. 성능 및 안정성 설계

- 게임 판정은 `UFF_ClawMachineRuntime` 한 곳에서만 수행한다.
- 이동과 단계 진행은 `DeltaSeconds` 기반이라 프레임률에 독립적이다.
- 한 프레임이 길어도 남은 시간을 다음 단계로 넘겨 최대 32단계 전이를 안전하게 처리한다.
- 인형 배치와 집기 편차는 세션 `RandomSeed`로 재현된다.
- 인형은 최대 64개까지 데이터 검증이 허용되지만 기본 렌더링 예산은 18개다.
- 인형 카탈로그는 DataTable을 세션 시작 시 한 번 정렬·검증해 런타임 규칙으로 복사한다.
- 인형별 메시/재질은 Soft Reference라 정의 로드 이전의 불필요한 강제 로드를 피한다.
- 물리 표현은 한 Actor Tick과 최대 8회의 고정 서브스텝으로 제한하고, 인형별 Tick·Chaos Body를 생성하지 않는다.
- 카메라는 조준 중 집게 위치를 추적하지 않고, 확정된 하강 위치만 연출 타겟으로 사용한다.
- 프레임 시간은 50ms로 제한하고 위치·쿼터니언 회전을 감쇠 보간해 히치에서도 카메라 특이값이 발생하지 않는다.
- 시설 메시: 1 Static Mesh, 9 공용 Material Slot, Lightmap 64, Nanite 비활성, CPU Access 비활성.
- 카탈로그 텍스처: UI 그룹, 최대 512, mip 없음.
- 내부 배경: World 그룹, 최대 1024, mip 사용.
- 입력 처리 중이 아닌 단계에서는 이동 입력을 즉시 0으로 만든다.
- 세션 ID가 다른 입력, NaN/Inf 입력, 조작 단계가 아닌 Drop 입력을 거부한다.
- 점수 합산은 `int32` 포화 연산으로 오버플로를 막는다.
- 결과/보상은 기존 Subsystem 경계에서 한 번만 반영한다.

기본 18개를 크게 늘릴 때에는 GPU 비용보다 UViewport 내 Primitive Component 수와 디자이너 인형 메시의 triangle/material 수를 먼저 확인한다.

## 7. 소스 및 콘텐츠 맵

| 영역 | 파일/에셋 |
|---|---|
| 데이터 구조 | `Source/FestaFiesta/Public/Minigame/ClawMachine/FF_ClawMachineTypes.h` |
| 순수 판정 함수 | `Source/FestaFiesta/Private/Minigame/ClawMachine/FF_ClawMachineRules.cpp` |
| 상태 머신 | `Source/FestaFiesta/Private/Minigame/FF_ClawMachineRuntime.cpp` |
| 정의 기본값 | `Source/FestaFiesta/Private/Minigame/FF_MinigameDefinition.cpp` |
| 3D 표현 | `Source/FestaFiesta/Private/UI/Minigame/FF_ClawMachineArena3D.cpp` |
| 화면/입력 | `Source/FestaFiesta/Private/UI/Minigame/FF_ClawMachineScreen.cpp` |
| 조이스틱 입력/표현 | `Source/FestaFiesta/Private/UI/Minigame/FF_ClawMachineJoystickWidget.cpp` |
| 비파괴 WBP 생성 | `Source/FestaFiesta/Private/UI/Minigame/FF_ClawMachineWbpBuilder.cpp` |
| 비파괴 인형 테이블 생성 | `Source/FestaFiesta/Private/UI/Minigame/FF_ClawMachinePrizeTableBuilder.cpp` |
| 단독 WBP 실행·캡처 | `Source/FestaFiesta/Private/UI/Minigame/FF_MinigameUIPreview.cpp` |
| 시설 생성 | `Tools/facilities/build_claw_machine_test_asset.py` |
| Unreal 임포트 | `Tools/minigame/import_claw_machine_content.py` |
| 정적 콘텐츠 검증 | `Tools/minigame/verify_claw_machine_content.py` |
| 미니게임 데이터 | `/Game/FestaFiesta/Minigame/Data/DA_Minigame_ClawMachine` |
| 인형 카탈로그 | `/Game/FestaFiesta/Minigame/Data/DT_ClawMachinePrizes` |
| 디자이너 조이스틱 | `/Game/FestaFiesta/UI/Minigame/WBP_FF_ClawMachineJoystick` |
| 시설 정의 | `/Game/FestaFiesta/Building/FF_Data_Building/Booth/PDA_FFZS_Booth_ActivityGame_ClawMachine` |

## 8. 재생성·임포트·검증 절차

에디터를 닫고 프로젝트 루트의 PowerShell에서 실행한다.

### 8.1 시설 FBX 재생성

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' `
  --background --factory-startup `
  --python 'Tools\facilities\build_claw_machine_test_asset.py'
```

현재 생성 결과 기준은 1,294 vertices, 1,918 polygons, 9 material slots이며 FBX SHA-256은 `7331bbc55365bc5b2a168aa29a87b937cc81e8eba2fa233057c3dd54449c25af`다. 스크립트나 Blender 버전을 바꾸면 해시는 달라질 수 있다.

### 8.2 C++ 빌드

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' `
  FestaFiestaEditor Win64 Development `
  'FestaFiesta.uproject' -WaitMutex -NoHotReload
```

### 8.3 인형 카탈로그 최초 생성

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecCmds=FF.BuildClawMachinePrizeTable,Quit' `
  -nop4 -nosplash -nullrhi -unattended -DDC-ForceMemoryCache
```

없을 때만 3개 시작 행을 생성한다. 이미 있으면 행 스트럭처·값을 검증한 뒤 재사용하며, 기획/아트 수정을 저장하지도 덮어쓰지도 않는다.

### 8.4 안전 임포트

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecutePythonScript=Tools\minigame\import_claw_machine_content.py' `
  -nop4 -nosplash -nullrhi -unattended -DDC-ForceMemoryCache
```

임포터가 만든 모든 패키지는 `FF.ClawMachine.Owner=FestaFiesta.ClawMachine.v1` 메타데이터를 가진다. 재실행 시 같은 소유권의 에셋만 갱신하고, 같은 경로에 디자이너가 만든 소유권 없는 에셋이 있으면 덮어쓰지 않고 실패한다.

`InstallCostKey`는 예외 가격을 사용하지 않으므로 비워 둔다. `FacilityDefinitionId`에서 `Cost.Booth.ActivityGame.ClawMachine`을 자동 생성하며, 임포터도 이 계약을 유지한다.

### 8.5 누락 WBP 최초 생성

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecCmds=FF.BuildClawMachineUI,Quit' `
  -nop4 -nosplash -nullrhi -unattended -DDC-ForceMemoryCache
```

이 명령은 메인 화면과 조이스틱 WBP가 없을 때만 각각 생성한다. 이미 존재하면 `REUSE` 로그를 남기고 디자이너 WidgetTree를 수정하지 않는다.

### 8.6 콘텐츠 계약 검증

```powershell
$verifyScript = (Resolve-Path 'Tools\minigame\verify_claw_machine_content.py').Path
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  "-ExecutePythonScript=$verifyScript" `
  -nop4 -nosplash -nullrhi -unattended -DDC-ForceMemoryCache
```

성공 시 로그에 `FF_CLAW_MACHINE_VERIFY_OK`가 기록된다.

### 8.7 디자이너 WBP 단독 실행·화면 캡처

다른 프론트엔드 UI 없이 실제 WBP, 실제 인형뽑기 런타임, 실제 3D 아레나만 실행한다. `ClawResult`를 사용하면 결과 상태를 확인할 수 있다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe' `
  'FestaFiesta.uproject' -game -windowed -ResX=1280 -ResY=720 -d3d11 `
  '-ExecCmds=DisableAllScreenMessages,FF.PreviewMinigameUI Claw Claw.png AutoExit' `
  -NoSplash -NoSound -NoLoadingScreen -nop4
```

프리뷰는 네이티브 폴백이 아니라 `WBP_FF_ClawMachine`의 Designer WidgetTree가 실제로 생성됐는지 검사한다. 런타임은 화면 생성 뒤 바인딩되므로, 화면은 `3D 배우 생성 완료`와 `게임 데이터 설정 완료` 상태를 분리한다. 이 지연 바인딩 처리가 빠지면 캐비닛만 기본 흰색 부품으로 보이고 인형이 생성되지 않는다.

## 9. 수동 QA 체크리스트

### 시설/연결

- 준비 단계 카탈로그에 `바다 친구 인형뽑기` 카드와 ImageGen 썸네일이 보인다.
- 시설 배치 미리보기, 설치, 회전, 제거가 정상 동작한다.
- 시설 메시가 바닥에 뜨거나 묻히지 않고 전면 방향이 올바르다.
- 시설이 플레이 가능 상태일 때 정보 패널에서 미니게임을 시작할 수 있다.
- 미니게임 정의가 다른 시설에서 노출되지 않는다.

### 조작/판정

- 키보드와 게임패드에서 대각선 이동 속도가 축 이동보다 빨라지지 않는다.
- 조작 경계를 넘어가지 않고 하강 중 이동 입력이 무시된다.
- 하강 → 닫힘 → 상승 → 복귀 → 배출이 끊기지 않는다.
- 급한 좌우 전환 후 케이블이 진자처럼 흔들리고 자연스럽게 감쇠한다.
- 획득 인형이 집게에 강성으로 붙지 않고 느린 관성으로 따라온다.
- 조준 중 카메라가 집게를 추적하지 않고 정면 정렬 구도를 유지한다.
- 근접 플레이 구도에서 전면 유리·집게·인형 수납부가 화면 중심을 크게 채우고 조작 패널이 함께 보인다.
- WASD·방향키·왼쪽 스틱을 누르는 동안 조준 카메라가 픽셀 단위로도 움직이지 않는다.
- 하강 중 집기 위치로 확대하고, 닫힘 구간에서 집게 발과 인형 접촉을 확인할 수 있다.
- 상승 초반에 획득/놓침 상태를 확인한 뒤 복귀 시점 전에 정면 구도로 돌아온다.
- 키보드·패드·마우스/터치 입력에서 화면 노브와 3D 조이스틱이 같은 방향으로 반응한다.
- 화면 조이스틱을 놓으면 중립으로 복귀하고 집게 이동이 즉시 멈춘다.
- 인형 중앙에 가까울수록 성공률이 명확하게 높다.
- 동일한 세션 시드/입력의 결과가 재현된다.
- 획득 인형은 다음 시도에서 다시 잡히지 않는다.
- 퍼펙트 획득 시 기본 점수와 120점 보너스가 한 번만 더해진다.
- 3회 배출 뒤 결과 패널이 표시되고 추가 Drop이 거부된다.

### 세션/보상/예외

- 정상 완료 시 점수 비례 Gold가 정확히 한 번 지급된다.
- Esc/닫기 취소 시 완료 보상이 지급되지 않는다.
- 제한 시간 만료가 기존 타임아웃 처리와 일치한다.
- 플레이 중 대상 시설 제거/세션 무효화 시 안전하게 닫힌다.
- 화면을 여러 번 열고 닫아도 입력 포커스와 UViewport가 누적되지 않는다.
- PIE 종료 및 패키지 빌드에서 누락 에셋 경고가 없다.

## 10. 현재 자동 검증 기준

2026-08-29 기준 다음 항목을 완료했다.

- `FestaFiestaEditor Win64 Development`: 빌드 성공.
- `DT_ClawMachinePrizes`: 전용 행 스트럭처, 3개 시작 행, 정의 Soft Reference 연결 검증 성공.
- Unreal Headless Map Check: 0 errors, 0 warnings.
- `verify_claw_machine_content.py`: 인형 카탈로그를 포함한 전체 콘텐츠 계약 성공.
- 시설 Static Mesh: 9개 공용 재질 슬롯, 단순 충돌 존재.
- 시설 카탈로그 원본: 1254×1254, 런타임 최대 512 설정.
- 내부 배경 원본: 1536×1024, 런타임 최대 1024 설정.
- 미니게임/시설 Data Asset 클래스, ID, 메시, 썸네일 참조 검증 성공.
- `WBP_FF_ClawMachine` 생성·Blueprint 컴파일·필수 BindWidget 계약 검증 성공.
- `WBP_FF_ClawMachineJoystick` 생성·Blueprint 컴파일·노브/캔버스 계약 검증 성공.
- `Claw_Joystick_Camera_v2.png`: 1280×720 실제 WBP 프리뷰에서 원형 조이스틱·DROP 버튼·3D 기계·인형 시야 비간섭 확인.
- `Claw_CloseGameplayView.png`: 1280×720 실제 WBP 프리뷰에서 근접 정면 구도, 집게·인형·전면 프레임·조작 패널 동시 노출 및 화면 잘림 없음 확인.
- `FF.PreviewMinigameUI Claw`: 실제 디자이너 WBP, 지연 런타임 바인딩, 18개 인형과 3지 집게 렌더링 및 1280×720 PNG 캡처 성공.
- 시설 `InstallCostKey=None` 및 표준 비용 키 자동 생성 계약 검증 성공.

실제 손맛, 난이도, 화면비, 패드 포커스와 보상 체감은 위 수동 QA 체크리스트에 따라 PIE에서 최종 승인한다.
