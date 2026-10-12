# Win64 패키징 운영 안내

팀의 공식 진입점은 [`Package-Win64.ps1`](./Package-Win64.ps1)이다.
빌드·검증·재빌드 절차는 이 문서에서 관리한다. 플레이어용 설치·실행·문제 보고는 [`배포본 실행 안내`](../../Distribution/README_KO.md)를 참고한다.
이 문서는 운영 절차이며, 현재 소스의 패키징이나 QA 통과를 미리 보장하지 않는다.

## 실행 전 준비

- 릴리스 후보는 검토한 커밋에서 만들고 작업 중 소스·에셋을 바꾸지 않는다.
- 에디터에서 Save All 후 Editor, Live Coding, 해당 게임을 종료한다.
- [`toolchain.lock.json`](./toolchain.lock.json)의 엔진 버전·CL, MSVC, Windows SDK를 설치한다.
- Git LFS 에셋을 내려받고 출력 SSD에 여유를 확보한다. 사전 검사의 최소치는 10 GiB다.
- 짧은 checkout을 권장한다. 깊은 기존 checkout은 같은 소스를 가리키는 junction을 사용할 수 있다.

아래 `C:\FFProj`, `C:\Unreal\UE_5.7`, `C:\Builds\FestaFiesta`는 팀원별로 조정하는 예시 경로다.
엔진 폴더 이름이 같아도 실제 빌드는 잠금 파일과 일치해야 한다.
`C:\FFProj`가 없다면 다음처럼 실제 checkout을 연결한다. 기존 경로를 덮어쓰지 않는다.

```powershell
$FFCheckout = 'C:\Git\8th-Team13-Final-Project'
New-Item -ItemType Junction -Path 'C:\FFProj' -Target $FFCheckout
```

소스의 Cook 경로와 산출물의 ZIP 경로를 각각 짧게 해야 한다.
`-OutputRoot`만 바꾸면 소스의 `Saved\Cooked` 경로는 짧아지지 않는다.
짧은 `-ProjectFile`은 junction 경로를 그대로 유지하며 같은 프로젝트인지 검사한다.

## 표준 릴리스 후보 만들기

두 RHI를 지원하는 GPU와 드라이버가 있는 Windows 제작·QA PC에서 실행한다.
날짜가 포함된 라벨은 예시다. 각 시도에 고유 라벨을 쓰며 실패한 라벨도 재사용하지 않는다.

```powershell
Set-Location 'C:\FFProj'
$FFEngine = 'C:\Unreal\UE_5.7'
$FFProject = 'C:\FFProj\FestaFiesta.uproject'
$FFOutput = 'C:\Builds\FestaFiesta'
$FFLabel = 'FF-Win64-Shipping-20260930-01'
git status --short
git rev-parse HEAD
git lfs pull
& '.\Tools\Build\Package-Win64.ps1' -EngineRoot $FFEngine -ProjectFile $FFProject -OutputRoot $FFOutput -BuildLabel $FFLabel -Configuration Shipping -GraphicsSmokeTest
```

기본은 clean Shipping 빌드다. 표준 후보에는 `-Incremental`, `-SkipTests`, `-SkipDataValidation`, `-SkipSmokeTest`, `-SkipZip`을 추가하지 않는다.
Editor 빌드, `FestaFiesta`·`FF_ZoneSystem` 전체 자동화, 데이터 검증 후 Build/Cook/Stage/Package/Archive를 실행한다.
전체 자동화에는 위젯 픽셀 검사가 포함되어 `-RenderOffscreen`으로 실행한다. 창을 띄우지 않아도 GPU·드라이버가 필요하며, 데이터 검증은 `-nullrhi`로 실행한다.
자동화의 각 필터 실행은 보고서 옆 `User-<고유 ID>`를 절대 `-UserDir`로 사용해 플레이어 저장과 이전 실행의 저장 데이터를 격리한다. 실패 분석을 위해 이 폴더도 검증 보고서와 함께 보관한다.
Pak·IoStore 압축, 필수 에셋·SM5/SM6 셰이더 포함, NullRHI 초기 실행, 기본 런처와 DX11 SM5 / DX12 SM6 초기화를 검사한다.
실제 실행 결과는 보고서로 판정하며 스크립트의 종료 성공만 보고 수동 플레이 QA까지 완료했다고 기록하지 않는다.
CI의 정식 패키징도 GPU가 있는 Windows runner에서 수행한다. `-GraphicsSmokeTest`를 생략한 후보의 패키지 그래픽 초기화는 QA PC에서 아래 명령으로 재검사한다.

```powershell
$FFRelease = Join-Path $FFOutput $FFLabel
& '.\Tools\Build\Verify-Package.ps1' -PackagePath (Join-Path $FFRelease 'Package') -EngineRoot $FFEngine -ProjectFile $FFProject -Configuration Shipping -SmokeTest -GraphicsSmokeTest -ReportDirectory (Join-Path $FFRelease 'Recheck')
```

## 요청자가 직접 테스트하는 빠른 패키징

자동 검증과 실행 테스트를 요청자가 직접 진행하기로 한 경우 아래 예외 명령을 사용한다.
위 준비 절차와 `$FFEngine`, `$FFProject`, `$FFOutput` 경로 설정은 동일하게 적용하고, 실행마다 고유 라벨을 지정한다.

