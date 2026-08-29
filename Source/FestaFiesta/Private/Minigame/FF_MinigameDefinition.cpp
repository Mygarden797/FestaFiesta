// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/FF_MinigameDefinition.h"

#include "Minigame/FF_ClawMachineRuntime.h"
#include "Minigame/FF_MinigameRuntime.h"
#include "Minigame/FF_SurfingRuntime.h"
#include "Minigame/FF_TargetShootingRuntime.h"
#include "Minigame/Surfing/FF_SurfingRules.h"
#include "Engine/DataTable.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "FFMinigameDefinition"

UFF_MinigameDefinition::UFF_MinigameDefinition()
{
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::SessionStarted);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::SessionCompleted);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::SessionCancelled);
}

const FPrimaryAssetType& UFF_MinigameDefinition::GetMinigameDefinitionAssetType()
{
	static const FPrimaryAssetType AssetType(TEXT("FFMinigameDefinition"));
	return AssetType;
}

bool UFF_MinigameDefinition::SupportsFacility(const FName FacilityDefinitionId) const
{
	return !FacilityDefinitionId.IsNone()
		&& CompatibleFacilityDefinitionIds.Contains(FacilityDefinitionId);
}

int32 UFF_MinigameDefinition::ResolveRewardGoldAtMaxScore(
	const int32 FestivalFallbackGold) const
{
	return RewardGoldAtMaxScore >= 0
		? RewardGoldAtMaxScore
		: FMath::Max(0, FestivalFallbackGold);
}

bool UFF_MinigameDefinition::IsDefinitionValid(TArray<FText>& OutErrors) const
{
	const int32 ErrorCountBefore = OutErrors.Num();
	AppendValidationErrors(OutErrors);
	return OutErrors.Num() == ErrorCountBefore;
}

void UFF_MinigameDefinition::AppendValidationErrors(TArray<FText>& OutErrors) const
{
	if (MinigameId.IsNone())
	{
		OutErrors.Add(LOCTEXT("MissingMinigameId", "MinigameId is required."));
	}

	if (DisplayName.IsEmpty())
	{
		OutErrors.Add(LOCTEXT("MissingDisplayName", "DisplayName is required."));
	}

	if (!FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0f)
	{
		OutErrors.Add(LOCTEXT("InvalidDuration", "DurationSeconds must be finite and greater than zero."));
	}

	if (ScoreForMaxReward <= 0)
	{
		OutErrors.Add(LOCTEXT("InvalidRewardScore", "ScoreForMaxReward must be greater than zero."));
	}
	if (RewardGoldAtMaxScore < -1)
	{
		OutErrors.Add(LOCTEXT(
			"InvalidDefinitionRewardGold",
			"클리어 최대 획득 골드는 -1(축제 공통값 사용) 또는 0 이상의 값이어야 합니다."));
	}

	if (!RuntimeClass)
	{
		OutErrors.Add(LOCTEXT("MissingRuntimeClass", "RuntimeClass is required."));
	}
	else if (RuntimeClass->HasAnyClassFlags(CLASS_Abstract))
	{
		OutErrors.Add(LOCTEXT("AbstractRuntimeClass", "RuntimeClass must be concrete."));
	}

	if (CompatibleFacilityDefinitionIds.IsEmpty())
	{
		OutErrors.Add(LOCTEXT("MissingCompatibleFacilities", "At least one compatible facility is required."));
	}

	FString AudioFailureReason;
	if (!AudioProfile.IsValid(&AudioFailureReason))
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("InvalidAudioProfile", "미니게임 사운드 설정이 올바르지 않습니다: {0}"),
			FText::FromString(AudioFailureReason)));
	}

	TSet<FName> SeenFacilityIds;
	for (const FName FacilityDefinitionId : CompatibleFacilityDefinitionIds)
	{
		if (FacilityDefinitionId.IsNone())
		{
			OutErrors.Add(LOCTEXT("EmptyCompatibleFacility", "Compatible facility IDs cannot be empty."));
			continue;
		}

		if (SeenFacilityIds.Contains(FacilityDefinitionId))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("DuplicateCompatibleFacility", "Compatible facility ID '{0}' is duplicated."),
				FText::FromName(FacilityDefinitionId)));
		}
		SeenFacilityIds.Add(FacilityDefinitionId);
	}
}

