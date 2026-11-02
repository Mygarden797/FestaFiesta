# 건축모드 UMG 편집 안내

언리얼 콘텐츠 브라우저 기준 경로입니다. 아래 WBP를 Designer에서 직접 수정하고 Compile / Save 하세요. Python 마이그레이션 스크립트를 다시 실행하면 스타일이 덮어써질 수 있으므로 일상적인 디자인 수정에는 실행하지 않습니다.

| 화면 | 편집할 WBP |
|---|---|
| 시설 목록 | `/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild` |
| 시설 카드 | `/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile` |
| 배치 중 시설 설치·건축 기능 안내 | `/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools` |
| 설치된 시설의 정보·탭 | `/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel` |
| 조명 편집 | `/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityLightingControls` |
| 매출 순위와 행 | `/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRanking`, `WBP_FF_FacilityRevenueRankRow` |
| 시설 도감과 카드 | `/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen`, `WBP_FF_FacilityCodexCard` |
| 상단 아이콘 버튼과 바 | `/Game/FestaFiesta/Building/FF_WBP_Building/Controls/FeatureGuide/WBP_FF_BuildFeatureGuideButton`, `WBP_FF_BuildFeatureGuideToolbar` |

## 크기와 배치

- 목록: Class Defaults → Appearance / Catalog → **화면 폭에 맞추기**가 켜져 있으면 오른쪽 정보 영역(550)과 여백(60)을 제외한 화면 폭을 채웁니다. 조합 도감 버튼의 실제 폭 160을 목록에서 정보 패널로 재배분했습니다. `Size_ClarityCatalogDock`의 폭은 이 옵션을 끄면 사용됩니다. 높이는 Designer에서 편집합니다.
- 여백 60은 현재 왼쪽 HUD 여백 12 + 오른쪽 24 + 패널 사이 24입니다. 다른 담당자의 하단 HUD는 변경하지 않습니다.
- 배치 정보: `Size_PlacementInfo`의 크기·캔버스 슬롯, `Border_PlacementInfo`의 배경·패딩을 편집합니다.
- 설치된 시설 정보: Class Defaults → Appearance / Build Layout에서 크기와 여백을 편집합니다. 오른쪽 패널 폭을 변경하면 목록에 예약하는 폭도 함께 맞춰야 합니다.
- 목록의 `VerticalBox_CatalogBody`는 Fill을 유지하세요. Class Defaults의 `CardsPerPage=12`, `bUsePagination=false`, `VisibleCatalogRows=2`와 `TileView_Facilities`의 세로 Entry Spacing `0`이 두 줄을 구성합니다. `Size_A5CatalogRows`는 기본 높이 224이며, 크기 변경 시 각 행의 높이가 목록 높이에 맞춰 조정됩니다. 카드 내부 여백은 카드 WBP에서 편집합니다.
- 카드와 하단 범례의 `Size_A5RequiredBadge`, `Size_A5LightingBadge`, `Size_A5ProgramBadge`는 표시 영역의 가로·세로를 고정합니다. 필수는 생성된 빨간 별, 조명은 노란 전구, 프로그램은 보라색 배지를 사용합니다. 즐겨찾기는 카드 오른쪽 아래에 별도 상태로 표시합니다.
- 건축 기능 안내는 `Size_FeatureGuideCard`의 크기·슬롯과 `Border_FeatureGuideCard`의 배경을 편집합니다. Class Defaults → Designer Page → **건축 기능 안내**로 미리 볼 수 있습니다. 안내가 열리면 같은 위치의 시설 설치·정보 패널은 잠시 접히고, 닫으면 복원됩니다.
- 자주 쓰는 키는 건축 도구 WBP의 `Size_ApprovedQuickKeys` 아래 7개 행을 편집합니다. 각 행은 별도의 반투명 배경과 텍스트로 구성하며 `T_BuildQuickKeyFade`를 공유합니다. `Text_ApprovedKey_W/A/S/D`는 실행 중 사용자 키 설정으로 갱신되므로 이름을 바꾸지 마세요. 안내는 입력을 가로채지 않도록 설정합니다.
- 상단 아이콘은 `WBP_FF_BuildFeatureGuideToolbar`의 각 버튼에서 아이콘 브러시와 `FeatureTopicId`를 편집합니다. 팝오버 디자인은 건축 도구 WBP의 `Size_ToolbarFeaturePopup`을 수정합니다. `Slot_WholeGridAddon`은 지도 플러그인이 전체 격자 버튼을 삽입하므로 삭제하지 마세요.
- 목록 스크롤 손잡이는 `TileView_Facilities`의 Scroll Bar Style과 두께에서 수정합니다. 손잡이의 세 상태는 기본·마우스 오버·드래그 색상으로 구분하며 별도의 흰색 선은 없습니다.
- 매출 순위의 운영 상태 열은 순위 WBP의 헤더와 행 WBP의 `Text_OperationState`에서 편집합니다. 문구는 시설의 실제 운영 정보를 사용하므로 이름과 데이터 연결을 유지하고 고정 텍스트를 입력하지 마세요.
- 시설 도감의 목록과 상세는 `bUseAuthoredCollectionLayout`, `bUseParallelCollectionLayout`을 함께 켠 상태로 편집합니다. `Vertical_ApprovedSummary`와 `Button_ApprovedOpen3D`/`Button_ApprovedClose3D`는 실제 선택 데이터 및 별도 3D 화면에 연결되어 있으므로 이름을 유지하세요.

