# 흰 HUD 탭 + 하늘색 테두리

셋업과 라이브의 메뉴 탭에 적용한다. 상태 바·버튼 폭·행 높이·문구·아이콘은 그대로 유지한다.
셋업은 높이32/36에서 글자가 눌리지 않도록 탭/단계 표시의 세로 여백만 줄였다.
런타임에 고정 색상이나 크기를 덮는 코드를 추가하지 않는다.

## 에디터에서 수정

공통 탭 에셋을 열고 **Designer**에서 수정한다.

- 셋업: `/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudTabButton`
- 라이브: `/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveTabButton`

| 수정할 항목 | 위치 |
| --- | --- |
| 버튼 최소 폭 | `SizeBox_Tab → Min Desired Width` (셋업 132 / 라이브 108) |
| 버튼 고정 폭 | `SizeBox_Tab → Width Override` 활성화 후 조절 (문구 잘림 주의) |
| 내용 여백 | `Button_Root → Style → Normal Padding / Pressed Padding` |
| 기본 테두리 | `Button_Root → Style → Normal → Outline Settings → Outline / Width` |
| 마우스 올림 | `Hovered → Outline Settings → Outline / Width` |
| 선택·누름 | `Pressed → Outline Settings → Outline / Width` |
| 흰색 채움 | 각 상태 브러시의 `Tint` |
| 글자 크기 | `Text_Label → Font → Size` |
| 글자·아이콘 색 | Class Defaults → `Normal Content Color / Selected Content Color` |

기본 테두리 sRGB `#A5CDDF` / 1.25, 호버 `#80BDD5` / 1.75, 선택 `#65ABC9` / 2.5.
선택은 색뿐 아니라 두께도 달라진다. `Pressed` 브러시는 패널이 열린 선택 상태에도 쓰인다.
`Draw As = Rounded Box`, `Rounding Type = Fixed Radius`를 유지한다.

**행 높이**는 부모 에셋에서 바꾼다.

- 셋업 부모: 같은 폴더의 `WBP_FF_PrepHudBar`
- 라이브 부모: 같은 폴더의 `WBP_FF_LiveHudScreen`
- `SizeBox_MenuRow → Height Override`: 현재 32
- `SizeBox_StatusRow → Height Override`: 현재 36
- 높이를 늘리면 글자/아이콘은 자동 확대되지 않으므로 별도로 조절한다.
- 셋업 부모의 `Use Designer Style`은 켜 둔다.
- 수정 후 탭 에셋과 부모 HUD를 Compile / Save한다. 기존 `FF.Apply…BrightBottomDesign`을 재실행하지 않는다.

## 최초 적용 도구

`apply.py`는 기존 WBP의 스타일 속성만 바꾸고 저장한다. 전체 위젯 트리를 재생성하지 않는다.
변경 전 4개 에셋을 `Saved/HudSkyOutline/Before-*`에 복사하고, 전후 치수/문구/슬롯을 비교한다.
성공 후 재실행은 막아 디자이너의 후속 수정을 보호한다.
`finish_spacing.py`는 후속 검사에서 확인한 셋업의 예전 44/52용 여백을 32/36에 맞춘다.
탭 슬롯의 위쪽3→0, 버튼 내용 위아래3→2, 단계 칩 위아래5→2. 가로 여백과 글꼴은 유지한다.

```powershell
& Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/hud_sky_outline/apply.py -Label hud_sky_outline_apply -NullRHI
& Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/hud_sky_outline/finish_spacing.py -Label hud_sky_outline_spacing -NullRHI
```

## 화면 확인

| 확인 | 통과 기준 | 실패 시 확인 위치 |
| --- | --- | --- |
| 평상시·호버·선택 | 흰 바탕/짙은 글자, 하늘색 선이 점차 두꺼워짐 | Button_Root Style / Selected Content Color |
| 패널 닫기 | 선택 해제 후 얇은 기본 테두리 복원 | 탭 재컴파일 후 PIE 재시작 |
| 크기 편집 | 폭/높이 변경이 실행 화면에도 유지 | SizeBox_Tab / SizeBox_MenuRow, 부모 HUD 컴파일 |

2026-09-28 검증: WBP 4개 컴파일·저장 성공. 기존 Prep/Live RenderedLayout 검사
(1280×720, 1920×1080, 2560×1080 및 행 높이 변경) 통과.
실제 PIE에서 셋업 프로그램/라이브 SNS 선택·해제 시 흰 채움, 짙은 글자·아이콘,
2.5→1.25 테두리 복귀를 확인했다. 물리 마우스·게임패드 입력과 패키징은 이번 확인에 포함하지 않았다.
