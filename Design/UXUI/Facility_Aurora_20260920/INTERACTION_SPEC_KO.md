# 시설 UI 개선안 — 상호작용·구현 연결 명세

작성일: 2026-09-20  
상태: **시안 검토용 / 구현 승인 전**  
대상: FestaFiesta, Unreal Engine 5.7, C++/UMG

이 문서는 첨부된 17개 게임 화면과 현재 저장소의 구현을 근거로, 시안의 각 조작을 실제 기능에 연결하기 위한 계약을 정의한다. 현재 단계에서는 디자인 문서만 작성하며 C++·WBP·uasset·게임 데이터는 변경하지 않는다. 단계 구분의 근거는 사용자의 **“먼저 시안 이미지를 만들고 내가 통과하면 이미지대로 버튼 및 기능을 전부 구현”** 요청이다. 다른 문서나 스킬에 있는 승인 절차를 이 요청에 추가하지 않는다.

이미지에 표시하는 수치·발견 개수·시설 수는 화면 구성을 설명하는 예시다. 구현할 때는 게임의 최신 스냅샷을 사용하며 이미지의 숫자를 상수로 넣지 않는다. 이미지 생성 과정에서 글자나 아이콘이 달라지면 이 명세의 기능 의미와 실제 게임 데이터가 우선한다. 시안 승인 이후에는 이미지와 명세 간 차이를 구현 전에 정리한다.

## 1. 공통 표현과 연결 원칙

### 1.1 톤앤매너

현재 `Source/FestaFiesta/Public/UI/Data/FF_UIPalette.h`의 `InGame` 팔레트를 기준으로 한다.

| 역할 | 기준 색상 |
|---|---|
| 기본 녹색 / 주요 행동 | `#527C6B` |
| 진한 녹색 / 제목·활성 탭 | `#315747` |
| 연한 녹색 / 선택 보조면 | `#B4C8BB` |
| 기본 종이면 / 보조면 | `#F7F8F6` / `#EEF2EF` |
| 본문 / 보조 글씨 | `#24332E` / `#6F7B75` |
| 경계선 | `#CCD5CF` |
| 경고 / 오류·철거 | `#E39A22` / `#D74E43` |

이리디센스는 선택 테두리와 소량의 광택, 오로라는 헤더의 부드러운 빛, 메쉬 그라디언트는 큰 장식 면의 낮은 채도 색 변화로 제한한다. 가격·표·본문 뒤에는 안정적인 종이면을 둔다. 장식의 밝기 변화로 선택·비활성·경고 상태를 대신하지 않는다. 이 효과와 새 배치는 **신규 디자인 제안**이며 현재 엔진에서 구현·성능 검증이 끝났다는 의미가 아니다.

### 1.2 공통 보존 계약

- WBP가 레이아웃과 스타일을 소유하고, C++가 불변 UI 스냅샷을 적용하고 액션을 전달하는 구조를 유지한다.
- 기존 `BindWidget`의 이름·타입, 선택 시설의 `FacilityInstanceId`, 정의의 `FacilityDefinitionId`를 보존한다. 표시 이름에서 식별자를 추측하지 않는다.
- 화면 변경·탭 변경·검색·필터·목록 선택은 경제 상태를 변경하지 않는다. 배치 확정·구매·강화·철거 등 명시적 행동만 권위 명령을 실행한다.
- 화면을 다시 열거나 갱신할 때 클릭 델리게이트를 중복 연결하지 않는다. 데이터 조회를 Blueprint의 매 프레임 바인딩으로 옮기지 않는다.
- Loading, Empty, Failed, Ready를 구분한다. “데이터 없음”을 임의의 `0 Gold`로 표현하지 않는다.
- 원인 설명이 필요한 비활성 버튼에는 해당 이유를 가까이 표시한다. 경고는 색·아이콘·문구를 함께 사용한다.
- 시설 선택을 바꾸면 조명·강화·프로그램·철거의 이전 시설 상태가 새 시설로 흘러가지 않게 한다.
- 장식 레이어는 히트 테스트를 받지 않는다. 패널 바깥 투명 영역은 의도한 경우 월드 조작을 계속 허용한다.

## 2. 10개 기능별 계약

### 2.1 시설 건축 카탈로그

**기존 연결:** `UFF_PrepPanelBuild`, `UFF_PrepBuildPresentationModel`, `UFF_PrepBuildTileWidget`, `UFF_BuildCatalogSubsystem`, `UFF_BuildFavoritesSubsystem`.

