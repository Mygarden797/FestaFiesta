// FF_FacilitySynergyTableBuilder.cpp
// 역할: 기존 조합 표 편집을 보존하면서 누락된 기본 행만 추가하는 에디터 도구이다.

// Copyright Epic Games, Inc. All Rights Reserved.

// Editor-only authoring: create a missing table or explicitly append missing CSV row keys.
// Neither operation rewrites an existing row, including its designer-authored display name.
// /Game/FestaFiesta/Data is already included by DirectoriesToAlwaysCook in DefaultGame.ini.
#include "FF_FacilitySynergyTableBuilder.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/AssetManager.h"
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

bool ValidateReferencedDefinitions(const UDataTable& Table, FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (!ValidateTable(Table, OutFailureReason)) return false;
	const UAssetManager* Manager = UAssetManager::GetIfInitialized();
	if (!Manager)
	{
		OutFailureReason = TEXT("asset manager is not initialized; wait for the facility catalog scan");
		return false;
	}
	TSet<FName> ValidatedIds;
	for (const FName RowName : Table.GetRowNames())
	{
		const auto* Row = Table.FindRow<FFF_FacilitySynergyRow>(RowName, TEXT("Validate facility references"));
		// Disabled rows remain valid work-in-progress data and never enter the evaluator.
		if (!Row->bEnabled) continue;
		for (const FName Id : {Row->TargetFacilityId, Row->PartnerFacilityId})
		{
			if (ValidatedIds.Contains(Id)) continue;
			const FSoftObjectPath Path = Manager->GetPrimaryAssetPath(
				FPrimaryAssetId(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Id));
			const auto* Definition = Cast<UFFZSFacilityDefinition>(Path.TryLoad());
			if (!Definition || Definition->Spec.FacilityDefinitionId != Id)
			{
				OutFailureReason = FString::Printf(TEXT("rule '%s' references missing/mismatched facility ID '%s'"),
					*RowName.ToString(), *Id.ToString());
				return false;
			}
			ValidatedIds.Add(Id);
		}
	}
	return true;
}

bool AppendMissingRows(UDataTable& Destination, const UDataTable& Source,
	TArray<FName>& OutAddedRows, FString& OutFailureReason)
{
	OutAddedRows.Reset();
	OutFailureReason.Reset();
	if (!ValidateTable(Destination, OutFailureReason) || !ValidateTable(Source, OutFailureReason)) return false;
	UDataTable* Merged = NewObject<UDataTable>(GetTransientPackage());
	Merged->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	for (const FName RowName : Destination.GetRowNames())
	{
		Merged->AddRow(RowName, *Destination.FindRow<FFF_FacilitySynergyRow>(RowName, TEXT("Preserve facility synergy")));
	}
	TArray<FName> MissingRows;
	for (const FName RowName : Source.GetRowNames())
	{
		if (Destination.GetRowMap().Contains(RowName)) continue;
		MissingRows.Add(RowName);
		Merged->AddRow(RowName, *Source.FindRow<FFF_FacilitySynergyRow>(RowName, TEXT("Append facility synergy")));
	}
	if (!ValidateReferencedDefinitions(*Merged, OutFailureReason)) return false;
	// A bad final CSV row or missing PDA cannot leave a partially appended destination.
	for (const FName RowName : MissingRows)
		Destination.AddRow(RowName, *Merged->FindRow<FFF_FacilitySynergyRow>(RowName, TEXT("Commit facility synergy append")));
	OutAddedRows = MoveTemp(MissingRows);
	return true;
}

FString GetCsvPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("data/unreal/facility_synergies_v0.csv"));
}

UDataTable* LoadSeedTable(FString& OutFailureReason)
{
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *GetCsvPath()))
	{
		OutFailureReason = FString::Printf(TEXT("cannot read %s"), *GetCsvPath());
		return nullptr;
	}
	// Parse and validate before creating a persistent object so malformed CSV leaves no asset.
	UDataTable* ParsedTable = NewObject<UDataTable>(GetTransientPackage());
	ParsedTable->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	const TArray<FString> ImportProblems = ParsedTable->CreateTableFromCSVString(Csv);
	if (!ImportProblems.IsEmpty() || !ValidateTable(*ParsedTable, OutFailureReason) || ParsedTable->GetRowNames().IsEmpty())
	{
		OutFailureReason = FString::Printf(TEXT("CSV validation failed: %s %s"),
			*OutFailureReason, *FString::Join(ImportProblems, TEXT("; ")));
		return nullptr;
	}
	return ParsedTable;
}

