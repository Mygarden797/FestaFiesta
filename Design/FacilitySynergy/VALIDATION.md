# 시설 조합 검증 기록

검증일: 2026-09-16. 환경: Unreal Engine 5.7.4, Windows, Win64 Development Editor, D3D12 / RTX 3060 Ti.

## 빌드와 검증 범위

- 시설 조합 계산, 수익 정산, 강화 예상액, 배치 미리보기, 시설 정보창의 조합 탭을 포함한 Editor 빌드 통과.
- 최신 빌드 로그: `Saved/FacilitySynergyValidation/NavigationGridBuild.log`.
- 필요한 기능 검증에 집중했으며 전체 프로젝트 테스트는 반복 실행하지 않았습니다.

| 검증 | 결과 | 확인한 내용 |
|---|---|---|
| DistanceBoundaryAndInvalidData | 통과 | 거리 경계 포함, 무효 설정 제외 |
| MoveAndDuplicatePreviewConsistency | 통과 | 이동 전후 결과, 중복 상대 시설, 같은 종류끼리 조합 |
| StrongestGroupDuplicatesAndCap | 통과 | 그룹별 최댓값, 중복 방지, 전체 상한 |
| PreviewPaymentAndLifecycle | 통과 | 배치 예상치와 실제 수수료, 이동·철거·동일 리비전 스냅샷 복원 |
| UpgradeRevenueCompletesAndRestores | 통과 | 기존 강화 정산 및 복원 회귀 검증 |
| UpgradeRevenueRespectsFundsCapacity | 통과 | 기존 수익 정산의 금액 상한 회귀 검증 |
| PresentationLifecycleAndLayout | 통과 | 실제 배치 미리보기 WBP, 720p·1080p 겹침과 가독성, 취소·숨김 |
| SynergyTab | 통과 | 실제 시설명·조합표 이름, 탭 순서·전환, 카드 재사용, 빈 상태, 첫 줄 잘림 방지 |

앞의 6개 결과는 `Saved/FacilitySynergyValidation/Report/index.json`에 있습니다. 이 최초 보고서의 UI 겹침 실패는 여백 수정 후 `UIReport/index.json`에서 통과했습니다. 조합 탭의 첫 줄 잘림은 새 위젯 두 개의 세로 정렬을 Top으로 수정했으며, 최종 `UIClipReport/index.json`에서 첫 프레임과 다음 프레임 모두 통과했습니다. 서로 다른 검증 8개가 모두 통과한 결과입니다. 경제 관련 세 테스트의 경고는 기존 군중 테스트 환경에서 기록하는 `FacilityWallCount` 메시지입니다.

720p 배치 미리보기 카드 하단은 428px, 기존 설치 도구 상단은 440px로 겹치지 않습니다. 시설 정보의 조합 요약은 188~228px에 배치되어 스크롤 표시 영역 186~529px 안에서 온전히 표시됩니다. 최종 조합 탭 테스트에는 오류와 경고가 없습니다.

## 실제 에셋과 디자이너 편집 보존

- `DT_FacilitySynergies`: 실제 DataTable 에셋, 행 구조 및 CSV 값·시설 ID 목록 연결 확인.
- `WBP_FF_BuildSynergy`: 실제 UMG 에셋, 9개 연결 위젯 및 Designer 표시 확인.
- `WBP_FF_FacilityInfoPanel`: 기존 154개 위젯의 글자·폰트·부모·슬롯·위치를 유지하면서 조합 탭 추가.
- `WBP_FF_FacilitySynergyEntry`: 조합 카드 한 개를 편집하는 실제 UMG 에셋, 5개 텍스트 연결 확인.
- 생성·마이그레이션 명령 재실행 전후 파일 해시를 비교해 기존 에셋을 다시 덮어쓰지 않는 것을 확인.
- 프로젝트의 기존 패키징 설정에 Data와 UI 폴더가 포함되어 있는 것을 확인.

에셋 검증 보고서: `Saved/FacilitySynergy/asset_verification.json`, `inspector_verification.json`.

조합 이름 `Burger + Seating`, `IceCream + Shade` 변경 시 다른 행 값과 가져오기 설정을 유지한 것은 `display_name_patch.json`, 새 위젯 두 개의 정렬만 수정한 것은 `summary_alignment_patch.json`에서 확인할 수 있습니다.

화면 이미지: `Saved/Automation/FacilitySynergy/FacilitySynergyTab720.png`, `FacilitySynergyTabEmpty720.png`, `Synergy_1280x720.png`, `Synergy_1920x1080.png`.

## 건축 목록의 조합 시설 강조 추가 검증

배치한 시설을 확인한 뒤 B로 목록에 돌아오면 상대 시설을 강조하는 기능을 추가했습니다. 기본 PrepHUD 목록은 조합 항목을 앞쪽에 배치하고, 새 기준 시설에서 처음 열 때 전체 분류·첫 페이지를 보여 줍니다. 이후 사용자가 고른 필터는 유지됩니다.

