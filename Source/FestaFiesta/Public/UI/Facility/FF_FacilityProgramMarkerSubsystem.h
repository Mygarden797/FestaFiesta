// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "UI/Facility/FF_FacilityProgramMarkerWidget.h"
#include "FF_FacilityProgramMarkerSubsystem.generated.h"

class UFF_FestivalProgressionSubsystem;
class UFF_FestivalStageSubsystem;
class UFF_ProgramReservationSubsystem;
class UDataTable;
class AFFZSFacilityActor;
class APlayerController;
struct FFFZSWorldChangeEvent;

struct FESTAFIESTA_API FFF_FacilityWorldBadgeData
{
	EFF_FacilityProgramMarkerState ProgramState = EFF_FacilityProgramMarkerState::Hidden;
	bool bShowRevenue = false;
	bool bEconomyAvailable = false;
	int32 TodayGold = 0;
};

namespace FFFacilityWorldBadgeLayout
{
	/** Whole two-row rectangles are placed together. Failure means no nearby readable location. */
	FESTAFIESTA_API bool Place(const FVector2D& Anchor, const FVector2D& Size, const FBox2D& SafeBounds,
		const TArray<FBox2D>& Occupied, float Separation, float MaximumDisplacement, FVector2D& OutPosition);
}

/** Read-only projection of program compatibility and today's reservations into world badges. */
UCLASS()
class FESTAFIESTA_API UFF_FacilityProgramMarkerSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	EFF_FacilityProgramMarkerState GetMarkerState(FGuid FacilityInstanceId) const;
	bool GetBadgeData(FGuid FacilityInstanceId, FFF_FacilityWorldBadgeData& OutData) const;
	static FSoftClassPath GetDefaultWidgetClassPath();

private:
	void EnsureReadModelBindings();
	void RefreshMarkerStates();
	void RefreshPresentation(APlayerController* Controller);
	void ClearPresentation();
	void UpdateMarkerLocations();
	void RefreshOcclusionWidgets();

	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);
	UFUNCTION()
	void HandleStateChanged();
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);
	UFUNCTION()
	void HandleRevenueChanged(FGuid FacilityInstanceId);
	UFUNCTION()
	void HandleAmountChanged(int32 Amount);
	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalProgressionSubsystem> BoundProgression;
	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalStageSubsystem> BoundStage;
	UPROPERTY(Transient)
	TObjectPtr<UFF_ProgramReservationSubsystem> BoundReservations;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> BoundRevenueTable;
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UFF_FacilityProgramMarkerWidget>> Markers;
	UPROPERTY(Transient)
	TSubclassOf<UFF_FacilityProgramMarkerWidget> MarkerWidgetClass;

	TMap<FGuid, EFF_FacilityProgramMarkerState> MarkerStates;
	TMap<FGuid, FFF_FacilityWorldBadgeData> BadgeStates;
	TMap<FGuid, TWeakObjectPtr<AFFZSFacilityActor>> MarkerActors;
	TArray<TWeakObjectPtr<UWidget>> OcclusionWidgets;
	TWeakObjectPtr<APlayerController> OwningController;
	uint64 CachedRegistryRevision = MAX_uint64;
	bool bStateDirty = true;
	float OcclusionRefreshSeconds = 0.f;
};
