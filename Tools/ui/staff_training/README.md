# P7 스태프 숙련도

> 10/14 기준: 이 단계의 `현황`/`숙련도` 탭은 이후 `Tools/ui/staff_management`(스태프 통합 관리) 적용으로 `Management_Legacy` 아래에 접혀 보관된다. 지금 두 스태프 창은 공통 관리 위젯 `Widget_StaffManagement`(`WBP_FF_StaffManagement`, `UFF_StaffManagementWidget`)를 쓰며, `WBP_FF_StaffTraining`·`WBP_FF_StaffPromotionRow` 는 그 안에서 재사용된다. 아래 탭 크기 조정(`Staff_TabRow`·`SizeBox_StaffOverviewTab`·`SizeBox_StaffTrainingTab`)은 숨겨진 옛 탭에 대한 설명이다.

준비 단계 인력 창과 라이브 스태프 창에 `현황` / `숙련도` 탭을 추가한다. 현황 탭은 기존 고용·배정 정보를 유지하고, 숙련도 탭은 공통 스태프 목록과 승급 상태를 보여준다.

현재 경제 원본 규칙을 그대로 사용한다. 고용한 스태프는 고용 다음 날부터 하루 한 단계씩 최대 Lv.3까지 승급할 수 있으며, 기본 인력은 1일차부터 가능하다. 승급으로 금화를 차감하지 않는다. 원본의 날짜 검사에 UI 전용 Stage 제한을 추가하지 않는다.

## 적용

에디터를 닫고 최신 C++ 빌드 후 실행한다.

```powershell
powershell -ExecutionPolicy Bypass -File Tools/ui/staff_training/run.ps1
```

`FF.ApplyStaffTrainingDesign`으로 공통 숙련도·승급 행 WBP를 만들고 기존 두 스태프 창에 탭을 연결한다. 기존 본문·푸터는 `Staff_OverviewContent`로 옮기며 폰트·색·크기·여백과 기본값을 유지한다. 이미 적용된 Designer 편집값은 재생성하지 않는다.

두 HUD의 `Panel Layouts`와 하단 행을 저장하거나 변경하지 않는다. 현재 메뉴 높이 32, 상태 행 높이 36, 준비 인력 창 높이 500을 포함해 사용자가 저장한 값이 유지된다.

보고서와 적용 전 에셋 백업은 `Saved/StaffTraining`에 저장한다. 개인 에디터 설정과 저장 파일은 실행 전 백업하고 종료 후 복원한다. 해당 WBP의 부모·필수 바인딩·행 연결·컴파일과 기존 스타일/HUD 보존만 확인한다. PIE·렌더·전체 테스트는 실행하지 않는다.

## 에디터에서 수정할 곳

| 대상 | 에셋 / 설정 |
| --- | --- |
| 준비 인력 창 위치·크기 | `/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar` → Class Defaults → `Panel Layouts` → `Staff` |
| 라이브 스태프 창 위치·크기 | `/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen` → Class Defaults → `Panel Layouts` → `Staff` |
| 준비 창 탭·현황 | `/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff` |
| 라이브 창 탭·현황 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff` |
| 공통 숙련도 화면 | `/Game/FestaFiesta/UI/Staff/WBP_FF_StaffTraining` |
| 스태프 한 줄 | `/Game/FestaFiesta/UI/Staff/WBP_FF_StaffPromotionRow` |

두 창의 `Staff_TabRow`, `SizeBox_StaffOverviewTab`, `SizeBox_StaffTrainingTab`에서 탭 크기·간격을 조절한다. `Switcher_StaffContent`의 0번은 현황, 1번은 `Widget_StaffTraining`이다.

숙련도 WBP의 요약·안내 글꼴과 목록 영역, 행 WBP의 `Text_StaffName`, `Text_StaffKind`, `Text_StaffLevel`, `Text_PromotionStatus`, `Text_Promote`와 각 슬롯에서 글꼴·열 폭·색·버튼 크기를 수정한다. 동적 안내·승급 결과 문구는 각 WBP Class Defaults에서 바꾼다. 숙련도 WBP의 `StaffRowClass`는 승급 행 WBP를 가리키며, 바인딩 위젯 이름은 유지한다.

사용자 확인: 두 창의 현황이 유지되고 숙련도 탭에서 스태프별 승급 가능 여부와 이유가 보이는지, 승급 결과가 실제 레벨로 갱신되는지 확인한다.
