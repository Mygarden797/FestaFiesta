#include "UI/PrepHUD/Panels/FF_PrepRecommendedFacilityRow.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UFF_PrepRecommendedFacilityRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Place) Button_Place->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePlaceClicked);
	ApplyPresentation();
}

void UFF_PrepRecommendedFacilityRow::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyPresentation();
}

void UFF_PrepRecommendedFacilityRow::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyPresentation();
}

void UFF_PrepRecommendedFacilityRow::ApplyPresentation()
{
	if (Text_Title) Text_Title->SetText(bCompactPresentation && !CompactTitleText.IsEmpty() ? CompactTitleText : TitleText);
	if (Text_Description)
	{
		Text_Description->SetText(DescriptionText);
		Text_Description->SetVisibility(bCompactPresentation ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (Text_Action) Text_Action->SetText(bCompactPresentation ? CompactActionText : ActionText);
	if (Image_Icon) Image_Icon->SetBrush(IconBrush);
	ApplyPlacementState();
}

void UFF_PrepRecommendedFacilityRow::SetPlacementEnabled(bool bCanPlace, const FText& Reason)
{
	bHasPlacementState = true;
	bPlacementAllowed = bCanPlace;
	PlacementDisabledReason = Reason;
	ApplyPlacementState();
}

void UFF_PrepRecommendedFacilityRow::ApplyPlacementState()
{
	if (Button_Place)
	{
		if (bHasPlacementState) Button_Place->SetIsEnabled(bPlacementAllowed);

		FText PlacementTooltip = PlacementDisabledReason;
		if (bCompactPresentation && !DescriptionText.IsEmpty())
		{
			PlacementTooltip = PlacementDisabledReason.IsEmpty()
				? DescriptionText
				: FText::Format(NSLOCTEXT("FestaFiestaPrepCheck", "RecommendedCompactTooltip", "{0}\n{1}"), PlacementDisabledReason, DescriptionText);
		}
		Button_Place->SetToolTipText(PlacementTooltip);
	}
}

void UFF_PrepRecommendedFacilityRow::HandlePlaceClicked()
{
	OnPlaceRequested.Broadcast(Kind);
}
