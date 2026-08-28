# FacilityInfo LightTeal

선택한 시설의 상태와 다음 행동을 한눈에 판단하는 비모달 운영 카드다. 지도와 하단 Build HUD를 계속 볼 수 있어야 하며, 장식보다 `상태 → 핵심 수치 → 프로그램 → 행동` 순서를 우선한다.

- 기준 시안: [FF_FacilityInfo_NoOverlap_LightTeal_v1.png](Generated/FF_FacilityInfo_NoOverlap_LightTeal_v1.png)
- 런타임 WBP: `/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel`
- 네이티브 계약: `UFF_FacilityInfoPanel`
- 데이터 원천: `FFF_FacilityInfoViewData`; Designer에 운영 수치를 하드코딩하지 않는다.

## 배치 계약

| 항목 | 값 |
|---|---:|
| Anchor / Alignment | `(1, 0)` / `(1, 0)` |
| Canvas Position | `(-24, 72)` |
| 고정 크기 | `388 × 500` |
| Canvas AutoSize | `false` |

루트 Canvas는 `SelfHitTestInvisible`을 유지한다. 카드 밖 월드 입력을 막는 스크림은 사용하지 않으며, 내용이 늘면 카드가 커지는 대신 내부 스크롤 또는 Build/Live 문맥별 섹션 전환으로 처리한다.

1080p 기준 시설 카드 하단은 `72 + 500 = 572px`다. Build HUD 상단은 `1080 - 상태 바 58 - 여백 12 - Build Host 320 = 690px`이므로 두 UI 사이에 `118px` 안전 간격이 남는다. `Panel_Size`, Y 오프셋 또는 Build Host 높이를 바꿀 때는 이 계산과 `FestaFiesta.UI.FacilityInfo.AssetContract`를 함께 갱신한다.

## 정보 구조

1. Teal 헤더: 문맥, 시설명, 닫기.
2. 요약: 썸네일, 분류·구역 칩, 현재 상태.
3. 운영 정보: Build에서는 크기·안전 여유·설치/회수 비용, Live에서는 대기·점유·처리량·예상 대기.
4. 프로그램: 편성 상태, 시간·규모, 편성 가능 여부와 사유.
5. 하단 행동: 이동은 중립/Teal, 철거는 Danger. 비활성은 색과 사유 문구를 함께 제공한다.

## LightTeal 토큰

색의 단일 원천은 `Source/FestaFiesta/Public/UI/Data/FF_UIPalette.h`의 `FFF_UIPaletteInGame`이다. C++에서는 직접 RGB 비율을 넣지 말고 `FLinearColor::FromSRGBColor`를 사용한다.

| 역할 | 토큰 | sRGB |
|---|---|---|
| 카드 면 / 보조 면 | `Paper` / `PaperSunken` | `#F7F8F6` / `#EEF2EF` |
| 헤더·주요 행동 / 포커스 | `Teal` / `TealDeep` | `#527C6B` / `#315747` |
| 본문 / 보조 / 비활성 글자 | `Ink` / `InkMuted` / `InkFaint` | `#24332E` / `#6F7B75` / `#94A09A` |
| 구분선 / 강조선 | `Line` / `LineStrong` | `#CCD5CF` / `#9DB1A5` |
| 주의 / 위험 / 정보 | `Amber` / `Danger` / `Info` | `#E39A22` / `#D74E43` / `#3D86B4` |

폰트는 `/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard`를 사용한다. 시설명은 20px SemiBold, 섹션 제목과 수치에는 Medium/SemiBold, 보조 라벨에는 Regular를 써서 위계를 만든다. 패널 모서리 8px, 내부 카드·버튼 6px, 칩 4px를 기준으로 한다.

## UMG Designer 편집 지점

- `Panel_Size`: 위 배치 계약을 유지한다.
- `Border_Panel`: Paper 면, Line 테두리, 그림자와 패딩.
- 헤더·요약: `Text_Context`, `Text_FacilityName`, `Button_Close`, `Image_Thumbnail`, `Text_Category`, `Text_Zone`, `Text_FacilityState`.
- 문맥 섹션: `Panel_BuildSection`, `Panel_LiveSection`; 브러시·간격·폰트·애니메이션은 수정 가능하다.
- 프로그램: `Panel_ProgramSection`, 상태/요약/사유 텍스트와 `Button_ScheduleProgram`.
- 행동: `Panel_Actions`, `Panel_BuildActions`, `Panel_LiveActions`, `Button_Move`, `Button_Remove`, `Button_StartMinigame`; 레이블, 단축키 표기와 상태 브러시를 정리한다.

`BindWidget` 이름과 위젯 타입, `Panel_Size`의 Canvas 직접 자식 관계는 C++/자동화 계약이므로 이름 변경·삭제·타입 교체를 금지한다. 표시 데이터, 섹션 visibility, 버튼 활성화와 클릭 권한은 네이티브 코드가 소유한다.

## 생성·리프레시

에디터 콘솔에서 실행한다.

- `FF.BuildFacilityInfoUMG`: 기본 비파괴 명령. 에셋이 없을 때 생성하고, 기존 에셋은 재사용하면서 누락된 계약만 마이그레이션한다. Designer 스타일을 덮어쓰지 않는다.
- `FF.RefreshFacilityInfoUMG`: 리뷰 승인 후 1회 실행하는 명시적 리프레시. 경로와 부모 클래스는 유지하지만 기존 WidgetTree와 Designer 스타일을 LightTeal 템플릿으로 재생성한 뒤 Compile/Save한다. 실행 전 변경분을 커밋하거나 백업한다.
- `Automation RunTests FestaFiesta.UI.FacilityInfo.AssetContract`: 생성 또는 리프레시 후 이름·타입·배치·Build HUD 비겹침 계약을 검증한다.

마지막으로 WBP Designer와 PIE의 Build/Live 문맥을 모두 확인한다. 긴 시설명, 편성 불가 사유, 썸네일 없음, 최대 수치에서도 텍스트가 잘리지 않고 내부 스크롤이 동작해야 한다.
