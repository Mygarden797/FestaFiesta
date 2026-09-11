// Explicit, editor-only migration for the build catalog's Designer surfaces.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FFBuildStudio"
namespace FFBuildStudioWbp
{
using namespace FFUIWbpBuilder;
const FString ChoicePackage = TEXT("/Game/FestaFiesta/UI/Building/WBP_FF_BuildCategoryChoice");
const FString TilePackage = TEXT("/Game/FestaFiesta/UI/PrepHUD/Build/WBP_FF_PrepBuildTile");
const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelBuild");
constexpr const TCHAR* LogPrefix = TEXT("FF_BUILD_STUDIO_UMG");

FSlateBrush Brush(const FLinearColor Color, float Radius = 6, const FLinearColor Outline = FLinearColor::Transparent)
{
	FSlateBrush B;
	B.DrawAs = ESlateBrushDrawType::RoundedBox;
	B.TintColor = Color;
	B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	B.OutlineSettings.CornerRadii = FVector4(Radius);
	B.OutlineSettings.Color = Outline;
	B.OutlineSettings.Width = Outline.A > 0 ? 1 : 0;
	return B;
}
UTextBlock* Text(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, FText Value,
	int32 Size, FLinearColor Color, bool Variable = false)
{
	auto* W = MakeWidget<UTextBlock>(BP, Tree, Name, Variable);
	StyleText(*W, Size, Color, TEXT("Medium"));
	W->SetText(Value);
	W->SetAutoWrapText(false);
	W->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	W->SetVisibility(ESlateVisibility::HitTestInvisible);
	return W;
}
UVerticalBoxSlot* V(UVerticalBox& Parent, UWidget& Child, bool Fill = false, FMargin Padding = FMargin(0))
{
	auto* Slot = Parent.AddChildToVerticalBox(&Child);
	Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
	Slot->SetHorizontalAlignment(HAlign_Fill);
	Slot->SetVerticalAlignment(VAlign_Fill);
	Slot->SetPadding(Padding);
	return Slot;
}
UHorizontalBoxSlot* H(UHorizontalBox& Parent, UWidget& Child, bool Fill = false, FMargin Padding = FMargin(0))
{
	auto* Slot = Parent.AddChildToHorizontalBox(&Child);
	Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
	Slot->SetVerticalAlignment(VAlign_Fill);
	Slot->SetPadding(Padding);
	return Slot;
}
USizeBox* Size(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, UWidget& Child, float Width, float Height)
{
	auto* Box = MakeWidget<USizeBox>(BP, Tree, Name);
	if (Width > 0) { Box->SetWidthOverride(Width); }
	if (Height > 0) { Box->SetHeightOverride(Height); }
	Box->AddChild(&Child);
	return Box;
}
UButton* Button(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, FName LabelName, FText Label, bool Primary = false)
{
	const auto& P = UFF_UIPalette::Get()->InGame;
	auto* B = MakeWidget<UButton>(BP, Tree, Name, true);
	FButtonStyle S;
	S.Normal = Brush(Primary ? P.TealDeep : P.PaperSunken);
	S.Hovered = Brush(P.Hover, 6, P.Teal);
	S.Pressed = Brush(P.TealDeep);
	S.Disabled = Brush(P.PaperSunken);
	S.NormalPadding = S.PressedPadding = FMargin(10, 3);
	B->SetStyle(S);
	auto* LabelWidget = Text(BP, Tree, LabelName, Label, 12, Primary ? P.White : P.Ink);
	LabelWidget->bIsVariable = LabelName == TEXT("Text_Label");
	auto* Slot = Cast<UButtonSlot>(B->AddChild(LabelWidget));
	Slot->SetHorizontalAlignment(HAlign_Center);
	Slot->SetVerticalAlignment(VAlign_Center);
	return B;
}

bool BuildChoice(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
	const auto& P = UFF_UIPalette::Get()->InGame;
	auto* B = Button(BP, Tree, TEXT("Button_Root"), TEXT("Text_Label"), LOCTEXT("Choice", "전체 시설"));
	UWidget* Label = B->GetContent();
	Label->RemoveFromParent();
	auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Choice"));
	auto* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Image_Icon"), true);
	Icon->SetDesiredSizeOverride(FVector2D(16, 16));
	Icon->SetVisibility(ESlateVisibility::Collapsed);
	H(*Row, *Icon, false, FMargin(0, 0, 6, 0))->SetVerticalAlignment(VAlign_Center);
	H(*Row, *Label)->SetVerticalAlignment(VAlign_Center);
	B->AddChild(Row);
	Tree.RootWidget = Size(BP, Tree, TEXT("Size_Choice"), *B, 0, 32);
	return true;
}

bool BuildTile(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
	const auto& P = UFF_UIPalette::Get()->InGame;
	auto* Card = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Card"), true);
	Card->SetBrush(Brush(P.White, 8, P.Line));
	Card->SetPadding(FMargin(10, 7, 10, 7));
	Card->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	// The page supplies six equal columns. Designer still owns the card height and
	// can opt into a fixed width; the default expands with its TileView column.
	Tree.RootWidget = Size(BP, Tree, TEXT("SizeBox_Card"), *Card, 0, 160);
	auto* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Content"));
	Card->AddChild(Content);
	auto* Backdrop = MakeWidget<UBorder>(BP, Tree, TEXT("Border_ThumbnailBackdrop"));
	Backdrop->SetBrush(Brush(P.StatusBar, 6));
	Backdrop->SetPadding(FMargin(0));
	auto* Scale = MakeWidget<UScaleBox>(BP, Tree, TEXT("ScaleBox_Thumbnail"));
	Scale->SetStretch(EStretch::ScaleToFit);
	auto* Image = MakeWidget<UImage>(BP, Tree, TEXT("Image_Thumbnail"), true);
	Image->SetDesiredSizeOverride(FVector2D(FFPrepBuildTileLayout::ThumbnailWidth, FFPrepBuildTileLayout::ThumbnailHeight));
	Scale->AddChild(Image);
	auto* ThumbnailStack = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Thumbnail"));
	ThumbnailStack->AddChildToOverlay(Scale)->SetHorizontalAlignment(HAlign_Center);
	auto* Placeholder = Text(BP, Tree, TEXT("Text_ThumbnailPlaceholder"), LOCTEXT("ThumbnailLoading", "이미지 준비 중"), 11, P.InkMuted, true);
	auto* PlaceholderSlot = ThumbnailStack->AddChildToOverlay(Placeholder);
	PlaceholderSlot->SetHorizontalAlignment(HAlign_Center);
	PlaceholderSlot->SetVerticalAlignment(VAlign_Center);
	auto* Favorite = Button(BP, Tree, TEXT("Button_Favorite"), TEXT("Text_Favorite"), FText::FromString(TEXT("☆")));
	auto* FavoriteText = Cast<UTextBlock>(Favorite->GetContent());
	FavoriteText->bIsVariable = true;
	StyleText(*FavoriteText, 20, P.InkMuted, TEXT("Medium"));
	FButtonStyle FavoriteStyle = Favorite->GetStyle();
	FavoriteStyle.Normal = Brush(P.White, 5, P.Line);
	FavoriteStyle.NormalPadding = FavoriteStyle.PressedPadding = FMargin(0);
	Favorite->SetStyle(FavoriteStyle);
	auto* FavoriteSlot = ThumbnailStack->AddChildToOverlay(Size(BP, Tree, TEXT("Size_Favorite"), *Favorite, 30, 30));
	FavoriteSlot->SetHorizontalAlignment(HAlign_Right);
	FavoriteSlot->SetVerticalAlignment(VAlign_Top);
	FavoriteSlot->SetPadding(FMargin(3));
	Backdrop->AddChild(ThumbnailStack);
	V(*Content, *Size(BP, Tree, TEXT("SizeBox_Thumbnail"), *Backdrop,
		0, 86));
	auto* CategoryPath = Text(BP, Tree, TEXT("Text_CategoryPath"), LOCTEXT("TileCategory", "부스  /  먹거리"), 11, P.InkMuted, true);
	CategoryPath->SetVisibility(ESlateVisibility::Collapsed);
	V(*Content, *CategoryPath);
	auto* Name = Text(BP, Tree, TEXT("Text_Name"), LOCTEXT("TileName", "푸드트럭"), 14, P.Ink, true);
	StyleText(*Name, 14, P.Ink, TEXT("SemiBold"));
	V(*Content, *Name, false, FMargin(3, 9, 3, 2));
	auto* Meta = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Meta"));
	H(*Meta, *Text(BP, Tree, TEXT("Text_Cost"), LOCTEXT("Price", "700 Gold"), 12, P.TealDeep, true), true);
	H(*Meta, *Text(BP, Tree, TEXT("Text_State"), LOCTEXT("Available", "배치 가능"), 11, P.TealDeep, true));
	V(*Content, *Meta, false, FMargin(3, 0));
	auto* BuildLimit = Text(BP, Tree, TEXT("Text_BuildLimit"), LOCTEXT("Count", "설치 0 / 10"), 11, P.InkMuted, true);
	BuildLimit->SetVisibility(ESlateVisibility::Collapsed);
	V(*Content, *BuildLimit);
	return true;
}

bool BuildPanel(UWidgetBlueprint& BP, UWidgetTree& Tree, UClass* TileClass, UWidget* PreservedCompact)
{
	const auto& P = UFF_UIPalette::Get()->InGame;
	auto* Root = MakeWidget<UBorder>(BP, Tree, TEXT("Border_PanelRoot"), true);
	Root->SetBrush(Brush(P.Paper, 10, P.LineStrong));
	Root->SetPadding(FMargin(0));
	Root->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	auto* Outer = MakeWidget<UBorder>(BP, Tree, TEXT("Border_ApprovedCatalogV9"));
	Outer->SetBrushColor(FLinearColor::Transparent);
	Outer->SetPadding(FMargin(16, 0));
	Outer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Outer->AddChild(Root);
	Tree.RootWidget = Outer;
	auto* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_ApprovedBuildV6"));
	Root->AddChild(Content);
	auto* HeaderGroup = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_Header"), true);
	auto* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_CatalogHeader"));
	// A simple four-block UMG mark matches the preview without a font-dependent icon.
	auto* Mark = MakeWidget<UBorder>(BP, Tree, TEXT("Border_BuildMark"));
	Mark->SetBrush(Brush(P.Hover, 8, P.Line));
	Mark->SetPadding(FMargin(8));
	auto* MarkRows = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_BuildMark"));
	Mark->AddChild(MarkRows);
	for (int32 RowIndex = 0; RowIndex < 2; ++RowIndex)
	{
		auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, FName(*FString::Printf(TEXT("Row_BuildMark%d"), RowIndex)));
		for (int32 ColIndex = 0; ColIndex < 2; ++ColIndex)
		{
			auto* Block = MakeWidget<UBorder>(BP, Tree, FName(*FString::Printf(TEXT("Block_BuildMark%d%d"), RowIndex, ColIndex)));
			Block->SetBrush(Brush(P.TealDeep, 1));
			Block->SetPadding(FMargin(0));
			H(*Row, *Size(BP, Tree, FName(*FString::Printf(TEXT("Size_BuildMark%d%d"), RowIndex, ColIndex)), *Block, 7, 7), false, FMargin(1));
		}
		V(*MarkRows, *Row);
	}
	H(*Header, *Mark, false, FMargin(0, 0, 11, 0))->SetVerticalAlignment(VAlign_Center);
	auto* TitleStack = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_CatalogTitle"));
	auto* Title = Text(BP, Tree, TEXT("Text_PanelTitle"), LOCTEXT("ApprovedTitle", "시설 배치"), 18, P.TealDeep, true);
	StyleText(*Title, 18, P.TealDeep, TEXT("SemiBold"));
	V(*TitleStack, *Title);
	auto* Subtitle = Text(BP, Tree, TEXT("Text_StudioHint"), LOCTEXT("ApprovedStudioHint", "마음에 드는 시설을 골라 놓아보세요"), 11, P.InkMuted);
	V(*TitleStack, *Subtitle, false, FMargin(0, 2, 0, 0));
	H(*Header, *TitleStack, true, FMargin(0, 0, 14, 0))->SetVerticalAlignment(VAlign_Center);
	auto* Search = MakeWidget<UEditableTextBox>(BP, Tree, TEXT("Search_Facilities"), true);
	Search->SetHintText(LOCTEXT("Search", "이름 · 종류 검색"));
	Search->SetMinDesiredWidth(180);
	FEditableTextBoxStyle SearchStyle = Search->GetWidgetStyle();
	SearchStyle.BackgroundImageNormal = Brush(P.White, 6, P.Line);
	SearchStyle.BackgroundImageHovered = Brush(P.White, 6, P.Teal);
	SearchStyle.BackgroundImageFocused = Brush(P.White, 6, P.TealDeep);
	SearchStyle.ForegroundColor = P.Ink;
	SearchStyle.FocusedForegroundColor = P.Ink;
	SearchStyle.Padding = FMargin(10, 5);
	SearchStyle.TextStyle.SetFont(Subtitle->GetFont());
	Search->SetWidgetStyle(SearchStyle);
	H(*Header, *Search, false, FMargin(0, 0, 14, 0))->SetVerticalAlignment(VAlign_Center);
	auto* GoldBacking = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Gold"));
	GoldBacking->SetBrush(Brush(FLinearColor::Transparent, 8));
	GoldBacking->SetPadding(FMargin(12, 6));
	GoldBacking->AddChild(Text(BP, Tree, TEXT("Text_Gold"), LOCTEXT("Gold", "보유 7,600 Gold"), 15, P.TealDeep, true));
	H(*Header, *GoldBacking, false, FMargin(0, 0, 8, 0))->SetVerticalAlignment(VAlign_Center);
	auto* InspectButton = Button(BP, Tree, TEXT("Button_InspectFacilities"), TEXT("Text_InspectFacilities"), LOCTEXT("InspectFacilities", "시설 확인  B"));
	H(*Header, *Button(BP, Tree, TEXT("Button_Close"), TEXT("Text_CloseLabel"), FText::FromString(TEXT("×"))))->SetVerticalAlignment(VAlign_Center);
	V(*HeaderGroup, *Size(BP, Tree, TEXT("Size_CatalogHeader"), *Header, 0, 68), false, FMargin(18, 0));
	auto* HeaderLine = MakeWidget<UBorder>(BP, Tree, TEXT("Border_HeaderDivider"));
	HeaderLine->SetBrush(Brush(P.Line, 0));
	HeaderLine->SetPadding(FMargin(0));
	V(*HeaderGroup, *Size(BP, Tree, TEXT("Size_HeaderDivider"), *HeaderLine, 0, 1));
	V(*Content, *HeaderGroup);

