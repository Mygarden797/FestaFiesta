# P4 홍보 패널 — 네 등급 비교와 구매 요약

홍보 패널을 흰색·하늘색 카드 네 장과 하단 구매 요약으로 구성한다.
가격·예상 방문객 배수는 실제 ViewModel 데이터를 사용하고, 구매 확인창과 축제당 1회·셋업 구매 규칙을 유지한다.
카드 선택만으로 결제하지 않으며 구매 결과 이벤트를 받아 완료 상태를 표시한다.

## 에디터 편집

`/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar`
→ **Class Defaults → Panel Layouts → Promotion**:

- `Size`: 기본 1140 × 620. X는 폭, Y는 높이.
- `Dock = Center`: 실제 게임 화면 중앙.
- `Offset`: X 양수는 오른쪽, Y 양수는 아래. 다른 탭에 영향을 주지 않는다.

홍보 WBP: `/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelPromotion`

| 변경할 항목 | Designer 위치 |
| --- | --- |
| 배경·외곽선 | `Border_PanelRoot → Brush → Tint / Outline Settings` |
| 제목·닫기 크기 | `SizeBox_Header`, `SizeBox_Close` |
| 본문 여백 | `ScrollBox_Body → Slot → Padding` |
| 카드 간격 | `Box_PromotionCards → Inner Slot Padding` |
| 하단 요약 | `Border_PurchaseSummary → Brush / Padding` |
| 구매 버튼 크기·색 | `SizeBox_PurchaseAction`, `Button_Purchase → Style` |
| 글자 크기 | 각 `Text_* → Font → Size` |

카드 WBP: `/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepPromotionCard`

- `SizeBox_Card`: 폭 240, 최소 높이 240. 긴 이름은 줄바꿈하고 본문은 스크롤된다.
- `Button_Root → Style`: Normal / Hovered / Pressed / Disabled. **Pressed가 선택 상태**의 채움과 테두리다.
- 카드 글자·아이콘 색은 **Class Defaults → Normal Content Color / Selected Content Color / Disabled Content Color**에서 수정한다. 상태 갱신 때 이 색을 사용한다.
- 카드 안쪽 여백은 `VerticalBox_Card → Slot (Overlay) → Padding`이다. 클릭 영역과 표시 내용은 형제 레이어로 두어 구매 후 글자가 이중으로 흐려지지 않게 했다. `VerticalBox_Card`의 Hit Test Invisible 설정은 유지한다.
- `Choice Icon Brushes`: 등급별 아이콘. 기존 프로젝트 아이콘을 재사용했다.
- `Cost Format`, `Effect Format`: `{0}`은 실제 등급 비용/배수다. 배수는 예상치이며 실제 입장 인원 증가를 보장하지 않는다.

패널의 **Class Defaults → FestaFiesta → UI → Prep → Promotion → Text**에서 선택/구매 완료 문구·금액 포맷·잔액 레이블·선택 배지·안내문을 수정한다.
구매 완료 후에는 구매한 등급과 **현재 잔액**을 표시한다. 이미 낸 홍보비를 한 번 더 빼지 않는다.
수치가 들어갈 자리의 `{0}`과 `BindWidget` 위젯 이름은 유지한다. Compile/Save 후 PIE를 다시 시작한다.

## 적용·검사

에디터를 닫고 Editor 타깃을 빌드한 다음:

```powershell
& Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/prep_promotion_sky/apply_and_verify.py -Label prep_promotion_sky
```

최초 실행은 `Saved/PrepPromotionSky/Before-*`에 대상 WBP를 백업한다. 홍보 패널·카드와 HUD의 **Promotion Size만** 수정한다.
인력 820×500, 다른 패널 설정, HUD 트리·행 높이·폰트·버튼 스타일은 보존한다.
콘솔 명령은 `FF.ApplyPrepPromotionSkyDesign`. 이미 적용된 패널에는 Designer 편집을 덮어쓰지 않는다.

`FestaFiesta.UI.PrepPromotion.*` 검사 결과는 `Saved/PrepPromotionSky/report.json`,
실제 UMG 렌더는 `Saved/PrepPromotionSky/Layout/`에 저장된다.
렌더는 게임 인스턴스가 없는 표시 fixture이며 실제 플레이 결제를 실행하지 않는다.

## 사용자 확인

- 홍보 탭이 중앙에 뜨고 네 등급의 가격·배수가 표시되는지 확인한다.
- 카드를 누르면 체크·테두리와 요약이 바뀌고, 구매를 누르면 기존 확인창이 열리는지 확인한다.
- 취소하면 결제되지 않고, 확인 후 결과가 오면 구매함·현재 잔액이 표시되는지 확인한다.
- ‘홍보 없음’·잔액 부족·구매 완료·셋업 종료에는 구매 버튼이 꺼지고 이유가 나타나는지 확인한다.
- 크기·Offset·색·폰트를 편집한 뒤 재실행해 저장값이 유지되는지 확인한다.

점검 패널(P5)은 다음 단계다. 커밋·푸시는 별도 요청 때 수행한다.
