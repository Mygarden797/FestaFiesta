# 라이브 패널 P1 — 공통 틀·중앙 배치·하단 금화

현재 7개 패널의 내부 구성까지 제작되어 있다. 최신 크기·글꼴·색·문구 수정은 [통합 에디터 안내](EDITOR_GUIDE.md)를 참고한다. 아래 P1 설명과 검증 기록은 공통 틀을 만들 당시의 이력이다.

운영·시설·관객·대기열·SNS·스태프·밀집의 공통 프레임을 셋업 스타일로 맞춘다.
각 화면의 카드·지표 재구성은 이후 단계다. 배치·리포트·피날레 레이아웃은 변경하지 않는다.

## 에디터에서 편집

- `WBP_FF_LiveHudScreen` → Class Defaults → **Panel Layouts**에 원하는 탭을 추가한다.
  **Dock=Center**는 화면 중앙이다. **Width/Height**로 크기, **Offset X/Y**로 위치를 정한다.
  X 양수는 오른쪽, Y 양수는 아래다. 값은 DPI 적용 전 단위다.
  항목이 없으면 **Default Panel Layout**을 사용한다. 기존 저장된 항목과 기본값은 보존한다.
- 작은 창은 표시 크기와 위치만 화면·하단 HUD 안으로 제한한다. 저장된 Width/Height/Offset은 유지한다.
  같은 실행에서 드래그한 위치는 탭을 다시 열어도 유지되지만 에셋에 저장되지는 않는다.
- 각 `WBP_FF_LivePanel...` Designer에서 **Border_PanelRoot**의 Brush/Outline으로 배경과 테두리를 바꾼다.
  **Sky_SizeBox_Header**, **Text_PanelTitle**, **Sky_SizeBox_Close**, **Sky_SizeBox_CloseIcon**으로
  머리글 높이·제목 글꼴·닫기 버튼 크기·X 크기를 조절한다. X는 기준선 오차가 없는 기존 아이콘이다.
- 시설·관객·대기열의 현재 목록은 P3~P5에서 만든 전용 행 WBP를 사용한다. 글꼴·열 폭은 행 WBP Designer, 상태색·동적 문구는 Class Defaults에서 수정한다. P1의 `Dynamic Row Font / Color / Note Color`는 구 WBP의 기본 생성 경로에 해당한다.
- 하단 보유 금화는 두 HUD 모두 **Gold Text Format=`{Amount}`**, 금화 이미지 Tint=흰색으로 원본 금색을 쓴다.
  기존 메뉴/상태 행 높이는 셋업 **32/32**, 라이브 **32/36**을 유지한다.

Compile/Save 후 PIE를 새로 시작하면 편집값이 반영된다.

## 적용·검증 도구

프로젝트 에디터를 닫고 새 C++ 에디터 빌드를 사용한다.

```powershell
& Tools/ui/live_panels_sky/run.ps1 -Mode Inspect
& Tools/ui/live_panels_sky/run.ps1 -Mode Apply
& Tools/ui/live_panels_sky/run.ps1 -Mode Verify
& Tools/ui/live_panels_sky/run.ps1 -Mode Finish
```

- Inspect: 대상 9 WBP의 트리·레이아웃·행 높이·금화 포맷 조사. 에셋 저장 없음.
- Apply: 원본 9파일 SHA 백업 후 `FF.ApplyLivePanelSkyDesign`으로 공통 프레임을 적용한다.
  기존 위젯·본문·GUID·바인딩을 보존하며 신규 위젯 이름은 `Sky_`로 구분한다.
  SNS/밀집의 기존 스크롤·새 글·히트맵 제어는 재사용한다.
  `FF.LivePanelSky.FrameVersion=1` 적용 후 재실행은 패널 Designer 편집을 덮어쓰지 않는다.
  HUD는 금화 포맷·미리보기·Tint만 변경하고 나머지 트리와 Layout을 비교한다.
- Verify: 저장된 에셋 자동화 + 실제 PIE의 세 해상도/7패널/편집·Nudge·재오픈/시계 흐름 확인.
  실제 GameViewportClient의 픽셀 경계와 PNG는 `FF.LivePanelCenterProbe <영문태그>`로 기록한다.
  Live 진입 순간만 필수시설 목록을 메모리에서 비웠다가 즉시 복원하는 표시 검사용 fixture다.
  시설 설치·개장 규칙·물리 마우스·게임패드·패키징 테스트를 대신하지 않는다.
- Finish: P9의 가벼운 읽기 전용 연결 확인. 기존 단계 보고서와 현재 7패널/목록 클래스·Layout·하단 금화 설정을 대조하고 대상 파일이 바뀌지 않았는지 확인한다. 에셋 저장·컴파일·PIE·렌더는 하지 않는다. 결과는 `finish-report.json`이다.

2026-10-01 이후 제작 우선 지시에 따라 보통 수정에서는 **Verify를 반복 실행하지 않는다**. 실제 화면은 사용자 확인 결과를 받아 수정하며 필요한 빌드·위젯 연결만 확인한다.

보고서와 이미지: `Saved/LivePanelsSky/`, 에디터 로그: `Saved/BrightGlobe/live_panels_sky_*.log`.
실행 전 개인 Saved ini/세이브를 백업하고 종료 후 해시를 대조해 복원한다.
전체 HUD 생성 명령으로 재생성할 필요는 없다.

## P1 완료 확인 — 2026-10-01

- UE 5.7.4 에디터 빌드 성공. 대상 9 WBP 적용·저장 및 기존 설정 보존 확인.
- 실제 PIE 1280×720 / 1920×1080 / 2560×1080에서 7패널과 이동·재열기 총 30회 측정 통과.
  Class Defaults의 Staff 크기 680×540 시험, 탭별 위치 독립, 하단 HUD 겹침 방지, Live 시간 흐름 확인.
- 자동화 검사 6개 통과. 검증 전후 보호 파일 582개와 개인 설정·세이브 5파일의 SHA256 일치.
- 최종 검증 에디터 정상 종료(코드 0). `verify-report.json`의 `status=passed`가 최종 근거다.
- P1은 공통 틀과 배치까지다. 내부 카드·지표 재구성은 P2 운영부터 단계별로 진행한다.
  물리 마우스·게임패드 입력과 패키징은 이번 검증에 포함하지 않았다.
