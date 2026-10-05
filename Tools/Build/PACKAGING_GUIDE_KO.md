# FestaFiesta Windows 패키징 실무 가이드 — SM5 / SM6 함께 지원하기

문서 기준: 2026-09-30 · 프로젝트 `FestaFiesta` · Unreal Engine **5.7.4 / CL 51494982** · Windows 64비트 Shipping 배포

Development 진단 빌드도 같은 스크립트의 `-Configuration Development`로 만들 수 있다. NPC 문제를 직접 확인하는 실행·콘솔·로그 절차는 [Development 테스트 안내](../../Distribution/DEVELOPMENT_TEST_GUIDE_KO.md)를 따른다. 현재 사용자 실행 방식은 **루트의 `FestaFiesta.exe` 하나를 더블클릭**하는 것이다. 새 Development 제작 명령의 라벨은 `FestaFiesta-Win64-Development-SingleExe-20260911`이다. 아래 0장의 수치는 최초 Shipping 배포 이력이며, 새 라벨의 검사 완료를 뜻하지 않는다. 각 빌드의 결과는 해당 라벨의 `Metadata`와 `Validation`에서 확인한다.

이 가이드는 프로젝트를 처음 받은 사람이 개발 환경을 설치하고, 배포 파일을 만들고, 다른 PC에서 검사하고, 다음 버전을 배포하는 과정까지 혼자 따라갈 수 있도록 작성했다. 명령은 **Windows PowerShell** 기준이며, 프로젝트의 공식 실행 입구는 `Tools/Build/Package-Win64.ps1`이다. 웹 문서의 최신 버전과 우리 엔진 버전이 다르면 이 문서에 연결한 **5.7 문서, 저장소 설정, 설치된 5.7.4 엔진 코드**를 우선 대조한다.

목표는 **한 Windows 패키지에 DX11 + SM5와 DX12 + SM6 셰이더를 함께 담고, 사용자는 `FestaFiesta.exe` 하나를 더블클릭해 시작하게 만드는 것**이다. 엔진이 시작할 때 지원 여부를 확인해 그래픽 경로를 선택한다. 데이터는 함께 제공하는 폴더에 유지한다. 이것만으로 모든 SM5 그래픽카드에서 같은 화면과 성능을 보장하지는 않는다. 게임 시작, 실제 장면, 입력, 저장, 성능을 두 경로에서 확인한 뒤 검증된 GPU와 드라이버 범위를 배포 조건에 적는다.

## 0-1. 2026-09-18 최적화 적용 빌드 기록

8장의 최적화(Shipping LTCG, 텍스처 RDO, 지구본 시안 제외, GameplayCue 경로 한정)와 파이프라인 보강(경로 길이·잠금 프로세스 사전검사, 툴체인 조기 검사, 쿠킹 누락 게이트, 용량 리포트, dirty 패치 보관)을 적용한 첫 빌드다.

| 항목 | 결과 |
| --- | --- |
| 빌드 라벨 / 소스 | `FestaFiesta-Win64-QA-20260918` / `feature/Package`의 `29b8765b7c1c` + 이번 수정 작업 트리(`dirty=true`, 차이는 `Metadata/source-dirty.patch`) |
| 판정 | **`quality-override` 내부 QA 빌드.** `latest.json`으로 승격하지 않음 |
| 자동화 테스트 | **Dev HEAD에서 6건 실패로 표준 절차 중단.** 수정 전 설정으로 되돌려도 같은 6건이 실패해 패키징 변경과 무관함을 확인. 도로 카탈로그 2건(9/14 `9ef52a79`에서 삭제된 `Road_Surface_Wood/Stone`을 테스트가 기대), 군중 도착·프로그램 2건, 경제 브리지 2건 |
| 데이터 검증 | 오류 0건·경고 1건 통과 |
| Shipping Build / Cook / Stage / Package / Archive | 전체 성공. 실제 선택 툴체인 MSVC `14.44.35228` + SDK `10.0.22621.0`, 쿠킹 누락 0건 |
| 초기 실행 검사 | NullRHI, DX11 SM5, DX12 SM6, 기본 실행(DX12 SM6 자동 선택) 모두 통과 |
| 크기 | 설치 775.6 MiB, ZIP 655.1 MiB(686,940,536바이트), SHA-256 `da150db4fc2751e744f2ecabdd42577cf7ee39b06959e253545f68f1164b0bda` |

공개 배포 후보는 위 6건을 담당자가 고친 커밋에서 **생략 옵션 없이** 5장의 표준 절차로 다시 만든다. 같은 날 표준 절차로 시도한 `FestaFiesta-Win64-Shipping-20260918`은 테스트 단계에서 멈췄고 실패 기록은 `Artifacts/Releases/.staging/FestaFiesta-Win64-Shipping-20260918/Metadata/failure.json`에 남아 있다.

## 0. 최초 Shipping 실행 이력과 문서의 적용 범위

아래는 `FestaFiesta-Win64-SM5SM6-20260911`의 **정규 패키징 완료 · 전체 자동 검사 통과 이력**이다. 이후 Development 및 실행 방식 변경 빌드의 완료 수치는 이 표에 포함하지 않는다. 방법 설명, 설정 완료, 패키지 생성 성공, 실기기 호환성 검증은 서로 다른 상태다. 최초 검사와 중간 QA 빌드에서 발견한 문제의 원인·수정·개별 검증은 부록에 보존했으며, 중간 결과를 새 빌드의 결과로 대신 사용하지 않는다.

| 항목 | 이번 실행 결과 |
| --- | --- |
| 빌드 라벨 / 소스 커밋 / 작업 트리 변경 여부 | `FestaFiesta-Win64-SM5SM6-20260911` / `feature/Package`의 `21b3de81a3bd` + 이번 수정 작업 트리 (`dirty=true`, 아직 커밋하지 않음) |
| 자동화 테스트 | **760 / 760건 성공, 실패 0건·미실행 0건·진행 중 0건.** FestaFiesta 647건(일반 성공 595 + 경고 성공 52), FF_ZoneSystem 113건(일반 성공 66 + 경고 성공 47). 경고가 있는 성공 합계 99건 |
| 데이터 검증 | **오류 0건·경고 1건 통과.** 에디터 commandlet 문맥에서 경제 시스템의 Queue 서브시스템 부재 경고 1건 기록 |
| Shipping Build / Cook / Stage / Package / Archive | **전체 성공.** UE 5.7.4, Shipping clean 빌드, UAT 종료 코드 0. 배포 폴더 29개 파일 / 739,523,960바이트(약 705.26 MiB) |
| SM5 / SM6 셰이더 파일 포함 검사 | **통과.** SM5·SM6 각각 Global / FestaFiesta 라이브러리, 총 4개를 실제 IoStore 목록에서 확인 |
| NullRHI 초기 실행 검사 | **통과.** 초기 실행 종료 코드 0. 그래픽·플레이 검사와 구분 |
| DX11 SM5 / DX12 SM6 그래픽 초기화 검사 | **둘 다 통과.** D3D11/SM5 및 D3D12/SM6 초기화 마커·실행별 nonce 일치, 두 프로세스 종료 코드 0 |
| 두 경로 실제 게임 장면 / 조작 / 저장 / 성능 | **미실시.** `renderedGameplayTested=false`. 구형 SM5 전용 GPU·실제 조작/저장/성능 검사는 10장 체크리스트로 별도 수행 |
| 결과 ZIP 위치 / 용량 / SHA-256 | `Artifacts/Releases/FestaFiesta-Win64-SM5SM6-20260911/FestaFiesta-Win64-SM5SM6-20260911.zip` / **619,451,905바이트 (약 590.76 MiB)** / `466706710a68453967ff077ef9d9e8c3eb417d2f43937b7898bf09ff519d76ec` |
| 공개 배포 판단 / 남은 문제 | **자동 품질 조건 충족, 품질 예외 없음.** `releaseStatus=validated`, `qualityOverride=false`, `latest.json` 승격 완료. 수동 플레이·다른 GPU 검증은 미실시 |

검사 PC: AMD Ryzen 7 7800X3D, NVIDIA GeForce RTX 3060 Ti, RAM 32 GB, Windows 11 25H2. 이 한 대에서 두 RHI를 검사해도 구형 SM5 전용 GPU 실기기를 검사한 것은 아니다. 자동화 테스트는 `-nullrhi` 에디터 테스트이며 그래픽 렌더링 검사가 아니다.

중간 호환성 확인본 `FestaFiesta-Win64-SM5SM6-QA-20260911-R2`와 `R3`는 `-SkipTests`와 사유를 기록한 `-QualityOverrideReason`을 사용한 내부 QA 이력이다. 데이터 검증과 패키지 검증은 유지했지만 품질 예외 표시가 붙으며 `latest.json`으로 승격하지 않는다. R3에서는 두 그래픽 경로의 실제 초기화 마커까지 확인했다. 그 이후 게임 콘텐츠와 테스트 계약을 추가 수정했으므로 **최종 결과는 생략 옵션 없는 5장의 표준 절차로 다시 판정한다.**

이 절에 기록한 Shipping 실행 라벨은 `FestaFiesta-Win64-SM5SM6-20260911`이다. `-GraphicsSmokeTest`를 사용하며 `-SkipTests`, `-SkipSmokeTest`, `-Incremental`은 사용하지 않았다. 즉 전체 자동화와 데이터 검증을 수행하고 기본 clean 빌드 및 두 그래픽 경로 초기화를 검사하는 절차다. 전체 단계가 완료됐으며 `build-manifest.json`, `verification.json`, ZIP 해시와 당시 `latest.json` 기록을 같은 빌드 라벨로 보관했다. 현재 `latest.json`은 이후 성공한 빌드로 바뀔 수 있으므로 이 이력을 확인할 때는 해당 라벨 폴더를 직접 읽는다.

자동화·데이터 검증 근거는 이 라벨의 `Validation/FestaFiesta/Report/index.json`, `Validation/FF_ZoneSystem/Report/index.json`, `Validation/DataValidation/DataValidation.log`다. 실행 중에는 `Artifacts/Releases/.staging/<빌드 라벨>`, 전체 패키징 성공 후에는 `Artifacts/Releases/<빌드 라벨>`에서 찾는다. 자동화·데이터 검증·Shipping 빌드·컨테이너 및 양쪽 RHI 초기화 검증·ZIP 생성이 모두 완료됐다. 최종 결과 경로는 `Artifacts/Releases/FestaFiesta-Win64-SM5SM6-20260911`이다.

`validated`라는 빌드 메타데이터 값은 스크립트에서 요구한 검사에 통과했다는 뜻이다. 모든 사용자 PC에서 호환된다는 인증이나 사람의 플레이 QA 완료 표시가 아니다. 아래 10장의 수동 검증까지 마친 빌드를 실제 배포 대상으로 삼는다.

## 1. 패키징은 무엇을 하는 작업인가

에디터에서 게임을 실행하는 것은 주방에서 요리사가 시식하는 것과 비슷하다. 주방에는 모든 재료와 도구가 있어서 빠진 것을 바로 꺼낼 수 있다. 사용자의 PC에는 우리 에디터와 원본 에셋이 없다. 사용자가 바로 먹을 수 있는 도시락처럼 **실행에 필요한 완성품과 재료만 빠짐없이 담아야** 한다.

| 단계 | 실제 작업 | 도시락 비유 | 문제가 생기면 확인할 것 |
| --- | --- | --- | --- |
| Build, 빌드 | C++와 플러그인을 Windows 실행 코드로 컴파일하고 연결 | 조리기구를 준비하고 조립 | C++ 오류, 컴파일러, SDK, 플러그인 |
| Cook, 쿠킹 | 맵·텍스처·머티리얼·오디오 등을 대상 플랫폼용 데이터로 가공하고 필요한 셰이더 준비 | 재료를 먹을 수 있게 조리 | 빠진 에셋, 잘못된 참조, 셰이더 오류 |
| Stage, 스테이징 | 실행 파일과 가공된 콘텐츠를 배포용 폴더에 모음 | 도시락에 반찬을 배치 | 누락된 DLL, 잘못된 폴더 구조 |
| Package, 패키지 | 콘텐츠를 컨테이너와 실행 가능한 배포 구조로 구성 | 도시락을 밀봉 | `.pak`, `.utoc`, `.ucas` 구성 |
| Archive, 보관 | 만들어진 배포 구조를 지정한 결과 위치에 복사·보관 | 출고 번호를 붙여 보관 | 출력 경로, 디스크 공간 |
| ZIP 생성 | 우리 스크립트가 배포 폴더 전체를 ZIP으로 압축 | 배송 상자에 넣기 | ZIP 생성, 해시, 다운로드 후 손상 |
| 검증·배포 | 다른 환경에서 실제 실행한 다음 사용자에게 전달 | 배송 전에 내용물과 맛 확인 | 실제 게임 흐름과 호환성 |

UAT는 Unreal Automation Tool의 약자다. 반복 작업을 순서대로 지휘하는 작업반장으로 생각하면 된다. 우리 PowerShell 스크립트가 사전 검사와 결과 보관을 관리하고, UAT의 `BuildCookRun`이 엔진 빌드 과정을 수행한다. Archive와 ZIP은 같은 말이 아니다. `-SkipZip`을 써도 Archive 단계의 실행 가능한 폴더는 만들어진다. [Epic: 패키징 개념](https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-your-project), [Epic: Build Operations](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine)

## 2. SM5 / SM6와 DirectX 11 / 12를 구분하기

**셰이더**는 GPU에 “표면의 색, 빛, 그림자를 이렇게 계산하라”라고 전달하는 작은 프로그램이다. **Shader Model, SM**은 그 프로그램이 쓸 수 있는 명령과 기능의 규격이다. **DirectX / Direct3D**는 게임이 GPU와 소통하는 그래픽 API이고, **RHI**는 Unreal이 여러 그래픽 API를 공통 방식으로 호출하기 위한 연결 계층이다.

비유하면 DirectX는 도로와 교통 체계, Shader Model은 그 도로에서 운전자가 사용할 수 있는 운전 기술이다. 둘은 관련되어 있지만 같은 버전 숫자가 아니다. “SM6이므로 반드시 하드웨어 레이 트레이싱도 된다”라는 뜻도 아니다. 기능마다 GPU, 드라이버, Windows 빌드, SM 세부 기능 조건이 추가된다.

