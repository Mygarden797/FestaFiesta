#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/PrepHUD/Build/FF_PrepBuildPresentationModel.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "Components/ComboBoxString.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Text/STextBlock.h"

namespace FFPrepBuildCatalogFilterTests
{
	bool ContainsSlateText(const TSharedRef<SWidget>& Widget, const FString& Expected)
	{
		if (Widget->GetType() == FName(TEXT("STextBlock"))
			&& StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() == Expected) return true;
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Index < Children->Num(); ++Index)
			if (ContainsSlateText(Children->GetChildAt(Index), Expected)) return true;
		return false;
	}

	FFF_BuildCatalogEntry Entry(const TCHAR* Id, const TCHAR* Name, const int32 Price)
	{
		FFF_BuildCatalogEntry Result;
		Result.FacilityDefinitionId = Id;
		Result.DisplayName = FText::FromString(Name);
		Result.Category = EFFZSFacilityCategory::Booth;
		Result.MinorToken = TEXT("FoodStall");
		Result.InstallCost = Price;
		Result.bHasResolvedInstallCost = true;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildAdvancedFiltersTest,
	"FestaFiesta.UI.PrepBuild.CatalogFilters.ComposeAndReset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildAdvancedFiltersTest::RunTest(const FString& Parameters)
{
	using namespace FFPrepBuildCatalogFilterTests;
	FFF_BuildCatalogEntry Target = Entry(TEXT("Booth_FoodStall_Cafe"), TEXT("카페"), 300);
	Target.bRequiredForSetup = true;
	Target.CompatibleProgramCount = 2;
	Target.bSupportsLighting = true;
	FFF_PrepBuildFilter Filter;
	Filter.bFilterByCategory = true;
	Filter.Category = EFFZSFacilityCategory::Booth;
	Filter.MinorToken = TEXT("FoodStall");
	Filter.SearchText = TEXT("카페");
	Filter.bFavoritesOnly = true;
	Filter.FavoriteIds.Add(Target.FacilityDefinitionId);
	Filter.bRequiredOnly = Filter.bAffordableOnly = Filter.bProgramsOnly = Filter.bLightingOnly = true;
	const auto Visible = [&](const FFF_BuildCatalogEntry& Candidate, int32 Gold = 300, bool bHasGold = true)
	{
		return FFF_PrepBuildPresentationRules::IsEntryVisible(Candidate, Filter, Gold, bHasGold);
	};
	TestTrue(TEXT("Every selected criterion composes and an exact balance is affordable"), Visible(Target));
	TestEqual(TEXT("All eight active restrictions are counted"), Filter.GetActiveFilterCount(), 8);
	TestFalse(TEXT("One coin short is excluded"), Visible(Target, 299));
	TestFalse(TEXT("An unknown balance cannot pass the available filter"), Visible(Target, 1000, false));
	FFF_BuildCatalogEntry Candidate = Target;
	Candidate.bBuildLimitReached = true;
	TestFalse(TEXT("A build limit cannot be bypassed by an adequate balance"), Visible(Candidate));
	Candidate = Target;
	Candidate.bHasResolvedInstallCost = false;
	TestFalse(TEXT("Unknown prices are not treated as free"), Visible(Candidate));
	Candidate = Target;
	Candidate.bRequiredForSetup = false;
	TestFalse(TEXT("Required criterion removes ordinary facilities"), Visible(Candidate));
	Candidate = Target;
	Candidate.CompatibleProgramCount = 0;
	TestFalse(TEXT("Programs criterion removes facilities without compatible programs"), Visible(Candidate));
	Candidate = Target;
	Candidate.bSupportsLighting = false;
	TestFalse(TEXT("Lighting criterion removes unlit booths"), Visible(Candidate));
	Candidate = Target;
	Candidate.Category = EFFZSFacilityCategory::Performance;
	TestFalse(TEXT("Tags never override an explicit category"), Visible(Candidate));
	Filter.SortOrder = EFF_PrepBuildSortOrder::PriceDescending;
	Filter.ResetSelections();
	TestEqual(TEXT("Reset removes every restriction"), Filter.GetActiveFilterCount(), 0);
	TestEqual(TEXT("Reset restores recommendation order"), Filter.SortOrder, EFF_PrepBuildSortOrder::Recommended);
	TestTrue(TEXT("Reset preserves saved favorite IDs"), Filter.FavoriteIds.Contains(Target.FacilityDefinitionId));
	TestTrue(TEXT("Reset restores ordinary catalog membership"), Visible(Candidate));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildCatalogSortingTest,
	"FestaFiesta.UI.PrepBuild.CatalogFilters.SortingAndBalance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildCatalogSortingTest::RunTest(const FString& Parameters)
{
	using namespace FFPrepBuildCatalogFilterTests;
	TArray<FFF_BuildCatalogEntry> Entries = {
		Entry(TEXT("Expensive"), TEXT("Charlie"), 600),
		Entry(TEXT("Cheap"), TEXT("Alpha"), 100),
		Entry(TEXT("EqualPrice"), TEXT("Bravo"), 100),
		Entry(TEXT("Unknown"), TEXT("Delta"), 0)
	};
	Entries[0].bRequiredForSetup = true;
	Entries[3].bHasResolvedInstallCost = false;
	UFF_PrepBuildPresentationModel* Model = NewObject<UFF_PrepBuildPresentationModel>();
	FFF_PrepBuildFilter Filter;
	const auto Reconcile = [&](int32 Gold = 1000)
	{
		Model->Reconcile(EFF_BuildCatalogLoadState::Ready, Entries, Filter, Gold, true, FText::GetEmpty());
	};
	Reconcile();
	UFF_PrepBuildTileData* OriginalCheap = Model->FindTile(TEXT("Cheap"));
	UFF_PrepBuildTileData* OriginalExpensive = Model->FindTile(TEXT("Expensive"));
	Filter.SortOrder = EFF_PrepBuildSortOrder::PriceAscending;
	Reconcile();
	const auto ExpectOrder = [&](const TCHAR* Context, const TArray<FName>& Expected)
	{
		const auto& Tiles = Model->GetVisibleTiles();
		TestEqual(Context, Tiles.Num(), Expected.Num());
		int32 Index = 0;
		for (const FName Id : Expected)
		{
			if (Tiles.IsValidIndex(Index)) TestEqual(Context, Tiles[Index]->GetFacilityDefinitionId(), Id);
			++Index;
		}
	};
	ExpectOrder(TEXT("Price ties preserve input order and unresolved prices sort last"),
		{TEXT("Cheap"), TEXT("EqualPrice"), TEXT("Expensive"), TEXT("Unknown")});
	Filter.SortOrder = EFF_PrepBuildSortOrder::PriceDescending;
	Reconcile();
	ExpectOrder(TEXT("Descending price still places unknown prices last"),
		{TEXT("Expensive"), TEXT("Cheap"), TEXT("EqualPrice"), TEXT("Unknown")});
	Filter.SortOrder = EFF_PrepBuildSortOrder::Name;
	Reconcile();
	ExpectOrder(TEXT("An explicit alphabetical choice takes priority over recommendation badges"),
		{TEXT("Cheap"), TEXT("EqualPrice"), TEXT("Expensive"), TEXT("Unknown")});
	Filter.bAffordableOnly = true;
	Reconcile(100);
	ExpectOrder(TEXT("Available filter reflects the current balance"), {TEXT("Cheap"), TEXT("EqualPrice")});
	Reconcile(600);
	ExpectOrder(TEXT("Increasing the balance restores newly affordable facilities"),
		{TEXT("Cheap"), TEXT("EqualPrice"), TEXT("Expensive")});
	TestTrue(TEXT("Filter/sort/balance changes reuse card objects"),
		Model->FindTile(TEXT("Cheap")) == OriginalCheap && Model->FindTile(TEXT("Expensive")) == OriginalExpensive);
	TestEqual(TEXT("Sorting never mutates the source catalog"), Entries[0].FacilityDefinitionId, FName(TEXT("Expensive")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepBuildCatalogSortInitializationTest,
	"FestaFiesta.UI.PrepBuild.CatalogFilters.SortLabelSurvivesInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepBuildCatalogSortInitializationTest::RunTest(const FString& Parameters)
{
	using namespace FFPrepBuildCatalogFilterTests;
	TStrongObjectPtr<UComboBoxString> Combo(NewObject<UComboBoxString>());
	for (int32 Index = 0; Index <= static_cast<int32>(EFF_PrepBuildSortOrder::PriceDescending); ++Index)
		Combo->AddOption(FFF_PrepBuildPresentationRules::ResolveSortDisplayName(static_cast<EFF_PrepBuildSortOrder>(Index)).ToString());
	Combo->SetSelectedIndex(0);
	TSharedRef<SWidget> Slate = Combo->TakeWidget();
	const auto ExpectLabel = [&](const TCHAR* Context, const EFF_PrepBuildSortOrder Order)
	{
		const FString Expected = FFF_PrepBuildPresentationRules::ResolveSortDisplayName(Order).ToString();
		TestEqual(Context, Combo->GetOptionCount(), 4);
		TestEqual(Context, Combo->GetSelectedOption(), Expected);
		// Checking only SelectedOption misses the UE5.7 failure: its visible SBox
		// can still contain SNullWidget after ClearOptions followed by the same value.
		TestTrue(Context, ContainsSlateText(Slate, Expected));
	};
	UFF_PrepPanelBuild::InitializeCatalogSortOptions(*Combo, EFF_PrepBuildSortOrder::Recommended);
	ExpectLabel(TEXT("Authored default label survives first NativeConstruct synchronization"), EFF_PrepBuildSortOrder::Recommended);
	UFF_PrepPanelBuild::InitializeCatalogSortOptions(*Combo, EFF_PrepBuildSortOrder::Name);
	ExpectLabel(TEXT("Reattachment retains the player's chosen order"), EFF_PrepBuildSortOrder::Name);
	Combo->ClearOptions();
	Combo->AddOption(TEXT("Legacy sort label"));
	UFF_PrepPanelBuild::InitializeCatalogSortOptions(*Combo, EFF_PrepBuildSortOrder::Name);
	ExpectLabel(TEXT("Rebuilding stale or translated options regenerates visible selected content"), EFF_PrepBuildSortOrder::Name);
	Combo->ReleaseSlateResources(true);
	Slate = Combo->TakeWidget();
	UFF_PrepPanelBuild::InitializeCatalogSortOptions(*Combo, EFF_PrepBuildSortOrder::Name);
	ExpectLabel(TEXT("A newly rebuilt Slate widget keeps all four options and its visible label"), EFF_PrepBuildSortOrder::Name);
	return true;
}

#endif
