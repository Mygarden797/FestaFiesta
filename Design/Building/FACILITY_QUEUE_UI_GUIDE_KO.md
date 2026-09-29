# 시설 매출·NPC 대기열·건축 화면 수정 가이드

작성일: 2026-09-29 · 대상: Festa Fiesta / Unreal Engine 5.7.4

이 문서는 프로그래밍을 처음 접하는 사람도 이번 문제의 원인을 설명하고, 수정된 게임을 확인하고, 요금·대기열·화면 설정을 직접 조정할 수 있도록 작성했습니다. 명령 예시는 프로젝트 폴더에서 PowerShell을 연 상태를 기준으로 합니다.

## 1. 먼저 이해할 것

시설 매출은 다음 순서로 발생합니다.

**손님이 방문할 시설을 발견 → 시설 선택 → 줄의 자기 자리까지 이동 → 순서가 되면 이용 → 이용 완료 → 요금 정산 → 시설 매출 표시**

이 중 앞부분이 끊기면 정산 코드가 정상이어도 매출은 0입니다. 또한 무료 시설은 이용자가 있어도 일반 이용 매출이 0인 것이 정상입니다.

| 용어 | 쉬운 설명 |
| --- | --- |
| NPC | 컴퓨터가 움직이는 축제 방문객 |
| 일행 / Party | 함께 입장하고 움직이는 손님 묶음 |
| 구역 / Zone | 게임이 축제장의 위치를 나누어 관리하는 단위 |
| 방문 후보 | 손님이 다음 목적지로 고를 수 있는 시설 목록 |
| 대기 슬롯 | 줄에서 특정 일행에게 배정한 자리 |
| 도착 확인 | 손님이 배정된 자리까지 실제 이동했다는 신호 |
| NavMesh | 캐릭터가 걸어갈 수 있는 지면을 나타낸 길 안내 데이터 |
| WBP | 버튼·글자·패널의 모양과 배치를 담는 Unreal 화면 에셋 |
| 빌드 | C++ 소스 코드를 실행 가능한 프로그램으로 만드는 과정 |
| 마이그레이션 | 기존 화면 에셋에 변경된 레이아웃·글꼴을 적용하는 과정 |
| 회귀 테스트 | 문제를 고친 뒤 같은 문제가 다시 생기지 않았는지 확인하는 자동 검사 |

## 2. 매출이 생기지 않았던 이유

수정 전 실제 실행 로그에는 `GZ_TestMain_08` 구역의 손님이 시설 19개를 모두 `Zone밖`으로 제외하고 방문 후보를 0개로 만드는 경고가 186회 반복되었습니다. 가까운 구역에서만 시설을 찾기 때문에, 손님이 출입구 쪽이나 빈 구역에 있으면 다른 구역에 열린 유료 부스가 있어도 발견하지 못하는 경우입니다.

수정은 가까운 시설을 우선하는 규칙을 유지하면서, 주변에 이용 가능한 후보가 하나도 없을 때 연결된 다른 구역도 찾도록 만드는 방향입니다. 도달 불가능한 구역까지 무조건 후보에 넣으면 손님이 벽을 향해 걷거나 멈추므로, 보행 경로 또는 구역 연결 정보를 함께 사용합니다.

기본 설정에서는 구역 연결을 따라 여러 구역을 탐색합니다. 보행 거리 행렬 기능을 켰고 현재 구역 정보도 준비되어 있으면 실제 보행 경로가 있는 구역만 확장합니다. 가까운 시설이 이미 있을 때의 우선순위와 후보 수 제한 12개는 유지합니다. 모든 유료 시설에 같은 매출을 보장하거나 방문객을 강제로 보내는 수정은 아닙니다.

### 무료 시설과 유료 시설 구분하기

모든 시설이 자동으로 돈을 버는 것은 아닙니다. 도로·조명·편의·휴게 시설처럼 기본 이용료가 0인 시설은 동선이나 만족도를 돕습니다. 유료 프로그램을 운영하는 시설은 일반 이용 수익과 프로그램 수익을 구분해서 보아야 합니다. 이번 수정에서 무료 시설 전체에 임의의 요금을 붙이지는 않았습니다.

현재 일반 수익표에는 114종이 있고, 음식·공예·게임·굿즈·야간 부스 등 31종이 유료입니다. 나머지 83종의 일반 이용 매출은 0으로 설정되어 있습니다. 체험장·관객댄스 무대의 `Program_SunsetYoga`, 공예 시설의 `Program_SandArtWorkshop` 같은 유료 프로그램은 별도 편성과 참가 인원이 필요합니다. 현재 포토존에는 기본 유료 프로그램이 없습니다.

일반 부스는 이용 인원과 단가를 바탕으로 플레이어 몫을 계산합니다. 예를 들어 강화·시너지 배율이 1인 경우, 4명이 1인 100 Gold 시설을 이용하고 수수료율이 25%라면 플레이어에게 100 Gold가 들어옵니다. 대기 중·이동 중·포기·취소는 과금하지 않습니다. 유료 프로그램은 시작 시각의 확정 예약 인원 × 참가비를 해당 시설 원장에 한 번 기록하고, 일반 부스 수수료율과 강화 배율을 다시 곱하지 않습니다.

확인할 때는 다음 세 가지를 따로 보세요.

1. **방문 자체가 없는가?** 운영 중인지, 손님이 걸어올 수 있는지, 목적지 후보에 포함되는지 확인합니다.
2. **방문은 하지만 이용이 끝나지 않는가?** 대기열 자리와 시설 입구에 도착하는지 확인합니다.
3. **이용은 끝났지만 0원인가?** 시설 요금표가 무료인지, 별도 유료 프로그램을 편성해야 하는지 확인합니다.

## 3. 줄이 흐트러졌던 이유

줄은 화면에 그린 선만으로 작동하지 않습니다. ‘몇 번째 자리인지’, ‘어디로 걸어가는지’, ‘실제로 도착했는지’가 서로 일치해야 합니다.

