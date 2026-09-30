# 셋업 패널 P6 통합 확인

프로그램·인력·홍보·점검을 실제 축제 PIE에서 검사한다. 배치 화면은 기존 Bottom 도킹을 유지하는지만 확인한다.
물리 마우스·게임패드 조작과 패키징 검증은 이 도구의 범위가 아니다.

## 실행

변경된 C++를 Development Editor로 빌드하고, 작업 중인 에디터를 저장·종료한 후 프로젝트 루트에서 실행한다.

```powershell
& Tools/ui/prep_panels_integration/run.ps1
```

실행기는 개인 Saved 설정과 SaveGames를 매번 새로 백업한다. 일회용 에디터가 종료되면 기존 파일을 복원하고 SHA256으로 확인한다. 테스트가 새로 만든 저장 파일은 검증 폴더로 옮겨 보관한다.
기존 에디터가 실행 중이면 시작하지 않는다. 강제 종료 등으로 복원이 보류되면 `Saved/PrepPanelsIntegration/SessionBefore-*`와 `session-restored.json`을 확인한다.

처음 한 번 `WBP_FF_PrepHudBar`의 `Box_Cta` 상하 여백을 5→2로 보정한다. 메뉴/상태 행 높이 32, 사용자 편집 Panel Layouts와 그 외 위젯 속성은 보존한다. 알려진 값과 다르면 자동 수정하지 않고 실패로 남긴다. 최초 HUD 원본은 `HudCtaBefore-*`에 보관한다.

## 확인 범위와 결과

- 실제 1280×720 / 1920×1080 / 2560×1080 PIE의 네 패널 중앙 위치·닫기·실행 버튼·본문과 고정 헤더/푸터 경계.
- Staff의 Size/Offset 변경, Nudge 경로, 닫기·재오픈 및 다른 탭 설정 보존.
- 배치 Bottom, 셋업·라이브 하단 HUD 렌더, 각 패널의 기존 규칙·작은 크기·긴 문구/폰트 검사.
- 실제 패널 처리 경로를 통한 인력 고용, 홍보 취소/구매/중복 확인 방어, 점검→배치 복귀와 필수 시설 차단.
- 픽셀 캡처는 활성 HUD를 소유한 PIE 월드의 `GameViewportClient`에서 가져온다. 다른 에디터 viewport의 PNG를 통과 근거로 사용하지 않는다.

결과: `Saved/PrepPanelsIntegration/report.json`, PNG/개별 geometry JSON, `hud-cta-fix-report.json`, `session-restored.json`.
에디터 로그: `Saved/BrightGlobe/prep_panels_integration.log`.
원래 P1 실행기는 `Tools/ui/prep_panel_center/verify.py`이며 P6는 실행 순서를 재사용한다.

실패한 UI 동작만 다시 확인할 때는 `run.ps1 -ActionsOnly`를 사용한다. 이 모드는 1920×1080 PIE 한 번과 실제 UI 처리 경로만 실행하며, 3해상도·33개 기존 검사를 다시 수행한 것으로 간주하지 않는다.

2026-09-30 확인: 빌드·기존검사33개, 3해상도24좌표, 실제 UI 처리12개 통과. 통합 근거는 `Saved/PrepPanelsIntegration/summary.json`이다. 최초 홍보 확인창 재열기 검사는 전환 애니메이션 대기가 부족해 실패했으며, 대기를 보완한 동작 재검사에서 통과했다. 게임의 확인창 코드는 변경하지 않았다.

## 에디터에서 수정할 곳

`/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar`

**Class Defaults → Panel Layouts → Program / Staff / Promotion / Check**에서 `Size`와 `Offset`을 개별 수정한다. `Dock=Center`일 때 Offset=(0,0)은 화면 중앙이다. 실행 중 드래그한 위치는 같은 실행에서만 유지된다.

현재 사용자 저장값: Program 1120×950, Staff 820×500, Promotion 1140×620, Check 1140×700. 메뉴/상태 행은 32/32다.
패널 내부 글자·색·버튼은 `/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanel*`의 Designer에서 편집한다. 표시 포맷은 각 Class Defaults를 사용한다.

| 직접 확인 | 통과 기준 | 이상 시 확인 |
| --- | --- | --- |
| 창 크기를 바꾼 뒤 패널 열기 | 닫기·실행 버튼이 보이고 하단 HUD와 겹치지 않음 | 해당 탭 Size/Offset, DPI |
| 제목을 잡아 이동하고 재오픈 | 이동한 위치 유지, 다른 탭은 그대로 | 드래그 대상·탭별 Layout |
| 실제 마우스로 고용/홍보 취소·구매 | 표시 인원·잔액 갱신, 취소는 변화 없음 | 선택/확인창 입력과 결과 이벤트 |
| 점검에서 배치 복귀·개장 | 필수 시설 미충족 시 사유 표시, 충족 후 정상 전환 | 원본 시설 조건·CTA 상태 |