FPrimaryAssetId UFF_MinigameDefinition::GetPrimaryAssetId() const
{
	return MinigameId.IsNone()
		? Super::GetPrimaryAssetId()
		: FPrimaryAssetId(GetMinigameDefinitionAssetType(), MinigameId);
}

#if WITH_EDITOR
EDataValidationResult UFF_MinigameDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Errors;
	if (!IsDefinitionValid(Errors))
	{
		for (const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

UFF_TargetShootingDefinition::UFF_TargetShootingDefinition()
{
	MinigameId = TEXT("TargetShooting");
	DisplayName = LOCTEXT("TargetShootingDisplayName", "부산 바다 물총 사격");
	CompatibleFacilityDefinitionIds = { TEXT("Booth_ActivityGame_Shoot") };
	DurationSeconds = 30.0f;
	ScoreForMaxReward = 2000;
	RewardGoldAtMaxScore = 150;
	RuntimeClass = UFF_TargetShootingRuntime::StaticClass();
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::TargetSpawned);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::ShotFired);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::TargetHit);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::TargetPrecision);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::TargetBullseye);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::TargetMissed);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::TargetEscaped);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::BonusTargetHit);
}

void UFF_TargetShootingDefinition::AppendValidationErrors(TArray<FText>& OutErrors) const
{
	Super::AppendValidationErrors(OutErrors);

	FString FailureReason;
	if (!Rules.IsValid(&FailureReason))
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("InvalidTargetShootingRules", "과녁 슈팅 세부 설정이 올바르지 않습니다: {0}"),
			FText::FromString(FailureReason)));
	}
}

UFF_SurfingDefinition::UFF_SurfingDefinition()
{
	MinigameId = TEXT("Surfing");
	DisplayName = LOCTEXT("SurfingDisplayName", "Busan Sea Free-Ride Surfing");
	CompatibleFacilityDefinitionIds = { TEXT("Booth_ActivityGame_Surfing") };
	DurationSeconds = 30.0f;
	ScoreForMaxReward = 3500;
	RuntimeClass = UFF_SurfingRuntime::StaticClass();
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::SurfObstacleHit);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::SurfObstacleSmashed);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::SurfCoinCollected);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::SurfHeartCollected);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::SurfBoostCollected);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::SurfBoostActivated);
	CourseLayoutTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/Minigame/Data/DT_SurfingCourse.DT_SurfingCourse")));
}

