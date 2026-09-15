# 승인된 건축 UI — UMG / WBP 편집 안내

건축 탭 이후의 시설 목록, 배치 도구, 시설 정보, 시설별 유료 프로그램 구매 화면을 다룹니다.
기존 팀원의 준비 HUD, 배치·프로그램·인력·홍보·점검 탭과 상태바는 수정하지 않습니다.

## 어디를 열어 수정하나요?

콘텐츠 브라우저에서 다음 WBP를 열고 **Designer**에서 배치·간격·브러시·폰트를 변경합니다.

| 화면 | 콘텐츠 경로 | 주요 편집 위치 |
|---|---|---|
| 건축 시설 목록 | `/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelBuild` | `Box_Header`, `HorizontalBox_Categories`, `HorizontalBox_Subcategories`, `TileView_Facilities`, `Button_FavoritesOnly` |
| 시설 카드 | `/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile` | `SizeBox_Card`, `SizeBox_Thumbnail`, `Border_Card`, `Button_Favorite` |
| 건축 분류 버튼 | `/Game/FestaFiesta/UI/Building/WBP_FF_BuildCategoryChoice` | `Size_Choice`, `Button_Root`의 Normal / Hovered / Pressed, `Text_Label` |
| 배치·정밀 이동·회전 | `/Game/FestaFiesta/UI/Building/WBP_FF_BuildTools` | `Border_Tools`, 이동 간격·미세 이동·회전 버튼, 각도 입력, 시설 이름·가격·확정 버튼 |
| 시설 정보 | `/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel` | 정보·운영·강화 탭, 프로그램 영역, 즐겨찾기, 철거 확인, 고정 하단 행동 영역 |
| 유료 프로그램 구매 | `/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramPicker` | 시설 이름, 프로그램 목록, 구매 비용·인력·잔액, 확정 버튼, 완료 화면 |
| 프로그램 선택 카드 | `/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramCard` | 프로그램 이름·설명·비용·선택 스타일 |

위젯 이름은 C++의 `BindWidget` / `BindWidgetOptional` 연결 계약입니다. **이름과 위젯 타입을 유지한 채**
계층·위치·크기·스타일을 바꿀 수 있습니다. 다른 위젯으로 대체하려면 해당 이름의 네이티브 계약도 함께 변경합니다.
레이아웃 생성 코드는 초기 이전용이며, 이전 후에는 저장된 WBP가 디자인의 원본입니다.

구매 화면의 수치는 한 문장으로 합치지 않고 개별 위젯으로 나눴습니다. `Text_PreviewTime`(시간),
`Text_RequiredStaff`(인력), `Text_Capacity`(정원), `Text_TicketFee`(참가비),
`Text_MaxRevenue`(최대 매출), `Text_PurchaseCost`(구매 비용)를 각각 옮기거나 꾸밀 수 있습니다.
완료 화면은 `Panel_PurchaseComplete` 아래 `Text_ReceiptProgramName`, `Text_PurchaseReceipt`,
`Text_ReceiptResources`, `Button_ReturnToFacility`를 편집합니다.

## 크기와 상태 색상

- 목록 WBP의 **Class Defaults → FestaFiesta / UI / Prep / Build**에서 `CatalogHeight`, `CatalogOffsetY`,
  `CompactHeight`, `CompactOffsetY`를 바꿉니다. 팀원의 바에 공개된 레이아웃 API로 건축 패널만 조정합니다.
  초기 목록 높이는 392, 시설 확인 띠는 42입니다. Offset Y -41은 상태바 기준점 위로 탭 높이와 간격을 확보합니다.
- **Appearance / Pages → CardsPerPage**에서 한 페이지 카드 수를 바꿉니다. 승인 시안 기본값은 6입니다.
  카드 폭은 실제 목록 영역을 균등하게 나누며, 화면 크기가 바뀔 때만 갱신합니다. `bUsePagination`을 끄면 스크롤 목록을 사용할 수 있습니다.
- 기본 카드의 `SizeBox_Card`는 너비를 자동으로 늘리고 높이는 160입니다. 높이를 바꿀 때는
  `TileView_Facilities`의 Entry Height도 맞춥니다. 별도 고정 카드가 필요하면 SizeBox에 Width Override를 지정할 수 있습니다.
