# 시설 콜리전으로 플레이어 막기 — 사용·확인 가이드

작성일: 2026-09-10 · 프로젝트: FestaFiesta · 확인 엔진: Unreal Engine 5.7.4

## 1. 무엇이 달라졌나요?

직접 조종하는 방문객 캐릭터가 설치된 시설을 뚫고 지나가지 않도록 적용했습니다. 시설 모델에 이미 들어 있는 콜리전을 그대로 사용합니다. 시설마다 새로운 박스를 배치하거나 별도 설정을 반복할 필요가 없습니다.

**콜리전**은 게임 안의 ‘보이지 않는 단단한 표면’입니다. 화면에 보이는 그림과 별도로, 캐릭터가 어디에서 멈춰야 하는지 알려 줍니다. 이번 변경은 시설에 이미 만들어 둔 이 표면을 플레이어에게 활성화한 것입니다.

- 직접 조종하는 방문객: 시설 콜리전에 막힙니다.
- NPC 군중: 이번 작업에서 이동·회피·충돌 코드를 변경하지 않았습니다.
- 설치 전 마우스를 따라다니는 미리보기: 캐릭터를 막지 않습니다.
- 시설 클릭과 선택: 기존 방식대로 동작합니다.
- 시설 이동·크기 변경·강화: 콜리전도 적용된 메시를 따라갑니다.
- 시설 철거: 이전 자리에 충돌이 남지 않습니다.
- 저장 후 불러오기: 복원된 시설의 콜리전이 다시 활성화됩니다.

## 2. 처음 실행하는 방법

이번 변경에는 새 충돌 채널과 C++ 코드가 포함됩니다. 팀에서 변경된 소스와 빌드 결과를 전달받은 뒤 사용하세요. 이미 이 작업 폴더에서 빌드된 결과를 사용하는 경우에는 에디터를 새로 실행하면 됩니다.

1. 언리얼에서 작업 중인 내용이 있다면 저장합니다.
2. 열려 있는 언리얼 에디터를 종료합니다. 새 채널을 읽도록 한 번 완전히 다시 실행해야 합니다.
3. 빌드가 필요한 팀원은 아래 ‘개발 담당자용 재현 명령’을 실행합니다. `Result: Succeeded`가 나오면 완료입니다.
4. 프로젝트 폴더의 `FestaFiesta.uproject`를 열고 에디터 상단 **재생(Play)**을 눌러 게임을 시작합니다. 게임의 시작 메뉴에서 축제 플레이 화면으로 진입합니다.

에디터의 일시적인 Live Coding만으로 확인하지 말고, 최초 적용 시에는 전체 에디터 빌드와 재실행을 해 주세요. 개별 시설의 Collision Preset을 수동으로 바꿀 필요는 없습니다.

## 3. 직접 걸어 보며 확인하기

아래 키는 기본값입니다. 키를 바꾼 경우 게임의 ‘조작 가이드’ 또는 설정의 ‘키보드 키 배치’에서 현재 키를 확인하세요.

1. 준비 시간에 **건축 탭** 또는 **B**를 눌러 시설 목록을 엽니다.
2. 시설을 선택합니다. 초록색 설치 가능 표시가 나오는 위치에서 **왼쪽 클릭 또는 Enter**로 설치합니다.
3. **오른쪽 클릭**으로 연속 배치를 종료합니다. 설정창 등 떠 있는 창도 닫습니다.
4. **Tab**으로 캐릭터 시점으로 전환합니다. 위에서 내려다보는 관찰 카메라가 아니라 직접 걷는 방문객으로 확인해야 합니다.
5. **WASD**로 시설의 벽이나 기둥을 향해 걸어갑니다. 캐릭터가 해당 콜리전 앞에서 멈추면 정상입니다.
6. **왼쪽 Shift**를 누르고 달려서 같은 위치에 접근합니다. 시설을 관통하지 않아야 합니다.
7. 벽에 비스듬하게 접근하고 모서리를 돌아봅니다. 실제 통로가 열려 있으면 이동할 수 있어야 합니다.
8. **Tab**으로 관찰 시점으로 돌아가 시설을 선택합니다. 선택이 기존처럼 동작하는지 확인합니다.
9. 게임 규칙상 이동·철거가 가능한 상태에서 시설을 이동하거나 철거하고, 다시 캐릭터로 이전 위치와 새 위치를 확인합니다.
10. 저장·불러오기를 사용하는 화면에서도 시설을 복원한 뒤 같은 걷기 검사를 합니다.

