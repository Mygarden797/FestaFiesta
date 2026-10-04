# 라이브 P6 — SNS 화면

셋업과 같은 흰 바탕·하늘색 테두리·짙은 초록 글자를 사용한다.
제목·필터·새 글 버튼·하단 안내는 고정하고 글 목록만 스크롤한다.
작성자·본문·반응 방향은 기존 ViewModel에서 읽는다.

## 에디터 편집

- 위치·전체 크기: `WBP_FF_LiveHudScreen → Class Defaults → Panel Layouts → SNS`.
  최초960×700, Center/Offset0. Width/Height로 크기, Offset X/Y로 위치를 조절한다.
- 패널: `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelSNS`.
  `Sky_SizeBox_Header` 높이84, 제목26·설명16. 기존 닫기 아이콘은 그대로 사용한다.
- 필터: `Sns_SizeBox_Filter0/1/2`의 폭132·높이44. 글꼴17은 각 `Text_Filter*`.
  테두리·호버는 각 `Button_Filter* → Style`, 선택/기본 면색과 글자색은 패널 Class Defaults의
  `Selected/Unselected Filter Background Color`, `Selected/Unselected Filter Text Color`.
  필터 면색은 버튼 브러시와 곱해지므로 기본 브러시 면을 흰색으로 유지하면 지정색 그대로 보인다.
- 새 글 버튼: `Sns_SizeBox_NewPosts` 높이38, `Button_NewPosts` 스타일과 `Text_NewPosts` 글꼴.
  새 글이 없어도 이 공간을 예약하여 글 목록이 움직이지 않게 한다.
- 글 카드: `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsPostEntry`.
  `Sns_SizeBox_Card` 최소높이100, `Border_Card` 테두리/패딩20·16.
  `SizeBox_Avatar`56×56, `Sns_Border_Avatar` 배경, `Sns_Image_Avatar` 공용 사람 아이콘.
  `Text_Author`20, `Text_Body`18, 본문은 자동 줄바꿈한다.
- 배지: `Border_Sentiment` 여백·모서리, `Text_Sentiment`15·`Text_SentimentSymbol`17.
  반응별 문구/기호/배경색은 카드 Class Defaults의 `Sentiment Labels/Symbols/Background Colors`.
  글자색은 `Positive/Negative/Neutral/Unknown Color`.
- 표시 한도·행 간격(`Post Spacing`, 기본10)·필터 이름·선택/글 수/새 글 문구·빈 상태 안내는 패널 Class Defaults에서 수정한다.
  기존 `Display Limit`을 보존한다(기본30, 원본 보관 한도80).

Compile/Save 후 새 PIE에서 확인한다. 이후 실행 때 폰트·크기·배치를 코드로 덮어쓰지 않는다.
기존 장식은 `Sns_LegacyDecorations`, 기존 아바타 조각은 `Overlay_Avatar` 아래 접어 보관한다.

## 유지한 동작

- 전체/긍정/아쉬움 필터, 중립/미확인 반응은 전체에서 표시.
- 새 글 도착은 미확인 개수만 갱신한다. 읽는 중인 목록·스크롤은 그대로다.
- 새 글 버튼·필터 변경·재열기에서만 최신 원본을 읽고 맨 위로 이동한다.
- ViewModel 접근 불가 / 원본 글 없음 / 필터 결과 없음을 구분한다.
  생산자 연결 상태는 별도 API가 없어 추측하지 않는다.
- 원본에 없는 작성시간·좋아요·댓글·사진·작성 기능을 추가하지 않는다.
- SNS를 열어도 Live 시간과 기존 월드 입력 규칙을 변경하지 않는다.

## 적용

최신 C++ 에디터 빌드 후 에디터를 종료하고 실행한다.

```powershell
& Tools/ui/live_sns_sky/run.ps1
```

`FF.ApplyLiveSnsSkyDesign`은 최초1회만 제작한다. 재실행 시 저장된 Designer 편집을 보존한다.
다른 패널 Layout·DisplayLimit·기존 글 클래스 참조를 유지한다.
백업·결과는 `Saved/LiveSnsSky/`. 빌드/WBP 컴파일/필수 바인딩만 확인하고 실제 화면은 사용자가 확인한다.

| 사용자 확인 | 기대 표시 | 이상할 때 확인 |
| --- | --- | --- |
| 라이브 → SNS | 중앙 흰 패널·하늘색 필터/글 카드 | SNS Layout·헤더/필터 높이 |
| 필터 선택 | 체크와 선택 면색, 맞는 반응만 표시 | 필터 문구/색·원본 방향 |
| 읽는 중 새 글 | 목록이 안 움직이고 상단 새 글 개수만 증가 | 도착 이벤트·버튼 예약 높이 |
| 새 글 보기 | 최신 목록과 맨 위로 이동 | 기존 새 글 버튼 연결 |
| 긴 본문/작성자 | 글이 줄바꿈되고 카드 높이가 늘어남 | Card 최소높이·폰트·AutoWrap |
| 반응/연결 없음 | 글 없음·필터 결과 없음·읽기 실패 안내 구분 | 데이터 공급·빈 상태 문구 |
