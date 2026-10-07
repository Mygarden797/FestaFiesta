# P3 — 점검 화면 입장료 조절

기존 점검 화면에 입장료 조절 영역을 추가한다. 슬라이더와 −/+ 버튼은 변경할 금액과 예상값을 미리 보여주며,
`적용`을 눌러야 기존 입장료 변경 명령을 요청한다. `되돌리기`는 현재 적용된 금액으로 돌린다.
적용하지 않고 창을 닫으면 조절 중인 값은 버린다. 미적용 변경이 있으면 개장 버튼에 적용·되돌리기 안내를 표시한다.

입장료 범위와 현재값은 ViewModel에서 읽는다. 현재 계약은 0~15, 기준값 5이지만 UI에 별도로 고정하지 않는다.
예상 방문객은 원본 예측의 단일값이며 임의의 최소~최대 범위를 만들지 않는다.
예상 수입과 실제 적용값을 구분하고, UI에서 경제 상태나 Stage를 직접 바꾸지 않는다.

## 에디터에서 수정

위젯: `/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck`.

- 전체 창 크기·위치: 기존 `WBP_FF_PrepHudBar → Class Defaults → Panel Layouts → Check`.
  현재 1140×700 설정을 포함해 사용자 편집값을 유지한다.
- 새 구역 배경·테두리·안쪽 여백: Designer의 `Border_AdmissionSection`.
- 양쪽 배치: `VerticalBox_AdmissionEditor`와 `VerticalBox_AdmissionPreview` 및 부모 슬롯의 Fill·패딩.
- 슬라이더: `Slider_AdmissionFee`의 Widget Style에서 선·핸들 색, Bar Thickness에서 선 굵기를 조절한다.
  실제 값·범위는 실행 때 원본 계약으로 갱신하므로 Designer의 임시 숫자를 게임 규칙으로 사용하지 않는다.
- 금액 칸과 버튼 크기: `SizeBox_AdmissionValue`, `SizeBox_AdmissionDecrease`, `SizeBox_AdmissionIncrease`,
  `SizeBox_AdmissionApply`, `SizeBox_AdmissionReset`.
- 현재 조절값·적용값·범위·상태·예측 변화·예상 수입: 각각 `Text_AdmissionFeeValue`, `Text_AppliedAdmissionFee`,
  `Text_AdmissionFeeRange`, `Text_AdmissionFeeState`, `Text_AdmissionPreviewDelta`, `Text_AdmissionPreviewRevenue`.
  각 Text의 글꼴·색·정렬을 Designer에서 수정한다.
- 실행 중 바뀌는 문구 형식: Class Defaults의 `FestaFiesta → UI → Prep → Admission → Text`.
  `{Amount}`, `{Min}`, `{Max}`, `{Reference}`, `{Sign}`, `{Count}` 자리표시자는 유지하면서 문장을 바꾼다.
- 예상 방문객은 기존 `SizeBox_ExpectedVisitorsCard` 전체를 새 예측 영역으로 옮긴다.
  내부 `Text_ExpectedVisitors`와 기존 카드 스타일·치수는 그대로 유지한다.

기존 날씨·목표·금화·준비 상태·개장 버튼은 재생성하지 않는다.
필수 위젯 이름을 바꾸면 C++ 바인딩도 수정해야 한다. Compile/Save 후 새 실행에서 확인한다.

## 적용

최신 C++ 에디터 빌드 후 열린 에디터를 종료하고 실행한다.

```powershell
& Tools/ui/prep_admission/run.ps1
```

`FF.ApplyPrepAdmissionDesign`은 기존 점검 패널의 `WrapBox_Metrics` 다음에 새 영역을 추가한다.
메타데이터 `FF_PrepAdmissionVersion=1`이 있으면 재구성하지 않아 이후 Designer 편집을 보존한다.
저장되지 않은 변경이나 PIE가 있으면 에디터 명령이 적용을 거부한다.

도구는 점검 WBP 한 개만 백업·컴파일·저장한다. 필수 바인딩, 기존 주요 위젯 스타일·점검 설정,
두 HUD의 파일·탭 레이아웃 보존만 확인하고 PIE·렌더·전체 테스트는 실행하지 않는다.
HUD나 맵은 저장하지 않으며 개인 설정·저장 파일은 일회용 에디터 실행 전 백업하고 종료 후 복원한다.

결과·백업: `Saved/PrepAdmission/`. 명령 자체의 원본 백업: `Saved/UIBackups/PrepAdmission/`.
실행 로그: `Saved/BrightGlobe/prep_admission_apply.log`. 저장 후 2초간 초기화 틱을 기다린 뒤 종료한다.

## 사용자 확인

- 점검을 열면 현재 적용된 입장료가 표시되고 조절만으로 실제 금액이 바뀌지 않는지 확인한다.
- −/+·슬라이더 조절 시 방문객 예측과 예상 수입이 갱신되며, 미적용 상태가 표시되는지 확인한다.
- `적용` 성공 후 적용값·상태가 갱신되고, `되돌리기`나 닫았다 다시 열기로 변경 초안이 버려지는지 확인한다.
- 적용하지 않은 변경이 있을 때 개장 버튼의 안내가 나오며, 적용 또는 되돌리기 후 기존 개장 조건을 따르는지 확인한다.
- 작은 창에서 내용은 스크롤되고 제목·하단 개장 버튼, 기존 편집한 크기·스타일이 유지되는지 확인한다.
