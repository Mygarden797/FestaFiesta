# FestaFiesta Dev 일일 빌드

한국 시간 매일 **19:00에 원격 `Dev` 브랜치를 확인**한다. 마지막으로 Google Drive 업로드·무결성 검증을 끝낸 커밋과 달라졌을 때 Windows 게임을 빌드하고, `C:\FestaFiestaBuild`에 저장한 뒤 지정 Drive 폴더에 올린다. 19:00은 빌드 시작 시각이며 완료 시각은 컴파일·Cook·검증·업로드 시간에 따라 달라진다.

기본 업로더는 공식 Cyberduck GUI에서 한 번 로그인한 뒤 같은 Windows 사용자의 Credential Manager를 사용하는 Cyberduck CLI다. 자체 OAuth 앱의 홈페이지·정책 도메인을 마련하는 절차 없이 공식 앱으로 인증한다. 실제 게임 패키징과 게시의 성공 여부는 각 실행 로그와 상태 파일로 확인한다.

## 구성과 저장 위치

| 항목 | 기본값 |
| --- | --- |
| Jenkins | [http://127.0.0.1:8090](http://127.0.0.1:8090) — 이 PC에서만 접속 |
| Job | `FestaFiesta-Dev-Daily` |
| 수동 Drive 연결 검사 Job | `FestaFiesta-Drive-ConnectionCheck` |
| Agent | `windows-local`, label `windows-festafiesta`, executor 1개 |
| 설치 폴더 | `C:\FestaFiestaCI` |
| 빌드 전용 checkout | `C:\FFCI\Dev` |
| 결과물 | `C:\FestaFiestaBuild\<빌드 라벨>` |
| 운영 설정 | `C:\FestaFiestaCI\settings.json` |
| 게시 상태 | `C:\FestaFiestaCI\state\daily-build-state.json` |
| Jenkins 관리자 | `festafiesta-admin` |
| 관리자 비밀번호 파일 | `C:\FestaFiestaCI\secrets\admin-password.txt` |
| 기본 업로더 | `UploadProvider: Duck` |
| 전용 CLI 실행 파일 | `C:\FestaFiestaCI\tools\duck\duck.exe` |
| 보호된 Drive 경로 설정 | `C:\FestaFiestaCI\secrets\duck.json` |
| OAuth 자격 증명 | 동일 Windows 사용자의 Credential Manager |
| 일회성 폴더 확인용 rclone 설정 | `C:\FestaFiestaCI\secrets\rclone.conf`, remote `festafiesta-drive` |
| Windows 예약 작업 | `FestaFiesta-Jenkins-Local` |
| 업로드 폴더 | [팀 빌드 Drive](https://drive.google.com/drive/folders/13QQv-LeeTXIWG00bcQ9N9q-1I2vE_spL) |

`settings.json`에는 경로·브랜치·업로더 종류·Drive 폴더 ID만 넣는다. `duck.json`에는 계정 식별자, Drive 경로, 대상 마커의 SHA-256이 들어가며 OAuth 토큰은 들어가지 않는다. 비밀번호, OAuth client secret, access token, refresh token은 저장소나 Job 파라미터에 넣지 않는다. 설치 폴더는 현재 Windows 사용자, SYSTEM, 로컬 Administrators만 접근하도록 ACL을 설정한다. `secrets` 내용과 Jenkins 전체 홈을 채팅·빌드 로그·Git에 복사하지 않는다.

제어 스크립트는 설치 시 `C:\FestaFiestaCI\scripts`로 복사된다. 저장소에서 수정한 스크립트는 설치본에도 반영해야 한다. 실제 빌드 소스는 별도 checkout의 고정 커밋이며, 개발 중인 작업 폴더를 checkout/reset하지 않는다. 이 자동화 파일을 로컬 브랜치에 추가하는 것과 원격 `Dev`에 push하는 것은 별도 작업이다.

## 설치와 시작

1. Windows에 프로젝트 [툴체인 잠금 파일](../Build/toolchain.lock.json)에 맞는 엔진·MSVC·Windows SDK와 Git/Git LFS를 설치한다. 현재 잠금은 UE 5.7.4, MSVC 14.44.35228, Windows SDK 10.0.22621.0이다. 실제 빌드 대상 커밋의 잠금 파일이 기준이다.
2. GPU와 드라이버가 있는 Windows 사용자 세션에서 진행한다. `C:\FFCI\Dev`는 비어 있어야 한다. 개발 checkout을 이 경로로 지정하지 않는다.
3. 저장소 루트의 PowerShell에서 설치한다. 엔진 경로가 다르면 `-EngineRoot`를 지정한다.

```powershell
& '.\Tools\Jenkins\Install-FestaFiestaJenkins.ps1' -Start
& 'C:\FestaFiestaCI\scripts\Install-FestaFiestaDuck.ps1'
```

첫 설치기는 Java, Jenkins LTS, 필요한 Pipeline 플러그인, rclone을 설치하고 로컬 Job과 agent를 준비한다. 두 번째 스크립트는 서명·체크섬을 확인한 공식 Cyberduck GUI와 CLI 9.5.4를 설치하고 전용 CLI 런타임을 준비한다. Windows 예약 작업은 현재 사용자가 로그인할 때와 이후 5분 간격으로 실행 상태를 확인한다. 설치 과정에서 빌드를 자동 요청하지 않는다.

현재 공식 CLI 패키지에서 .NET 10 런타임과 일부 .NET 8 DLL의 버전 불일치가 확인되어, `Repair-FestaFiestaDuckRuntime.ps1`가 `tools\duck`의 전용 복사본에만 해당 런타임 버전의 Microsoft 서명 DLL 세 개를 적용한다. 시스템에 설치된 Cyberduck 파일은 바꾸지 않는다. 기록은 `tools\duck\festafiesta-runtime.json`에 남는다. CLI를 갱신하면 다음 명령으로 전용 복사본도 다시 준비하고 연결 검사를 수행한다.

```powershell
& 'C:\FestaFiestaCI\scripts\Repair-FestaFiestaDuckRuntime.ps1'
```

이미 설치했다면 다음 명령으로 시작한다.

```powershell
& 'C:\FestaFiestaCI\scripts\Start-FestaFiestaJenkins.ps1'
```

[Jenkins](http://127.0.0.1:8090)에 `festafiesta-admin`으로 로그인하고, 비밀번호는 보호된 로컬 비밀번호 파일에서 직접 확인한다. agent `windows-local`이 online 상태인지 확인한다.

이 구성은 **현재 사용자가 로그인한 상태**에서 실행한다. OAuth 자격 증명과 GPU를 사용하는 검증 모두 같은 사용자 세션에 의존한다. Windows 서비스의 비대화형 세션으로 실행하지 않는다. 로그아웃·절전 상태에서는 19시 작업이 정상 실행되지 않을 수 있으므로 빌드 시간 동안 PC를 켜고 로그인 상태를 유지한다.

## Google Drive 최초 연결

빌드 계정은 지정 Drive 폴더에 파일과 하위 폴더를 만들 수 있어야 한다. 공식 Cyberduck GUI에서 Google Drive 연결을 만들고 브라우저 동의를 완료한다. Jenkins agent를 실행하는 **동일한 Windows 사용자**로 로그인하고 자격 증명을 저장한다. CLI는 이 자격 증명을 재사용하며, CLI 자체로 Google Drive의 최초 브라우저 인증을 시작하지 않는다. 다른 앱의 client ID를 빌리거나 토큰을 추출·복사하지 않는다. [공식 Google Drive 연결 안내](https://docs.cyberduck.io/protocols/googledrive/), [CLI 안내](https://docs.cyberduck.io/cli/)

최초 대상 등록에는 기존에 허가받은 rclone 연결을 한 번 사용한다. `Complete-FestaFiestaDuckConnection.ps1`는 요청된 **원래 폴더 ID**에 고유 마커 `.festafiesta-ci-target.json`을 만들고, Duck 경로에서 마커와 테스트 파일의 SHA-256을 확인한 뒤 원래 폴더 ID로도 교차 검증한다. 모두 성공해야 `settings.json`을 `UploadProvider: Duck`으로 전환한다. 현재 설치에는 이 등록이 완료되어 있으며, 평소 Duck 업로드는 rclone 토큰 갱신에 의존하지 않는다.

다음 명령은 최초 등록·대상 재설정용이다. `-Username`에는 GUI에서 로그인한 계정을 지정하고, `-RootUri`는 GUI에서 확인한 정확한 대상 경로를 URL 인코딩해 지정한다. 예시 계정은 실제 로그인 계정으로 바꾼다.

```powershell
& 'C:\FestaFiestaCI\scripts\Complete-FestaFiestaDuckConnection.ps1' `
    -Username 'build-account@example.com' `
    -RootUri 'googledrive:/My%20Drive/FestaFiesta%21'
```

마커의 해시·폴더 ID가 다르거나 마커를 읽을 수 없으면 업로드를 거부한다. 같은 이름의 다른 폴더를 대상으로 삼지 않기 위한 검사다. 마커를 다른 폴더로 복사하거나 검사를 통과시키려고 해시를 수동 변경하지 않는다. 폴더를 이동·이름 변경하거나 로그인 계정을 바꾸면 경로와 권한을 확인한 뒤 연결 검사를 다시 수행한다.

GUI 재인증 뒤에는 다음 명령으로 **Jenkins의 실제 Windows agent**에서 연결 검사를 요청한다.

```powershell
& 'C:\FestaFiestaCI\scripts\Manage-FestaFiestaJenkins.ps1' -Action VerifyDrive
```

[연결 검사 Job](http://127.0.0.1:8090/job/FestaFiesta-Drive-ConnectionCheck/)은 같은 agent에서 마커 확인 → 작은 파일 업로드 → 전체 SHA-256 재다운로드 검증을 실행한다. 검사 파일은 휴지통으로 이동하고, 새 UUID 검사 폴더도 빈 목록을 확인한 경우에만 휴지통으로 이동한다. 목록이 비어 있지 않으면 폴더를 보존하며 재귀 삭제는 하지 않는다. 결과는 `state\jenkins-agent-drive-verification.json`에 사용자 SID와 함께 기록한다. 이 검사는 게임을 빌드하거나 게시 상태를 바꾸지 않는다.

2026-11-03 **17:41:57 KST**의 연결 검사 **#4**가 성공했다. agent의 사용자 SID 일치, 대상 마커, 업로드 후 SHA-256 재다운로드 검증, 검사 파일과 빈 검사 폴더의 휴지통 이동까지 확인되었다. 이 기록은 Drive 연결 검증이며 게임 패키징·전체 일일 빌드 성공을 뜻하지 않는다.

자체 Desktop OAuth client를 사용하는 rclone 경로는 선택 사항이다. 필요한 경우 `Connect-FestaFiestaDrive.ps1 -ClientJsonPath '<다운로드한 Desktop JSON 경로>'`로 연결하며 재인증에는 `-Reconnect`를 추가한다. 외부 앱을 `Testing` 상태로 두고 Drive 권한을 요청하면 refresh token이 7일 뒤 만료된다. 이 경로를 장기 업로더로 운영하려면 Branding 필수 항목과 게시 상태를 완료하고 재인증해야 한다. 테스트 사용자 등록만으로 이 조건이 해결되지는 않는다. 현재 Duck 경로에는 자체 OAuth 앱의 게시가 필요하지 않다. [Google 토큰 만료 안내](https://developers.google.com/identity/protocols/oauth2#expiration)

기존 폴더에 접근하는 rclone 구성에서 `drive.file` scope만으로 동작한다고 가정하지 않는다. `drive.file`은 앱이 만들거나 앱에 별도로 허용한 파일에 한정된다. `root_folder_id`도 작업의 시작 폴더를 정할 뿐 OAuth 권한 자체를 그 폴더로 축소하지 않는다. [Google Drive scope 안내](https://developers.google.com/workspace/drive/api/guides/api-specific-auth), [rclone scope 안내](https://rclone.org/drive/#scopes)

## 평소 사용과 빌드 결과

Job은 `TZ=Asia/Seoul`과 `0 19 * * *`로 예약한다. 실행 중 중복 Job을 허용하지 않으며, Windows agent는 한 번에 하나의 작업을 처리한다. 파이프라인 실행 제한은 6시간이다.

1. 이전 업로드 실패본이 있으면 먼저 같은 파일의 업로드·검증을 재개한다.
2. 원격 `refs/heads/Dev`의 SHA를 조회한다. `Dev`의 대소문자를 유지한다.
3. 마지막 **업로드 성공 SHA**와 같으면 빌드 없이 종료한다.
4. 다르면 그 SHA로 checkout하고 LFS 에셋을 받는다. 빌드 도중 새 push가 와도 이번 실행은 처음 선택한 SHA를 유지한다.
5. `Package-Win64.ps1`의 표준 패키징과 `-GraphicsSmokeTest`를 실행한다. 검증 생략 옵션은 사용하지 않는다.
6. 성공본의 ZIP, `build-manifest.json`, `ARCHIVE-SHA256.txt`, `verification.json`을 같은 라벨의 Drive 하위 폴더에 올린다. 각 파일을 올리기 전에 대상 마커를 검증하며, 업로드 뒤에는 파일 전체를 다시 내려받아 SHA-256과 크기를 대조한다.
7. 네 파일의 업로드와 검증이 끝난 뒤에만 게시 성공 SHA를 원자적으로 갱신한다.

전체 파일 재다운로드 검증으로 업로드 용량과 비슷한 다운로드 트래픽·임시 디스크 공간이 추가로 필요하다. 6시간 제한에는 패키징과 이 왕복 검증이 모두 포함된다. 네 파일이 전부 검증되기 전에는 Drive에 폴더가 보이더라도 게시 성공으로 보지 않는다.

라벨 예시는 `FF-v1.0.0.0-20261103-190000-b12-a1b2c3d4`다. 날짜·시각은 한국 시간이며 버전은 선택한 커밋의 `ProjectVersion`, `b12`는 Jenkins 빌드번호다. 소스의 게임 버전을 자동 증가시키지는 않는다.

로컬 라벨 폴더에는 `Package`, ZIP, 내부 분석용 `Symbols`, `Metadata`, `Validation`, `Logs`가 남는다. Drive에는 배포 ZIP과 확인용 메타데이터만 올린다. 게임을 전달할 때 EXE 하나가 아닌 ZIP 전체를 사용한다. 실행 방법은 [배포본 안내](../../Distribution/README_KO.md)를 따른다.

상태 확인과 수동 실행은 다음 명령을 쓴다. `Build`는 정상 게시 실행이며 `BuildOnly`는 아래의 초기 검증 모드다.

```powershell
& 'C:\FestaFiestaCI\scripts\Manage-FestaFiestaJenkins.ps1' -Action Status
& 'C:\FestaFiestaCI\scripts\Manage-FestaFiestaJenkins.ps1' -Action Logs
& 'C:\FestaFiestaCI\scripts\Manage-FestaFiestaJenkins.ps1' -Action Build
```

빌드 중에는 전용 checkout `C:\FFCI\Dev`의 에디터나 게임을 열지 않는다. 사전 검사는 프로젝트 파일명 대신 실제 물리 경로를 대조하므로, 다른 개발 checkout의 에디터·Live Coding은 유지할 수 있다. junction이나 symlink처럼 경로만 다르고 실제 파일이 같은 checkout은 사용 중으로 판정한다. 이 검사를 시작 시점과 UAT 직전에 반복하며, 자동화가 개발 에디터를 강제 종료하지는 않는다.

검증 단계에서 CI의 Development Editor를 먼저 빌드하고 자동 검사·데이터 검사를 수행한다. 이후 UAT에는 공식 `-nocompileeditor`를 전달해 방금 검증한 Editor DLL을 보존하고 Cook에 사용한다. 배포용 게임 타깃의 clean build, Cook, 패키지·그래픽 실행 검증은 그대로 수행한다. 별도 checkout에서 열린 개발 에디터 때문에 공유 엔진 실행 파일의 Live Coding mutex에 걸려 CI Editor를 다시 빌드하지 못하는 상황을 피하는 구성이다.

## 처음 검증할 때

인증·파일·네트워크를 바꾸지 않는 계획 조회:

```powershell
& 'C:\FestaFiestaCI\scripts\Run-DailyBuild.ps1' -Plan
```

이 모드는 원격 SHA도 조회하지 않는다. 원격 변경 여부는 실제 실행에서 판단한다.

Drive 인증 전 패키징 검증은 Job의 **Build with Parameters → `BUILD_ONLY=true`**로 요청하거나 다음 명령을 쓴다.

```powershell
& 'C:\FestaFiestaCI\scripts\Run-DailyBuild.ps1' -BuildOnly
```

`BUILD_ONLY`는 초기 검증용이다. 완성본을 `pending` 상태로 남기고 Drive 업로드와 게시 성공 SHA 갱신을 생략한다. 이미 업로드 대기본이 있으면 새 빌드를 만들지 않는다. 인증을 마친 뒤 기본값 `BUILD_ONLY=false`로 실행하면 그 파일부터 업로드한다.

오프라인 회귀 검증:

```powershell
& '.\Tools\Jenkins\Test-FestaFiestaDailyBuild.ps1'
& '.\Tools\Jenkins\Test-FestaFiestaDuck.ps1' -CliHelpPath '.\Artifacts\Jenkins\duck-help.txt'
& '.\Tools\Jenkins\Test-FestaFiestaLiveDriveCleanup.ps1'
```

확인된 fixture 검증은 핵심 흐름 46개, Duck 전송 52개다. Duck의 52개에는 캡처한 실제 CLI 도움말의 옵션 확인이 포함된다. 위 도움말 파일은 이번 설치의 캡처 결과다. 다른 checkout에 파일이 없다면 설치된 `duck.exe --help`를 캡처해 그 경로를 지정한다. `-CliHelpPath`를 생략하면 도움말 확인을 제외한 51개를 실행한다. 원격 Git, Unreal 빌드, 실제 Drive 요청 없이 변경 생략, KST 라벨, 실패 복구, 파일 변조, 잠금, 마커 불일치, 폴더 생성 실패, 업로드 실패, 원격 변조·누락 등을 검사한다. 별도 cleanup fixture 24개는 CLI의 커서 제어문자를 포함한 빈 목록 처리, 비어 있지 않은 폴더 보존, 실패 시 성공 기록 거부를 확인한다. fixture 통과는 실제 게임 빌드나 수동 플레이 QA의 통과를 뜻하지 않는다.

## 문제 확인과 재시도

| 증상 | 확인할 곳과 다음 행동 |
| --- | --- |
| Jenkins 페이지가 열리지 않음 | `Start-FestaFiestaJenkins.ps1` 실행 후 `C:\FestaFiestaCI\logs\controller.err.log` 확인. 설치 폴더와 포트 충돌도 확인한다. |
| Job이 대기 상태 | `windows-local` online 여부, 같은 PC에서 수행 중인 이전 작업, 로그인·절전 상태 확인. `agent.err.log`를 확인한다. |
| PowerShell 모듈을 찾지 못함 | 관리 Job의 shell 환경을 확인한다. 기존 Job에는 `Manage-FestaFiestaJenkins.ps1 -Action RepairShell`로 `PSModulePath` 초기화를 반영할 수 있다. |
| CLI 시작 시 ConfigurationManager 버전 오류 | 전용 복사본에 `Repair-FestaFiestaDuckRuntime.ps1` 실행 후 `VerifyDrive`를 수행한다. 시스템 DLL을 수동 교체하지 않는다. |
| Cyberduck 실행 파일·설정 누락 | `DuckExe`, `DuckConfig`, 전용 CLI 준비 여부와 최초 연결 완료 여부를 확인한다. 인증 전 패키징만 검증하려면 `BUILD_ONLY`를 쓴다. |
| Duck 인증 오류·연결 검사 실패 | 같은 Windows 사용자로 GUI에서 Google Drive를 다시 연결한 뒤 `VerifyDrive`를 실행한다. CLI 진단 출력은 인증 정보 보호를 위해 억제된다. |
| 마커·폴더 ID 불일치 | 대상 경로의 이동·이름 변경·동명이름 폴더를 확인한다. 마커나 저장된 해시를 임의로 덮어쓰지 않는다. |
| checkout 소유권·dirty 오류 | 전용 checkout 경로와 `.git\festafiesta-jenkins-owner.json`을 확인한다. 다른 작업 폴더를 지정하거나 임의로 파일을 복사하지 않는다. |
| 엔진·SDK·컴파일·검증 실패 | Jenkins Console Output과 라벨의 `Validation`, `Logs\BuildCookRun.log`, `.staging\<라벨>\Metadata\failure.json` 확인. 원인을 고친 뒤 새 빌드번호로 실행한다. |
| Drive 업로드·해시 검증 실패 | 인증·폴더 편집 권한·용량·네트워크 확인 후 `BUILD_ONLY=false`로 재실행. 완성 패키지는 다시 빌드하지 않고 업로드를 재개한다. |
| pending 파일이 변경됐다는 오류 | 해당 파일을 덮어쓰지 말고 원래 ZIP·메타데이터와 해시를 확인한다. 검증 없이 상태를 성공으로 바꾸지 않는다. |
| state가 다른 설정에 속한다는 오류 | repository, branch, checkout, output, Drive 폴더, remote, configuration을 바꿨는지 확인한다. 다른 배포 대상은 별도 `StateRoot`를 사용한다. |

`C:\FestaFiestaBuild\latest.json`은 **로컬 패키징 성공 색인**이다. Drive 게시 성공은 `daily-build-state.json`의 `published`로 확인한다. `pending.phase=ready`이면 완성본 업로드가 남아 있다. `phase=building`은 빌드 도중 중단된 상태다. 다음 실행은 완성본이 있으면 복구하고, 없으면 새 시도로 빌드한다.

빌드 로그 보존은 Jenkins 최근 30일/30회다. 로컬 결과물과 Drive 산출물은 이 설정으로 자동 삭제되지 않으므로 저장 용량은 별도로 관리한다. 실제 플레이 QA, Discord·Slack 메시지 발송, 원격 push, 이전 결과물 삭제는 이 자동화에 포함하지 않는다.

## 중지와 해제

예약 빌드만 멈추려면 Jenkins에서 `FestaFiesta-Dev-Daily`를 **Disable**한다. 실행 중인 빌드는 상태·로그를 확인한 뒤 `Manage-FestaFiestaJenkins.ps1 -Action StopBuild`로 중지를 요청할 수 있다.

로그인 시 자동 시작과 5분 주기 상태 확인도 멈추려면 PowerShell에서 실행한다.

```powershell
Disable-ScheduledTask -TaskName 'FestaFiesta-Jenkins-Local'
```

완전히 해제할 때는 해당 예약 작업을 삭제하고 Jenkins controller/agent를 종료한다. controller는 `controller.pid`의 PID와 실제 실행 경로·명령줄을 확인한다. agent는 `agent-process.json`의 `id`, `startedAtUtc`를 실제 프로세스의 PID와 UTC 시작 시각에 대조한다. 두 프로세스가 `C:\FestaFiestaCI\tools\java\bin\java.exe`의 해당 Jenkins 실행임을 확인한 뒤 종료한다. PID가 재사용되었거나 시작 시각이 다르면 그 프로세스를 종료하지 않는다. 예약 작업을 멈추는 것만으로 이미 실행 중인 Java가 종료되지는 않는다.

```powershell
Unregister-ScheduledTask -TaskName 'FestaFiesta-Jenkins-Local' -Confirm:$false
```

결과물·게시 상태·로그를 보관한 뒤 설치 폴더 정리 여부를 결정한다. 인증도 해제하려면 Google 계정에서 Cyberduck 앱의 접근 권한을 철회하고 해당 Windows 사용자의 저장된 연결 자격 증명을 제거한다. 선택 사항인 자체 OAuth/rclone 연결도 사용했다면 별도로 철회한다. 이 절차만으로 기존 Drive 결과물이나 `C:\FestaFiestaBuild`를 삭제하지 않는다.
