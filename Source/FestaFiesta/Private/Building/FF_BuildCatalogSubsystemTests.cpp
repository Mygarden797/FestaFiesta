// FF_BuildCatalogModel.cpp

#include "Building/FF_BuildCatalogModel.h"

// 개발/테스트 빌드에서만 컴파일하여 제품 런타임에는 자동화 테스트 코드가 포함되지 않게 합니다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "FF_FFZSFacilityDefinition.h"

namespace
{
	//정렬·그룹화 규칙에 필요한 최소 필드만 채운 테스트 항목을 만듭니다.
	// PrimaryAssetId, Definition, Thumbnail 등 해당 순수 변환에 관여하지 않는 값은 기본값으로 두어
	// 테스트 실패가 실제 비교 키와 그룹 키에만 집중되도록 합니다.
	 
	FFF_BuildCatalogEntry MakeCatalogEntry(
		const TCHAR* DefinitionId,
		const EFFZSFacilityCategory Category,
		const TCHAR* MinorToken,
		const TCHAR* DisplayName)
	{
		FFF_BuildCatalogEntry Entry;
		Entry.FacilityDefinitionId = FName(DefinitionId);
		Entry.Category = Category;
		Entry.MinorToken = FName(MinorToken);
		Entry.DisplayName = FText::FromString(DisplayName);
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogInstallCostProjectionTest,
	"FestaFiesta.Building.Catalog.InstallCostProjection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogInstallCostProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UFFZSFacilityDefinition* Definition = NewObject<UFFZSFacilityDefinition>();
	UDataTable* CostTable = NewObject<UDataTable>();
	TestNotNull(TEXT("A transient Facility Definition is available"), Definition);
	TestNotNull(TEXT("A transient facility cost table is available"), CostTable);
	if (!Definition || !CostTable)
	{
		return false;
	}

	Definition->Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_PriceProjection");
	Definition->Spec.Category = EFFZSFacilityCategory::Booth;
	Definition->Spec.InstallCostKey = NAME_None;
	CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow CostRow;
	CostRow.InstallCost = 400;
	CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), CostRow);

	FFF_BuildCatalogEntry Entry;
	Entry.FacilityDefinitionId = Definition->Spec.FacilityDefinitionId;
	FString FailureReason;
	TestTrue(TEXT("The UI snapshot uses the production Major.Minor price resolver"),
		FFBuildCatalogModel::ApplyInstallCost(Entry, Definition, CostTable, FailureReason));
	TestTrue(TEXT("The projected catalog entry marks its price as resolved"),
		Entry.bHasResolvedInstallCost);
	TestEqual(TEXT("The projected FoodStall price is 400 Gold"), Entry.InstallCost, 400);
	TestEqual(TEXT("The UI snapshot records the exact matched DataTable row"),
		Entry.MatchedInstallCostRow, FName(TEXT("Cost.Booth.FoodStall")));

	CostRow.InstallCost = 0;
	CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), CostRow);
	TestFalse(TEXT("A non-positive price is not displayable"),
		FFBuildCatalogModel::ApplyInstallCost(Entry, Definition, CostTable, FailureReason));
	TestFalse(TEXT("A failed projection clears the resolved flag"),
		Entry.bHasResolvedInstallCost);
	TestEqual(TEXT("A failed projection never leaves a misleading zero-price row"),
		Entry.MatchedInstallCostRow, NAME_None);
	return true;
}

