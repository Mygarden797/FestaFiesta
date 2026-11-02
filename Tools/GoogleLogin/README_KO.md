# FestaFiesta Google 로그인

Windows 기본 브라우저에서 Google 계정을 선택하고 게임으로 돌아오는 선택형 로그인이다. 로그인하지 않아도 기존 플레이·저장·불러오기를 사용할 수 있다.

## 비용과 범위

- Google OAuth 데스크톱 클라이언트와 기존 게임만 사용한다. Cloud Run, VM, 유료 데이터베이스, Firebase/Identity Platform, 문자 인증은 만들거나 활성화하지 않는다.
- Google Cloud의 **업그레이드/전체 계정 활성화**를 누르거나 유료 결제 계정을 새로 연결할 필요가 없다. 현재 무료 체험 계정은 사용자가 직접 유료 계정으로 전환하지 않는 한 청구되지 않는다. 기존 다른 서비스의 과금 여부는 이 로그인 구현과 별개다.
- 요청 범위는 `openid profile email`이다. 계정 식별자(`sub`), 표시 이름, 확인된 이메일만 게임 메모리에 둔다. 비밀번호를 게임에 입력하지 않는다.
- 로그인 토큰·인증 코드·PKCE verifier를 SaveGame이나 설정 파일에 저장하지 않는다. 게임을 종료하면 로그인 상태가 사라진다.
- 이 기능은 로컬 게임에서 계정 프로필을 표시한다. 클라우드 저장, Google 계정별 저장 슬롯, 다른 PC로 진행 이전, 서버 권한 인증은 포함하지 않는다.

