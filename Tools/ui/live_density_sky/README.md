# 라이브 P8 밀집 UI

셋업과 같은 흰 바탕·하늘색 테두리·짙은 초록 글자로 최대 밀도, 혼잡한 칸, 집계 단위를 표시한다. 헤더와 요약 카드, 하단 히트맵 버튼은 고정하고 범례만 스크롤한다.

## 에디터에서 수정하기

| 바꿀 항목 | 에디터 위치 |
| --- | --- |
| 패널 위치·전체 크기 | `WBP_FF_LiveHudScreen → Class Defaults → Panel Layouts → Density`의 `Dock / Offset / Width / Height`. 최초값 Center, Offset0, 1100×760 |
| 헤더 높이·아이콘 | `WBP_FF_LivePanelDensity → Designer → Sky_SizeBox_Header`(80), `Density_SizeBox_TitleIcon`(32×32) |
| 요약 카드 크기 | `Density_Peak_Size`, `Density_Congested_Size`, `Density_Cell_Size`: 폭340·최소높이132 |
| 카드 간격 | `Density_Metrics → Inner Slot Padding`: 12×12 |
| 값 글꼴·색 | `Text_Peak`, `Text_PeakGrade`, `Text_Congested`, `Text_CellSize`, `Text_CellArea` |
| 범례 스크롤 | `Scroll_Density`. 상단 카드와 하단 버튼은 이 위젯 밖에 있음 |
| 범례 행 높이·여백 | `WBP_FF_LiveDensityLegendRow → Designer → SizeBox_Row` 최소높이54, `Border_Row → Padding` |
| 등급 칸 너비 | 범례 행의 `SizeBox_Grade`(190), `SizeBox_CurrentPeak`(104) |
| 범례 글꼴 | 범례 행의 `Text_Grade`(18), `Text_Range`(16), `Text_CurrentPeak`(14) |
| 범례 행 배경·현재 최대 강조 배경 | 범례 행 `Class Defaults → Density`의 `Row Background Color / Peak Background Color` |
| 범례 행 사이 간격·클래스 | 밀집 패널 `Class Defaults → Density`의 `Legend Row Padding / Legend Row Widget Class` |
| 동적 문구 | 밀집 패널과 범례 행의 `Class Defaults → Density` 아래 텍스트 포맷·안내 |
| 히트맵 버튼 크기·색 | `Density_SizeBox_Toggle`(190×46), `Button_Toggle → Style` |
| 하단 안내 | `Text_HeatmapStatus`, `Text_MissingNote`, `Density_FooterBorder` |

기본 범례 행: `/Game/FestaFiesta/UI/LiveHUD/Rows/WBP_FF_LiveDensityLegendRow`.
Compile·Save 후 다시 열면 적용된다. 실행 코드가 글꼴·크기를 초기값으로 덮어쓰지 않는다. 동적으로 변하는 문구는 Designer의 예시 Text 대신 Class Defaults의 포맷을 수정한다. 범례 색상 칩은 바닥 히트맵과 일치하도록 군중 시스템의 색을 사용한다.

## 표시 의미와 보존 범위

- 최대 밀도·혼잡 칸 수는 기존 Crowd 집계값이다. 혼잡 칸 수를 D등급 칸 수로 바꾸거나 설명하지 않는다.
- 등급 구간은 기존 `FFF_DensityRules::BuildLegend()`가 `DensityToLOS()`에서 읽고, 색은 `LOSToColor()`에서 읽는다. UI에 구간 상수를 복제하지 않는다.
- 칸 크기와 면적도 Grid의 실제 값을 사용한다. 2m·4㎡로 고정하지 않는다.
- 격자 미생성/원본 없음은 `—`와 안내로 표시하며 실제 0과 구분한다. 현재 최대 등급은 배경뿐 아니라 `현재 최대` 문구를 함께 표시한다.
- 히트맵은 기존 개발용 바닥 표시다. 지원 빌드와 준비된 격자에서만 버튼이 활성화된다. 미지원 빌드에서도 밀도 숫자·범례를 확인할 수 있다.
- 패널을 닫아도 사용자가 켠 히트맵을 끄지 않는다. Live 시간이나 배속 정책을 바꾸지 않는다.
- 다른 6개 탭 Layout·하단 HUD 크기·원본 군중 로직은 보존한다.

## 적용 도구

닫힌 에디터에서 `Tools/ui/live_density_sky/run.ps1` 실행. C++ 빌드 후 사용한다.
`FF.ApplyLiveDensitySkyDesign`은 최초 한 번 기존 밀집 위젯을 재배치하고 범례 행을 생성한다. 적용 메타데이터가 있으면 Designer 수정 보존을 위해 재생성하지 않는다.
`apply.py`는 범례 행 클래스 연결과 Density Layout을 추가한다. 기존 Layout 값이 있으면 덮어쓰지 않는다.

- 결과: `Saved/LiveDensitySky/apply-report.json`
- 에셋 백업: `Saved/LiveDensitySky/Before-*`
- 개인 설정·세이브 복원: `Saved/LiveDensitySky/apply-session-restored.json`

## 사용자 화면 확인

제작 단계의 자동 확인은 빌드·WBP 컴파일/저장·필수 위젯 연결까지만 한다. 아래 항목은 실제 화면에서 확인한다.

- 제목·요약 카드가 겹치지 않고 마지막 범례 행을 스크롤로 볼 수 있는지.
- 현재 최대 한 행에 강조 문구가 보이며 색상과 등급·수치가 함께 읽히는지.
- 격자가 없는 경우와 실제 최대0/혼잡0의 표시가 다른지.
- 히트맵 켜기/끄기, 닫은 뒤 표시 유지, 미지원 안내가 맞는지.
- `Panel Layouts → Density` 편집값이 재열기에도 반영되는지.
