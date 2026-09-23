// FF_BuildMultiSelectionWidget.cpp
// A native fallback mirrors the editable WBP contract when its asset is unavailable.

#include "Building/Presentation/FF_BuildMultiSelectionWidget.h"

#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildCategoryIcons.h"
#include "Building/Presentation/FF_BuildTypography.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/Core/FF_BuildPearlStyle.h"

#define LOCTEXT_NAMESPACE "FFBuildMultiSelection"

namespace
{
FSlateBrush Surface(const FLinearColor Color, const float Radius,
	const FLinearColor Outline = FLinearColor::Transparent)
{
	return FFBuildPearl::Rounded(Color, Radius, Outline, Outline.A > 0.f ? 1.f : 0.f);
}

UTextBlock* MakeText(UWidgetTree& Tree, const FName Name, const FText& Value,
	const int32 FontSize, const FLinearColor Color, const bool bStrong = false)
{
	UTextBlock* Text = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Text->SetText(Value);
	Text->SetFont(FFBuildTypography::Make(bStrong
		? FFBuildTypography::ERole::Label : FFBuildTypography::ERole::Body, FontSize));
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Text;
}

void AddToRow(UHorizontalBox& Row, UWidget& Child, const float RightPadding = 0.0f,
	const bool bFill = false)
{
	UHorizontalBoxSlot* Slot = Row.AddChildToHorizontalBox(&Child);
	Slot->SetPadding(FMargin(0.0f, 0.0f, RightPadding, 0.0f));
	Slot->SetVerticalAlignment(VAlign_Center);
	Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
}

UButton* MakeAction(UWidgetTree& Tree, const FName Name, const FText& Caption,
	const bool bPrimary)
{
	UButton* Button = Tree.ConstructWidget<UButton>(UButton::StaticClass(), Name);
	FButtonStyle Style;
	Style.Normal = Surface(bPrimary ? FFBuildPearl::Charcoal : FFBuildPearl::Pearl, 20.0f,
		bPrimary ? FFBuildPearl::Cyan : FFBuildPearl::Line);
	Style.Hovered = Surface(bPrimary ? FFBuildPearl::Charcoal : FFBuildPearl::Pearl, 20.0f, FFBuildPearl::Cyan);
	Style.Pressed = Surface(FFBuildPearl::Charcoal, 20.0f, FFBuildPearl::Cyan);
	Style.Disabled = Surface(FFBuildPearl::Pearl, 20.0f, FFBuildPearl::Line);
	Style.NormalPadding = Style.PressedPadding = FMargin(9.0f, 5.0f);
	Button->SetStyle(Style);
	UTextBlock* Label = MakeText(Tree, NAME_None, Caption, 12,
		bPrimary ? FLinearColor::White : FFBuildPearl::Ink, true);
	UButtonSlot* ButtonSlot = CastChecked<UButtonSlot>(Button->AddChild(Label));
	ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	return Button;
}
}

UFF_BuildMultiSelectionWidget::UFF_BuildMultiSelectionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> UFF_BuildMultiSelectionWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) { BuildNativeTree(); }
	ResolveBindings();
	return Super::RebuildWidget();
}

void UFF_BuildMultiSelectionWidget::ResolveBindings()
{
	if (!WidgetTree) { return; }
	Border_MultiSelection = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_MultiSelection")));
	Text_MultiSelectionCount = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_MultiSelectionCount")));
	Row_SelectedFacilities = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("Row_SelectedFacilities")));
	Button_InspectSelection = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_InspectSelection")));
	Button_ClearSelection = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_ClearSelection")));
	Button_ExitMultiSelection = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_ExitMultiSelection")));
}