| 문제 | 화면에서 보이는 현상 | 수정 원리 |
| --- | --- | --- |
| 실제 도착을 기다리지 않고 서비스 시작 | 줄에 오기도 전에 이용이 끝나거나 손님이 줄을 건너뜀 | 대기열 자리 도착을 확인한 뒤 서비스 진행 |
| 앞사람이 빠져 자리 이동 후 도착 신호 누락 | 줄의 앞자리가 비어 있거나 다음 이용이 멈춤 | 새 자리에서도 도착을 다시 확인 |
| 한 칸 이동이 경로 재계산 기준보다 작음 | 이전 자리나 엉뚱한 경로를 계속 따라감 | 슬롯이 바뀌면 기존 이동 경로를 무효화 |
| 일행의 이동 경로를 따로 계산하지 않음 | 동행이 시설 모서리에 막히거나 줄을 가로지름 | 각자 배정된 자리로 가는 보행 경로 계산 |
| 경로 계산 중 순회 시작점이 바뀜 | 일부 동행의 경로 갱신이 누락됨 | 한 번의 순회 동안 시작점을 고정 |
| 일행의 도착 판정 범위가 너무 넓음 | 줄에서 멀리 떨어져 있는데 도착했다고 판단 | 줄에 맞는 가까운 거리 기준 사용 |
| 도착 후에도 관성·회피 이동 적용 | 제자리에 도착한 손님이 다시 밀려남 | 자기 자리에 도착하면 안정적으로 정지 |

도착 기준은 기본 45cm입니다. NPC를 순간이동시키는 대신, 자리 근처까지 걸어오면 정지시킵니다. 새 자리가 생기면 다시 걷습니다. 줄의 중심점과 방향은 시설에 작성된 대기열 데이터를 사용합니다.

## 4. 화면을 읽기 쉽게 만드는 기준

화면에는 제목, 현재 상태, 다음 행동이 구분되어 보여야 합니다. 한 칸에 글자를 더 많이 넣기 위해 글자만 줄이면 설명과 가격을 읽기 어려워집니다. 이번 작업은 한글 글꼴을 통일하고, 제목·본문·가격의 굵기와 크기를 구분하며, 내용이 길어지는 곳에 줄바꿈과 스크롤 공간을 확보하는 방식입니다.

- **조합 도감:** 제목과 검색·필터가 서로 침범하지 않도록 화면 폭에 맞춰 배치하고, 수집 보상 항목도 한 줄에 무리하게 몰아넣지 않습니다.
- **시설 정보:** 탭을 한 줄로 정리해 강화 전후 수치와 비용·잔액이 함께 보이게 했습니다. 화면 높이에 맞춰 창을 제한하고 긴 본문은 내부에서 스크롤합니다.
- **건축 카탈로그:** 카드의 시설 이름과 가격을 읽을 수 있게 공간을 확보하고, 화면 폭에 맞는 기존 카탈로그의 크기 조절을 유지합니다.
- **건축·배치 모드:** 조작 안내의 글자와 간격을 정돈하고, 상단의 별도 골드 배지를 숨깁니다.

골드 제거는 **상단 표시를 없애는 작업**입니다. 실제 보유금, 설치비 차감, 비용 안내, 잔액 부족 판정은 그대로 작동합니다. 예전 화면 에셋을 불러와도 상단 배지가 다시 나타나지 않도록 실행 코드에서도 숨깁니다.

최종 크기 기준은 카탈로그 패널 높이 378, 카드 높이 264, 이름 영역 높이 96입니다. 이는 화면 배율을 적용하기 전 UI 좌표입니다. 시설명 15, 가격 17, 본문·안내 14 이상을 기준으로 조정했고, 도감 상세 제목은 24, 시설 정보 제목은 21을 사용합니다. 배치 카드의 모드 제목과 두 버튼은 썸네일 옆의 좁은 칸에서 꺼내 카드 전체 폭을 쓰는 별도 줄로 배치했습니다.

## 5. 수정본 적용 방법

### 후속 요청: 도감 닫기·전체 조합·보상 영역

도감 우측 상단의 **X 버튼**은 목록이나 상세 화면 어디에서든 도감을 닫고 원래 카탈로그 또는 시설 정보 화면으로 돌아갑니다. 카탈로그의 검색·스크롤과 시설 선택 상태는 기존 복귀 처리로 보존합니다. 왼쪽 제목 버튼과 ESC는 기존처럼 상세에서 목록으로 먼저 돌아갑니다.

왼쪽의 부스·화장실·안내 등 상세 분류는 제거하고 **전체 조합** 하나만 표시합니다. 이를 누르면 시설·분류 조건이 해제됩니다. 검색창과 위쪽의 발견·미발견·적용 중·수령 가능 필터는 계속 사용할 수 있습니다.

수집 보상은 가로 스크롤 안에서 고정된 최소 너비만 사용해 넓은 화면의 중간에서 끝나던 구조를 수정했습니다. 이제 오른쪽 화면 여백까지 가용 폭을 채우고 보상 카드를 고르게 배치합니다. 작은 화면에서는 보상 카드를 읽을 수 있는 너비를 유지하며 가로 스크롤로 접근합니다.

후속 변경의 저장된 도감 화면 적용은 아래의 전용 명령을 사용합니다. **에디터를 저장·종료하고 최신 에디터 빌드를 마친 뒤** 실행하세요. 도감 화면·카드·보상·진입 버튼 WBP 4개를 백업하고, 다른 UI의 변경 여부와 재실행 결과를 검사합니다.

```powershell
$projectFile = Join-Path (Get-Location) 'FestaFiesta.uproject'
$codexScript = Join-Path (Get-Location) 'Tools/BuildModeClarity/apply_codex_navigation.py'
& 'C:\Unreal\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $projectFile /Engine/Maps/Entry -unattended -nop4 -nullrhi "-ExecutePythonScript=$codexScript"
```

결과는 `Saved/BuildModeClarity/codex-navigation/latest-result.json`에 저장됩니다. 성공은 `status: PASS`입니다. 후속 제작 코드는 `FF_FacilityCodexNavigationBuilder.cpp`, 적용 명령은 `FF.ApplyCodexNavigation`입니다. 아래 11절의 통합 적용 스크립트는 최신 디자이너 정리까지 함께 적용합니다.

