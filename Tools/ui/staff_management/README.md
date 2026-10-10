# 스태프 통합 관리 WBP 적용

셋업·라이브 스태프 패널에 안내시설 배정과 직원 숙련도를 함께 표시한다. 기존 고용·승급 객체를 옮겨 재사용하고, 이전 현황/숙련도 탭과 본문은 `Management_Legacy` 아래에 접어 보관한다.

## 실행

에디터를 종료하고 새 C++를 빌드한 다음 프로젝트 루트에서 실행한다.

```powershell
./Tools/ui/staff_management/run.ps1 -Mode Apply
```

기존 스태프 화면의 추가 고용 `−/+`만 20px·가운데 정렬·패딩 0으로 고치고 준비 HUD 탭을 `스태프`로 바꾸려면 `-Mode Polish`를 쓴다. C++ 재빌드 없이 기존 WBP 2개만 컴파일·저장하며 별도 PIE/자동화 검사는 실행하지 않는다. 원본 백업은 `Saved/UIBackups/StaffManagementPolish/`, 결과는 `Saved/StaffManagement/polish-report.json`에 남는다.

이미 열린 에디터 콘솔에서는 `FF.ApplyStaffManagementDesign`으로 디자인만 적용할 수 있다. PIE 중이거나 대상에 저장하지 않은 변경이 있으면 적용하지 않는다.

## 적용 범위

- 신규 `/Game/FestaFiesta/UI/Staff/WBP_FF_StaffManagement`: 4개 지표, 안내시설 배정, 직원 숙련도, 라이브 사건 바로가기.
- 신규 `/Game/FestaFiesta/UI/Staff/WBP_FF_StaffFacilityRow`: 시설명·배정/정원·상태·배정/해제/위치 버튼.
- 기존 `WBP_FF_StaffTraining`, `WBP_FF_StaffPromotionRow`: 전체/미배정 필터와 배정시설/업무 상태 열. 기존 승급 액션 유지.
- 기존 `WBP_FF_PrepPanelStaff`, `WBP_FF_LivePanelStaff`: 공통 관리 위젯 연결. 셋업 고용 하단은 기존 버튼/수량/견적 객체를 재사용하며 보유금화 옆에만 금화 아이콘을 표시한다.
- `Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar`, `UI/LiveHUD/WBP_FF_LiveHudScreen`: Staff 항목 크기만 1300×800으로 변경한다.

## 보존 및 확인

대상 원본은 `Saved/UIBackups/StaffManagement/<GUID>/`에 먼저 복사한다. 각 에셋의 `FF.StaffManagement.Version=1` 이후 재실행은 Designer 편집을 유지한다. 이름/타입 보존, 필수 바인딩, 행 클래스, 기존 탭의 숨김, 다른 HUD 탭 배치가 바뀌지 않았는지 확인한다. 별도 PIE나 새 테스트는 만들지 않는다.

같은 에디터에서 기존 `FestaFiesta.UI.ViewModel.EconomyRelay.StaffPromotion`, `FestaFiesta.UI.PrepStaff.QuoteClamps` 두 검사만 실행한다. 결과는 `Saved/StaffManagement/apply-report.json`, 로그는 `Saved/BrightGlobe/staff_management_apply.log`에 남는다. 실행 도구는 기존 저장 파일과 개인 에디터 설정을 복원한다. 실제 배정·위치 보기와 시각적 크기는 사용자 플레이로 확인한다.

`runpy.run_path(..., init_globals={'FF_BATCH_APPLY': True})`로 통합할 때는 종료 콜백/검사 실행을 생략한다. 이 경우 통합 스크립트가 위 두 검사와 에디터 종료를 맡는다.

런타임에서는 새 공통 관리 위젯만 활성화한다. 숨겨 보관한 부모의 이전 `Widget_StaffTraining`을 동시에 활성화하지 않는다.
