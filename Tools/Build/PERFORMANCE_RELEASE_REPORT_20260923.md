# FestaFiesta 성능·패키징·릴리스 검증 기록 — 2026-09-23

> **출시 판정: FAIL — 출시 승인 불가.** Development R3/R4와 Shipping R1은 내부 검증용 `quality-override` 패키지다. R4에서 SNS DataTable의 GC 참조 수명 문제와 Incident 카탈로그 설정·soft load 문제의 재현은 사라졌고, 수정 후 전체 자동화 테스트 933/933건은 통과했다. Shipping 빌드와 초반 화면 흐름도 확인했지만, 원본이 없는 UI 아이콘 5개, 60 FPS 미달, 시설·NPC·저장 복원·장시간 플레이의 실제 Shipping 검증이 남아 있다. 아래 PASS는 명시한 시험 범위에만 적용한다.

## 1. 범위와 환경

- 프로젝트: [`FestaFiesta.uproject`](../../FestaFiesta.uproject)의 `EngineAssociation=5.7`. [`Config/DefaultEngine.ini`](../../Config/DefaultEngine.ini)의 Game Default Map은 `/Game/FestaFiesta/Maps/Frontend`, 플레이 맵은 `/Game/FestaFiesta/Maps/Test_Main`이다.
- 프로젝트 문서에 확정 최소 사양·FPS 목표가 없어 **Windows x64, DX12/SM6, 1920×1080, 60 FPS**를 임시 목표로 사용했다. DX11/SM5도 Cook·초기화만 확인했다. 이 PC 결과는 최소 사양 결과가 아니다.
- 측정 PC: Windows 11 Home 10.0.26200, Ryzen 7 7800X3D 8코어/16스레드, RTX 3060 Ti 8 GB(드라이버 32.0.15.9186), 물리 RAM 33,457,156,096 B. GPU 선택과 D3D12/SM6는 각 `run.json`·`analysis.json`·`Game.log`에서 확인했다.
- 설치 엔진: `C:\Unreal\UE_5.7\Engine\Build\Build.version`의 **5.7.4**, CL **51494982**. Windows SDK **10.0.22621.0**, MSVC **14.44.35228**(Visual Studio 2022 Community), 번들 .NET **8.0.412**, Git LFS **3.7.1**. 고정 버전은 [`toolchain.lock.json`](toolchain.lock.json)과 각 `build-manifest.json`에 기록된다.
- 런타임 의존성: 로컬 `FF_ZoneSystem`, GameplayAbilities, CommonUI, Water/GeometryProcessing 등 `.uproject`와 Plugins를 확인했다. Water는 Experimental, CommonUI는 Beta다. Cesium은 비활성이다. 엔진 업그레이드나 엔진 소스 변경은 없었다.

## 2. 단계별 상태

