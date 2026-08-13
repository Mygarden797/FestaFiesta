// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/Presentation/FF_BuildFacilityCostTooltip.h"

#include "CommonTextBlock.h"

#define LOCTEXT_NAMESPACE "FFBuildFacilityCostTooltip"

void UFF_BuildFacilityCostTooltip::Configure(const FFF_BuildCatalogEntry& InEntry)
{
	Entry = InEntry;
	bConfigured = true;
	ApplyEntryToWidgets();
}

FText UFF_BuildFacilityCostTooltip::GetFacilityNameText() const
{
	if (!Entry.DisplayName.IsEmpty())
	{
		return Entry.DisplayName;
	}
	if (!Entry.UniqueNameToken.IsEmpty())
	{
		return FText::FromString(Entry.UniqueNameToken);
	}
	return Entry.FacilityDefinitionId.IsNone()
		? FText::GetEmpty()
		: FText::FromName(Entry.FacilityDefinitionId);
}

FText UFF_BuildFacilityCostTooltip::GetInstallCostText() const
{
	if (!HasResolvedInstallCost())
	{
		return LOCTEXT("InstallCostUnavailable", "가격 정보 없음");
	}

	return FText::Format(
		LOCTEXT("InstallCostFormat", "{0} Gold"),
		FText::AsNumber(Entry.InstallCost));
}

FText UFF_BuildFacilityCostTooltip::GetDisplayedInstallCostText() const
{
	return TXT_InstallCost ? TXT_InstallCost->GetText() : FText::GetEmpty();
}

bool UFF_BuildFacilityCostTooltip::HasResolvedInstallCost() const
{
	return bConfigured && Entry.bHasResolvedInstallCost && Entry.InstallCost > 0;
}

void UFF_BuildFacilityCostTooltip::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyEntryToWidgets();
}

void UFF_BuildFacilityCostTooltip::ApplyEntryToWidgets()
{
	const bool bUseDesignerPreview = IsDesignTime() && !bConfigured;
	const FText FacilityName = bUseDesignerPreview
		? DesignerPreviewFacilityName
		: GetFacilityNameText();
	const FText InstallCostText = bUseDesignerPreview
		? FText::Format(
			LOCTEXT("InstallCostFormat", "{0} Gold"),
			FText::AsNumber(DesignerPreviewInstallCost))
		: GetInstallCostText();

	if (TXT_FacilityName)
	{
		TXT_FacilityName->SetText(FacilityName);
	}
	if (TXT_InstallCost)
	{
		TXT_InstallCost->SetText(InstallCostText);
	}

	BP_OnTooltipDataChanged(
		bUseDesignerPreview || HasResolvedInstallCost(),
		bUseDesignerPreview ? DesignerPreviewInstallCost : Entry.InstallCost);
}

#undef LOCTEXT_NAMESPACE