| 노출 요소 | 기존 행동 / 전환 | 상태·검증 |
|---|---|---|
| 전체·시설 대분류·소분류 | 표시 목록 필터 변경 | 전체를 임의의 시설 enum 값으로 대체하지 않음. 대분류에 맞는 소분류만 표시 |
| 검색 | 이름·정의 ID·소분류 토큰 검색 | 검색 중 건축 단축키와 월드 조작을 차단. 결과 없음 안내와 초기화 제공 |
| 즐겨찾기 별 | 해당 시설 정의의 즐겨찾기 변경 | 시설 카드 클릭 이벤트와 분리하여 배치를 시작하지 않음 |
| 즐겨찾기만 | 개인 즐겨찾기 필터 토글 | 개수와 활성 상태 즉시 갱신 |
| 시설 카드 | `StartPlacement(FacilityDefinitionId)` 요청 | 현재 단계·가격·보유금·건축 한도 확인. 성공 시 Placing |
| 이전·다음 페이지 | `PreviousCatalogPage` / `NextCatalogPage` | 첫/끝 페이지 비활성. 빈 목록은 0페이지이며 인덱스는 0 |
| 필터 초기화 | 카탈로그 필터 초기화 | 보유 시설이나 즐겨찾기 자체는 변경하지 않음 |
| 시설 확인 / B | CatalogOpen → BuildMode | 카탈로그 선택 이력과 관련 시설 문맥 보존 |
| 매출 순위 / 도감 | 각 전용 화면 열기 | 복귀 후 카탈로그 상태를 과도하게 초기화하지 않음 |
| 닫기 | 기존 패널 소유자의 닫기 처리 | 카탈로그 닫기와 전체 건축 종료의 의미를 버튼 문구로 구분 |

**중요 상태:** 불러오는 중, 정상 목록, 빈 검색 결과, 목록 실패, 배치 가능, 잔액 부족, 가격 미확정, 잔액 미확정, 건축 한도 도달. 메인공연장 등 필수 시설 안내는 조건 충족 시 안내와 여백을 함께 접는다. 시설 수·페이지 수를 이미지의 `114`, `19` 등으로 고정하지 않는다.

**구현 파일:**

- `Source/FestaFiesta/Private/UI/PrepHUD/Panels/FF_PrepPanelBuild.cpp`
- `Source/FestaFiesta/Public/UI/PrepHUD/Build/FF_PrepBuildPresentationTypes.h`
- `Source/FestaFiesta/Private/UI/PrepHUD/Build/FF_PrepBuildTileWidget.cpp`
- `Source/FestaFiesta/Private/Building/Presentation/FF_BuildStudioWbpBuilder.cpp`
- `Content/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild.uasset`
- `Content/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepBuildTile.uasset`

첨부 카탈로그의 주 구현은 PrepHUD 경로다. 구형 `FF_BuildCatalogScreen`만 변경하고 완료 처리하지 않는다.

### 2.2 시설별 매출 순위

**기존 연결:** `UFF_FacilityRevenueRankingSubsystem`, `UFF_FacilityRevenueRankingPanel`, `UFF_FacilityRevenueRankRow`.

| 노출 요소 | 기존 행동 / 전환 | 상태·검증 |
|---|---|---|
| 오늘 / 축제 누적 | 기간 변경 후 순위 재계산 | 화면 복사본만 정렬하며 경제 장부를 변경하지 않음 |
| 시설 행 | 단일 설치 시설 선택 | 동일 종류 시설도 서로 다른 인스턴스 행으로 유지 |
| 선택 시설 정보 보기 | 선택한 인스턴스의 정보 열기 | 미선택·삭제된 시설·조회 실패 시 비활성 또는 사유 제공 |
| 닫기 | 매출 패널 닫기 | 기존 입력 소유권 복원 |

매출 총액, 설치 시설 수, 순위, 금액, 상대 매출 막대, 매출 비중을 일관되게 표시한다. 같은 매출은 공동 순위다. 전체 매출이 0일 때 비중의 미정 상태를 실제 계산 가능 비중과 구분한다. 설치된 수익 시설이 없는 상태, 시설은 있지만 결제가 없는 상태, 경제 데이터가 아직 준비되지 않은 상태를 별도로 표현한다.

**구현 파일:**

- `Source/FestaFiesta/Public/UI/FacilityRevenue/FF_FacilityRevenueRanking.h`
- `Source/FestaFiesta/Private/UI/FacilityRevenue/FF_FacilityRevenueRanking.cpp`
- `Source/FestaFiesta/Private/UI/FacilityRevenue/FF_FacilityRevenueWbpBuilder.cpp`
- `Content/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRanking.uasset`
- `Content/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityRevenueRankRow.uasset`

### 2.3 시설 배치 시 그리드

**기존 연결:** `UFF_BuildModeSubsystem`, `AFFZSBuildableGridActor`, `AFFZSPlacementPreviewActor`.