void UFF_SurfingDefinition::AppendValidationErrors(TArray<FText>& OutErrors) const
{
	Super::AppendValidationErrors(OutErrors);

	FString FailureReason;
	if (!Rules.IsValid(&FailureReason))
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("InvalidSurfingRules", "Surfing rules are invalid: {0}"),
			FText::FromString(FailureReason)));
	}

	if (CourseLayoutTable.IsNull())
	{
		OutErrors.Add(LOCTEXT(
			"MissingSurfingCourseLayout",
			"Surfing CourseLayoutTable is required."));
		return;
	}

	const UDataTable* CourseTable = CourseLayoutTable.LoadSynchronous();
	if (!CourseTable)
	{
		OutErrors.Add(LOCTEXT(
			"UnloadedSurfingCourseLayout",
			"Surfing CourseLayoutTable could not be loaded."));
		return;
	}
	if (CourseTable->GetRowStruct() != FFF_SurfingCourseRow::StaticStruct())
	{
		OutErrors.Add(LOCTEXT(
			"WrongSurfingCourseRowStruct",
			"Surfing CourseLayoutTable must use FFF_SurfingCourseRow."));
		return;
	}

	bool bHasEnabledObstacle = false;
	bool bHasEnabledPickup = false;
	TArray<FName> RowNames = CourseTable->GetRowNames();
	RowNames.Sort([](const FName A, const FName B)
	{
		return A.LexicalLess(B);
	});
	for (const FName RowName : RowNames)
	{
		const FFF_SurfingCourseRow* Row = CourseTable->FindRow<FFF_SurfingCourseRow>(
			RowName,
			TEXT("UFF_SurfingDefinition::AppendValidationErrors"),
			false);
		if (!Row || !Row->bEnabled)
		{
			continue;
		}

		if (!FMath::IsFinite(Row->PositionNormalized.X)
			|| !FMath::IsFinite(Row->PositionNormalized.Y)
			|| Row->PositionNormalized.X < -1.0
			|| Row->PositionNormalized.X > 1.0
			|| Row->PositionNormalized.Y < -1.0
			|| Row->PositionNormalized.Y > 1.0)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT(
					"InvalidSurfingCoursePosition",
					"Surfing course row '{0}' has a non-finite or out-of-range PositionNormalized."),
				FText::FromName(RowName)));
		}
		if (!FMath::IsFinite(Row->CollisionRadiusNormalized)
			|| Row->CollisionRadiusNormalized <= 0.0f)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT(
					"InvalidSurfingCourseRadius",
					"Surfing course row '{0}' must have a finite positive collision radius."),
				FText::FromName(RowName)));
		}
		if (!FMath::IsFinite(Row->ScoreValueOverride)
			|| Row->ScoreValueOverride < 0.0f)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT(
					"InvalidSurfingCourseScoreOverride",
					"Surfing course row '{0}' must have a finite non-negative score override."),
				FText::FromName(RowName)));
		}

		const bool bIsObstacle = FFSurfingRules::IsObstacleType(Row->Type);
		const bool bIsPickup = Row->Type == EFF_SurfingObjectType::Coin
			|| Row->Type == EFF_SurfingObjectType::Heart
			|| Row->Type == EFF_SurfingObjectType::BoostPickup;
		if (!bIsObstacle && !bIsPickup)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT(
					"InvalidSurfingCourseType",
					"Surfing course row '{0}' has an invalid object type."),
				FText::FromName(RowName)));
		}
		bHasEnabledObstacle |= bIsObstacle;
		bHasEnabledPickup |= bIsPickup;
	}

	if (!bHasEnabledObstacle)
	{
		OutErrors.Add(LOCTEXT(
			"MissingSurfingCourseObstacle",
			"Surfing CourseLayoutTable requires at least one enabled obstacle."));
	}
	if (!bHasEnabledPickup)
	{
		OutErrors.Add(LOCTEXT(
			"MissingSurfingCoursePickup",
			"Surfing CourseLayoutTable requires at least one enabled pickup."));
	}
}

UFF_ClawMachineDefinition::UFF_ClawMachineDefinition()
{
	MinigameId = TEXT("ClawMachine");
	DisplayName = LOCTEXT("ClawMachineDisplayName", "바다 친구 인형뽑기");
	CompatibleFacilityDefinitionIds = { TEXT("Booth_ActivityGame_ClawMachine") };
	DurationSeconds = 75.0f;
	ScoreForMaxReward = 1500;
	RewardGoldAtMaxScore = 150;
	RuntimeClass = UFF_ClawMachineRuntime::StaticClass();
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::ClawDropStarted);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::ClawClosing);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::ClawRelease);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::ClawPrizeCaptured);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::ClawPerfectCapture);
	AudioProfile.EventSounds.FindOrAdd(EFF_MinigameAudioEvent::ClawPrizeMissed);
	ScreenClass = TSoftClassPtr<UFF_MinigameScreenBase>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_ClawMachine.WBP_FF_ClawMachine_C")));
	PrizeCatalogTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/Minigame/Data/DT_ClawMachinePrizes.DT_ClawMachinePrizes")));

	FFF_ClawMachinePrizeTuning Seal;
	Seal.PrizeId = TEXT("SeaSeal");
	Seal.DisplayName = LOCTEXT("ClawPrizeSeal", "하프물범 바다물개");
	Seal.ScoreValue = 280;
	Seal.SpawnWeight = 1.35f;
	Seal.GripDifficulty = 0.34f;
	Seal.CaptureRadiusNormalized = 0.115f;
	Seal.MassKg = 0.28f;
	Seal.SurfaceFriction = 0.68f;
	Seal.GripCompliance = 0.78f;
	Seal.FallbackColor = FLinearColor(0.18f, 0.78f, 0.86f, 1.0f);
	Seal.VisualScale = FVector(1.05f, 1.28f, 0.82f);

	FFF_ClawMachinePrizeTuning Octopus;
	Octopus.PrizeId = TEXT("CoralOctopus");
	Octopus.DisplayName = LOCTEXT("ClawPrizeOctopus", "산호빛 문어");
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

	FFF_ClawMachinePrizeTuning Whale;
	Whale.PrizeId = TEXT("BlueWhale");
	Whale.DisplayName = LOCTEXT("ClawPrizeWhale", "파도빛 고래");
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

	Rules.PrizePool = { Seal, Octopus, Whale };
}