| 단계 | 상태 | 증거와 한계 |
|---|---|---|
| 프로젝트·엔진·SDK·툴체인 조사 | **PASS** | `.uproject`, Source/Config/Plugins, 고정 버전 파일 및 R3/R4 UBT 로그 확인. |
| Development R3 기준 Build → Cook → Stage → Package → Archive | **PASS** | `C:\FFBuilds\FF-Dev-Baseline-20260923-R3\Logs\BuildCookRun.log:1049-1052`, UAT 종료 코드 0. 테스트 생략을 명시한 내부용 예외 빌드. |
| Development R4 수정본 클린 Build → Cook → Stage → Package → Archive | **PASS** | `C:\FFBuilds\FF-Dev-Fixed-20260923-R4\Logs\BuildCookRun.log:1055-1061`, UAT 종료 코드 0, `cleanBuild=true`. 내부용 예외 빌드. |
| 에셋 DataValidation | **PASS** | R4 `Validation\DataValidation\DataValidation.log:933,16572`: 7,381개 검사, 0 error, 0 warning. 이는 플레이 성공 증명이 아니다. |
| Cook 파일·셰이더 라이브러리·초기 실행 검사 | **PASS** | R4 `Metadata\verification.json`: 빌드 시 필수 35개, UnrealPak/IoStore, SM5/SM6의 Global·게임 셰이더 라이브러리, NullRHI·DX11·DX12·기본 런처 초기화 통과. 현재 37개 정확한 `.uasset/.umap` 경로로 재검증도 PASS. `renderedGameplayTested=false`는 초기 실행 검사만 뜻한다. |
| R3 AuthoredMap 첫 실행 캡처 | **FAIL** | `Measurements\Baseline-R3-Authored-First\run.json`에 측정기 종료 코드 기록 결함으로 FAIL. 원본 트레이스 일부는 남았지만 성공한 기준선으로 사용하지 않는다. |
| R3 AuthoredMap warm·FestivalFlow 첫/warm·SyntheticCrowd 재시도 | **PASS** | 각 `run.json`과 `analysis.json` PASS. 진단 명령을 사용한 시나리오별 측정이며 실제 UI 버튼 조작·자연 최대 부하를 대신하지 않는다. Synthetic 최초 시도는 FAIL, 재시도만 PASS. |
| R4 FestivalFlow 첫/warm·warm 재시도 패키지 실측 | **PASS** | 세 실행의 `run.json`·`analysis.json` PASS. 최초 warm의 큰 편차는 재시도에서 재현되지 않았고, 전후 최적화 성과는 입증되지 않았다. |
| 병목 원인 분석 | **PASS** | R3 CSV/Insights에서 Render Thread·GPU와 그림자/Virtual Shadow Map 비용이 우선 조사 대상으로 나타남. 타이머는 중첩되므로 합산하지 않는다. |
| 성능 최적화 효과·동일 부하 재측정 | **FAIL** | R4에 렌더링 최적화는 적용하지 않았다. Festival 첫 실행도 개선 근거가 없고 warm 재시도는 NPC 중앙값이 같아졌으나 카메라·GPU 외부 점유가 통제되지 않았다. 60 FPS 목표 미달. |
| 패키지 NPC 시간표·GC 회귀 검사 | **PASS** | R4 `C:\FFBuilds\FixedR4-NpcGC-20260923\summary.json`: NullRHI 자동화 1건 Success, 0 error, 5 warning. 실제 맵의 공연 집결·장시간 플레이는 포함하지 않는다. |
| 수정 후 전체 자동화 테스트 | **PASS** | `C:\FFBuilds\FullValidation-20260923-R2`: FestaFiesta 809/809(성공 743·경고 성공 66), FF_ZoneSystem 124/124(성공 116·경고 성공 8), 총 933/933 성공 또는 경고 성공, 실패 0. 기존 R1의 `UI.FacilityCodex.PresenterLifecycle` 실패는 테스트가 현재 가상화 목록 동작을 검사하도록 수정한 뒤 재실행에서 해소됐다. |
| Shipping 클린 Cook·전체 빌드·패키지 구조 검사 | **PASS** | `C:\FFBuilds\FF-Shipping-RC-20260923-R1`: UAT BuildCookRun 종료 0, `BUILD SUCCESSFUL`, 필수 에셋 37개, Pak/IoStore·SM5/SM6, NullRHI·DX11·DX12·기본 실행파일 초기화 PASS. ZIP·해시·심볼 분리 완료. 품질 예외 표기 때문에 출시 승인으로 승격되지 않았다. |
| Shipping 실제 화면 시작·맵 진입 | **PASS** | 분리된 `UserDir`에서 실제 Shipping 창의 Title → 플레이 → 새 축제 → 대한민국 → 축제 지도 → 다대포 바다축제 → Test_Main 진입과 건축 카탈로그 표시를 눈으로 확인했다. 시설 선택 후 설치 완료 여부는 확인하지 못했다. |
| Shipping 배치·철거·NPC·저장 복원·장시간·최소 사양·별도 PC | **NOT TESTED** | 사용자가 실제 플레이 검증을 이어서 수행하기로 했다. 저자는 이 단계에서 UI 조작을 중단했다. 사운드, 공연 집결, 저장/불러오기, 메모리 누적 및 개발 도구 없는 PC도 미검증이다. |
| 최종 출시 준비 | **FAIL** | 위 실패와 미검증 항목이 남아 있다. |

## 3. 재현 가능한 빌드와 Cook 범위

[`Package-Win64.ps1`](Package-Win64.ps1)은 엔진·프로젝트·출력 경로/라벨을 받고 UAT BuildCookRun의 Build → Cook → Stage → Package → Archive를 실행한다. 종료 코드, 로그, SHA256, Git branch/commit/dirty, 툴체인·옵션·심볼 경로를 기록한다. 기본 최종 후보는 클린 빌드/클린 Cook이고, `-Incremental`은 반복 개발용이다. [`FestaFiesta.Build.psm1`](FestaFiesta.Build.psm1)은 `.uproject`·Game/Editor 타깃·ProjectID를 검사하고 prospective Cook 경로 길이 260자 이상을 빌드 전에 거부한다. 원본 긴 경로에서는 717개가 한계를 넘었고, `C:\FFProj` junction 경로는 7,210개 검사, 최장 208자였다. 사용자 저장 데이터와 공유 DDC는 삭제하지 않았다.

R3/R4 로그의 실제 UAT 명령은 다음과 같다. R4는 라벨만 `FF-Dev-Fixed-20260923-R4`로 바뀐다.