프로젝트 소스만 수정한 상태와 게임에 반영된 상태는 다릅니다. C++ 변경은 빌드해야 하고, 화면 제작 코드의 변경은 저장된 WBP에도 적용해야 합니다. 이미 만들어 둔 배포용 EXE는 새 소스로 다시 패키징하기 전까지 이전 동작을 사용합니다.

### 준비

1. Unreal Editor에서 작업 중인 내용을 저장하고 에디터를 닫습니다.
2. 프로젝트 폴더에서 PowerShell을 엽니다. `FestaFiesta.uproject`가 보이는 폴더가 맞습니다.
3. 아래 경로에서 `C:\Unreal\UE_5.7`은 실제 Unreal 5.7 설치 폴더로 바꿀 수 있습니다. 이 작업을 수행한 PC에서는 해당 경로를 사용했습니다.

### C++ 빌드

```powershell
$projectFile = Join-Path (Get-Location) 'FestaFiesta.uproject'
& 'C:\Unreal\UE_5.7\Engine\Build\BatchFiles\Build.bat' FestaFiestaEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE
```

마지막에 성공 결과가 나와야 합니다. `Error`로 끝났다면 에디터를 실행하기 전에 로그에서 첫 번째 오류를 확인합니다. 수정된 소스를 실행 중인 에디터에 부분 반영하는 Live Coding만으로 화면 제작 도구까지 모두 갱신되었다고 판단하지 마세요.

### 저장된 화면 에셋 적용

아래 스크립트는 기존 에셋을 먼저 백업하고, 지정한 화면만 변경하며, 같은 작업을 두 번 실행해도 결과가 계속 달라지지 않는지 확인합니다. 이 명령을 실행하는 동안 프로젝트의 다른 에디터는 닫아 두세요.

```powershell
$projectFile = Join-Path (Get-Location) 'FestaFiesta.uproject'
$uiScript = Join-Path (Get-Location) 'Tools/BuildModeClarity/apply_readability.py'
& 'C:\Unreal\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $projectFile /Engine/Maps/Entry -unattended -nop4 -nullrhi "-ExecutePythonScript=$uiScript"
```

`Saved/BuildModeClarity/readability/latest-result.json`에서 `status`가 `PASS`, `idempotent`와 `unrelated_ui_unchanged`가 `true`인지 확인합니다. `backup`에 이전 에셋 보관 위치가 적혀 있습니다. 종료 코드만 보지 말고 이 결과 파일도 확인해야 합니다.

스크립트가 실행하는 순서는 `FF.ApplyBuildReferenceTools` → `FF.ApplyBuildModeTypography` → `FF.ApplyBuildReadability`입니다. 엔진 콘솔에서 수동으로 실행할 수도 있지만, 백업·적용 범위·재실행 검사가 포함된 스크립트를 권장합니다.

## 6. 직접 플레이하며 확인하기

### 매출

1. 축제를 시작하고 준비 단계에서 유료 음식 부스를 설치합니다.
2. 부스 앞에 손님이 걸어올 수 있는 공간과 대기열 공간을 확보합니다.
3. 부스가 없는 입구 쪽 구역과 부스가 있는 구역이 연결되어 있는지 확인합니다.
4. 축제를 개장하고 방문객이 부스를 선택하고 줄을 서서 이용을 마칠 때까지 기다립니다.
5. 해당 시설 정보의 매출과 이용 인원이 증가하는지 확인합니다. 단순히 시설을 지은 순간에는 매출이 발생하지 않습니다.
6. 비교용 무료 휴게·도로 시설의 0원은 위 유료 부스의 매출 누락과 구분합니다.

### 대기열

1. 인기 있는 유료 부스 앞에 두 일행 이상이 모이는 상황을 만듭니다.
2. 첫 일행이 줄에 도착하기 전에 이용 완료 처리되지 않는지 봅니다.
3. 뒤 일행이 지정된 줄을 따라 자기 자리에서 멈추는지 봅니다.
4. 앞 일행이 이용을 마치면 뒤 일행이 한 칸 앞으로 이동하고 다시 멈추는지 봅니다.
5. 일행의 대표 한 명뿐 아니라 동행 NPC도 같은 줄 주변에 머무는지 봅니다.
6. 준비 단계에서 시설을 회전하거나 대기열 모양을 바꾼 뒤 다시 개장해서 방향이 일치하는지 확인합니다. 기존 단축키는 `7` 기본, `8` 좌측, `9` 우측, `0` U자입니다.

### UI와 골드

1. 화면을 1280×720, 1920×1080으로 각각 실행합니다.
2. 건축 카탈로그에서 긴 한글 시설 이름과 가격을 확인합니다.
3. 조합 도감에서 검색·필터·보상·긴 조합 설명을 확인합니다.
4. 시설 정보의 기본·운영·강화·조합 화면을 열고, 긴 내용은 스크롤로 끝까지 접근 가능한지 확인합니다.
5. 배치를 시작하고 상단 골드 배지가 사라졌는지 확인합니다.
6. 카탈로그 복귀, 배치 재진입, 시설 이동을 반복해도 배지가 다시 나타나지 않는지 확인합니다.
7. 실제 설치 시 비용이 정상 차감되는지 확인합니다.

## 7. 문제를 직접 추적하는 방법

게임 로그는 `Saved/Logs` 폴더에 있습니다. 로그는 실행할 때 새로 기록되므로 중요한 재현 로그는 복사해 보관하세요. 이번 작업 시작 전 로그는 `Saved/FacilityQueueUI_20260929/before-game.log`에 보관했습니다.

PowerShell에서 아래처럼 방문 후보 누락 메시지를 찾을 수 있습니다.

```powershell
Select-String -Path 'Saved/Logs/FestaFiesta.log' -Pattern '후보가 0개|Zone밖|Revenue|Error:'
```

`후보가 0개`는 ‘손님이 돈을 안 썼다’보다 앞 단계의 문제입니다. 시설이 실제로 열렸는지와 구역 연결을 먼저 보세요. `Error`라는 글자가 있다고 모두 이번 문제는 아닙니다. 발생 시각과 바로 앞의 행동을 함께 읽어야 합니다.

누군가에게 재현 방법을 전달할 때는 **맵 이름 / 축제 단계 / 시설 이름 / 시설 위치 / 화면 해상도 / 문제 발생 직전 행동 / 기대 결과 / 실제 결과**를 적으면 됩니다.