## 생성 아이콘 편집

- 2026-11-02부터 상단 기능 22개와 시설 분류 11개는 `/Game/Asset/Icon_HKH/T_HKH_*`의 개별 텍스처를 사용합니다. 같은 폴더의 PNG가 원본입니다. 상단 `Guide_*`의 `IconBrush`와 분류 `Choice_Category*`의 `AuthoredIcon`에서 교체하며, 상단의 아틀라스 사용과 분류의 `IconAtlas`를 비웁니다. 흰색 단색 분류 아이콘은 원본 색 유지 옵션을 꺼야 기존 선택 상태 틴트가 적용됩니다.
- 상단 버튼의 **Feature Guide → Motion**에서 반응 모션을 끄거나 확대·눌림·상승 강도를 조절합니다. 그림만 움직이고 클릭 영역은 고정됩니다. 마우스와 키보드 포커스 모두 반응합니다. 제작 프롬프트와 적용 도구는 `Tools/BuildModeIcons/README_KO.md`에 있습니다.
- 나머지 배지와 구형 화면의 아틀라스는 `/Game/FestaFiesta/UI/Generated/Building`의 `T_FF_BuildIconAtlas`, `T_FF_BuildCategoryAtlas`, `T_FF_BuildUtilityAtlas`, `T_FF_ProgramBadge`입니다. 전체 격자 이미지는 지도 플러그인의 `/FF_MapOverview/UI/T_FF_WholeGridGenerated`에 있습니다.
- `FF Build Atlas Image`를 선택하면 Details의 **Appearance → Atlas**에서 텍스처, 열과 행의 수, 0부터 시작하는 셀 인덱스를 바꿀 수 있습니다. 셀은 왼쪽에서 오른쪽, 위에서 아래 순서입니다. 이미지 크기와 색은 일반 UMG 이미지 설정에서 편집합니다.
- 상단 바의 각 `Guide_*` 인스턴스는 **Feature Guide → Icon Atlas**의 사용 여부, 열·행·셀과 `IconBrush`의 텍스처를 편집합니다. `FeatureTopicId`는 설명 연결에 사용하므로 유지하세요. 카테고리 인스턴스는 **Catalog Tab → Atlas**의 텍스처와 셀을 편집합니다. 원본 색 유지 옵션을 끄면 기존 선택 상태 색상 처리가 적용됩니다.
- 일반 Image에 연결된 아이콘은 Brush의 리소스와 UV 영역으로 특정 셀을 표시합니다. 원본의 분할 수나 셀 순서를 변경할 때는 해당 위젯의 설정도 함께 수정하세요. 아틀라스는 UI 텍스처 그룹이며 밉맵을 사용하지 않습니다.

