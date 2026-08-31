// Copyright Epic Games, Inc. All Rights Reserved.

// One-shot, non-destructive authoring command for the planner-owned prize catalog.
// Existing valid tables are never rewritten, so live balance and artist references survive reruns.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Minigame/ClawMachine/FF_ClawMachineTypes.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/MetaData.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFClawMachinePrizeTableBuilder, Log, All);

namespace FFClawMachinePrizeTableBuilder
{
	const FString TableFolder(TEXT("/Game/FestaFiesta/Minigame/Data"));
	const FString TablePackage(TableFolder / TEXT("DT_ClawMachinePrizes"));
	const FString TableObjectPath(TablePackage + TEXT(".DT_ClawMachinePrizes"));

	bool ValidateTable(const UDataTable& Table, FString& OutFailureReason)
	{
		if (Table.GetRowStruct() != FFF_ClawMachinePrizeTuning::StaticStruct())
		{
			OutFailureReason = TEXT("row struct must be FFF_ClawMachinePrizeTuning");
			return false;
		}

		const TArray<FName> RowNames = Table.GetRowNames();
		if (RowNames.IsEmpty())
		{
			OutFailureReason = TEXT("at least one enabled prize row is required");
			return false;
		}

		TSet<FName> SeenPrizeIds;
		for (const FName RowName : RowNames)
		{
			const FFF_ClawMachinePrizeTuning* Row = Table.FindRow<FFF_ClawMachinePrizeTuning>(
				RowName, TEXT("FF.BuildClawMachinePrizeTable"), false);
			if (!Row)
			{
				OutFailureReason = FString::Printf(TEXT("row '%s' could not be read"), *RowName.ToString());
				return false;
			}

			FFF_ClawMachinePrizeTuning Resolved = *Row;
			if (Resolved.PrizeId.IsNone())
			{
				Resolved.PrizeId = RowName;
			}
			FString RowFailureReason;
			if (!Resolved.IsValid(&RowFailureReason))
			{
				OutFailureReason = FString::Printf(
					TEXT("row '%s' is invalid: %s"), *RowName.ToString(), *RowFailureReason);
				return false;
			}
			if (SeenPrizeIds.Contains(Resolved.PrizeId))
			{
				OutFailureReason = FString::Printf(
					TEXT("duplicate PrizeId '%s'"), *Resolved.PrizeId.ToString());
				return false;
			}
			SeenPrizeIds.Add(Resolved.PrizeId);
		}
		return true;
	}

	void AddDefaultRows(UDataTable& Table)
	{
		FFF_ClawMachinePrizeTuning Seal;
		Seal.PrizeId = TEXT("SeaSeal");
		Seal.DisplayName = FText::FromString(TEXT("하프물범 바다물개"));
		Seal.ScoreValue = 280;
		Seal.SpawnWeight = 1.35f;
		Seal.GripDifficulty = 0.34f;
		Seal.CaptureRadiusNormalized = 0.115f;
		Seal.MassKg = 0.28f;
		Seal.SurfaceFriction = 0.68f;
		Seal.GripCompliance = 0.78f;
		Seal.FallbackColor = FLinearColor(0.18f, 0.78f, 0.86f, 1.0f);
		Seal.VisualScale = FVector(1.05f, 1.28f, 0.82f);
		Table.AddRow(TEXT("SeaSeal"), Seal);

		FFF_ClawMachinePrizeTuning Octopus;
		Octopus.PrizeId = TEXT("CoralOctopus");
		Octopus.DisplayName = FText::FromString(TEXT("산호빛 문어"));
		Octopus.ScoreValue = 420;
		Octopus.SpawnWeight = 0.92f;
		Octopus.GripDifficulty = 0.49f;
		Octopus.CaptureRadiusNormalized = 0.100f;
		Octopus.MassKg = 0.36f;
		Octopus.SurfaceFriction = 0.62f;
		Octopus.GripCompliance = 0.72f;
		Octopus.CenterOfMassOffsetNormalized = FVector2D(0.015f, -0.01f);
		Octopus.FallbackColor = FLinearColor(1.0f, 0.28f, 0.34f, 1.0f);
		Octopus.VisualScale = FVector(0.95f, 0.95f, 1.08f);
		Table.AddRow(TEXT("CoralOctopus"), Octopus);

		FFF_ClawMachinePrizeTuning Whale;
		Whale.PrizeId = TEXT("BlueWhale");
		Whale.DisplayName = FText::FromString(TEXT("파도빛 고래"));
		Whale.ScoreValue = 650;
		Whale.SpawnWeight = 0.58f;
		Whale.GripDifficulty = 0.61f;
		Whale.CaptureRadiusNormalized = 0.092f;
		Whale.MassKg = 0.52f;
		Whale.SurfaceFriction = 0.48f;
		Whale.GripCompliance = 0.58f;
		Whale.CenterOfMassOffsetNormalized = FVector2D(0.045f, 0.0f);
		Whale.FallbackColor = FLinearColor(0.13f, 0.34f, 0.88f, 1.0f);
		Whale.VisualScale = FVector(1.28f, 0.82f, 0.78f);
		Table.AddRow(TEXT("BlueWhale"), Whale);
	}