개발 중 게임 콘솔에서 `FF.Crowd.Queue`를 입력하면 가입, 도착 통지, 슬롯 목표 적용, 인원 불일치 등을 확인할 수 있습니다. `FF.Crowd.Stats`는 군중 상태 요약이고, `Log LogFestaFiestaQueue Verbose`는 대기열 로그를 자세히 출력합니다. UI의 Output Log와 게임 콘솔은 코드 편집기가 아니라 진단 명령을 실행하는 창입니다.

## 8. 직접 설정·코드를 바꾸는 순서

1. 바꾸려는 값을 하나만 정합니다. 예: 글자 크기, 부스 이용료, 대기열 간격.
2. 원래 값을 적고 해당 파일이나 에셋을 백업합니다.
3. 아래 파일 안내에서 해당 기능의 원본 위치를 찾습니다.
4. 한 번에 한 가지씩 바꿉니다. 요금과 이동 속도와 서비스 시간을 동시에 바꾸면 결과의 원인을 알기 어렵습니다.
5. C++이면 빌드하고, 화면 제작 코드이면 WBP 적용도 실행합니다.
6. 위 수동 확인 절차를 반복합니다. 글자를 키운 경우에는 작은 해상도와 긴 문장을 반드시 같이 확인합니다.
7. 검증에 통과한 값과 확인 결과를 기록합니다.

### 글꼴과 레이아웃을 수정할 때

공통 글꼴 규칙은 `Source/FestaFiesta/Public/Building/Presentation/FF_BuildTypography.h`에 있습니다. 제목과 가격은 Bold, 라벨은 Medium, 본문은 Regular 계열을 사용합니다. 글꼴 에셋 경로만 바꾸고 크기·여백을 그대로 두면 줄 높이가 달라져 다시 겹칠 수 있습니다.

`Source/FestaFiestaEditor/Private/Building/Presentation/FF_BuildTypographyBuilder.cpp`는 저장된 화면에 공통 글꼴을 적용합니다. `FF_BuildReadabilityBuilder.cpp`는 카드 높이·여백·줄바꿈 같은 읽기 편한 배치를 적용합니다. 이번 구조 정리 후에는 **WBP의 디자인을 기준**으로 수정합니다. 이전 디자인 생성 명령을 다시 실행하면 직접 수정한 모양이 바뀔 수 있으므로, 일반적인 디자인 수정에는 11절의 Designer 편집 방법을 사용하세요.

제작 도구는 중복 적용을 막는 버전 표식을 저장합니다. 제작 코드의 값을 바꾸고 같은 버전으로 실행하면 ‘이미 적용됨’으로 건너뛸 수 있습니다. 후속 수정은 해당 변경의 버전 표식을 새 버전으로 올리고, 적용 전 백업과 적용 후 재실행 검사를 함께 수행합니다.

버전을 올릴 때는 `apply_readability.py`의 `TYPOGRAPHY_VERSION` 또는 `READABILITY_VERSION`도 해당 C++ 값과 함께 바꾸세요. 버전이 다르면 스크립트는 적용 여부를 확인할 수 없어 실패합니다.

### 이용료를 직접 수정할 때

1. Unreal의 콘텐츠 브라우저에서 `/Game/FestaFiesta/Data/Economy/DT_FacilityRevenues`를 찾아 엽니다.
2. 시설의 Definition ID와 같은 이름의 행을 선택합니다. 표시용 한글 이름과 내부 ID는 다릅니다.
3. `1인 1회 매출 (Gold)`를 수정합니다. 코드의 필드명은 `RevenuePerUseGold`입니다.
4. 시설별 비율이 필요하면 `시설별 수수료율 사용`을 켜고 `수수료율 (0~1)`을 설정합니다. 12%는 `0.12`입니다. `12`가 아닙니다.
5. 강화별 차이는 `레벨별 수익 배율`에서 조정합니다.
6. 저장하고 게임을 다시 시작해 이용 완료 후 수익을 확인합니다.

적용 우선순위는 **수익표의 해당 행 → 시설 DataAsset의 재정의 값 → 소분류 기본값**입니다. 명시적인 0은 무료 설정으로 유지됩니다. 따라서 다른 곳의 기본값을 바꿨는데 이 시설만 0이라면 표의 해당 행부터 확인하세요. 유료 프로그램 참가비는 일반 시설 단가와 다른 설정인 `프로그램별 1인 참가비 (Gold)`를 확인합니다.

### 대기열을 직접 수정할 때

시설 정의 에셋의 `QueueRules`에서 다음 값을 확인합니다. 실제 시설 정의 경로는 시설 종류마다 다르므로 콘텐츠 브라우저에서 해당 시설 ID를 검색하세요.

| 필드 | 의미 | 조정 시 확인할 점 |
| --- | --- | --- |
| `SlotSpacingCm` | 팀별 자리 간격(cm) | 간격을 늘리면 더 긴 대기열 동선이 필요함 |
| `MaxWaitingAgents` | 저장 이름과 달리 현재 사용 규칙은 팀별 자리 수 | 손님 머릿수로 오해하지 않기 |
| `ServiceDurationSeconds` | 한 팀이 이용하는 시간(초) | 길수록 줄이 늘고 단위시간당 완료 인원이 줄어듦 |
| `ServiceCapacity` | 동시에 이용하는 팀 수 | 시설의 실제 공간과 함께 조정 |

대기열 동선 `QueueSplinePointsCm`의 길이는 최소 `(MaxWaitingAgents − 1) × SlotSpacingCm`이어야 합니다. 간격을 바꿔도 동선이 자동으로 길어지지는 않습니다. 대기열 예약 공간 `QueueReservedRects`와 시설 입구 공간도 함께 확인하세요. 기존 저장 시설은 설치 당시 사양을 보관하므로, 시설 정의를 수정한 뒤에는 새로 설치한 테스트 시설로 확인합니다.

