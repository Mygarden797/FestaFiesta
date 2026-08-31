> **현재 상태 · 2026-08-28**  
> LightTeal 시안과 실제 UMG 패널·가상화 타일·전체 7종 카테고리·BuildMode 배치 연결·AlwaysCook label까지 구현했습니다. **기존 파일 삭제·이동·덮어쓰기는 0건**이며, BuildMode C++ 2개에는 기본값 `false`의 최소 opt-in/event만 가산 적용했습니다. Unreal 빌드와 자동화 7/7, 기존 회귀 2/2, 에셋 재실행 무변경까지 통과했습니다.

{{VISUAL_REVIEW_EMBED}}

# 1. 1분 요약

- 실제 게임의 시설 카탈로그와 배치 조작 UI는 **UMG로 구현**합니다.
- 웹 UI는 리뷰·레이아웃 실험에는 유용하지만 출시 런타임에는 사용하지 않습니다.
- 종태님 HUD의 LightTeal 팔레트, Pretendard, 하단 도킹 구조를 그대로 기준으로 삼습니다.
- 시설 데이터, 가격, 배치 가능 판정, 회전, 확정, 취소, 저장은 기존 시스템을 재사용합니다.
- 목록은 `UTileView` 가상화, 비동기 썸네일, 이벤트 기반 갱신으로 최적화합니다.
- `전체 + 부스·화장실·안내·휴게·공연·안전·포토스팟` 8개 선택 칩을 가로 스크롤로 제공하고 선택 상태를 필터와 동기화합니다.
- 신규 패널·카드·모델·테스트·WBP·Cook label은 모두 신규 경로에 구현했습니다.
- 기존 WBP·이미지·설정은 건드리지 않았고, BuildMode의 최소 opt-in/event만 가산 적용했습니다.
- Unreal 5.7 Editor 빌드와 PrepBuild 자동화 7/7, 기존 BuildMode 2/2, AlwaysCook 및 위젯 계약 테스트를 통과했습니다.

## 발표할 때 한 문장

“종태님 HUD의 톤을 유지한 UMG 표현 계층을 새 경로에 구현하고, 게임 규칙은 기존 시스템을 재사용하며, 기존 자산은 삭제·덮어쓰기 없이 최소 opt-in으로 연결한 방식입니다.”

# 2. 왜 UMG인가 — UMG와 웹 UI 판단

| 항목 | UMG | 웹 UI |
|---|---|---|
| Unreal 입력·Enhanced Input | 직접 연결 | 별도 브리지 필요 |
| 게임패드 포커스 | 엔진 경로에서 관리 | 별도 구현·검증 필요 |
| DPI·Safe Zone | 프로젝트 설정과 통합 | 브라우저 스케일과 이중 관리 |
| 현지화·폰트 | Unreal Localization과 통합 | 별도 문자열·폰트 경로 필요 |
| 에셋 Cook·패키징 | 동일 파이프라인 | CEF/플랫폼 검증 비용 추가 |
| 월드 배치 상태 동기화 | 동일 UObject 생명주기 | 메시지 왕복과 상태 불일치 위험 |
| 빠른 리뷰 시안 | 보통 | 매우 유리 |

**결정:** 출시 UI는 UMG, 웹은 선택적인 검토 프로토타입까지만 사용합니다. 이 화면은 월드 입력과 배치 상태가 밀접하므로 UMG의 이점이 특히 큽니다.

# 3. 사용자 흐름

```text
배치 탭 선택
  ↓
LightTeal 시설 카탈로그 열림
  ↓
카테고리 / 검색 / 가격 확인
  ↓
시설 카드 선택
  ↓
카탈로그 축약 + 월드 배치 프리뷰
  ↓
회전 ─ 확정 ─ 취소
          │       └─ 기존 필터·스크롤의 카탈로그로 복귀
          └─ 배치 결과 반영
```

핵심은 **찾는 순간에는 정보가 충분하고, 놓는 순간에는 월드가 주인공**이 되는 것입니다. 화면 전체를 어둡게 덮는 모달은 사용하지 않습니다.

# 4. 종태님 UI와 맞추는 디자인 기준

프로젝트의 `FF_UIPalette.h`와 Pretendard를 소스 오브 트루스로 사용합니다. 생성 시안의 색을 임의로 스포이드하지 않습니다.

