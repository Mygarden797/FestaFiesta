# FestaFiesta Windows 배포본

이 폴더는 Windows 64비트용 Shipping 빌드입니다. 최상위의 `FestaFiesta.exe`를 실행하세요.

## 실행 환경

- 64비트 Windows 10 또는 Windows 11
- DirectX 12 및 Shader Model 6 지원 그래픽 환경 권장
- 최초 실행 전 `Engine/Extras/Redist/en-us/vc_redist.x64.exe` 설치 필요

DirectX 12를 사용할 수 없는 환경에서는 프로젝트에 포함된 DirectX 11 경로로 실행을 시도할 수 있지만, 그래픽 기능과 성능이 제한될 수 있습니다.

## 무결성 확인

배포처에서 함께 제공하는 `PACKAGE-SHA256SUMS.txt` 또는 ZIP의 `ARCHIVE-SHA256.txt`와 다운로드한 파일의 SHA-256을 비교하세요. 빌드 버전과 소스 커밋은 `build-manifest.json`에 기록되어 있습니다.

## 문제 보고

문제가 재현되면 다음 정보를 함께 전달하세요.

- `build-manifest.json`
- Windows 버전과 GPU/드라이버 버전
- 재현 순서
- `%LOCALAPPDATA%/FestaFiesta/Saved/Logs` 아래의 최신 로그(존재하는 경우)

외부 에셋과 라이선스 고지는 `Licenses/THIRD_PARTY_NOTICES.md`에서 확인할 수 있습니다.
