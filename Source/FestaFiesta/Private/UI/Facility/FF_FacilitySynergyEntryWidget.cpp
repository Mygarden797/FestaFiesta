// FF_FacilitySynergyEntryWidget.cpp
// 역할: 시설 정보의 조합 효과 한 행과 상대 시설로 이동하는 버튼을 표시한다.

#include "UI/Facility/FF_FacilitySynergyEntryWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"

void UFF_FacilitySynergyEntryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (Button_OpenPartnerCatalog)
    {
        Button_OpenPartnerCatalog->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePartnerCatalogClicked);
        Button_OpenPartnerCatalog->SetIsEnabled(!CurrentEntry.PartnerFacilityDefinitionId.IsNone());
    }
}

void UFF_FacilitySynergyEntryWidget::HandlePartnerCatalogClicked()
{
    if (!bHasEntry || CurrentEntry.PartnerFacilityDefinitionId.IsNone() ||
        GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden) return;
    const FName Destination = CurrentEntry.PartnerFacilityDefinitionId;
    OnPartnerCatalogClicked(Destination);
    OnPartnerCatalogRequested.Broadcast(Destination);
}

void UFF_FacilitySynergyEntryWidget::ApplyEntry(const FFF_FacilityInfoSynergyEntry& Entry)
{
    // Facility refreshes may also be triggered by queue occupancy. Unchanged combo data
    // keeps the existing Slate text layouts and avoids unnecessary Blueprint callbacks.
    if (bHasEntry && CurrentEntry.RuleId == Entry.RuleId && CurrentEntry.bActive == Entry.bActive &&
        CurrentEntry.PartnerFacilityDefinitionId == Entry.PartnerFacilityDefinitionId &&
        CurrentEntry.TitleText.EqualTo(Entry.TitleText) && CurrentEntry.PartnerText.EqualTo(Entry.PartnerText) &&
        CurrentEntry.EffectText.EqualTo(Entry.EffectText) && CurrentEntry.ConditionText.EqualTo(Entry.ConditionText) &&
        CurrentEntry.StatusText.EqualTo(Entry.StatusText))
    { return; }

    CurrentEntry = Entry;
    bHasEntry = true;
    if (Button_OpenPartnerCatalog) Button_OpenPartnerCatalog->SetIsEnabled(!Entry.PartnerFacilityDefinitionId.IsNone());
    if (Text_Title) Text_Title->SetText(Entry.TitleText);
    if (Text_Partner) Text_Partner->SetText(Entry.PartnerText);
    if (Text_Effect) Text_Effect->SetText(Entry.EffectText);
    if (Text_Condition) Text_Condition->SetText(Entry.ConditionText);
    if (Text_Status) Text_Status->SetText(Entry.StatusText);
    OnEntryApplied(Entry);
}