	auto* CatalogShell = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Panel_Catalog"), true);
	auto* Catalog = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_CatalogBody"));
	auto* CategoryScroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("ScrollBox_Categories"));
	CategoryScroll->SetOrientation(Orient_Horizontal);
	CategoryScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	CategoryScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	CategoryScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
	CategoryScroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	auto* Categories = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Categories"), true);
	CategoryScroll->AddChild(Categories);
	V(*Catalog, *Size(BP, Tree, TEXT("Size_CategoryRow"), *CategoryScroll, 0, 34), false, FMargin(0, 10, 0, 8));
	auto* CategoryLine = MakeWidget<UBorder>(BP, Tree, TEXT("Border_CategoryDivider"));
	CategoryLine->SetBrush(Brush(P.Line, 0));
	CategoryLine->SetPadding(FMargin(0));
	V(*Catalog, *Size(BP, Tree, TEXT("Size_CategoryDivider"), *CategoryLine, 0, 1), false, FMargin(0, 0, 0, 8));
	// Keep legacy bindings available during migration. New layouts populate the authored category row.
	auto* Tools = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Tools"), true);
	Tools->SetVisibility(ESlateVisibility::Collapsed);
	for (auto* B : {
		Button(BP, Tree, TEXT("Button_FilterAll"), TEXT("Text_FilterAll"), LOCTEXT("All", "전체"), true),
		Button(BP, Tree, TEXT("Button_FilterBooth"), TEXT("Text_FilterBooth"), LOCTEXT("Booth", "부스")),
		Button(BP, Tree, TEXT("Button_FilterPerformance"), TEXT("Text_FilterPerformance"), LOCTEXT("Performance", "공연")) })
	{
		H(*Tools, *B);
	}
	V(*Catalog, *Tools);
	auto* Browser = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Browser"));
	V(*Catalog, *Browser, true);
	auto* SubRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_SubcategoryHeader"));
	auto* Breadcrumb = Text(BP, Tree, TEXT("Text_CategoryBreadcrumb"), LOCTEXT("AllFacilities", "전체 시설"), 13, P.TealDeep, true);
	Breadcrumb->SetVisibility(ESlateVisibility::Collapsed);
	H(*SubRow, *Breadcrumb);
	auto* SubScroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("ScrollBox_Subcategories"), true);
	SubScroll->SetOrientation(Orient_Horizontal);
	SubScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	SubScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	SubScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
	// ScrollBox desired width must never become a minimum for the Fill slot.
	// Clip both its viewport and content so hidden chips cannot paint or hit-test over favorites.
	SubScroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	auto* Sub = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Subcategories"), true);
	SubScroll->AddChild(Sub);
	auto* SubViewport = Size(BP, Tree, TEXT("Size_SubcategoryViewport"), *SubScroll, 0, 32);
	SubViewport->SetMinDesiredWidth(0);
	SubViewport->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	H(*SubRow, *SubViewport, true);
	auto* Favorites = Button(BP, Tree, TEXT("Button_FavoritesOnly"), TEXT("Text_FavoritesOnly"), LOCTEXT("FavoritesOnly", "☆ 즐겨찾기만 · 0"));
	auto* FavoriteLabel = Cast<UTextBlock>(Favorites->GetContent());
	FavoriteLabel->bIsVariable = true;
	FavoriteLabel->SetText(LOCTEXT("FavoritesLabel", "☆ 즐겨찾기만"));
	FavoriteLabel->RemoveFromParent();
	auto* FavoriteRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_FavoriteFilter"));
	H(*FavoriteRow, *FavoriteLabel)->SetVerticalAlignment(VAlign_Center);
	auto* CountBadge = MakeWidget<UBorder>(BP, Tree, TEXT("Border_FavoriteCount"));
	CountBadge->SetBrush(Brush(P.PaperSunken, 4));
	CountBadge->SetPadding(FMargin(5, 1));
	CountBadge->AddChild(Text(BP, Tree, TEXT("Text_FavoriteCount"), FText::FromString(TEXT("0")), 11, P.TealDeep, true));
	H(*FavoriteRow, *CountBadge, false, FMargin(7, 0, 0, 0))->SetVerticalAlignment(VAlign_Center);
	Favorites->AddChild(FavoriteRow);
	FButtonStyle FavoritesStyle = Favorites->GetStyle();
	FavoritesStyle.Normal = Brush(P.White, 6, P.Line);
	FavoritesStyle.Pressed = Brush(FLinearColor::FromSRGBColor(FColor(0xFA, 0xF1, 0xDC)), 6,
		FLinearColor::FromSRGBColor(FColor(0xC7, 0xA9, 0x5B)));
	Favorites->SetStyle(FavoritesStyle);
	H(*SubRow, *Size(BP, Tree, TEXT("Size_FavoritesOnly"), *Favorites, 164, 32), false, FMargin(12, 0, 0, 0));
	V(*Browser, *Size(BP, Tree, TEXT("Size_SubcategoryHeader"), *SubRow, 0, 32), false, FMargin(0, 0, 0, 8));
	auto* Tiles = MakeWidget<UFF_PrepBuildTileView>(BP, Tree, TEXT("TileView_Facilities"), true);
	Tiles->ConfigureEntryClass(TileClass);
	Tiles->SetEntryHeight(160);
	Tiles->SetScrollbarVisibility(ESlateVisibility::Collapsed);
	auto* CardsOverlay = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_CatalogCards"));
	auto* TilesSlot = CardsOverlay->AddChildToOverlay(Tiles);
	TilesSlot->SetHorizontalAlignment(HAlign_Fill);
	TilesSlot->SetVerticalAlignment(VAlign_Fill);
	auto* Empty = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Panel_Empty"), true);
	Empty->SetVisibility(ESlateVisibility::Collapsed);
	auto* EmptyStar = Text(BP, Tree, TEXT("Text_EmptyStar"), FText::FromString(TEXT("☆")), 26, P.InkMuted);
	EmptyStar->SetJustification(ETextJustify::Center);
	V(*Empty, *EmptyStar);
	auto* EmptyTitle = Text(BP, Tree, TEXT("Text_EmptyTitle"), LOCTEXT("EmptyTitle", "즐겨찾기한 시설이 없어요"), 14, P.TealDeep, true);
	EmptyTitle->SetJustification(ETextJustify::Center);
	V(*Empty, *EmptyTitle, false, FMargin(0, 6));
	auto* EmptyHint = Text(BP, Tree, TEXT("Text_EmptyHint"), LOCTEXT("EmptyHint", "자주 쓰는 시설의 ☆을 눌러 모아보세요."), 12, P.InkMuted, true);
	EmptyHint->SetJustification(ETextJustify::Center);
	V(*Empty, *EmptyHint, false, FMargin(0, 0, 0, 6));
	auto* Reset = Button(BP, Tree, TEXT("Button_ResetFilters"), TEXT("Text_ResetFilters"), LOCTEXT("ResetFilters", "전체 시설 보기"));
	V(*Empty, *Reset)->SetHorizontalAlignment(HAlign_Center);
	auto* EmptySlot = CardsOverlay->AddChildToOverlay(Empty);
	EmptySlot->SetHorizontalAlignment(HAlign_Center);
	EmptySlot->SetVerticalAlignment(VAlign_Center);
	V(*Browser, *CardsOverlay, true);
	auto* Footer = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Footer"));
	H(*Footer, *Text(BP, Tree, TEXT("Text_Status"), LOCTEXT("ApprovedHint", "카드 선택 → 배치 시작  ·  ☆ 즐겨찾기  ·  B 시설 확인"), 11, P.InkMuted, true), true);
	H(*Footer, *InspectButton, false, FMargin(8, 0));
	H(*Footer, *Text(BP, Tree, TEXT("Text_ItemCount"), LOCTEXT("ItemCount", "시설 45종"), 11, P.InkMuted, true), false, FMargin(12, 0));
	H(*Footer, *Size(BP, Tree, TEXT("Size_PreviousPage"), *Button(BP, Tree, TEXT("Button_PreviousPage"), TEXT("Text_PreviousPage"), FText::FromString(TEXT("<"))), 28, 28));
	H(*Footer, *Text(BP, Tree, TEXT("Text_PageIndex"), FText::FromString(TEXT("1 / 8")), 12, P.TealDeep, true), false, FMargin(10, 0))->SetVerticalAlignment(VAlign_Center);
	H(*Footer, *Size(BP, Tree, TEXT("Size_NextPage"), *Button(BP, Tree, TEXT("Button_NextPage"), TEXT("Text_NextPage"), FText::FromString(TEXT(">"))), 28, 28));
	for (UPanelSlot* Slot : Footer->GetSlots())
	{
		CastChecked<UHorizontalBoxSlot>(Slot)->SetVerticalAlignment(VAlign_Center);
	}
	V(*CatalogShell, *Catalog, true, FMargin(18, 0, 18, 10));
	auto* FooterBacking = MakeWidget<UBorder>(BP, Tree, TEXT("Border_CatalogFooter"));
	FooterBacking->SetBrush(Brush(P.PaperSunken, 0));
	FooterBacking->SetPadding(FMargin(18, 6));
	FooterBacking->AddChild(Footer);
	V(*CatalogShell, *FooterBacking);
	V(*Content, *CatalogShell, true);
	auto* Compact = MakeWidget<UBorder>(BP, Tree, TEXT("Panel_PlacementCompact"), true);
	Compact->SetBrush(Brush(P.Paper, 10, P.Line));
	Compact->SetPadding(FMargin(12, 5));
	Compact->SetVisibility(ESlateVisibility::Collapsed);
	auto* CompactRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_PlacementCompact"));
	Compact->AddChild(CompactRow);
	H(*CompactRow, *Text(BP, Tree, TEXT("Text_PlacementGuide"), LOCTEXT("InspectGuide", "시설 확인 · 설치된 시설을 선택하세요"), 12, P.TealDeep, true), true)->SetVerticalAlignment(VAlign_Center);
	H(*CompactRow, *Button(BP, Tree, TEXT("Button_ReturnToCatalog"), TEXT("Text_ReturnToCatalog"), LOCTEXT("CatalogButton", "시설 목록  B")), false, FMargin(8, 0));
	H(*CompactRow, *Button(BP, Tree, TEXT("Button_ExitBuildMode"), TEXT("Text_ExitBuildMode"), LOCTEXT("ExitBuild", "건축 종료")));
	V(*Content, *Compact);
	return true;
}