| 토큰 | 값 | 용도 |
|---|---:|---|
| Paper | `#F7F8F6` | 패널·카드 기본 면 |
| PaperSunken | `#EEF2EF` | 검색창·비활성 면 |
| StatusBar | `#E8EDE9` | 하단 상태 바 |
| Ink | `#24332E` | 제목·주요 수치 |
| InkMuted | `#6F7B75` | 설명·메타 정보 |
| Teal | `#527C6B` | 선택·주요 액션 |
| TealDeep | `#315747` | 강조·호버·눌림 |
| Line | `#CCD5CF` | 구분선·테두리 |
| Amber | `#E39A22` | 비용 관련 주의 |
| Danger | `#D74E43` | 배치 불가·오류 |

- 폰트: `/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard`
- 패널 제목: 22px SemiBold
- 패널 반경: 8px
- 카드·버튼 반경: 6px
- 탭·칩 반경: 4px
- 확장 패널: 좌우·하단 12px 여백, 기본 약 320px, 뷰포트 높이 최대 40%
- 장식보다 정보 위계와 여백을 우선합니다.
- 색만으로 상태를 전달하지 않고 문구, 아이콘, 버튼 상태를 함께 사용합니다.

# 5. 구현된 UMG 구조

```text
WBP_FF_PrepPanelBuild (UFF_PrepPanelBase)
└─ SafeZone
   └─ Border_Panel
      └─ VerticalBox
         ├─ Header: 제목 / 결과 수 / 닫기
         ├─ ToolRow: 가로 스크롤 전체+7종 카테고리 / 검색 / 현재 골드
         ├─ WidgetSwitcher_State
         │  ├─ Loading
         │  ├─ Empty
         │  ├─ TileView_Facilities
         │  ├─ PlacingCompact
         │  └─ Error
         └─ Footer: 확정 / 회전 / 취소 힌트
```

`WBP_FF_PrepPanelBuild`라는 정확한 이름은 기존 `UFF_PrepHudBar::ResolvePanelClass` 규칙으로 자동 탐색할 수 있습니다. 기존 HudBar나 PanelClasses를 수정하지 않아도 되는 공식 결합 지점입니다.

BuildMode에는 external presentation opt-in을 적용했습니다. 기본값 `false`에서는 기존 전체 화면 카탈로그가 그대로 동작하고, 신규 패널이 opt-in한 동안에만 중복 CatalogScreen 생성을 건너뜁니다.

# 6. 책임 분리

| 계층 | 담당 | 책임 |
|---|---|---|
| 신규 UMG 패널 | UI 작업자 | 레이아웃, 필터, 검색, 카드, 포커스, 상태 표시 |
| 신규 TileData | UI 작업자 | 시설 엔트리를 화면용 스냅샷으로 변환 |
| 기존 BuildCatalogSubsystem | 기존 시스템 | 시설 목록, 카테고리, 비동기 썸네일 |
| 기존 EconomyReadModel | 기존 시스템 | 현재 축제 기준 실제 가격 계산 |
| 기존 BuildModeSubsystem | 해당 파일 소유자 | 배치 세션, 판정, 회전, 확정, 취소, 저장 |
| PresentationRules | UI 작업자 | 상태→화면, 필터, 구매 가능 여부의 순수 규칙 |

이 분리의 목적은 새 UI가 게임 규칙을 복제하지 않도록 하는 것입니다. 같은 시설 가격이나 배치 가능 여부를 UI에서 다시 계산하지 않습니다.

# 7. 이벤트 기반 데이터 흐름

```text
Panel Open
  → 카탈로그/썸네일/골드/BuildMode 이벤트 구독
  → 시설 목록과 카테고리 조회
  → EconomyReadModel로 현재 비용 투영
  → TileData 목록을 한 번에 생성
  → TileView에 교체

시설 카드 선택
  → 시설 Definition ID를 기존 BuildMode에 전달
  → BuildMode 상태 변경 이벤트 수신
  → WidgetSwitcher를 Catalog / Placing / Moving / Closed로 전환

Panel Close
  → 모든 delegate 해제
  → thumbnail preload 참조 반환
```

프레임마다 `Tick`으로 상태나 골드를 조회하지 않습니다. 값이 바뀐 순간에만 필요한 부분을 갱신합니다.

# 8. 팀 파일 보호 규칙