bool SaveTable(UDataTable& Table)
{
	const FString Filename = FPackageName::LongPackageNameToFilename(TablePackage, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Table.GetOutermost(), &Table, *Filename, SaveArgs);
}

void CreateTableIfMissing()
{
	// Check existence before loading: a failed load may leave an empty in-memory package.
	if (FPackageName::DoesPackageExist(TablePackage) || FindPackage(nullptr, *TablePackage))
	{
		UDataTable* Existing = LoadObject<UDataTable>(nullptr, *TableObjectPath);
		FString FailureReason;
		if (!Existing || !ValidateReferencedDefinitions(*Existing, FailureReason))
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
	UDataTable* ParsedTable = LoadSeedTable(FailureReason);
	if (!ParsedTable || !ValidateReferencedDefinitions(*ParsedTable, FailureReason))
	{
		UE_LOG(LogFFFacilitySynergyTableBuilder, Error, TEXT("FF_SYNERGY_TABLE_FAIL: %s"), *FailureReason);
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
		Table->AssetImportData->Update(GetCsvPath());
	}
	Table->MarkPackageDirty();
	if (!SaveTable(*Table))
	{
		UE_LOG(LogFFFacilitySynergyTableBuilder, Error, TEXT("FF_SYNERGY_TABLE_FAIL: could not save %s"), *TablePackage);
		return;
	}
	UE_LOG(LogFFFacilitySynergyTableBuilder, Display,
		TEXT("FF_SYNERGY_TABLE_CREATE: %s (%d rules); covered by /Game/FestaFiesta/Data cook directory."),
		*TablePackage, Table->GetRowNames().Num());
}

void AppendMissingSeedRows()
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, *TableObjectPath);
	if (!Table || Table->GetOutermost()->IsDirty())
	{
		UE_LOG(LogFFFacilitySynergyTableBuilder, Error,
			TEXT("FF_SYNERGY_TABLE_FAIL: append requires a saved existing table; save designer edits first or use FF.CreateFacilitySynergyTable for a missing asset."));
		return;
	}
	FString FailureReason;
	UDataTable* Source = LoadSeedTable(FailureReason);
	TArray<FName> AddedRows;
	if (!Source || !AppendMissingRows(*Table, *Source, AddedRows, FailureReason))
	{
		UE_LOG(LogFFFacilitySynergyTableBuilder, Error, TEXT("FF_SYNERGY_TABLE_FAIL: append preserved existing rows: %s"), *FailureReason);
		return;
	}
	if (AddedRows.IsEmpty())
	{
		UE_LOG(LogFFFacilitySynergyTableBuilder, Display,
			TEXT("FF_SYNERGY_TABLE_APPEND_NOOP: %s already contains every CSV row key; no values changed and no package saved."), *TablePackage);
		return;
	}
	Table->MarkPackageDirty();
	if (!SaveTable(*Table))
	{
		for (const FName RowName : AddedRows) Table->RemoveRow(RowName);
		Table->GetOutermost()->SetDirtyFlag(false);
		UE_LOG(LogFFFacilitySynergyTableBuilder, Error,
			TEXT("FF_SYNERGY_TABLE_FAIL: save failed; newly appended rows rolled back in memory: %s"), *TablePackage);
		return;
	}
	UE_LOG(LogFFFacilitySynergyTableBuilder, Display,
		TEXT("FF_SYNERGY_TABLE_APPEND: %s added %d rows, now %d; all existing row values and import settings preserved."),
		*TablePackage, AddedRows.Num(), Table->GetRowNames().Num());
}

FAutoConsoleCommand CreateFacilitySynergyTableCommand(
	TEXT("FF.CreateFacilitySynergyTable"),
	TEXT("Creates the missing facility synergy DataTable from its CSV; preserves all existing designer edits."),
	FConsoleCommandDelegate::CreateStatic(&CreateTableIfMissing));

FAutoConsoleCommand AppendFacilitySynergyRowsCommand(
	TEXT("FF.AppendFacilitySynergyRows"),
	TEXT("Explicitly appends missing CSV row keys to the saved facility synergy DataTable; never overwrites existing rows."),
	FConsoleCommandDelegate::CreateStatic(&AppendMissingSeedRows));

} // namespace FFFacilitySynergyTableBuilder

#endif // WITH_EDITOR
