# P1 — 피날레 선택과 구매 확인

사용자 제공 C/B/A/S 원본을 선택 카드와 구매 확인창에 사용한다.
가격·만족도 보너스·구매 가능 여부는 기존 ViewModel/경제 설정에서 읽는다.
피날레 창 안의 금액은 숫자로 표시하며 금화 아이콘을 반복하지 않는다.

## 에디터에서 수정

- 위치·전체 크기: `/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar`
  → Class Defaults → Panel Layouts → Finale. Size로 크기, Dock/Offset으로 위치를 조절한다.
- 선택창: `/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelFinale`.
  Class Defaults의 Tier Presentations에서 등급별 Preview Texture와 Description을 바꾼다.
  안내·버튼 문구는 Finale 설정, 글꼴·여백·배경·테두리는 Designer에서 편집한다.
- 카드: `/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepFinaleCard`.
  `Image_Preview`의 상위 SizeBox/ScaleBox에서 사진 크기와 비율을 맞춘다.
  `Text_Label`, `Text_Description`, `Text_Cost`, `Text_Bonus`의 글꼴을 직접 조절할 수 있다.
  선택 상태는 테두리와 `Text_Badge`를 함께 사용한다.
- 확인창: `/Game/FestaFiesta/UI/PrepHUD/Finale/WBP_FF_FinaleConfirmDialog`.
  패널 크기·배치·폰트·버튼 스타일은 Designer에서 수정한다.
  `Image_Preview`, `Text_Grade`, `Text_Bonus`, `Text_Cost`, `Text_GoldAfter`는 선택값을 표시한다.
- 원본 텍스처: `/Game/FestaFiesta/UI/PrepHUD/Finale/Textures/T_FF_Finale_C` 및 `_B`, `_A`, `_S`.
  제공 원본 파일은 `SourceArt/UI/Finale/`에 보관한다.

가격과 만족도 보너스는 UI 문구로 고정하지 않고 해당 축제의 기존 피날레 설정을 수정한다.
Compile/Save 후 새 실행에서 확인한다. 필수 위젯 이름을 바꾸면 C++ 연결도 수정해야 한다.

## 적용

최신 C++ 에디터 빌드 후 열린 에디터를 종료하고 실행한다.

```powershell
& Tools/ui/prep_finale_sky/run.ps1
```

`apply.py`는 없는 텍스처만 임포트한 뒤 `FF.ApplyPrepFinaleSkyDesign`을 실행한다.
이미 생성된 이미지·Designer 편집과 다른 탭 위치·크기를 보존한다.
강제 재임포트·재생성, 전체 에셋 저장, 테스트 맵 저장은 하지 않는다.
일회용 에디터를 쓰기 전에 개인 설정·저장 파일을 백업하고 종료 후 복원한다.
백업과 결과는 `Saved/PrepFinaleSky/`, 실행 로그는 `Saved/BrightGlobe/prep_finale_sky_apply.log`에 남는다.

확인은 WBP 컴파일과 필수 바인딩·기존 탭 레이아웃 보존까지만 한다. PIE/렌더는 실행하지 않는다.

## 사용자 확인

- 마지막 날 아침 점검에서 피날레 탭을 열고 C/B/A/S 선택, 미리보기와 구매 후 잔액을 확인한다.
- 구매 확인창의 돌아가기는 결제하지 않으며, 구매 확정은 기존 구매 명령을 요청한다.
- 잔액 부족·시점 변경·이미 구매한 상태는 구매 차단 사유를 표시한다.
- 구매 성공 후 구매한 등급을 표시하며 변경·취소·중복 구매를 허용하지 않는다.
- 카드와 확인창의 금액 옆에 금화 아이콘이 없는지, 이미지가 찌그러지지 않는지 확인한다.
