# 시설 배치 UI — UMG 프로덕션 가이드

## 1. 한 줄 결론

기존 준비 HUD를 삭제·교체하지 않고 신규 `LightTeal` 시설 패널을 UMG 표현 계층으로 추가했다. 시설 데이터·가격·배치 판정은 기존 서브시스템을 재사용하고, BuildMode에는 기본값 `false`의 표현 opt-in과 상태 이벤트만 가산적으로 적용했다.

**구현·검증 상태:** Unreal 5.7 Editor 빌드 성공, 전체 7종 카테고리 계약을 포함한 `FestaFiesta.UI.PrepBuild` 7/7 및 기존 `FestaFiesta.Building.BuildMode` 회귀 2/2 성공, 신규 WBP 2개와 AlwaysCook label 2개 생성, 생성기 재실행 전후 SHA-256 동일. 실제 맵의 DPI·게임패드·패키지 실행은 수동 통합 QA 항목이다.

## 2. 사용자 경험 목표

사용자는 준비 단계에서 다음 흐름을 끊김 없이 이해해야 한다.

1. 하단 메뉴의 `배치`를 선택한다.
2. 기존 HUD의 연장선처럼 보이는 시설 카탈로그가 열린다.
3. 전체·부스·화장실·안내·휴게·공연·안전·포토스팟 카테고리나 검색으로 시설을 찾는다.
4. 카드를 선택하면 카탈로그가 축약되고 월드의 배치 프리뷰에 집중한다.
5. 회전·확정·취소 입력이 가까운 위치에 명확하게 보인다.
6. 취소하면 동일한 필터와 스크롤 위치의 카탈로그로 돌아온다.

## 3. UMG와 웹 UI 판단

### 출시 런타임은 UMG

UMG는 Unreal의 입력 모드, Enhanced Input, 게임패드 포커스, DPI 스케일, Safe Zone, 현지화, 에셋 Cook과 동일한 생명주기에서 동작한다. 현재 프로젝트도 UMG/CommonUI 기반이므로 유지보수 경로가 하나로 유지된다.

### 웹 UI가 적합한 범위

웹은 빠른 프로토타입, 이해관계자 리뷰, 레이아웃 실험에는 유용하다. 다만 실제 게임에서 WebBrowser/CEF를 사용하면 별도의 포커스, 이벤트 브리지, 텍스처 메모리, IME, 패키징과 플랫폼 검증이 필요하다. 이 화면처럼 월드 배치 입력과 밀접한 기능에서는 비용 대비 이점이 작다.

따라서 이번 시안 이미지는 리뷰 기준으로 사용하고, 실제 구성은 UMG로 옮긴다.

## 4. 기존 톤앤매너를 지키는 토큰

현재 `FF_UIPalette.h`의 값을 단일 기준으로 사용한다.

| 토큰 | 값 | 권장 용도 |
|---|---:|---|
| Paper | `#F7F8F6` | 패널과 카드의 기본 면 |
| PaperSunken | `#EEF2EF` | 검색창, 비활성 영역, 축약 컨트롤 배경 |
| StatusBar | `#E8EDE9` | 하단 상태 바 |
| Ink | `#24332E` | 제목과 주요 수치 |
| InkMuted | `#6F7B75` | 보조 설명과 메타 정보 |
| Teal | `#527C6B` | 선택, 주요 액션, 활성 탭 |
| TealDeep | `#315747` | 강조 텍스트와 호버/누름 상태 |
| Line | `#CCD5CF` | 구분선과 카드 테두리 |
| Amber | `#E39A22` | 주의와 비용 부족 임박 상태 |
| Danger | `#D74E43` | 배치 불가와 오류 |

- 폰트: `/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard`
- 패널 제목: 22px SemiBold
- 패널 반경: 8px
- 카드·버튼 반경: 6px
- 탭·칩 반경: 4px
- 기존 하단 HUD 높이: 메뉴 29px + 상태 29px
- 확장 패널: 좌우·하단 12px 여백, 기본 높이 약 320px, 뷰포트 높이의 최대 40%
- 모달 스크림은 사용하지 않고 월드 가시성을 유지한다.

## 5. 권장 화면 구조

```text
WBP_FF_PrepPanelBuild (UFF_PrepPanelBase)
└─ SafeZone
   └─ Border_Panel
      └─ VerticalBox
         ├─ Header
         │  ├─ Text_Title "시설 배치"
         │  ├─ Text_ResultCount
         │  └─ Button_Close
         ├─ ToolRow
         │  ├─ Horizontal ScrollBox: 전체 + 7종 CategoryChips
         │  ├─ SearchBox
         │  └─ Text_CurrentGold
         ├─ WidgetSwitcher_State
         │  ├─ View_Loading
         │  ├─ View_Empty
         │  ├─ TileView_Facilities
         │  ├─ View_PlacingCompact
         │  └─ View_Error
         └─ Footer_Hints
            ├─ Confirm
            ├─ Rotate
            └─ Cancel
```

