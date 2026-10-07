# P2 — 왼쪽 위 SNS·음악 도크

왼쪽 위의 원형 버튼으로 SNS와 음악 패널을 연다. 같은 버튼을 다시 누르거나 X로 닫고,
다른 버튼을 누르면 열린 패널을 바꾼다. 동시에 펼쳐지는 패널은 하나다.
SNS는 기존 글·필터·새 글 알림을, 음악은 기존 오디오 시스템의 재생·곡 선택·자동 다음 곡·셔플을 사용한다.
패널을 여닫아도 Live 시간을 멈추거나 재생을 초기화하지 않는다. 진행 막대는 읽기 표시다.

## 에디터에서 수정

도크: `/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveUtilityDock`.

Class Defaults에서 아래 값을 조절한다.

| 설정 | 편집 대상 |
| --- | --- |
| Dock Position | 왼쪽 위 원형 버튼 묶음의 위치(화면 왼쪽 위 기준 X·Y, 기본 24/24) |
| Show Button Labels | 아이콘 아래 `SNS`·`음악` 이름 표시. 기본은 꺼짐(아이콘만), 이름은 툴팁에 남는다 |
| Button Diameter / Button Gap | 버튼 지름과 두 버튼 사이 간격 |
| Panel Offset | 도크 기준 펼침 패널의 위치 |
| Unread As Dot / Unread Dot Size | SNS 버튼 새 글 배지를 숫자 대신 빨간 점으로(기본 끔 = 숫자). 배지 색은 Designer `Border_Unread` |
| Sns Panel Size / Music Panel Size | 각 패널의 폭·높이 |
| Reserved Bottom Space | 하단 HUD와 겹치지 않도록 남기는 공간 |
| Sns Panel Class / Music Panel Class | 연결할 SNS·음악 패널 WBP |

버튼 배경·테두리·마우스 반응색은 Designer의 `Button_SNS`, `Button_Music` 스타일에서 수정한다.
2026-10-06부터 평소 배경은 그리지 않고(흰 원 제거) 마우스·누름에만 옅은 어두운 원이 보인다.
아이콘(`Image_SNS`, `Image_Music`)과 글자는 흰색+그림자(`Image_*Shadow`, 글자 Shadow)로 월드 위에 뜬다.
아이콘·글자 색은 Class Defaults의 `Icon Color`(평소)·`Selected Icon Color`(열린 패널, 민트)가 Designer 색보다 우선한다.
예전 ✓(`Text_SNSSelected`, `Text_MusicSelected`)와 `♫` 글자(`Text_MusicIcon`)는 접혀 있고 쓰지 않는다.
음표 텍스처는 `Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_Music_White_256`(자체 제작, `Tools/ui/generate_live_hud_music_icon.js`).
새 글 숫자의 글꼴과 여백도 Designer에서 편집한다.

### 펼침 패널 (2026-10-07 승인안)

`FF.ApplyLiveFlyoutRedesign`(`apply_flyout_redesign.py`)으로 SNS·음악 패널을 승인 시안 모양으로 다시 짰다.
도크 버튼(아이콘)·하단 HUD·기존 하단 SNS 패널은 바꾸지 않았다. 원본은 `Saved/UIBackups/LiveFlyoutRedesign/`.
다시 실행해도 이미 새 구조(`Underline_FilterAll`·`Image_RepeatOne`·`Border_Row`·`Image_Avatar`)인 WBP는 건너뛴다 — Designer 편집이 남는다.
런타임은 색·보이기·글자만 바꾸고 모양·크기·글꼴·여백은 Designer 값을 쓴다.