| 노출 요소 | 기존 행동 / 전환 | 상태·검증 |
|---|---|---|
| 격자 표시 / G | `SetPlacementGridVisible` | 표시 토글이 배치 허용 판정을 바꾸지 않음 |
| 1m / 25cm / 1cm | `SetPreferredPositionSnapCm` | 선호 단위와 Ctrl/Alt로 잠시 적용된 실제 단위 구분 |
| 배치 영역·안전 여유 | 현재 시설의 권위 검증 결과 표시 | 배치 가능·충돌·영역 밖 등 실제 사유와 표현 일치 |

건축 세션에서 주변 건축 가능 영역을 살펴볼 수 있고 일반 플레이에서는 격자를 숨기는 현재 정책을 유지한다. 1cm 배치에서도 측정 기준 1m 격자는 남으며, 25cm 모드에는 보조선을 적용하는 기존 의미를 유지한다.

**신규 표현 제안:** 전체 격자의 시각적 강도를 낮추고 선택 시설 근처, 시설 경계, 안전 여유를 우선 읽히게 한다. 불가 영역은 선·면·문구를 함께 사용한다. 표현 개선이 좌표 스냅·충돌 판정·영역 규칙을 바꾸면 안 된다.

**구현 파일:**

- `Source/FestaFiesta/Public/Building/FF_BuildModeSubsystem.h`
- `Source/FestaFiesta/Private/Building/FF_BuildModeSubsystem.cpp`
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Public/FF_FFZSBuildableGridActor.h`
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSBuildableGridActor.cpp`
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Public/FF_FFZSPlacementPreviewActor.h`
- `Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSPlacementPreviewActor.cpp`

### 2.4 시설 배치 시 정보와 조작

**기존 연결:** `FFF_BuildToolsViewData`, `UFF_BuildToolsWidget`, `UFF_BuildModeSubsystem`.

| 노출 요소 | 기존 행동 / 전환 | 상태·검증 |
|---|---|---|
| 배치 확정 / 설치 | `CommitPlacement()` | 최신 커서·revision·대상·비용 재검증. 설치 성공 후 연속 배치 유지 |
| 이동 확정 | 같은 `CommitPlacement()`의 Move 경로 | 성공 시 BuildMode. 실패 시 프리뷰 유지 |
| 시설 목록 / B | `ReturnToCatalog` 또는 기존 B 상태 전환 | 진행 중 프리뷰 정리 후 CatalogOpen |
| Q / E 회전 | `RotatePlacementDegrees` | 현재 회전 스냅 설정 사용 |
| Shift+Q / Shift+E | 버튼 누름·해제와 자유 회전 | 마우스 캡처 해제·패널 종료 때 회전 타이머 종료 |
| 각도 입력 | `SetPlacementYawDegrees` | 유한한 0~360 소수만 허용. 360은 0으로 정규화. 잘못된 값은 이전 각도 유지 |
| -X / +X / -Y / +Y | `NudgePlacement` | 현재 이동 단위를 반영. 최신 배치 유효성 갱신 |
| 배치 취소 | 기존 취소 경로 | 신규 배치→카탈로그, 이동→시설 확인의 차이 유지 |
| 건축 종료 | `ExitBuildMode` | 모든 프리뷰·선택·임시 입력 상태 정리 |

시설 이름, 설치 비용 또는 이동 상태, 현재 스냅·각도, 설치 가능 여부와 불가 사유를 행동 버튼 가까이 둔다. 카탈로그를 접은 상태에서는 지면이 충분히 보여야 한다.

**신규 조작 제안:** `정밀 조정 펼치기/접기`는 순수 표현 상태를 바꾸는 버튼으로 추가한다. 접힌 상태에는 스냅·좌우 회전·확정 등 핵심 조작, 펼친 상태에는 숫자 각도·축 이동·자유 회전을 둔다. 펼침 상태 변경은 시설 위치나 가격을 변경하지 않는다. 접어도 키보드 정밀 조작은 유지하며 현재 적용 단위를 보여준다.

**구현 파일:**

- `Source/FestaFiesta/Public/Building/Presentation/FF_BuildToolsWidget.h`
- `Source/FestaFiesta/Private/Building/Presentation/FF_BuildToolsWidget.cpp`
- `Source/FestaFiesta/Private/Building/Presentation/FF_BuildToolsWbpBuilder.cpp`
- `Source/FestaFiesta/Private/Building/FF_BuildModeSubsystem.cpp`

### 2.5 시설 정보

**기존 연결:** `UFF_FacilityInfoSubsystem` → `FFF_FacilityInfoViewData` → `UFF_FacilityInfoPanel`.

| 노출 요소 | 기존 행동 / 전환 | 상태·검증 |
|---|---|---|
| 시설 정보 / 조합 / 운영 / 강화 탭 | `SetInfoPage` | 탭 전환은 게임 상태를 변경하지 않음. 지원하지 않는 강화 탭 숨김 |
| 즐겨찾기 | `OnFavoriteRequested` | 정의 ID 단위로 변경, 패널·카탈로그 동기화 |
| 이동 / M | `OnMoveRequested` → 이동 프리뷰 | Build 문맥과 `bCanMove`에 따름 |
| 철거 | 철거 확인 페이지 열기 | Build 문맥과 `bCanRemove`에 따름. 처음 누르는 행동이 즉시 철거하면 안 됨 |
| 철거 확인 / 취소 | `OnRemoveRequested` / Overview 복귀 | 회수액·설치비·대상 시설 명확히 표시. 중복 요청 차단 |
| 프로그램 편성 | 시설 전용 프로그램 선택 창 | `bShowProgramScheduleAction`, `bCanScheduleProgram`, 비활성 이유 반영 |
| 닫기 | `OnCloseRequested` | 월드 선택·패널 수명 계약 유지 |

시설 이름·썸네일·종류·운영 상태는 상단에서 빠르게 확인한다. 오늘/누적 매출과 1인 예상 수익을 구분한다. 설치 정보에는 크기, 안전 여유, 설치비, 회수액, 대기/동시 이용 수, 서비스 시간, 현재/최대 설치 수를 제공한다. `MaxInstancesPerWorld == 0`은 “제한 없음”이다. 내부 존 ID를 사용자가 이해할 수 있는 표시명으로 바꾸는 경우에도 실제 ID는 내부 연결에 보존한다.

**신규 표현 제안:** 현재 이미 존재하는 `bFacilityGeneratesRevenue`와 `RevenueStatusText`를 이용해 공연장 등에는 “직접 매출 없음 · 주변 시설 집객 지원”처럼 역할을 우선 보여준다. 새로운 집객 효과 수치나 측정되지 않은 효과를 만들어 표시하지 않는다.

**구현 파일:**

- `Source/FestaFiesta/Public/UI/Facility/FF_FacilityInfoTypes.h`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoTypes.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoSubsystem.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoPanel.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoWbpBuilder.cpp`
- `Content/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityInfoPanel.uasset`

