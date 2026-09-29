# P2 인력 패널 — 흰색·하늘색 2열 구성

인력 패널 하나에 적용하는 에디터 제작 도구다. 프로그램·홍보·점검은 다음 단계에서 같은 틀을 사용한다.
실제 고용은 기존 ViewModel 요청과 결과 이벤트를 사용한다. 이 도구는 경제·시간 규칙을 바꾸지 않는다.

## 에디터에서 수정하는 곳

콘텐츠 브라우저에서 `WBP_FF_PrepHudBar`를 열고 **Class Defaults → Panel Layouts → Staff**:

- `Dock = Center`: 게임 화면 중앙 기준.
- `Offset X / Y`: 오른쪽/아래가 양수. 다른 패널에 영향을 주지 않는다.
- `Size X / Y`: 초기값 820 × 470. DPI 적용 전 단위이며 편집값은 실행 중 강제 덮어쓰지 않는다.

HUD 경로: `/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar`

인력 패널 `/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelStaff`의 **Designer**:

| 원하는 변경 | 위젯 / 속성 |
| --- | --- |
| 배경·전체 테두리 | `Border_PanelRoot → Brush → Tint / Outline Settings` |
| 제목 줄 높이 | `SizeBox_Header → Height Override` |
| 닫기 버튼 크기 | `SizeBox_Close → Width / Height Override` |
| 본문 여백 | `ScrollBox_Body → Slot (Vertical Box) → Padding` |
| 좌우 열 간격 | `VerticalBox_CurrentStaff`, `VerticalBox_HireQuote → Slot → Padding` |
| 구분선 색 | `Border_HeaderDivider`, `Border_ColumnDivider` 등 → Brush → Tint |
| 정원 막대 | `Progress_StaffCapacity → Style`; 높이는 `SizeBox_StaffCapacity` |
| 인원 조절 버튼 | `SizeBox_Decrease / SizeBox_Increase` 크기, 내부 Button의 Style |
| 고용 버튼 | `SizeBox_HireAction` 최소 크기, `Button_Hire → Style` |
| 하단 배경·여백 | `Border_Footer → Brush / Padding` |
| 글자 크기·색 | 각 `Text_* → Font / Color and Opacity` |

수치가 포함된 **문구**는 인력 WBP의 **Class Defaults → FestaFiesta → UI → Prep → Staff → Text**에서 편집한다.

| 속성 | 치환값 |
| --- | --- |
| Staff Capacity Text Format | `{Current}`, `{Maximum}` |
| Staff Breakdown Text Format | `{Base}`, `{Hired}` |
| Selected Count Text Format | `{Count}` |
| Hire Unit Cost Text Format | `{Count}`, `{UnitCost}` |
| Gold Value Text Format | `{Gold}` |
| Hire Action Text Format | `{Count}` |

영구 고용 안내는 `Daily Notice Text`, 패널을 열 때 선택 인원은 `Default Selected Count`다.
Designer 미리보기 수치를 바꿔도 실제 게임 값은 바뀌지 않는다. BindWidget 대상 이름은 바꾸지 않는다.
고용비·인원 상한은 축제 원본 데이터가 결정한다. 정원은 기본 인력 + 추가 고용 상한이다.

## 적용·검증

에디터를 닫은 상태에서 Editor 빌드 후:

```powershell
& Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/prep_staff_sky/apply_and_verify.py -Label prep_staff_sky
```

최초 실행은 `Saved/PrepStaffSky/Before-*`에 인력/HUD WBP를 백업하고 인력 트리와 Staff Size만 바꾼다.
하단 HUD의 기존 행 높이·테두리·다른 패널 설정은 보존한다. 재실행하면 적용 여부를 확인해 Designer 편집을 유지하고 검사만 한다.
에셋을 적용하는 콘솔 명령은 `FF.ApplyPrepStaffSkyDesign`이며 인력 WBP에만 작용한다.
HUD의 크기·인력 표시 포맷까지 포함한 최초 적용은 위 스크립트를 사용한다.

자동화는 `FestaFiesta.UI.PrepStaff.*`를 실행한다. 결과는 `Saved/PrepStaffSky/report.json`,
실제 UMG 렌더는 `Saved/PrepStaffSky/Layout/`에 남는다. 렌더 검사는 게임 상태를 바꾸지 않는 표시값 fixture다.

## 사용자가 확인할 항목

| 확인 | 통과 기준 | 이상할 때 확인 |
| --- | --- | --- |
| 인력 탭 | 화면 중앙에 2열 패널, 닫기/하단 고용 버튼 표시 | HUD의 Staff Layout, 인력 WBP |
| ± / 고용 | 선택 수와 비용이 함께 변경되고 성공 후 인원·잔액·막대 갱신 | 실제 VM 데이터/명령 결과 |
| 잔액 부족·상한 | 버튼 비활성 + 이유 문구 | Text_HireBlockReason 바인딩 |
| 크기·스타일 수정 | 저장 후 실행해도 편집값 유지, 작은 높이는 본문 스크롤 | ScrollBox_Body, 슬롯 Padding, 각 SizeBox |

커밋·푸시는 별도 사용자 요청 때 수행한다.
