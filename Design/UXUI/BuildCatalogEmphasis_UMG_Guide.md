# 건축 카탈로그: 1일차 필수 시설과 프로그램 강조

시설 카드의 **주황색 `1일차 필수 설치`**는 아직 설치 조건을 충족하지 못한 메인시설입니다.
조건을 충족하면 **초록색 `1일차 필수 완료`**로 바뀝니다. **보라색 `프로그램`**은 현재 축제에서
편성할 수 있는 프로그램과 호환되는 시설입니다. 필수·프로그램·조합 배지는 함께 표시될 수 있습니다.
프로그램 배지는 구매·편성이 이미 완료되었다는 뜻이 아닙니다.

상단의 `메인시설은 1일차 시작 전 필수 설치` 안내는 필수 설치 조건을 충족하면 사라집니다.
같은 줄의 프로그램 범례가 제목 아래 왼쪽으로 당겨지며, 철거로 조건이 다시 부족해지면 안내가 나타납니다.
시설 카드의 초록색 완료 배지는 유지됩니다.

## Designer 편집 위치

| WBP | 위젯 | 편집 내용 |
|---|---|---|
| `/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile` | `Overlay_Thumbnail` → `Overlay_RequiredStatus` | 필수 배지 위치 |
| 같은 카드 | `Border_RequiredMissing`, `Text_RequiredMissing` | `1일차 필수 설치` 배경·문구·글꼴 |
| 같은 카드 | `Border_RequiredSatisfied`, `Text_RequiredSatisfied` | `1일차 필수 완료` 배경·문구·글꼴 |
| 같은 카드 | `Border_ProgramBadge`, `Text_ProgramBadge` | 프로그램 배경·문구·글꼴 |
| `/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild` | `VerticalBox_CatalogTitle` → `HorizontalBox_CatalogEmphasisGuide` | 기존 제목 아래 공간의 안내·범례 |
| 같은 패널 | `Border_CatalogRequiredGuide` → `Text_StudioHint` | `메인시설은 1일차 시작 전 필수 설치` 안내 |
| 같은 패널 | `Border_CatalogProgramLegend`, `Text_CatalogProgramLegend`, `Text_CatalogProgramGuide` | 보라색 프로그램 범례와 `편성 가능한 시설` 설명 |
| `/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/Legacy/WBP_FF_BuildFacilityCard` | `Thumbnail_Layer` 및 같은 배지 이름 | 구형 카탈로그의 동일 상태 표시 |

카드 배지의 Border/Text 이름과 타입, **Is Variable**을 유지합니다. 런타임은 데이터와 표시 상태를
갱신하며 배경·글꼴·문구·배치는 저장된 WBP에서 가져옵니다. 배지는 클릭을 가로채지 않는
`Hit Test Invisible` 상태로 표시됩니다. 데이터가 없거나 가상화 카드가 재사용되면 숨깁니다.

기본 86 높이 썸네일에서 즐겨찾기는 상단 3–33, 필수 배지는 왼쪽 위 35, 프로그램 배지는 왼쪽 아래
여백 3을 사용합니다. 기존 조합 배지는 카드 상단에 유지됩니다. 썸네일 높이나 글꼴 크기를 바꿀 때
필수·프로그램·조합 배지와 즐겨찾기가 동시에 표시되는 상태를 확인합니다.

상단 안내의 `Border_CatalogRequiredGuide`는 **Is Variable**을 유지합니다. 프로그램 범례의 왼쪽
슬롯 여백은 0이며, 두 안내 사이 간격은 필수 안내의 오른쪽 슬롯 여백으로 편집합니다. 그래야
필수 안내가 `Collapsed` 상태일 때 간격도 함께 사라집니다.

## 최초 적용

1. `FestaFiestaEditor Win64 Development`를 빌드합니다.
2. 에디터 콘솔의 `FF.RefreshBuildCatalogEmphasisUMG` 또는 새 에디터 프로세스의
   `-ExecutePythonScript=Tools/ui/apply_catalog_emphasis.py`로 적용합니다.
3. Python 스크립트는 위 WBP 3개를 백업하고, 강조 외 위젯의 상태·스타일·슬롯 보존과 재실행 시
   파일 해시 불변을 검증합니다. 결과는 `Artifacts/Validation/BuildCatalogEmphasis/asset-migration.json`입니다.