void UFF_BuildMultiSelectionWidget::BuildNativeTree()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetWidthOverride(920.0f);
	Width->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_MultiSelection"));
	Panel->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Panel")));
	Panel->SetBrushColor(FLinearColor::White);
	Panel->SetPadding(FMargin(13.0f, 9.0f));
	Width->AddChild(Panel);
	UCanvasPanelSlot* Position = Canvas->AddChildToCanvas(Width);
	Position->SetAnchors(FAnchors(0.5f, 1.0f));
	Position->SetAlignment(FVector2D(0.5f, 1.0f));
	Position->SetPosition(FVector2D(0.0f, -108.0f));
	Position->SetAutoSize(true);

	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Body);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Body->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
	AddToRow(*Header, *MakeText(*WidgetTree, NAME_None,
		LOCTEXT("Mode", "다중 선택"), 15, FFBuildPearl::Ink, true), 10.0f);
	AddToRow(*Header, *MakeText(*WidgetTree, TEXT("Text_MultiSelectionCount"),
		LOCTEXT("ZeroSelected", "선택 0개"), 12, FFBuildPearl::Muted), 0.0f);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>();
	AddToRow(*Header, *Spacer, 0.0f, true);
	AddToRow(*Header, *MakeAction(*WidgetTree, TEXT("Button_InspectSelection"),
		LOCTEXT("Inspect", "상세 보기"), false), 7.0f);
	AddToRow(*Header, *MakeAction(*WidgetTree, TEXT("Button_ClearSelection"),
		LOCTEXT("Clear", "전체 해제"), false), 7.0f);
	AddToRow(*Header, *MakeAction(*WidgetTree, TEXT("Button_ExitMultiSelection"),
		LOCTEXT("Exit", "종료 ×"), true), 0.0f);

	USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>();
	ScrollSize->SetHeightOverride(64.0f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Scroll->SetOrientation(Orient_Horizontal);
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	Scroll->SetAllowOverscroll(false);
	ScrollSize->AddChild(Scroll);
	Body->AddChildToVerticalBox(ScrollSize);
	UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("Row_SelectedFacilities"));
	Scroll->AddChild(Cards);
}

void UFF_BuildMultiSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildMode = GetWorld() ? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	if (UFF_BuildModeSubsystem* System = BuildMode.Get())
	{
		System->OnBuildMultiSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleSelectionChanged);
	}
	if (UFF_BuildCatalogSubsystem* Source = Catalog.Get())
	{
		Source->OnCatalogChanged.AddUniqueDynamic(this, &ThisClass::HandleCatalogChanged);
		Source->OnCatalogThumbnailsChanged.AddUniqueDynamic(this, &ThisClass::HandleCatalogChanged);
	}
	if (Button_ClearSelection)
	{
		Button_ClearSelection->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClearClicked);
	}
	if (Button_InspectSelection)
	{
		Button_InspectSelection->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleInspectClicked);
	}
	if (Button_ExitMultiSelection)
	{
		Button_ExitMultiSelection->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleExitClicked);
	}
	RefreshSelection();
}

void UFF_BuildMultiSelectionWidget::NativeDestruct()
{
	SetMarquee(false, FVector2D::ZeroVector, FVector2D::ZeroVector);
	if (UFF_BuildModeSubsystem* System = BuildMode.Get())
	{
		System->OnBuildMultiSelectionChanged.RemoveDynamic(this, &ThisClass::HandleSelectionChanged);
	}
	if (UFF_BuildCatalogSubsystem* Source = Catalog.Get())
	{
		Source->OnCatalogChanged.RemoveDynamic(this, &ThisClass::HandleCatalogChanged);
		Source->OnCatalogThumbnailsChanged.RemoveDynamic(this, &ThisClass::HandleCatalogChanged);
	}
	if (Button_ClearSelection) { Button_ClearSelection->OnClicked.RemoveDynamic(this, &ThisClass::HandleClearClicked); }
	if (Button_InspectSelection) { Button_InspectSelection->OnClicked.RemoveDynamic(this, &ThisClass::HandleInspectClicked); }
	if (Button_ExitMultiSelection) { Button_ExitMultiSelection->OnClicked.RemoveDynamic(this, &ThisClass::HandleExitClicked); }
	BuildMode.Reset();
	Catalog.Reset();
	Super::NativeDestruct();
}

