#include "UI/LiveHUD/FF_LiveTabButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UFF_LiveTabButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Text_Label && !DisplayLabelOverride.IsEmpty()) { Text_Label->SetText(DisplayLabelOverride); }
	if (Image_Icon && DesignerIcon.GetResourceObject()) { Image_Icon->SetBrush(DesignerIcon); }
}
