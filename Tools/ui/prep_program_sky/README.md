# 프로그램 패널 — 흰색·하늘색 UI

P3 범위: 프로그램 카드, 운영 시설 선택, 편성 요약. 기존 ViewModel의 시설 GUID와 편성 명령·결과 이벤트를 사용한다.
배치·인력·홍보·점검·라이브 에셋은 변경하지 않는다.

## 위치·전체 크기

콘텐츠 브라우저에서 `/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar`:

**Class Defaults → Panel Layouts → Program**

- `Dock = Center`: 화면 중앙.
- `Offset X / Y`: 중앙에서 이동할 거리. X 양수는 오른쪽, Y 양수는 아래.
- `Size X / Y`: 패널 폭·높이. 이번 기본값은 **1120×600**이며 저장한 값은 다음 PIE에서 반영한다.
- 인력의 `Staff` 항목은 별개다. 사용자가 변경한 높이 500은 보존한다.
- 본문이 높이를 초과하면 스크롤된다. 제목·닫기·편성 버튼은 고정 영역에 남는다.

## 패널 안의 글자·칸·버튼

`/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelProgram`의 Designer:

| 대상 | 수정 위치 |
| --- | --- |
| 전체 배경·테두리 | `Border_PanelRoot → Brush / Outline Settings` |
| 제목 높이·글꼴 | `SizeBox_Header`, `Text_PanelTitle → Font` |
| 카드와 요약 사이 간격 | `VerticalBox_Catalog → Slot → Padding` |
| 오른쪽 요약 폭 | `SizeBox_Summary → Width Override` |
| 요약 배경 | `Border_Summary → Brush / Padding` |
| 시설 드롭다운 | `Combo_Facilities → Font / Foreground Color / Style / Item Style / Content Padding` |
| 실행 버튼 크기 | `SizeBox_ScheduleAction → Min Desired Width / Height` |
| 실행 버튼 색·선 | `Button_Schedule → Style → Normal / Hovered / Pressed / Disabled` |
| 안내·요약 글자 | 해당 `Text_* → Font / Color / Slot Padding` |

Class Defaults의 `FestaFiesta → UI → Prep → Program`에서 `Daily Notice Text`,
그 아래 `Text`에서 `No Selection Text`, `Day Text Format` (`{0}` = 일차),
`Empty Catalog Text`, `Facility Placeholder Text`, `No Facility Text`를 수정한다.
선택한 프로그램명·금액·시설명은 실제 데이터로 갱신되므로 Designer 예시 문구를 게임 데이터로 쓰지 않는다.

## 프로그램 카드

`/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCard`:

- `SizeBox_Card`: 폭 / 최소 높이. 긴 이름은 줄바꿈하며 카드 자체도 늘어날 수 있다.
- `Button_Root → Style`: Normal은 기본, Hovered는 마우스, **Pressed는 선택 상태**다.
- Class Defaults의 **Tint → Normal Content Color / Selected Content Color / Disabled Content Color**가 상태별 글자색이다.
  카드 텍스트의 Designer 색만 바꾸면 상태 갱신 때 이 값으로 돌아오므로 이 항목을 수정한다.
- `Text_Label / Text_TimeRange / Text_Capacity / Text_Cost / Text_Badge`에서 글꼴과 여백을 편집한다.
- 선택은 체크 문구·두꺼운 선·배경으로 함께 표시한다. 시설을 고르기 전에는 최대 매출을 확정하지 않는다.
- 운영 가능한 시설이 없으면 설치 가능한 시설을 안내하고 기존 건축목록으로 연결한다.
  전용 안내 버튼은 `WBP_FF_PrepProgramFacilityChoice`이며 다른 패널의 공유 버튼과 분리되어 있다.

## 적용·검증 도구

일반 편집은 WBP를 수정하고 Compile / Save하면 된다. 전체 이전 HUD 생성 명령을 다시 실행하지 않는다.

최초 적용 명령은 `FF.ApplyPrepProgramSkyDesign`이다. 이미 `Combo_Facilities`가 있으면 트리를 다시 만들지 않는다.
`apply_and_verify.py`는 첫 적용 전에 원본 에셋을 `Saved/PrepProgramSky/Before-*`로 백업하고,
프로그램 Size 외의 Panel Layouts·HUD 트리 및 인력 등 보호 에셋 해시를 비교한다.
이미 적용된 에셋의 재실행은 Designer 편집을 보존하며 검사만 수행한다.

빌드 후 에디터를 닫은 상태에서:

```powershell
& Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/prep_program_sky/apply_and_verify.py -Label prep_program_sky
```

결과: `Saved/PrepProgramSky/report.json`, `Saved/PrepProgramSky/Layout/Program_*.png`.
레이아웃 PNG는 저장 WBP에 표시 데이터를 넣은 검사 화면이며 전체 게임의 물리 입력 검증과는 구분한다.

## 게임에서 확인할 항목

1. 프로그램 탭을 열면 중앙에 표시되고 닫기·다른 탭 전환이 동작한다.
2. 카드 선택 시 체크·선·선택명·비용이 같이 바뀐다. 다시 누르면 선택이 해제된다.
3. 시설 하나는 자동 선택, 여러 시설은 드롭다운으로 고른다. 시설 선택 전 최대 매출은 미확정 안내다.
4. 편성 후 실제 잔액·편성 상태가 갱신되고, 편성 불가 시 버튼과 사유가 함께 보인다.
5. `Program → Size / Offset`을 바꾸어도 Staff 높이 500과 하단 HUD 치수가 유지된다.
