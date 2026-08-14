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

FText UFF_BuildFacilityCostTooltip::GetRecoveryCostText() const
{
	if (!HasResolvedRecoveryCost())
	{
		return LOCTEXT("RecoveryCostUnavailable", "가격 정보 없음");
	}

	return FText::Format(
		LOCTEXT("RecoveryCostFormat", "{0} Gold"),
		FText::AsNumber(Entry.RecoveryCost));
}

FText UFF_BuildFacilityCostTooltip::GetDisplayedRecoveryCostText() const
{
	return TXT_RecoveryCost ? TXT_RecoveryCost->GetText() : FText::GetEmpty();
}

bool UFF_BuildFacilityCostTooltip::HasResolvedInstallCost() const
{
	return bConfigured && Entry.bHasResolvedInstallCost && Entry.InstallCost > 0;
}

bool UFF_BuildFacilityCostTooltip::HasResolvedRecoveryCost() const
{
	return bConfigured && Entry.bHasResolvedRecoveryCost && Entry.RecoveryCost >= 0;
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
	const FText DesignerCostPlaceholder = LOCTEXT("DesignerCostPlaceholder", "— Gold");
	const FText InstallCostText = bUseDesignerPreview
		? DesignerCostPlaceholder
		: GetInstallCostText();
	const FText RecoveryCostText = bUseDesignerPreview
		? DesignerCostPlaceholder
		: GetRecoveryCostText();

	if (TXT_FacilityName)
	{
		TXT_FacilityName->SetText(FacilityName);
	}
	if (TXT_InstallCost)
	{
		TXT_InstallCost->SetText(InstallCostText);
	}
	if (TXT_RecoveryCost)
	{
		TXT_RecoveryCost->SetText(RecoveryCostText);
	}

	BP_OnTooltipDataChanged(
		!bUseDesignerPreview && HasResolvedInstallCost(),
		bUseDesignerPreview ? 0 : Entry.InstallCost);
}

#undef LOCTEXT_NAMESPACE
