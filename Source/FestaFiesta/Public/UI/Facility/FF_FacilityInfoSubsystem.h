// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Time/FF_FestivalStage.h"
#include "UI/Facility/FF_FacilityInfoTypes.h"
#include "UI/Facility/FF_FacilityPaidProgramTypes.h"
#include "FF_FacilityInfoSubsystem.generated.h"

class AFFZSFacilityActor;
class AFF_FestivalPlayerController;
class AActor;
class APlayerController;
class UFF_FacilityInfoPanel;
class UFF_FacilityPaidProgramPicker;
class UFF_FestivalProgressionSubsystem;
class UFF_FestivalStageSubsystem;
struct FFFZSWorldChangeEvent;
#if WITH_DEV_AUTOMATION_TESTS
class FFF_FacilityInfoIndicatorLifecycleTest;
class FFF_FacilityInfoPausePresentationLifecycleTest;
class FFF_FacilityInfoProgramActionLifecycleTest;
class FFF_FacilityInfoProgramReadModelTest;
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

	/**
	 * Resolves a currently registered installed facility from its dedicated padded selection proxy.
	 * Build and Live input share this path so selection is independent of visual-mesh collision and
	 * the exact authority footprint. A real Visibility blocker in front of the proxy still wins.
	 */
	AFFZSFacilityActor* ResolveFacilityUnderCursor(
		APlayerController& Controller,
		AFFZSFacilityActor* PreferredHoveredFacility = nullptr) const;

	/**
	 * Pause Hub가 동일 Viewport 상위 Z-order의 시설 UI를 완전히 숨기고 입력도 차단한다.
	 * 선택과 picker 의미 상태는 유지하므로 해제 시 사용자가 보던 표면만 복원된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info|Pause")
	void SetPauseMenuPresentationSuppressed(bool bSuppressed);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Info|Pause")
	bool IsPauseMenuPresentationSuppressed() const
	{
		return bPauseMenuPresentationSuppressed;
	}

	/** Keeps authoritative command failure details visible until selection changes. */
	void SetFeedback(const FText& Feedback, bool bIsError);

	static FSoftClassPath GetDefaultPanelClassPath();
	static FSoftClassPath GetDefaultPaidProgramPickerClassPath();

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Facility Info")
	FFF_OnFacilityInfoSelectionChanged OnSelectionChanged;

	FFF_OnFacilityInfoActionRequested OnMoveRequested;
	FFF_OnFacilityInfoActionRequested OnRemoveRequested;
	FFF_OnFacilityInfoActionRequested OnProgramScheduleRequested;
	FFF_OnFacilityInfoActionRequested OnMinigameRequested;
	FFF_OnFacilityInfoActionRequested OnCloseRequested;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_FacilityInfoIndicatorLifecycleTest;
	friend class FFF_FacilityInfoPausePresentationLifecycleTest;
	friend class FFF_FacilityInfoProgramActionLifecycleTest;
	friend class FFF_FacilityInfoProgramReadModelTest;
	friend class FFF_FacilityInfoSelectionGeometryTest;
#endif

	bool EnsurePanel(APlayerController& OwningController);
	bool EnsurePaidProgramPicker(APlayerController& OwningController);
	bool BuildPaidProgramPickerViewData(
		AFFZSFacilityActor& Facility,
		FFF_FacilityPaidProgramPickerViewData& OutViewData,
		const FText& Feedback = FText::GetEmpty(),
		bool bIsFeedbackError = false) const;
	/** Makes the picker and facility card mutually exclusive so two panels never stack. */
	void SetPaidProgramPickerOpen(bool bOpen);
	void ClosePaidProgramPicker();
	void EnsureProgramReadModelBindings();
	bool IsContextCurrentlyAllowed(EFF_FacilityInfoContext Context) const;
	bool BuildViewData(AFFZSFacilityActor& Facility, FFF_FacilityInfoViewData& OutViewData) const;
	void RefreshPanel();
	void AcquireThumbnailSession();
	void ReleaseThumbnailSession();
	void EnsurePickControllerBinding();
	void ReleasePickControllerBinding();
	AFFZSFacilityActor* ResolveFacilityAlongRay(
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
	void HandlePanelProgramScheduleRequested();
	void HandlePanelUpgradeRequested();
	void HandlePanelLightingRequested(FGuid FacilityId, const FFFZSFacilityLightingState& State);
	void HandlePanelLightingResetRequested(FGuid FacilityId);
	bool CanEditSelectedLighting(FGuid FacilityId) const;
	void HandlePaidProgramConfirmed(FName ProgramId, FGuid FacilityInstanceId);
	void HandlePaidProgramPickerCloseRequested();
	void HandlePanelMinigameRequested();
	void HandlePanelCloseRequested();

	UFUNCTION()
	void HandleScheduledProgramsChanged();

	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);

	UFUNCTION()
	void HandleWorldSnapshotRestored();

	UFUNCTION()
	void HandleFacilityLightingChanged(FGuid FacilityInstanceId);

	/** Adapts the existing camera pick signal without changing Camera module files. */
	UFUNCTION()
	void HandleActorPicked(AActor* PickedActor);

	UPROPERTY(Transient)
	TObjectPtr<UFF_FacilityInfoPanel> Panel;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FacilityPaidProgramPicker> PaidProgramPicker;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalProgressionSubsystem> BoundProgression;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalStageSubsystem> BoundStage;

	TWeakObjectPtr<APlayerController> OwningController;
	TWeakObjectPtr<AFF_FestivalPlayerController> BoundPickController;
	TWeakObjectPtr<AFFZSFacilityActor> SelectionIndicatorHoverFacility;
	TWeakObjectPtr<AFFZSFacilityActor> SelectionIndicatorSelectedFacility;
	FGuid SelectedFacilityId;
	EFF_FacilityInfoContext SelectionContext = EFF_FacilityInfoContext::None;
	FText FeedbackText;
	bool bFeedbackIsError = false;
	bool bThumbnailSessionAcquired = false;
	// RootLayout보다 높은 전역 Z-order 시설 UI가 Pause Hub 위에서 클릭되지 않도록 하는 presentation lease.
	bool bPauseMenuPresentationSuppressed = false;
	// suppress 중 Collapsed 상태를 새 기준으로 덮지 않고 원래 활성 표면 하나만 복원한다.
	bool bPanelWasVisibleBeforePause = false;
	bool bPaidProgramPickerWasVisibleBeforePause = false;
	// Pause 중 월드 복원/삭제로 선택 세대가 바뀌면 오래된 UI를 되살리지 않기 위한 identity snapshot.
	FGuid PauseMenuSuppressedFacilityId;
	EFF_FacilityInfoContext PauseMenuSuppressedContext = EFF_FacilityInfoContext::None;
	float RefreshAccumulatorSeconds = 0.0f;
	float SelectionIndicatorRefreshAccumulatorSeconds = 0.0f;

	static constexpr float RefreshIntervalSeconds = 0.25f;
	static constexpr float SelectionIndicatorRefreshIntervalSeconds = 0.05f;
	/** Global viewport Z-orders; stage HUDs live in the same viewport layer at RootLayout Z=0. */
	static constexpr int32 PanelZOrder = 95;
	static constexpr int32 PaidProgramPickerZOrder = 96;
};