### 2.6 시설 조합

현재 시설을 기준으로 적용 중인 조합과 설치/거리 조건이 부족한 조합을 읽기 쉽게 보여준다. “설정 보너스”와 “현재 적용 보너스”를 혼동하지 않는다.

| 노출 요소 | 기존 행동 / 전환 | 상태·검증 |
|---|---|---|
| 조합 항목의 상대 시설 찾기 | `HandleSynergyPartnerCatalogRequested` → `OpenCatalogForFacility` | 해당 시설을 가리는 필터를 해제하고 카드를 드러냄. 자동 배치 시작 금지 |
| 시설 조합 도감 | 시설 문맥이 있는 도감 열기 | 수익을 받는 시설·파트너 어느 쪽으로도 필터 가능 |
| 조합 상태/조건 | 현재 설치와 거리 조건 표시 | 없음·미설치·거리 부족·조건 충족·실제 효과 적용을 구분 |

레시피는 **보너스를 받는 시설 A + 도움을 주는 시설 B → A의 이용 수수료 증가**인 방향 관계다. 거리 기준은 시설 중심 간 수평 거리이며 높이는 무시한다. 같은 레시피의 파트너를 반복 설치해도 보너스가 반복 가산되지 않고, 같은 중복 방지 그룹은 가장 큰 보너스 하나만 적용한다. 시안의 `+8%`, `+12%`를 합계로 임의 가산하지 않는다.

**구현 파일:**

- `Source/FestaFiesta/Public/Building/Synergy/FF_FacilitySynergyTypes.h`
- `Source/FestaFiesta/Private/Building/Synergy/FF_FacilitySynergySubsystem.cpp`
- `Source/FestaFiesta/Private/Building/Synergy/FF_FacilitySynergyEvaluator.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilitySynergyEntryWidget.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoPanel.cpp`
- `Content/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilitySynergyEntry.uasset`
- `data/unreal/facility_synergies_v0.csv`

### 2.7 시설 운영

현재 운영 페이지는 상태를 읽고 관련 화면으로 이동하는 영역이다. 실제 연결이 없는 “운영 시작/정지”, 요금 조절, 인력 자동 배치 버튼을 시안에 장식으로 추가하지 않는다.

| 노출 요소 | 기존 행동 / 전환 | 상태·검증 |
|---|---|---|
| 시설별 매출 순위 | `OnRevenueRankingRequested` | 선택 시설과 기간 문맥 확인 |
| 운영 상태와 지표 | 권위 스냅샷 표시 | 준비 완료·운영 중·입장 가능·종료 등 실제 상태 사용 |
| 대기·이용 정보 | 대기 팀 수, 인원, 남은 서비스 인원, 예상 대기, 처리량 | 0명과 계산 불가를 구분. 단위와 분모를 명시 |

