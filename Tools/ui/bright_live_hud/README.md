# 밝은 라이브 하단 HUD 적용

사용자가 승인한 셋업 HUD 스타일을 라이브 HUD에 적용하는 일회성 도구다.

- `apply.py`: 라이브 HUD와 탭 WBP 2개를 백업한 뒤 `FF.ApplyLiveHudBrightBottomDesign` 실행.
- 적용 전후 패널 설정과 셋업 HUD·다른 라이브 패널의 파일 해시를 비교한다.
- 성공 보고서가 있으면 재적용을 거부한다. 이후 크기와 문구는 UMG Designer에서 바꾼다.
- `verify.py`: 3개 화면비와 행 높이 변경을 실제 Slate로 그린 뒤 PIE의 Live·SNS·배속·피날레 표시를 확인한다.
- PIE 진입 시 필수시설 조건을 메모리에서 잠시 비우고 즉시 복원하는 UI 전용 검증이다. 게임 진행 조건이나 건축 기능의 수용 테스트가 아니다.
- 물리 마우스·게임패드 검증과 패키지 검증은 포함하지 않는다.

## 실행

에디터를 닫고 Development Editor 빌드 후 프로젝트 루트에서 실행한다.

```powershell
& Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/bright_live_hud/apply.py -Label bright_live_hud_apply -NullRHI
& Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/bright_live_hud/verify.py -Label bright_live_hud_verify
```

결과는 `Saved/BrightLiveHud/`에 저장한다. 검증 시 생성되는 개인 에디터 설정은 실행 전 사본으로 복원한다.

## 편집 위치

`/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen`

- `SizeBox_MenuRow`: Height Override **32**
- `SizeBox_StatusRow`: Height Override **36**
- `Border_StatusRow`: 직각 흰 바, Padding
- `Text_FestivalName`, `Text_Gold`, `Text_Staff` 등: Font Size
- 각 `Tab_*`: Display Label Override, Designer Icon
- Class Defaults → FestaFiesta → UI → Live → Text: 숫자 표시 형식
- `Button_Speed1/2/4`: Normal·Hovered·Pressed 브러시. Pressed가 선택 배속 표시에도 쓰인다.

`WBP_FF_LiveTabButton`의 `SizeBox_Tab`에서 최소 폭, `Text_Label`에서 글꼴 크기를 변경한다.
하단 위치는 자동 크기와 아래쪽 앵커로 유지한다. 패널 도킹도 상태 행의 현재 높이를 읽는다.
좌우 정보는 작은 창에서만 각각 축소해 가운데 일차 표시와 겹치지 않게 한다.