도착 허용 거리는 `FF_CrowdSimulationSubsystem.h`의 `QueueSlotArrivalRadiusUU`입니다. Unreal의 이 값은 cm로 사용되며 기본값은 45입니다. 게임 콘솔에서 `FF.Crowd.Tune queueslot 45`로 임시 확인할 수 있지만 재실행하면 기본 설정으로 돌아옵니다. `FF_ZoneQueueProjection.cpp`의 `bRequireArrivalConfirmation = true`를 false로 바꾸면 도착 전에 이용되는 문제가 다시 생깁니다.

## 9. 검증 기록과 변경 파일

### 확인한 결과

- **에디터·게임 빌드:** Unreal 5.7.4 / Win64 Development 성공.
- **자동 검사:** 통합 280개 실행 후 발견한 문제를 수정했습니다. 영향 범위 189개 재검사와 도감 추가 검사를 반영한 테스트별 최신 결과는 **280개 모두 성공**입니다. 경고가 포함된 성공도 있으며, 초기에 실패한 로그도 삭제하지 않고 보관했습니다.
- **매출:** 유료 시설 31종의 대기열 구성·이용 완료·양수 정산, 빈 입구에서 먼 시설 발견, 도착 전 미정산, 완료 후 한 번만 정산 확인.
- **대기열:** 일행 전원 도착 대기, 앞팀 완료, 뒷팀 전진과 재도착, 정지 후 재이동, 멤버별 경로 갱신 확인.
- **화면:** 실제 WBP를 렌더링해 720p·1080p를 검사했습니다. 도감 목록은 1024×768과 720×960도 검사했습니다. 긴 시설명, 배치 헤더 버튼, 강화 수치·비용·잔액을 이미지로 직접 확인했습니다.
- **에셋 적용:** 요청 화면과 그 하위 위젯 24개에 글꼴을 적용했습니다. 489개 텍스트 중 473개 글꼴과 16개 기호를 감사했고, 재실행해도 저장 파일이 변하지 않으며 대상 외 UI가 유지되는 것을 확인했습니다.
- **실제 게임 화면:** Test_Main에서 카탈로그·조작 안내·배치·직접 조정 화면을 캡처했습니다. 캡처 과정에서 지도·UI 에셋 변경이 없었고, 캡처 직전 백업한 저장 파일이 복원됐습니다.

### 결과 파일 위치

`Saved` 아래 결과물은 로컬 검증 자료이며 Git에 포함되지 않습니다. 다른 PC에서는 검사를 다시 실행해 생성합니다.

| 위치 | 담긴 내용 |
| --- | --- |
| `Saved/FacilityQueueUI_20260929/before-game.log` | 수정 전 방문 후보 0개 증거 |
| `Saved/FacilityQueueUI_20260929/build-editor-final-readability.log` | 최종 에디터 빌드 |
| `Saved/FacilityQueueUI_20260929/build-game-final-readability.log` | 최종 게임 빌드 |
| `Saved/FacilityQueueUI_20260929/Integrated/index.json` | 280개 통합 검사와 초기 실패 원인 |
| `Saved/FacilityQueueUI_20260929/AffectedV2/index.json` | 보완 후 영향 범위 189개 모두 통과 |
| `Saved/FacilityQueueUI_20260929/CodexReadabilityFinal2/index.json` | 도감 4개 해상도 배치·이미지·세로 화면 검색 안내 재검사 |
| `Saved/FacilityQueueUI_20260929/validation-summary.json` | 테스트별 최신 결과 집계 |
| `Saved/BuildModeClarity/readability/latest-result.json` | 에셋 백업 위치·적용·글꼴·중복 적용 검사 |
| `Saved/FacilityQueueUI_20260929/discovery-audit.json` | 실제 지도 구역 연결·요금·대표 지점 경로 감사 |
| `Saved/BuildModeClarity/runtime-facility-queue-ui-20260929` | 실제 게임 화면 7장과 캡처 보고서 |
| `Saved/Automation/BuildableGrid` | 배치창·건축 카탈로그 렌더 이미지 |
| `Saved/Automation/FacilityInfoV2` | 시설 정보의 기본·운영·강화 등 렌더 이미지 |
| `Saved/Automation/FacilityCodex/ReadableCollection_*.png` | 도감 목록·상세의 해상도별 렌더 이미지 |

### 검증 범위와 저장 파일 주의사항

실제 지도의 구역 연결 정보와 요금표를 확인했고, 모든 유료 시설의 이용 완료 정산을 자동 검사했습니다. 지도 보행 검사는 대표 지점 기준이며, 시설을 임의 배치한 모든 위치의 NPC 이동까지 보장하는 검사는 아닙니다. 같은 구역도 높이가 다른 지면에 보행 영역이 나뉘어 있어 구역 원점과 실제 생성 위치의 경로 결과가 달랐습니다. 이 때문에 지도 감사 상태는 `AUDIT_COMPLETE_WITH_PATH_GAPS`로 기록했으며, 전 지도 이동 성공을 의미하는 `PASS`로 기록하지 않았습니다.

실제 화면 캡처에서는 새 시설을 설치하지 않았으므로 시설 정보의 조합 화면은 저장 WBP를 이용한 별도 렌더 검사로 확인했습니다. 키보드·마우스를 사람이 직접 조작한 실험은 아니며, 게임의 공개 UI 동작을 호출한 검사입니다. 새 배포 패키지 제작과 장시간 축제 운영 부하 검사는 이번 결과에 포함되지 않습니다.

**자동 검사를 다시 돌리기 전에 에디터를 닫고 `Saved/SaveGames` 폴더를 별도 위치에 복사하세요.** 기존 `Crowd.Production` 검사는 실제 게임을 시작하므로 자동 저장이 발생합니다. 이번 첫 통합 검사에서 `FF_AutoSave_A.sav`, `FF_AutoSave_B.sav`, `FF_SlotIndex.sav`가 테스트 상태로 갱신됐습니다. 그 검사 이전 자동 저장의 백업은 없어 원본 복원은 하지 못했습니다. 수동 저장 `day1.sav`와 `FF_MetaStore.sav`는 변경되지 않았습니다. 이후 실제 화면 캡처에는 저장 파일 백업·복원을 적용했으며, 이는 첫 검사 이전 상태로 복원했다는 뜻은 아닙니다.

### 자동 검사를 직접 실행하기