운영의 오늘/누적 매출은 실제 결제 완료 기준이며, 대기 인원이나 1인 예상 수익을 곱한 추정값으로 대체하지 않는다. 시설을 옮겨야 하는 상태와 이용 가능 여부도 최신 스냅샷에 따른다.

**구현 파일:** `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoSubsystem.cpp`, `FF_FacilityInfoPanel.cpp`, `FF_FacilityInfoTypes.cpp`; `Source/FestaFiesta/Private/Queue/FF_FacilityQueue.cpp`.

### 2.8 시설 정보의 유료 프로그램

**기존 연결:** `UFF_FacilityPaidProgramPicker`, `UFF_FacilityPaidProgramCard`, `UFF_FacilityInfoSubsystem`과 프로그램 편성 권위 처리.

| 노출 요소 | 기존 행동 / 전환 | 상태·검증 |
|---|---|---|
| 프로그램 카드 | 로컬 선택·상세 미리보기 | 현재 설치 시설과 호환되는 카드만 표시 |
| 구매·편성 버튼 | `OnConfirmed(ProgramId, FacilityInstanceId)` | 확인 시 금액·가용 인력·호환성·시간·현재 상태 재검증 |
| 닫기 | `OnCloseRequested` | 미리보기만 닫으면 구매 없음 |
| 완료 후 시설 정보로 돌아가기 | 영수증을 닫고 시설 패널 복귀 | 성공 응답 후에만 완료 표시 |

미리보기에는 프로그램명, 설명, 운영 시각, 필요 인력, 정원, 참가비, 최대 매출, 구매비, Gold/가용 인력의 전후 변화를 제공한다. 최대 매출은 정원×참가비이며 **확정 수익·순이익이 아니다**. 부족한 자원은 음수 잔액 허용으로 보이지 않게 “Gold 부족/인력 부족”으로 표시한다.

구매 전에 “오늘만 운영 · 당일 취소 및 환불 불가 · 편성 후 시설 이동·강화·철거 제한”을 CTA 가까이 안내한다. 구매 처리 중에는 중복 제출을 막고, 실패 시 선택을 유지하면서 최신 사유를 보여준다. 성공 영수증에는 프로그램명·시간·결제 금액·남은 Gold·남은 가용 인력을 표시한다. 라이브에서는 기존 편성을 읽기 전용으로 표시한다.

**중요 상태:** 호환 프로그램 없음, 아직 미선택, 편성 가능, 자원 부족/조건 불가, 제출 중, 제출 실패, 구매·편성 완료. 별도의 “무료 체험” 등 경제 규칙에 없는 행동을 만들지 않는다.

**구현 파일:**

- `Source/FestaFiesta/Public/UI/Facility/FF_FacilityPaidProgramTypes.h`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityPaidProgramPicker.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityPaidProgramCard.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoSubsystem.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoWbpBuilder.cpp`
- `Content/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramPicker.uasset`
- `Content/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityPaidProgramCard.uasset`

### 2.9 시설 강화

`FFF_FacilityUpgradePreview`를 기준으로 현재 레벨→다음 레벨, 1인 수익, 서비스 시간, 분당 처리량, 크기, 가격, 강화 후 잔액을 비교한다. `bHasLevels == false`이면 강화 탭·레벨 배지를 숨긴다.

| 노출 요소 | 기존 행동 / 전환 | 상태·검증 |
|---|---|---|
| 강화 탭 | Upgrade 페이지 | 지원 시설만 표시 |
| `Lv.N 강화 · X Gold` | `OnUpgradeRequested` | 현재 설치 인스턴스·검토한 레벨·최신 가격·Gold·설치 공간·프로그램 보호 등 권위 검증 |
| 오류·불가 이유 | `UpgradeDisabledReasonText` 등 | 잔액 부족, 최대 레벨, 데이터/공간/단계 제한을 실제 사유로 표시 |

0 Gold 견적도 유효할 수 있으므로 무료와 견적 없음은 다르다. 잔액 음수는 부채 허용이 아니라 부족 금액이다. 강화로 시설 크기가 증가할 수 있으며, 실패 시 가격 차감이나 시설 외형만 먼저 바뀌면 안 된다. 강화비는 철거 시 환급되지 않는 기존 안내를 유지한다. 구매 완료 후 최신 레벨·수익·크기·다음 견적이 갱신돼야 한다.

**구현 파일:**

- `Source/FestaFiesta/Public/Building/FF_FacilityUpgradeTypes.h`
- `Source/FestaFiesta/Private/Building/FF_FacilityUpgradeSubsystem.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoUpgradeLayout.h`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoTypes.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoPanel.cpp`
- `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoWbpBuilder.cpp`

### 2.10 시설 조합 도감