- SNS 패널 `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyout`
  - Designer: 제목(`Text_PanelTitle` 21)·부제(13)·닫기, 탭(`Button_Filter*` 안 `Text_Filter*` 15·밑줄 `Underline_Filter*`),
    `Text_Count`(12), 새 글 버튼(`Button_NewPosts` 민트 면·`SizeBox_NewPosts` 높이), 목록, 아래 안내 글.
  - Class Defaults `SNS|Style`: `Selected/Unselected Filter Text Color`, `Selected Filter Typeface`(선택 탭 굵기, 기본 SemiBold),
    `Tint Filter Background`(끔 — 켜면 예전처럼 선택 탭 바탕을 칠한다), `Post Spacing`(0, 행이 구분선을 가진다).
  - Class Defaults `SNS|Text`: 탭 이름(`Filter Labels`)·`Count Text Format`·`New Posts Text Format`·빈 목록 문구.
  - Class Defaults `SNS|NewPosts` (2026-10-07): `Auto Show New Posts`(켬 — 새 글이 버튼 없이 바로 목록 맨 위에 들어간다. 끄면 예전 `새 글 N개 보기` 버튼),
    `New Mark Clear`(빨간 점을 `패널을 닫을 때` 또는 `보이고 몇 초 뒤` 지움), `New Mark Seconds`, `Keep Reading Offset`(이만큼 내려 읽는 중이면 새 글이 와도 보던 글을 제자리에 둠).
- SNS 글 행 `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyoutPostRow` (새 에셋, 부모는 기존 행과 같은 `UFF_LiveSnsPostEntry`)
  - Designer: 원형 프로필(`Border_Avatar`·`Image_Avatar`), 이름 `Text_Author`(13 SemiBold), 반응 `Image_Sentiment`+`Text_Sentiment`(12),
    본문 `Text_Body`(14, 자동 줄바꿈), 위아래 여백 `Border_Card` Padding, 구분선 `Image_Divider`,
    새 글 빨간 점 `Image_NewDot`(색은 브러시, 크기는 `SizeBox_NewDot` 7, 이름과 간격은 슬롯 왼쪽 여백 6).
  - Class Defaults: `Sentiment Icons`(반응별 아이콘), `Positive/Negative/Neutral Color`, `Tint Sentiment Icon`, `Sentiment Labels`.
  - 기존 카드형 `WBP_FF_LiveSnsPostEntry`는 그대로 남아 하단 SNS 패널이 쓴다.
- 음악 패널 `/Game/FestaFiesta/UI/Audio/WBP_FF_PlaylistFlyout`
  - Designer: 제목·닫기, 음표 타일(`SizeBox_Cover` 72), 현재 곡 제목(`Text_TrackTitle` 17, `SizeBox_TrackTitle` 최대 높이 48 = 두 줄),
    진행 막대(`SizeBox_Progress` 4), 조작 한 줄(`HorizontalBox_Controls`: 셔플·이전·재생(`Button_PlayPause_Size` 52)·다음·반복),
    켜짐 점 `Dot_Shuffle`/`Dot_Repeat`, 키보드 설명 `Text_ControlHint`, 목록 제목·`Text_ListCount`.
  - Class Defaults `Audio|Style`: `Toggle Active/Inactive Icon Color`, `Show Toggle State By Button Style`(끔 — 아이콘 색+점으로 표시).
  - Class Defaults `Audio|Text`: 셔플·반복·재생 툴팁(`Shuffle On/Off Tooltip`, `Repeat One Tooltip`, `Auto Advance Tooltip`, `Play/Pause Tooltip`),
    `List Count Format`, 재생 상태 문구. `Audio|Layout → Track Row Slot Padding`(곡 행 사이 여백).
  - 반복 버튼: 자동 다음 곡이면 `Image_Repeat`(옅은 색), 한 곡 반복이면 `Image_RepeatOne`(안에 1, 초록+점).
    마우스는 툴팁, 키보드 포커스는 조작 줄 아래 `Text_ControlHint`에 같은 설명이 나온다.
- 곡 행 `/Game/FestaFiesta/UI/Audio/WBP_FF_PlaylistTrackRow`
  - Designer: 높이 `SizeBox_Track`(42), 바탕 `Border_Row`(둥글기), 번호/이퀄라이저 칸 `SizeBox_Index`(30), 제목 `Text_Title`(14),
    상태 칸 `SizeBox_State`(54, `재생 중`·재생 삼각형 `Image_PlayHint`), 강조선 `Image_Accent`, 구분선 `Image_Divider`.
  - Class Defaults: `Normal/Hover/Current Background Color`, `Normal/Current Text Color`, `Current Title Typeface`,
    `Playing/Paused Text`, `Play/Current Tooltip Format`.
