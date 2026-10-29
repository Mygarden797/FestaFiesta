# Blender, Tripo, Meshy MCP 설정 (Windows)

프로젝트 루트의 `.mcp.json`은 이 형식을 지원하는 MCP 클라이언트용이다. Codex는
별도의 사용자 설정을 사용하므로 아래 명령으로 등록한다. 기존 설정은 지우지 않는다.
서버는 외부 서비스이며 모델 생성 등에는 별도의 계정과 크레딧이 필요하다.

## 준비

- Node.js 20 이상과 `npx`, Python 3.10 이상, Git을 설치한다.
- Blender 5.1 이상과 [Blender Lab MCP 애드온](https://www.blender.org/lab/mcp-server/)을 설치하고 활성화한다.
- Blender 공식 MCP 서버를 설치하고 `blender-mcp`가 PATH에서 실행되는지 확인한다.

```powershell
python -m pip install "git+https://projects.blender.org/lab/blender_mcp.git"
Get-Command blender-mcp
```

Blender에서 MCP 애드온의 서버를 시작한다. 연결은 기본적으로 로컬을 사용한다.
백그라운드 Blender 기능을 쓰려면 Blender 실행 파일을 PATH에 추가하거나 각 클라이언트
프로세스의 `BLENDER_PATH` 환경 변수에 전체 경로를 설정한다.
이 서버는 Blender에서 임의 Python 코드를 실행할 수 있으므로 신뢰하는 작업에만 사용한다.

Tripo는 [Tripo CLI](https://developers.tripo3d.com/en/docs/cli)의 계정 로그인 또는
`TRIPO_API_KEY`를 사용한다. Meshy는 [API 키](https://docs.meshy.ai/en/api/ai)를
`MESHY_API_KEY` 환경 변수로 설정한다. 키는 저장소, PR, 채팅에 올리지 않는다.
환경 변수를 설정한 뒤 MCP 클라이언트를 다시 시작해야 한다.

## Codex 등록

Codex는 프로젝트 `.mcp.json`을 자동으로 읽는 설정으로 취급하지 않는다.
기존에 같은 이름이 있으면 `codex mcp get 이름`으로 먼저 확인하고 중복 등록을 피한다.

```powershell
codex mcp add blender -- blender-mcp --transport stdio
codex mcp add tripo --env TRIPO_NO_UPDATE_CHECK=1 -- cmd /c npx -y tripo-cli@0.7.2 mcp
codex mcp add meshy -- cmd /c npx -y @meshy-ai/meshy-mcp-server@0.6.6
codex mcp list
```

이후 사용자 `~/.codex/config.toml`의 해당 서버 섹션에 전달할 환경 변수 이름을 추가한다.
값이 아니라 이름만 기록한다.

```toml
# [mcp_servers.meshy] 아래
env_vars = ["MESHY_API_KEY"]

# [mcp_servers.tripo] 아래: API 키 방식 또는 지역 설정을 사용하는 경우
env_vars = ["TRIPO_API_KEY", "TRIPO_REGION"]
```

CLI 로그인을 사용하는 경우
각 사용자의 로그인 상태가 필요하다. `npx`는 최초 실행 시 npm 접속이 필요하다.
클라이언트를 재시작한 뒤 Blender 씬 조회, Tripo 잔액 조회, Meshy 잔액 조회로 연결을 확인한다.
키 없이 도구 목록이 나타나는 것만으로는 계정 인증이 검증된 것이 아니다.

버전 변경 시 [Tripo CLI 문서](https://developers.tripo3d.com/en/docs/cli)와
[Meshy MCP 문서](https://github.com/meshy-dev/meshy-mcp-server)를 확인하고 설정을 함께 갱신한다.
