# 프로그램 시설 안내·편성 완료 표시

`FF.ApplyPrepProgramFeedback`는 기존 프로그램 패널과 카드에 필요한 위젯만 추가한다. 원본 에셋은 `Saved/UIBackups/PrepProgramFeedback/`에 백업한다. 버전 메타데이터가 있으면 다시 생성하지 않아 이후 Designer 편집을 보존한다.

에디터를 닫고 C++ 에디터 타깃 빌드 후 실행:

```powershell
./Tools/ui/prep_program_feedback/run.ps1 -Mode Apply
./Tools/ui/prep_program_feedback/run.ps1 -Mode Test
```

Apply는 대상 WBP 바인딩과 기존 글꼴·치수·스타일 보존을 확인한다. Test는 기존 `FestaFiesta.UI.PrepProgram` 테스트만 NullRHI로 실행한다. PIE·화면 캡처·전체 UI 회귀 검사는 하지 않는다. 개인 에디터 설정과 세이브 파일은 실행 전 백업 후 복원한다.

## 에디터 조정 위치

| 에셋/항목 | 조정 내용 |
| --- | --- |
| `WBP_FF_PrepPanelProgram` → `Border_FacilityGuide` | 시설이 없을 때 안내 배경·테두리·여백 |
| 같은 패널 → `Box_CatalogChoices` | 시설 바로가기 간격·최소 높이 |
| 같은 패널 → Class Defaults → Program → FacilityGuide / Feedback | 안내/완료 문구, 바로가기 열 수, 토스트 표시 시간 |
| 같은 패널 → `SizeBox_ScheduleToast`의 Canvas Slot | 토스트 위치, 너비 |
| 같은 패널 → `Text_ScheduleToastTitle/Detail`, `Text_ProgramCompletion`, `Text_SummaryStatus` | 완료 표시 글꼴·크기·색 |
| `WBP_FF_PrepProgramCatalogChoice` → `SizeBox_Choice`, `Button_Root`, `Border_Attention` | 버튼 최소 높이·패딩·상태별 테두리·강조 색 |
| 같은 바로가기 → Class Defaults → Program Shortcut | 강조 시간·횟수·불투명도, 보조 문구 |
| `WBP_FF_PrepProgramCard` → `Border_CompletionBadge`, `Text_CompletionBadge` | 카드 완료 배지 모양·글꼴 |
| 같은 카드 → Class Defaults → Program → Completion | 완료 카드 배경·테두리·글자 색, 완료 문구 |

`Border_FacilityGuide`, `Border_ScheduleToast`, `Border_CompletionBadge`는 기본 Collapsed이다. 미리 볼 때 Designer에서 잠깐 펼칠 수 있으며 실행 시 실제 상태가 표시 여부를 결정한다.

편성 성공은 소유자 명령 결과로만 표시한다. 프로그램별 완료 상태를 유지하며 다른 프로그램/다른 시설 편성을 막지 않는다. 토스트는 실제 시간 기준으로 닫히며 게임 시간을 정지시키지 않는다.