- 썸네일 영역은 `SizeBox_Thumbnail`의 Width / Height Override로 조정합니다. 사진은 그 영역 안에서 원래 비율을 유지합니다.
  이 위젯을 생략한 별도 카드에서만 Class Defaults의 `ThumbnailFallbackBounds`가 사용됩니다.
- `Size_SubcategoryViewport`와 `ScrollBox_Subcategories`는 **Clip to Bounds Always**로 유지합니다.
  즐겨찾기는 옆의 별도 `Size_FavoritesOnly` 슬롯에 고정되어, 긴 종류 목록이 버튼 영역에 그려지거나 클릭되지 않습니다.
- ‘전체’에서는 ‘전체 시설’ 하나만 표시하고 대분류를 선택했을 때 해당 종류를 표시합니다.
  하단의 `Button_PreviousPage`, `Button_NextPage`, `Text_PageIndex`와 중앙의 `Panel_Empty`도 Designer에서 편집합니다.
- `Overlay_CatalogCards` 안의 `TileView_Facilities` 슬롯은 가로·세로 정렬을 **Fill**로 유지합니다.
  페이지 모드에서는 스크롤바를 숨기며, 카드 높이와 위아래 영역을 합친 만큼 목록 전체 높이를 확보해야 합니다.
- 카드의 기본 브러시·글꼴·글자색은 Designer에서 수정합니다. Class Defaults의 **Appearance / States**에는
  hover·선택·잔액 부족·즐겨찾기 색상이 있습니다.
- 정밀 배치 버튼의 일반 모습은 Button Style, 선택 상태는 **Appearance / Selection**에서 수정합니다.
  준비 완료·배치 불가 문구의 색은 도구 WBP의 **Appearance / Status**에서 수정합니다.
- 시설 정보와 구매 패널은 우측 동일 영역을 번갈아 사용합니다. 크기·앵커는 각 WBP의 Canvas Slot에 저장됩니다.
  내부 내용은 스크롤되고 하단 행동 버튼은 유지됩니다.

## 실제 동작과 데이터

- 기존 배치 탭을 누르면 새 목록이 열립니다. 시설을 고르면 목록이 접히고 전용 배치 도구가 나타납니다.
- 카드의 별은 **즐겨찾기만 변경**합니다. 배치 시작과 분리된 버튼입니다. 즐겨찾기 필터는 분류·검색과 함께 적용됩니다.
- 시설 정보의 별과 카드의 별은 `UFF_BuildFavoritesSubsystem`을 공유합니다. 개인 설정은
  `Saved/Config/.../GameUserSettings.ini`의 `[FestaFiesta.BuildFavorites]`에 저장되며 축제 저장 데이터와 분리됩니다.
- 이동 간격은 1m / 25cm / 1cm입니다. Ctrl은 임시 25cm, Alt는 임시 1cm이며 미세 이동은 구역 X/Y 축 기준입니다.
  도구 위에서는 배치 위치가 유지됩니다. 기존 건축 시스템이 실제 위치와 설치 가능 여부를 검증합니다.
- Q/E는 다음 맞춤 각도로 회전합니다. Shift를 먼저 누르고 Q/E를 누르는 동안 자유 회전합니다.
  화면의 자유 회전 버튼도 누르는 동안 회전하며, 버튼 밖으로 나가거나 포커스를 잃으면 멈춥니다.
- 시설 정보의 이동·강화·철거·조명·미니게임은 기존 기능을 사용합니다. 철거에는 별도 확인 단계가 있습니다.
- 프로그램 카드 선택은 구매하지 않습니다. 별도 **Gold로 구매·편성** 버튼에서 기존 스케줄링 시스템이 다시 검증하고
  비용과 인력을 반영합니다. 구매 완료는 실제 성공 결과가 온 뒤에 표시합니다. 정원 기준 최대 매출은 확정 수익과 구분합니다.
- 실제 프로그램·비용·호환 시설·시간은 축제 정의와 경제 데이터에서 읽습니다. 시안에 있던 200 Gold 등의 값을 게임 로직에 고정하지 않습니다.
- 기존 팀원의 독립된 프로그램 탭은 유지합니다. 새 구매 화면은 시설 정보에서 진입하는 시설별 구매 화면입니다.

## 초기 이전과 검증

