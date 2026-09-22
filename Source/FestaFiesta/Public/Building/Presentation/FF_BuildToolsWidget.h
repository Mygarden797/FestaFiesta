// FF_BuildToolsWidget.h
// 역할: 건설 도구 버튼과 각도 입력을 건설 상태 및 명령에 연결한다.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Types/SlateEnums.h"
#include "TimerManager.h"
#include "FF_BuildToolsWidget.generated.h"

class UBorder;
class UEditableTextBox;
class UTextBlock;
class UFF_BuildModeSubsystem;
class UFF_BuildCatalogEconomyReadModel;

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_BuildToolsViewData
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bPreviewActive = false;
    UPROPERTY(BlueprintReadOnly) bool bMoving = false;
    UPROPERTY(BlueprintReadOnly) bool bDragPlacementEnabled = false;
    UPROPERTY(BlueprintReadOnly) bool bCanConfirm = false;
    UPROPERTY(BlueprintReadOnly) bool bGridVisible = false;
    UPROPERTY(BlueprintReadOnly) int32 PreferredSnapCm = 100;
    UPROPERTY(BlueprintReadOnly) int32 ActiveSnapCm = 100;
    UPROPERTY(BlueprintReadOnly) float YawDegrees = 0.0f;
    UPROPERTY(BlueprintReadOnly) FText FacilityName;
    UPROPERTY(BlueprintReadOnly) FText CostText;
    UPROPERTY(BlueprintReadOnly) FText StatusText;
    UPROPERTY(BlueprintReadOnly) FText BalanceText;
    UPROPERTY(BlueprintReadOnly) FText FootprintText;
    UPROPERTY(BlueprintReadOnly) FText QueueText;
};

namespace FFBuildToolsModel
{
    // Explicit degree input is finite, decimal and within the displayed 0..360 range.
    // Invalid edits leave the caller's previous value untouched; 360 wraps to 0.
    FESTAFIESTA_API bool TryParseYawText(const FString& Text, float& OutYawDegrees);
}

// UButton exposes focus initialization to subclasses, avoiding deprecated property writes.
UCLASS()
class FESTAFIESTA_API UFF_BuildToolsActionButton final : public UButton
{
    GENERATED_BODY()
public:
    UFF_BuildToolsActionButton();
    /** Editor builder only, before Slate construction. Catalog activation remains focusable. */
    void ConfigureFocusable(bool bFocusable) { InitIsFocusable(bFocusable); }

    // Normal/hover/pressed remain the ordinary Designer Button Style. Only the selected
    // variant lives here, so refreshes never replace a designer's brushes or padding.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Selection") bool bUseSelectionStyle = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Selection") FButtonStyle SelectionStyle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Selection") FSlateColor SelectionTextColor = FSlateColor(FLinearColor::White);
    void ApplySelection(bool bSelected, UTextBlock* Label);

private:
    UPROPERTY(Transient) FButtonStyle RestingStyle;
    FSlateColor RestingTextColor;
    bool bHasRestingStyle = false;
    bool bAppliedSelected = false;
};

// The editable WBP owns layout and styling. This class only binds user actions and projects
// BuildModeSubsystem state; tick only reprojects the small, non-interactive context marker.
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Tools"))
class FESTAFIESTA_API UFF_BuildToolsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    bool IsPointerOverTools() const;
    bool IsEditingAngle() const;
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ToggleRadialMenu();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ToggleHelp();
    bool IsRadialMenuOpen() const;
    bool IsAuxiliaryPanelOpen() const;

    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools")
    void RefreshTools();

    UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Tools")
    FFF_BuildToolsViewData GetToolsViewData() const { return ViewData; }
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ConfirmPlacement();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ReturnToCatalog();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ToggleGrid();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void SelectSnapStep(int32 StepCm);
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void RotateToNextSnap(int32 Direction);
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void MoveInZoneAxes(FIntPoint Direction);
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") bool ApplyYawText(const FText& Text);
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ToggleDirectAdjustment();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void FindNearbyPlacement();
    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools") void ShowNextPlacementProblem();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Status")
    FSlateColor ReadyStatusColor = FSlateColor(FLinearColor::FromSRGBColor(FColor(0x31, 0x57, 0x47)));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Status")
    FSlateColor InvalidStatusColor = FSlateColor(FLinearColor::FromSRGBColor(FColor(0xD7, 0x4E, 0x43)));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Status")
    FSlateColor NeutralStatusColor = FSlateColor(FLinearColor::FromSRGBColor(FColor(0x6F, 0x7B, 0x75)));

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Building|Tools")
    void BP_OnToolsStateChanged(const FFF_BuildToolsViewData& Data);

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_Tools;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_PlacementInfo;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_PlacementHelp;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_ContextPing;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ContextPing;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementAdvice;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlacementObstruction;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_AssistanceMessage;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_DirectAdjustment;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_FindNearby;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NextPlacementProblem;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_Radial;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_PlacementFacts;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Radial;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Facts;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Help;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Balance;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Footprint;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Queue;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ControlHelp;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Details;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_Details;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Mode;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Legend;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FacilityName;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Cost;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ActiveSnap;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_RotationHint;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Catalog;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Confirm;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Confirm;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Grid;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Grid;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SnapMeter;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SnapMeter;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SnapPrecision;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SnapPrecision;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_SnapFree;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SnapFree;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_RotateLeft;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_RotateRight;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_FreeRotateLeft;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_FreeRotateRight;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> Editable_Angle;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeLeft;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeUp;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeDown;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeRight;

private:
    TWeakObjectPtr<UFF_BuildModeSubsystem> BuildMode;
    TWeakObjectPtr<UFF_BuildCatalogEconomyReadModel> Economy;
    UPROPERTY(Transient) FFF_BuildToolsViewData ViewData;
    FTimerHandle FreeRotationTimer;
    int32 PointerRotationDirection = 0;
    int32 PlacementProblemIndex = 0;
    double LastPointerRotationTime = 0.0;
    UFUNCTION() void ToggleDetails();
    UFUNCTION() void ToggleFacts();
    void SetHelpExpanded(bool bExpanded);
    void CloseRadialMenu();
    void StartPointerFreeRotation(int32 Direction);
    void AdvancePointerFreeRotation();
    UFUNCTION() void StopPointerFreeRotation();
    UFUNCTION() void HandleFreeRotateLeftPressed();
    UFUNCTION() void HandleFreeRotateRightPressed();
    UFUNCTION() void HandleGoldChanged(int32 Gold);
    void RefreshAngleText();
    bool ApplyAngleText(const FText& Text);
    bool ApplyPendingAngle();
    void SetSnap(int32 StepCm);
    void Nudge(FIntPoint Direction);
    void SetChoiceState(UButton* Button, UTextBlock* Label, bool bSelected);

    UFUNCTION() void HandleGrid();
    UFUNCTION() void HandleConfirm();
    UFUNCTION() void HandleSnapMeter();
    UFUNCTION() void HandleSnapPrecision();
    UFUNCTION() void HandleSnapFree();
    UFUNCTION() void HandleRotateLeft();
    UFUNCTION() void HandleRotateRight();
    UFUNCTION() void HandleNudgeLeft();
    UFUNCTION() void HandleNudgeUp();
    UFUNCTION() void HandleNudgeDown();
    UFUNCTION() void HandleNudgeRight();
    UFUNCTION() void HandleAngleCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