`TileView_Facilities`는 카드 수가 늘어나도 보이는 엔트리만 생성하도록 반드시 가상화한다. 시설 카드를 `ScrollBox`에 직접 전부 생성하지 않는다.

## 6. 권장 코드 책임

| 신규 계층 | 책임 | 하지 않는 일 |
|---|---|---|
| `UFF_PrepPanelBuild` | 패널 생명주기, 이벤트 구독, 필터, 상태 전환, 카탈로그와 골드 표시 | 배치 판정과 저장 |
| `UFF_PrepBuildTileData` | 시설 엔트리의 UI 스냅샷, 가격·구매 가능 여부 | 원본 정의 소유 |
| `UFF_PrepBuildTileWidget` | `IUserObjectListEntry` 기반 카드 렌더링 | 동기 에셋 로드 |
| `FF_PrepBuildPresentationRules` | 7종 카테고리 순서·한글 라벨, 상태→뷰, 필터, 빈 상태, 구매 가능 여부의 순수 규칙 | UObject 접근 |
| 기존 `UFF_BuildCatalogSubsystem` | 카탈로그, 카테고리, 비동기 썸네일 | 패널 레이아웃 |
| 기존 `UFF_BuildCatalogEconomyReadModel` | 현재 축제 기준 비용 투영 | 버튼 상태 렌더링 |
| 기존 `UFF_BuildModeSubsystem` | 배치 세션, 검증, 회전, 확정, 취소 | 신규 패널 디자인 |

## 7. 이벤트 기반 데이터 흐름

```text
사용자가 배치 탭 선택
  → PrepHudBar가 이름 규칙으로 신규 패널 탐색
  → NativeOnPanelOpened
  → CatalogSubsystem 이벤트 구독 + 썸네일 preload 참조 획득
  → GetEntries / GetCategoryGroups
  → EconomyReadModel로 현재 가격 투영
  → TileData 목록 생성
  → TileView에 한 번에 교체
  → 카드 선택 이벤트
  → BuildMode에 시설 ID 전달
  → 상태 변경 delegate
  → WidgetSwitcher가 Catalog/Placing/Moving/Closed로 전환
  → NativeOnPanelClosed에서 delegate 해제 + preload 참조 반환
```

`Tick`에서 상태, 골드, 카탈로그를 매 프레임 조회하지 않는다. 카탈로그 변경, 썸네일 준비, 골드 변경, BuildMode 상태 변경 이벤트에서 필요한 부분만 갱신한다.

## 8. 실제 구현 경로

아래 신규 경로에 구현했다. 기존 경로 수정은 BuildMode `.h/.cpp`의 opt-in/event 2개 파일뿐이다.

```text
Source/FestaFiesta/Public/UI/PrepHUD/Build/
  FF_PrepBuildPresentationModel.h
  FF_PrepBuildPresentationRules.h
  FF_PrepBuildPresentationTypes.h
  FF_PrepBuildTileData.h
  FF_PrepBuildTileWidget.h

Source/FestaFiesta/Public/UI/PrepHUD/Panels/
  FF_PrepPanelBuild.h

Source/FestaFiesta/Private/UI/PrepHUD/Build/
  FF_PrepBuildPresentationModel.cpp
  FF_PrepBuildPresentationRules.cpp
  FF_PrepBuildTileData.cpp
  FF_PrepBuildTileWidget.cpp
  FF_PrepBuildWbpBuilder.cpp
  Tests/FF_PrepBuildPresentationTests.cpp
  Tests/FF_PrepBuildWidgetContractTests.cpp

Source/FestaFiesta/Private/UI/PrepHUD/Panels/
  FF_PrepPanelBuild.cpp

Content/FestaFiesta/UI/PrepHUD/Panels/
  WBP_FF_PrepPanelBuild.uasset

Content/FestaFiesta/Building/FF_WBP_Building/Catalog/
  WBP_FF_PrepBuildTile.uasset
  PAL_FF_PrepBuildUI.uasset
  PAL_FF_PrepBuildCategoryDependencies.uasset

Design/UXUI/PrepBuild_LightTeal/
  Generated/
  문서 산출물
```

`WBP_FF_PrepPanelBuild`라는 정확한 이름을 사용하면 기존 `UFF_PrepHudBar::ResolvePanelClass` 규칙으로 자동 탐색할 수 있다. 카테고리 행은 기존 패널 WBP를 변경하지 않고 `HorizontalBox_Tools`에 런타임 ScrollBox를 삽입해 공용 `WBP_FF_PrepChoiceButton` 8개를 만든다. 패널·카드와 동적 ChoiceButton 참조가 패키징에서 빠지지 않도록 서로 독립된 신규 `PrimaryAssetLabel` 2개를 AlwaysCook으로 둔다.