bool UFF_ClawMachineDefinition::BuildResolvedPrizePool(
	TArray<FFF_ClawMachinePrizeTuning>& OutPrizePool,
	FString* OutFailureReason) const
{
	OutPrizePool = Rules.PrizePool;
	if (PrizeCatalogTable.IsNull())
	{
		return !OutPrizePool.IsEmpty();
	}

	const UDataTable* Table = PrizeCatalogTable.LoadSynchronous();
	if (!Table)
	{
		if (OutFailureReason) { *OutFailureReason = TEXT("PrizeCatalogTable could not be loaded"); }
		return false;
	}
	if (Table->GetRowStruct() != FFF_ClawMachinePrizeTuning::StaticStruct())
	{
		if (OutFailureReason) { *OutFailureReason = TEXT("PrizeCatalogTable has the wrong row struct"); }
		return false;
	}

	TArray<FName> RowNames = Table->GetRowNames();
	RowNames.Sort(FNameLexicalLess());
	OutPrizePool.Reset(RowNames.Num());
	TSet<FName> SeenPrizeIds;
	for (const FName RowName : RowNames)
	{
		const FFF_ClawMachinePrizeTuning* Source = Table->FindRow<FFF_ClawMachinePrizeTuning>(
			RowName, TEXT("ClawMachinePrizeCatalog"), false);
		if (!Source)
		{
			continue;
		}
		FFF_ClawMachinePrizeTuning Row = *Source;
		if (Row.PrizeId.IsNone())
		{
			Row.PrizeId = RowName;
		}
		FString FailureReason;
		if (!Row.IsValid(&FailureReason) || SeenPrizeIds.Contains(Row.PrizeId))
		{
			if (OutFailureReason)
			{
				*OutFailureReason = SeenPrizeIds.Contains(Row.PrizeId)
					? FString::Printf(TEXT("duplicate PrizeId '%s'"), *Row.PrizeId.ToString())
					: FString::Printf(TEXT("invalid row '%s': %s"), *RowName.ToString(), *FailureReason);
			}
			return false;
		}
		SeenPrizeIds.Add(Row.PrizeId);
		OutPrizePool.Add(MoveTemp(Row));
	}
	if (OutPrizePool.IsEmpty())
	{
		if (OutFailureReason) { *OutFailureReason = TEXT("PrizeCatalogTable requires at least one row"); }
		return false;
	}
	return true;
}

void UFF_ClawMachineDefinition::AppendValidationErrors(TArray<FText>& OutErrors) const
{
	Super::AppendValidationErrors(OutErrors);
	FString FailureReason;
	if (!Rules.IsValid(&FailureReason))
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("InvalidClawMachineRules", "인형뽑기 세부 설정이 올바르지 않습니다: {0}"),
			FText::FromString(FailureReason)));
	}
	TArray<FFF_ClawMachinePrizeTuning> ResolvedPrizePool;
	FailureReason.Reset();
	if (!BuildResolvedPrizePool(ResolvedPrizePool, &FailureReason))
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("InvalidClawMachinePrizeCatalog", "인형 카탈로그가 올바르지 않습니다: {0}"),
			FText::FromString(FailureReason)));
	}
}

#undef LOCTEXT_NAMESPACE