| 노출 요소 | 기존 행동 / 전환 | 상태·검증 |
|---|---|---|
| 전체 / 발견 / 미발견 | `SetDiscoveryFilter` | 발견 이력과 현재 적용 여부를 섞지 않음 |
| 검색 | `SetSearchText` | 시설명·조합명 검색. 결과 없음 표시 |
| 시설 필터 해제 | `SetFacilityFilter(NAME_None)` | 검색·발견 상태와 각 필터의 의미 구분 |
| 레시피 카드 | 상세 선택 | 두 시설 이미지, 이름, 보너스를 받는 시설, 거리, 발견 상태, 현재 조건, 실제 적용 표시 |
| 닫기 | `OnCloseRequested` | 이전 화면과 입력 포커스 복원 |

**신규 버튼 제안:** “효과 적용 중” 필터를 추가한다. `EFF_FacilityCodexFilterState::Active`는 현재 모델에 이미 있으나 첨부 화면의 필터 버튼에는 없으므로, 새 버튼과 이벤트 연결이 필요하다. 이를 별도 게임 규칙 개발과 혼동하지 않는다.

발견은 양쪽 시설을 설치한 이력이고, 조건 충족은 현재 설치·거리 조건이며, 효과 적용은 중복 방지 등을 적용한 실제 보너스다. 발견 기록이 남아도 현재 효과는 없어질 수 있다. 합계는 레시피 수이며 설치 시설 수 합계가 아니다. 같은 시설끼리 조합하는 레시피는 서로 다른 설치 인스턴스가 필요하다.

**구현 파일:**

- `Source/FestaFiesta/Public/Building/Codex/FF_FacilityCodexTypes.h`
- `Source/FestaFiesta/Private/Building/Codex/FF_FacilityCodexModel.cpp`
- `Source/FestaFiesta/Private/Building/Codex/FF_FacilityCodexSubsystem.cpp`
- `Source/FestaFiesta/Private/UI/FacilityCodex/FF_FacilityCodexScreen.cpp`
- `Source/FestaFiesta/Private/UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.cpp`
- `Source/FestaFiesta/Private/UI/FacilityCodex/FF_FacilityCodexWbpBuilder.cpp`
- `Content/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen.uasset`
- `Content/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard.uasset`

## 3. 부가 기능 보존

### 지원 시설의 조명 설정

`bHasLightingRig`인 시설만 조명 영역을 보이고 `bCanEditLighting`에 따라 활성화한다. 켜기/끄기, 밝기 슬라이더, 원래 색/청록/노을/보라 프리셋, 6자리 sRGB 입력, 기본 설정으로 되돌리기를 보존한다. 색 입력이 잘못되면 권위 명령을 보내지 않는다. 입력·슬라이더 드래그 중 시설 선택이 바뀌어도 다른 시설에 이전 편집을 보내지 않는다. 준비/라이브 문맥 모두 지원한다.

파일: `Source/FestaFiesta/Private/UI/Facility/FF_FacilityLightingControls.cpp`, `Source/FestaFiesta/Public/UI/Facility/FF_FacilityLightingControls.h`, `Content/FestaFiesta/Building/FF_WBP_Building/Facility/WBP_FF_FacilityLightingControls.uasset`.

### 미니게임

데이터에 미니게임이 연결된 시설은 `bHasMinigame`과 `MinigameActionText`에 따라 행동을 표시한다. `bCanStartMinigame`과 Live 문맥을 확인한 뒤 `OnMinigameRequested`를 전달한다. 단순한 “시설 이용” 버튼으로 바꾸거나 모든 시설에 공통으로 표시하지 않는다. 세션 시작·실패·복귀 후 시설 선택과 입력 상태를 검증한다.

### 월드 시설 배지

프로그램 가능 여부와 오늘 매출 등 기존 월드 배지를 유지한다. 선택한 시설과 해당 배지의 인스턴스가 일치해야 한다. 패널 뒤에 가려진 배지·장식이 클릭을 가로채지 않도록 한다. 배지 값은 화면 예시가 아니라 프로그램/매출 서브시스템을 따른다.

관련 파일: `Source/FestaFiesta/Private/UI/Facility/FF_FacilityProgramMarkerSubsystem.cpp`, `FF_FacilityProgramMarkerWidget.cpp`, `FF_FacilityWorldBadgeWbpBuilder.cpp`.

## 4. 화면 크기·입력·스크롤 기준

다음은 **승인 후 구현해야 할 디자인 목표**이며 현재 모든 해상도에서 검증됐다는 의미가 아니다.