| 우리 프로젝트의 경로 | 포함할 셰이더 형식 | 목적 | 일반 실행 시 선택 |
| --- | --- | --- | --- |
| DirectX 12 + SM6 | `PCD3D_SM6` | 기본 그래픽 경로 | 무옵션 `FestaFiesta.exe` 시작 시 지원 판정을 통과하면 사용 |
| DirectX 11 + SM5 | `PCD3D_SM5` | 더 넓은 GPU 환경을 위한 호환 경로 | 기본 경로가 지원되지 않을 때 지원되는 D3D11/SM5로 전환 |

`Config/DefaultEngine.ini`의 핵심은 다음과 같다. 이미 저장소에 있는 값이므로 처음 따라 할 때 그대로 중복 추가하지 않는다.

```ini
[/Script/WindowsTargetPlatform.WindowsTargetSettings]
DefaultGraphicsRHI=DefaultGraphicsRHI_DX12
-D3D12TargetedShaderFormats=PCD3D_SM5
+D3D12TargetedShaderFormats=PCD3D_SM6
-D3D11TargetedShaderFormats=PCD3D_SM5
+D3D11TargetedShaderFormats=PCD3D_SM5
```

Unreal 설정 파일의 `+`는 배열에 항목을 추가하고 `-`는 기존 항목을 제거한다. 위 설정은 DX12의 SM5를 제외하고 DX12 SM6 및 DX11 SM5를 선택한다. DX11 아래의 `-` 다음 `+`는 상속된 값을 정리한 뒤 원하는 항목을 확실히 넣는 방식이다. `DefaultGraphicsRHI`는 기본 선택이고, `TargetedShaderFormats`는 만들고 동봉할 셰이더 종류다. 메뉴 기본값을 바꾸는 것과 재료를 실제로 포장하는 것은 별개다.

두 경로의 셰이더를 넣으면 쿠킹 시간과 셰이더 용량이 늘 수 있다. 공통 에셋을 전부 두 벌로 복사하는 방식은 아니므로 패키지가 반드시 두 배가 되는 것은 아니다. 이 비용은 호환 범위를 넓히기 위한 선택이며, 필요 없는 세 번째 경로를 계속 추가하지 않는 것이 좋다.

### 같은 패키지라도 화면이 같지 않을 수 있는 이유