1. 앞의 빌드·화면 에셋 적용을 끝냅니다.
2. 에디터를 닫고 `Saved/SaveGames`를 별도 폴더에 복사합니다.
3. 다음 명령으로 이번 작업의 검사 범위를 실행합니다. 실제 렌더 검사가 필요하므로 `-nullrhi`를 추가하지 마세요.

```powershell
$projectFile = Join-Path (Get-Location) 'FestaFiesta.uproject'
$reportFolder = Join-Path (Get-Location) ('Saved/FacilityQueueUI_Check_' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$filter = 'FestaFiesta.Queue+FestaFiesta.Crowd+FestaFiesta.NPC.ZoneSystemBridge+FestaFiesta.Economy.QueueBridge+FestaFiesta.Economy.FacilityRevenueTable+FestaFiesta.ZoneIntegration+FestaFiesta.Building.BuildTools+FestaFiesta.UI.PrepBuild+FestaFiesta.UI.FacilityInfo+FestaFiesta.FacilityCodex.UI'
& 'C:\Unreal\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $projectFile /Engine/Maps/Entry -unattended -nop4 -RenderOffscreen -nosound "-ReportExportPath=$reportFolder" '-TestExit=Automation Test Queue Empty' "-ExecCmds=Automation RunTests $filter"
```

4. 결과 폴더의 `index.json`에서 `failed`와 `notRun`이 모두 0인지 봅니다. 경고가 포함된 성공은 `succeededWithWarnings`로 별도 표시됩니다.
5. 모든 에디터가 종료된 뒤, 2번에서 복사한 저장 파일을 원래 `Saved/SaveGames` 위치로 복원합니다. 원래 없던 테스트 저장 파일은 백업의 파일 목록과 비교해 구분하세요.
6. 실패가 있으면 `fullTestPath`로 테스트 이름을 찾고 해당 `entries`의 `Error` 메시지를 읽습니다. 숫자나 문구가 바뀌었다고 바로 검사를 삭제하지 말고, 실제 화면이나 동작이 올바른지 먼저 확인합니다.

### 기능별 수정 파일 안내

아래 경로는 프로젝트 폴더 기준입니다. `Source/FestaFiesta`는 게임 실행 코드, `Source/FestaFiestaEditor`는 화면 제작 도구입니다.

| 파일 | 직접 수정할 때의 역할 |
| --- | --- |
| `Source/FestaFiesta/Private/NPC/Integration/FF_ZoneSystemBridge.cpp` | 손님에게 보여 줄 시설 후보와 탐색 범위 |
| `Source/FestaFiesta/Private/Zone/FF_ZoneQueueProjection.cpp` | 시설 규칙을 실제 팀 대기열 설정으로 변환, 도착 확인 활성화 |
| `Source/FestaFiesta/Private/CrowdSystem/Systems/FF_CrowdSimulationSubsystem.cpp` | 일행의 경로, 자리 배정, 도착 확인, 앞자리 이동 |
| `Source/FestaFiesta/Private/CrowdSystem/Systems/FF_CrowdSteering.cpp` | 자리 도착 후 정지와 다시 움직이기 |
| `Source/FestaFiesta/Private/Queue/FF_QueueCrowdLink.cpp` | 대기열 자리 정보를 군중에 전달, 인원 불일치 진단 |
| `Source/FestaFiesta/Private/UI/FacilityCodex/FF_FacilityCodexCollection.cpp` | 도감 제목·필터·목록·상세의 화면 크기별 배치 |
| `Source/FestaFiesta/Private/UI/FacilityCodex/FF_FacilityCodexScreen.cpp` | 좁은 도감 검색창의 안내 문구·너비·표시 범위 |
| `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoPanel.cpp` | 시설 정보 창의 크기 조절 |
| `Source/FestaFiesta/Public/Building/Presentation/FF_BuildTypography.h` | 제목·본문·숫자 공통 글꼴 경로 |
| `Source/FestaFiestaEditor/Private/Building/Presentation/FF_BuildTypographyBuilder.cpp` | 저장된 WBP에 글꼴 적용 및 감사 |
| `Source/FestaFiestaEditor/Private/Building/Presentation/FF_BuildReadabilityBuilder.cpp` | 카드 크기·이름 영역·탭·스크롤·배치 헤더 정돈 |
| `Source/FestaFiesta/Private/Building/Presentation/FF_BuildToolsWidget.cpp` | 실행 중 상단 골드 표시 제거 |
| `Source/FestaFiestaEditor/Private/Building/Presentation/FF_BuildReferenceToolsWbpBuilder.cpp` | 저장된 화면에서도 상단 골드 표시 제거 |
| `Tools/BuildModeClarity/apply_readability.py` | UI 백업·반영·재실행·변경 범위 검사 |
| `Tools/FacilityRevenue/audit_discovery.py` | 실제 지도와 수익표를 저장 없이 읽는 진단 |

수정 내용을 되돌릴 때는 먼저 에디터를 닫고 작업 중인 다른 변경을 보관합니다. WBP는 적용 보고서의 `backup` 아래에서 해당 파일만 원래 위치로 복사할 수 있습니다. 코드 변경도 함께 되돌려야 하는 경우에는 Git의 변경 비교를 보고 해당 수정 부분만 복원하고 다시 빌드하세요. 다른 사람이 작업한 파일까지 일괄 초기화하지 마세요.

## 10. 다른 사람에게 설명할 때

“시설 매출은 손님이 시설을 발견하고 줄에 도착해서 이용을 끝내야 올라갑니다. 기존에는 다른 구역의 시설을 못 찾는 경우와 줄에 실제 도착하기 전에 이용이 진행되는 경우가 있었습니다. 시설 검색 범위와 도착·자리 이동 처리를 고쳤고, 건축 화면은 글자와 패널 공간을 함께 조정했습니다. 상단 골드는 표시만 제거했으며 실제 돈 계산은 유지했습니다. 코드 빌드와 화면 에셋 적용을 모두 해야 수정이 게임에 반영됩니다.”

## 11. 디자이너에서 현재 UI를 직접 수정하기

### 왜 디자이너와 게임 화면이 달랐는가