이번 작업과 이후 UI 제작에서 다음을 고정 규칙으로 사용합니다.

- 기존 WBP, C++, 이미지, 설정 파일을 덮어쓰지 않습니다.
- 기존 파일을 rename, move, delete하지 않습니다.
- 기존 WBP를 열어 둔 상태에서 `Save All`을 하지 않습니다.
- 기존 폴더에 Fix Redirectors를 실행하지 않습니다.
- 빌더는 없는 신규 패키지만 만들고 같은 이름이 있으면 중단합니다.
- 팀 소유 파일 변경은 기능 연결에 필요한 최소 범위와 기본값 회귀 안전장치를 명시합니다.
- 작업 종료 시 `D`, `R` 또는 의도하지 않은 기존 경로의 `M`이 보이면 즉시 중단합니다.

## 신규 파일 구현 경로

```text
Source/FestaFiesta/Public/UI/PrepHUD/Build/
Source/FestaFiesta/Private/UI/PrepHUD/Build/
Content/FestaFiesta/UI/PrepHUD/Build/
Design/UXUI/PrepBuild_LightTeal/
```

동적 이름 탐색 에셋이 패키징에서 빠지지 않도록 신규 `PAL_FF_PrepBuildUI`와 `PAL_FF_PrepBuildCategoryDependencies` PrimaryAssetLabel을 신규 경로에 두었습니다. 팀원의 공용 ChoiceButton은 수정하지 않고 읽기 전용 참조합니다.

# 9. 개발 단계와 완료 기준

## A. 디자인 고정 — 완료

- LightTeal 카탈로그 열림 상태 시안 제작
- 배치 중 축약 상태 시안 제작
- 팔레트, 폰트, 간격, 상태 전환 원칙 정의

**완료 기준:** 월드 가시성과 기존 HUD 연결감을 이미지로 합의할 수 있습니다.

## B. 표현 규칙과 테스트 — 완료

- 카테고리, 검색, 구매 가능 여부, 로딩·빈 상태 규칙을 순수 함수로 분리
- UI 에셋 없이 Automation Test 작성

**완료 기준:** 상태 규칙 테스트가 모두 통과합니다.

## C. 독립 UMG 데이터 프리뷰 — 완료

- 신규 패널, TileData, `IUserObjectListEntry` 카드 생성
- 기존 카탈로그와 가격 모델을 읽기 전용으로 연결
- TileView 가상화와 썸네일 preload 수명 검증

**완료 기준:** 실제 데이터의 카테고리, 검색, 가격, 로딩·오류·빈 상태가 표시됩니다.

## D. 팀 소유 연동 지점 — 완료

- 기본값 `false`인 외부 카탈로그 표현 opt-in
- BuildMode 상태 변경 delegate
- 기본값과 위젯 계약 자동화 테스트

**완료 기준:** 기존 경로는 변화가 없고, 신규 경로에서 중복 화면 없이 카드 선택→배치→취소→목록 복귀가 됩니다.

## E. 패키징·성능·접근성 — 자동화 완료 / 수동 QA 잔여

- AlwaysCook label과 명시 WBP 참조 자동화 확인
- Development 패키지 실제 실행 확인
- Slate/Unreal Insights 측정
- 해상도, DPI, 마우스·키보드·게임패드 검증

**완료 기준:** 11장의 QA 체크리스트를 모두 통과합니다.

# 10. 실제 배치 연결에 필요한 최소 연동안

> **적용 완료:** 아래 최소 연동은 `FF_BuildModeSubsystem.h/.cpp`에 기존 기능 삭제 없이 가산 적용했습니다.

## 10-1. 외부 카탈로그 표현 opt-in

개념 API:

```cpp
void SetExternalCatalogPresentationActive(bool bActive);
bool IsExternalCatalogPresentationActive() const;
```

- 기본값은 반드시 `false`로 둡니다.
- `false`: 기존 전체 화면 CatalogScreen이 그대로 동작합니다.
- `true`: CatalogOpen의 세션·입력·상태·썸네일 수명은 유지하되 기존 화면 생성만 생략합니다.
- 배치 비용, 판정, 저장 규칙은 플래그로 분기하지 않습니다.

## 10-2. BuildMode 상태 변경 이벤트