Nanite, Virtual Shadow Maps, 하드웨어 레이 트레이싱은 DX11 경로에서 같은 기능을 기대할 수 없다. Nanite 에셋은 대체 메시의 모양과 비용을 확인해야 하며, 조명과 그림자는 지원되는 대체 경로에서 직접 비교한다. [Epic: 렌더링 경로별 기능](https://dev.epicgames.com/documentation/en-us/unreal-engine/supported-features-by-rendering-path-for-desktop-with-unreal-engine?application_version=5.7)

Lumen에 대해서는 일반 기능 표만 보고 SM5에서 동일하게 동작한다고 결론 내리지 않는다. 5.7의 Lumen 상세 문서는 소프트웨어 레이 트레이싱에도 DX12 + SM6 조건을 설명한다. 우리 프로젝트의 SM5 경로에서는 Lumen과 같은 간접광 결과를 가정하지 않고, 낮·밤·실내·그늘의 가독성을 검사한다. `r.AllowStaticLighting=False`이므로 Lumen이 빠졌다고 이미 구워진 정적 조명이 자동으로 대신 생기는 것도 아니다. [Epic: Lumen 상세 조건](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine?application_version=5.7)

프로젝트는 `r.Substrate=True`, `r.Substrate.ProjectGBufferFormat=0`으로 Substrate의 Blendable GBuffer를 사용한다. 이 경로는 예측 가능한 메모리 사용과 성능을 지향한다. Epic 문서는 DX11에서 Substrate에 알려진 문제가 있을 수 있다고 명시한다. 따라서 물, 투명 유리, 금속, 잔디, 피부, UI 머티리얼의 검은색·체커보드·깜빡임을 확인한다. **호환성을 얻겠다고 Substrate를 갑자기 꺼서는 안 된다.** 이미 변환된 머티리얼이 검게 나올 수 있어 에셋 이관과 시각 검증이 필요한 별도 작업이다. [Epic: Substrate 개요와 제한](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-substrate-materials-in-unreal-engine?application_version=5.7)

일반 사용자는 `FestaFiesta.exe`를 더블클릭한다. 설치된 UE 5.7.4의 Windows RHI 선택 코드는 기본 DX12/SM6의 지원 여부를 확인하고, 미지원이면 지원되는 D3D11/SM5 경로로 전환한다. 이 동작은 **초기 지원 판정 단계의 자동 선택**이며, 그래픽 장치를 만든 뒤 발생한 드라이버 크래시나 플레이 중 오류에서 게임을 다시 시작해 주는 기능은 아니다. 강제로 지정한 `-dx12 -sm6`는 해당 경로를 요구하므로 자동 선택과 다르게 동작한다. 두 경로를 강제로 비교하는 명령은 10장의 담당자용 고급 QA에서만 사용한다.

## 3. 무엇을 설치해야 하나 — 제작 PC와 플레이 PC

**제작 PC**에는 엔진과 컴파일러가 필요하다. **플레이 PC**에는 완성된 배포 폴더, 지원되는 Windows·GPU 드라이버, x64 실행 런타임이 필요하다. 사용자가 Unreal Editor나 Visual Studio를 설치할 필요는 없다.

| 제작 PC 구성 요소 | 이 저장소의 고정 기준 | 확인 이유 |
| --- | --- | --- |
| Unreal Engine | `5.7.4`, CL `51494982` | 같은 5.7이라도 패치에 따라 쿠커·셰이더·빌드 결과가 달라질 수 있음 |
| Visual Studio | VS 2022, 17.14 계열 권장 | Unreal 5.7의 C++ 개발 환경 기준 |
| MSVC 실제 도구 버전 | `14.44.35228` | 컴파일과 링크 도구 일치. 2026-09-18 VS 17.14 서비스 업데이트로 `35225`→`35228` 갱신 |
| MSVC 설치 폴더 버전 | `14.44.35207` | 버전 표시와 폴더 이름이 서로 다를 수 있음 |
| Windows SDK | `10.0.22621.0` | Windows 헤더와 라이브러리 일치 |
| Git LFS | `3.5.1` **이상** | 대용량 에셋 다운로드와 무결성 검사. 결과물에 영향을 주지 않는 도구라 정확한 패치 대신 최소 버전만 요구 |
| 결과 대상 | `Win64`, `Shipping` | 64비트 Windows 배포본 |

위의 정확한 값은 `Tools/Build/toolchain.lock.json`에서 읽는다. Epic의 일반 최소 요구 버전과 **이 프로젝트가 재현을 위해 고정한 버전**은 다를 수 있다. “더 최신이 설치됐으니 같은 것”으로 취급하지 않는다. 버전을 올리려면 잠금 파일, 설치 환경, 실제 빌드 로그를 함께 갱신하고 다시 검증한다. [Epic: Unreal 5.7 Visual Studio 설정](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.7)

### 3-1. 엔진 설치

1. Epic Games Launcher의 Unreal Engine 라이브러리에서 팀과 같은 5.7.4 설치본을 준비한다. 기존 설치본을 이용할 때도 아래 파일을 확인한다.
2. 일반적인 설치 경로는 `C:\Program Files\Epic Games\UE_5.7`이다. D 드라이브 등 다른 위치를 썼다면 이후의 `$FFEngine` 값을 실제 위치로 바꾼다.
3. `Engine\Build\Build.version`의 `MajorVersion=5`, `MinorVersion=7`, `PatchVersion=4`, `Changelist=51494982`를 확인한다. 다른 패치를 프로젝트에서 즉석 변환하여 맞추지 않는다.
4. 컴파일러가 준비된 뒤 `.uproject`를 열어 최초 에셋과 셰이더 준비를 마친다. 모듈 빌드가 필요하다는 정상 안내가 뜨면 설치된 5.7.4로 빌드한다. 버전 변환 안내라면 선택한 엔진을 먼저 확인한다.

### 3-2. Visual Studio / SDK 설치

1. Windows 검색에서 **Visual Studio Installer**를 연다.
2. VS 2022의 **수정(Modify)**을 누른다.
3. **C++를 사용한 게임 개발**, **C++를 사용한 데스크톱 개발** 워크로드를 선택한다.
4. **개별 구성 요소**에서 MSVC v143의 14.44 계열, Windows 11 SDK `10.0.22621.0`이 설치되도록 선택한다. 목록의 표시 이름만으로 정확한 패치가 같다고 판단하지 말고 설치 후 폴더와 실제 컴파일러 출력을 확인한다.
5. Unreal용 Visual Studio 도구, Blueprint 디버거 도구, Unreal Engine Test Adapter는 개발 편의를 위해 함께 설치할 수 있다.
6. 설치를 완료하고 새 PowerShell 창을 연다. 설치 프로그램이 요청했다면 Windows를 재시작한다.

이 프로젝트의 현재 사전 검사는 기본 VS 설치 경로인 `C:\Program Files\Microsoft Visual Studio\2022\<에디션>` 아래를 찾는다. VS를 다른 위치에 설치한 환경은 검사 경로 지원을 추가하거나 팀 기준 설치 경로를 사용해야 한다. 에디션은 Community, Professional, Enterprise 등 실제 폴더에 따라 다르다. [Microsoft: Unreal용 Visual Studio 도구 설치](https://learn.microsoft.com/en-us/visualstudio/gamedev/unreal/get-started/vs-tools-unreal-install)

### 3-3. Git과 Git LFS

Git은 소스의 변경 이력을 관리하고, Git LFS는 큰 에셋 파일을 별도로 저장한다. LFS 다운로드가 안 된 `.uasset`은 실제 그림이나 맵 대신 “진짜 파일은 이 주소에 있다”라는 작은 안내장이 들어 있는 상태다. 파일 이름이 보인다고 다운로드가 끝난 것은 아니다.

저장소를 이미 받았다면 다시 복제하지 않는다. 새 PC에서는 팀 저장소 접근 권한을 준비한 뒤 실제 저장소 URL을 사용하여 복제한다. 아래 `<팀 저장소 URL>`은 교체할 자리이며 그대로 실행하지 않는다.

```powershell
git clone <팀 저장소 URL> C:\GitHub\FestaTeam\8th-Team13-Final-Project
```

기존·신규 PC 공통으로 프로젝트 폴더에서 실행한다.

```powershell
Set-Location 'C:\GitHub\FestaTeam\8th-Team13-Final-Project'
git --version
git lfs version
git lfs install --local
git lfs pull
git lfs fsck
git status --short
```

`git lfs version`이 `3.5.1` 이상인지 확인하고, `git lfs fsck`가 정상 완료되어야 한다. 다운로드 실패, 인증 실패, 용량 제한이 나오면 해당 문제를 해결한 뒤 `git lfs pull`을 다시 수행한다. 파일 내용을 텍스트로 수정해서 포인터 오류를 없애려 하면 에셋을 손상시킨다.

### 3-4. 디스크와 작업 폴더

소스, 엔진, 중간 산출물, 가공 캐시, 압축 전 패키지, ZIP이 동시에 존재한다. 스크립트는 출력 드라이브에 최소 **10 GiB**를 요구하지만 이는 작업 완료를 보장하는 용량이 아니다. 이 프로젝트의 이전 성공 빌드 크기와 캐시를 기준으로 충분한 추가 여유를 확보한다. 로컬 SSD와 짧은 경로를 사용하면 파일 접근과 경로 문제를 줄이기 쉽다. 동기화 중인 클라우드 폴더에서 릴리스 작업을 수행할 때는 파일 잠금·동기화 간섭을 확인한다.

## 4. 첫 명령을 실행하기 전 점검

1. Unreal Editor에서 **Save All**로 변경을 저장한다. 디스크에 저장되지 않은 변경은 쿠킹에 들어가지 않는다.
2. Editor, 해당 게임, Live Coding 실행을 종료한다. DLL·실행 파일이 사용 중이면 새 빌드가 교체하지 못할 수 있다.
3. 아래 명령으로 작업 폴더, 버전, 변경 파일을 확인한다.
4. 공개 배포 후보는 팀이 검토한 커밋에서 만드는 것을 원칙으로 한다. `dirty`는 커밋에 없는 변경이 있다는 뜻이므로 변경 이유와 차이를 함께 기록한다. 변경을 없애려고 `git reset --hard`를 실행하지 않는다.

```powershell
Set-Location 'C:\GitHub\FestaTeam\8th-Team13-Final-Project'
$FFEngine = 'C:\Program Files\Epic Games\UE_5.7'
Get-Content -LiteralPath '.\Tools\Build\toolchain.lock.json'
Get-Content -LiteralPath (Join-Path $FFEngine 'Engine\Build\Build.version')
git status --short
git rev-parse HEAD
git lfs version
git lfs fsck
```

코드 블록의 명령 앞에 표시되지 않은 `PS C:\...>` 프롬프트는 복사할 필요가 없다. `$FFEngine`은 뒤에서 재사용하는 경로 변수이고, `&`는 PowerShell에서 파일 경로를 실행하라는 기호다. 작은따옴표 안의 경로는 공백이 있어도 하나로 처리한다. 이 문서는 긴 명령을 한 줄로 제공하므로 줄 끝 백틱 누락을 걱정하지 않아도 된다.

회사 정책 때문에 스크립트 실행이 차단된다면 정책 담당 절차를 따른다. 개인 PC에서 내려받은 신뢰 가능한 저장소 스크립트가 인터넷 출처 표시로만 차단된 경우에는 해당 파일의 속성에서 차단 해제를 확인할 수 있다. 기계 전체의 실행 정책을 무조건 해제하는 명령을 선행 준비로 사용하지 않는다.

## 5. 가장 먼저 따라 할 표준 패키징

### 5-1. 한 번에 전체 절차 실행

표준 명령은 자동화 테스트나 데이터 검증에 실패가 있으면 그 단계에서 멈춘다. 실패 원인을 수정하고 다시 실행하여 공개 배포 후보를 만든다. 부록의 중간 QA 빌드에서 사용한 품질 예외를 일반 배포 절차로 사용하지 않는다. 개별 테스트를 고친 사실과 최종 전체 검사의 통과는 각각 보고서로 확인한다.

GPU를 직접 사용할 수 있는 제작 PC에서는 다음 명령으로 자동 검사, Shipping 패키징, 두 그래픽 경로 초기화 검사, ZIP 생성까지 진행한다.

```powershell
& '.\Tools\Build\Package-Win64.ps1' -EngineRoot $FFEngine -GraphicsSmokeTest
```

`-GraphicsSmokeTest`는 패키지의 두 그래픽 경로 초기화를 검사하는 선택 사항이다. 현재 전체 자동화에는 실제 위젯 픽셀 검사가 포함되므로 `Run-AutomationTests.ps1`은 `-RenderOffscreen`으로 실행하고 GPU·드라이버가 있는 Windows runner가 필요하다. 창을 띄우지 않는 것과 그래픽 장치가 필요 없는 것은 다르다. 데이터 검증은 `-nullrhi`를 유지한다. 아래 명령은 패키지 그래픽 초기화만 생략하므로, 결과물을 QA PC로 보내 9장의 그래픽 검사와 10장의 플레이 검사를 수행한다.

```powershell
& '.\Tools\Build\Package-Win64.ps1' -EngineRoot $FFEngine
```

아무 출력이 잠시 없다고 곧바로 실패는 아니다. 처음에는 셰이더 컴파일과 데이터 가공에 시간이 많이 걸릴 수 있다. 작업 관리자에서 프로세스의 CPU·디스크 사용과 로그의 최종 수정 시간을 확인한다. 기종·캐시·변경량에 따라 시간 차이가 커서 고정 소요 시간을 약속하지 않는다.

**경로 길이 주의.** 스크립트는 소스의 `Saved\Cooked\Windows\FestaFiesta\Content` 아래 에셋 경로와 출력의 `.staging\<라벨>\Package` 아래 배포 경로를 각각 검사한다. `-OutputRoot`나 짧은 라벨은 출력·ZIP 경로를 줄이지만 소스의 Cook 경로를 줄이지 않는다. 프로젝트가 `OneDrive\Desktop\...`처럼 깊은 곳에 있으면 짧은 checkout이나 같은 checkout을 가리키는 junction을 쓰고 `-ProjectFile`로 짧은 프로젝트 경로를 전달한다. 콘텐츠를 삭제하거나 이름을 바꿔 경로 문제를 우회하지 않는다.

다음은 현재 checkout을 `C:\FFProj`로 연결하는 예시다. 해당 경로가 없을 때 한 번만 junction을 만들며, 이미 있다면 원하는 checkout을 가리키는지 확인한다. 경로·드라이브·라벨은 팀원 환경에 맞게 바꾼다. `-ProjectFile`은 junction의 짧은 경로를 유지하고 같은 FestaFiesta 프로젝트인지 검사한다.

```powershell
$FFCheckout = (Get-Location).Path
New-Item -ItemType Junction -Path 'C:\FFProj' -Target $FFCheckout
$FFProject = 'C:\FFProj\FestaFiesta.uproject'
& '.\Tools\Build\Package-Win64.ps1' -EngineRoot $FFEngine -ProjectFile $FFProject -OutputRoot 'C:\Builds\FestaFiesta' -BuildLabel 'FF-Win64-Shipping-20260930-01' -GraphicsSmokeTest
```

기본 결과 위치는 `Artifacts\Releases`다. 빌드 라벨은 UTC 시각과 Git 커밋으로 자동 생성된다. 예를 들어 `FestaFiesta-Win64-Shipping-20260911-120000-abcdef123456`처럼 구분되며, 변경된 작업 트리는 `-dirty`가 붙을 수 있다.

### 5-2. 결과 위치와 이름을 직접 지정

```powershell
$FFLabel = 'FestaFiesta-Win64-Shipping-SM5-SM6-20260911-01'
& '.\Tools\Build\Package-Win64.ps1' -EngineRoot $FFEngine -OutputRoot 'D:\FestaFiestaBuilds' -BuildLabel $FFLabel -GraphicsSmokeTest
```

`D:\FestaFiestaBuilds`는 예시다. 실제 있는 출력 드라이브로 바꾼다. 빌드 라벨은 1~120자이고, 영문·숫자로 시작하며 영문·숫자·점·밑줄·하이픈만 사용할 수 있다. 이미 같은 라벨의 결과나 `.staging` 폴더가 있으면 덮어쓰지 않고 멈춘다. 재시도할 때는 `-02`처럼 새 라벨을 사용하면 이전 실패 기록을 보존할 수 있다.

### 5-3. 스크립트가 자동으로 하는 일

1. 선택한 엔진 버전과 CL, 고정 SDK·MSVC 설치 위치, Git LFS 최소 버전·파일 무결성·포인터 잔존 여부, 출력 드라이브 여유를 검사한다.
2. 검증에 사용할 `FestaFiestaEditor Win64 Development`를 빌드한다.
3. 기본 `FestaFiesta`, `FF_ZoneSystem` 전체 자동화 테스트를 `-RenderOffscreen`으로 실행하고, 프로젝트 데이터 검증은 `-nullrhi`로 실행한다.
4. UAT로 `FestaFiesta Win64 Shipping`을 Build → Cook → Stage → Package → Archive한다. UAT 로그에 실제 MSVC·SDK 선택이 보고되면 잠금 버전과 대조하고 불일치 시 중단한다.
5. 필요한 런타임·라이선스·한국어 실행 안내를 구성한다. 사용자는 엔진이 만든 루트 `FestaFiesta.exe`를 더블클릭하며, 데이터·라이브러리는 원래 하위 폴더에 유지한다. Win64에 불필요한 ARM64 재배포 설치기는 제외한다.
6. Shipping PDB를 내부 `Symbols`에 따로 보관한다. dirty 작업 트리에서는 `Metadata/source-dirty.patch`와 함께 수정된 추적 파일의 실제 바이트를 `source-dirty-tracked.zip`, 미추적 파일을 `source-dirty-untracked.zip`에 보관하여 LFS 포인터만 남는 경우도 복원할 수 있게 한다.
7. 컨테이너, 필수 맵·데이터·UI, 두 종류의 셰이더 라이브러리 포함 여부와 기본 초기 실행을 검사한다. 요청했으면 두 그래픽 경로의 초기화 검사도 수행한다.
8. 파일·ZIP의 SHA-256, 빌드 메타데이터, 검증 보고서를 남긴다.
9. 성공한 결과를 `.staging`에서 최종 라벨 폴더로 승격하고 정상 품질 검사를 거친 빌드의 `latest.json`을 갱신한다.

이 절차는 실패 중간물을 최신 배포본으로 오인하지 않게 한다. `latest.json`은 최신 스크립트 통과 결과를 가리키는 색인이지 자동 업로드나 앱스토어 공개 동작이 아니다. 실제 도구 선택은 `Validation\EditorBuild.log`와 `Logs\BuildCookRun.log`의 MSVC·SDK 선택 메시지도 대조한다. `build-manifest.json`은 요청한 버전과 실제 로그에서 확인한 버전을 구분한다. 최신 상태라 재컴파일하지 않아 UBT가 선택 메시지를 생략하면 실제 선택 값은 `null`, 근거는 `not-reported-by-UBT`로 남긴다. 설치 유무만으로 바이너리를 만든 도구까지 입증했다고 기록하지 않는다.

## 6. 모든 실행 옵션의 의미

| `Package-Win64.ps1` 옵션 | 기본 동작 / 용도 | 사용할 때 판단 |
| --- | --- | --- |
| `-EngineRoot <경로>` | 엔진 위치 지정 | 여러 엔진이 설치된 PC에서 명시하는 편이 이해하기 쉬움 |
| `-ProjectFile <경로>` | 기본은 스크립트가 속한 저장소의 `FestaFiesta.uproject` | 깊은 checkout은 `C:\FFProj\FestaFiesta.uproject`처럼 짧은 junction 경로로 Cook 경로를 줄임 |
| `-OutputRoot <경로>` | 기본 `Artifacts\Releases` | 결과를 별도 드라이브에 둘 때 |
| `-BuildLabel <이름>` | 기본 UTC 시각 + Git 커밋 | 회차를 사람이 알아보기 쉽게 구분할 때 |
| `-Configuration Shipping 또는 Development` | 기본 `Shipping` | 콘솔과 로그로 문제를 직접 확인하는 빌드는 `Development`를 명시 |
| `-Incremental` | 기본적으로 UAT `-clean` 사용; 이 옵션은 `-clean` 생략 | 반복 확인 시간을 줄이는 내부 작업에 사용 |
| `-GraphicsSmokeTest` | 선택한 GPU에서 DX11 SM5 / DX12 SM6 초기화 검사 추가 | GPU 제작·QA PC에서 사용; 전체 플레이 검사와 함께 수행 |
| `-SkipZip` | 기본 ZIP 생성 | 내부에서 폴더 자체를 검사할 때 ZIP 시간·공간 절약 |
| `-SkipTests` | 기본 자동화 테스트 수행 | 진단용 품질 예외에만 사용; Editor 빌드·데이터 검증은 유지 |
| `-SkipSmokeTest` | 기본 NullRHI 초기 실행 검사 수행 | 진단용 품질 예외에만 사용 |
| `-QualityOverrideReason <사유>` | 기본 없음 | 위 두 생략 옵션을 쓸 때 10~500자의 사유 필수 |

`-Incremental`은 **기존 스크립트의 clean 플래그를 생략하는 옵션**이다. 이름만 보고 `-iterate` 쿠킹, 차등 패치, 완전한 캐시 적중을 보장한다고 생각하지 않는다. `FullRebuild=True` 등 프로젝트 설정도 빌드에 관여한다. 실제로 무엇을 재사용했는지는 UAT 로그에서 확인한다.

`-SkipTests`나 `-SkipSmokeTest`를 사용하면 `quality-override`로 기록되고 결과물에 `QUALITY-OVERRIDE.txt`가 들어간다. 이 결과는 내부 진단용이며 `latest.json`으로 승격하지 않는다. 테스트가 실패했다고 그대로 옵션으로 건너뛰어 공개 배포하면 문제가 숨겨질 뿐이다. `-QualityOverrideReason`은 생략 옵션 없이 단독으로 사용할 수 없다.

자주 반복하는 내부 확인 예시는 다음과 같다. 자동화 테스트와 기본 초기 실행 검사는 계속 수행한다.

```powershell
& '.\Tools\Build\Package-Win64.ps1' -EngineRoot $FFEngine -Incremental -SkipZip
```

### Development 진단 빌드

```powershell
& '.\Tools\Build\Package-Win64.ps1' -EngineRoot $FFEngine -Configuration Development -BuildLabel 'FestaFiesta-Win64-Development-SingleExe-20260911' -GraphicsSmokeTest
```

이 옵션은 실제 게임 빌드 구성을 바꾼다. 검사용 에디터만 Development로 만드는 것과 다르다. 자동화·콘텐츠·패키지 품질 검사는 그대로 수행하며, Development용 `README_KO.md`를 배포 폴더에 포함한다. 두 구성 모두 사용자가 누르는 파일은 맨 위 `FestaFiesta.exe` 하나다. 이 파일은 엔진의 기본 bootstrap으로, 필요한 내부 실행 파일을 시작한다. 내부 파일은 Development의 경우 `FestaFiesta/Binaries/Win64/FestaFiesta.exe`, Shipping의 경우 같은 폴더의 `FestaFiesta-Win64-Shipping.exe`다. 내부 파일도 필수 구성 요소이므로 지우지 않는다. PDB는 배포 ZIP 밖 `Symbols`에 보관한다.

일반 더블클릭 실행에 QA용 인자를 자동 주입하지 않는다. Development에서 콘솔을 열거나 NPC 상태를 읽는 방법은 동봉 안내서를 따른다. 일반 실행 로그는 압축을 푼 폴더 기준 `FestaFiesta/Saved/Logs/FestaFiesta.log`에서 찾는다. `-log`는 별도 로그 창을 여는 옵션이며 기본 파일 로그를 남기기 위해 필수인 옵션은 아니다. 위 라벨이 이미 있으면 재시도에는 `-R2`처럼 새 라벨을 사용한다.

이미 만든 Development 결과를 다시 검사할 때도 구성을 명시한다. `$FFRelease`에는 해당 결과 폴더의 전체 경로를 넣는다.

```powershell
& '.\Tools\Build\Verify-Package.ps1' -PackagePath (Join-Path $FFRelease 'Package') -EngineRoot $FFEngine -Configuration Development -SmokeTest -GraphicsSmokeTest -ReportDirectory (Join-Path $FFRelease 'Recheck')
& '.\Tools\Build\Test-PackagedNpcArrival.ps1' -PayloadRoot (Join-Path $FFRelease 'Package\Windows')
```

첫 명령은 배포 파일 구성과 그래픽 초기화를 검사한다. 검사 중 생기는 로그·사용자 설정은 배포 폴더 밖에 저장하며, 배포 폴더에 `Saved`가 섞이면 오류로 처리한다. 사용자가 플레이한 복사본 대신 새로 압축을 푼 배포본을 검사한다. 두 번째 명령은 완성된 Development 실행 파일에서 NPC 시간표의 메모리 수명 회귀 검사 1개를 실행한다. 새 JSON 보고서의 정확한 검사 이름·성공 상태·완료 개수 및 종료 코드 0을 확인해야 통과하며, 결과는 기본적으로 `Artifacts/Validation/NpcSpawn/Packaged-<실행 식별자>`에 남는다. 이 검사는 화면을 그리지 않으므로 실제 조작·표시 확인은 Development 안내서의 수동 절차로 이어서 수행한다.

## 7. 에디터 UI에서는 무엇을 확인하나

정식 결과는 위 스크립트로 만들고, 에디터는 설정을 이해하고 에셋을 확인하는 데 사용한다. 에디터에서 패키징 버튼만 누르면 우리 스크립트의 해시·별도 심볼·필수 에셋·다중 셰이더 검사까지 모두 자동 수행되지는 않는다.

1. **Edit → Project Settings → Platforms → Windows**에서 Default RHI가 DirectX 12인지, D3D12 SM6와 D3D11 SM5가 대상에 포함되는지 확인한다. UI 언어에 따라 명칭이 번역될 수 있으므로 검색창에서 `RHI`, `Shader`를 검색해 찾는다.
2. **Project Settings → Maps & Modes**에서 Game Default Map이 `Frontend`인지 확인한다. 에디터에서 현재 열어 둔 맵과 플레이어가 처음 보는 맵은 다를 수 있다.
3. **Project Settings → Packaging**에서 Shipping, Pak, IoStore, 압축, Prerequisites, 맵 목록을 확인한다. 고급 항목은 펼치거나 검색한다.
4. Rendering의 SM·RHI·Substrate 관련 설정을 변경했다면 저장하고 요구되는 에디터 재시작을 수행한다. 이 변경은 많은 셰이더 재컴파일을 유발할 수 있다.
5. 단순 UI 패키징을 익히고 싶다면 상단 **Platforms → Windows → Shipping / Package Project** 흐름을 사용한다. 패키징 로그를 열어 완료·오류를 확인하고, 배포 후보는 공식 스크립트로 다시 만들어 검증 자료를 남긴다. [Epic: 패키징 UI와 Project Launcher](https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-your-project)

## 8. 우리 프로젝트가 선택한 최적화와 이유

현업의 최적화는 무조건 많은 기능을 끄는 것이 아니다. **측정 가능한 크기·시간·성능을 개선하면서 재현성과 실행 안정성을 유지하는 것**이다. 현재 설정의 근거와 다음 개선의 전제 조건을 분리해서 이해한다.

| 현재 선택 | 얻는 효과 | 비용과 확인 사항 |
| --- | --- | --- |
| Shipping 빌드 | 배포용 코드 구성 사용 | 개발용 콘솔·통계·일부 로그 도구의 사용 범위가 다름 |
| Pak + IoStore | 콘텐츠를 관리 가능한 컨테이너로 묶음 | 파일 한 개를 임의로 빼면 실행에 필요한 데이터가 사라짐 |
| Oodle Kraken, Test/Shipping 레벨 7 | 압축 효율을 고려한 배포 설정 | 압축 준비 시간이 증가할 수 있음; 실제 로딩 시간은 측정 필요 |
| 셰이더 코드 공유 | 머티리얼 사이의 중복 셰이더 코드 감소 | 두 SM 경로의 라이브러리가 모두 들어갔는지 검사 |
| 결정적 셰이더 정렬 | 결과 비교·변화 추적에 유리 | 전체 ZIP이 매번 바이트까지 동일하다는 뜻은 아님 |
| 두 시작·게임 맵 지정 | 불필요한 전체 맵 쿠킹을 억제 | 새 실제 게임 맵을 추가하면 목록도 갱신 |
| 동적 경로 폴더 Always Cook | 문자열로 로드하는 UI·데이터 누락 방지 | 폴더 단위라 필요 이상으로 포함될 수 있음 |
| PDB 별도 보관 | 배포 다운로드 크기 줄이면서 내부 장애 분석 자료 유지 | ZIP만 보관하면 정확한 크래시 분석 자료가 사라짐 |
| en / ko 문화권 스테이징 | 지원 언어에 맞게 국제화 데이터 구성 | 실제 한글 글꼴·문자 줄바꿈도 확인 |
| x64 prerequisite만 포함 | Win64 대상에 맞는 실행 의존성 제공 | 새 PC에서 설치와 첫 실행 확인 필요 |
| Chunk 생성 안 함 | 현재 단일 배포 흐름을 단순하게 유지 | DLC·차등 업데이트는 별도 설계가 필요 |
| Shipping LTCG (`FestaFiesta.Target.cs`) | 게임·`FF_ZoneSystem` 코드의 모듈 경계를 넘는 인라이닝과 미사용 코드 제거 | 링크 시간 증가. 런처 엔진 라이브러리는 미리 빌드돼 대상이 아님. 공유 빌드 환경 검사 때문에 `bDefineForceInlineHintToInline=false`를 함께 고정 |
| Oodle Texture RDO, lambda 30 (`DefaultEngine.ini`) | 텍스처 데이터가 Kraken으로 더 잘 압축됨. 2026-09-18 측정 −18.3 MiB | 설정 변경 후 첫 쿠킹에서 텍스처 재인코딩. 번짐이 보이는 텍스처는 개별적으로 Lossy Compression Amount를 끔 |
| 미사용 지구본 시안 NeverCook (`BrightGlobe_v1/P02`, `P03`) | UI 폴더 AlwaysCook이 끌고 오던 이전 시안 제외. −35.8 MiB | 참조 그래프로 게임 루트에서 도달 불가함을 확인. 쿠킹 대상이 하드 참조하면 쿠킹 누락 게이트가 빌드를 실패시킴 |
| GameplayCue 스캔 경로 한정 (`DefaultGame.ini`) | 쿠킹·게임 시작 때의 `/Game` 전체 스캔과 경고 제거 | GameplayCue를 추가할 때는 `/Game/FestaFiesta/GameplayCues`에 둠 |

### 압축을 더 세게 하면 FPS도 올라가나

압축은 주로 저장 크기, 다운로드 크기, 읽기와 압축 해제 비용에 영향을 준다. 삼각형 수, 조명 계산, AI·군중 연산을 자동으로 줄여 주지는 않는다. Kraken이라는 **방법**은 압축률과 해제 속도의 균형을 정하고, 레벨 7이라는 **노력 수준**은 압축할 때 쓸 시간의 성격을 정한다. 작은 ZIP이 곧 높은 FPS라는 뜻은 아니다. [Epic: Oodle Data](https://dev.epicgames.com/documentation/en-us/unreal-engine/oodle-data?application_version=5.7)

### 왜 Editor Content 제외를 무조건 켜지 않나

현재 `bSkipEditorContent=False`다. `Test_Main`의 Water·Landmass 및 포토존 런타임 의존성 일부가 엔진의 에디터 콘텐츠에 닿아 있어, 현 상태에서 일괄 제외하면 쿠킹이 실패할 수 있다는 프로젝트 기록이 있다. “Editor”라는 이름만 보고 폴더를 삭제하거나 설정을 바꾸지 않는다. Reference Viewer로 의존성을 확인하고 필요한 런타임 에셋으로 교체한 뒤 재검증하는 것이 개선 순서다. 이는 에디터 **실행 바이너리**를 플레이어에게 넣어도 된다는 뜻과는 다르다. 검증 스크립트는 에디터 바이너리 유출을 별도로 검사한다.

### 왜 UI 폴더를 통째로 쿠킹하나

코드가 에셋을 객체 참조로 갖고 있으면 쿠커가 참조를 따라갈 수 있다. 그러나 C++의 문자열 경로로 나중에 로드하는 에셋은 자동 수집에서 놓칠 수 있다. 이 프로젝트는 과거 홍보 확인창과 시간표 데이터 누락 때문에 패키지에서만 UI가 안 열리거나 개장이 진행되지 않은 기록이 있어 관련 경로를 `DirectoriesToAlwaysCook`으로 명시했다.

현재 핵심 경로는 `Zone/UI`, `Building/FF_WBP_Building`, `UI`, `Data`이다. `Minigame` Cook/scan 설정은 타인 소유 파일에 남아 있지만 삭제한 서핑 자료를 패키지 필수 에셋으로 요구하지 않는다. 새로 문자열 경로 로딩을 추가하면 해당 에셋의 Cook 규칙도 함께 검토한다. 장기적으로는 Asset Manager의 Primary Asset, 명시적 soft reference와 규칙을 정리하여 필요한 에셋만 정확하게 포함시키는 방향으로 개선할 수 있다. **폴더 제외부터 하는 것보다 참조 설계를 먼저 고치는 것이 안전하다.**

### 런타임 성능은 따로 측정한다

비교 조건은 같은 맵·같은 카메라 경로·같은 군중 규모·같은 해상도·같은 그래픽 품질로 맞춘다. 프레임 시간 16.7 ms는 약 60 FPS, 33.3 ms는 약 30 FPS다. 평균 FPS뿐 아니라 간헐적으로 한 프레임이 길어지는 현상을 기록한다. GPU가 느리면 해상도·그림자·재질 비용을 보고, CPU가 느리면 AI·군중·틱·경로 탐색 비용을 본다. 디스크 크기 최적화와 별도의 문제다.

Unreal Insights 등 상세 진단은 필요한 기능이 포함된 Development 또는 Test 빌드로 수행하고, 최종 체감과 장시간 안정성은 Shipping 결과물에서도 확인한다. Shipping 콘솔에서 `stat unit`이나 `-ExecCmds`가 반드시 동작할 것이라고 가정한 절차를 만들지 않는다.
### 2026-09-18 최적화 측정: 무엇이 얼마나 줄었나

같은 커밋(`29b8765b`)과 같은 UAT 인자로 베이스라인과 최적화본을 각각 Clean Shipping 패키징한 뒤 IoStore 목록을 패키지 단위로 비교했다. 원인별로 분리한 컨테이너(압축 후) 기준 수치다.

| 원인 | 컨테이너 변화 | 판정 근거 |
| --- | --- | --- |
| 지구본 이전 시안 P02·P03 제외 | −35.8 MiB | 사라진 패키지는 정확히 이 두 폴더의 37개뿐이고 추가된 패키지는 0개 |
| Oodle Texture RDO | −18.3 MiB | 메시 외 패키지 381.4 → 363.1 MiB. 대부분 텍스처 mip(`.ubulk`) |
| (참고) 메시 인덱스 버퍼 | −19.8 MiB | 설정 변경 효과가 **아니다**. 아래 설명 참고 |

합계는 ZIP 721.1 → 655.1 MiB(−9.2%), 설치 용량 849.7 → 775.6 MiB(−8.7%)다. LTCG는 실행 파일 크기를 거의 바꾸지 않는다(143.9 → 144.0 MiB). 효과는 CPU 시간이므로 크기가 아니라 10장의 같은 시나리오 프레임 시간으로 비교한다.

**메시 인덱스 버퍼 차이에 대해.** UE 5.7 기본값 `r.SupportDepthOnlyIndexBuffers=0`, `r.SupportReversedIndexBuffers=0`에서는 뎁스 전용·역방향 인덱스 버퍼를 쓰지 않는다. 그런데 메시가 바뀐 직후 DDC가 비어 있는 첫 쿠킹에서는 새로 빌드된 렌더 데이터의 이 버퍼가 그대로 저장되어, 32비트 인덱스 메시에서 삼각형당 정확히 24바이트가 더 들어갔다(예: `SM_BrightWorld_04_Top` 307,737 × 24 = 7,385,688바이트). 베이스라인 설정 그대로 다시 쿠킹해도 작은 쪽 크기가 나왔으므로 이번 설정 변경의 효과로 계산하지 않았다. 기능 차이는 없으며, 같은 패키지 크기를 비교하려면 DDC가 채워진 상태에서 두 번째 쿠킹 결과를 기준으로 삼는다.

### 다음 크기 최적화 후보

매 릴리스의 `Metadata/size-report.json`에서 확인한다. 아래는 2026-09-18 측정에서 나온, 담당자 판단이 필요한 후보다.

| 후보 | 규모(압축 후) | 막는 이유와 해결 방법 |
| --- | --- | --- |
| `UI/Atlas/Earth`의 나머지 미사용 메시(`DioramaWorld_v1`, `P03v2`) | 약 114 MiB | 실사용 P05·P10 머티리얼이 이 폴더의 텍스처 3개(`T_Diorama_Height`, `T_Bright_BiomeSource`, `T_Bright_LandClass`)를 공유한다. 담당자가 세 텍스처를 실사용 폴더로 옮긴 뒤 두 폴더를 NeverCook하거나 UI 밖으로 보관 |
| 달 텍스처(`Environment/moon`) | 약 36 MiB | 하늘의 먼 물체에 초고해상도 텍스처. Maximum Texture Size 또는 LOD Bias로 해상도 제한 후 화면 비교 |
| 셰이더(SM5 + SM6) | 약 144 MiB | 두 그래픽 경로와 레이 트레이싱이 켜져 있어 크다. 렌더링 기능 결정이므로 패키징에서 단독으로 바꾸지 않음 |

## 9. 결과 폴더 읽기와 자동 재검사

정상 결과 폴더는 대략 다음 구조다. `Package` 아래 플랫폼 폴더 이름은 엔진 결과에 따라 다를 수 있으므로 최종 출력의 `Payload:` 경로를 기준으로 찾는다.

```text
Artifacts/Releases/
  latest.json
  <빌드 라벨>/
    <빌드 라벨>.zip
    Package/
      Windows/                 # 예시: FestaFiesta.exe가 있는 실제 payload
        FestaFiesta.exe
        README_KO.md
        build-manifest.json
        Engine/
        FestaFiesta/
          Binaries/Win64/
          Content/Paks/
            FestaFiesta-Windows.pak
            FestaFiesta-Windows.utoc
            FestaFiesta-Windows.ucas
            global.utoc
        Licenses/THIRD_PARTY_NOTICES.md
    Metadata/
      build-manifest.json
      verification.json
      Smoke-NullRHI.json
      Smoke-DX11-SM5.json       # GraphicsSmokeTest를 수행한 경우
      Smoke-DX12-SM6.json       # GraphicsSmokeTest를 수행한 경우
      Smoke-Default-EXE.json    # GraphicsSmokeTest: 기본 런처·기본 RHI 초기화
      IoStore-Contents.csv
      size-report.json          # 용량 분해·직전 승격 빌드 대비 증감
      source-dirty.patch        # dirty 작업 트리에서 만든 경우에만
      source-dirty-tracked.zip  # 변경된 추적 파일이 있으면 실제 바이트 보관; LFS 포함
      source-dirty-untracked.zip # 미추적 파일이 있으면 실제 바이트 보관
      source-dirty-untracked.txt # dirty 후보의 UTF-8 미추적 파일 목록
      PACKAGE-SHA256SUMS.txt
      SYMBOLS-SHA256SUMS.txt
      ARCHIVE-SHA256.txt
    Symbols/
    Logs/BuildCookRun.log
    Validation/
      EditorBuild.log
      FestaFiesta/
      FF_ZoneSystem/
      DataValidation/
```

`.ucas`는 큰 콘텐츠의 본체, `.utoc`는 그 콘텐츠를 찾기 위한 목차로 이해하면 쉽다. `.pak`도 함께 필요한 보조 콘텐츠를 담는다. 목차만 보내거나 본체만 보내서는 완성된 배포가 아니다. **사용자가 더블클릭할 시작 파일은 `FestaFiesta.exe` 하나이며, ZIP에 들어 있는 폴더 구조 전체가 게임이다.** 실행 파일만 따로 복사하지 않는다. 극장의 입구를 하나로 만들더라도 무대와 장비가 필요한 것처럼 `FestaFiesta`, `Engine`, `Licenses`와 문서를 함께 유지한다. 콘텐츠를 실행 파일 안에 합치는 자체 압축 해제 프로그램을 만드는 방식이 아니다.

### 최신 정상 빌드 경로를 찾기

기본 출력 위치로 빌드했을 때 다음 코드를 사용할 수 있다.

```powershell
$FFReleaseBase = Join-Path (Get-Location) 'Artifacts\Releases'
$FFLatest = Get-Content -LiteralPath (Join-Path $FFReleaseBase 'latest.json') -Raw | ConvertFrom-Json
$FFRelease = Join-Path $FFReleaseBase $FFLatest.releasePath
Get-Content -LiteralPath (Join-Path $FFRelease 'Metadata\build-manifest.json')
Get-Content -LiteralPath (Join-Path $FFRelease 'Metadata\verification.json')
Get-ChildItem -LiteralPath $FFRelease
```

다른 출력 경로를 썼다면 `$FFReleaseBase`를 바꾼다. 방금 빌드가 실패했다면 `latest.json`은 **이전 성공 빌드**를 가리킬 수 있으므로, 빌드 라벨과 생성 시간을 반드시 대조한다.

### 이미 만들어진 폴더를 다시 검사하기

```powershell
& '.\Tools\Build\Verify-Package.ps1' -PackagePath (Join-Path $FFRelease 'Package') -EngineRoot $FFEngine -SmokeTest -GraphicsSmokeTest -ReportDirectory (Join-Path $FFRelease 'Recheck')
```

`-PackagePath`에는 ZIP 파일이 아니라 **압축이 풀린 패키지 폴더**를 전달한다. 스크립트가 하위에서 실제 payload를 찾는다. `-ReportDirectory`를 생략하면 임시 폴더에 보고서를 남긴다. 추가 보고서는 공개 payload 안에 만들지 않는다.

| 검사 | 증명하는 범위 | 증명하지 못하는 범위 |
| --- | --- | --- |
| 파일·용량·디버그/에디터 파일 검사 | 필요한 구조 존재, 명백한 비정상 용량, 불필요한 개발 파일 유출 여부 | 실제 게임 정상 동작 |
| UnrealPak `-Test` | 기본 `.pak`에 대한 무결성 검사 | 모든 IoStore 데이터 블록·게임 로직 완전 검증 |
| IoStore 목록 검사 | 요구 맵·에셋·SM5/SM6 셰이더 라이브러리가 컨테이너 목록에 존재 | 모든 에셋의 시각 품질·모든 경로 참조 성공 |
| `-SmokeTest` | NullRHI로 초기 컨테이너 읽기와 프로세스 시작·정상 종료 | GPU 지원, 화면 렌더링, 맵 플레이 |
| `-GraphicsSmokeTest` | 엔진 초기화 후 확인한 실제 D3D11 + SM5 / D3D12 + SM6, 해당 실행의 nonce가 일치하는 새 마커, 종료 코드 0 | Frontend 화면 완성, Test_Main 맵 플레이, 재질·그림자·FPS |

`Smoke`는 전원을 켰을 때 연기가 나지 않는지 보는 최소 검사라는 비유에서 이해할 수 있다. **NullRHI 검사만** `-ForceQuitAfterEarlyReads`로 초기 데이터 읽기 후 종료한다. 실제 그래픽 검사에는 이 조기 종료 인자를 사용하지 않는다. 지원하지 않는 강제 RHI를 엔진이 거절해도 종료 코드가 0일 수 있으므로, **그래픽 검사는 종료 코드 0만으로 합격할 수 없다.**

R3부터 그래픽 검사는 다음 순서로 실제 초기화 증거를 만든다.

1. 검증 스크립트가 실행마다 새로운 32자리 16진수 nonce를 만든다. nonce는 이번 검사만 구분하는 일회용 확인 번호다. 파일 이름도 매번 바꾸므로 이전 실행의 합격 파일을 재사용하지 않는다.
2. Shipping 실행 파일에 `-FFGraphicsProbe=<보고서 절대 경로>`와 `-FFGraphicsProbeToken=<nonce>`를 전달하고, 각각 `-dx11 -sm5`, `-dx12 -sm6`으로 실행한다. `-RenderOffscreen`으로 화면 표시 없이 그래픽 장치를 초기화한다.
3. 게임 모듈의 `FCoreDelegates::OnPostEngineInit` 콜백이 엔진 초기화 완료 후 `GDynamicRHI->GetName()`과 `GMaxRHIFeatureLevel`을 읽는다. 실제 조합이 D3D11 + SM5 또는 D3D12 + SM6일 때 `token`, `rhi`, `shaderModel`, `initialized=true`를 JSON에 기록하고 정상 종료한다. 기록 실패나 잘못된 인자는 실패 종료 코드로 처리한다.
4. 검증 스크립트는 제한 시간 내 종료, 종료 코드 0, 새 마커 파일 존재, nonce 일치, `initialized=true`, **요청한 RHI와 SM의 정확한 일치**를 모두 요구한다. DX12 요청에 DX11이 켜진 결과도 합격으로 처리하지 않는다.

비유하면 “시험장이 닫혔다”는 사실만 확인하던 검사를 “이번 수험표 번호와 실제 응시 과목이 적힌 완료 확인서”까지 받는 방식으로 보완한 것이다. 일반 게임 실행은 위 probe 인자를 주지 않으므로 이 콜백을 등록하거나 검사 파일을 쓰지 않는다. 검사 모드도 실제 장면을 플레이하기 전에 끝나므로 10장의 수동 QA는 계속 필요하다.

이 방식은 중간 R3 후보에서 도입한 뒤 **최종 `FestaFiesta-Win64-SM5SM6-20260911` 패키지에서도 새 마커로 재검증해 통과했다.** 최종 근거는 `Artifacts/Releases/FestaFiesta-Win64-SM5SM6-20260911/Metadata/verification.json`과 같은 폴더의 두 `GraphicsStartup-*.json`이다. 실제 D3D11 + SM5, D3D12 + SM6가 기록되어 있으며 각각의 nonce와 종료 코드 0을 대조했다.

`Verify-Package.ps1`의 `-SmokeTimeoutSeconds` 기본값은 90초이며 15~600초 범위다. 각 초기화 검사에 적용된다. 처음 드라이버를 준비하는 PC에서 원인을 확인한 뒤 시간을 늘릴 수 있지만, 무한히 기다리거나 시간 제한을 없애는 대신 GPU·드라이버·오류 대화상자를 확인한다.

`verification.json`의 `shaderFormats`에는 `PCD3D_SM5`, `PCD3D_SM6` 두 값이 있어야 한다. `shaderLibraries`는 각 형식의 `Global`과 `FestaFiesta`, 총 네 라이브러리를 기록한다. `graphicsSmokeTested=true`일 때 `smokeResults`의 DX11·DX12 두 항목에서 `status=passed`, `actualRhi`, `actualShaderModel`, `initializationReport`를 함께 읽는다. 개별 `Smoke-*.json`과 그 항목이 가리키는 `GraphicsStartup-<경로>-<nonce>.json`을 같이 보관한다. 이는 초기화 검사 기록이며 Shipping 게임 전체 로그나 플레이 완료를 대신하지 않는다. `renderedGameplayTested=false`가 의미하는 범위를 임의로 넓혀 해석하지 않는다.

## 10. 실제 두 그래픽 경로 플레이 QA — 혼자 따라 하는 순서

제작 PC에서 성공한 ZIP을 **새 폴더에 완전히 압축 해제**한다. 가능하면 Unreal과 Visual Studio가 없는 별도 Windows PC에서도 같은 검사를 한다. 같은 GPU에서 두 경로를 실행한 검사는 경로 검증이고, 오래된 SM5 GPU 실기기 검증과는 범위가 다르다.

1. 압축 해제한 payload의 `Engine\Extras\Redist\en-us\vc_redist.x64.exe`를 실행한다. 필요한 경우 설치 또는 복구를 완료한다.
2. 기존 실행 중인 게임이 없는지 확인한다.
3. 맨 위 `FestaFiesta.exe`를 더블클릭한다. 타이틀이 표시되고 마우스와 키보드에 반응하는지 확인한다.
4. Frontend에서 실제 게임 시작 경로를 따라 `Test_Main`으로 진입한다. 맵을 직접 강제 실행해서 정상 진입 동선을 건너뛰지 않는다.
5. 아래 표의 순서대로 플레이한다. 게임을 정상 종료한다.
6. 새 사용자 상태와 기존 저장 데이터가 있는 상태를 각각 확인한다. 테스트 전 세이브는 별도 백업하고, 문제를 해결한다고 사용자의 `Saved` 폴더 전체를 삭제하지 않는다.

### 담당자용 고급 QA: 같은 EXE에 그래픽 옵션 지정

일반 사용자 안내는 위의 더블클릭 실행으로 유지한다. 두 경로를 따로 검증하는 담당자는 게임을 종료한 뒤, 압축을 푼 맨 위 폴더에서 PowerShell을 열어 아래 명령을 한 번에 하나씩 실행한다.

```powershell
.\FestaFiesta.exe -dx11 -sm5

# 첫 게임을 정상 종료한 뒤 실행
.\FestaFiesta.exe -dx12 -sm6
```

각 실행에서 위 4~6단계를 반복하고 같은 장면의 캡처와 성능을 비교한다. 강제 옵션은 해당 그래픽 경로를 요구하므로, 일반 실행의 자동 지원 판정과 구분해 기록한다. 한 경로를 실행한 결과를 다른 경로의 통과로 대신하지 않는다. Development 로그는 게임 폴더의 `FestaFiesta/Saved/Logs`에서 수집하고, 검사별로 폴더 밖에 복사해 구분한다. Shipping 로그의 제한은 11장을 따른다.

| 순서 | 실제 할 일 | 합격 기준 / 특히 확인할 것 |
| --- | --- | --- |
| 1 | 타이틀 → 시작/옵션/저장 목록을 열었다 닫기 | 모든 버튼 동작, 한글 정상, 잘림·입력 포커스 문제 없음 |
| 2 | 해상도·전체 화면·음량 변경 후 적용과 복귀 | 화면이 사라지지 않고 설정 반영·재실행 유지 |
| 3 | 실제 플레이 맵 진입 | 로딩이 끝나고 UI와 월드가 함께 표시됨 |
| 4 | 카메라 이동·줌, 시설 배치·회전·선택·철거 | 지형·충돌·배치 표시·정보창 정상 |
| 5 | 필수 시설 준비, 홍보 확인창, 개장과 시간 진행 | 데이터 테이블 누락 없이 단계 전환, 막힘 없음 |
| 6 | 관람객·대기열·혼잡 상태를 늘려 관찰 | 경로 탐색 정상, 프레임 저하·정지 여부 기록 |
| 8 | 낮·노을·밤, 물·유리·금속·풀·피부·불꽃 비교 | 검은 머티리얼·체커보드·누락 메시·그림자 이상 없음 |
| 9 | 저장 → 종료 → 다시 실행 → 불러오기 | 데이터 손실 없이 주요 진행 상태 복원 |
| 10 | 종료·재실행, 창 전환, 오디오 장치 확인 | 멈춤·잔류 프로세스·소리 문제 없음 |
| 11 | 정해진 시간 동안 동일 시나리오 플레이 | 크래시·지속적인 메모리 증가·허용 불가 끊김 없음 |

SM5에서 고급 조명이 줄어드는 것은 예상할 수 있지만, 중요한 시설이 안 보이거나 밤에 조작을 할 수 없으면 배포 가능한 호환 경로가 아니다. 에셋별 대체 메시·머티리얼·조명 보완이 필요하다. “실행됨”과 “게임을 정상적으로 할 수 있음”을 구분해 판정한다.

### QA 기록 양식

| 기록 항목 | 작성 예시 / 의미 |
| --- | --- |
| 빌드 | 실제 buildLabel, Git commit, ZIP SHA-256 |
| PC | CPU / RAM / GPU 정확한 모델 / VRAM |
| 시스템 | Windows 버전과 빌드 / GPU 드라이버 버전 |
| 실행 경로 | 일반 EXE 더블클릭 또는 고급 QA에서 지정한 DX11 SM5 / DX12 SM6 옵션 |
| 조건 | 해상도 / 품질 설정 / 군중 수 / 저장 상태 |
| 첫 실행 | 타이틀까지 시간 / 첫 맵까지 시간 / 첫 방문 끊김 |
| 반복 실행 | 같은 조건의 로딩·프레임 시간 |
| 기능 | 위 체크 항목별 통과·실패·미검증 |
| 화면 | 문제 장면 캡처와 정확한 재현 동선 |
| 결론 | 승인 / 수정 후 재검사 / 특정 GPU 미지원 |

최소 사양은 이 표로 확인한 결과에서 정한다. `dxdiag`의 시스템 탭에 “DirectX 12”라고 보이는 것만으로 GPU가 모든 SM6 기능을 지원한다고 결론 내릴 수 없다. 운영체제의 API 설치 상태와 실제 GPU·드라이버 기능은 서로 다르다.

## 11. 실패했을 때 어디부터 확인하나

실패 로그 마지막의 `Unknown Error`, `AutomationTool exiting with ExitCode`는 최종 실패 요약인 경우가 많다. 같은 로그에서 **처음 발생한 구체적인 오류와 바로 앞 문맥**을 찾는다. 경고를 모두 실패 원인으로 취급하지 않고, 누락 파일 이름·에셋 경로·컴파일 파일·도구 버전을 기록한다.

빌드 도중 실패하면 보통 `Artifacts\Releases\.staging\<빌드 라벨>\Metadata\failure.json`에 단계와 관련 로그 위치가 남는다. 시작 전 사전 검사에서 실패한 경우에는 결과 폴더가 아직 없어 콘솔 메시지가 첫 단서다. 로그 검색 예시는 다음과 같다. `<실패 빌드 라벨>`을 실제 폴더 이름으로 바꾼다.

```powershell
$FFFailed = '.\Artifacts\Releases\.staging\<실패 빌드 라벨>'
Get-Content -LiteralPath (Join-Path $FFFailed 'Metadata\failure.json')
Get-Content -LiteralPath (Join-Path $FFFailed 'Logs\BuildCookRun.log') -Tail 100
Select-String -LiteralPath (Join-Path $FFFailed 'Logs\BuildCookRun.log') -Pattern 'Error:|Fatal error|error C[0-9]+|error LNK|Missing|Shader' -Context 3,3
```

| 증상 / 메시지 | 의미 | 해결 순서 |
| --- | --- | --- |
| `Engine build does not match` | 엔진 패치 또는 CL 불일치 | `$FFEngine`와 Build.version 확인 → 5.7.4 CL 51494982 설치 선택 |
| `Locked Windows SDK ... not installed` | 고정 SDK 없음 | VS Installer에서 22621 SDK 추가 → 경로 확인 |
| `Locked MSVC ... not installed` | 고정 설치 폴더 없음 | 14.44 계열과 설치 위치 확인 → 실제 파일 버전·빌드 로그 대조 |
| `Git LFS is too old` | LFS가 최소 버전 미만 | Git LFS를 3.5.1 이상으로 업데이트 |
| `LFS objects are not materialized` | 에셋이 안내장 상태 | 인증·다운로드 문제 해결 → `git lfs pull` → `git lfs fsck` |
| `Development Editor build failed` | 검증용 에디터 모듈 컴파일 실패 | `Validation/EditorBuild.log`의 첫 C++·링크 오류 해결 |
| `Automation validation failed` | 로직·에셋 계약 테스트 실패 | 해당 필터의 Automation.log와 Report 확인 → 원인 수정 |
| `Project data validation failed` | 디스크 에셋의 참조·규칙 오류 | DataValidation.log에 나온 실제 에셋을 에디터에서 수정·저장 |
| `Unable to find package`, required asset missing | 쿠킹 누락·잘못된 참조 | 경로 오타·대소문자·리디렉터·Cook 규칙과 NeverCook 충돌 확인 |
| 셰이더 컴파일 오류 / shader archive missing | 특정 형식의 머티리얼·셰이더 누락 | 최초 오류의 플랫폼과 머티리얼 확인 → 두 TargetedShaderFormats → 재쿠킹 |
| DLL 또는 `.exe` 파일 사용 중 | 게임·에디터·Live Coding이 파일 보유 | 관련 프로세스 정상 종료 → 새 라벨로 재실행 |
| 출력 이미 존재 | 기존 결과 보호 장치 | 새 라벨 사용; 이전 로그는 원인 분석용으로 보관 |
| `Output path is too deep for Windows MAX_PATH` | 출력 경로가 너무 깊어 ZIP 단계에서 실패할 상태 | 더 짧은 `-BuildLabel` 또는 `-OutputRoot` 사용 |
| `Close the editor/game using this project` | 에디터·게임이 프로젝트 DLL을 잡고 있어 Clean 빌드가 실패할 상태 | 에디터, Rider에서 띄운 에디터, 실행 중인 패키지 게임을 종료 후 재실행 |
| `UBT selected a toolchain different from toolchain.lock.json` | VS 업데이트 등으로 실제 컴파일러가 잠금 버전과 다름. 에디터 빌드 직후 검사하므로 긴 빌드 전에 멈춤 | 로그의 실제 버전을 확인하고, 의도한 업데이트라면 잠금 파일을 갱신해 새 기준으로 전체 검증 |
| `The cook dropped ... referenced package(s)` | 쿠킹된 에셋이 NeverCook 등으로 빠진 에셋을 하드 참조 | 로그의 `Content is missing from cook` 줄에서 Source·Target 패키지를 확인하고, 참조를 고치거나 NeverCook 규칙을 좁힘 |
| 디스크 부족 | 스테이징·ZIP·캐시 동시 사용량 초과 | 완료된 오래된 산출물 보관 정책 적용 또는 출력 드라이브 변경 |
| prerequisite / VCRUNTIME 관련 오류 | 플레이 PC 실행 런타임 미설치·손상 | 동봉 x64 재배포 설치기의 설치·복구 후 재실행 |
| 지원되는 GPU / SM6 필요 오류 | 강제 DX12 SM6 경로 또는 드라이버·OS 조건 불충족 | 실제 GPU 확인 → 제조사 드라이버 확인 → 일반 EXE 실행과 10장의 고급 DX11 검사 결과 비교 |
| DX11 화면만 검거나 누락 | 대체 메시·조명·Substrate 호환 문제 가능 | 같은 장면 비교 → 문제 에셋과 기능 단위로 원인 분리 |
| 초기화 검사 시간 초과 | 초기 셰이더·드라이버 지연 또는 멈춤 | 팝업·프로세스·GPU 상태 확인 → 필요할 때 검증 타임아웃 증가 |
| 에디터는 정상, 패키지만 UI 실패 | 문자열 참조·쿠킹 누락 가능 | IoStore 목록과 AlwaysCook/AssetManager 규칙 → 실제 클릭 흐름 확인 |

자동화 테스트만 빠르게 재현할 때는 다음처럼 실행한다. `-TestFilter`는 PowerShell 문자열 배열이며, 기본값은 `FestaFiesta`, `FF_ZoneSystem` 두 범위다. 현재 렌더링 테스트를 포함한 전체 범위를 실행하려면 GPU·드라이버가 필요하다.

```powershell
& '.\Tools\Build\Run-AutomationTests.ps1' -EngineRoot $FFEngine -TestFilter @('FestaFiesta', 'FF_ZoneSystem') -OutputRoot '.\Artifacts\Validation\Manual-Recheck-01'
```

`Run-AutomationTests.ps1`에는 `-SkipDataValidation`도 있으나 개별 로직 원인을 분리하는 진단용이다. 이 옵션으로 통과한 부분 결과를 전체 릴리스 검증 통과로 바꾸어 적지 않는다.

### Shipping에서 로그 파일이 없을 때

현재 일반 Shipping 실행은 개발 빌드와 같은 로그·콘솔 기능을 모두 제공하지 않는다. `-log`, `-ExecCmds`를 붙이기만 하면 상세 로그가 반드시 생기는 것이 아니다. `%LOCALAPPDATA%\FestaFiesta\Saved\Logs`에 로그가 **실제로 있는 경우** 수집하고, 없으면 build-manifest, OS·GPU·드라이버 정보, 재현 동선, 오류 화면, Windows 응용 프로그램 오류 기록을 먼저 수집한다. 필요하면 같은 소스의 Development 진단 빌드와 내부 PDB로 원인을 좁힌다. 이 프로젝트는 Crash Reporter를 기본 포함하지 않으므로 자동 업로드를 기대하지 않는다.

## 12. 배포할 파일과 팀 내부에 보관할 파일

**플레이어에게 전달할 것:** 최종 `<빌드 라벨>.zip`, ZIP용 `ARCHIVE-SHA256.txt`, 실행 환경·변경점·알려진 문제를 정리한 릴리스 안내. ZIP 안에는 루트 `FestaFiesta.exe`, 게임 파일 전체, x64 prerequisite, 한국어 안내, 라이선스 고지, build-manifest가 포함된다. 사용자가 폴더별 파일 무결성도 확인해야 한다면 `PACKAGE-SHA256SUMS.txt`를 별도로 함께 제공한다.

**팀이 해당 빌드와 함께 보관할 것:** `Symbols`, `Metadata`, `Logs`, `Validation`, 실제 사용한 소스 커밋과 변경 차이, QA 기록. PDB는 실행 파일의 주소를 소스 함수와 줄로 해석하는 지도다. 다른 버전의 지도는 맞지 않을 수 있으므로 빌드별로 짝을 유지한다. 원본 프로젝트 전체, Git 이력, 개발 캐시를 플레이어 ZIP에 넣지 않는다.

### ZIP 해시 확인

SHA-256은 파일 내용을 요약한 지문이다. 배포처가 제공한 지문과 다운로드한 파일의 지문이 같으면 전달 과정에서 같은 바이트를 받았는지 확인할 수 있다. 해시는 코드 서명이나 악성 코드 검사와 다른 기능이다.

```powershell
Get-FileHash -LiteralPath 'C:\Downloads\<실제 빌드 라벨>.zip' -Algorithm SHA256
Get-Content -LiteralPath 'C:\Downloads\ARCHIVE-SHA256.txt'
```

두 해시의 영문 대소문자는 달라도 같은 값으로 비교한다. 다르면 압축을 풀어 실행하기 전에 올바른 빌드인지 확인하고 다시 다운로드한다. ZIP을 수정하거나 재압축했으면 기존 해시와 달라진다. 서명을 도입하는 팀은 실행 파일 서명과 검증을 **최종 ZIP·해시 계산 전** 단계로 넣어야 한다. 현재 스크립트는 Authenticode 상태를 기록하며 자동으로 회사 인증서를 발급하거나 서명하지 않는다.

### 플레이어에게 안내할 실행 순서

1. ZIP 전체를 일반 폴더에 압축 해제한다. ZIP 탐색 창 안에서 바로 실행하지 않는다.
2. 동봉 `vc_redist.x64.exe` 설치를 완료한다.
3. 맨 위 폴더의 `FestaFiesta.exe` 하나를 더블클릭한다. 그래픽 경로는 시작할 때 지원 여부를 확인해 선택한다.
4. `FestaFiesta`, `Engine`, `Licenses`와 동봉 문서를 같은 위치에 유지한다. 파일을 골라 따로 옮기지 않는다.
5. 오류가 있으면 build-manifest의 빌드 이름과 PC 정보, 일반 더블클릭 실행 여부, 재현 순서를 함께 보고한다.

라이선스 고지를 삭제하여 몇 KB를 줄이는 식의 최적화는 하지 않는다. 외부 에셋·플러그인의 실제 배포 가능 범위와 팀의 권한은 해당 자산의 조건을 확인해 관리한다.

## 13. 다음 버전을 만들 때의 일상 절차

1. 이전 승인 빌드, QA 기록, 알려진 문제를 읽는다.
2. 팀이 정한 커밋을 받아 변경을 확인하고 LFS 에셋을 내려받는다. 로컬 미저장 작업을 먼저 정리하고 자동 덮어쓰기나 강제 초기화를 피한다.
3. `ProjectVersion` 변경이 필요한 릴리스라면 `Config/DefaultGame.ini`에서 버전을 갱신한다. 버전 숫자와 매번 생성하는 buildLabel은 다른 정보다.
4. 새로운 맵·문자열 로딩 에셋·플러그인이 생겼다면 Cook 포함·제외 규칙, 필수 에셋 검증 목록, 라이선스 문서를 함께 갱신한다.
5. 일상 확인은 `-Incremental -SkipZip`으로 시간을 줄일 수 있다. 배포 후보는 표준 명령으로 새 라벨을 만들어 전체 검증한다.
6. 같은 조건에서 크기, 빌드 시간, 로딩 시간, 프레임 시간, 테스트 결과를 이전 승인 빌드와 비교한다.
7. 양쪽 RHI 실제 게임 QA와 기존 저장 데이터 업그레이드 확인을 마친다.
8. 변경점과 알려진 문제를 작성하고 최종 ZIP·해시를 함께 배포한다. 이전 승인 빌드도 되돌림을 위해 보관한다.

현재 `bGenerateChunks=False`이고 차등 패치 자동화는 구성되어 있지 않다. 작은 수정이어도 현재 방식은 전체 새 ZIP을 만드는 방식이다. 실행 파일이나 `.ucas` 일부만 임의로 덮어쓰는 업데이트를 안내하지 않는다. 차등 배포가 필요해지면 플랫폼 업데이트 체계, 청크 경계, 에셋 의존성, 이전 버전 기준 데이터, 저장 데이터 호환성을 함께 설계한다.

### 캐시는 언제 지우나

DDC, Derived Data Cache는 원본을 플랫폼용으로 가공한 결과를 재사용하는 저장소다. 같은 재료를 매번 처음부터 손질하지 않도록 해 주는 냉장고라고 생각하면 된다. 원본 소스는 아니므로 재생성 가능하지만, 무조건 지우면 다음 빌드가 오래 걸린다. 에셋 버전·엔진·설정 변화나 손상 증거가 있을 때 영향 범위를 좁혀 정리하고, 팀 공유 캐시는 서버 정책에 따라 관리한다. [Epic: DDC 사용](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-derived-data-cache-in-unreal-engine?application_version=5.7)

| 폴더 / 데이터 | 성격 | 관리 원칙 |
| --- | --- | --- |
| `Content`, `Config`, `Source`, 프로젝트 플러그인 | 프로젝트 원본 | 빌드 오류 해결 목적으로 삭제하지 않음 |
| `Intermediate` | 빌드 중간 파일 | 관련 도구 종료 후 재생성 비용을 이해하고 필요한 범위만 정리 |
| DDC / Zen 캐시 | 가공 데이터 재사용 | 전체 삭제를 첫 해결책으로 쓰지 않음 |
| `Saved` | 로그·에디터 상태·자동저장·기타 작업 데이터 | 통째로 지우기 전 내용과 필요한 백업을 구분 |
| `Artifacts` | 빌드 결과·테스트·진단 증거 | 승인 릴리스·심볼 보관 정책을 먼저 정하고 오래된 결과 정리 |
| 사용자 `%LOCALAPPDATA%\FestaFiesta\Saved` | 사용자 설정·세이브 등 | 세이브 백업 없이 초기화하지 않음 |

### 셰이더가 패키지에 있는데 첫 실행이 끊기는 이유

셰이더 코드를 준비하는 일과 특정 GPU가 그릴 상태 조합인 PSO를 준비하는 일은 다르다. 요리법을 갖고 있어도 첫 주문에 맞춰 조리대를 준비하는 시간이 필요할 수 있는 것과 비슷하다. PSO precaching은 필요한 상태를 미리 모으고 비동기로 준비하여 갑작스러운 끊김을 줄이는 기능이다. DDC는 제작 측의 데이터 재사용이고, PSO/드라이버 캐시는 실제 실행 환경의 그래픽 준비와 관련되어 있다. 서로 대체하지 않는다.

개선은 먼저 첫 실행과 두 번째 실행을 같은 장면에서 비교하고, PSO 준비가 실제 원인인지 측정한 뒤 시작한다. SM5·SM6 및 GPU·드라이버별 결과를 구분한다. 기본 precaching으로 남는 문제가 입증되면 필요한 경로를 수집한 bundled PSO cache를 검토한다. 이 가이드를 추가했다는 이유만으로 PSO 수집·배포가 이미 완료된 것은 아니다. [Epic: PSO Precaching](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine?application_version=5.7)

## 14. 출고 전 마지막 체크리스트

- [ ] 도구 버전과 실제 빌드 로그의 선택 버전을 대조했다.
- [ ] 소스 커밋, dirty 여부, 프로젝트 버전, 빌드 라벨을 기록했다.
- [ ] LFS 에셋과 자동화 테스트, 데이터 검증이 통과했다.
- [ ] Shipping 패키징이 완료됐고 실패 스테이징 폴더와 구분했다.
- [ ] DX11 SM5와 DX12 SM6의 글로벌·프로젝트 셰이더가 포함됐다.
- [ ] NullRHI 초기 실행과 GPU가 있는 PC의 두 RHI 초기화 결과를 기록했다.
- [ ] 두 경로에서 Frontend부터 실제 게임·저장·종료까지 검사했다.
- [ ] 낮·밤·물·투명·그림자·Nanite 대체 메시를 육안으로 확인했다.
- [ ] 별도 플레이 PC와 기존 저장 데이터 조건에서 실행을 확인했다.
- [ ] ZIP 전체, x64 prerequisite, 실행 안내, 라이선스 고지를 확인했다.
- [ ] 파일과 ZIP 해시가 최종 내용에 맞고 PDB·로그·QA 기록을 보관했다.
- [ ] `QUALITY-OVERRIDE.txt`가 있는 진단본을 공개 배포본으로 선택하지 않았다.
- [ ] 실제 확인한 GPU·드라이버와 남은 문제를 릴리스 안내에 적었다.

## 15. 혼자 다시 수행할 때 보는 명령 카드

첫 실행은 3~4장의 설치·점검을 끝낸 상태를 전제로 한다. 아래 명령은 기본 경로 설치와 기본 출력 위치를 사용한다.

```powershell
# 1. 프로젝트와 엔진 위치
Set-Location 'C:\GitHub\FestaTeam\8th-Team13-Final-Project'
$FFEngine = 'C:\Program Files\Epic Games\UE_5.7'

# 2. 변경 상태와 대용량 파일 확인
git status --short
git lfs version
git lfs pull
git lfs fsck

# 3. GPU PC의 전체 패키징
& '.\Tools\Build\Package-Win64.ps1' -EngineRoot $FFEngine -GraphicsSmokeTest

# 4. 최신 성공 결과 확인; 방금 실행한 라벨인지 반드시 대조
$FFReleaseBase = Join-Path (Get-Location) 'Artifacts\Releases'
$FFLatest = Get-Content -LiteralPath (Join-Path $FFReleaseBase 'latest.json') -Raw | ConvertFrom-Json
$FFRelease = Join-Path $FFReleaseBase $FFLatest.releasePath
Get-Content -LiteralPath (Join-Path $FFRelease 'Metadata\verification.json')
Get-Content -LiteralPath (Join-Path $FFRelease 'Metadata\ARCHIVE-SHA256.txt')
Get-ChildItem -LiteralPath $FFRelease
```

이후 ZIP을 별도 폴더에 풀고 맨 위 `FestaFiesta.exe`를 더블클릭하여 사용자의 첫 실행을 확인한다. 담당자는 10장의 고급 명령으로 DX12와 DX11 수동 QA도 각각 수행한다. 검사 기록과 최종 해시를 남긴 다음 사용자에게 ZIP 전체를 전달한다.

## 근거가 되는 프로젝트 파일

- `Tools/Build/Package-Win64.ps1`: 사전 조건·자동 검증·UAT·심볼·해시·ZIP·결과 승격.
- `Tools/Build/Verify-Package.ps1`: 배포 구조·IoStore 에셋과 셰이더·초기 실행 검사.
- `Source/FestaFiesta/FestaFiesta.cpp`: 명시적 검사 인자가 있을 때만 엔진 초기화 후 실제 RHI·SM과 nonce 마커를 기록.
- `Tools/Build/Run-AutomationTests.ps1`: Development Editor 빌드·자동화 테스트·데이터 검증.
- `Tools/Build/FestaFiesta.Build.psm1`: 엔진 찾기·LFS·Git 메타데이터·해시 도우미.
- `Tools/Build/toolchain.lock.json`: 엔진·MSVC·SDK·LFS 기준 버전.
- `Config/DefaultEngine.ini`: RHI·셰이더·렌더링·기본 맵.
- `Config/DefaultGame.ini`: 압축·문화권·맵·AlwaysCook/NeverCook·Asset Manager.
- `Distribution/README_KO.md`, `Distribution/DEVELOPMENT_TEST_GUIDE_KO.md`, `THIRD_PARTY_NOTICES.md`: 단일 EXE 시작 안내, Development 진단과 배포 고지.

문서의 기본 절차가 스크립트와 달라졌다면 복사한 옛 명령을 계속 쓰지 말고, 변경된 옵션·설정과 그 빌드의 `build-manifest.json`을 기준으로 함께 갱신한다.

## 부록: 최초 실패 12건의 실제 원인과 해결 근거

아래는 최초 실패를 지우지 않고, 무엇이 잘못됐고 어떤 계약을 보존하면서 고쳤는지 추적하는 기록이다. **전체 수정 후 표준 실행의 자동화 760 / 760건이 성공했고, 아래 최초 실패 12건도 모두 통과했다. 데이터 검증은 오류 0건·경고 1건이다.** Shipping 패키징·최종 그래픽 초기화 검사·ZIP 생성도 완료했으며 결과와 해시는 0장에 기록했다. 실제 화면·조작·저장·성능의 수동 플레이 QA는 별도다.

최초 증거는 `Artifacts/Validation/SM5SM6-20260911/FestaFiesta/Report/index.json`과 같은 경로의 `FF_ZoneSystem/Report/index.json`이다. 당시 두 묶음은 합계 758건 중 746건 성공·12건 실패였다. 첫 복구 후 `Artifacts/Validation/SM5SM6-20260911-Repaired`에서는 신규 레인 회귀 테스트가 추가되어 합계 759건 중 748건 성공·11건 실패였다. `InTransitCountsAccumulateWithinBatch`가 애니메이션 참조 복구로 통과한 시점의 **중간 기록**이며, 이후의 비용·대기영역·내비게이션·조명 수정 결과를 나타내지 않는다.

수정 전 동일 조건의 기준선이 없으므로 “기존부터 실패하던 문제”라고 단정하지 않는다. 모두 이번 검사에서 발견한 문제다. 조사 결과에는 실제 콘텐츠 참조·대기 공간·구워진 경로 데이터의 결함과, 현재 제품 설계를 따르지 않는 오래된 테스트 기대값이 함께 있었다. 실패 12건이 서로 독립적인 게임 버그 12개라는 뜻도 아니다. 같은 오래된 비용 키가 여러 테스트의 설치 단계에 영향을 주었다.

아래 앞의 10개는 `FestaFiesta.` 접두사를 생략했다. 마지막 2개는 전체 이름을 표시한다. 표의 마지막 열은 확인한 수정 근거이며, 최종 전체 검사의 결과는 0장의 실행 결과표에서 통합해 판정한다.

| 최초 실패 테스트 | 실제 원인과 적용한 해결 | 검증 근거 / 최종 확인 |
| --- | --- | --- |
| `Building.UI.CurrentGoldTracksProgressionLifecycle` | 사라진 `FoodStall_DadaPochaFoodTruck` 경로를 현재 카탈로그의 `PDA_FFZS_Booth_FoodStall_Taco`로 교체. 실제 진행 시스템에 따른 보유 Gold 표시 계약은 유지 | `Source/FestaFiesta/Private/Building/UI/FF_BuildCatalogGoldTests.cpp` |
| `Content.FacilityDefinitions.AllDefinitionsAreProjectable` | 도로·조명에 고객 서비스 앵커를 요구하던 검사와 인접 예약 사각형의 합집합 경계 오판을 수정. 실제 신체 여유·진입 동선 노출은 34개 정의의 예약 영역을 최소 보완하는 별도 에셋 수정으로 처리 | `Source/FestaFiesta/Private/Building/FF_FacilityDefinitionContentTests.cpp`, 대기영역 복구 도구·보고서. 34개 저장, 새 프로세스에서 46개 정의 확인·남은 수정 0개 |
| `Crowd.Avoidance.CounterFlowPathOpens` | 마지막 순간 막힌 인원이 양쪽 모두 0인데 `0 < 0`을 요구하던 오류. 거의 전원 통과·평균 전진량 개선·막힘 비증가를 함께 검사하도록 수정 | `Source/FestaFiesta/Private/CrowdSystem/Systems/FF_CrowdSystemTests.cpp`; 진단에서 기본 회피 80/80명, 이전 방식 67/80명 통과 |
| `Crowd.Decision.InTransitCountsAccumulateWithinBatch` | `Breathing_Idle`의 비어 있는 Skeleton 참조로 로딩 오류가 테스트에 전파됨. 검증한 기존 캐릭터 Skeleton을 연결 | `Artifacts/AnimationSkeletonRepair` 보고서·원본 백업, 중간 `SM5SM6-20260911-Repaired`에서 해당 테스트 통과 |
| `Economy.ZoneSystemBridge.ContentContract.DefaultFestivalCostsFoodTruck` | 현재 Taco 정의와 정규 비용 행을 사용하도록 테스트 수정. 숨겨진 레거시 `Booth_FoodTruck_Taco`는 ID·경로를 보존하고 공식 `InstallCostKey` 재정의로 `Cost.Booth.FoodStall.Taco`에 연결 | `Source/FestaFiesta/Private/Economy/Tests/FF_ZoneSystemEconomyBridgeSubsystemTests.cpp`; 레거시 저장·새 프로세스 검증 종료 코드 0. 명시적 비용 재정의도 실제 행·가격까지 검사하도록 범위를 강화. 아래 비용 복구 근거 참조 |
| `Probe.NavMeshBakeStatus` | 끊긴 Blueprint 상호작용 노드 참조를 복구한 후, 지형·충돌·내비게이션 에이전트 설정 변경 없이 `Test_Main`의 NavMesh 재생성. 실제 게임의 기준점·출구·도착 조건으로 검사하고 부분 경로는 합격시키지 않음 | `Artifacts/NavigationRepair/20260911-104159/report.json`: 출구 연결 샘플을 가진 존 22→31. 최종 표준 실행의 `Validation/FestaFiesta/Report/index.json`: 31개 존·런타임 출구 목표 3개·방향별 존 간 경로 930개·실제 도착 반경 400cm 검사 통과 |
| `ZoneIntegration.Queue.FacadeLifecycleE2E` | 없는 `Cost.Booth.FoodStall.DadaPochaFoodTruck` 대신 실제 `Cost.Booth.FoodStall.Taco`를 테스트 시설에 연결. 설치부터 대기·서비스까지의 검사는 유지 | `Source/FestaFiesta/Private/Zone/FF_ZoneSystemIntegrationTests.cpp` |
| `ZoneIntegration.Queue.MoveRefreshesProjection` | 같은 오래된 비용 키로 이동 전에 설치가 실패하던 테스트 준비를 수정. 이동 후 투영 갱신 기대는 유지 | 위 통합 테스트 파일 |
| `ZoneIntegration.Queue.ProgramFacilityWaitingAssets` | 사라진 DanceBattle 경로를 현재 `PDA_FFZS_Performance_ExperienceStage_Experience` 정의로 교체. 실제 프로그램 시설의 대기 수용 조건 검사는 유지 | 위 통합 테스트 파일 |
| `ZoneIntegration.Stage.FacilityLifecycleBatch` | 같은 비용 키를 현재 Taco 행으로 수정. 여러 시설의 단계별 설치·철거 계약은 유지 | `Source/FestaFiesta/Private/Zone/FF_ZoneFacilityLifecycleTests.cpp` |
| `FF_ZoneSystem.Lighting.ProductionMainStageInstallation` | 아티스트가 `67483cfbf81bcc2b2c5ea3929234112ede099826`에서 런타임 조명 하나를 의도적으로 제거했는데 테스트는 이전 10개를 기대. 최신 9개의 정확한 이름과 밝기·켜기/끄기·복원 동작을 검사하도록 수정 | `Plugins/FF_ZoneSystem/Source/FF_ZoneSystemTests/Private/FF_FFZSProductionLightingTests.cpp`; `Lighting-authored-audit.json`에서 원본 미리보기 10개·런타임 9개, 에셋 SHA 불변. 조명을 억지로 추가하지 않음 |
| `FF_ZoneSystem.Unit.Definitions.PassiveLightingHasNoServiceGeometry` | 도로·조명의 공통 규칙으로 바뀐 오류를 옛 한국어 표현으로 찾던 테스트. `FFZSTypes / NonServiceFacilityHasServiceData`라는 번역 식별자로 정확한 오류를 검사 | `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/Tests/FF_FFZSFacilityDefinitionTests.cpp`; 서비스 정원·앵커·대기 공간 거부 및 서비스 시설의 필수 조건 검사는 유지 |

원인 수정 후에는 관련 테스트를 먼저 재현하고, 전체 자동화·데이터 검증·표준 패키징·양쪽 RHI 플레이 QA를 다시 수행한다. 테스트 기대값을 고칠 때도 실제 제품 요구와 달라졌다는 근거를 남긴다. 실패를 숨기기 위해 기대값만 무조건 완화하지 않는다. 자동화 보고서의 경고는 실패와 별도로 수와 내용을 기록한다.

### 비용·시설 정의: 이름을 바꿀 때 데이터 계약도 함께 확인하기

현재 비용 규칙은 `Cost.<대분류>.<소분류>.<시설명>`처럼 네 부분으로 된 정규 행을 사용한다. 현행 Taco의 `Cost.Booth.FoodStall.Taco`는 실제 비용표에서 설치비 300 Gold다. 특정 테스트에서 사용하는 고정 금액 700은 격리된 테스트용 표의 값이며 현재 게임 가격으로 쓰지 않는다. 생산 데이터 검사는 테스트에 적힌 옛 가격이 아니라 실제 정규 행과 해석 결과가 같은지를 확인한다.

숨겨진 레거시 정의를 이름이 비슷하다는 이유로 삭제하거나 현재 카탈로그에 노출하면 저장·참조 호환성이 달라질 수 있다. 이번에는 기존 ID·숨김 상태·메시를 확인하고, 지원되는 `InstallCostKey`만 명시했다. 비용 규칙 자체를 느슨하게 바꾸지 않았다. 모든 생산용 정의에 대한 비용 행 검사를 유지하고, 명시적 재정의가 있는 정의도 건너뛰지 않고 정확한 행 존재·실제 해석된 키·가격을 검사한다.

비용만 바꾼 단계의 적용 근거는 `Artifacts/Validation/LegacyFacilityCost-20260911T104309377562Z/report.json`과 같은 폴더의 `Before` 백업이다. 별도 프로세스의 `LegacyFacilityCost-20260911T104349240061Z/report.json`은 저장된 키가 이미 맞고 추가 변경 0건임을 확인했다. 이후 같은 자산의 대기 예약 영역도 별도 절차로 수정할 수 있으므로, 이 해시는 **비용 복구 단계의 증거**로 보관하고 최종 에셋 해시는 최종 빌드 기록에서 확인한다.

### 대기 공간: 사람의 중심점과 몸 전체를 구분하기

대기열의 선은 줄 서는 사람의 중심 경로이고 `QueueReservedRects`는 다른 시설이 침범하지 않도록 확보한 바닥 공간이다. 바닥에 발을 놓을 점 하나가 들어간다고 어깨와 이동 동선까지 안전한 것은 아니다. 반대로 바닥을 두 사각형으로 나누어 예약했어도 둘의 합집합이 몸을 덮으면 유효해야 한다.

최초 시설 투영 오류 86건은 세 종류로 분리했다. 도로 6개·조명 6개에 고객 서비스 정보를 요구한 24건은 비서비스 시설의 계약에 맞게 검사했다. 인접 사각형의 경계를 틈으로 오인한 2건은 합집합 전체의 덮임을 계산하도록 고쳤으며, 실제 1cm 틈은 거부하는 음성 테스트를 추가했다. 남은 실제 노출 60건은 34개 정의를 대상으로 진입점에서 대기열 선두까지 35cm 신체 여유가 있는 통로를 추가하고 좁은 예약 영역을 필요한 만큼만 넓혀 처리했다. 시설 ID·스플라인·대기 수용 인원·비용은 보존했다.

적용 보고서 `Artifacts/FacilityQueueReservationRepair/20260911-105153-521437/report.json`은 34개 저장·남은 수정 0개를 기록한다. 새 Unreal 프로세스의 `20260911-105221-669076/report.json`은 46개 정의를 다시 읽어 추가 변경 0개·남은 수정 0개를 확인했고, 두 프로세스는 종료 코드 0이었다. 예약 사각형 외의 원래 Spec 값도 보존했는지 검사했다. 이후 최종 표준 실행의 `AllDefinitionsAreProjectable`을 포함한 전체 자동화도 통과했다. 진단·계획은 `Artifacts/Validation/AllFailures-20260911/facility_geometry_failures.json`과 `facility_queue_repair_plan.json`, 적용 도구는 `Tools/facilities/repair_facility_queue_reservations.py`다. 적용 보고서와 같은 실행 폴더의 `Backups`를 함께 보관한다.

### 내비게이션과 조명: 테스트를 위해 제품 설계를 바꾸지 않기

NavMesh는 관람객에게 주는 보행 지도다. 실제 지형은 그대로인데 오래된 지도만 남으면 눈앞에 길이 있어도 길 찾기는 실패할 수 있다. 이번에는 지형을 옮기거나 관람객 크기를 줄이는 대신 원본 맵을 백업하고 현재 지형에 맞게 보행 지도를 다시 구웠다. 샘플 검사에서 출구까지 완전 경로가 있는 존이 22개에서 31개로 늘었다. 별도의 네이티브 테스트는 31개 실제 존, 게임에서 사용하는 출구 목표 3개, 서로 다른 존 사이 31 × 30 = 930개의 방향별 경로를 검사했고 실제 출구 도착 반경 400cm도 대조해 통과했다. 이 반경은 테스트를 통과시키려고 늘린 값이 아니라 게임이 사용하는 도착 조건이다. 단순 투영 성공이나 목적지에 닿지 못하는 부분 경로를 통과로 바꾸지 않았다.

보조 도구의 새 프로세스 읽기 전용 보고서 `Artifacts/NavigationRepair/20260911-105827-338078/report.json`은 31개 존 × 81개 = 2,511개 샘플을 빠짐없이 기록한다. 그중 2,493개는 완전 경로, 18개는 NavMesh 투영 불가다. **모든 샘플이 성공했다는 뜻은 아니다.** 검사 전후 맵 해시가 같으며, 게임에서 사용하는 실제 존 기준점과 출구의 정상 경로는 위 네이티브 검사로 확인했다.

런타임 무대 조명 9개는 누락 사고가 아니라 2026-09-10 아티스트 변경의 결과였다. 원본 미리보기 Blueprint에는 10개가 남아 있으므로 두 에셋의 목적을 구분했다. 테스트는 9라는 숫자만 확인하는 대신 의도된 9개의 이름을 각각 검증하여 다른 조명이 빠지고 무관한 조명이 들어와도 적발한다. `Artifacts/Validation/SM5SM6-20260911/Lighting-authored-audit.json`은 실제 런타임 조명 9개가 양의 밝기와 Movable 설정을 가지며 에셋을 수정하지 않았음을 기록한다.

### 다음 변경에서 같은 문제가 되풀이되지 않게 하는 순서

1. 시설을 이름 변경·이동할 때 카탈로그 정의, 비용표, 프로그램 시설 참조, 자동화 fixture를 함께 검색한다. 레거시 정의를 남기는 이유와 명시적 비용 연결을 기록한다.
2. 스플라인·진입 앵커·대기 용량을 바꾸면 시설 투영 검사를 실행한다. 중심점뿐 아니라 신체 여유와 예약 영역의 연속성을 확인한다.
3. 지형·충돌·맵 배치 변경 뒤에는 NavMesh를 재생성하고 저장된 맵을 새 프로세스에서 검사한다. 존에서 출구까지 완전 경로가 이어지는지도 확인한다.
4. 아티스트가 조명이나 시각 구성을 바꾸면 원래 설계의 근거와 변경한 이름을 확인한 뒤 테스트·생성 도구·검증 도구를 함께 갱신한다.
5. 오류 문구가 번역·통합되어도 같은 오류를 식별할 수 있도록 안정적인 식별자를 검사하고, 잘못된 데이터의 거부 조건은 별도로 유지한다.
6. 개별 복구 도구의 성공만으로 릴리스를 결정하지 않고, 저장·새 프로세스 검증 후 전체 표준 패키징과 두 그래픽 경로 QA를 수행한다.

## 부록: 이번에 실제로 복구한 패키징 차단 원인

첫 패키징 시도는 데이터 검증에서 오류 23건·경고 7건으로 중단됐다. 오류 개수는 서로 다른 고장 개수와 같지 않다. 예를 들어 한 애니메이션의 스켈레톤이 없으면 로딩, 컨트롤 리그, 압축 검사에서 연쇄 오류가 발생한다. 다음 조치를 적용한 뒤 R2 패키징의 데이터 검증은 오류 0건·경고 1건으로 통과했다. 이 절은 그 첫 복구의 이력이고, 이후 전체 수정 상태의 최종 데이터 검증 결과는 0장에서 따로 확인한다.

| 복구 항목 | 원인과 처리 | 실제 확인한 증거 |
| --- | --- | --- |
| `BP_FF_CrowdLane` 템플릿 검사 | 각 배치 레인에 필요한 `LaneId`를 설계도인 Blueprint 기본 객체에도 요구했다. `!IsTemplate()`인 배치 객체에만 ID를 요구하고 폭·스플라인·공간 검사는 유지했다 | 신규 C++ 회귀 테스트가 템플릿 ID 생략 허용·잘못된 폭 거부·배치 레인 ID 필수를 모두 확인 |
| `Test_Main`의 상호작용 그래프 | 네이티브 클래스·함수는 존재하지만 캐스트 대상과 `TryStartInteraction` 노드 참조가 끊겨 있었다. 해당 노드 쌍만 재연결하고 컴파일 후 저장했다 | 원본 `Saved/PackagingRepair/D7EFE57D41864057452D9E804533D5FD/Test_Main.umap`; 신규 프로세스에서 LevelScriptActor 1개와 맵 로딩 확인 |
| `50_Man/Breathing_Idle`, `50_Man/Walk_With_BriefcaseTake_001` | 스켈레톤 참조가 비어 있었다. 실제 메시·정상 애니메이션의 뼈 31개 이름과 순서를 비교한 뒤 기존 `50_Man` 스켈레톤을 연결했다 | `Artifacts/AnimationSkeletonRepair/20260911-101925-081991/report.json`과 `Backups`의 원본·해시 |
| `Elder_Woman/Idle__2_Take_001` | 같은 방식으로 기존 `Elder_woman` 스켈레톤을 연결했다 | 별도 Unreal 프로세스의 `Artifacts/AnimationSkeletonRepair/20260911-101957-457102/report.json`: 세 에셋 모두 이미 올바름, 추가 변경 0개, 프로세스 종료 코드 0 |

설계도 비유로 설명하면 Blueprint 기본 객체는 빈 양식이고, 레벨에 배치한 Actor는 내용을 채운 실제 신청서다. 서로 다른 도로를 구분하는 ID는 실제 신청서에는 필요하지만, 모든 양식에 같은 ID를 적어 두는 방식은 올바른 해결책이 아니다. 그래서 빈 양식의 ID만 허용하고 실제 배치 객체 검사는 남겼다.

애니메이션의 스켈레톤은 동작을 적용할 뼈의 주소록이다. 이름이 비슷하다는 이유만으로 아무 주소록이나 연결하면 캐릭터가 뒤틀릴 수 있다. 이번에는 실제 메시와 정상 동작 에셋을 비교했고, 저장 전후 뼈 이름·키 개수·재생 길이가 같다는 것을 검사했다. 두 `Take_001` 에셋은 원래 키 2개·약 0.0333초인 짧은 데이터였으며 길이를 늘리거나 동작을 새로 만들지 않았다. 자연스러운 움직임인지는 별도 플레이 QA에서 확인한다.

복구 도구는 반복 가능한 작업 기록으로 남겼다. **이 문서의 일반 패키징 과정마다 실행할 필요는 없다.** 이미 복구된 에셋이 저장소 작업 트리에 반영되어 있다.

- `Tools/Build/repair_packaging_animations.py`: 기본은 읽기 전용 검사. `FESTAFIESTA_APPLY_ANIMATION_REPAIR=1`일 때만 정확히 지정된 세 에셋을 검사·백업·복구한다. 임의 에셋을 일괄 수정하지 않는다.
- `Tools/Build/repair_packaging_interaction.py`: 에디터 전용 `FF.Packaging.RepairInteractionGraph apply` 명령을 호출한다. 알려진 캐스트·함수 쌍만 고치며 원본 백업과 컴파일 통과가 저장 조건이다.
- `Source/FestaFiesta/Private/Editor/FF_PackagingInteractionRepair.cpp`: 위 복구 명령의 C++ 구현. `WITH_EDITOR`로 감싸져 Shipping 게임에 이 보조 기능이 포함되지 않는다.
- `Tools/Diagnostics/repair_legacy_facility_cost.py`: 기본은 읽기 전용. `-FFApplyLegacyFacilityCost`로 지정된 숨겨진 Taco 정의의 비용 키만 백업 후 변경하며, `-FFVerifyLegacyFacilityCost`로 새 프로세스에서 저장 값을 검사한다.
- `Tools/facilities/repair_facility_queue_reservations.py`: 기본은 읽기 전용 계획·검사. `FESTAFIESTA_APPLY_QUEUE_REPAIR=1`일 때 필요한 예약 영역만 백업 후 저장한다. 해당 변수를 주지 않은 새 프로세스에서 다시 읽어 남은 수정 0개를 확인한다.
- `Tools/Build/rebuild_packaging_navigation.py`: 기본은 읽기 전용 경로 샘플 검사. `FESTAFIESTA_REBUILD_NAVIGATION=1`일 때 맵을 백업하고 NavMesh를 재생성한다. 이전에 성공한 존이나 개별 샘플이 사라지거나 좌표·폴리곤·출구 입력이 달라지면 저장을 거부한다. 단순 성공 개수 비교로 퇴행을 놓치지 않는다. 저장 후 `FestaFiesta.Probe.NavMeshBakeStatus`로 실제 게임이 사용하는 경로를 별도 검사한다.

맵을 처음 복구·저장하는 과정에서 일시적인 LevelScriptActor 중복 ensure가 기록됐다. 이를 무시하고 성공으로 간주하지 않고 새 Unreal 프로세스에서 저장된 맵을 다시 검사했다. 실제 저장된 LevelScriptActor는 1개였고, 재로딩 시 중복 오류가 없었다. 검사 전후 맵 SHA-256도 동일했다. 근거는 `Artifacts/Validation/SM5SM6-20260911/Interaction-persisted-map-audit.json`이다. 이 확인은 저장 구조 검증이며, 사진 상호작용의 실제 버튼 입력·플레이 검증은 여전히 필요하다.

추가로 `FFF_CrowdAppearenceRow::bIsChild`의 불리언 기본값이 빠져 있던 엔진 초기화 오류를 수정했다. 기본값을 `false`로 명시하여 새 외형 데이터 행의 상태가 확정되도록 했다. 저장된 기존 행에 지정한 아동 여부를 일괄 변경한 작업은 아니다.

자동화 실행기는 종료 코드와 로그만 보지 않고 **이번 실행에서 새로 생성한 `index.json`**도 검사한다. 테스트 0개, 실패, 미실행, 실행 중인 항목, 상태·개수 불일치를 성공으로 처리하지 않는다. 정상·경고 성공과 여섯 가지 불합격 조건을 실제 판정식으로 확인한 8개 시나리오는 `Artifacts/Validation/SM5SM6-20260911/Automation-report-gate-fixtures.json`에 남겼다. 경고 성공 테스트 99개와 DataValidation 경고 1건은 실패 0건과 구분해서 읽는다.

실제 컴파일 로그에서도 MSVC `14.44.35225`와 Windows SDK `10.0.22621.0` 선택을 확인했다. 최종 근거는 `Artifacts/Releases/FestaFiesta-Win64-SM5SM6-20260911/Logs/BuildCookRun.log`와 `Metadata/build-manifest.json`이다. 파일이 설치되어 있다는 사전 검사와 빌드 도구가 실제로 선택했다는 로그는 구분해서 보관한다.
