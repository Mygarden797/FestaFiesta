# 건축모드 UMG 편집 안내

언리얼 콘텐츠 브라우저 기준 경로입니다. 아래 WBP를 Designer에서 직접 수정하고 Compile / Save 하세요. Python 마이그레이션 스크립트를 다시 실행하면 스타일이 덮어써질 수 있으므로 일상적인 디자인 수정에는 실행하지 않습니다.

| 화면 | 편집할 WBP |
|---|---|
| 시설 목록 | `/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild` |
| 시설 카드 | `/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile` |
| 배치 중 시설 설치·건축 기능 안내 | `/Game/FestaFiesta/Building/FF_WBP_Building/Controls/WBP_FF_BuildTools` |
| 설치된 시설의 정보·탭 | `/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel` |
| 조명 편집 | `/Game/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityLightingControls` |

## 크기와 배치

- 목록: Class Defaults → Appearance / Catalog → **화면 폭에 맞추기**가 켜져 있으면 오른쪽 정보 영역(550)과 여백(60)을 제외한 화면 폭을 채웁니다. 조합 도감 버튼의 실제 폭 160을 목록에서 정보 패널로 재배분했습니다. `Size_ClarityCatalogDock`의 폭은 이 옵션을 끄면 사용됩니다. 높이는 Designer에서 편집합니다.
- 여백 60은 현재 왼쪽 HUD 여백 12 + 오른쪽 24 + 패널 사이 24입니다. 다른 담당자의 하단 HUD는 변경하지 않습니다.
- 배치 정보: `Size_PlacementInfo`의 크기·캔버스 슬롯, `Border_PlacementInfo`의 배경·패딩을 편집합니다.
- 설치된 시설 정보: Class Defaults → Appearance / Build Layout에서 크기와 여백을 편집합니다. 오른쪽 패널 폭을 변경하면 목록에 예약하는 폭도 함께 맞춰야 합니다.
- 목록의 `VerticalBox_CatalogBody`는 Fill을 유지하세요. `Size_A5CatalogRows` 높이 224는 카드 112 높이의 두 줄입니다. 이 제한을 없애면 스크롤 영역이 내용만큼 늘어나 스크롤과 하단 안내가 가려집니다.
- 카드와 하단 범례의 `Size_A5RequiredBadge`, `Size_A5LightingBadge`, `Size_A5ProgramBadge`는 표시 영역의 가로·세로를 고정합니다. 필수는 빨간 ★(투명 Border), 조명·프로그램은 원형입니다. 즐겨찾기는 카드 오른쪽 아래의 금색 별입니다.
- 건축 기능 안내는 `Size_FeatureGuideCard`의 크기·슬롯과 `Border_FeatureGuideCard`의 배경을 편집합니다. Class Defaults → Designer Page → **건축 기능 안내**로 미리 볼 수 있습니다. 안내가 열리면 같은 위치의 시설 설치·정보 패널은 잠시 접히고, 닫으면 복원됩니다.

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
- 탭은 `Grid_InspectorTabs`의 3열 UniformGrid입니다. 지원하지 않는 탭은 런타임에 접고 빈칸 없이 정렬합니다. 버튼의 동일한 크기는 Grid의 최소 슬롯 크기·Slot Padding으로, 글자는 각 버튼 아래 CommonTextBlock에서 편집합니다(현재 전부 14).
- 기본 탭의 매출 순위 바로가기는 제거했습니다. 매출 정보와 순위 이동은 매출 탭에서 확인합니다.
