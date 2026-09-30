# 홍보 구매 확인창 · 닫기 아이콘

프로그램·인력·홍보의 기존 Designer 편집을 유지하면서 닫기 X만 중앙 정렬 이미지로 바꾼다. 홍보 구매는 전용 밝은 확인창을 사용한다. 다른 화면의 공용 확인창은 변경하지 않는다.

## 에디터 수정

- 확인창: `/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepConfirmSky`
  - `SizeBox_Dialog`: 폭(기본640). 높이는 내용에 맞춘다.
  - `Overlay_Root` 아래 `SizeBox_Dialog`의 슬롯: 중앙 정렬·여백.
  - `Border_Dialog`: 배경·테두리·안쪽 여백.
  - `Text_Title`, `Text_Body`: 글자 크기·색.
- 수치 행: `WBP_FF_PrepConfirmSkyRow`의 `Text_Label`, `Text_Value`.
- 버튼: `WBP_FF_PrepConfirmSkyCancel/Accept`의 `SizeBox_Root`(높이46), 색은 `Styles/CBS_FF_PrepConfirmSkyCancel/Accept`, 글자 스타일은 `Styles/CTS_FF_PrepConfirmSkyInk/Light`.
- 홍보 패널 Class Defaults의 `Confirm Title Format`: `{0}`은 선택한 등급명으로 바뀐다. 백틱으로 감싸면 Unreal 포맷 이스케이프가 되므로 사용하지 않는다.
- 프로그램·인력·홍보 `Button_Close > SizeBox_CloseIcon > Image_Close`: 버튼36, 아이콘18. 각각 Designer에서 수정 가능하며 런타임이 치수를 덮어쓰지 않는다.
- 전체 프로그램 패널 높이는 HUD `Panel Layouts > Program > Size Y`의 사용자 지정950을 유지한다. 화면이 작으면 기존 가용 영역 제한과 본문 스크롤을 따른다.

## 적용·검사

`FF.FixPrepSkyCloseIcons`는 세 패널의 기존 글자 X만 바꾸며 이미 이미지이면 재생성하지 않는다. `FF.ApplyPrepConfirmSkyDesign`은 확인창 최초 생성 전용이다.

`apply_and_verify.py`는 대상 에셋 백업, 닫기 외 트리 보존, 프로그램 높이950·나머지 레이아웃 보존, 전용 확인창 연결 후 자동검사를 수행한다. 결과는 `Saved/PrepSkyCorrections/report.json`, 이미지는 `Layout/`에 남는다. 표시 검사와 실제 PIE 구매 조작은 별도다.