```powershell
& 'C:\Unreal\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  '-project=C:\FFProj\FestaFiesta.uproject' -target=FestaFiesta -noP4 -utf8output -unattended -WaitForUATMutex `
  -platform=Win64 -clientconfig=Development -nodebuginfo -makebinaryconfig -build -cook -stage -package `
  -pak -iostore -compressed -prereqs -archive `
  '-archivedirectory=C:\FFBuilds\.staging\FF-Dev-Fixed-20260923-R4\Package' -clean
```

같은 파이프라인을 새 출력에 재실행하는 예시다. 기존 결과를 덮어쓰지 않도록 라벨을 바꾼다.

```powershell
.\Tools\Build\Package-Win64.ps1 -EngineRoot 'C:\Unreal\UE_5.7' `
  -ProjectFile 'C:\FFProj\FestaFiesta.uproject' -OutputRoot 'C:\FFBuilds' `
  -BuildLabel 'FF-Dev-Recheck-UNIQUE' -Configuration Development `
  -SkipTests -QualityOverrideReason 'Internal measurement; tests run separately.' `
  -GraphicsSmokeTest -SkipZip
```

[`Config/DefaultGame.ini`](../../Config/DefaultGame.ini)는 `bCookAll=False`이며 Frontend/Test_Main을 지정한다. 런타임 문자열/soft load가 있는 UI, Data, Building UI, Minigame 등은 현재 `DirectoriesToAlwaysCook`에 포함된다. 시설·미니게임 정의는 Asset Manager `AlwaysCook`이다. 에디터 전용·기존 검토용 경로만 `DirectoriesToNeverCook`한다. Water/Landmass 참조가 남아 `bSkipEditorContent=False`다. 전체 프로젝트 강제 Cook이나 원본 에셋 삭제는 하지 않았다. NPC 입장/외형 표, 스폰 actor, 군중 BP·애니메이션 및 12개 soft 캐릭터 에셋, 핵심 UI·Input·Audio·Finale의 IoStore 경로를 [`Verify-Package.ps1`](Verify-Package.ps1)에서 정확한 확장자까지 확인한다. Incident 카탈로그/정의, SNS 표, UI 폰트도 포함해 현재 총 37개다.

| Development 결과 | R3 | R4 |
|---|---:|---:|
| payload 전체 | 1,107,080,041 B | 1,107,084,216 B |
| Pak/IoStore 컨테이너 합계 | 713,243,721 B | 713,239,625 B |
| 필수 에셋 검사 | 당시 manifest 33개 PASS; 새 정확 경로 검사 35개도 PASS | 빌드 manifest 35개 PASS; 현재 검사 스크립트의 37개 독립 재검증 PASS |
| ZIP 다운로드 크기 | NOT TESTED (`-SkipZip`) | NOT TESTED (`-SkipZip`) |

R4의 전체 payload는 R3보다 **4,175 B 증가**했다. 두 버전의 `size-report.json` 값이며 RAM·VRAM이나 다운로드 압축률은 아니다. Pak+IoStore, Oodle Kraken, prerequisite `vc_redist.x64.exe`, en/ko 및 분리된 `Symbols\`을 확인했다. 다른 압축·컨테이너 방식의 로딩/호환성 비교는 **NOT TESTED**다. 두 manifest 모두 `feature/Package` / `94cd325ec657`, `dirty=true`, `quality-override`이며 R4 `Metadata\source-dirty.patch`와 `source-dirty-untracked.zip`에 변경 증거가 남는다.

정확한 실행파일·증거 경로:

```text
C:\FFBuilds\FF-Dev-Baseline-20260923-R3\Package\Windows\FestaFiesta.exe
C:\FFBuilds\FF-Dev-Baseline-20260923-R3\Package\Windows\FestaFiesta\Binaries\Win64\FestaFiesta.exe
C:\FFBuilds\FF-Dev-Fixed-20260923-R4\Package\Windows\FestaFiesta.exe
C:\FFBuilds\FF-Dev-Fixed-20260923-R4\Package\Windows\FestaFiesta\Binaries\Win64\FestaFiesta.exe
C:\FFBuilds\FF-Dev-Fixed-20260923-R4\Logs\BuildCookRun.log
C:\FFBuilds\FF-Dev-Fixed-20260923-R4\Metadata\build-manifest.json
C:\FFBuilds\FF-Dev-Fixed-20260923-R4\Metadata\verification.json
C:\FFBuilds\FF-Dev-Fixed-20260923-R4\Metadata\IoStore-Contents.csv
C:\FFBuilds\FF-Dev-Fixed-20260923-R4\Symbols\
```

Shipping 후보 R1은 별도 라벨과 클린 전체 빌드·Cook으로 만들었다. 호출 명령과 실제 UAT 로그는 다음과 같다. 패키지 스크립트 안에서는 `-SkipTests`를 기록했지만, 같은 소스의 전체 자동화 **933/933 PASS**를 앞서 별도 실행했다. 수동 QA가 끝나지 않아 `latest.json`으로 승격하지 않았다.

```powershell
& 'C:\FFProj\Tools\Build\Package-Win64.ps1' -EngineRoot 'C:\Unreal\UE_5.7' `
  -ProjectFile 'C:\FFProj\FestaFiesta.uproject' -OutputRoot 'C:\FFBuilds' `
  -Configuration Shipping -BuildLabel 'FF-Shipping-RC-20260923-R1' `
  -SkipTests -QualityOverrideReason 'Full 933-test automation suite and 7381-asset DataValidation passed separately; Shipping manual gameplay and release approval remain pending.' `
  -GraphicsSmokeTest
