# FacilityInfo LightTeal

선택한 시설의 상태와 다음 행동을 한눈에 판단하는 비모달 운영 카드다. 지도와 하단 Build HUD를 계속 볼 수 있어야 하며, 장식보다 `상태 → 강화 → 핵심 수치 → 프로그램 → 행동` 순서를 우선한다.

- 기준 시안: [FF_FacilityInfo_NoOverlap_LightTeal_v1.png](Generated/FF_FacilityInfo_NoOverlap_LightTeal_v1.png)
- 런타임 WBP: `/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel`
- 유료 프로그램 Picker WBP: `/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramPicker`
- 유료 프로그램 Card WBP: `/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramCard`
- 네이티브 계약: `UFF_FacilityInfoPanel`, `UFF_FacilityPaidProgramPicker`, `UFF_FacilityPaidProgramCard`
- 데이터 원천: `FFF_FacilityInfoViewData`, `FFF_FacilityPaidProgramPickerViewData`, `FFF_FacilityPaidProgramCardViewData`; Designer에 운영 수치를 하드코딩하지 않는다.
- 강화 규칙·비용·구매 API: [시설 강화 구현 문서](../../FacilityUpgrade/README_KO.md).

## 배치 계약

### 시설 정보 패널

| 항목 | 값 |
|---|---:|
| Anchor / Alignment | `(1, 0)` / `(1, 0)` |
| Canvas Position | `(-24, 72)` |
| 고정 크기 | `388 × 500` |
| Canvas AutoSize | `false` |

루트 Canvas는 `SelfHitTestInvisible`을 유지한다. 카드 밖 월드 입력을 막는 스크림은 사용하지 않으며, 내용이 늘면 카드가 커지는 대신 내부 스크롤 또는 Build/Live 문맥별 섹션 전환으로 처리한다.

1080p 기준 시설 카드 하단은 `72 + 500 = 572px`다. 하단 Build UI 예약 경계는 `1080 - 400 = 680px`이므로 두 UI 사이에 `108px` 간격이 남는다.

### 유료 프로그램 Picker

| 항목 | 값 |
|---|---:|
| Anchor Min / Max | `(1, 0)` / `(1, 1)` |
| Alignment | `(1, 0)` |
| 우측 / 상단 여백 | `24px` / `72px` |
| 고정 폭 | `520px` |
| 하단 Build UI 예약 | `400px` |
| Canvas AutoSize | `false` |

Picker는 우측에 도킹하고 세로로 스트레치한다. 하단 `400px`는 `Build Host 320px + 상태 바 58px + 여백 12px + 안전 간격 10px`의 합이며, Picker의 하단이 이 예약 영역을 침범하면 안 된다. 따라서 1080p에서는 Picker가 `y=72..680`을 사용한다. `Border_Picker`의 폭·여백·앵커 또는 Build Host 높이를 바꿀 때는 이 계산과 `FestaFiesta.UI.FacilityInfo.AssetContract`를 함께 갱신한다.

시설 정보 패널과 유료 프로그램 Picker는 동시에 표시하지 않는다. `SetPaidProgramPickerOpen(true)`는 시설 정보 패널을 `Collapsed`로 전환하고 Picker만 표시한다. Picker 닫기 또는 편성 성공 뒤에도 현재 시설 선택이 유효할 때만 시설 정보 패널을 복원한다. 이 상호 배타 규칙으로 우측 창끼리 겹치지 않게 한다.

## 정보 구조

