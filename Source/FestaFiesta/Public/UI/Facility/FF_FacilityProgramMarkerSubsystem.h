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
class UWidgetComponent;
class APlayerController;
struct FFFZSWorldChangeEvent;

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

private:
	void EnsureReadModelBindings();
	void RefreshMarkerStates();
	void RefreshPresentation(APlayerController* Controller);
	void ClearPresentation();
	void UpdateMarkerLocations();

	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);
	UFUNCTION()
	void HandleStateChanged();
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);
	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalProgressionSubsystem> BoundProgression;
	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalStageSubsystem> BoundStage;
	UPROPERTY(Transient)
	TObjectPtr<UFF_ProgramReservationSubsystem> BoundReservations;
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UWidgetComponent>> Markers;

	TMap<FGuid, EFF_FacilityProgramMarkerState> MarkerStates;
	TWeakObjectPtr<APlayerController> OwningController;
	uint64 CachedRegistryRevision = MAX_uint64;
	bool bStateDirty = true;
	float LocationRefreshSeconds = 0.f;
};