**낮은 턱이나 계단 위로 올라가는 것은 관통과 다릅니다.** 기존 캐릭터 이동 기능이 허용하는 높이는 올라갈 수 있습니다. 충돌 확인은 먼저 충분히 높은 벽이나 기둥에서 진행하세요.

검사할 때는 캐릭터가 시설 바깥의 빈 공간에 있도록 두세요. 시설을 캐릭터 위에 생성하거나 확대했을 때 캐릭터를 안전한 곳으로 자동 이동시키는 기능은 이번 변경에 포함되어 있지 않습니다.

## 4. 시설 하나만 뚫리거나, 빈 공간에서 막힐 때

화면에 보이는 시설 외형과 에셋에 저장된 콜리전은 서로 다를 수 있습니다. 이번 구현은 저장된 형상을 바꾸지 않고 사용합니다.

### 특정 시설만 뚫립니다

1. 실행을 중지합니다.
2. 해당 시설 정의 에셋에서 **Facility Specification → Visual Mesh**를 확인합니다. 강화 후 외형이 바뀐 시설은 현재 강화 단계의 **Level Visual Mesh**도 확인하세요. 이 항목이 비어 있으면 기본 Visual Mesh를 사용합니다. 메시 파일 이름은 보통 `SM_`으로 시작합니다.
3. 콘텐츠 브라우저에서 메시를 더블클릭하여 Static Mesh Editor를 엽니다.
4. 콜리전 표시 옵션에서 **Simple Collision**을 켜고, 충돌 형상이 있는지 확인합니다. 메뉴 위치는 에디터 언어와 레이아웃에 따라 다를 수 있습니다.
5. 충돌 형상이 없다면 에셋 제작 담당자에게 ‘이 메시의 단순 콜리전 또는 UCX가 필요하다’고 전달합니다. 설정을 켜는 것만으로 없는 형상이 생성되지는 않습니다.
6. 수정한 에셋을 저장한 뒤 다시 플레이하여 확인합니다.

단순 박스 형태의 새 시설을 직접 제작하는 경우 Static Mesh Editor의 **Collision → Add Box Simplified Collision**으로 충돌체를 만들고 크기와 위치를 맞출 수 있습니다. 출입구나 내부 통로가 있는 시설을 하나의 큰 박스로 감싸면 통로까지 막히므로, 벽·기둥에 맞는 여러 단순 형상이나 제작한 UCX를 사용하세요. 이 작업은 해당 메시를 사용하는 다른 장소에도 영향을 줍니다.

### 문이나 시설 내부가 비어 보이는데 들어갈 수 없습니다

기존 콜리전이 입구까지 덮고 있을 가능성이 있습니다. 화면의 그림이 아니라 Simple Collision 표시를 확인하고, 제작 담당자가 통로를 열어 주도록 수정합니다. 캐릭터 몸통이 들어갈 폭도 필요합니다. 현재 플레이어 캡슐 반지름은 34cm이므로 지름은 68cm이며, 실제 통로에는 추가 여유를 두는 편이 좋습니다.

### 모든 시설을 통과합니다

- 관찰 카메라 대신 **Tab으로 전환한 방문객**으로 검사했는지 확인합니다.
- 변경된 C++ 빌드가 성공했는지, 이후 에디터를 완전히 다시 실행했는지 확인합니다.
- 설치가 확정되고 시설 메시가 보이는 상태인지 확인합니다. 메시 로딩이 끝나기 전에는 그 메시의 충돌체도 사용할 수 없습니다.
- 개발 담당자는 프로젝트 설정의 Collision에서 `FacilityPlayerCollision` Object Channel과 `Ignore` 기본 응답을 확인합니다. 실제 플레이어 캡슐은 코드가 이 채널을 `Block`으로 설정합니다.