- 스크롤바: 두 패널 Class Defaults의 `Scroll Style`(아래 공용 스크롤바 참고).
- 새 아이콘 8종(자체 제작, `generate_flyout_icons.js`): `Icons/Actions/T_FF_LH_Icon_{Shuffle,Repeat,RepeatOne,Equalizer,PlayFill,PauseFill,NextFill,PreviousFill}_White_256`.

### 공용 스크롤바

셋업·라이브 패널(배치 패널 제외), 음악 패널, 최종 결산은 각 WBP Class Defaults의 `Scroll Style`을 쓴다.
`Thickness`(보이는 폭 8), `Grab Width`(마우스로 잡히는 폭 14, 양옆은 투명), `Corner Radius`(끝 둥글기 3),
`Padding`(왼쪽이 목록과의 간격), `Track/Thumb/Thumb Hovered/Thumb Dragged Color`. `Apply`를 끄면 ScrollBox Designer 스타일을 쓴다.
패널이 처음 만들어질 때 한 번 적용하고 이후에는 덮어쓰지 않는다.

새 WBP의 필수 이름은 `apply.py`의 `REQUIRED` 목록에 있다. 이름을 바꾸면 C++ 바인딩도 수정해야 한다.
Compile/Save 후 새 실행에서 확인한다. 기존 셋업·라이브 HUD의 행 높이, 위치, 탭 설정은 수정하지 않는다.

## 적용

최신 C++ 에디터 빌드와 도크 클래스의 Audio Settings 연결을 반영하고, 열린 에디터를 종료한 뒤 실행한다.

```powershell
& Tools/ui/live_utility_dock/run.ps1
```

도구는 다음 명령을 순서대로 실행한다.

1. `FF.LiveSnsFlyoutSky`
2. `FF.PlaylistFlyoutSky`
3. `FF.LiveUtilityDockSky` (끝에 `FF.ApplyLiveDockIconDesign`을 이어서 부른다)

이미 만든 WBP에 아이콘 정리만 적용할 때는 `apply_dock_icons.py`(음표 텍스처 임포트 → `FF.ApplyLiveDockIconDesign`)를 쓴다.
두 WBP에 `Image_Music`/`Image_TitleIcon`이 있으면 다시 적용하지 않고 Designer 편집을 보존한다.

신규 4개 WBP의 컴파일·부모 클래스·필수 바인딩·연결만 확인한다.
기존 HUD, SNS 패널·글 카드, 독립 음악 플레이어의 파일과 두 HUD의 탭 레이아웃이 그대로인지 확인한다.
기존 HUD·맵을 저장하거나 PIE·렌더·전체 테스트를 실행하지 않는다.

일회용 에디터 실행 전 개인 설정·저장 파일을 백업하고 종료 후 복원한다.
백업과 적용 보고서는 `Saved/LiveUtilityDock/`, 실행 로그는 `Saved/BrightGlobe/live_utility_dock_apply.log`에 남는다.
저장 후 2초간 초기화 틱을 기다린 뒤 정상 종료한다.

## 연결과 사용자 확인

`Audio Settings → Playlist Player Widget Class`가 새 도크를 참조한다.
도크 안에서 음악 패널을 만들므로 기존 독립 플레이어와 중복 표시하지 않는다.
오디오 원본과 기존 재생 상태를 계속 사용하며 하단 SNS 진입점은 도크가 활성화된 동안 중복 표시하지 않는다.

- SNS·음악 버튼을 각각 열고 같은 버튼·X·다른 버튼으로 닫기와 전환을 확인한다.
- SNS 새 글·필터·목록 스크롤, 음악 재생/일시정지·이전/다음·곡 선택·자동 다음 곡·셔플을 확인한다.
- 음악 패널을 닫았다 다시 열어도 재생 위치가 이어지고, 패널을 연 동안 Live 시간이 흐르는지 확인한다.
- 작은 창에서 하단 HUD를 가리지 않는지, 위치·크기·글꼴을 편집한 값이 다음 실행에도 유지되는지 확인한다.
