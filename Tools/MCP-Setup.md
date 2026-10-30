# FestaFiesta MCP 연결 안내

프로젝트 `.mcp.json`에는 Slack, Notion, Monolith, Blender, Tripo, Meshy가 함께 등록되어 있다.
Windows용 설정이며 `.mcp.json`을 지원하는 클라이언트에서 사용한다. Codex의 사용자 설정은
별도로 관리되므로 기존 서버를 지우지 말고 필요한 서버만 등록한다.

## Slack과 Notion

- Slack 봇 토큰은 로컬 `SLACK_BOT_TOKEN` 환경 변수에 설정한다. 설정 파일의
  `${SLACK_BOT_TOKEN}`은 이를 지원하는 클라이언트에서만 확장되며, 토큰 값은 저장소에 넣지 않는다.
  Slack 앱에는 사용하는 채널의 조회·메시지 전송 권한과 채널 접근 권한이 필요하다.
- Slack MCP의 메시지 도구가 활성화되어 있어도 사용자의 요청이 있을 때만 메시지를 전송한다.
  팀 채널은 `C0CCNP7GYNS`, 개인 채널은 `C0CDK3PLT6C`이다.
- Notion은 `https://mcp.notion.com/mcp`로 연결하며, 해당 MCP 클라이언트에서 Notion 로그인을
  완료하고 필요한 페이지에 접근 권한을 부여해야 한다.
- Codex는 프로젝트 `.mcp.json` 대신 별도 MCP 설정 또는 연결된 Slack·Notion 앱을 사용한다.
  기존 연결이 있으면 중복 등록하지 않는다.

## Unreal과 3D 제작 도구

- Unreal Monolith 설치와 시작은 [Monolith 설치 안내](Monolith-Setup.md)를 따른다.
- Blender, Tripo, Meshy 설치·인증 및 Codex 등록은 [3D MCP 설치 안내](3D-MCP-Setup.md)를 따른다.
- 키를 설정하거나 MCP 설정을 변경한 뒤에는 클라이언트를 다시 시작하고 읽기 전용 조회부터 확인한다.
  Meshy와 Tripo의 모델 생성에는 각 서비스의 계정과 크레딧이 필요하다.