Google 공식 안내: [데스크톱 OAuth](https://developers.google.com/identity/protocols/oauth2/native-app), [OpenID Connect](https://developers.google.com/identity/openid-connect/openid-connect), [무료 체험과 유료 전환](https://cloud.google.com/signup-faqs).

## Google Cloud 설정

1. 프로젝트 `project-d35b65f5-f81a-4c82-81a`의 **Google 인증 플랫폼**을 연다.
2. 앱 이름을 `FestaFiesta`로 지정한다. 지원 이메일과 개발자 연락처는 소유자가 승인한 주소로 설정한다. 개인 Gmail 프로젝트는 외부 사용자를 선택한다.
3. 필요한 Google API 사용자 데이터 정책을 검토하고 동의한다. 별도 결제 상품에는 가입하지 않는다.
4. **클라이언트 → 클라이언트 만들기 → 데스크톱 앱**에서 `FestaFiesta Windows`를 만든다. 웹 애플리케이션/서비스 계정/API 키가 아니다.
5. 발급된 클라이언트 ID를 Unreal **Project Settings → Game → FestaFiesta Google Login**에 입력한다. 데스크톱 클라이언트 비밀번호가 필요한 경우에만 같은 데스크톱 앱의 값을 선택 설정한다. 웹 서버용 비밀키는 게임에 넣지 않는다.
6. 기본 프로필 범위만 요청한다. Drive/Gmail/People API 등 추가 권한은 필요 없다. 테스트/프로덕션 상태와 조직의 Google 로그인 제한은 배포 전 확인한다.

게임은 `127.0.0.1`에 임시 포트를 열고 매 시도마다 다른 콜백 주소를 사용한다. 데스크톱 클라이언트에는 웹사이트 허용 도메인이나 고정 웹 콜백 URL을 등록하지 않는다. 로그인 중에는 게임을 실행한 상태로 둔다.

### 현재 프로젝트 연결

2026-11-02에 `FestaFiesta Windows` 데스크톱 클라이언트를 발급하고 공개 ID를 `Config/DefaultGame.ini`에 연결했다. Google 토큰 서버에서 이 클라이언트가 `client_secret`도 요구하는 것을 더미 인증 코드로 확인했다.

필요한 데스크톱 인증값은 Git에서 제외한 `Config/Windows/GeneratedWindowsGame.ini`의 같은 설정 섹션에 둔다. 이 파일은 Unreal의 기본 Windows Game 설정 계층으로 읽으며, `[Staging]` 허용 목록으로 Windows 배포 파일에도 포함한다. 원본 로컬 보관은 Git에서 제외한 `Saved/GoogleLogin/desktop-client.local.json`이다. 어느 파일도 채팅이나 로그에 값을 출력하지 않는다.

다른 PC/CI에서 빌드할 때도 소유자가 관리하는 경로로 이 로컬 설정을 공급해야 한다. Git만 복제하면 공개 ID만 있으므로 이 클라이언트로 실제 로그인하기 전에 로컬 인증값을 설정한다. `DesktopClientSecret`이라는 이름과 관계없이 데스크톱 앱에 배포되는 값은 추출 가능하며, 서버용 비밀키와 같은 보호를 제공하지 않는다.

현재 연결 확인은 Google 토큰 서버의 더미 코드 응답(`invalid_grant / Malformed auth code`)과 Unreal 네이티브 `IsConfigured=true`, ID·로컬 인증값 로드 성공으로 수행했다. 실제 사용자 인증이나 계정 동의는 실행하지 않았다. 기존 배포 EXE에는 새 빌드를 만들어야 적용된다.

`DesktopClientId`는 공개 앱 식별자라서 배포 설정에 포함할 수 있다. 데스크톱 앱에 포함되는 `DesktopClientSecret`도 Google이 기밀 클라이언트 인증 수단으로 취급하지 않지만, 불필요하게 로그·채팅·소스에 공개하지 않는다. 액세스 토큰/갱신 토큰/사용자 인증 코드는 공개 식별자가 아니다.

## 게임에서 사용

게임 실행 후 타이틀에 처음 들어오면 화면 중앙에 Google 계정 로그인 창이 먼저 표시된다. Google 공식 **Sign in with Google** 버튼을 누르면 기본 브라우저에서 로그인할 수 있다. **로그인 없이 계속**을 누르면 기존 메뉴로 돌아간다. 같은 실행 중 타이틀을 다시 만들거나 돌아올 때는 시작 창을 반복 표시하지 않는다.

타이틀 오른쪽 위와 두 옵션 화면의 **계정정보**에서도 같은 계정 상태를 확인한다. 로그인하면 Google 표시 이름과 이메일이 표시되며, 로그인 완료·실패·취소·로그아웃은 열려 있는 화면에 즉시 반영된다. 설정 저장 버튼을 누를 필요는 없다. 기존 로컬 플레이어 이름과 기기 설정은 계정 정보와 구분해 표시한다.

진행 중 **취소**, 로그인 후 **로그아웃**을 사용할 수 있다. 로그아웃은 게임의 메모리 상태만 지우며 브라우저의 Google 계정을 로그아웃하지 않는다.

버튼 이미지는 [Google 공식 브랜드 자료](https://developers.google.com/identity/branding-guidelines)에서 제공하는 승인된 이미지를 그대로 사용한다. `Content/GoogleLogin`을 UFS 패키징 목록에 포함해 배포 게임에서도 표시한다. 출처와 파일 해시는 [BRANDING.md](BRANDING.md)에 기록한다.

설정이 없거나 네트워크/브라우저 로그인이 실패하면 상태 메시지를 표시하고 기존 오프라인 플레이를 계속 사용할 수 있다. 로그인 도중 레벨을 이동해도 서브시스템의 시도는 유지된다.

Blueprint에서는 `Get Game Instance Subsystem → FF Google Auth Subsystem`으로 접근하고 `OnAuthStateChanged`를 구독한다. UI는 이 이벤트를 재구성 시 다시 구독하고 제거 시 해제한다.

## 검증

에디터 빌드는 `Tools/Build/Build-Editor.ps1`을 사용한다. 인증 자동화는 `FestaFiesta.Authentication` 필터로 실행하며, 실행마다 별도 `-UserDir`을 지정해 플레이어 저장을 보호한다. 화면 렌더 검사는 `-RenderOffscreen`을 사용한다.

실제 계정 테스트 중에는 `LogHttp VeryVerbose`를 켜지 않는다. Unreal의 상세 HTTP 추적은 인증 헤더를 기록할 수 있다. 이 기능 자체는 사용자 정보나 토큰을 로그에 출력하지 않는다.

실제 Google 계정 테스트는 별도 사용자 동의가 필요하다. 다음을 Windows Development 게임에서 확인한다.

1. 설정 미입력·네트워크 단절에도 플레이와 저장이 가능한지 확인한다.
2. 로그인 버튼 → 기본 브라우저 → 계정 선택/동의 → 게임에 이름 표시를 확인한다.
3. Google 동의 거절, 게임의 취소 버튼, 브라우저를 닫은 뒤 시간 초과, 재시도를 확인한다.
4. 로그아웃·게임 재시작 후 계정 정보가 지워지고 기존 로컬 저장은 유지되는지 확인한다.
5. 로그 및 SaveGame에 인증 코드·토큰·이메일이 기록되지 않는지 확인한다.

컴파일·자동화 통과는 실제 Google 로그인 왕복이나 Shipping 패키지 검증을 대신하지 않는다.
