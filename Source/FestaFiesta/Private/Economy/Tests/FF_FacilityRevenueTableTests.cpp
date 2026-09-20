// Contracts for the editor-authored facility revenue table.

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "DataTables/FF_FacilityRevenueTableRow.h"
#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalSettings.h"
#include "Misc/AutomationTest.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueTable_ExactRowAuthority,
	"FestaFiesta.Economy.FacilityRevenueTable.ExactRowAuthority",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityRevenueTable_ExactRowAuthority::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TMap<FName, int32> Legacy = {{TEXT("FoodStall"), 20}};
	FFFZSFacilitySpec First;
	First.FacilityDefinitionId = TEXT("Booth_FoodStall_RevenueTableFirst");
	First.bOverrideRevenuePerUseGold = true;
	First.RevenuePerUseGold = 999;
	FFFZSFacilitySpec Second = First;
	Second.FacilityDefinitionId = TEXT("Booth_FoodStall_RevenueTableSecond");
	TStrongObjectPtr<UDataTable> Table(NewObject<UDataTable>());
	Table->RowStruct = FFF_FacilityRevenueTableRow::StaticStruct();
	FFF_FacilityRevenueTableRow Row;
	Row.RevenuePerUseGold = 48;
	Table->AddRow(First.FacilityDefinitionId, Row);
	Row.RevenuePerUseGold = 80;
	Table->AddRow(Second.FacilityDefinitionId, Row);
	TestEqual(TEXT("The exact table row overrides a copied Data Asset price"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(First, Legacy, Table.Get()), 48);
	TestEqual(TEXT("Two facilities in one minor category retain independent table prices"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(Second, Legacy, Table.Get()), 80);
	Row.RevenuePerUseGold = 100;
	Table->AddRow(First.FacilityDefinitionId, Row);
	TestEqual(TEXT("Editing a row changes the next lookup for the same installed spec"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(First, Legacy, Table.Get()), 100);
	TestEqual(TEXT("Editing one row leaves its category sibling unchanged"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(Second, Legacy, Table.Get()), 80);
	TestEqual(TEXT("Table resolution does not overwrite the saved legacy amount"), First.RevenuePerUseGold, 999);
	Row.RevenuePerUseGold = 0;
	Table->AddRow(First.FacilityDefinitionId, Row);
	TestEqual(TEXT("A zero row deliberately disables income despite paid fallbacks"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(First, Legacy, Table.Get()), 0);
	First.bOverrideRevenuePerUseGold = false;
	TestEqual(TEXT("Table authority does not require enabling the old per-asset checkbox"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(First, Legacy, Table.Get()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueTable_MissingAndInvalidRows,
	"FestaFiesta.Economy.FacilityRevenueTable.MissingAndInvalidRows",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityRevenueTable_MissingAndInvalidRows::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TMap<FName, int32> Legacy = {{TEXT("FoodStall"), 20}};
	FFFZSFacilitySpec Spec;
	Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_RevenueTableFallback");
	Spec.bOverrideRevenuePerUseGold = true;
	Spec.RevenuePerUseGold = 64;
	TStrongObjectPtr<UDataTable> Table(NewObject<UDataTable>());
	Table->RowStruct = FFF_FacilityRevenueTableRow::StaticStruct();
	FFF_FacilityRevenueTableRow Row;
	Row.RevenuePerUseGold = 300;
	Table->AddRow(TEXT("FoodStall"), Row);
	TestEqual(TEXT("No table retains the individual Data Asset fallback"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(Spec, Legacy, nullptr), 64);
	TestEqual(TEXT("A minor-category row cannot impersonate the exact facility ID"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(Spec, Legacy, Table.Get()), 64);
	Spec.bOverrideRevenuePerUseGold = false;
	TestEqual(TEXT("A missing exact row with no per-asset override uses the legacy minor"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(Spec, Legacy, Table.Get()), 20);
	TestEqual(TEXT("No table with no per-asset override preserves the legacy minor"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(Spec, Legacy, nullptr), 20);
	Spec.bOverrideRevenuePerUseGold = true;
	Row.RevenuePerUseGold = -1;
	Table->AddRow(Spec.FacilityDefinitionId, Row);
	TestEqual(TEXT("A negative exact row fails to zero instead of silently paying a fallback"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(Spec, Legacy, Table.Get()), 0);
	TStrongObjectPtr<UDataTable> WrongType(NewObject<UDataTable>());
	WrongType->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost;
	Cost.InstallCost = 1000;
	WrongType->AddRow(Spec.FacilityDefinitionId, Cost);
	TestEqual(TEXT("The cost table cannot be reinterpreted as facility revenue"),
		FFF_BoothRevenueRules::ResolveRevenuePerUse(Spec, Legacy, WrongType.Get()), 0);
	TestEqual(TEXT("A wrong schema cannot supply a commission rate"), FFF_BoothRevenueRules::ResolveCommissionRate(Spec, .12f, WrongType.Get()), 0.f);
	TestEqual(TEXT("A wrong schema cannot supply a level multiplier"), FFF_BoothRevenueRules::ResolveRevenueMultiplier(Spec, 2, 1.5f, WrongType.Get()), 0.f);
	TestEqual(TEXT("A wrong schema cannot supply program fees"), FFF_BoothRevenueRules::ResolveProgramFeePerPerson(Spec.FacilityDefinitionId, TEXT("Program_SandArtWorkshop"), 30, WrongType.Get()), 0);
	TestEqual(TEXT("A wrong schema cannot supply minigame rewards"), FFF_BoothRevenueRules::ResolveMinigameRewardGold(Spec.FacilityDefinitionId, 150, WrongType.Get()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueTable_AllIncomeControls,
	"FestaFiesta.Economy.FacilityRevenueTable.AllIncomeControls",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityRevenueTable_AllIncomeControls::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFFZSFacilitySpec Spec;
	Spec.FacilityDefinitionId = TEXT("Booth_CraftExperience_AllIncomeControls");
	const FName Program(TEXT("Program_SandArtWorkshop"));
	TStrongObjectPtr<UDataTable> Table(NewObject<UDataTable>());
	Table->RowStruct = FFF_FacilityRevenueTableRow::StaticStruct();
	FFF_FacilityRevenueTableRow Row;
	Row.bOverrideCommissionRate = true;
	Row.CommissionRate = .5f;
	Row.RevenueMultiplierByLevel = {{1, .75f}, {2, 2.f}};
	Row.ProgramFeesPerPerson.Add(Program, 70);
	Table->AddRow(Spec.FacilityDefinitionId, Row);
	TestEqual(TEXT("The facility row overrides the Festival's commission rate"),
		FFF_BoothRevenueRules::ResolveCommissionRate(Spec, .12f, Table.Get()), .5f);
	TestEqual(TEXT("A level-one table reduction is applied without a design minimum"),
		FFF_BoothRevenueRules::ResolveRevenueMultiplier(Spec, 1, 1.f, Table.Get()), .75f);
	TestEqual(TEXT("The selected level uses its absolute table multiplier"),
		FFF_BoothRevenueRules::ResolveRevenueMultiplier(Spec, 2, 1.5f, Table.Get()), 2.f);
	TestEqual(TEXT("An unlisted level preserves the installed upgrade fallback"),
		FFF_BoothRevenueRules::ResolveRevenueMultiplier(Spec, 3, 2.25f, Table.Get()), 2.25f);
	TestEqual(TEXT("A program fee uses both the facility row and program ID"),
		FFF_BoothRevenueRules::ResolveProgramFeePerPerson(Spec.FacilityDefinitionId, Program, 30, Table.Get()), 70);
	TestEqual(TEXT("An unlisted program preserves its catalog fee"),
		FFF_BoothRevenueRules::ResolveProgramFeePerPerson(Spec.FacilityDefinitionId, TEXT("Program_SunsetYoga"), 25, Table.Get()), 25);
	TestEqual(TEXT("An unlisted facility cannot inherit another facility's program fee"),
		FFF_BoothRevenueRules::ResolveProgramFeePerPerson(TEXT("Booth_CraftExperience_Other"), Program, 30, Table.Get()), 30);
	Row.CommissionRate = 0.f;
	Row.RevenueMultiplierByLevel.Add(2, 0.f);
	Row.ProgramFeesPerPerson.Add(Program, 0);
	Table->AddRow(Spec.FacilityDefinitionId, Row);
	TestEqual(TEXT("Zero commission disables ordinary income without a paid fallback"),
		FFF_BoothRevenueRules::ResolveCommissionRate(Spec, .12f, Table.Get()), 0.f);
	TestEqual(TEXT("Zero level multiplier disables that level's ordinary income"),
		FFF_BoothRevenueRules::ResolveRevenueMultiplier(Spec, 2, 1.5f, Table.Get()), 0.f);
	TestEqual(TEXT("Zero program fee deliberately offers free admission"),
		FFF_BoothRevenueRules::ResolveProgramFeePerPerson(Spec.FacilityDefinitionId, Program, 30, Table.Get()), 0);
	Row.bOverrideCommissionRate = false;
	Row.CommissionRate = .9f;
	Table->AddRow(Spec.FacilityDefinitionId, Row);
	TestEqual(TEXT("The unchecked commission override retains the Festival rate"),
		FFF_BoothRevenueRules::ResolveCommissionRate(Spec, .12f, Table.Get()), .12f);
	for (const float Invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		Row.bOverrideCommissionRate = true;
		Row.CommissionRate = Invalid;
		Row.RevenueMultiplierByLevel.Add(2, Invalid);
		Row.ProgramFeesPerPerson.Add(Program, -1);
		Table->AddRow(Spec.FacilityDefinitionId, Row);
		TestEqual(TEXT("Invalid authored commission fails to zero"),
			FFF_BoothRevenueRules::ResolveCommissionRate(Spec, .12f, Table.Get()), 0.f);
		TestEqual(TEXT("Invalid authored level multiplier fails to zero"),
			FFF_BoothRevenueRules::ResolveRevenueMultiplier(Spec, 2, 1.5f, Table.Get()), 0.f);
		TestEqual(TEXT("Negative program fees fail to zero"),
			FFF_BoothRevenueRules::ResolveProgramFeePerPerson(Spec.FacilityDefinitionId, Program, 30, Table.Get()), 0);
	}
	TestEqual(TEXT("No table preserves the Festival commission"), FFF_BoothRevenueRules::ResolveCommissionRate(Spec, .12f, nullptr), .12f);
	TestEqual(TEXT("No table preserves saved upgrade multipliers"), FFF_BoothRevenueRules::ResolveRevenueMultiplier(Spec, 2, 1.5f, nullptr), 1.5f);
	TestEqual(TEXT("No table preserves legacy program fees"), FFF_BoothRevenueRules::ResolveProgramFeePerPerson(Spec.FacilityDefinitionId, Program, 30, nullptr), 30);
	Row = FFF_FacilityRevenueTableRow();
	Row.bOverrideMinigameRewardGold = true;
	Row.MinigameRewardAtMaxScoreGold = 90;
	Table->AddRow(Spec.FacilityDefinitionId, Row);
	TestEqual(TEXT("The exact facility row overrides its minigame's authored maximum reward"),
		FFF_BoothRevenueRules::ResolveMinigameRewardGold(Spec.FacilityDefinitionId, 150, Table.Get()), 90);
	TestEqual(TEXT("A facility without a minigame row keeps its own legacy reward"),
		FFF_BoothRevenueRules::ResolveMinigameRewardGold(TEXT("Booth_ActivityGame_Other"), 150, Table.Get()), 150);
	TestEqual(TEXT("No table preserves legacy minigame reward"),
		FFF_BoothRevenueRules::ResolveMinigameRewardGold(Spec.FacilityDefinitionId, 150, nullptr), 150);
	Row.MinigameRewardAtMaxScoreGold = 0;
	Table->AddRow(Spec.FacilityDefinitionId, Row);
	TestEqual(TEXT("Explicit zero disables minigame rewards without falling back"),
		FFF_BoothRevenueRules::ResolveMinigameRewardGold(Spec.FacilityDefinitionId, 150, Table.Get()), 0);
	Row.MinigameRewardAtMaxScoreGold = -1;
	Table->AddRow(Spec.FacilityDefinitionId, Row);
	TestEqual(TEXT("Negative minigame reward fails to zero"),
		FFF_BoothRevenueRules::ResolveMinigameRewardGold(Spec.FacilityDefinitionId, 150, Table.Get()), 0);
	Row.bOverrideMinigameRewardGold = false;
	Row.MinigameRewardAtMaxScoreGold = 90;
	Table->AddRow(Spec.FacilityDefinitionId, Row);
	TestEqual(TEXT("Disabling the table reward override keeps minigame-specific fallback"),
		FFF_BoothRevenueRules::ResolveMinigameRewardGold(Spec.FacilityDefinitionId, 150, Table.Get()), 150);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueTable_AuthoringValidation,
	"FestaFiesta.Economy.FacilityRevenueTable.AuthoringValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityRevenueTable_AuthoringValidation::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UFF_FestivalDefinition> Festival(NewObject<UFF_FestivalDefinition>());
	Festival->FestivalId = TEXT("RevenueValidationAutomation");
	Festival->RequiredSetupFacilities.Reset();
	TStrongObjectPtr<UDataTable> Costs(NewObject<UDataTable>());
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	Costs->AddRow(TEXT("Cost.Booth.Default"), FFF_FacilityCostRow());
	Festival->FacilityCostTable = Costs.Get();
	TStrongObjectPtr<UDataTable> Revenues(NewObject<UDataTable>());
	Revenues->RowStruct = FFF_FacilityRevenueTableRow::StaticStruct();
	Festival->FacilityRevenueTable = Revenues.Get();
	const FName RowName(TEXT("Booth_FoodStall_RevenueValidation"));
	FFF_FacilityRevenueTableRow Row;
	Row.RevenuePerUseGold = 0;
	Revenues->AddRow(RowName, Row);
	FDataValidationContext FreeRowContext;
	TestEqual(TEXT("A deliberate free row passes the editor validator"), Row.IsDataValid(FreeRowContext), EDataValidationResult::Valid);
	TArray<FText> Errors;
	TestTrue(TEXT("A Festival accepts the configured zero-price revenue table"), Festival->IsDefinitionValid(Errors));
	TestEqual(TEXT("The valid fixture has no unrelated validation errors"), Errors.Num(), 0);
	Row.RevenuePerUseGold = -1;
	Revenues->AddRow(RowName, Row);
	FDataValidationContext NegativeRowContext;
	TestEqual(TEXT("The Data Table editor rejects a negative revenue row"), Row.IsDataValid(NegativeRowContext), EDataValidationResult::Invalid);
	Errors.Reset();
	TestFalse(TEXT("Festival validation rejects an invalid revenue row before play"), Festival->IsDefinitionValid(Errors));
	TestTrue(TEXT("The validation message identifies the offending facility row"), Errors.ContainsByPredicate([&](const FText& Error)
	{
		return Error.ToString().Contains(RowName.ToString());
	}));
	const auto RejectRow = [&](const FFF_FacilityRevenueTableRow& Invalid, const TCHAR* Label)
	{
		Revenues->AddRow(RowName, Invalid);
		FDataValidationContext Context;
		TestEqual(Label, Invalid.IsDataValid(Context), EDataValidationResult::Invalid);
		Errors.Reset();
		TestFalse(TEXT("Festival validation also rejects the invalid income control"), Festival->IsDefinitionValid(Errors));
	};
	Row = FFF_FacilityRevenueTableRow();
	Row.bOverrideCommissionRate = true;
	Row.CommissionRate = 1.01f;
	RejectRow(Row, TEXT("Commission cannot exceed one hundred percent"));
	Row.CommissionRate = std::numeric_limits<float>::quiet_NaN();
	RejectRow(Row, TEXT("Nonfinite commission is invalid"));
	Row = FFF_FacilityRevenueTableRow();
	Row.RevenueMultiplierByLevel.Add(0, 1.f);
	RejectRow(Row, TEXT("Level zero cannot be authored"));
	Row.RevenueMultiplierByLevel = {{2, -1.f}};
	RejectRow(Row, TEXT("Negative level income multiplier is invalid"));
	Row.RevenueMultiplierByLevel = {{2, std::numeric_limits<float>::infinity()}};
	RejectRow(Row, TEXT("Infinite level income multiplier is invalid"));
	Row = FFF_FacilityRevenueTableRow();
	Row.ProgramFeesPerPerson.Add(TEXT("Program_SandArtWorkshop"), -1);
	RejectRow(Row, TEXT("Negative program fees are invalid"));
	Row.ProgramFeesPerPerson.Reset();
	Row.ProgramFeesPerPerson.Add(NAME_None, 10);
	RejectRow(Row, TEXT("A program override must identify its program"));
	Row = FFF_FacilityRevenueTableRow();
	Row.bOverrideMinigameRewardGold = true;
	Row.MinigameRewardAtMaxScoreGold = -1;
	RejectRow(Row, TEXT("Negative minigame reward overrides are invalid"));
	Festival->FacilityRevenueTable = Costs.Get();
	Errors.Reset();
	TestFalse(TEXT("A mistaken cost-table assignment is rejected during authoring"), Festival->IsDefinitionValid(Errors));
	TestTrue(TEXT("Wrong table schema produces a useful validation error"), !Errors.IsEmpty());
	Festival->FacilityRevenueTable.Reset();
	Errors.Reset();
	TestTrue(TEXT("Explicitly clearing the table retains valid legacy-only Festivals"), Festival->IsDefinitionValid(Errors));
	return true;
}
#endif // WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueTable_ContentCoverage,
	"FestaFiesta.Economy.FacilityRevenueTable.ContentCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityRevenueTable_ContentCoverage::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UFF_FestivalSettings* Settings = UFF_FestivalSettings::Get();
	const UFF_FestivalDefinition* Festival = Settings ? Settings->DefaultFestival.LoadSynchronous() : nullptr;
	if (!TestNotNull(TEXT("The configured Festival asset loads"), Festival)) return false;
	const UDataTable* Table = Festival->GetFacilityRevenueTable();
	if (!TestNotNull(TEXT("The Festival supplies its facility revenue table"), Table)) return false;
	TestEqual(TEXT("Revenue is authored in the requested Economy folder"), Table->GetPathName(),
		FString(TEXT("/Game/FestaFiesta/Data/Economy/DT_FacilityRevenues.DT_FacilityRevenues")));
	if (!TestTrue(TEXT("The asset uses the facility revenue row schema"),
		Table->GetRowStruct() == FFF_FacilityRevenueTableRow::StaticStruct())) return false;
	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FPrimaryAssetId> Assets;
	AssetManager.GetPrimaryAssetIdList(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Assets);
	if (!TestTrue(TEXT("Production facility definitions are registered"), !Assets.IsEmpty())) return false;
	TSet<FName> DefinitionIds;
	for (const FPrimaryAssetId& AssetId : Assets)
	{
		const UFFZSFacilityDefinition* Definition = Cast<UFFZSFacilityDefinition>(AssetManager.GetPrimaryAssetPath(AssetId).TryLoad());
		if (!TestNotNull(*FString::Printf(TEXT("Facility loads: %s"), *AssetId.ToString()), Definition)) continue;
		const FName Id = Definition->Spec.FacilityDefinitionId;
		DefinitionIds.Add(Id);
		const FFF_FacilityRevenueTableRow* Row = Table->FindRow<FFF_FacilityRevenueTableRow>(Id, TEXT("FacilityRevenueContentTest"), false);
		if (!TestNotNull(*FString::Printf(TEXT("Every facility has its own editable revenue row: %s"), *Id.ToString()), Row)) continue;
		TestTrue(*FString::Printf(TEXT("Revenue is nonnegative: %s"), *Id.ToString()), Row->RevenuePerUseGold >= 0);
		TArray<FText> RowErrors;
		TestTrue(*FString::Printf(TEXT("All authored income controls validate: %s"), *Id.ToString()), Row->IsRowValid(RowErrors));
		for (const FText& Error : RowErrors) AddError(FString::Printf(TEXT("%s: %s"), *Id.ToString(), *Error.ToString()));
		TestTrue(*FString::Printf(TEXT("The table owns this facility's commission rate: %s"), *Id.ToString()), Row->bOverrideCommissionRate);
		TestTrue(*FString::Printf(TEXT("The table authors level-one income: %s"), *Id.ToString()), Row->RevenueMultiplierByLevel.Contains(1));
		if (Definition->Spec.bUsesFacilityLevels)
		{
			for (const FFFZSFacilityUpgradeTier& Tier : Definition->Spec.UpgradeTiers)
			{
				TestTrue(*FString::Printf(TEXT("The table authors upgrade level %d income: %s"), Tier.TargetLevel, *Id.ToString()), Row->RevenueMultiplierByLevel.Contains(Tier.TargetLevel));
			}
		}
		TestFalse(*FString::Printf(TEXT("Editor rows have readable facility names: %s"), *Id.ToString()), Row->DisplayName.IsEmpty());
		TestEqual(*FString::Printf(TEXT("Runtime resolves the actual table price: %s"), *Id.ToString()),
			FFF_BoothRevenueRules::ResolveRevenuePerUse(Definition->Spec, Festival->RevenuePerUseByMinorToken, Table), Row->RevenuePerUseGold);
	}
	// Minigame definitions may reserve a facility ID before its buildable asset is added.
	// Accept only IDs actually routed by registered minigame assets, never arbitrary orphans.
	TArray<FPrimaryAssetId> MinigameAssets;
	AssetManager.GetPrimaryAssetIdList(UFF_MinigameDefinition::GetMinigameDefinitionAssetType(), MinigameAssets);
	TestTrue(TEXT("Registered minigame routing assets are discoverable"), !MinigameAssets.IsEmpty());
	for (const FPrimaryAssetId& AssetId : MinigameAssets)
	{
		const UFF_MinigameDefinition* Minigame = Cast<UFF_MinigameDefinition>(AssetManager.GetPrimaryAssetPath(AssetId).TryLoad());
		if (!TestNotNull(*FString::Printf(TEXT("Minigame routing asset loads: %s"), *AssetId.ToString()), Minigame)) continue;
		for (const FName FacilityId : Minigame->CompatibleFacilityDefinitionIds)
		{
			DefinitionIds.Add(FacilityId);
			const FFF_FacilityRevenueTableRow* Row = Table->FindRow<FFF_FacilityRevenueTableRow>(FacilityId, TEXT("MinigameRevenueContentTest"), false);
			if (!TestNotNull(*FString::Printf(TEXT("A registered minigame target has an editable reward row: %s"), *FacilityId.ToString()), Row)) continue;
			TestTrue(TEXT("Registered minigame target rewards are controlled by the table"), Row->bOverrideMinigameRewardGold);
			TArray<FText> RowErrors;
			TestTrue(TEXT("Reserved minigame revenue rows validate"), Row->IsRowValid(RowErrors));
		}
	}
	for (const FName RowName : Table->GetRowNames())
	{
		TestTrue(*FString::Printf(TEXT("Table rows refer to a current facility or registered minigame target: %s"), *RowName.ToString()), DefinitionIds.Contains(RowName));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
