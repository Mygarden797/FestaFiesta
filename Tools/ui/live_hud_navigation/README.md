# 라이브 HUD 명칭·매출 순위 진입점

에디터 모듈 빌드 후 PIE를 종료하고 `FF.ApplyLiveHudNavigation`을 실행한다.
명령은 기존 위젯 크기·글꼴·색·슬롯을 유지하면서 다음 부분만 한 번 변경한다.

- `관객` 탭과 패널 제목 → `현황`
- `시설` 탭의 열린 패널 제목 → `시설 상태` (하단 `시설` 탭 이름 유지)
- `시설` 바로 뒤에 `매출 순위` 탭 추가

매출 순위는 `UFF_FacilityRevenueRankingSubsystem`의 기존 화면을 사용한다.
시설 선택 없이 열 수 있고, 동일 탭 재클릭·다른 하단 탭 선택 시 닫힌다.
원래 순위 화면의 오늘/축제 누적, 시설 정보 보기, ESC, 읽기 전용 경제 집계를 유지한다.
실행 중에는 기존 순위 화면의 자체 위치·Z71을 따른다. 일반 라이브 패널 위치 표를 새로 만들지 않는다.

변경 전 원본은 `Saved/UIBackups/LiveHudNavigation/` 아래에 백업한다.
적용 버전은 WBP 메타데이터 `FF_LiveHudNavigationVersion=1`이며 재실행 시 추가 편집을 보존한다.
`apply.py`는 같은 명령을 실행하고 원본 위젯 속성 보존 및 바인딩을 확인한다.
결과는 `Saved/LiveHudNavigation/apply-report.json`에 남긴다.

에디터를 닫은 상태에서 `./Tools/ui/live_hud_navigation/run.ps1`로 일회용 에디터에 적용할 수 있다.
개인 에디터 설정과 세이브를 보존하며, 같은 실행에서 바깥 클릭 입력 경로와 라이브 탭 단계 규칙 2개만 확인한다.
실제 화면과 마우스 조작은 사용자가 에디터에서 확인한다.

디자이너에서 `WBP_FF_LiveHudScreen > Tab_RevenueRanking`의 라벨·아이콘·슬롯을 편집할 수 있다.
관련 에셋 전체를 다시 만드는 `FF.LiveHudUMG`/밝은 HUD 재생성 명령은 필요하지 않다.