## 문구와 스타일

- 고정 제목·항목명: 각 TextBlock의 Text, Font, Color를 직접 편집합니다. 예: `Text_A5PlacementTitle`, `Text_A5InstallHeading`, `Text_PlacementSynergyHeading`, `V2_InspectorTitle`, `Reference_BasicCapacityHeading`, `Clarity_Label_Text_InstallCost`.
- 배경·모서리·간격: Border의 Brush / Padding, 슬롯의 Padding, SizeBox에서 수정합니다. 모서리는 Fixed Radius를 사용합니다.
- 배치 비용: Class Defaults → Appearance / Text → **Installation Cost Format**. 예: `설치 비용: {Cost} Gold`. `{Cost}`는 실제 비용으로 대체됩니다.
- 배치 시설 크기: 같은 위치의 **Footprint Format**. 예: `크기: {Width} × {Depth} m`.
- 그 밖의 동적 문구: 두 정보 WBP의 **Dynamic Text Formats** 맵에서 Key에 위젯 이름, Value에 `{Value}`를 포함한 문구를 입력합니다. 예: Key `Text_Mode`, Value `현재 모드: {Value}`. 설치된 시설은 `Text_InstallCost`, `Text_RecoveryCost`, `Text_ServiceDuration`, `Text_Category` 등 데이터가 투영되는 TextBlock에 적용됩니다.
- 배치 정보에서 지원하는 맵 Key: `Text_FacilityName`, `Text_Mode`, `Text_Cost`, `Text_Balance`, `Text_Footprint`, `Text_Legend`, `Text_PlacementSynergySummary`, `Text_PlacementSynergyTitle1`~`3`, `Text_PlacementSynergyDetail1`~`3`.
- `Border_PlacementSynergy`는 이전 운영 공간 안내를 대체합니다. 실제 규칙·거리·중복/상한을 반영한 보너스만 표시하고, 추가 조합은 카탈로그의 조합 도감에서 확인합니다. 제목·상세 TextBlock의 글꼴과 여백은 Designer에서 편집합니다.
- 실제 가격·정원·시설 이름 등은 시설 데이터가 원본입니다. 표시 형식에 `{Value}`를 유지하면 값이 갱신됩니다. 위젯의 이름은 C++ 연결에 사용하므로 변경하지 마세요.

## 선택 동작

- 카탈로그 카드를 클릭하면 목록을 유지한 채 배치를 시작합니다.
- 설치된 시설을 클릭하면 미확정 배치 미리보기를 취소하고 같은 카탈로그 옆에서 시설 정보를 엽니다. 이 동작은 설치나 결제를 실행하지 않습니다.
- 기본·운영·프로그램·강화·조명·스태프 탭은 기존 데이터와 기능 연결을 유지합니다.
- 대기열 설정은 `Button_TabQueue` → `Panel_FacilityQueueSettings`에서 편집합니다. 대기열을 사용하는 시설에만 탭이 보이며, 운영 중에는 기존 규칙에 따라 읽기 전용입니다. 각 입력란·설명·적용 버튼은 모두 이 WBP 안의 UMG 위젯입니다.
- 건축 모드 탭은 `Grid_InspectorTabs`의 5열 UniformGrid입니다. Class Defaults의 `BuildInspectorTabColumns`와 `bShowUnavailableBuildTabs`에서 열 수와 미지원 탭 표시를 조절합니다. 지원하지 않는 탭은 기본 설정에서 비활성으로 남습니다. Live 모드는 기존 3열·미지원 탭 숨김을 유지합니다. 버튼의 크기는 Grid 최소 슬롯 크기·Slot Padding으로, 글자는 각 버튼 아래 CommonTextBlock에서 편집합니다.
- 기본 탭의 매출 순위 바로가기는 제거했습니다. 매출 정보와 순위 이동은 매출 탭에서 확인합니다.