## 9. 개발 순서

### 단계 A — 디자인 고정

- 생성 시안으로 `카탈로그 열림`과 `배치 중` 두 핵심 상태를 합의한다.
- 기존 팔레트와 Pretendard를 디자인 토큰의 원본으로 지정한다.
- 텍스트, 수치, 시설 썸네일은 생성 이미지에 합성하지 않고 UMG 요소로 분리한다.

완료 기준: 1920×1080에서 월드 가시성과 HUD 연결감이 승인된다.

### 단계 B — 표현 규칙과 테스트 — 완료

- 카테고리 필터, 검색, 구매 가능 여부, 빈 상태, 로딩 상태를 순수 규칙으로 작성한다.
- Automation Test로 규칙을 먼저 검증한다.
- 패널이나 이미지가 없어도 테스트 가능한 구조를 유지한다.

완료 기준: UI 상태 규칙 테스트가 모두 통과한다.

### 단계 C — UMG 데이터 프리뷰 — 완료

- `UFF_PrepPanelBase` 기반 신규 패널과 `UTileView` 카드를 만든다.
- 카탈로그와 가격은 기존 읽기 모델에서 가져온다.
- 썸네일은 `AcquireThumbnailPreload`/`ReleaseThumbnailPreload` 수명을 균형 있게 관리한다.
- 에디터 전용 빌더는 없는 신규 패키지만 생성하고 기존 에셋을 만나면 중단한다.

완료 기준: 카테고리, 검색, 가격, 로딩·오류·빈 상태가 실제 데이터로 보인다.

### 단계 D — 팀 소유 연동 지점 적용 — 완료

- `OWNER_INTEGRATION_PROPOSAL.md`의 opt-in 표현 모드와 상태 delegate를 가산적으로 적용했다.
- 기본값 `false`를 자동화 테스트로 고정했다.
- 카드 선택, 배치, 취소/복귀의 상태 전환을 기존 BuildMode API에 연결했다.

완료 기준: 기존 카탈로그 경로 회귀가 없고, 외부 표현 모드에서 중복 화면이 나타나지 않는다.

### 단계 E — 패키징·성능·접근성 검증 — 자동화 완료 / 수동 QA 잔여

- PrimaryAssetLabel의 `AlwaysCook`과 명시 참조를 자동화로 확인했다.
- PIE와 Development 패키지에서 실제 에셋 표시를 확인한다.
- Slate Insights/Unreal Insights로 위젯 수, invalidation, 프레임 비용을 측정한다.
- 마우스, 키보드, 게임패드, DPI와 화면비를 모두 확인한다.

완료 기준: QA 표의 필수 항목을 모두 통과한다.

## 10. 성능 원칙

- `Tick` 금지: 값 변경 delegate로만 UI를 갱신한다.
- `UTileView` 가상화: 화면에 보이는 카드만 생성·재사용한다.
- Soft reference와 비동기 로딩: 카드 생성 중 `LoadSynchronous`를 호출하지 않는다.
- 썸네일 선로딩 참조 수명: 패널 열기/닫기와 대칭을 이룬다.
- 리스트 갱신 배치: 필터 입력마다 카드를 개별 추가하지 말고 필터 결과를 한 번에 교체한다.
- 이미지 크기 예산: 카드 실표시 크기보다 과도하게 큰 텍스처를 넣지 않는다.
- 정적 레이어는 invalidation 적용 후보로 두되, 측정 후 적용한다.
- 애니메이션은 위치·투명도 중심의 짧은 전환만 사용하고 반복 애니메이션을 피한다.
- 숨긴 위젯이 입력을 가로채지 않도록 `Collapsed`와 Hit Test 설정을 상태별로 검증한다.

## 11. 입력과 포커스

- 패널이 열려도 월드 카메라 입력이 필요한지 상태별로 명시한다.
- 카탈로그 상태: UI 탐색 포커스를 유지하고 첫 번째 사용 가능한 카드에 포커스를 준다.
- 배치 상태: 월드 입력을 우선하고 패널은 `SelfHitTestInvisible` 또는 축약 조작부만 Hit Test 가능하게 한다.
- `Esc/B`: 배치 중이면 카탈로그 복귀, 카탈로그면 패널 닫기라는 일관된 계층을 사용한다.
- 가로 카테고리 행은 포커스 이동 시 선택 칩을 자동으로 화면 안에 스크롤하고, CatalogOpen 진입·복귀 때 선택 칩의 실제 버튼 루트로 포커스를 복원한다.
- 동일 패널 인스턴스를 viewport에서 제거했다 다시 붙여도 `NativeConstruct`/`NativeDestruct` 대칭 바인딩으로 검색·칩·타일 입력을 복구한다.
- 카드 비활성은 색만 바꾸지 말고 가격 부족 문구와 비활성 버튼 상태를 함께 제공한다.

