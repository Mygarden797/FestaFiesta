# P6 운영 패널 화제성

기존 운영 패널의 일일 목표 오른쪽에 화제성 카드를 추가한다. 관심도 수치와 오늘 변화량을 문구·막대로 함께 보여주고, 막대는 원본 수치를 부드럽게 따라간다. 수치를 읽지 못한 상태는 `아직 집계하지 않음`으로 구분한다.

## 적용

에디터를 닫고 최신 C++ 빌드 후 실행한다.

```powershell
powershell -ExecutionPolicy Bypass -File Tools/ui/live_operations_buzz/run.ps1
```

`FF.ApplyLiveOperationsBuzzDesign`으로 기존 `WBP_FF_LivePanelOperations`에 새 카드만 추가한다. 이미 적용된 디자인은 재생성하지 않는다. 기존 목표 카드와 자식 위젯을 보존하며, 목표 카드가 쓰던 바깥 여백·정렬을 새 가로 행이 이어받는다. 기존 폰트·색·기본값과 운영 창 위치·크기는 유지한다.

보고서와 적용 전 에셋 백업은 `Saved/LiveOperationsBuzz`에 저장한다. 개인 에디터 설정과 저장 파일은 실행 전 백업하고 종료 후 복원한다. 해당 WBP 컴파일·필수 바인딩·기존 위젯과 두 HUD 보존만 확인하며 PIE·렌더·전체 테스트는 실행하지 않는다.

## 에디터에서 수정할 곳

| 대상 | 수정 위치 |
| --- | --- |
| 운영 창 위치·크기 | `/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveHudScreen` → Class Defaults → `Panel Layouts` → `Operations` |
| 운영 창 디자인 | `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelOperations` → Designer |
| 목표·화제성 카드 비율과 간격 | `Ops_ProgressColumns` 아래 `Ops_GoalBorder` / `Ops_BuzzBorder`의 슬롯 |
| 화제성 배경·테두리·안쪽 여백 | `Ops_BuzzBorder` |
| 화제성 글꼴·색 | `Ops_BuzzTitle`, `Text_BuzzValue`, `Text_BuzzChange`, `Text_BuzzNote` |
| 막대 높이·모양·색 | `Ops_BuzzProgressSize` / `Bar_Buzz` |

운영 패널 Class Defaults에서 아래 문구와 보간 속도를 바꿀 수 있다. 위젯 바인딩 이름은 유지한다.

| 설정 | 기본값 |
| --- | --- |
| `BuzzValueTextFormat` | `{Value} / 100` |
| `BuzzIncreaseTextFormat` | `오늘 +{Change}` |
| `BuzzDecreaseTextFormat` | `오늘 −{Change}` |
| `BuzzUnchangedText` | `오늘 변화 없음` |
| `BuzzNoteText` | `축제를 향한 관심도` |
| `BuzzUnavailableText` | `아직 집계하지 않음` |
| `BuzzInterpSpeed` | `6` — 값이 클수록 빠르게 따라가며 `0`이면 즉시 반영 |

사용자 확인: 운영 창에서 기존 일일 목표와 화제성이 나란히 표시되고, 수치가 변할 때 막대와 변화 문구가 함께 갱신되는지 확인한다.
