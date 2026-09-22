// FF_FacilityCodexPresenterSubsystem.h
// 역할: 도감 열기·닫기와 선택 시설·카탈로그 이동을 연결한다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Components/SlateWrapperTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "TimerManager.h"
#include "FF_FacilityCodexPresenterSubsystem.generated.h"

class AActor;
class APlayerController;
class AFF_FestivalPlayerController;
class SWidget;
class UFF_BuildModeSubsystem;
class UFF_FacilityCodexScreen;
class UFF_FacilityInfoSubsystem;
class UFF_FestivalStageSubsystem;
class UFFZSWorldSubsystem;
class UGameViewportClient;
class UPanelWidget;
class UFF_PrepHudBar;
class UFF_PrepPanelBuild;

/**
 * Facilities open an authored codex through this event-driven presentation boundary.
 * It temporarily owns input, never changes facility gameplay data or festival time,
 * and restores only the presentation leases that it acquired itself.
 */
UCLASS()
class FESTAFIESTA_API UFF_FacilityCodexPresenterSubsystem final : public UWorldSubsystem
{

	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

	/** NAME_None shows the whole collection; otherwise begin with the chosen facility. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Codex")
	bool OpenCodex(APlayerController* RequestingController, FName FocusFacilityId = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Codex")
	void CloseCodex();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Codex")
	bool IsCodexOpen() const;

	static FSoftClassPath GetDefaultScreenClassPath();

private:
	void BindSessionLifecycle(AFF_FestivalPlayerController& Controller);
	void CloseSession(bool bRestoreFocus);
	void HandleFacilityCatalogRequested(FName FacilityDefinitionId);
	void CompleteCatalogNavigation(FName FacilityDefinitionId);
	void RestoreInput(AFF_FestivalPlayerController& Controller);
	void SuppressCatalogVisibility(AFF_FestivalPlayerController& Controller);
	void RestoreCatalogVisibility();

	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	UFUNCTION()
	void HandleSnapshotRestored();

	UFUNCTION()
	void HandleControllerEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	UPROPERTY(Transient)
	TObjectPtr<UFF_FacilityCodexScreen> Screen;

	TWeakObjectPtr<AFF_FestivalPlayerController> OwningController;
	TWeakObjectPtr<UGameViewportClient> InputViewport;
	TWeakObjectPtr<UFF_BuildModeSubsystem> SuspendedBuildMode;
	TWeakObjectPtr<UFF_FacilityInfoSubsystem> SuppressedFacilityInfo;
	TWeakObjectPtr<UFF_FestivalStageSubsystem> BoundStage;
	TWeakObjectPtr<UFFZSWorldSubsystem> BoundZoneSystem;
	TWeakObjectPtr<UFF_PrepPanelBuild> SuppressedCatalog;
	TWeakObjectPtr<UPanelWidget> SuppressedCatalogParent;
	TWeakObjectPtr<UFF_PrepHudBar> SuppressedCatalogBar;
	ESlateVisibility PreviousCatalogVisibility = ESlateVisibility::Collapsed;
	bool bCatalogWasInViewport = false;
	bool bCatalogHadOwningBar = false;
	TWeakPtr<SWidget> PreviousFocusedWidget;
	FTimerHandle CatalogNavigationTimer;
	bool bOwnsModalInput = false;
	bool bPreviousCursorVisible = false;
	bool bPreviousViewportIgnoredInput = false;
	bool bClosing = false;
	bool bDeinitializing = false;
};