| 기준 화면 | 레이아웃 목표 | 검증 항목 |
|---|---|---|
| 1280×720 | 제목·주요 값·행동 버튼 가독성을 유지. 세부 목록은 내부 스크롤, 정밀 조정은 펼침 사용. 도감은 카드가 작아지면 열 수를 줄이거나 상세를 아래로 재배치 | 작은 글씨로 전체를 축소하지 않음. 고정 하단 행동과 HUD가 겹치지 않음 |
| 1920×1080 | 기준 시안의 주 화면. 패널 고정 폭과 충분한 월드 시야, 핵심 행동을 스크롤 없이 확인 | 정보 위계·패널/월드 경계·가격/CTA 위치를 시안과 비교 |
| 2560×1440 | DPI 설정에 따라 논리적 크기를 유지하고 목록의 여유 공간을 활용 | 글씨가 지나치게 작아지거나 패널이 불필요하게 넓어지지 않음 |

- 게임의 실제 DPI 스케일을 적용한 PIE/Standalone 결과에서 검증한다. 문서의 픽셀 수를 모든 해상도의 고정 크기로 사용하지 않는다.
- 하단 HUD와 주요 탭의 안전 영역을 확보한다. 시설 정보의 이동/철거 등 고정 행동 영역과 본문 스크롤을 분리한다.
- 도감의 기존 `MinimumCardWidth`, 상세 재배치 정책을 검토하여 가독성을 유지한다. 720p에서 무조건 6열을 고정하지 않는다.
- 스크롤은 현재 포인터/포커스가 있는 패널에서만 작동한다. 내부 목록 스크롤이 월드 카메라 줌으로 새지 않게 한다.
- 검색·색상·각도 입력 중 B/Q/E/M/Enter 등 게임 단축키가 의도치 않게 실행되지 않게 한다. Enter는 현재 입력의 확정 의미가 우선한다.
- 패널 위 클릭·드래그는 월드 배치/이동/선택으로 전달되지 않는다. 각도 편집 또는 도구 호버 중 배치 목표 위치를 고정한다.
- 키보드 탭 순서는 화면의 읽기 순서와 같게 한다. 아이콘 버튼에도 접근 가능한 이름·툴팁을 둔다. 포커스와 선택 상태를 구분한다.
- 세부 화면을 닫으면 유효한 이전 선택/카테고리로 포커스를 돌린다. 삭제된 위젯에 포커스가 남지 않게 한다.
- 기존 ESC의 Pause Hub 연계와 상태 복귀 계약을 조사 없이 바꾸지 않는다. 닫기 버튼과 취소·Pause 입력을 같은 함수에 무조건 연결하지 않는다.
- 자유 회전의 누름/해제, 슬라이더 캡처, 텍스트 편집, Pause 진입/복귀, 창 포커스 이탈을 함께 검증한다.

## 5. 승인 후 버튼·상태 연결 체크리스트

아래 항목은 구현 완료 전에 실제 게임에서 확인하고, 결과와 미완료 이유를 기록한다. 시안 이미지의 버튼처럼 보이는 요소가 장식인지 조작인지 먼저 구분하고, 조작은 모두 이 목록 또는 동등한 검증 항목에 연결한다.

### 화면별 행동

- [ ] 카탈로그: 모든 대분류·소분류·검색·검색 초기화·즐겨찾기 별·즐겨찾기 필터·이전/다음 페이지·카드 선택·시설 확인·매출·도감·닫기가 작동한다.
- [ ] 카탈로그: 카드 별 버튼을 누르면 배치가 시작되지 않으며, 필요한 카드 찾기는 자동 구매/자동 배치로 이어지지 않는다.
- [ ] 순위: 오늘/누적·행 선택·정보 보기·닫기가 연결되고 0매출·빈 목록·동점·철거된 시설을 처리한다.
- [ ] 배치: 그리드·세 스냅·좌우 회전·자유 회전 누름/해제·각도 입력·네 방향 미세 이동·확정·목록·취소·종료가 연결된다.
- [ ] 신규 정밀 조정: 펼침/접기, 키보드 접근, 현재 스냅/각도 표시를 확인한다.
- [ ] 시설 정보: 모든 탭·즐겨찾기·이동·철거 진입·철거 확인·철거 취소·닫기·프로그램·매출 이동을 확인한다.
- [ ] 조합: 상대 시설 카탈로그 찾기·도감 연결과 미설치/거리 부족/중복 제한/적용 중 상태를 확인한다.
- [ ] 운영: 준비/라이브/종료 상태, 대기와 처리량, 매출 화면 연결이 실제 데이터와 맞는다.
- [ ] 프로그램: 카드 선택·구매·닫기·중복 제출 차단·실패 복구·성공 영수증·시설 정보 복귀가 연결된다.
- [ ] 강화: 탭·구매·최대 레벨·지원 없음·무료 견적·가격 없음·잔액 부족·공간 부족·프로그램 보호 상태를 확인한다.
- [ ] 도감: 전체/발견/미발견·검색·시설 필터 해제·선택 상세·닫기와 신규 “효과 적용 중” 필터를 확인한다.
- [ ] 조명: 켜기/끄기·밝기·모든 프리셋·색 입력·기본값 복원이 지원 시설에서만 작동한다.
- [ ] 이미지 또는 최종 WBP에 위 목록 밖의 버튼·아이콘·링크·탭이 있으면 기능 소유자, 이벤트, 활성 조건, 성공/실패 결과를 추가로 기록한다.