이전에는 예전 화면의 노드를 숨긴 채 남겨 두고, 카테고리 버튼·위치·크기를 게임 실행 중 코드로 다시 구성했습니다. 따라서 디자이너에서는 옛 화면이 겹쳐 보이거나, 실행 중에만 생기는 버튼이 보이지 않았습니다. 현재 WBP에는 필요한 화면 구조와 버튼을 저장하고, 사용하지 않는 옛 시각 요소를 제거했습니다. 실제 이용 중 표시되는 오류·보상 상태·도움말 같은 조건부 위젯은 기능에 필요하므로 유지합니다.

### 수정해야 할 WBP 찾기

도감의 조합 목록은 **한 줄에 카드 6개**를 유지합니다. 수집 진행도와 단계별 보상은 목록 아래의 넓은 하단 영역을 사용하며, 보상은 오른쪽 끝까지 고르게 배치합니다. 이 배치는 Designer와 게임 화면에 동일하게 적용합니다.

콘텐츠 브라우저의 `/Game/FestaFiesta/Building/FF_WBP_Building`에서 다음 파일을 엽니다.

| 화면 | 현재 편집 대상 |
| --- | --- |
| 건축 카탈로그 전체 | `Catalog/WBP_FF_PrepPanelBuild` |
| 카탈로그 시설 카드 | `Catalog/WBP_FF_PrepBuildTile` |
| 카탈로그 분류 버튼 | `Controls/WBP_FF_BuildCategoryChoice` |
| 건축·배치 도구 | `Controls/WBP_FF_BuildTools` |
| 시설 정보 창 | `Facility/WBP_FF_FacilityInfoPanel` |
| 시설 조합 도감 | `FacilityCodex/WBP_FF_FacilityCodexScreen` |
| 도감 조합 카드 | `FacilityCodex/WBP_FF_FacilityCodexCard` |
| 도감 수집 보상 카드 | `FacilityCodex/Controls/WBP_FF_CodexCollectionReward` |
| 카탈로그의 도감 열기 버튼 | `FacilityCodex/WBP_FF_FacilityCodexCatalogLauncher` |

`Catalog/Legacy`는 별도 대체 카탈로그가 참조하는 호환 에셋입니다. 현재 준비 화면의 카탈로그를 수정할 때는 위 표의 `WBP_FF_PrepPanelBuild`를 사용하세요. 사용 중인 대체 경로의 파일까지 삭제하면 다른 진입 경로가 깨질 수 있습니다.

### 글자·간격·위치를 바꾸는 기본 순서

1. 대상 WBP를 열고 **Designer** 탭으로 이동합니다.
2. **Hierarchy**에서 수정할 글자·버튼·패널을 선택합니다. `Text_`는 글자, `Button_`은 버튼, `Size_`는 크기 영역인 경우가 많습니다.
3. **Details**에서 글자는 Font·Color, 버튼은 Style, 간격은 Padding, 캔버스 위치는 Slot의 Anchors·Offsets를 변경합니다.
4. 오른쪽에 붙는 버튼은 우측 앵커를, 양쪽으로 늘어나는 영역은 좌우 앵커를 사용합니다. 화면 픽셀 위치 하나만 고정하면 작은 해상도에서 잘릴 수 있습니다.
5. **Compile → Save**를 누릅니다. 1280×720과 1920×1080으로 확인하고 게임에서도 동작을 확인합니다.
6. 실행 중 갱신되는 시설명·금액·진행도는 예시 문구가 아닌 실제 게임 데이터로 바뀝니다. 글꼴·색·여백과 데이터 갱신을 구분하세요.

코드가 찾는 위젯 이름과 이벤트 연결은 기능의 주소입니다. 모양을 바꿀 때 이름을 임의 변경하거나 필수 버튼을 삭제하지 마세요. 실제로 기능을 제거하는 작업은 바인딩 코드와 검증도 함께 수정해야 합니다.

카탈로그 분류는 `Choice_CategoryAll`, `Choice_Category숫자`를 선택한 뒤 **Catalog Tab**에서 탭 이름·아이콘을 바꿉니다. 순서는 Hierarchy에서 같은 Horizontal Box 아래 자식 순서를, 간격은 각 자식의 Slot Padding을 조절합니다. 분류 ID는 실제 시설 분류와 연결되므로 모양만 바꿀 때는 유지하세요. 카탈로그 높이는 `Size_ClarityCatalogDock`, 화면 폭에 맞추는 옵션과 좌우 여백은 Class Defaults의 **Appearance / Catalog**에서 조절합니다. 같은 곳의 **디자이너 카탈로그 레이아웃 사용**을 켜 두어야 저장한 탭과 치수를 게임에서도 사용합니다.

카탈로그 시설 카드 크기를 바꿀 때는 `WBP_FF_PrepBuildTile`의 `SizeBox_Card`와 부모 `WBP_FF_PrepPanelBuild`의 `TileView_Facilities`에 있는 Entry Width/Height를 함께 확인하세요. 카드만 크게 만들고 목록의 한 칸 크기를 그대로 두면 카드가 다시 겹칠 수 있습니다.

카탈로그 카드의 대표 그림은 `Image_Thumbnail`의 Brush에서 바꿉니다. 그림이 지정되어 있는데도 미리보기에 보이지 않으면 Brush의 Image Size가 0인지 확인하세요. 기존 파일은 이 값이 0이어서 보이지 않았고, 이번 정리에서 실제 그림 크기로 복구했습니다. 게임에서는 시설별 실제 이미지가 표시됩니다.

도감 위치는 `Canvas_CodexLayout` 아래 각 위젯의 Anchors·Offsets로 조절합니다. Class Defaults의 **Collection / Layout → Use Authored Collection Layout**을 켜 두면 게임이 저장된 위치를 사용합니다. 시설 정보 창 크기는 `Panel_Size`, 건축 도움말 위치는 `Size_BuildWorldHelp`, 건축 설정 높이는 `Size_BuildSettings`에서 조절합니다.

도감의 `TileView_Entries`에서 읽기 전용인 `Column Count`가 **6**인지 확인하세요. 카드 모양은 `WBP_FF_FacilityCodexCard`에서 수정합니다. 하단 띠의 높이·위치는 `Row_CollectionProgress`, 보상 사이 간격은 `Row_CollectionMilestones` 아래 각 항목의 슬롯 Padding에서 바꿉니다. 각 보상 슬롯의 크기 규칙은 Fill을 유지해야 오른쪽 끝까지 고르게 채워집니다.