4. 렌더 가능한 RHI에서 PrepBuild 자동 검증을 실행하여 1280×720 / 1920×1080 배지 가독성과
   필수 충족 전후·프로그램 지원·카드 재활용을 확인합니다.

적용 버전 태그는 `FF.BuildCatalog.EmphasisVersion = 20260920.v1`입니다. 적용 후 같은 명령은
저장된 Designer 편집을 보존합니다. 전체 HUD나 카탈로그 레이아웃을 재생성하지 않으며, 기존 검색·분류·
즐겨찾기·페이지·배치 입력은 유지합니다.

조건부 상단 안내는 별도 `FF.BuildCatalog.RequiredGuideVersion = 1`로 한 번만 이전합니다.
기존 강조 버전이 적용된 경우에도 같은 적용 스크립트가 패널의 바인딩과 간격만 갱신합니다.

2026-09-20 최초 적용 전 세 WBP의 원본은 `Saved/Automation/BuildCatalogEmphasis/Before`에 별도로
백업했습니다. Python 검증의 추가 백업은 `Artifacts/Validation/BuildCatalogEmphasis/Backups`에 보관합니다.

## 데이터와 검증 결과 · 2026-09-20

- 필수 여부와 설치 충족은 축제의 `RequiredSetupFacilities` 및 기존 필수 시설 서브시스템에서 읽습니다.
  이름에 ‘메인’이 포함되는지만으로 필수 시설을 추가하지 않습니다.
- 프로그램 표시는 현재 축제의 `ProgramCatalog`와 기존 편성 규칙을 사용합니다. 툴팁에는 호환 프로그램
  이름과 설치 후 구매·편성 안내가 표시됩니다. 잔액·설치비 조회 실패와 별개이며 축제 변경 시 갱신됩니다.
- UE 5.7.4 `FestaFiestaEditor Win64 Development` 빌드 통과.
- 관련 자동 테스트 총 24종 통과: PrepBuild, 프로그램 호환 표시, 필수 시설 진행 조건, 골드 수명주기.
  최초 실행의 23종 성공 후, 렌더 테스트가 프로그램 시설의 실제 페이지를 선택하도록 검증 코드를 수정하고
  해당 1종을 재실행해 성공했습니다. 결과는 `Saved/Automation/BuildCatalogEmphasis/Report`와 `RenderReport`입니다.
- 실제 저장된 WBP와 축제 데이터로 1280×720 / 1920×1080 필수 설치·완료·프로그램 시설 화면 6개를
  렌더링했습니다. 배지 영역·비중첩·즐겨찾기 클릭·카드 재사용을 검증했습니다. 전체 맵 PIE 플레이 검증은 아닙니다.
  기존 공통 HUD 탐색 아이콘 에셋 누락 경고는 남아 있으며 이번 카탈로그 배지는 해당 텍스처를 사용하지 않습니다.
- 적용 명령을 재실행해 기존 위젯 145개의 상태 보존 및 세 WBP의 파일 해시 불변을 확인했습니다.
- 미리보기: `Saved/Automation/BuildableGrid/BuildCatalog_{해상도}_{RequiredMissing,RequiredSatisfied,ProgramFacilities}.png`.

### 상단 안내 접기 후속 검증

- 필수 조건 충족 시 상단 안내가 `Collapsed`로 전환되고 프로그램 범례가 제목 왼쪽에 맞춰지는 것을 확인했습니다.
  철거 후 안내가 다시 나타나며 두 안내 사이 간격도 복원됩니다.
- 일반 에디터 빌드와 PrepBuild·RequiredFacilities 자동 테스트 20종 통과, 실패·미실행 0.
  결과: `Saved/Automation/BuildCatalogRequiredGuide/Report/index.json`.
- 720p·1080p에서 미설치→설치→철거 화면과 프로그램 시설 화면 총 8개를 렌더 검증했습니다.
  철거 화면은 `BuildCatalog_{해상도}_RequiredRemoved.png`입니다.
- 이번 에셋 갱신은 `WBP_FF_PrepPanelBuild`만 저장했으며 두 시설 카드 WBP의 해시는 동일합니다.
