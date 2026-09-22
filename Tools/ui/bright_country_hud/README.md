# 밝은 한국 지도 · 하단 HUD

승인된 평면 한국 지도와 직각 하단 HUD를 기존 UI에 적용하는 도구다.
새 지리를 그리지 않고 기존 2400×2160 지도 알파·DEM 데이터를 그대로 사용한다.

## 범위

- 국가 지도 / 지도 핀 / 시작·뒤로 버튼 WBP 4개와 전용 CommonButton 스타일
- 준비 HUD / 탭 WBP 2개
- 한국 전용 표시 재질과 배경 재질. 다른 국가는 기존 MapImage로 돌아간다.
- 국가 카탈로그, 레벨, 시설·프로그램 패널은 적용 대상이 아니다.

## 적용

에디터를 닫고 Development Editor를 빌드한 다음 다음 명령으로 최초 적용한다.

```powershell
& 'Tools/ui/blender/bright_globe/run_editor.ps1' -Script 'Tools/ui/bright_country_hud/apply.py' -Label 'bright_country_hud_apply'
```

기존 대상 에셋은 `Saved/BrightCountryHud/backup-*`에 백업한다.
이미 성공한 적용은 다시 실행하지 않는다. 에디터에서 수정한 UI가 초기화되는 것을 방지하기 위함이다.
재질 생성기도 기존 재질/인스턴스를 덮지 않는다.

## 에디터에서 조절

`/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar`

| 대상 | 수정 위치 |
|---|---|
| 상태 바 높이 | Designer → SizeBox_StatusRow → Height Override (기본 52) |
| 탭 줄 높이 | Designer → SizeBox_MenuRow → Height Override (기본 44) |
| 상태 글자 크기 | Text_FestivalName / Text_Gold / Text_Staff / Text_DayStage → Font |
| 좌우 여백 | Border_StatusRow → Padding |
| 탭 이름·아이콘 | 각 Tab_* → Display Label Override / Designer Icon |
| 동적 표시 형식 | Class Defaults → Gold Text Format / Staff Text Format / Day Stage Text Format |

탭 하나의 기본 폭/글꼴/아이콘 크기는 같은 폴더의 `WBP_FF_PrepHudTabButton`에서
`SizeBox_Tab`의 Min Desired Width, `Text_Label`의 Font, `Image_Icon`의 Desired Size Override를 바꾼다.
`WBP_FF_PrepHudBar`의 Class Defaults → `Use Designer Style`은 켜 둔다. 두 줄은 VerticalBox 안에 있고 전체 바는 하단에 고정되어 있어
높이를 바꿀 때 Y 위치를 따로 맞출 필요가 없다.

`/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasCountry_Natural`

- 제목: Text_MapHeading, Text_MapSubtitle
- 카드 폭/프리뷰/버튼 높이: SizeBox_Card, SizeBox_Preview, SizeBox_StartFestival
- 시작 버튼 높이는 Button_StartFestival → Button Size Override.Y도 함께 맞춘다(기본 58).
- 지도 영역: CanvasPanel_Map 여백. 지도/경계/선택 레이어는 같은 영역으로 자동 배치된다.
- Class Defaults의 Country Map Display Materials에서 KR의 표시 재질을 지정한다.
- `Use Designer Text And Colors`를 켜면 고정 문구와 색을 실행 때 덮어쓰지 않는다.

`/Game/FestaFiesta/UI/Atlas/Korea/CleanBright/MI_KoreaCleanBrightMap`의 LowlandColor,
HighlandColor, ReliefStrength 등으로 지도 색상/지형 명암을 조절한다.
`MI_KoreaCleanBrightBackdrop`의 CreamColor, WaterColor, WaterStrength로 배경을 조절한다.
원본 지도 알파와 축제 위경도 데이터는 변경하지 않는다.

## 확인

- 실제 HUD 배치 테스트: `FestaFiesta.UI.PrepHUD.BrightBottom.RenderedLayout` (real RHI 필요)
- 캡처: `Saved/BrightCountryHud/Layout/`
- 실제 국가 지도→축제 HUD 캡처: `capture.py`, 결과 `Saved/BrightCountryHud/Runtime/`
- 캡처는 키보드/마우스 물리 입력 검증을 대신하지 않는다.
