#include "UI/PrepHUD/Build/FF_PrepBuildCategoryChoice.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "UI/Core/FF_BuildAtlasImage.h"

FSlateBrush UFF_PrepBuildCategoryChoice::GetEffectiveIcon() const
{
	FSlateBrush Result = AuthoredIcon;
	if (IconAtlas)
		FFBuildAtlas::TryApply(Result, IconAtlas, IconAtlasColumns, IconAtlasRows, IconAtlasIndex);
	return Result;
}

void UFF_PrepBuildCategoryChoice::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// These values are serialized on each tab. A dynamically populated minor tab may
	// still call ConfigureChoice afterwards, without PreConstruct undoing its data.
	ConfigureTab(AuthoredChoiceId, AuthoredLabel, GetEffectiveIcon());
}

void UFF_PrepBuildCategoryChoice::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime())
	{
		ConfigureTab(AuthoredChoiceId, AuthoredLabel, GetEffectiveIcon());
		SetSelected(bPreviewSelected);
	}
}

void UFF_PrepBuildCategoryChoice::RefreshVisualState()
{
	Super::RefreshVisualState();
	FSlateBrush Probe = AuthoredIcon;
	const bool bKeepSourceColours = bPreserveIconColours &&
		FFBuildAtlas::TryApply(Probe, IconAtlas, IconAtlasColumns, IconAtlasRows, IconAtlasIndex);
	if (Image_Icon && bKeepSourceColours)
		Image_Icon->SetColorAndOpacity(FLinearColor::White);
	if (Border_IconBackdrop)
		Border_IconBackdrop->SetVisibility(bKeepSourceColours ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
