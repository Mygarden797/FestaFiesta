#pragma once

#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "FF_MapWholeGridButtonWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UFFMapOverviewSubsystem;

/** Designer-authored toolbar cell; native code owns actions and state only. */
UCLASS(Blueprintable)
class FF_MAPOVERVIEW_API UFFMapWholeGridButtonWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitializeOverview(UFFMapOverviewSubsystem* InSubsystem);
    UFUNCTION(BlueprintCallable, Category="Map Overview|Toolbar") void RefreshState();
    UFUNCTION(BlueprintCallable, Category="Map Overview|Toolbar") void ActivateWholeGrid();
    UFUNCTION(BlueprintPure, Category="Map Overview|Toolbar") bool IsWholeGridSelected() const { return bSelected; }
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Selection") bool bUseSelectionStyle = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Selection") FButtonStyle SelectionStyle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Selection") FLinearColor SelectedIconColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance|Selection") FSlateColor SelectedTextColor = FSlateColor(FLinearColor::White);
    UFUNCTION(BlueprintImplementableEvent, Category="Map Overview|Toolbar") void OnWholeGridStateChanged(bool bWholeGridSelected);
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> WholeGridButton;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Icon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Label;
private:
    UFUNCTION() void NotifyInteraction();
    TWeakObjectPtr<UFFMapOverviewSubsystem> Subsystem;
    FButtonStyle RestingStyle;
    FLinearColor RestingIconColor = FLinearColor::White;
    FSlateColor RestingTextColor;
    bool bHasRestingStyle = false;
    bool bSelected = false;
    bool bStateInitialized = false;
};