### 숨겨진 화면도 미리 보기

- **카탈로그:** Class Defaults의 `Designer Preview`에서 `빈 목록 미리보기`를 켜면 결과가 없는 화면을 확인합니다. 기본값은 시설 카드 목록입니다.
- **카탈로그 카드:** `Designer Preview`의 `대표 카드 상태`에서 일반·필수 미설치·필수 완료·프로그램·조합·도감·설치 불가를 선택합니다. 여러 상태를 동시에 겹쳐 표시하지 않습니다.
- **건축 도구:** Class Defaults의 `Designer Preview`에서 `Designer Page`를 바꾸면 배치·카탈로그·설정 화면을 살펴볼 수 있습니다. `Designer Show Help`로 도움말 표시를 선택합니다.
- **시설 정보:** `Designer Preview`의 `Designer Page`에서 기본·운영·조합·강화·매출·프로그램·조명·철거 확인 중 원하는 페이지를 선택합니다.
- **도감:** `Collection / Designer Preview`의 `Preview Collection Detail`을 켜면 상세 화면을, 끄면 목록 화면을 미리 봅니다.
- 미리보기 설정은 게임 저장이나 실제 보상을 변경하지 않습니다.

Unreal은 게임에서 숨기는 `Visibility`와 Designer에서 숨기는 상태를 별도로 관리합니다. 이번 미리보기 옵션은 두 상태를 맞춰, 안내 문구와 다른 페이지가 한꺼번에 겹치지 않게 합니다. 건축 도구·시설 정보의 자동 페이지 미리보기를 끄려면 `Preview Designer Page`를 끄고, Hierarchy의 눈 표시로 편집할 부분을 선택하세요.

### 개발자가 최신 WBP 구조를 다시 적용할 때

일반적인 디자인 변경은 WBP에서 직접 저장하면 됩니다. 아래 명령은 이번 구조 정리가 아직 적용되지 않은 체크아웃에서만 필요합니다. 각 마이그레이션은 버전 표식을 확인하므로, 이미 적용된 버전은 건너뛰어 디자이너의 후속 수정을 보존합니다.

```powershell
$projectFile = Join-Path (Get-Location) 'FestaFiesta.uproject'
$designerScript = Join-Path (Get-Location) 'Tools/BuildModeClarity/apply_designer_ui.py'
& 'C:\Unreal\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $projectFile /Engine/Maps/Entry -unattended -nop4 -nullrhi "-ExecutePythonScript=$designerScript"
```

먼저 최신 C++ 에디터 빌드를 마치고, 다른 에디터는 저장·종료한 상태로 실행합니다. 대상 WBP 9개는 실행 전에 백업됩니다. `Saved/BuildModeClarity/designer-ui/latest-result.json`에서 적용 결과·백업 위치·삭제된 노드 목록·대상 외 UI 보존 여부를 확인할 수 있습니다. 이전 디자인 생성 명령을 임의로 섞어 실행하지 마세요.

디자이너 렌더 검사는 `FestaFiesta.Building.UI.DesignerPreviews`입니다. 게임을 시작하지 않고 엔진의 실제 Designer 생성 경로와 화면 해상도별 DPI로 네 화면 및 설정·강화·도감 상세 페이지를 렌더링하며, 이미지는 `Saved/Automation/BuildDesigner`에 저장합니다.

### 이번 WBP 정리와 도감 배치의 최종 확인 결과

2026-09-29 추가 작업에서는 위 표의 WBP 9개를 저장했고, 정리 전과 비교해 사용하지 않는 옛 위젯 노드 286개를 제거했습니다. 다른 UI 에셋은 이 적용 도구가 변경하지 않았습니다. 같은 도구를 다시 실행했을 때 파일 내용이 동일한지도 확인했으므로, 이미 적용한 뒤 디자이너가 수정한 내용을 재실행으로 덮어쓰지 않습니다.

- **시설 조합 도감:** 한 줄 6개, 7번째 카드는 다음 줄의 맨 왼쪽에 배치됩니다. 1280×720과 1920×1080에서 실제 데이터 카드 12개와 Designer 예시 카드로 확인했습니다.
- **수집 진행도:** 목록 아래 하단 영역을 사용하고, 4개 보상과 진행선이 오른쪽까지 고르게 이어집니다. 목록과 진행도 영역이 겹치지 않도록 공간을 분리했습니다.
- **화면 이동:** 오른쪽 위 X는 도감을 닫고 이전 화면으로 돌아갑니다. 왼쪽 시설 분류는 `전체 조합` 한 개만 남겼습니다.
- **빌드·검사:** Editor와 Game Development 빌드 성공, 관련 UI 검사 69개 성공·실패 0개입니다. 그중 화면 수명 검사 1개에서 자동 검사 전용 플레이어의 입력 객체가 없다는 경고 2건이 발생했습니다. 이번 후속 검사 전후 저장 파일은 백업과 해시를 비교해 복원 여부를 확인했습니다. 앞 절에 기록한 최초 통합 검사의 자동 저장 문제와는 별도입니다.

검사 기록은 `Saved/CodexNavigation_20260929/validation-summary.json`에 있습니다. 화면별 원본 결과는 같은 폴더의 `DesignerValidationV4/index.json`, 보완 후 재검사한 `DesignerFinalSixColumns/index.json`, 카탈로그 이미지 크기 복구 후의 `DesignerCatalogFinal/index.json`에 있습니다. 여러 번 실행한 같은 검사는 마지막 결과로 집계합니다. 6열 카드의 실제 렌더는 `Saved/Automation/FacilityCodex/ReadableCollection_SixColumns_1280x720.png`와 `ReadableCollection_SixColumns_1920x1080.png`, Designer 화면은 `Saved/Automation/BuildDesigner/Codex_1280x720.png`와 `Codex_1920x1080.png`에서 볼 수 있습니다. 이 이미지는 자동 렌더 검사 결과이며 사람의 수동 플레이 검사나 새 배포 패키지를 의미하지 않습니다.