### Tab으로 전환되지 않습니다

먼저 떠 있는 설정창이나 모달 화면을 닫고 다시 시도합니다. 그래도 전환 대상이 없다면 개발 담당자가 GameMode의 `VisitorCharacterClass`와 `bPrepareVisitorCharacter`를 확인해야 합니다. 이 항목은 방문객 캐릭터를 준비하는 기존 설정입니다.

## 5. 이번에 확인한 결과

- UE 5.7.4 Development Editor 빌드: 성공.
- 관련 자동화 테스트: **6개 실행, 6개 성공, 0개 실패, 프로세스 종료 코드 0**. 이 중 5개는 경고를 포함한 성공입니다.
- 실제 플레이어 클래스의 캡슐을 한 번에 500cm 이동시키는 Sweep 검사에서 시설 메시 앞에 멈추는 것을 확인했습니다.
- 동일한 검사에서 새 채널에 참여하지 않은 일반 캐릭터는 통과하는 것을 확인했습니다.
- 반복 등록, 설치 미리보기, 시설 이동·철거, 저장 복원, 강화 메시 교체·기본 메시 대체·이전 비동기 요청 무시를 검증했습니다.
- 기존 시설 클릭 선택과 NPC 내비게이션 영역 관련 회귀 테스트를 통과했습니다.
- 시설 에셋 콜리전 점검: **시설 정의 34개가 참조하는 서로 다른 메시 33개 모두 기존 단순 콜리전을 보유**했습니다. 기본 메시 누락 0개, 점검 오류 0개이며 에셋을 수정하지 않았습니다.

자동화는 화면 렌더링을 생략한 환경에서 수행했습니다. 모든 시설의 문·계단·모서리를 사람이 직접 걸어 본 시각적 품질 검사를 완료했다는 뜻은 아닙니다. 3절의 절차는 팀원이 현장에서 최종 확인할 수 있도록 작성했습니다.

실행 로그에는 기존 서핑 재질 경로 누락과 군중 데이터의 bool 초기화 경고/오류도 출력되었습니다. 해당 코드는 이번에 변경하지 않았으며, 위 6개 테스트의 결과는 모두 성공입니다.

## 6. 개발 담당자용 구현 설명

시설 `VisualMesh`는 에셋에 저장된 BodySetup을 재사용합니다. 공유 에셋이나 Collision Complexity는 수정하지 않으며, 새로운 충돌 형상을 생성·복제하지 않습니다.

- 시설의 Object Type: `FacilityPlayerCollision` (`ECC_GameTraceChannel2`).
- 새 채널의 기본 응답: `Ignore`. 기존 Pawn이나 NPC 전체의 설정을 바꾸지 않습니다.
- 시설 메시: 등록되고 메시가 있을 때만 `QueryOnly`; `Pawn`만 `Block`, 나머지는 `Ignore`.
- `AFF_FestivalCharacter` 캡슐: 기존 `Pawn` Object Type과 월드 충돌을 유지하면서 시설 채널만 `Block`으로 참여합니다. 기존 Blueprint 응답도 `BeginPlay`에서 복구합니다.
- 충돌 판정: 언리얼 `CharacterMovement`의 기존 캡슐 Sweep를 사용합니다.
- 업데이트: 생성·등록·메시 적용·등록 해제·철거·저장 복원 시점에 수행합니다. 별도 Tick, 매 프레임 시설 순회, Hit/Overlap 이벤트, 시설 물리 시뮬레이션을 추가하지 않습니다.
- 클릭용 SelectionProxy와 NPC용 NavArea_Null은 기존 역할을 유지합니다.

최적화는 추가 충돌체와 게임 코드의 반복 검사를 줄이는 구조에 적용했습니다. 실제 FPS나 밀리초 개선 수치를 측정한 것은 아니며, 비용은 각 에셋의 기존 콜리전 복잡도와 시설 수에 영향을 받습니다. 복잡한 삼각형 콜리전으로 일괄 전환하지 마세요.