```

`C:\FFBuilds\FF-Shipping-RC-20260923-R1\Logs\BuildCookRun.log`의 UAT `BuildCookRun -clientconfig=Shipping -build -cook -stage -package -pak -iostore -compressed -prereqs -archive -clean`은 **176.67초**, 종료 코드 **0**이었다. `Metadata\build-manifest.json`은 5.7.4, `cleanBuild=true`, `quality-override`, Git `feature/Package` / `94cd325ec657`, `dirty=true`를 기록한다. `Metadata\verification.json`은 **37/37** 필수 에셋과 NullRHI·DX11/SM5·DX12/SM6·기본 EXE 초기화 PASS를 기록한다. Shipping payload **848,281,512 B**, 전체 Pak/IoStore 컨테이너 **647,529,508 B**(`verification.json`), ZIP **723,922,526 B**(`size-report.json`)다. `size-report.json`의 main-container 압축 크기 619,005,720 B는 전체 컨테이너 합계와 범위가 다르다. ZIP SHA256은 `6d42cb12798f04acd030912aaf7e989c93d9d628d0bea95f154e7fb47689c50a`다. 실행파일 Authenticode는 **NotSigned**이며, 배포 심볼 PDB **257,282,048 B**는 payload 밖에 있다.

```text
C:\FFBuilds\FF-Shipping-RC-20260923-R1\Package\Windows\FestaFiesta.exe
C:\FFBuilds\FF-Shipping-RC-20260923-R1\Package\Windows\FestaFiesta\Binaries\Win64\FestaFiesta-Win64-Shipping.exe
C:\FFBuilds\FF-Shipping-RC-20260923-R1\FF-Shipping-RC-20260923-R1.zip
C:\FFBuilds\FF-Shipping-RC-20260923-R1\Symbols\FestaFiesta-Win64-Shipping.pdb
C:\FFBuilds\FF-Shipping-RC-20260923-R1\Metadata\build-manifest.json
C:\FFBuilds\FF-Shipping-RC-20260923-R1\Metadata\verification.json
C:\FFBuilds\FF-Shipping-RC-20260923-R1\Metadata\ARCHIVE-SHA256.txt
C:\FFBuilds\shipping-rc-r1-console.log
```

## 4. 성능 기준선·R4 재측정

[`Measure-PackagedPerformance.ps1`](Measure-PackagedPerformance.ps1)은 패키지를 읽기 전용으로 실행하고 분리된 `UserDir`, 로그, CSV, `capture.utrace`, 프로세스 RAM 샘플을 남긴다. [`Analyze-UnrealCsv.py`](Analyze-UnrealCsv.py)는 평균 FPS=`1000 × 선택 프레임 수 / 프레임시간 합`, P95/P99는 nearest-rank로 계산한다. 모든 아래 메인 Festival 행은 1920×1080·D3D12/SM6·프로젝트 기본 품질·프레임 1200–3599의 2,400프레임이며 `run.json`의 실제 커맨드라인을 근거로 한다. 새 `UserDir`는 시스템/드라이버 공유 캐시까지 완전 냉간 상태라는 뜻이 아니다. `nvidia-smi` 값은 전체 GPU 사용량이므로 게임 전용 VRAM으로 사용하지 않는다.

| Development 실행 | 상태 | 평균 FPS | P95 / P99 프레임 | >33.33 ms | GT / RT / GPU 중앙값 | 활성 NPC 최소/중앙/최대 |
|---|---|---:|---:|---:|---:|---:|
| R3 Festival 첫 실행 | PASS | 44.56 | 24.13 / 24.86 ms | 0 | 7.13 / 22.38 / 21.59 ms | 22 / 59 / 92 |
| R3 Festival warm | PASS | 44.81 | 23.99 / 24.72 ms | 0 | 6.97 / 22.26 / 21.46 ms | 22 / 59 / 88 |
| R4 Festival 첫 실행 | PASS | 44.39 | 24.55 / 25.43 ms | 0 | 7.35 / 22.39 / 21.55 ms | 22 / 59 / 97 |
| R4 Festival warm | PASS, 성능 목표 FAIL | 31.80 | 37.10 / 38.60 ms | 619 | 8.25 / 30.42 / 29.61 ms | 35 / 85 / 136 |
| R4 Festival warm 재시도 | PASS, 성능 목표 FAIL | 46.48 | 23.58 / 24.46 ms | 0 | 6.98 / 21.47 / 20.62 ms | 22 / 59 / 84 |

R3/R4 첫 실행은 같은 스크립트·해상도·RHI·품질과 NPC 중앙값 59로 가장 가까운 비교다. R4가 빨라졌다는 근거는 없다. 최초 R4 warm은 NPC 중앙값 85로 R3 warm의 59와 다르고 카메라/시점 동일성도 확정되지 않아 **31.80 대 44.81 FPS를 코드 변경의 회귀율로 계산하지 않는다**. 이 실행의 긴 프레임 619개는 실제 관측이다. 활성 인원 40–59명의 초기 구간도 R4 최초 warm 약 27.98 FPS, R3 warm 약 45.36 FPS여서 NPC 수 차이만으로 설명되지 않았다. R4 warm 프레임 1200–1799의 `GPU/ShadowDepths`는 약 18.5 ms, `GPU LocalUsed` 약 4007.8 MiB였다가 뒤 구간에 약 15 ms/3688 MiB로 낮아지며 27→33 FPS로 회복했다(`Fixed-R4-Festival-Warm\frames.csv`). 같은 실행의 `global-gpu-memory.csv`에서 전체 GPU 사용량도 5795→5494 MiB로 감소했다. 이 값에는 다른 프로세스가 포함되어 외부 GPU 간섭 가능성은 있으나 **원인은 미확정**이다. 유휴 상태에서 같은 R4 패키지와 첫 실행 `UserDir`로 warm을 재시도한 `Fixed-R4-Festival-Warm-Retry\run.json`과 `analysis.json`은 46.48 FPS/P95 23.58 ms/P99 24.46 ms, 긴 프레임 0건, 활성 NPC 22/59/84명, 계획 425명/입장 112명, PhysicalUsed 2490.125 MiB/GPU LocalUsed 3557.6992 MiB로 최초 warm의 큰 하락이 재현되지 않았다. 따라서 최초 warm은 재현되지 않은 이상값으로 취급하되, 원인을 특정하거나 재시도 결과를 코드 성능 개선으로 주장하지 않는다. 동일 카메라·부하·외부 GPU 점유를 통제한 추가 반복이 필요하다. R3/R4의 카메라 probe 스크린샷은 육안상 같은 장면이지만 둘 다 `TimeScale=4.00`이고 메인 3,600프레임 실행은 `TimeScale=1.00`이다. probe의 R3 46.23 FPS/R4 45.43 FPS 역시 메인 비교나 개선 증거로 쓰지 않는다. R4의 생산 최대 부하와 동일 NPC·카메라 고정 비교는 **NOT TESTED**다.

| 추가 기준 측정 | 상태 | 결과·한계 |
|---|---|---|
| R3 AuthoredMap 첫 실행 | FAIL | 측정기 ExitCode 기록 결함. `Baseline-R3-Authored-First` 원본은 보존하되 성공한 기준선에서 제외. |
| R3 AuthoredMap warm | PASS | `Baseline-R3-Authored-Warm\analysis.json`: 1,800프레임(600–2399), 56.25 FPS, P95 18.9065 ms, P99 19.4562 ms, >33.33 ms 0. GT/RT/GPU 중앙값 5.744/17.700/16.877 ms. Test_Main 직접 로드. |
| R3 SyntheticCrowd 최초 | FAIL | 실행/분석 조건 미충족. |
| R3 SyntheticCrowd 재시도 | PASS | `Baseline-R3-Synthetic-Retry`: `FF.Crowd.Demo 500 1500`로 **498**명 active/bound(로그 :1272), 2,400프레임(600–2999), 74.86 FPS, P95 14.63 ms, P99 15.08 ms. Setup의 합성 부하이며 시간표·Live·공연 검증이 아니다. |

Festival 측정은 Frontend → `FF.Flow.Start` → day1 시설 배치 저장본 복원(시설 2개) → 진단 명령 `FF.Crowd.GoLive` → 시간표 입장 순서다. UI의 실제 진행 버튼 조작은 검증하지 않았다. R3/R4 첫 실행 모두 표 로드, 계획 425명, 프레임 3540까지 116명 입장 확인(`run.json`); 최초 R4 warm은 168명, warm 재시도는 112명 입장까지 진행됐다. 패키지 안에서 로드됐다는 증거와 GC 후에도 유효하다는 증거는 구분한다. R4 NullRHI NPC 수명 테스트는 두 날의 57명 계획을 끝까지 활성·표시하고 GC 후 arrival/appearance table을 유지했다(`FixedR4-NpcGC-20260923\Report\index.json`). 다만 이 fixture에는 NavMesh·시설 벽·메인 공연 시작 신호가 없어 경고 5개가 남으며 실제 맵의 이동/공연 성공으로 확장하지 않는다.

R3 Festival 첫 실행 CSV의 `GPU/ShadowDepths` 평균 **11.4551 ms**(2,400개 유효 샘플), Insights GPU Live `ShadowDepths` 약 **11.409 ms/call**, `RenderVirtualShadowMaps` 9.779, TSR 5.427, MainPass 5.570, Nanite DrawGeometry 4.822이다(`Baseline-R3-Festival-First\InsightsExport\timer_statistics_gpu_live.csv`). 이 타이머들은 중첩되므로 합산할 수 없다. Render Thread가 GPU/occlusion query를 기다리는 약 17 ms 관측도 있어 렌더·GPU 경로가 우선 조사 대상이다. 원인별 그림자 생성 주체와 화질 영향이 확인되지 않아 그림자 품질 CVar, Nanite/HLOD/TSR 등은 바꾸지 않았다. R3 Festival 첫 실행 UE 카운터 중앙값은 PhysicalUsed **2452.98 MiB**, GPU LocalUsed **3558.57 MiB**이고, R4 첫 실행은 **2503.82/3559.45 MiB**다. R4 warm은 **2459.4/3688.1 MiB**로 시나리오 부하가 달라 메모리 회귀로 단정하지 않는다. Memory Insights 할당 태그 분석은 시도했지만 `Baseline-R3-Festival-First\InsightsExport\UnrealInsights.log:392`의 `[MemAlloc] TagTracker errors: 28177`로 할당 원인 귀속은 **BLOCKED**다. 장시간 메모리 누적 측정은 **NOT TESTED**다.

실측 `LoadMap` 로그: R3 첫 실행 Frontend **0.729276 s**, Test_Main **0.964811 s**; R4 첫 실행 **0.700192/0.945346 s**. warm은 R3 **0.631525/0.899140 s**, R4 **0.679211/0.925054 s**다. 엔진의 맵 로드 구간일 뿐 전체 실행·PSO 준비 시간은 포함하지 않으며 반복 통계가 적어 로딩 개선이라고 판단하지 않는다. `r.PSOPrecaching=1`, Components=1, Resources=0, GlobalShaders=1, Validation=0이며 DX12 디스크 캐시 옵션 2개는 0이다. `FestaFiesta_PCD3D_SM6.stable.upipelinecache`를 열지 못한 로그가 있다. R3 Festival 시작 구간(프레임 0–1199)의 >33.33 ms 프레임은 첫 실행 4건(>50 ms 3건), warm 3건(>50 ms 3건)이다. 전체 PSO miss는 첫 실행 147건, warm 143건이다. 프레임 600–1200에는 각각 PSO miss 4건이 있었고, 첫 실행 프레임 630에서 `GraphicsPSOHitch` 약 **38.9008 ms** 1건이 관측됐지만 warm에는 GraphicsPSOHitch가 없었다(`Baseline-R3-Festival-First\frames.csv`, `Baseline-R3-Festival-Warm\frames.csv`). 위 표의 안정 Live 구간(프레임 1200–3599)에서는 >33.33 ms 프레임이 0건이다. 시작 구간 끊김 및 PSO miss는 **측정됨(PASS)**; Validation=0이어서 모든 누락의 원인 분석과 수집형 PSO 캐시의 생성·적용·재검증은 **NOT TESTED**다.

재현 예시(각 실행은 고유 출력 디렉터리를 사용):

```powershell
$payload = 'C:\FFBuilds\FF-Dev-Fixed-20260923-R4\Package\Windows'
.\Tools\Build\Measure-PackagedPerformance.ps1 -PayloadRoot $payload -Scenario FestivalFlow `
  -OutputRoot 'C:\FFBuilds\Measurements\R4-Festival-First-UNIQUE' -EngineRoot 'C:\Unreal\UE_5.7' `
  -CaptureFrames 3600 -WarmupFrames 1200 -SampleGlobalGpuMemory