### 비용·권위 검증

- [ ] 카드/견적을 연 뒤 Gold가 달라진 경우 최신 비용과 잔액을 재검증한다.
- [ ] 미리보기 이후 시설 제거·이동·레벨 변경·프로그램 편성·축제 단계 변경이 발생해도 잘못된 대상에 명령을 보내지 않는다.
- [ ] 배치/이동의 최신 충돌·영역·안전 여유·설치 한도 검증을 건너뛰지 않는다.
- [ ] 프로그램의 Gold·인력·시간·시설 호환성과 이동/강화/철거 보호 정책을 유지한다.
- [ ] 강화의 검토 레벨과 권위 레벨이 달라지면 오래된 견적으로 결제하지 않는다.
- [ ] 제출 중 중복 클릭, 빠른 탭 전환, 패널 닫기와 동시에 발생한 응답을 처리한다.
- [ ] 서버/권위 처리 성공 전에는 성공 영수증, 차감 완료, 설치 완료를 확정 표시하지 않는다.
- [ ] 실패 시 프리뷰/선택을 가능한 한 유지하고 실제 실패 사유를 표시한다.
- [ ] 금액·인력·수익·조건은 포맷된 문자열을 역파싱하지 않고 타입이 있는 스냅샷에서 가져온다.

### 표현·회귀 검증

- [ ] 720p/1080p/1440p에서 잘림, 줄바꿈, 작은 글씨, 스크롤, 하단 HUD 중첩을 확인한다.
- [ ] 밝은 지면·어두운 야간·복잡한 조명 위에서 텍스트와 선택 상태가 읽힌다.
- [ ] 이리디센스/오로라/메쉬 그라디언트가 텍스트 대비나 버튼 활성 상태를 흐리지 않는다.
- [ ] 프로그램 미지원·강화 미지원·직접매출 없음·조명 미지원 시설은 관련 영역을 정확히 숨기거나 의미를 설명한다.
- [ ] 스크린샷의 내부 디버그 문구를 플레이어 안내로 옮기지 않는다.
- [ ] WBP 계약·화면별 기존 자동화 테스트와 변경된 액션에 필요한 회귀 검증을 실행한다.
- [ ] 시안 대비 최종 게임 캡처를 비교하고, 이미지와 달라진 항목은 이유와 실제 동작을 명시한다.

## 6. 기존 테스트와 구현 완료 근거

관련 기존 검증 위치는 다음과 같다. 모든 테스트를 무조건 새로 작성한다는 뜻은 아니다. 변경한 계약에 맞는 기존 테스트를 먼저 사용하고, 동작이 바뀌거나 기존 검증 공백이 있는 경우에만 의미 있는 검증을 추가한다.

- 카탈로그: `Source/FestaFiesta/Private/Building/UI/FF_BuildCatalogWidgetBlueprintTests.cpp`, `Source/FestaFiesta/Private/Building/FF_BuildSynergyCatalogContextTests.cpp`
- 배치: `Source/FestaFiesta/Private/Building/FF_BuildModeSubsystemTests.cpp`, `FF_BuildPlacementPointerPolicyTests.cpp`, `Presentation/FF_BuildToolsVisualTest.cpp`
- 시설 정보/조합: `Source/FestaFiesta/Private/UI/Facility/FF_FacilityInfoTests.cpp`, `FF_FacilitySynergyTabTests.cpp`
- 유료 프로그램: `Source/FestaFiesta/Private/UI/Facility/FF_FacilityPaidProgramPreviewTests.cpp`
- 강화: `Source/FestaFiesta/Private/Building/FF_FacilityUpgradeRulesTests.cpp`, `FF_FacilityUpgradeIntegrationTests.cpp`
- 순위: `Source/FestaFiesta/Private/UI/FacilityRevenue/FF_FacilityRevenueTests.cpp`
- 도감: `Source/FestaFiesta/Private/UI/FacilityCodex/FF_FacilityCodexUiTests.cpp`, `FF_FacilityCodexCatalogUiTests.cpp`, `FF_FacilityCodexPresenterTests.cpp`

이 문서 작성 단계에서는 테스트나 게임 화면 검증을 수행하지 않았다. 완료 기준은 사용자 시안 승인 후 실제 UMG 화면과 모든 노출 조작의 연결, 권위 상태 처리, 위 해상도·입력 검증까지 끝난 상태다.