주요 파일(프로젝트 폴더 기준):

- `Config/DefaultEngine.ini`: 새 Object Channel 등록.
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Public/FF_FFZSCollision.h`: 게임·플러그인이 공유하는 채널 상수.
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSFacilityActor.cpp`: 메시 충돌 정책과 비동기 로드 적용.
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSWorldSubsystem.cpp`: 등록·등록 해제·저장 복원 연결.
- `Source/FestaFiesta/Private/Character/FF_FestivalCharacter.cpp`: 플레이어 캡슐만 참여.
- `Tools/facilities/audit_player_collision.py`: 에셋을 수정하지 않는 콜리전 점검 도구.

### 재현 명령

프로젝트 폴더에서 PowerShell을 열어 실행합니다. 엔진 설치 위치가 다르면 `$engine`을 바꾸세요.

```powershell
$engine = 'C:/Program Files/Epic Games/UE_5.7'
$project = (Resolve-Path './FestaFiesta.uproject').Path
& "$engine/Engine/Build/BatchFiles/Build.bat" FestaFiestaEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE -NoUBA -NoUBALocal
```

빌드가 성공한 뒤 관련 테스트를 실행합니다.

```powershell
$qa = Join-Path (Get-Location) 'Saved/FacilityPlayerCollision'
New-Item -ItemType Directory -Path $qa -Force | Out-Null
[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', (Join-Path $qa '_cache'), 'Process')
$filter = 'FestaFiesta.Character.FacilityCollision+FF_ZoneSystem.Integration.Collision+FF_ZoneSystem.World.FacilityLevels+FF_ZoneSystem.Integration.World.FacilityLookupAtWorldPoint+FF_ZoneSystem.Integration.Navigation.FacilityFootprintObstacleLifecycle+FF_ZoneSystem.Integration.World.FacilityUpgradeSizeCollisionRollbackAndSave'
& "$engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $project /Engine/Maps/Entry -unattended -nop4 -nosplash -nullrhi -nosound -NoSourceControl -DisablePlugins=RiderLink -DDC=InstalledNoZenLocalFallback -NoZenAutoLaunch "-abslog=$qa/Automation.log" "-ReportExportPath=$qa/Report" '-TestExit=Automation Test Queue Empty' "-ExecCmds=Automation RunTests $filter"
$testExit = $LASTEXITCODE
$passed = @(Select-String -LiteralPath "$qa/Automation.log" -Pattern 'Test Completed. Result=\{Success\}')
$failed = @(Select-String -LiteralPath "$qa/Automation.log" -Pattern 'Result=\{Fail\}|Fatal error:|No automation tests matched')
if ($testExit -ne 0 -or $passed.Count -ne 6 -or $failed.Count -gt 0) { throw '충돌 테스트 결과를 확인하세요.' }
```

결과 파일: `Saved/FacilityPlayerCollision/Report/index.json`, `Automation.log`, `Build.log`, `UBT.log`, `asset_audit.json`.

에셋 점검은 에디터의 Python 콘솔에서 아래 명령으로도 실행할 수 있습니다. 에셋을 저장하거나 수정하지 않습니다.

```python
import unreal
from pathlib import Path
script = Path(unreal.Paths.project_dir()) / 'Tools/facilities/audit_player_collision.py'
exec(compile(script.read_text(encoding='utf-8'), str(script), 'exec'))
```

## 참고 자료

- [Unreal Engine 5.7 — Collision Response Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/collision-response-reference-in-unreal-engine?application_version=5.7)
- [Unreal Engine 5.7 — Simple versus Complex Collision](https://dev.epicgames.com/documentation/en-us/unreal-engine/simple-versus-complex-collision-in-unreal-engine?application_version=5.7)
- [Unreal Engine 5.7 — Static Mesh 콜리전 설정](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-collisions-with-static-meshes-in-unreal-engine?application_version=5.7)