일반 실행 중 WBP를 생성하거나 재작성하지 않습니다. 이미 적용된 프로젝트를 편집할 때 아래 명령을 다시 실행할 필요가 없습니다.
명령을 재실행해도 승인 버전이 있으면 디자이너의 편집을 유지합니다.

1. UE 5.7.4에서 `FestaFiestaEditor Win64 Development`를 빌드합니다.
2. 에디터 콘솔에서 필요한 범위만 실행합니다.
   - `FF.RefreshBuildStudioUMG`
   - `FF.RefreshBuildToolsUMG`
   - `FF.MigrateFacilityInspectorUMG`
3. 또는 별도 unattended Editor에서 `Tools/ui/apply_approved_build_ui.py`를 `-ExecutePythonScript`로 실행합니다.
   이 스크립트는 대상 6개 WBP를 `Artifacts/Validation/BuildUIApproved/Backups`에 복사한 뒤 위 세 명령을 실행하고 종료합니다.
4. `FestaFiesta.UI.PrepBuild`, `FestaFiesta.UI.FacilityInfo`, `FestaFiesta.Building.BuildTools`,
   `FestaFiesta.Building.BuildMode`, `FestaFiesta.Economy.ProgramScheduling` 자동 테스트를 실행합니다.
   실제 렌더 검증은 `-RenderOffscreen -d3d11`을 사용합니다. `-nullrhi`는 화면 검증을 생략합니다.
   브라우저가 필요 없는 이 테스트에서는 `-nocef`로 에디터 내장 브라우저의 offscreen RHI 충돌을 방지합니다.

제한된 실행 환경에서 공용 DDC를 쓸 수 없다면 해당 프로세스의 `UE-LocalDataCachePath`를 프로젝트의
`DerivedDataCache`로 지정하고 `-ddc=InstalledNoZenLocalFallback -NoZenAutoLaunch`를 사용합니다.
프로젝트의 전역 엔진 설정이나 팀원의 캐시 설정을 바꿀 필요가 없습니다.

팀원의 화면까지 다시 만드는 전체 HUD 생성 명령은 이 작업에 필요하지 않습니다.

## 적용 검증 기록 · 2026-09-11

- UE 5.7.4 `FestaFiestaEditor Win64 Development` 빌드 성공.
- 위 범위의 자동 테스트 47개 통과, 실패·미실행 0개. 최신 결과는
  `Artifacts/Validation/BuildUIApproved/CatalogV9Report/index.json`에 있습니다.
- 검증 내용: 배치 입력 연결, 즐겨찾기 필터와 별 클릭 분리, 카드 재활용·Designer 설정 보존,
  정밀 도구 계약, 시설 선택·철거 확인, 권위 프로그램 편성·중복 구매 방지.
- 실제 시설 45종·부산 축제 가격표를 사용해 전체 8페이지, 전체/안전/응급처치 분류,
  즐겨찾기 필터의 독립 클릭 영역과 빈 결과 복구를 확인했습니다.
- 1280×720 / 1920×1080 / 1742×738에서 저장된 팀원 HUD와 프로젝트 DPI 배율을 적용해
  카탈로그를 렌더링했습니다. 6개 카드의 완전 노출·너비·비중첩·페이지 접근성을 확인합니다.
  이미지는 `Saved/Automation/BuildableGrid/BuildCatalog_*`에 있습니다.
- 구매·완료 화면 렌더도 통과했습니다. 해당 이미지는 `Saved/Automation/PaidProgramPurchase_*`에 있습니다.
  이 검증은 분리된 읽기 전용 데이터로 실행하는 UMG 렌더 테스트이며, 전체 맵 PIE 플레이 검증과 구분합니다.
- 변경한 콘텐츠 에셋은 안내 표의 건축 관련 WBP 6개입니다. 기존 프로그램 선택 카드와 팀원의 HUD 에셋은 보존했습니다.
- 겹침 수정에서는 그중 시설 목록·카드·분류 버튼 3개만 다시 저장했습니다.
  시설 정보·구매·정밀 배치 도구 3개는 이전/이후 파일 해시가 같습니다.
- 기존 테스트 환경의 PlayerInput 경고, Entry 맵 저장 권한 경고, 공통 HUD 아이콘의 누락 참조 경고가 남습니다.
  해당 팀원 UI 및 프로젝트 공통 데이터는 이번 승인 범위에서 변경하지 않았습니다.