1. Teal 헤더: 문맥, 시설명, 닫기.
2. 요약: 썸네일, 분류·구역 칩, 현재 상태, 1인 예상 수익과 기본 대비 추가 Gold. 수익은 레벨 없는 시설에서도 표시하며 무료 시설은 0 Gold다.
3. 강화: 레벨을 사용하는 시설만 현재 레벨을 Build/Live 모두 표시한다. Build에서는 현재→다음 수익·변화 Gold·증감률·Lv.1 기준 배율을 먼저 보여 주고 서비스 시간·최대 처리량·비용·구매 후 잔액을 비교한다. 크기가 바뀌면 현재와 다음 바닥 크기를 미터 단위로 표시한다. 골드가 모자라면 부족액을, 공간이 부족하면 구매 불가 사유를 버튼 옆에 표시한다. 무료 단계는 `Lv.N 강화 · 무료` 버튼으로 표시하며 효과가 같거나 감소해도 견적을 숨기지 않는다.
4. 운영 정보: Build에서는 크기·안전 여유·설치/회수 비용, Live에서는 대기·점유·처리량·예상 대기.
5. 프로그램: 편성 상태, 시간·규모, 편성 가능 여부와 사유. 편성 버튼은 같은 시설을 대상으로 하는 우측 Picker를 연다.
6. 하단 행동: 이동은 중립/Teal, 철거는 Danger. 비활성은 색과 사유 문구를 함께 제공한다.

강화 영역은 `ScrollBox_Details > Panel_Content` 안에서 요약 다음, 기존 프로그램·운영 정보 앞에 놓인다. 카드 크기와 고정 하단 행동 영역은 유지한다. `bHasFacilityLevels`가 꺼진 시설은 `Panel_UpgradeSection` 전체를 숨겨 레벨이나 비활성 강화 버튼도 남기지 않는다. 요약의 현재 수익은 유지한다. 강화 버튼 클릭은 표시된 가격·효과에 대한 구매 확인이며, 게임 시스템이 표시 당시 레벨과 현재 구매 조건을 다시 검증한다. Live에서는 구매 상세와 버튼을 숨긴다. 최대 레벨·가격 조회 실패·속도와 수익 모두 개선되지 않는 경우에는 유효하지 않은 다음 단계 견적을 표시하지 않는다. 속도가 같아도 수익이 증가하면 견적을 표시한다.

현재→다음 수익은 실제 정산 함수를 한 명 기준으로 호출한 값이다. 변화 Gold와 증감률도 반올림된 표시액끼리 계산하며 현재 수익이 0이면 %를 생략한다. 증가는 `+N`, 감소는 `-N`, 유지하면 `0`으로 표시한다. `기본 ×1.5`는 Lv.1 기준 설정 배율이다. 파티 전체 정산은 인원까지 곱해 마지막에 한 번 반올림하므로 1인 예상치를 단순 합산한 값과 차이가 날 수 있다. 유효 견적은 `bHasUpgradeQuote`로 판단하여 무료와 가격 오류를 구분한다.

Picker는 `시설명 → 호환 프로그램 카드 목록 → 선택 미리보기 → 명시적 편성 확인` 순서로 읽힌다. 카드 선택은 로컬 미리보기만 바꾸며 결제하지 않는다. 확인 버튼을 눌렀을 때 현재 시설·단계·재화·인력을 다시 검증한 뒤 편성한다.

## LightTeal 토큰

색의 단일 원천은 `Source/FestaFiesta/Public/UI/Data/FF_UIPalette.h`의 `FFF_UIPaletteInGame`이다. C++에서는 직접 RGB 비율을 넣지 말고 `FLinearColor::FromSRGBColor`를 사용한다.

| 역할 | 토큰 | sRGB |
|---|---|---|
| 카드 면 / 보조 면 | `Paper` / `PaperSunken` | `#F7F8F6` / `#EEF2EF` |
| 헤더·주요 행동 / 포커스 | `Teal` / `TealDeep` | `#527C6B` / `#315747` |
| 본문 / 보조 / 비활성 글자 | `Ink` / `InkMuted` / `InkFaint` | `#24332E` / `#6F7B75` / `#94A09A` |
| 구분선 / 강조선 | `Line` / `LineStrong` | `#CCD5CF` / `#9DB1A5` |
| 주의 / 위험 / 정보 | `Amber` / `Danger` / `Info` | `#E39A22` / `#D74E43` / `#3D86B4` |