```powershell
$FFLabel = 'FF-Win64-Shipping-20261012-ManualQA-01'
& '.\Tools\Build\Package-Win64.ps1' -EngineRoot $FFEngine -ProjectFile $FFProject -OutputRoot $FFOutput -BuildLabel $FFLabel -Configuration Shipping -Incremental -SkipTests -SkipDataValidation -SkipSmokeTest -QualityOverrideReason 'Manual QA will be performed by the requester; skip automated validation and runtime smoke tests.'
```

Editor 컴파일과 Build/Cook/Stage/Package/Archive, 정적 패키지 검사는 유지하며 자동화 테스트·데이터 검증·실행 smoke 테스트를 생략한다.
ZIP은 기본대로 생성한다. `-Incremental`은 UAT `-clean`만 생략하므로 캐시 적중이나 반복 Cook을 보장하지 않는다.
산출물은 수동 QA 대기 상태의 `quality-override`로 기록하며 `latest.json`으로 승격하지 않는다. 데이터 검증 생략은 manifest의 `validation.dataValidation=false`에 남는다.

## 산출물과 판정

| 위치 | 역할 |
| --- | --- |
| `<OutputRoot>/<라벨>/Package` | 완성된 게임 폴더. 콘솔의 `Payload:` 아래 루트 `FestaFiesta.exe`가 플레이어 진입점 |
| `<라벨>.zip` | payload 전체 배포 묶음. EXE나 `.ucas` 일부만 따로 배포하지 않음 |
| `Symbols` | 해당 바이너리의 PDB·target receipt. 장애 분석용 내부 보관; 공개 ZIP 밖에 유지 |
| `Metadata` | 소스·툴체인·품질 상태 manifest, 패키지·RHI 검증, IoStore 목록과 SHA-256 근거 |
| `Validation` | Editor 빌드, 전체 자동화와 데이터 검증 보고서 |
| `Logs/BuildCookRun.log` | 실제 컴파일러·SDK 선택, Cook 누락, 빌드 실패의 근거 |
| `Metadata/size-report.json` | payload·컨테이너·ZIP 크기와 직전 승격 빌드 대비 증감 |
| `<OutputRoot>/latest.json` | 예외 없는 스크립트 통과 빌드의 색인. 라벨·시간을 대조해 현재 시도인지 확인 |
| `.staging/<라벨>/Metadata/failure.json` | 보통 실패 단계와 로그 위치. 사전 검사 실패는 폴더 없이 콘솔에만 남을 수 있음 |

빌드는 `.staging`에서 진행하고 전체 성공 후 최종 라벨 폴더로 이동한다.
`latest.json`은 공개 업로드나 수동 QA 승인 기록이 아니다. 실패 시 이전 성공 빌드를 계속 가리킬 수 있다.
`Metadata/build-manifest.json`, `verification.json`, 테스트 보고서와 해시를 함께 보관한다.
dirty 후보는 패치와 수정된 추적 파일·미추적 파일의 실제 바이트 ZIP을 남긴다. LFS 에셋도 포인터와 함께 수정본을 보관하며 공개 후보는 검토한 커밋에서 다시 만든다.

## 예외와 출고 QA

`-SkipTests`, `-SkipDataValidation`, `-SkipSmokeTest` 중 하나라도 사용하면 10~500자의 `-QualityOverrideReason`이 필수다.
결과는 `quality-override`와 `QUALITY-OVERRIDE.txt`로 표시하며 `latest.json`으로 승격하지 않는다.
`-SkipTests`만 사용하면 Editor 빌드·데이터 검증은 유지한다. 데이터 검증 생략은 별도 `-SkipDataValidation`으로 지정하며 Editor 빌드는 항상 유지한다. 테스트 실패는 원인을 고쳐 표준 절차로 다시 검증한다.
그래픽 smoke는 RHI·셰이더 모델 초기화 근거다. 화면·게임 진행·성능의 수동 QA는 별도다.
두 RHI에서 Frontend → 실제 맵 → 조작·건설·저장·불러오기·종료를 확인하고 GPU·드라이버·증상을 기록한다.
렌더링 차이, 첫 실행·반복 실행의 프레임 시간, 기존 세이브 호환성, 별도 플레이 PC도 확인한다.
출고 담당자는 QA 기록, 알려진 문제, ZIP 해시, 실행 안내·라이선스를 확인하고 이전 승인본을 보관한다.

## 재빌드와 유지보수

일상 내부 반복은 `-Incremental -SkipZip`을 사용할 수 있다. `-Incremental`은 UAT `-clean`만 생략한다.
캐시 적중·반복 Cook·차등 패치를 보장하지 않으며 배포 후보는 새 라벨로 표준 clean 전체 검증을 수행한다.
엔진·MSVC·SDK 갱신은 잠금 파일과 설치 환경을 함께 갱신하고 실제 빌드 로그의 선택 버전을 대조한다.
도구 갱신 후 전체 테스트·clean 패키징·두 RHI·수동 QA를 다시 수행하고 이전 크기·시간과 비교한다.
맵·동적 로딩 에셋·플러그인 변경 시 Cook 규칙, 필수 에셋 검사, 라이선스 고지도 함께 수정한다.
DDC·Intermediate는 손상 근거와 영향 범위를 확인해 정리한다. `Saved`의 로그·자동저장·세이브는 먼저 보존한다.
