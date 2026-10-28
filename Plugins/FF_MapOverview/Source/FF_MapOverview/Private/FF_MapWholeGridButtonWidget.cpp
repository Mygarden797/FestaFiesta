#include "FF_MapWholeGridButtonWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "FF_MapOverviewSubsystem.h"

void UFFMapWholeGridButtonWidget::InitializeOverview(UFFMapOverviewSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    RefreshState();
}

void UFFMapWholeGridButtonWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!Subsystem.IsValid() && GetWorld()) Subsystem = GetWorld()->GetSubsystem<UFFMapOverviewSubsystem>();
    if (WholeGridButton)
    {
        RestingStyle = WholeGridButton->GetStyle();
        if (Icon) RestingIconColor = Icon->GetColorAndOpacity();
        if (Label) RestingTextColor = Label->GetColorAndOpacity();
        bHasRestingStyle = true;
        WholeGridButton->OnPressed.AddUniqueDynamic(this, &ThisClass::NotifyInteraction);
        WholeGridButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ActivateWholeGrid);
    }
    bStateInitialized = false;
    RefreshState();
}

void UFFMapWholeGridButtonWidget::NativeDestruct()
{
    if (WholeGridButton)
    {
        WholeGridButton->OnPressed.RemoveDynamic(this, &ThisClass::NotifyInteraction);
        WholeGridButton->OnClicked.RemoveDynamic(this, &ThisClass::ActivateWholeGrid);
        if (bHasRestingStyle) WholeGridButton->SetStyle(RestingStyle);
    }
    if (Icon) Icon->SetColorAndOpacity(RestingIconColor);
    if (Label) Label->SetColorAndOpacity(RestingTextColor);
    bHasRestingStyle = false;
    Super::NativeDestruct();
}

void UFFMapWholeGridButtonWidget::RefreshState()
{
    const bool bBuilding = Subsystem.IsValid() && Subsystem->IsBuildSessionActive();
    const bool bNewSelected = bBuilding && Subsystem->IsWholeGridEnabled() && Subsystem->IsGridVisible();
    SetIsEnabled(bBuilding);
    if (WholeGridButton) WholeGridButton->SetIsEnabled(bBuilding);
    if (bStateInitialized && bSelected == bNewSelected) return;
    bStateInitialized = true;
    bSelected = bNewSelected;
    if (bHasRestingStyle && WholeGridButton && bUseSelectionStyle)
    {
        WholeGridButton->SetStyle(bSelected ? SelectionStyle : RestingStyle);
        if (Icon) Icon->SetColorAndOpacity(bSelected ? SelectedIconColor : RestingIconColor);
        if (Label) Label->SetColorAndOpacity(bSelected ? SelectedTextColor : RestingTextColor);
    }
    OnWholeGridStateChanged(bSelected);
}

void UFFMapWholeGridButtonWidget::NotifyInteraction()
{
    if (Subsystem.IsValid()) Subsystem->NotifyMapInteraction();
}

void UFFMapWholeGridButtonWidget::ActivateWholeGrid()
{
    if (!Subsystem.IsValid() || !Subsystem->IsBuildSessionActive() || !GetIsEnabled()) return;
    Subsystem->ToggleWholeGrid();
    RefreshState();
}
