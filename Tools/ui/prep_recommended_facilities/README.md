# 점검창 권장 시설 안내

조명·음식·화장실·즐길거리·휴게·음료·응급·소방 중 없는 항목만 보여준다. 권장 시설이 없어도 기존 필수 시설 조건을 충족하면 시작할 수 있다. `배치하기`는 기존 건축 목록에서 해당 시설 카드를 연다.

## 에디터에서 수정

- `WBP_FF_PrepPanelCheck` → `Border_RecommendedFacilities`: 배경색·테두리·여백.
- 같은 WBP → `VerticalBox_RecommendedRows` 안의 각 `RecommendedRow_*`: Kind, Title Text, Description Text, Action Text, Icon Brush. `Enabled`를 끄면 해당 안내를 제외한다.
- `WBP_FF_PrepRecommendedFacilityRow`: 공통 행 디자인. `SizeBox_Row`로 높이, `SizeBox_Action`으로 버튼 크기, 각 TextBlock으로 글자 크기·색을 수정한다.
- 점검 WBP의 클래스 기본값 → `Recommended Facilities`: 음료 시설 ID/분류와 안내·시작 버튼 문구를 수정한다.
- `Text_RecommendedSummary`: 고정 하단 경고 글자 크기·색.

두 WBP는 `/Game/FestaFiesta/UI/PrepHUD/Panels`에 있다. 런타임에는 내용·표시 여부만 갱신하고 디자이너에서 정한 치수·폰트·색은 유지한다.

## 판정

현재 배치된 시설 중 검증 완료·운영 중·닫힘·이동 대기 상태를 센다. 철거·초안·재배치가 필요한 시설은 제외한다. 시설 하나가 여러 권장 항목을 충족할 수 있다. 즐길거리에서는 필수 메인 무대를 제외한다. 음료는 판매 시설 기준이므로 쿨링 시설과 일반 음식 부스 전체를 음료로 취급하지 않는다.

응급·소방·휴게·음료의 안내는 시설 부재를 설명하며, 구현되지 않은 피해나 불이익을 표시하지 않는다. 월드 데이터가 없으면 미설치 8종으로 오인하지 않고 확인 중으로 표시한다.

## 최초 적용

에디터를 닫고 빌드한 다음 `generate_icons.ps1`, `run.ps1` 순서로 실행한다. 기존 점검 WBP에 안내만 추가하며 하단 HUD와 기존 위젯 속성을 보존한다. 원본 백업과 적용 보고서는 `Saved/PrepRecommended`, 추가 백업은 `Saved/UIBackups/PrepRecommended`에 남긴다.

콘솔 명령은 `FF.ApplyPrepRecommendedFacilities`다. 적용 버전이 있는 WBP에는 다시 덮어쓰지 않는다. 생성 후에는 WBP에서 직접 편집한다.

판정 테스트: `run.ps1 -Mode Test`로 `FestaFiesta.UI.PrepCheck.RecommendedFacilities` 한 개만 실행한다.