template <typename BuildFunction>
bool ReplaceVisualTree(UWidgetBlueprint& BP, BuildFunction Build)
{
	UWidgetTree* OldTree = BP.WidgetTree;
	if (!OldTree) { return false; }
	TStrongObjectPtr<UWidgetTree> PreviousTree(OldTree);
	const auto PreviousGuids = BP.WidgetVariableNameToGuidMap;
	// The command is explicit and limited to these build assets. Graphs and class defaults stay intact.
	OldTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	BP.WidgetTree = NewObject<UWidgetTree>(&BP, TEXT("WidgetTree"), RF_Transactional);
	BP.WidgetVariableNameToGuidMap.Reset();
	if (!Build(*BP.WidgetTree) || !SaveNewBlueprint(LogPrefix, BP))
	{
		BP.WidgetTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		OldTree->Rename(TEXT("WidgetTree"), &BP, REN_DontCreateRedirectors | REN_NonTransactional);
		BP.WidgetTree = OldTree;
		BP.WidgetVariableNameToGuidMap = PreviousGuids;
		SaveNewBlueprint(LogPrefix, BP);
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: restored previous visual tree: %s"), LogPrefix, *BP.GetName());
		return false;
	}
	return true;
}

void RefreshBuildStudio()
{
	UWidgetBlueprint* Choice = EnsureWidgetBlueprint(LogPrefix, ChoicePackage,
		TEXT("/Game/FestaFiesta/UI/Building"), *UFF_PrepChoiceButton::StaticClass(), BuildChoice);
	UWidgetBlueprint* Tile = LoadExistingBlueprint(TilePackage);
	UWidgetBlueprint* Panel = LoadExistingBlueprint(PanelPackage);
	if (!Choice || !Choice->GeneratedClass || !Tile || !Panel || !Panel->WidgetTree)
	{
		UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: required build WBPs are missing"), LogPrefix);
		return;
	}
	if (Panel->WidgetTree->FindWidget(TEXT("Border_ApprovedCatalogV9")) &&
		Tile->WidgetTree && Tile->WidgetTree->FindWidget(TEXT("Button_Favorite")))
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: approved Designer trees already exist; preserving edits"), LogPrefix);
		return;
	}
	UWidget* Compact = Panel->WidgetTree->FindWidget(TEXT("Panel_PlacementCompact"));
	if (!ReplaceVisualTree(*Choice, [Choice](UWidgetTree& Tree) { return BuildChoice(*Choice, Tree); })) { return; }
	if (!Compact || !ReplaceVisualTree(*Tile, [Tile](UWidgetTree& Tree) { return BuildTile(*Tile, Tree); })) { return; }
	if (!ReplaceVisualTree(*Panel, [Panel, Tile, Compact](UWidgetTree& Tree)
		{ return BuildPanel(*Panel, Tree, Tile->GeneratedClass, Compact); })) { return; }
	if (FClassProperty* Property = FindFProperty<FClassProperty>(Panel->GeneratedClass.Get(), TEXT("CategoryChoiceClass")))
	{
		Property->SetObjectPropertyValue_InContainer(Panel->GeneratedClass->GetDefaultObject(), Choice->GeneratedClass);
	}
	for (const TPair<FName, float>& Value : {
		TPair<FName, float>(TEXT("CatalogHeight"), 392.f),
		TPair<FName, float>(TEXT("CatalogOffsetY"), -41.f),
		TPair<FName, float>(TEXT("CompactHeight"), 42.f),
		TPair<FName, float>(TEXT("CompactOffsetY"), -41.f) })
	{
		if (FFloatProperty* Property = FindFProperty<FFloatProperty>(Panel->GeneratedClass.Get(), Value.Key))
		{
			Property->SetPropertyValue_InContainer(Panel->GeneratedClass->GetDefaultObject(), Value.Value);
		}
	}
	if (FIntProperty* Property = FindFProperty<FIntProperty>(Panel->GeneratedClass.Get(), TEXT("CardsPerPage")))
	{
		Property->SetPropertyValue_InContainer(Panel->GeneratedClass->GetDefaultObject(), 6);
	}
	if (FBoolProperty* Property = FindFProperty<FBoolProperty>(Panel->GeneratedClass.Get(), TEXT("bUsePagination")))
	{
		Property->SetPropertyValue_InContainer(Panel->GeneratedClass->GetDefaultObject(), true);
	}
	if (SaveNewBlueprint(LogPrefix, *Panel))
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_DONE: approved catalog, favorite tile and category choice saved; teammate HUD untouched"), LogPrefix);
	}
}
FAutoConsoleCommand RefreshCommand(TEXT("FF.RefreshBuildStudioUMG"),
	TEXT("Migrates build catalog/tile to the approved design once. Existing approved Designer edits are preserved."),
	FConsoleCommandDelegate::CreateStatic(&RefreshBuildStudio));
}
#undef LOCTEXT_NAMESPACE
#endif