시설 정보 패널, 유료 프로그램 Picker와 Card는 모두 위 LightTeal 팔레트를 공유한다. 폰트는 `/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard`를 사용한다. 시설명은 20px SemiBold, 섹션 제목과 수치에는 Medium/SemiBold, 보조 라벨에는 Regular를 써서 위계를 만든다. 패널 모서리 8px, 내부 카드·버튼 6px, 칩 4px를 기준으로 한다.

## UMG Designer 편집 지점

- `Panel_Size`: 위 배치 계약을 유지한다.
- `Border_Panel`: Paper 면, Line 테두리, 그림자와 패딩.
- 헤더·요약: `Text_Context`, `Text_FacilityName`, `Button_Close`, `Image_Thumbnail`, `Text_Category`, `Text_Zone`, `Text_FacilityState`.
- 현재 수익: `Text_FacilityRevenue`는 `CommonTextBlock`이며 `BindWidgetOptional` 계약이다. 기본 위치는 `Summary_Meta`의 현재 상태 아래다. 레벨 없는 시설에서도 보이도록 강화 영역 밖에 둔다.
- 문맥 섹션: `Panel_BuildSection`, `Panel_LiveSection`; 브러시·간격·폰트·애니메이션은 수정 가능하다.
- 강화: `Panel_UpgradeSection`은 `VerticalBox`, `Button_Upgrade`는 `Button`이다. `Text_FacilityLevel`, `Text_UpgradeEffect`, `Text_UpgradeCost`, `Text_UpgradeReason`, `Text_Upgrade`는 `CommonTextBlock`이다. 이 이름들은 네이티브 `BindWidgetOptional` 계약이며, 생성된 WBP에서는 모두 변수로 노출한다.
- 프로그램: `Panel_ProgramSection`, 상태/요약/사유 텍스트와 `Button_ScheduleProgram`.
- 행동: `Panel_Actions`, `Panel_BuildActions`, `Button_Move`, `Button_Remove`; 레이블, 단축키 표기와 상태 브러시를 정리한다.
- Picker: `Border_Picker`, `Button_Close`, `Text_FacilityName`, `Text_Empty`, `ScrollBox_Programs`, `Text_PreviewName`, `Text_PreviewDescription`, `Text_PreviewDetails`, `Text_PreviewAvailability`, `Text_NoRefundNotice`, `Text_Feedback`, `Button_Confirm`, `Text_Confirm`.
- Card: `Button_Select`, `Text_ProgramName`, `Text_ProgramMeta`, `Text_ProgramAvailability`.

`BindWidget` 이름과 위젯 타입, `Panel_Size`와 `Border_Picker`의 Canvas 직접 자식 관계는 C++/자동화 계약이므로 이름 변경·삭제·타입 교체를 금지한다. 표시 데이터, 섹션 visibility, 버튼 활성화와 클릭 권한은 네이티브 코드가 소유한다.

현재 수익은 `FacilityRevenueText`, 다음 수익 비교는 `UpgradeRevenueEffectText`, 크기 효과는 `UpgradeSizeEffectText`에서 만든다. 다음 수익·시간/처리량·크기는 기존 `Text_UpgradeEffect`에 합친다. 서비스 시간과 처리량, 비용과 잔액은 각각 한 줄에 표시해 강화 버튼까지 첫 화면에 담는다. 카드 높이는 변경하지 않는다.

## 생성·리프레시

에디터 콘솔에서 실행한다.

