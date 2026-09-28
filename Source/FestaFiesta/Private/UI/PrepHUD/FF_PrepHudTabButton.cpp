#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UFF_PrepHudTabButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Text_Label && !DisplayLabelOverride.IsEmpty())
	{
		Text_Label->SetText(DisplayLabelOverride);
	}
	if (Image_Icon && DesignerIcon.GetResourceObject())
	{
		Image_Icon->SetBrush(DesignerIcon);
	}
}