상태 전환을 한 helper로 모으고, 실제로 값이 달라질 때만 `OldState`와 `NewState`를 알립니다. 신규 패널은 이 이벤트로 `Catalog / Placing / Moving / Closed` 화면을 전환합니다.

이 확장이 없으면 UI가 매 프레임 polling하거나 입력 결과를 추측해야 하므로 현업 방식으로 권장하지 않습니다.

## 채택하지 않는 우회 방식

카드 클릭에서 기존 `OpenCatalog()`를 호출한 즉시 `StartPlacement()`를 호출하면 화면이 잠깐 생성됐다 사라지게 만들 수 있습니다. 그러나 B/Esc 복귀 시 기존 화면이 다시 나오고, 불필요한 위젯 생성과 입력 전환이 생기므로 출시 구현으로 사용하지 않습니다.

# 11. 최적화 체크리스트

- [x] UI 갱신을 위해 Tick을 사용하지 않는다.
- [x] 시설 목록은 `UTileView` 가상화를 사용한다.
- [x] 카드 내부에서 텍스처를 동기 로드하지 않는다.
- [x] 패널 열기/닫기의 thumbnail preload 참조가 대칭이다.
- [x] 검색·필터 결과는 카드별 추가가 아니라 목록을 한 번에 교체한다.
- [ ] 실제 카드 크기보다 과도하게 큰 텍스처를 사용하지 않는다.
- [ ] 반복 애니메이션을 피하고 짧은 위치·투명도 전환만 사용한다.
- [ ] 숨긴 위젯은 `Collapsed`와 Hit Test 설정을 상태별로 검증한다.
- [ ] 최적화는 추측이 아니라 Slate/Unreal Insights 측정 뒤 결정한다.

# 12. QA 체크리스트

## 기능

- [ ] 배치 탭 한 번에 패널 하나만 열린다.
- [ ] 카테고리와 검색을 함께 적용해도 결과가 정확하다.
- [x] 전체 7종 카테고리 칩·한글 라벨·선택 상태가 실제 필터와 일치한다.
- [ ] 카드 가격이 현재 축제의 실제 가격과 일치한다.
- [ ] 구매 불가 시설은 선택할 수 없고 이유가 표시된다.
- [ ] 카드 선택 시 기존 전체 화면 카탈로그가 중복 표시되지 않는다.
- [ ] 배치 확정·회전·취소가 기존 BuildMode와 동일하다.
- [ ] 취소 뒤 필터·스크롤·포커스가 합리적으로 복원된다.

## 해상도와 입력

- [ ] 1920×1080
- [ ] 2560×1440
- [ ] 16:10 화면비
- [ ] Windows DPI 100% / 125% / 150%
- [ ] 마우스·키보드
- [ ] 게임패드 포커스와 Confirm/B 입력
- [ ] 빠른 연속 클릭과 탭 전환

## 성능과 패키징

- [x] 패널 닫힘 코드에서 delegate와 preload 참조를 해제한다.
- [x] `UTileView` 계약으로 전체 항목 위젯 생성을 피한다.
- [x] 타일은 이미 로드된 썸네일만 읽는다.
- [x] 패널·카드가 `AlwaysCook` label에 명시된다.
- [x] opt-in 기본값 `false`가 자동화로 고정된다.
- [ ] Development 패키지에서 폰트·아이콘 포함 실제 표시를 확인한다.

## 협업 안전

- [x] 신규 UI 산출물은 신규 경로에만 존재한다.
- [x] tracked `D`, `R`과 삭제 파일은 0건이다.
- [x] 기존 에셋을 overwrite하거나 이동하지 않았다.
- [x] BuildMode C++ 2개에만 최소 opt-in/event 변경을 적용했다.

# 13. 이미지 생성 원칙

이미지 생성은 시각 방향을 빠르게 합의하기 위해 사용했습니다. 1차는 카탈로그 열림 상태를 새로 생성했고, 2차는 같은 시안을 참조해 배치 상태로 편집했습니다.

제품에서는 다음 원칙을 지킵니다.

- 생성 이미지 속 문구와 숫자를 최종 UI에 사용하지 않습니다.
- 시설명, 가격, 골드, 상태, 버튼 힌트는 UMG Text/Icon으로 구성합니다.
- 실제 시설 썸네일은 기존 카탈로그의 Soft reference와 비동기 preload를 사용합니다.
- 생성 이미지는 콘셉트 또는 빈 상태 일러스트 범위로 제한합니다.
- 모든 신규 이미지는 고유 파일명으로 저장하고 기존 이미지를 덮어쓰지 않습니다.