다음 관련 검사 6개만 실행하여 모두 통과했습니다. 보고서는 `Saved/FacilitySynergyValidation/CatalogHighlightReport/index.json`, 실행 로그는 `CatalogHighlightTests.log`입니다.

| 검증 | 확인한 내용 |
|---|---|
| DistanceBoundaryAndInvalidData | 활성·유효한 규칙의 양방향 관계 조회, 모르는 종류 제외 |
| StrongestGroupDuplicatesAndCap | 중복 조합 행이 추천 시설을 중복 생성하지 않음 |
| SynergyCatalogContext | 정보창에서 목록으로 돌아갈 때 추천 유지, 취소·시설 교체·철거·복원·종료 시 정리, 이전 전체 화면 목록의 분류 선택 |
| SynergyHighlightPreservesFiltersAndPool | 추천 우선 정렬, 필터와 카드 객체 유지, 잔액·건축 한도 독립, 해제 후 순서 복원 |
| DesignerAppearanceSurvivesRuntime | WBP 색·글꼴·문구·크기 보존, 재활용 카드 강조 해제 |
| RenderedPreview (`-FFSynergyCatalogPreview`) | 실제 WBP·시설 이름·썸네일로 720p 렌더링, Seating 첫 카드 강조, 배지 범위와 즐겨찾기 입력 |

오류는 0개입니다. 렌더 검사에는 기존 HUD 내비게이션 아이콘 5개가 없는 데서 나온 경고 10개가 기록되었습니다. 해당 아이콘 및 HUD 파일은 이번에 변경하지 않았습니다.

실제 에셋 `WBP_FF_PrepBuildTile`의 기존 위젯 18개와 `WBP_FF_BuildFacilityCard`의 기존 위젯 12개를 보존하면서 장식 레이어만 추가했습니다. 명령 재실행 시 에셋 파일 해시가 같은 것을 확인했습니다. 보고서: `Saved/FacilitySynergy/catalog_highlight_verification.json`.

강조 화면: `Saved/Automation/BuildableGrid/BuildCatalog_1280x720_Synergy.png`. 실제 카드의 테두리와 배지를 직접 확인했으며, 이 이미지는 맵 배경 없이 독립적으로 렌더링한 UI 검증 화면입니다. 전체 게임의 클릭부터 배치까지 수동으로 조작한 검증은 아닙니다.

## 건축 목록의 필수 표시와 설치 현황 추가 검증

설치 한도가 있는 시설은 기존 상태 칸에 현재 설치 수와 전체 한도를 표시합니다. 실제 기본 필수 시설인 `SunsetMainStage`는 `0/1 → 1/1`로 바뀌고, 최대 3개인 `LocalArtist`는 `0/3`으로 표시됩니다. 필수 여부는 별도 배지로 보여 주며 최소 개수를 채우면 `필수 설치 → 필수 충족`으로 바뀝니다.

최종 Editor 빌드와 아래 관련 검사 5개가 모두 통과했습니다. 보고서: `Saved/FacilitySynergyValidation/RequiredCatalogReport/index.json`. 실행 로그: `RequiredCatalogTests.log`.

| 검증 | 확인한 내용 |
|---|---|
| StageGateAndNotifications | 기존 필수 시설 판정의 설치·철거·복원 알림과 단계 진행 조건 |
| TileDataSnapshot | `0/1 → 1/1 → 0/1`, 한도 및 잔액 부족 시 숫자 유지, 무제한 시설의 기존 문구 |
| ModelReusesTileObjects | 필수 상태 투영·해제, 무효 중복 설정, 같은 카드 객체 재사용, 잔액·조합 강조 독립 |
| DesignerAppearanceSurvivesRuntime | 필수·충족 배지 전환, 조합 배지 공존, WBP 문구·색·글꼴 보존, 잘못된 최소 개수 안내, 카드 재활용 초기화 |
| RenderedPreview (`-FFRequiredCatalogPreview`) | 실제 축제 설정과 WBP로 720p 설치 전후 렌더링, `0/1 → 1/1`, 배지 범위와 즐겨찾기 입력 |

오류는 0개입니다. 단계 진행 검사에는 의도한 진행 차단 경고 1개, 렌더 검사에는 기존 HUD 내비게이션 아이콘 5개 누락 경고 10개가 있습니다.

`FF.AddBuildRequiredFacilityBadges`로 두 실제 카드 WBP에 필수 배지를 추가했습니다. Prep 카드의 기존 위젯 23개와 이전 전체 화면 카드의 기존 위젯 17개의 부모·글자·폰트·색·슬롯을 보존했고, 명령을 다시 실행해도 에셋 파일 해시가 같았습니다. 보고서: `Saved/FacilitySynergy/required_badge_verification.json`.