- `FF.BuildFacilityInfoUMG`: 기본 비파괴 명령. 시설 정보 패널, 유료 프로그램 Picker와 Card가 없을 때 생성한다. 기존 시설 정보 패널은 지원되는 가산 계약만 마이그레이션하며, 기존 Picker/Card는 계약을 검증해 재사용한다. 불일치한 기존 Picker/Card는 덮어쓰지 않고 오류로 보고한다.
- `FF.RefreshFacilityPaidProgramUMG`: 시설 정보 패널은 보존하고 유료 프로그램 Picker와 Card의 WidgetTree만 LightTeal/Pretendard 템플릿으로 갱신한다.
- `FF.RefreshFacilityInfoUMG`: 리뷰 승인 후 1회 실행하는 명시적 리프레시. 시설 정보 패널뿐 아니라 유료 프로그램 Picker와 Card의 WidgetTree도 모두 LightTeal/Pretendard 템플릿으로 재생성한 뒤 Compile/Save한다. 경로와 부모 클래스는 유지하며, 실행 전 변경분을 커밋하거나 백업한다.
- `Automation RunTests FestaFiesta.UI.FacilityInfo.AssetContract`: 시설 정보 패널의 이름·타입·배치 계약을 검증한다.
- `Automation RunTests FestaFiesta.UI.FacilityInfo.PaidProgramWbpContract`: Picker/Card 이름·타입, 우측 도킹과 하단 Build UI `400px` 비겹침 계약을 검증한다. Picker/시설 패널 상호 배타 표시 흐름은 PIE에서도 별도로 확인한다.
- `Automation RunTests FestaFiesta.UI.FacilityInfo.UpgradeReadModel`: 강화 견적, 수익과 반올림·증가율·0 Gold, 수익 전용 강화, 크기 변화, 레벨 없는 시설, 부족액, Live 전환, 최대 레벨과 잘못된 가격 표시를 검증한다.
- `Automation RunTests FestaFiesta.UI.FacilityInfo.UpgradeVisualPreview`: 실제 시설 WBP에 Lv.1 → Lv.2 예시를 적용해 `388 × 500` PNG를 생성한다.

강화 UI를 기존 에셋에 반영할 때도 `FF.BuildFacilityInfoUMG`를 사용한다. 누락된 강화 위젯·현재 수익 위젯·변수 계약을 추가하고, 이미 존재하는 강화 영역도 요약 다음 순서로 필요한 경우 이동해 저장한다. 기존 노드·그래프·폰트·브러시·크기를 재생성하거나 덮어쓰지 않는다. 계약과 순서가 맞는 에셋은 그대로 재사용한다. 충돌하는 이름·타입이 있으면 오류로 보고한다.

시각 검증은 실제 RHI가 필요하다. 에디터를 `-RenderOffscreen`으로 실행한 뒤 위 테스트를 호출하거나, 프로젝트 루트의 PowerShell에서 실행한다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  (Resolve-Path '.\FestaFiesta.uproject').Path `
  -unattended -nop4 -nosplash -nosound -RenderOffscreen `
  '-ExecCmds=Automation RunTests FestaFiesta.UI.FacilityInfo.UpgradeVisualPreview' `
  '-TestExit=Automation Test Queue Empty'
```

출력은 프로젝트의 `Saved/Automation/FacilityUpgradePreview.png`다. `-NullRHI` 실행에서는 건너뛴다는 안내만 남기며 PNG를 생성하지 않는다. PNG 저장 테스트의 통과 여부와 별개로 결과 이미지에서 글자 잘림·색 대비·강화 버튼 표시를 직접 확인한다.

시설 강화의 실제 실행 결과와 제한된 쓰기 환경에서 사용한 DDC·셰이더 작업 경로 옵션은 [시설 강화 검증 기록](../../FacilityUpgrade/VALIDATION.md)에 정리한다. 기존 강화 위젯이 이미 있어도 마이그레이션은 요약 바로 다음 순서를 검사하여 필요한 경우 강화 영역만 이동한다.

마지막으로 세 WBP의 Designer와 PIE의 Build/Live 문맥을 모두 확인한다. 긴 시설명, 편성 불가 사유, 썸네일 없음, 최대 수치에서도 텍스트가 잘리지 않고 내부 스크롤이 동작해야 한다. Picker를 열고 닫거나 편성에 성공할 때 시설 패널과 Picker가 한 프레임도 겹쳐 보이지 않는지도 확인한다.