	bool SaveNewTable(UDataTable& Table)
	{
		UPackage* Package = Table.GetOutermost();
		if (!Package)
		{
			return false;
		}

		FMetaData& MetaData = Package->GetMetaData();
		MetaData.SetValue(&Table, TEXT("FF.ClawMachine.Owner"), TEXT("FestaFiesta.ClawMachine.v1"));
		MetaData.SetValue(
			&Table,
			TEXT("FF.ClawMachine.GeneratedSource"),
			TEXT("Source/FestaFiesta/Private/UI/Minigame/FF_ClawMachinePrizeTableBuilder.cpp"));

		Table.MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Table, *Filename, Args);
	}

	void BuildPrizeTable()
	{
		if (UDataTable* Existing = LoadObject<UDataTable>(nullptr, *TableObjectPath))
		{
			FString FailureReason;
			if (!ValidateTable(*Existing, FailureReason))
			{
				UE_LOG(LogFFClawMachinePrizeTableBuilder, Error,
					TEXT("FF_CLAW_PRIZE_TABLE_FAIL: existing table was preserved but is invalid: %s (%s)"),
					*TablePackage, *FailureReason);
				return;
			}
			UE_LOG(LogFFClawMachinePrizeTableBuilder, Display,
				TEXT("FF_CLAW_PRIZE_TABLE_REUSE: valid planner-owned table preserved: %s"),
				*TablePackage);
			return;
		}

		if (FPackageName::DoesPackageExist(TablePackage))
		{
			UE_LOG(LogFFClawMachinePrizeTableBuilder, Error,
				TEXT("FF_CLAW_PRIZE_TABLE_FAIL: package exists but is not a readable UDataTable; preserved: %s"),
				*TablePackage);
			return;
		}

		UDataTableFactory* Factory = NewObject<UDataTableFactory>();
		Factory->Struct = FFF_ClawMachinePrizeTuning::StaticStruct();
		FAssetToolsModule& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UDataTable* Table = Cast<UDataTable>(AssetTools.Get().CreateAsset(
			FPackageName::GetLongPackageAssetName(TablePackage),
			TableFolder,
			UDataTable::StaticClass(),
			Factory));
		if (!Table || Table->GetRowStruct() != FFF_ClawMachinePrizeTuning::StaticStruct())
		{
			UE_LOG(LogFFClawMachinePrizeTableBuilder, Error,
				TEXT("FF_CLAW_PRIZE_TABLE_FAIL: could not create %s"), *TablePackage);
			return;
		}

		AddDefaultRows(*Table);
		FString FailureReason;
		if (!ValidateTable(*Table, FailureReason) || !SaveNewTable(*Table))
		{
			UE_LOG(LogFFClawMachinePrizeTableBuilder, Error,
				TEXT("FF_CLAW_PRIZE_TABLE_FAIL: validation/save failed for %s (%s)"),
				*TablePackage, *FailureReason);
			return;
		}

		UE_LOG(LogFFClawMachinePrizeTableBuilder, Display,
			TEXT("FF_CLAW_PRIZE_TABLE_OK: created planner/art catalog with 3 starter rows: %s"),
			*TablePackage);
	}

	FAutoConsoleCommand BuildPrizeTableCommand(
		TEXT("FF.BuildClawMachinePrizeTable"),
		TEXT("Creates the missing planner-owned claw-machine prize DataTable without overwriting it."),
		FConsoleCommandDelegate::CreateStatic(&BuildPrizeTable));
}

#endif // WITH_EDITOR