// 에셋 발견 순서와 무관하게 평면 목록의 순서가 항상 같고, 그 정렬 결과가 메뉴 계층으로
// 항목 손실 없이 변환되는지 검증합니다.
 
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogDeterministicSortAndGroupingTest,
	"FestaFiesta.Building.Catalog.DeterministicSortAndGrouping",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogDeterministicSortAndGroupingTest::RunTest(const FString& Parameters)
{
	// 대분류·소분류·표시 이름 순서를 의도적으로 뒤섞습니다. AlphaA/AlphaB는 표시 이름까지 같아서
	// 마지막 FacilityDefinitionId 타이브레이커가 실제로 결정적 순서를 만드는 사례입니다.
	TArray<FFF_BuildCatalogEntry> Entries = {
		MakeCatalogEntry(TEXT("Toilet_Standard_Alpha"), EFFZSFacilityCategory::Toilet, TEXT("Standard"), TEXT("Alpha")),
		MakeCatalogEntry(TEXT("Booth_FoodStall_Zulu"), EFFZSFacilityCategory::Booth, TEXT("FoodStall"), TEXT("Zulu")),
		MakeCatalogEntry(TEXT("Booth_FoodStall_AlphaB"), EFFZSFacilityCategory::Booth, TEXT("FoodStall"), TEXT("Alpha")),
		MakeCatalogEntry(TEXT("Booth_ActivityGame_RingToss"), EFFZSFacilityCategory::Booth, TEXT("ActivityGame"), TEXT("Ring Toss")),
		MakeCatalogEntry(TEXT("Booth_FoodStall_AlphaA"), EFFZSFacilityCategory::Booth, TEXT("FoodStall"), TEXT("Alpha")),
	};

	FFBuildCatalogModel::SortEntries(Entries);

	// 기대 순서는 enum 대분류 -> 소분류 토큰 -> 표시 이름 -> 정의 ID 규칙을 모두 한 번씩 확인합니다.
	const TArray<FName> ExpectedOrder = {
		TEXT("Booth_ActivityGame_RingToss"),
		TEXT("Booth_FoodStall_AlphaA"),
		TEXT("Booth_FoodStall_AlphaB"),
		TEXT("Booth_FoodStall_Zulu"),
		TEXT("Toilet_Standard_Alpha"),
	};
	// 정렬은 순서만 바꿔야 하므로 먼저 항목 수 보존을 검증한 뒤 안전한 공통 범위에서 각 ID를 비교합니다.
	TestEqual(TEXT("all entries remain after sorting"), Entries.Num(), ExpectedOrder.Num());
	for (int32 Index = 0; Index < FMath::Min(Entries.Num(), ExpectedOrder.Num()); ++Index)
	{
		TestEqual(
			FString::Printf(TEXT("entry %d has deterministic order"), Index),
			Entries[Index].FacilityDefinitionId,
			ExpectedOrder[Index]);
	}

	TArray<FFF_BuildCatalogCategoryGroup> Groups;
	FFBuildCatalogModel::BuildCategoryGroups(Entries, Groups);
	// 동일한 평면 항목이 대분류 2개로 나뉘고, Booth 안에서는 소분류 2개가 정렬 순서대로 생겨야 합니다.
	TestEqual(TEXT("two major categories are emitted"), Groups.Num(), 2);
	// 앞선 수 검증이 실패해도 잘못된 배열 접근으로 테스트 프로세스가 중단되지 않도록 범위를 방어합니다.
	if (Groups.Num() == 2)
	{
		TestEqual(TEXT("enum order puts Booth first"), Groups[0].Category, EFFZSFacilityCategory::Booth);
		TestEqual(TEXT("Toilet follows Booth"), Groups[1].Category, EFFZSFacilityCategory::Toilet);
		TestEqual(TEXT("Booth contains two minor groups"), Groups[0].MinorGroups.Num(), 2);
		if (Groups[0].MinorGroups.Num() == 2)
		{
			// 소분류 정렬과 FoodStall 세 항목의 보존을 함께 확인합니다.
			TestEqual(TEXT("ActivityGame minor is lexical first"), Groups[0].MinorGroups[0].MinorToken, FName(TEXT("ActivityGame")));
			TestEqual(TEXT("FoodStall contains all three items"), Groups[0].MinorGroups[1].Items.Num(), 3);
		}
	}

	return true;
}

// 연속 새로고침과 서브시스템 종료 시 이전 비동기 콜백이 더 이상 현재 요청으로 인정되지 않는지
// 요청 세대 규칙을 엔진의 실제 비동기 로드 없이 검증합니다.
 
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogRequestGenerationTest,
	"FestaFiesta.Building.Catalog.RequestGenerationRejectsStaleCallbacks",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogRequestGenerationTest::RunTest(const FString& Parameters)
{
	// 0은 요청 전 상태이고, 각 BeginNextRequest 호출이 활성 세대를 앞으로 이동시킵니다.
	uint64 ActiveGeneration = 0;
	const uint64 FirstRequest = FFBuildCatalogModel::BeginNextRequest(ActiveGeneration);
	const uint64 SecondRequest = FFBuildCatalogModel::BeginNextRequest(ActiveGeneration);

	// 두 번째 요청이 시작된 뒤에는 취소된 첫 요청 콜백은 거부되고 최신 콜백만 반영할 수 있어야 합니다.
	TestFalse(
		TEXT("a callback from the canceled refresh cannot replace the new catalog"),
		FFBuildCatalogModel::IsCurrentRequest(FirstRequest, ActiveGeneration));
	TestTrue(
		TEXT("the latest refresh callback may commit"),
		FFBuildCatalogModel::IsCurrentRequest(SecondRequest, ActiveGeneration));

	// Deinitialize도 동일하게 세대를 증가시켜 종료 직전 진행 중이던 요청의 늦은 콜백을 무효화합니다.
	FFBuildCatalogModel::BeginNextRequest(ActiveGeneration);
	TestFalse(
		TEXT("deinitialization invalidates the last in-flight callback"),
		FFBuildCatalogModel::IsCurrentRequest(SecondRequest, ActiveGeneration));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS: 개발/자동화 테스트 빌드 전용 영역