# 14. 의사결정 기록(ADR)

## ADR-01 · 런타임 UI는 UMG

- 결정: UMG/CommonUI 계층 사용
- 이유: 입력, 포커스, DPI, 현지화, Cook, 플랫폼 대응을 한 경로로 관리
- 제외: WebBrowser 기반 출시 UI

## ADR-02 · 기존 로직 재사용

- 결정: 카탈로그, 가격, 배치 판정과 저장은 기존 시스템 사용
- 이유: 동일 규칙의 이중 구현과 상태 불일치 방지

## ADR-03 · 팀 파일 비삭제·최소 변경

- 결정: 기존 에셋은 무수정, 연결에 필요한 BuildMode C++만 가산 변경
- 이유: 병렬 작업 충돌을 피하면서 실제 기능을 우회 없이 연결

## ADR-04 · AI 이미지는 콘셉트 레퍼런스

- 결정: 동적 텍스트와 상태는 UMG에서 구현
- 이유: DPI, 현지화, 접근성, 실제 데이터 정확성 보장

# 15. 내가 설명할 때 쓰는 1분 브리핑

“이번 UI는 종태님 HUD를 교체하지 않고 LightTeal 팔레트, Pretendard, 하단 도킹 규칙을 이어 실제 UMG로 구현했습니다. 전체 7종 시설 카테고리는 가로 스크롤 선택 칩으로 제공하며 시설 목록과 가격은 기존 카탈로그·경제 읽기 모델, 배치 판정과 저장은 기존 BuildMode를 재사용합니다. TileView 가상화, UObject 풀링, 비동기 썸네일, 이벤트 갱신으로 최적화했고, 기존 에셋은 삭제·덮어쓰기하지 않았습니다. BuildMode에는 기본값 false의 표현 opt-in과 상태 이벤트만 추가했습니다. Editor 빌드와 자동화 7/7, 기존 회귀 2/2, AlwaysCook 계약, 생성기 재실행 해시 동일까지 검증했습니다.”

# 16. 진행 기록 템플릿

```text
날짜:
작업자:
현재 단계:
이번에 만든 신규 파일:
확인한 기존 파일(읽기 전용):
검증한 해상도·입력:
성능 측정 결과:
기존 파일 변경 여부: 없음 / 있음(즉시 중단)
남은 결정:
담당자 검토 요청:
```

# 17. 로컬 산출물

| 산출물 | 역할 |
|---|---|
| `README.md` | 결정과 현재 완료 범위 요약 |
| `UMG_PRODUCTION_GUIDE.md` | UMG 구조, 개발 순서, 성능과 QA 상세 |
| `OWNER_INTEGRATION_PROPOSAL.md` | 팀 소유 최소 연동안과 수용 기준 |
| `IMAGEGEN_PROVENANCE.md` | 생성 방식, 최종 프롬프트, 실제 사용 원칙 |
| `VALIDATION_REPORT.md` | 이미지·Notion·참조 경로·Git 무변경 검증 결과 |
| `FF_PrepBuild_CatalogOpen_LightTeal_v1.png` | 카탈로그 열림 시안 원본 |
| `FF_PrepBuild_Placing_LightTeal_v1.png` | 배치 중 시안 원본 |

# 18. 참고한 프로젝트 경로

- `Source/FestaFiesta/Public/UI/Data/FF_UIPalette.h`
- `Source/FestaFiesta/Private/UI/PrepHUD/FF_PrepHudBar.cpp`
- `Source/FestaFiesta/Public/UI/PrepHUD/FF_PrepPanelBase.h`
- `Source/FestaFiesta/Public/Building/FF_BuildCatalogSubsystem.h`
- `Source/FestaFiesta/Public/Building/FF_BuildCatalogEconomyReadModel.h`
- `Source/FestaFiesta/Public/Building/FF_BuildModeSubsystem.h`
- `Source/FestaFiesta/Private/Building/FF_BuildModeSubsystem.cpp`

---

최종 문서 기준: **기존 파일 삭제·이동·덮어쓰기 0 / 전체 7종 카테고리 / 신규 산출물 분리 / BuildMode 최소 opt-in 연동 / 자동화 7/7 + 회귀 2/2**