void UFF_BuildMultiSelectionWidget::RefreshSelection()
{
	if (!Border_MultiSelection || !Text_MultiSelectionCount || !Row_SelectedFacilities)
	{
		TakeWidget();
	}
	UFF_BuildModeSubsystem* System = BuildMode.Get();
	if (!System && GetWorld())
	{
		System = GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>();
		BuildMode = System;
	}
	const bool bActive = System && System->IsMultiSelectionActive();
	if (Border_MultiSelection)
	{
		Border_MultiSelection->SetVisibility(bActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetVisibility(bActive || bMarqueeVisible
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!Row_SelectedFacilities || !Text_MultiSelectionCount) { return; }
	if (!bActive)
	{
		Row_SelectedFacilities->ClearChildren();
		if (Button_ClearSelection) { Button_ClearSelection->SetIsEnabled(false); }
		if (Button_InspectSelection) { Button_InspectSelection->SetVisibility(ESlateVisibility::Collapsed); }
		return;
	}

	const TArray<AFFZSFacilityActor*> Facilities = System->GetMultiSelectedFacilities();
	Text_MultiSelectionCount->SetText(FText::Format(LOCTEXT("SelectedCount", "선택 {0}개"),
		FText::AsNumber(System->GetMultiSelectionCount())));
	if (Button_ClearSelection) { Button_ClearSelection->SetIsEnabled(!Facilities.IsEmpty()); }
	if (Button_InspectSelection)
	{
		Button_InspectSelection->SetVisibility(Facilities.Num() == 1
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	Row_SelectedFacilities->ClearChildren();
	if (Facilities.IsEmpty())
	{
		AddEmptyHint();
		return;
	}
	int32 DisplayIndex = 0;
	for (const AFFZSFacilityActor* Facility : Facilities)
	{
		if (IsValid(Facility)) { AddFacilityCard(*Facility, ++DisplayIndex); }
	}
}

void UFF_BuildMultiSelectionWidget::AddEmptyHint()
{
	if (!WidgetTree || !Row_SelectedFacilities) { return; }
	AddToRow(*Row_SelectedFacilities, *MakeText(*WidgetTree, NAME_None,
		LOCTEXT("EmptyHint", "시설을 클릭하거나 드래그하여 선택하세요"),
		13, FFBuildPearl::Muted), 0.0f);
}

void UFF_BuildMultiSelectionWidget::AddFacilityCard(const AFFZSFacilityActor& Facility,
	const int32 DisplayIndex)
{
	if (!WidgetTree || !Row_SelectedFacilities) { return; }
	const FFFZSFacilitySpec& Spec = Facility.GetFacilitySpec();
	FFF_BuildCatalogEntry Entry;
	const bool bHasEntry = Catalog.IsValid() && Catalog->FindEntry(Spec.FacilityDefinitionId, Entry);
	const FText Name = bHasEntry && !Entry.DisplayName.IsEmpty() ? Entry.DisplayName
		: !Spec.DisplayName.IsEmpty() ? Spec.DisplayName : LOCTEXT("UnnamedFacility", "시설");

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(180.0f);
	Size->SetHeightOverride(52.0f);
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
	Card->SetBrush(Surface(FFBuildPearl::Pearl, 14.0f, FFBuildPearl::Cyan));
	Card->SetBrushColor(FLinearColor::White);
	Card->SetPadding(FMargin(6.0f));
	Size->AddChild(Card);
	UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
	Card->AddChild(Content);

	UBorder* ImageGround = WidgetTree->ConstructWidget<UBorder>();
	ImageGround->SetBrush(Surface(FFBuildPearl::Pearl, 10.0f, FFBuildPearl::Line));
	ImageGround->SetBrushColor(FLinearColor::White);
	ImageGround->SetPadding(FMargin(4.0f));
	USizeBox* ImageSize = WidgetTree->ConstructWidget<USizeBox>();
	ImageSize->SetWidthOverride(40.0f);
	ImageSize->SetHeightOverride(40.0f);
	ImageSize->AddChild(ImageGround);
	UImage* Image = WidgetTree->ConstructWidget<UImage>();
	if (bHasEntry && Entry.Thumbnail.Get())
	{
		Image->SetBrushFromTexture(Entry.Thumbnail.Get(), true);
	}
	else
	{
		Image->SetBrush(FFBuildCategoryIcons::Resolve(false, Spec.Category));
	}
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	ImageGround->AddChild(Image);
	AddToRow(*Content, *ImageSize, 7.0f);

	UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>();
	AddToRow(*Content, *Copy, 0.0f, true);
	Copy->AddChildToVerticalBox(MakeText(*WidgetTree, NAME_None,
		FText::Format(LOCTEXT("CardIndex", "{0}번 시설"), FText::AsNumber(DisplayIndex)),
		10, FFBuildPearl::Muted));
	Copy->AddChildToVerticalBox(MakeText(*WidgetTree, NAME_None, Name, 13, FFBuildPearl::Ink, true));
	Card->SetToolTipText(Name);
	AddToRow(*Row_SelectedFacilities, *Size, 7.0f);
}

bool UFF_BuildMultiSelectionWidget::IsPointerOverStrip() const
{
	if (!Border_MultiSelection || !Border_MultiSelection->IsVisible() ||
		GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
	{
		return false;
	}
	return Border_MultiSelection->IsHovered() || (FSlateApplication::IsInitialized() &&
		Border_MultiSelection->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos()));
}

void UFF_BuildMultiSelectionWidget::SetMarquee(const bool bVisible,
	const FVector2D StartViewportPx, const FVector2D EndViewportPx)
{
	if (bMarqueeVisible == bVisible && (!bVisible ||
		(MarqueeStartViewportPx == StartViewportPx && MarqueeEndViewportPx == EndViewportPx)))
	{
		return;
	}
	bMarqueeVisible = bVisible;
	MarqueeStartViewportPx = StartViewportPx;
	MarqueeEndViewportPx = EndViewportPx;
	const bool bActive = BuildMode.IsValid() && BuildMode->IsMultiSelectionActive();
	SetVisibility(bActive || bMarqueeVisible
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	InvalidateLayoutAndVolatility();
}

int32 UFF_BuildMultiSelectionWidget::NativePaint(const FPaintArgs& Args,
	const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bMarqueeVisible) { return BaseLayer; }
	const FVector2D ViewportPx = UWidgetLayoutLibrary::GetViewportSize(GetWorld());
	if (ViewportPx.X <= 0.0f || ViewportPx.Y <= 0.0f) { return BaseLayer; }
	const FVector2D Scale(AllottedGeometry.GetLocalSize().X / ViewportPx.X,
		AllottedGeometry.GetLocalSize().Y / ViewportPx.Y);
	const FVector2D A(MarqueeStartViewportPx.X * Scale.X, MarqueeStartViewportPx.Y * Scale.Y);
	const FVector2D B(MarqueeEndViewportPx.X * Scale.X, MarqueeEndViewportPx.Y * Scale.Y);
	const FVector2D Min(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
	const FVector2D Max(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
	const FVector2D Size = Max - Min;
	if (Size.X < 1.0f || Size.Y < 1.0f) { return BaseLayer; }
	FLinearColor FillColor = FFBuildPearl::Cyan;
	FillColor.A = 0.17f;
	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
	FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 1,
		AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Min))),
		FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, FillColor);
	const TArray<FVector2f> Outline = {
		FVector2f(Min), FVector2f(Max.X, Min.Y), FVector2f(Max),
		FVector2f(Min.X, Max.Y), FVector2f(Min)
	};
	FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 2,
		AllottedGeometry.ToPaintGeometry(), Outline, ESlateDrawEffect::None,
		FFBuildPearl::Cyan, true, 2.0f);
	OutDrawElements.PopClip();
	return BaseLayer + 2;
}

void UFF_BuildMultiSelectionWidget::HandleSelectionChanged() { RefreshSelection(); }
void UFF_BuildMultiSelectionWidget::HandleCatalogChanged() { RefreshSelection(); }
void UFF_BuildMultiSelectionWidget::HandleInspectClicked()
{
	if (UFF_BuildModeSubsystem* System = BuildMode.Get())
	{
		const TArray<AFFZSFacilityActor*> Selected = System->GetMultiSelectedFacilities();
		if (Selected.Num() == 1 && IsValid(Selected[0]))
		{
			System->InspectMultiSelectedFacility(Selected[0]);
		}
	}
}
void UFF_BuildMultiSelectionWidget::HandleClearClicked()
{
	if (UFF_BuildModeSubsystem* System = BuildMode.Get()) { System->ClearMultiSelection(); }
}
void UFF_BuildMultiSelectionWidget::HandleExitClicked()
{
	if (UFF_BuildModeSubsystem* System = BuildMode.Get()) { System->ToggleMultiSelection(); }
}

#undef LOCTEXT_NAMESPACE