설치 전후 이미지 `Saved/Automation/BuildableGrid/BuildCatalog_1280x720_RequiredMissing.png`, `BuildCatalog_1280x720_RequiredSatisfied.png`를 직접 확인했습니다. 실제 시설 정의·축제 설정·저장된 WBP를 사용했으며, 설치 전후의 월드 입력은 격리된 검증용 스냅샷입니다. 전체 맵에서 수동으로 설치·철거한 플레이 검증은 아닙니다.

## 조합 상대 바로가기와 그리드 유지 검증

조합 카드의 `Text_Partner`를 클릭 가능한 `Button_OpenPartnerCatalog`로 감쌌습니다. 실제 시설 ID로 건축 목록을 열고, 해당 대분류·소분류·페이지를 선택해 카드를 보여 줍니다. 자동 설치는 하지 않습니다. 검색·즐겨찾기 제한은 이번 이동에서 해제하며, 요청을 처리한 다음에는 사용자의 분류 선택을 유지합니다. 로딩 중 요청은 대기하고 목록에서 나가면 취소합니다.

격자의 기존 공간 변경 확인 주기는 유지했습니다. 확정된 지면이 시간 만료만으로 사라지지 않게 하고, 선의 위치·색·두께가 같으면 렌더 데이터를 교체하지 않습니다. 변경된 선 배열은 빈 배열로 먼저 지우지 않고 한 번에 반영합니다. 시설 레지스트리 교체와 지면 컴포넌트의 삭제·이동·충돌 변경도 다시 확인합니다.

Editor 빌드와 관련 검사 6개가 모두 통과했습니다. 보고서는 `Saved/FacilitySynergyValidation/NavigationGridReport/index.json`, 실행 로그는 `NavigationGridTests.log`입니다.

| 검증 | 확인한 내용 |
|---|---|
| SynergyTab | 양방향 상대 시설 ID, 실제 Slate 버튼 입력 전달, 렌더된 이름의 마우스 클릭 경로, 없는 목적지 비활성, 기존 스타일 보존 |
| SynergyCatalogContext | 이전 전체 화면 목록의 목표 시설 이동, 설치 한도와 탐색 독립, 자동 설치 없음, 이후 사용자 분류 유지 |
| RenderedPreview (`-FFSynergyNavigationPreview`) | 실제 Prep WBP에서 검색·즐겨찾기 해제, 로딩 중 요청 대기·취소, 다른 페이지로 이동, 목표 카드 선택 및 720p 렌더링 |
| ComponentLifecycle | 같은 결과의 렌더 갱신 생략, 주기 확인 후 표시 유지, 실제 시설·레지스트리 변경 반영 |
| ConfirmedFloorPersistsUntilChanged | 오래된 확정 지면 유지, 지면 이동·충돌 해제·삭제 시 재확인 |
| GroundRulesAndIgnoredRoofs | 지면 판정과 제외할 지붕 처리의 기존 동작 유지 |

오류는 0개입니다. 렌더 검사에는 기존 HUD 내비게이션 아이콘 누락 경고 10개, 격자 검사에는 격리된 월드에 경제 Queue가 없다는 기존 경고가 각 1개 기록되었습니다.

실제 조합 카드 WBP의 기존 위젯 8개의 스타일을 보존했습니다. 추가 버튼은 원래 부모 슬롯과 간격을 이어받고, 입력을 막던 루트의 Visibility만 자식 클릭을 허용하도록 변경했습니다. 반복 마이그레이션의 에셋 해시 동일 확인: `Saved/FacilitySynergy/partner_link_verification.json`.

`Saved/Automation/FacilitySynergy/FacilitySynergyTab720.png`와 `Saved/Automation/BuildableGrid/BuildCatalog_1280x720_PartnerNavigation.png`를 직접 확인했습니다. UI는 실제 WBP와 시설 데이터를 사용한 격리 렌더이며, 전체 게임 맵에서 클릭부터 설치까지 수동 조작하거나 장시간 격자 영상을 촬영한 검증은 아닙니다.

## 검증의 한계

UI 이미지는 실제 저장된 WBP를 독립적인 Slate 화면에서 렌더링한 검증 이미지입니다. 시설 정보창은 실제 시설 정의의 표시 이름을 사용하며, 월드 배치와 수익 상태는 테스트 입력입니다. 게임 맵 전체를 직접 조작한 플레이 영상은 아닙니다. 월드 연결선은 표식 데이터·상태 정리와 빌드를 확인했으며, 실제 플레이 카메라에서의 수동 확인과 Shipping 패키징, 대규모 시설 수에 대한 성능 측정은 이번 범위에서 실행하지 않았습니다.

사용·설정 방법은 [README_KO.md](README_KO.md)와 [노션 가이드](https://app.notion.com/p/3dd96c43a7f780058651e5e04a81927f)를 참고하세요.
