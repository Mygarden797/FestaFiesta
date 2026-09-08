#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Types/SlateEnums.h"
#include "FF_BuildToolsWidget.generated.h"

class UBorder;
class UEditableTextBox;
class UTextBlock;
class UFF_BuildModeSubsystem;

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
};

// The editable WBP owns layout and styling. This class only binds user actions and projects
// BuildModeSubsystem state; it has no tick, placement authority or duplicated snap state.
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Tools", DisableNativeTick))
class FESTAFIESTA_API UFF_BuildToolsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    bool IsPointerOverTools() const;
    bool IsEditingAngle() const;

    UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Tools")
    void RefreshTools();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_Tools;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Mode;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Legend;
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
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UEditableTextBox> Editable_Angle;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeLeft;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeUp;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeDown;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_NudgeRight;

private:
    TWeakObjectPtr<UFF_BuildModeSubsystem> BuildMode;
    void RefreshAngleText();
    bool ApplyAngleText(const FText& Text);
    bool ApplyPendingAngle();
    void SetSnap(int32 StepCm);
    void Rotate(float Degrees);
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
