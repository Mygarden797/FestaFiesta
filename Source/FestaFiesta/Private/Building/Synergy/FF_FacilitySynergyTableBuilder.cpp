// Copyright Epic Games, Inc. All Rights Reserved.

// Editor-only authoring: the CSV seeds a missing asset, never rewrites designer changes.
// /Game/FestaFiesta/Data is already included by DirectoriesToAlwaysCook in DefaultGame.ini.
#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"
#include "FF_FFZSFacilityDefinition.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFFacilitySynergyTableBuilder, Log, All);

namespace FFFacilitySynergyTableBuilder
{

const FString TableFolder(TEXT("/Game/FestaFiesta/Data/Economy"));
const FString TablePackage(TableFolder / TEXT("DT_FacilitySynergies"));
const FString TableObjectPath(TablePackage + TEXT(".DT_FacilitySynergies"));

bool ValidateTable(const UDataTable& Table, FString& OutFailureReason)
{
	if (Table.GetRowStruct() != FFF_FacilitySynergyRow::StaticStruct())
	{
		OutFailureReason = TEXT("row struct must be FFF_FacilitySynergyRow");
		return false;
	}

	for (const FName RowName : Table.GetRowNames())
	{
		const FFF_FacilitySynergyRow* Row = Table.FindRow<FFF_FacilitySynergyRow>(
			RowName, TEXT("FF.CreateFacilitySynergyTable"), false);
		FString RowError;
		if (!Row || (Row->bEnabled && !Row->IsUsable(&RowError)))
		{
			OutFailureReason = FString::Printf(TEXT("invalid enabled rule '%s': %s"), *RowName.ToString(), *RowError);
			return false;
		}
	}
	return true;
}

bool ValidateSeedDefinitions(FString& OutFailureReason)
{
	// Verify the exact production IDs, not their display names or economy cost keys.
	const TCHAR* DefinitionPaths[] = {
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/PDA_FFZS_Booth_FoodStall_Burger.PDA_FFZS_Booth_FoodStall_Burger"),
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Rest/PDA_FFZS_Rest_Seating_Seating.PDA_FFZS_Rest_Seating_Seating"),
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/PDA_FFZS_Booth_FoodStall_IceCream.PDA_FFZS_Booth_FoodStall_IceCream"),
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Rest/PDA_FFZS_Rest_Shade_Shade.PDA_FFZS_Rest_Shade_Shade")
	};
	const FName ExpectedIds[] = {
		TEXT("Booth_FoodStall_Burger"), TEXT("Rest_Seating_Seating"),
		TEXT("Booth_FoodStall_IceCream"), TEXT("Rest_Shade_Shade")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(DefinitionPaths); ++Index)
	{
		const UFFZSFacilityDefinition* Definition = LoadObject<UFFZSFacilityDefinition>(nullptr, DefinitionPaths[Index]);
		if (!Definition || Definition->Spec.FacilityDefinitionId != ExpectedIds[Index])
		{
			OutFailureReason = FString::Printf(TEXT("missing/mismatched seed facility '%s'"), *ExpectedIds[Index].ToString());
			return false;
		}
	}
	return true;
}

void CreateTableIfMissing()
{
	// Check existence before loading: a failed load may leave an empty in-memory package.
	if (FPackageName::DoesPackageExist(TablePackage) || FindPackage(nullptr, *TablePackage))
	{
		UDataTable* Existing = LoadObject<UDataTable>(nullptr, *TableObjectPath);
		FString FailureReason;
		if (!Existing || !ValidateTable(*Existing, FailureReason))
		{
			UE_LOG(LogFFFacilitySynergyTableBuilder, Error,
				TEXT("FF_SYNERGY_TABLE_FAIL: existing unreadable or invalid package preserved: %s (%s)"),
				*TablePackage, *FailureReason);
			return;
		}
		UE_LOG(LogFFFacilitySynergyTableBuilder, Display,
			TEXT("FF_SYNERGY_TABLE_REUSE: preserved %s (%d rules); CSV was not reapplied."),
			*TablePackage, Existing->GetRowNames().Num());
		return;
	}

	FString FailureReason;
	if (!ValidateSeedDefinitions(FailureReason))
	{
		UE_LOG(LogFFFacilitySynergyTableBuilder, Error, TEXT("FF_SYNERGY_TABLE_FAIL: %s"), *FailureReason);
		return;
	}

	const FString CsvPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("data/unreal/facility_synergies_v0.csv"));
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *CsvPath))
	{
		UE_LOG(LogFFFacilitySynergyTableBuilder, Error, TEXT("FF_SYNERGY_TABLE_FAIL: cannot read %s"), *CsvPath);
		return;
	}

	// Parse and validate before creating a persistent object so malformed CSV leaves no asset.
	UDataTable* ParsedTable = NewObject<UDataTable>(GetTransientPackage());
	ParsedTable->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	const TArray<FString> ImportProblems = ParsedTable->CreateTableFromCSVString(Csv);
	if (!ImportProblems.IsEmpty() || !ValidateTable(*ParsedTable, FailureReason) || ParsedTable->GetRowNames().IsEmpty())
	{
		UE_LOG(LogFFFacilitySynergyTableBuilder, Error,
			TEXT("FF_SYNERGY_TABLE_FAIL: CSV validation failed: %s %s"),
			*FailureReason, *FString::Join(ImportProblems, TEXT("; ")));
		return;
	}

	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = FFF_FacilitySynergyRow::StaticStruct();
	FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UDataTable* Table = Cast<UDataTable>(AssetTools.Get().CreateAsset(
		FPackageName::GetLongPackageAssetName(TablePackage), TableFolder, UDataTable::StaticClass(), Factory));
	if (!Table)
	{
		UE_LOG(LogFFFacilitySynergyTableBuilder, Error, TEXT("FF_SYNERGY_TABLE_FAIL: could not create %s"), *TablePackage);
		return;
	}
	for (const FName RowName : ParsedTable->GetRowNames())
	{
		Table->AddRow(RowName, *ParsedTable->FindRow<FFF_FacilitySynergyRow>(RowName, TEXT("Seed facility synergies")));
	}
	if (Table->AssetImportData)
	{
		Table->AssetImportData->Update(CsvPath);
	}
	Table->MarkPackageDirty();
	const FString Filename = FPackageName::LongPackageNameToFilename(TablePackage, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Table->GetOutermost(), Table, *Filename, SaveArgs))
	{
		UE_LOG(LogFFFacilitySynergyTableBuilder, Error, TEXT("FF_SYNERGY_TABLE_FAIL: could not save %s"), *TablePackage);
		return;
	}
	UE_LOG(LogFFFacilitySynergyTableBuilder, Display,
		TEXT("FF_SYNERGY_TABLE_CREATE: %s (%d rules); covered by /Game/FestaFiesta/Data cook directory."),
		*TablePackage, Table->GetRowNames().Num());
}

FAutoConsoleCommand CreateFacilitySynergyTableCommand(
	TEXT("FF.CreateFacilitySynergyTable"),
	TEXT("Creates the missing facility synergy DataTable from its CSV; preserves all existing designer edits."),
	FConsoleCommandDelegate::CreateStatic(&CreateTableIfMissing));

} // namespace FFFacilitySynergyTableBuilder

#endif // WITH_EDITOR