.\Tools\Build\Measure-PackagedPerformance.ps1 -PayloadRoot $payload -Scenario FestivalFlow `
  -OutputRoot 'C:\FFBuilds\Measurements\R4-Festival-Warm-UNIQUE' -EngineRoot 'C:\Unreal\UE_5.7' `
  -CaptureFrames 3600 -WarmupFrames 1200 `
  -ReuseUserDir 'C:\FFBuilds\Measurements\R4-Festival-First-UNIQUE\UserData' -SampleGlobalGpuMemory
.\Tools\Build\Test-PackagedNpcArrival.ps1 -PayloadRoot $payload `
  -OutputRoot 'C:\FFBuilds\R4-NpcGC-UNIQUE'
```

## 5. 결함·경고·수정·원복

| 상태/우선순위 | 원인과 증거 | 조치·영향·원복 |
|---|---|---|
| **PASS — SNS DataTable 수명 수정** | R3 Festival `Game.log:1260`에서 `UFF_CrowdInsightsSubsystem`의 `GetAllRows`가 잘못된 RowStruct/깨진 테이블 이름을 출력. 설정은 soft path이나 로드된 `UDataTable` 멤버가 GC 추적되지 않았다. Cook 누락 근거는 없다. | [`FF_CrowdInsightsSubsystem.h`](../../Source/FestaFiesta/Public/NPC/Insights/FF_CrowdInsightsSubsystem.h)에 `UPROPERTY(Transient)`를 붙여 참조 수명을 명시하고 실제 GC 회귀 테스트를 추가했다. R4 Festival `Game.log`에 이 오류 0건, 집중 테스트 PASS. 게임플레이·화질 설정을 변경하지 않았다. 원복은 해당 UPROPERTY와 추가 테스트를 되돌리는 것(원래 수명 오류 재발 위험). |
| **PASS — Incident 카탈로그 설정·soft load 수정** | R3 Frontend/패키지에서 `Catalog is not exist`. R3 IoStore에는 catalog와 definition이 존재하므로 Cook 누락이 아니다. `Settings->Catalog.Get()`을 로드 전에 `IsValid`로 검사했고 기본 설정 경로도 없었다. | [`DefaultGame.ini`](../../Config/DefaultGame.ini)에 catalog soft 경로를 지정하고 [`FF_IncidentSubsystem.cpp`](../../Source/FestaFiesta/Private/Incident/FF_IncidentSubsystem.cpp)의 null/로드 실패를 구별했다. [`FF_IncidentCatalogLoadTests.cpp`](../../Source/FestaFiesta/Private/Incident/Tests/FF_IncidentCatalogLoadTests.cpp) 집중 자동화 1/1 PASS. R4 Festival에서 기존 경고 0건. 원복 지점은 이 설정 섹션·로더 변경·테스트. |
| **PASS — 패키지 경로·정확한 Cook 검증** | 원본 checkout의 긴 경로가 Cook 시 MAX_PATH에 걸리고, IoStore CSV 전체 문자열 검색은 에셋 접두어로 오탐할 수 있었다. | 빌드 모듈의 `-ProjectFile` override/프로젝트 동일성·Cook 경로 사전 검사와 Verify의 CSV 정확 경로 비교를 추가했다. R3 35개 재검증 및 R4 현재 37/37 PASS (`C:\FFBuilds\AuditVerify-FixedR4-37Assets`). 런타임 FPS에 직접 영향 없음. 원복은 `Package-Win64.ps1`, `Run-AutomationTests.ps1`, `Verify-Package.ps1`, `FestaFiesta.Build.psm1` 변경을 되돌리는 것. |
| **FAIL — UI 아이콘 5개** | R3/R4 Festival `Game.log:1085-1098`에서 Build/Program/Promotion/Finale/Check 아이콘 경로가 로더에 없다고 기록. 원본 `Content/.../Icons/Navigation/`에도 해당 `.uasset` 5개가 없다. | 단순 Cook 누락이 아니라 누락된 원본/경로 계약이다. UI 외형·대체 표시를 실제 플레이에서 확인하고 필요한 에셋 제작 또는 올바른 기존 자산 지정이 필요하다. 임의 대체·일괄 화질 저하는 적용하지 않았다. |
| **PASS — 전체 자동화 R1 실패 해소** | R1 `FullValidation-20260923\FestaFiesta\Automation.log:10768-10777`: `UI.FacilityCodex.PresenterLifecycle`의 구형 페이지네이션 기대가 현재 연속 가상화 목록 동작과 달라 1건 실패했다. | [`FF_FacilityCodexPresenterTests.cpp`](../../Source/FestaFiesta/Private/UI/FacilityCodex/FF_FacilityCodexPresenterTests.cpp)의 기대값을 실제 `UTileView` 검색·스크롤 동작으로 갱신했다. R2 `C:\FFBuilds\FullValidation-20260923-R2`에서 FestaFiesta 809/809, FF_ZoneSystem 124/124, 합계 933/933 성공 또는 경고 성공, 실패 0. |
| **NOT TESTED — Cook/런타임 경고 영향** | R4 Cook의 AssetRegistry 69개 경고는 `C:\FFProj` junction과 OneDrive 원본 경로의 capitalization 비교에서 발생했다. `DemoMaps.umap` unknown custom version은 NeverCook 레거시 경로다. EOS Shared의 `SteamShared` cook metadata 생략도 표시된다. | Cook은 성공했으나 경고를 숨기지 않는다. Steam/EOS 실제 런타임 의존 여부, 해당 레거시 경로의 제외 유지, junction 환경 외 재빌드 시 차이를 확인해야 한다. |
| **NOT TESTED — 렌더링 최적화 및 PSO 추가 캐시** | 측정은 그림자/RT-GPU 대기를 지목하지만 원인 자산별 분해와 동일 카메라 반복 비교가 미완료. | ISM/HISM, LOD, Nanite/HLOD, Lumen/TSR, UI Invalidation, 풀링, PSO 수집 캐시 등은 이득을 입증하지 못해 미적용. NPC 수·기능·화질을 줄이지 않았다. |

R3 패키지 GC 테스트 실패는 주로 64칸 fixture에 실제 425명 계획이 흘러와 풀을 소진한 시험 조건 결함이었다(`BaselineNpcGC-20260923\Report\index.json`). R4 테스트는 생산 경제 브리지의 기대 방문객 수를 fixture의 57명으로 고정해 1/1 Success로 확인했다. **생산 NPC 수를 낮춘 변경은 아니다.** R4 결과의 5개 경고 중 NavMesh 경계 없음·시설 벽 없음·메인 공연 시작 신호 없음은 작은 NullRHI fixture의 한계를 나타내며 실제 맵 검증을 대체하지 않는다. `NaniteUtilities` 불필요 Build.cs 의존도 제거했으나 실행 성능 효과는 주장하지 않는다.

## 6. 남은 릴리스 게이트

1. **FAIL 해결:** UI 아이콘 5개의 원본/경로와 화면 영향을 해결한다. 전체 자동화 933/933건은 R2에서 통과했지만 실제 UI 외형·입력 검증을 대신하지 않는다.
2. **NOT TESTED/BLOCKED:** R4 warm 재시도에서 최초 warm 하락은 재현되지 않았지만, 동일 카메라·시간배속·NPC 수·외부 GPU 점유를 고정한 충분한 반복 비교, R4 실제 최대 부하, 장시간 RAM/VRAM 누적, PSO miss의 포괄적 원인 분석과 수집형 캐시 검증이 남았다. Memory Insights TagTracker 오류로 할당 원인 귀속은 BLOCKED다. 성능 개선이 없는 변경을 최적화 성과로 계산하지 않는다.
3. **PASS:** Shipping R1 클린 Build/Cook/Stage/Package/Archive, 37개 필수 에셋, DX11·DX12 포함 초기 실행, ZIP·해시·분리 심볼까지 확인했다. 실제 Shipping GUI는 2026-09-23 UTC 11:25경 별도 `C:\FFBuilds\ShippingQA-20260923\UserData`로 시작했다(프로세스 ID 272632, 1280×720 DX12/SM6). Title → PlayHub → 대한민국 → 다대포 바다축제 → Test_Main까지 눈으로 확인했고 배치 카탈로그와 메인공연장 선택 화면을 열었다. 1280×720 수동 화면 확인은 위 1920×1080 성능 수치와 비교하지 않는다. Shipping에서 `-abslog`를 지정해도 `Game.log`가 생성되지 않아 이 단계의 근거는 화면 관찰과 살아 있는 프로세스뿐이다.
4. **NOT TESTED:** 사용자가 이어서 실제 플레이 검증을 수행한다. Setup에서 메인공연장 등 필수 시설 설치, 비필수·비운영 시설 배치와 철거, 준비 HUD 점검/확정 → Day 시작 → 개장하기, NPC 자연 입장·이동·공연 집결, 입력·UI 아이콘·폰트/현지화·사운드를 확인해야 한다. 하루 시작 자동 저장 후 종료/재실행하여 `최근 축제 계속하기`와 시설 복원을 확인한다(Shipping 수동 저장 콘솔 없음). 종료·장시간 반복 및 RAM/VRAM 누적, 개발 도구 없는 별도 PC와 최소 사양 PC도 아직 없다. 사용자가 실제 테스트를 맡겠다고 요청하여 더 이상의 UI 조작은 수행하지 않았다.

`C:\FFBuilds`의 R3/R4/Shipping 패키지·로그·트레이스·CSV·심볼은 서로 분리했다. Shipping 패키징 성공만으로 출시 승인을 내리지 않으며 위 FAIL/NOT TESTED가 남는 동안 판정은 **FAIL**이다.
