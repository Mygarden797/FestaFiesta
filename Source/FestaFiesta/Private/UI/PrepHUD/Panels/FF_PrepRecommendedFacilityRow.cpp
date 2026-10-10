#include "UI/PrepHUD/Panels/FF_PrepRecommendedFacilityRow.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UFF_PrepRecommendedFacilityRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Place) Button_Place->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePlaceClicked);
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
	if (Text_Title) Text_Title->SetText(TitleText);
	if (Text_Description) Text_Description->SetText(DescriptionText);
	if (Text_Action) Text_Action->SetText(ActionText);
	if (Image_Icon) Image_Icon->SetBrush(IconBrush);
}

void UFF_PrepRecommendedFacilityRow::SetPlacementEnabled(bool bCanPlace, const FText& Reason)
{
	if (Button_Place)
	{
		Button_Place->SetIsEnabled(bCanPlace);
		Button_Place->SetToolTipText(Reason);
	}
}

void UFF_PrepRecommendedFacilityRow::HandlePlaceClicked()
{
	OnPlaceRequested.Broadcast(Kind);
}
