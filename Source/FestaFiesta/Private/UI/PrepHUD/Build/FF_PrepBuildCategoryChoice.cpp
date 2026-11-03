#include "UI/PrepHUD/Build/FF_PrepBuildCategoryChoice.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "UI/Core/FF_BuildAtlasImage.h"
#include "Widgets/Images/SImage.h"

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

void UFF_PrepBuildCategoryChoice::NativeConstruct()
{
	Super::NativeConstruct();
	// UImage's desired-size override only reaches an existing SImage. Apply the
	// compact layout after Slate construction as well as on subsequent selections.
	RefreshVisualState();
}

void UFF_PrepBuildCategoryChoice::RefreshIconOnlyLayout()
{
	if (!Button_Root || !Image_Icon) return;
	UButtonSlot* ContentSlot = Button_Root->GetContent()
		? Cast<UButtonSlot>(Button_Root->GetContent()->Slot) : nullptr;
	// Authored category art can have a SizeBox/Border around the actual image.
	// The caption gap belongs to that outer host's row slot, not to Image_Icon.
	UHorizontalBoxSlot* IconSlot = nullptr;
	for (UWidget* Host = Image_Icon; Host && Host != Button_Root; Host = Host->GetParent())
	{
		if (auto* RowSlot = Cast<UHorizontalBoxSlot>(Host->Slot))
		{
			IconSlot = RowSlot;
			break;
		}
	}
	if (bIconOnly && !bCapturedIconOnlyLayout)
	{
		const FButtonStyle& Style = Button_Root->GetStyle();
		AuthoredNormalPadding = Style.NormalPadding;
		AuthoredPressedPadding = Style.PressedPadding;
		if (ContentSlot)
		{
			AuthoredButtonSlotPadding = ContentSlot->GetPadding();
			AuthoredButtonHorizontalAlignment = ContentSlot->GetHorizontalAlignment();
			AuthoredButtonVerticalAlignment = ContentSlot->GetVerticalAlignment();
		}
		if (IconSlot)
		{
			AuthoredIconSlotPadding = IconSlot->GetPadding();
			AuthoredIconSlotSize = IconSlot->GetSize();
			AuthoredIconHorizontalAlignment = IconSlot->GetHorizontalAlignment();
			AuthoredIconVerticalAlignment = IconSlot->GetVerticalAlignment();
		}
		bCapturedIconOnlyLayout = true;
	}
	if (bIconOnly || bAppliedIconOnlyLayout)
	{
		FButtonStyle Style = Button_Root->GetStyle();
		// The caption button's 10px style insets leave less room than the icon
		// itself in a narrow rail. Remove both those insets and the caption gap.
		Style.NormalPadding = bIconOnly ? FMargin(0) : AuthoredNormalPadding;
		Style.PressedPadding = bIconOnly ? FMargin(0) : AuthoredPressedPadding;
		Button_Root->SetStyle(Style);
		if (ContentSlot)
		{
			ContentSlot->SetPadding(bIconOnly ? FMargin(0) : AuthoredButtonSlotPadding);
			ContentSlot->SetHorizontalAlignment(bIconOnly ? HAlign_Center : AuthoredButtonHorizontalAlignment);
			ContentSlot->SetVerticalAlignment(bIconOnly ? VAlign_Center : AuthoredButtonVerticalAlignment);
		}
		if (IconSlot)
		{
			IconSlot->SetPadding(bIconOnly ? FMargin(0) : AuthoredIconSlotPadding);
			IconSlot->SetSize(bIconOnly ? FSlateChildSize(ESlateSizeRule::Automatic) : AuthoredIconSlotSize);
			IconSlot->SetHorizontalAlignment(bIconOnly ? HAlign_Center : AuthoredIconHorizontalAlignment);
			IconSlot->SetVerticalAlignment(bIconOnly ? VAlign_Center : AuthoredIconVerticalAlignment);
		}
		if (const TSharedPtr<SWidget> SlateImage = Image_Icon->GetCachedWidget())
		{
			const TSharedRef<SImage> Image = StaticCastSharedRef<SImage>(SlateImage.ToSharedRef());
			// UImage's authored size comes from its brush. Its SImage is rebuilt
			// without an override; clear our compact override when restoring text.
			Image->SetDesiredSizeOverride(bIconOnly
				? TOptional<FVector2D>(FVector2D(FMath::Max(1.f, IconOnlySize)))
				: TOptional<FVector2D>());
		}
		bAppliedIconOnlyLayout = bIconOnly;
	}
}

void UFF_PrepBuildCategoryChoice::RefreshVisualState()
{
	Super::RefreshVisualState();
	if (Text_Label)
	{
		if (!bCapturedLabelVisibility)
		{
			AuthoredLabelVisibility = Text_Label->GetVisibility();
			bCapturedLabelVisibility = true;
		}
		Text_Label->SetVisibility(bIconOnly ? ESlateVisibility::Collapsed : AuthoredLabelVisibility);
#if WITH_EDITOR
		if (IsDesignTime()) Text_Label->bHiddenInDesigner = bIconOnly;
#endif
	}
	if (bIconOnly)
	{
		SetToolTipText(AuthoredLabel);
		if (Button_Root) Button_Root->SetToolTipText(AuthoredLabel);
	}
	RefreshIconOnlyLayout();
	FSlateBrush Probe = AuthoredIcon;
	const bool bKeepSourceColours = bPreserveIconColours &&
		FFBuildAtlas::TryApply(Probe, IconAtlas, IconAtlasColumns, IconAtlasRows, IconAtlasIndex);
	if (Image_Icon && bKeepSourceColours)
		Image_Icon->SetColorAndOpacity(FLinearColor::White);
	if (Border_IconBackdrop)
		Border_IconBackdrop->SetVisibility(bKeepSourceColours ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