## 12. QA 체크리스트

### 기능

- [ ] 배치 탭을 한 번 눌렀을 때 패널이 하나만 열린다.
- [x] 카테고리와 검색 결합 규칙 및 결과 필터가 정확하다.
- [x] 전체 7종 카테고리 칩과 한글 라벨·결정적 순서가 정확하다.
- [x] 선택된 칩의 배경·문자 상태가 실제 필터와 동기화된다.
- [x] 가격·골드 미확정 상태를 `0 Gold`로 잘못 표시하지 않는다.
- [x] 구매 불가 시설은 선택할 수 없고 이유 상태가 분리된다.
- [ ] 카드 선택 시 기존 전체 화면 카탈로그가 중복 표시되지 않는다.
- [ ] 배치 확정, 회전, 취소가 기존 BuildMode 규칙과 동일하다.
- [ ] 취소 뒤 필터, 스크롤, 포커스가 합리적으로 복원된다.

### 해상도·입력

- [ ] 1920×1080
- [ ] 2560×1440
- [ ] 16:10 화면비
- [ ] Windows DPI 100% / 125% / 150%
- [ ] 마우스·키보드
- [ ] 게임패드 포커스 순환과 B/Confirm 입력
- [ ] 빠른 연속 클릭과 탭 전환

### 성능·패키징

- [x] 패널 닫힘 코드에서 delegate와 preload 참조를 대칭 해제한다.
- [x] 시설 목록이 `UTileView` 전용 가상화 위젯을 사용한다.
- [x] 타일 엔트리는 이미 비동기 로드된 썸네일만 읽는다.
- [x] 패널·카드 WBP와 동적 ChoiceButton 의존성이 AlwaysCook label에 명시적으로 포함된다.
- [x] opt-in 기본값 `false` 계약이 유지된다.
- [ ] 실제 Development 패키지에서 폰트·아이콘 포함 표시를 확인한다.

### 협업 안전

- [x] 신규 UI 코드·WBP·PAL·문서는 모두 신규 경로다.
- [x] tracked `D`, `R` 및 `git ls-files --deleted`는 0건이다.
- [x] 기존 WBP·이미지·설정 파일 수정은 0건이다.
- [x] 에셋 빌더 재실행은 WBP 2개와 PAL 2개를 모두 `REUSE`하고 바이트 해시를 유지했다.
- [x] 기존 C++ 변경은 BuildMode `.h/.cpp`의 최소 opt-in/event에 한정했다.

## 13. 보고용 1분 설명

“기존 종태님 HUD의 LightTeal 팔레트와 Pretendard, 하단 도킹 구조를 기준으로 실제 UMG 패널을 구현했습니다. 전체 7종 시설 카테고리는 가로 스크롤 선택 칩으로 제공하고, 작은 해상도에서도 검색창을 보존합니다. 시설 데이터·가격·배치 검증·저장은 기존 서브시스템을 재사용하며 목록은 TileView 가상화, 비동기 썸네일, UObject 풀링, 이벤트 갱신으로 최적화했습니다. 기존 에셋과 설정은 건드리지 않았고 BuildMode 두 파일에는 기본값 false의 opt-in과 상태 이벤트만 추가했습니다. 빌드와 자동화 7/7, 기존 회귀 2/2, AlwaysCook 계약, 에셋 재실행 무변경까지 검증했습니다.”

## 14. 진행 기록 템플릿

```text
날짜:
작업자:
단계:
이번에 만든 신규 파일:
확인한 기존 파일(읽기 전용):
검증한 화면/입력:
성능 측정 결과:
기존 파일 변경 여부: 없음 / 있음(즉시 중단 및 사유 기록)
남은 의사결정:
담당자 검토 요청:
```

## 15. 용어

- 표현 계층: 화면에 보이는 레이아웃과 사용자 입력을 담당하는 UI 부분
- 상태/도메인 계층: 실제 배치 가능 여부, 비용, 저장 같은 게임 규칙
- opt-in: 명시적으로 켠 경우에만 새 동작을 사용하는 방식
- delegate: 상태가 바뀐 순간 구독자에게 알리는 Unreal 이벤트
- virtualization: 목록의 모든 항목이 아니라 보이는 항목만 위젯으로 만드는 방식
- Cook: 패키지에 필요한 에셋을 포함하도록 Unreal이 변환하는 과정
- Soft reference: 에셋을 즉시 메모리에 올리지 않고 필요할 때 비동기로 불러오는 참조
