// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "UI/Facility/FF_FacilityInfoTypes.h"
#include "FF_FacilityInfoSubsystem.generated.h"

class AFFZSFacilityActor;
class AFF_FestivalPlayerController;
class AActor;
class APlayerController;
class UFF_FacilityInfoPanel;
struct FFFZSWorldChangeEvent;
#if WITH_DEV_AUTOMATION_TESTS
class FFF_FacilityInfoIndicatorLifecycleTest;
class FFF_FacilityInfoSelectionGeometryTest;
#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FFF_OnFacilityInfoSelectionChanged,
	FGuid, FacilityInstanceId,
	EFF_FacilityInfoContext, Context);

DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnFacilityInfoActionRequested, FGuid);

/**
 * One world-level coordinator for installed-facility selection and its non-modal UMG inspector.
 *
 * This project is single-local-player and its build/queue authority is world scoped, so a world
 * subsystem keeps selection lifecycle aligned with facility actors. Both build input and live
 * input call this same boundary; the widget never owns gameplay state.
 */
UCLASS()
class FESTAFIESTA_API UFF_FacilityInfoSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Loads and creates the hidden WBP early, avoiding a first-click asset-load hitch. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info")
	bool PrimePanel(APlayerController* OwningController);

	/** Selects only a currently registered installed facility. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info")
	bool SelectFacility(
		APlayerController* OwningController,
		AFFZSFacilityActor* Facility,
		EFF_FacilityInfoContext Context);

	/** Uses the Zone System's shared footprint resolver, then selects the result. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info")
	bool SelectFacilityAtWorldPoint(
		APlayerController* OwningController,
		const FVector& WorldPoint,
		EFF_FacilityInfoContext Context,
		FName ZoneId = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info")
	void ClearSelection();

	/** Clears only the caller-owned selection, avoiding cross-context teardown races. */
	void ClearSelectionIfMatches(FGuid FacilityInstanceId, EFF_FacilityInfoContext Context);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Info")
	AFFZSFacilityActor* GetSelectedFacility() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Info")
	FGuid GetSelectedFacilityId() const { return SelectedFacilityId; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Info")
	EFF_FacilityInfoContext GetSelectionContext() const { return SelectionContext; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Info")
	bool HasSelection() const { return SelectedFacilityId.IsValid(); }

	/** Used by raw build/camera input to prevent clicking through the visible card. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Info")
	bool IsPointerOverPanel() const;

	/** Keeps authoritative command failure details visible until selection changes. */
	void SetFeedback(const FText& Feedback, bool bIsError);

	static FSoftClassPath GetDefaultPanelClassPath();

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Facility Info")
	FFF_OnFacilityInfoSelectionChanged OnSelectionChanged;

	FFF_OnFacilityInfoActionRequested OnMoveRequested;
	FFF_OnFacilityInfoActionRequested OnRemoveRequested;
	FFF_OnFacilityInfoActionRequested OnCloseRequested;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_FacilityInfoIndicatorLifecycleTest;
	friend class FFF_FacilityInfoSelectionGeometryTest;
#endif

	bool EnsurePanel(APlayerController& OwningController);
	bool IsContextCurrentlyAllowed(EFF_FacilityInfoContext Context) const;
	bool BuildViewData(AFFZSFacilityActor& Facility, FFF_FacilityInfoViewData& OutViewData) const;
	void RefreshPanel();
	void AcquireThumbnailSession();
	void ReleaseThumbnailSession();
	void EnsurePickControllerBinding();
	void ReleasePickControllerBinding();
	AFFZSFacilityActor* ResolveLiveFacilityUnderCursor(
		APlayerController& Controller,
		AFFZSFacilityActor* PreferredHoveredFacility = nullptr) const;
	AFFZSFacilityActor* ResolveLiveFacilityAlongRay(
		const FVector& RayOrigin,
		const FVector& RayDirection,
		AFFZSFacilityActor* PreferredHoveredFacility,
		const AActor* IgnoredActor = nullptr) const;
	void UpdateLiveSelectionIndicators(float DeltaTime);
	void ApplyLiveSelectionIndicatorTargets(
		AFFZSFacilityActor* HoveredFacility,
		AFFZSFacilityActor* SelectedFacility);
	void ClearLiveSelectionIndicators();

	void HandlePanelMoveRequested();
	void HandlePanelRemoveRequested();
	void HandlePanelCloseRequested();

	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);

	UFUNCTION()
	void HandleWorldSnapshotRestored();

	/** Adapts the existing camera pick signal without changing Camera module files. */
	UFUNCTION()
	void HandleActorPicked(AActor* PickedActor);

	UPROPERTY(Transient)
	TObjectPtr<UFF_FacilityInfoPanel> Panel;

	TWeakObjectPtr<APlayerController> OwningController;
	TWeakObjectPtr<AFF_FestivalPlayerController> BoundPickController;
	TWeakObjectPtr<AFFZSFacilityActor> SelectionIndicatorHoverFacility;
	TWeakObjectPtr<AFFZSFacilityActor> SelectionIndicatorSelectedFacility;
	FGuid SelectedFacilityId;
	EFF_FacilityInfoContext SelectionContext = EFF_FacilityInfoContext::None;
	FText FeedbackText;
	bool bFeedbackIsError = false;
	bool bThumbnailSessionAcquired = false;
	float RefreshAccumulatorSeconds = 0.0f;
	float SelectionIndicatorRefreshAccumulatorSeconds = 0.0f;

	static constexpr float RefreshIntervalSeconds = 0.25f;
	static constexpr float SelectionIndicatorRefreshIntervalSeconds = 0.05f;
	static constexpr int32 PanelZOrder = 95;
};
