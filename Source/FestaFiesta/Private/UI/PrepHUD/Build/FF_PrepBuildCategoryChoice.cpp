#include "UI/PrepHUD/Build/FF_PrepBuildCategoryChoice.h"

void UFF_PrepBuildCategoryChoice::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// These values are serialized on each tab. A dynamically populated minor tab may
	// still call ConfigureChoice afterwards, without PreConstruct undoing its data.
	ConfigureTab(AuthoredChoiceId, AuthoredLabel, AuthoredIcon);
}

void UFF_PrepBuildCategoryChoice::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime())
	{
		ConfigureTab(AuthoredChoiceId, AuthoredLabel, AuthoredIcon);
		SetSelected(bPreviewSelected);
	}
}
