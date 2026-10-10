# 소식·매출 순위 통합 적용

에디터 모듈을 빌드한 뒤 열린 에디터를 닫고 `Tools/ui/unified_live_style/run.ps1` 실행.

- 소식 WBP 3개: `FF.ApplyUnifiedNews`
- 매출 순위 WBP 2개: `FF.ApplyFacilityRevenueSky`
- 각 적용기는 원본 에셋을 `Saved/UIBackups`에 백업하고 한 번만 마이그레이션한다.
- 개인 에디터 설정과 세이브는 실행 전 백업/실행 후 해시 확인 복원한다.
- 보고서: `Saved/UnifiedLiveStyle/apply-report.json`
- 한 에디터에서 소식 필터 검사 1개만 실행. 실제 화면 확인은 별도다.

피날레 금색 탭은 기존 `WBP_FF_PrepHudTabButton` 베이스의 표시 기능이다. 에셋을 재생성하지 않는다.
부모 HUD의 `Tab_Finale`을 선택하면 `FestaFiesta > UI > Prep > Finale`에서 색, 반사 강도/주기,
움직임 여부와 배지 문구를 조정할 수 있다. `Preview Finale Attention`은 디자이너 전용 미리보기다.
런타임에는 ViewModel의 피날레 구매 가능 상태를 읽고 구매 성공/단계 변화 이벤트로 갱신한다.
안내 카드나 시간을 멈추는 팝업은 추가하지 않는다.
